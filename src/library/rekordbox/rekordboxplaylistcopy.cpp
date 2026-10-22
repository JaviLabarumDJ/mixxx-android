#include "library/rekordbox/rekordboxplaylistcopy.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QtConcurrentRun>
#include <QtDebug>

#include "library/dao/playlistdao.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "moc_rekordboxplaylistcopy.cpp"
#include "track/track.h"
#include "track/trackref.h"
#ifdef __ANDROID_USB_DRIVE__
#include "library/usbdrive/androidusbdrive.h"
#endif

namespace {

const QString kTargetRoot = QStringLiteral("/storage/emulated/0/Music/Mixxx");
const QString kPartialSuffix = QStringLiteral(".part");

/// Makes the local copy of a file of an unmounted drive, if needed
bool fetch(const QString& path) {
#ifdef __ANDROID_USB_DRIVE__
    return mixxx::usbdrive::fetchFile(path);
#else
    return QFileInfo::exists(path);
#endif
}

QString safeFileName(QString name) {
    static const QRegularExpression kInvalid(QStringLiteral("[/\\\\:*?\"<>|\\x00-\\x1F]"));
    name.replace(kInvalid, QStringLiteral("_"));
    name = name.trimmed();
    // Android's FAT and exFAT file systems drop trailing dots
    while (name.endsWith(QChar('.'))) {
        name.chop(1);
    }
    return name;
}

} // namespace

namespace mixxx {
namespace android {

RekordboxPlaylistCopy::RekordboxPlaylistCopy(
        TrackCollectionManager* pTrackCollectionManager, QObject* pParent)
        : QObject(pParent),
          m_pTrackCollectionManager(pTrackCollectionManager) {
}

RekordboxPlaylistCopy::~RekordboxPlaylistCopy() {
    m_canceled = true;
    m_future.waitForFinished();
}

// static
QString RekordboxPlaylistCopy::targetDirectory(const QString& playlistName) {
    QString name = safeFileName(playlistName);
    if (name.isEmpty()) {
        name = QStringLiteral("Rekordbox");
    }
    return kTargetRoot + QChar('/') + name;
}

QString RekordboxPlaylistCopy::start(
        const QList<RekordboxTrack>& tracks, const QString& playlistName) {
    if (m_running) {
        return tr("A playlist is already being copied");
    }
    if (tracks.isEmpty()) {
        return tr("The playlist is empty");
    }
    const QString name = playlistName.trimmed();
    if (name.isEmpty()) {
        return tr("Enter a name for the playlist");
    }
    const QString directory = targetDirectory(name);
    if (!QDir().mkpath(directory)) {
        return tr(
                "Could not create the folder %1. Grant Mixxx access to all files and "
                "try again.")
                .arg(directory);
    }

    m_playlistName = name;
    m_sources.clear();
    m_playlistOrder.clear();
    QHash<QString, int> sourceIndexByLocation;
    QSet<QString> targetNames;
    for (const auto& track : tracks) {
        const auto existing = sourceIndexByLocation.constFind(track.location);
        if (existing != sourceIndexByLocation.constEnd()) {
            m_playlistOrder.append(existing.value());
            continue;
        }
        // Different folders on the drive may contain files with the same
        // name. The names are given in the order of the playlist, so copying
        // the playlist again finds the same files.
        const QFileInfo fileInfo(track.location);
        QString fileName = safeFileName(fileInfo.fileName());
        for (int i = 2; targetNames.contains(fileName.toLower()); ++i) {
            fileName = safeFileName(fileInfo.completeBaseName()) +
                    QStringLiteral(" (%1).").arg(i) + fileInfo.suffix();
        }
        targetNames.insert(fileName.toLower());
        sourceIndexByLocation.insert(track.location, static_cast<int>(m_sources.size()));
        m_playlistOrder.append(static_cast<int>(m_sources.size()));
        m_sources.append({track, directory + QChar('/') + fileName, TrackId()});
    }

    m_done = 0;
    m_copied = 0;
    m_existing = 0;
    m_failed = 0;
    m_canceled = false;
    m_running = true;
    emit progressChanged();
    qInfo() << "Copying" << m_sources.size() << "files of the Rekordbox playlist" << name
            << "to" << directory;
    m_future = QtConcurrent::run([this, sources = m_sources]() {
        copyFiles(sources);
    });
    return QString();
}

void RekordboxPlaylistCopy::cancel() {
    m_canceled = true;
}

// Runs on the worker thread
void RekordboxPlaylistCopy::copyFiles(const QList<Source>& sources) {
    for (int i = 0; i < sources.size(); ++i) {
        if (m_canceled) {
            break;
        }
        const Source& source = sources[i];
        FileResult result = FileResult::Failed;
        if (fetch(source.track.location)) {
            const QFileInfo sourceInfo(source.track.location);
            const QFileInfo targetInfo(source.target);
            if (targetInfo.exists() && targetInfo.size() == sourceInfo.size()) {
                result = FileResult::Existing;
            } else {
                // A partial file of a canceled copy doesn't have the name of
                // the track, so it is neither added to the library nor taken
                // for a complete copy
                const QString partialPath = source.target + kPartialSuffix;
                QFile::remove(partialPath);
                if (QFile::copy(source.track.location, partialPath) &&
                        (!QFile::exists(source.target) || QFile::remove(source.target)) &&
                        QFile::rename(partialPath, source.target)) {
                    result = FileResult::Copied;
                } else {
                    qWarning() << "Unable to copy" << source.track.location << "to"
                               << source.target;
                    QFile::remove(partialPath);
                }
            }
            if (result != FileResult::Failed) {
                // The beatgrid and the cues are read after the copy
                fetch(source.track.analyzePath);
                const QString& anlzPath = source.track.analyzePath;
                fetch(anlzPath.left(anlzPath.length() - 3) + QStringLiteral("EXT"));
            }
        } else {
            qWarning() << "Unable to read" << source.track.location;
        }
        QMetaObject::invokeMethod(
                this,
                [this, i, result]() {
                    onFileDone(i, result);
                },
                Qt::QueuedConnection);
    }
    QMetaObject::invokeMethod(
            this,
            [this]() {
                finish();
            },
            Qt::QueuedConnection);
}

void RekordboxPlaylistCopy::onFileDone(int sourceIndex, FileResult result) {
    Source& source = m_sources[sourceIndex];
    switch (result) {
    case FileResult::Copied:
        ++m_copied;
        break;
    case FileResult::Existing:
        ++m_existing;
        break;
    case FileResult::Failed:
        ++m_failed;
        break;
    }
    if (result != FileResult::Failed) {
        // Added in the order of the playlist, so the "date added" follows it
        bool alreadyInLibrary = false;
        const TrackPointer pTrack = m_pTrackCollectionManager->getOrAddTrack(
                TrackRef::fromFilePath(source.target), &alreadyInLibrary);
        if (pTrack) {
            if (!alreadyInLibrary) {
                applyRekordboxData(pTrack, source.track);
            }
            source.trackId = pTrack->getId();
        } else {
            ++m_failed;
        }
    }
    ++m_done;
    emit progressChanged();
}

void RekordboxPlaylistCopy::finish() {
    m_running = false;
    emit progressChanged();

    QStringList details;
    if (m_copied > 0) {
        details << tr("%1 copied").arg(m_copied);
    }
    if (m_existing > 0) {
        details << tr("%1 already in the folder").arg(m_existing);
    }
    if (m_failed > 0) {
        details << tr("%1 failed").arg(m_failed);
    }
    const QString summary = details.join(QStringLiteral(", "));
    if (m_canceled) {
        emit finished(tr("Copy canceled: %1. The playlist was not changed.").arg(summary), true);
        return;
    }

    QList<TrackId> trackIds;
    for (const int sourceIndex : std::as_const(m_playlistOrder)) {
        const TrackId trackId = m_sources[sourceIndex].trackId;
        if (trackId.isValid()) {
            trackIds.append(trackId);
        }
    }
    PlaylistDAO& playlistDao = m_pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    int playlistId = playlistDao.getPlaylistIdFromName(m_playlistName);
    if (playlistId != kInvalidPlaylistId &&
            playlistDao.getHiddenType(playlistId) != PlaylistDAO::PLHT_NOT_HIDDEN) {
        // e.g. a history playlist with the same name
        playlistId = kInvalidPlaylistId;
    }
    if (playlistId == kInvalidPlaylistId) {
        playlistId = playlistDao.createPlaylist(m_playlistName);
    } else if (playlistDao.isPlaylistLocked(playlistId)) {
        emit finished(tr("%1. The playlist %2 is locked and was not changed.")
                              .arg(summary, m_playlistName),
                true);
        return;
    } else {
        // Refilled in the current order of the Rekordbox playlist
        QList<int> positions;
        const int maxPosition = playlistDao.getMaxPosition(playlistId);
        for (int position = 1; position <= maxPosition; ++position) {
            positions.append(position);
        }
        if (!positions.isEmpty()) {
            playlistDao.removeTracksFromPlaylist(playlistId, positions);
        }
    }
    if (playlistId == kInvalidPlaylistId ||
            (!trackIds.isEmpty() && !playlistDao.appendTracksToPlaylist(trackIds, playlistId))) {
        emit finished(tr("%1. Could not create the playlist %2.").arg(summary, m_playlistName),
                true);
        return;
    }
    qInfo() << "Copied the Rekordbox playlist" << m_playlistName << ":" << summary;
    emit finished(tr("Playlist %1 has %2 tracks: %3.")
                          .arg(m_playlistName, QString::number(trackIds.size()), summary),
            m_failed > 0);
}

} // namespace android
} // namespace mixxx
