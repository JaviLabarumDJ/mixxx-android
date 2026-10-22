#include "qml/qmlmobilelibraryproxy.h"

#include <QDir>
#include <QSqlQuery>
#include <QStandardPaths>

#include "analyzer/analyzerscheduledtrack.h"
#include "library/dao/directorydao.h"
#include "library/dao/playlistdao.h"
#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "moc_qmlmobilelibraryproxy.cpp"
#include "qml/qmllibraryproxy.h"
#include "track/track.h"
#include "util/assert.h"
#include "util/fileinfo.h"
#include "util/sandbox.h"
#ifdef __ANDROID__
#include "util/androidpermissions.h"
#endif

namespace mixxx {
namespace qml {

namespace {

TrackCollectionManager* trackCollectionManager() {
    Library* pLibrary = QmlLibraryProxy::get();
    return pLibrary ? pLibrary->trackCollectionManager() : nullptr;
}

QString cleanLocalPath(const QString& rawPath) {
    QString path = rawPath.trimmed();
    if (path.startsWith(QStringLiteral("file://"))) {
        path = QUrl(path).toLocalFile();
    }
    return path;
}

} // namespace

QmlMobileLibraryProxy::QmlMobileLibraryProxy(QObject* pParent)
        : QObject(pParent) {
}

// static
QmlMobileLibraryProxy* QmlMobileLibraryProxy::create(QQmlEngine* pQmlEngine, QJSEngine*) {
    return new QmlMobileLibraryProxy(pQmlEngine);
}

QVariantList QmlMobileLibraryProxy::getSourcesList() const {
    QVariantList list;
    if (!trackCollectionManager()) {
        return list;
    }
    const auto roots = trackCollectionManager()
                               ->internalCollection()
                               ->getRootDirectories();
    for (const auto& r : roots) {
        QVariantMap item;
        item[QStringLiteral("path")] = r.path;
        item[QStringLiteral("trackCount")] = r.trackCount;
        item[QStringLiteral("totalSecond")] = r.totalSecond;
        list.append(item);
    }
    return list;
}

bool QmlMobileLibraryProxy::addSourcePath(const QString& rawPath) {
    if (!trackCollectionManager() || rawPath.trimmed().isEmpty()) {
        return false;
    }
    const QString cleanPath = cleanLocalPath(rawPath);
    QDir dir(cleanPath);
    if (!dir.exists()) {
        return false;
    }
    Sandbox::createSecurityTokenForDir(dir);
    auto res = trackCollectionManager()->addDirectory(mixxx::FileInfo(cleanPath));
    emit sourcesChanged();
    return (res == DirectoryDAO::AddResult::Ok || res == DirectoryDAO::AddResult::AlreadyWatching);
}

bool QmlMobileLibraryProxy::removeSourcePath(const QString& rawPath) {
    if (!trackCollectionManager() || rawPath.trimmed().isEmpty()) {
        return false;
    }
    const QString cleanPath = cleanLocalPath(rawPath);
    auto res = trackCollectionManager()->removeDirectory(mixxx::FileInfo(cleanPath));
    emit sourcesChanged();
    return (res == DirectoryDAO::RemoveResult::Ok);
}

QStringList QmlMobileLibraryProxy::getSubDirectories(const QString& rawPath) const {
    QString path = cleanLocalPath(rawPath);
    if (path.isEmpty()) {
#if defined(Q_OS_ANDROID)
        path = QStringLiteral("/storage/emulated/0");
#else
        path = QDir::homePath();
#endif
    }
    QDir dir(path);
    if (!dir.exists() || !dir.isReadable()) {
        return {};
    }
    return dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name);
}

QString QmlMobileLibraryProxy::defaultMusicDirectory() const {
#if defined(Q_OS_ANDROID)
    return QStringLiteral("/storage/emulated/0/Music");
#else
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::MusicLocation);
    if (!paths.isEmpty()) {
        return paths.first();
    }
    return QDir::homePath();
#endif
}

void QmlMobileLibraryProxy::analyzeTracksMatching(const QString& sqlQuery) const {
    if (!trackCollectionManager()) {
        return;
    }
    const auto pTrackCollection = trackCollectionManager()->internalCollection();
    if (!pTrackCollection) {
        return;
    }
    QSqlQuery query(pTrackCollection->database());
    query.prepare(sqlQuery);
    if (!query.exec()) {
        return;
    }
    QList<AnalyzerScheduledTrack> tracks;
    while (query.next()) {
        TrackId id(query.value(0));
        if (id.isValid()) {
            tracks.append(id);
        }
    }
    if (!tracks.isEmpty()) {
        emit QmlLibraryProxy::get() -> analyzeTracks(tracks);
    }
}

void QmlMobileLibraryProxy::analyzeAllTracks() const {
    analyzeTracksMatching(QStringLiteral("SELECT id FROM library WHERE mixxx_deleted = 0"));
}

void QmlMobileLibraryProxy::analyzeUnanalyzedTracks() const {
    analyzeTracksMatching(QStringLiteral(
            "SELECT id FROM library WHERE mixxx_deleted = 0 AND "
            "(bpm = 0 OR bpm IS NULL OR key IS NULL OR key = '')"));
}

QVariantList QmlMobileLibraryProxy::playlists() const {
    QVariantList result;
    VERIFY_OR_DEBUG_ASSERT(trackCollectionManager()) {
        return result;
    }
    const PlaylistDAO& playlistDao =
            trackCollectionManager()->internalCollection()->getPlaylistDAO();
    const auto playlists = playlistDao.getPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN);
    for (const auto& [id, name] : playlists) {
        result.append(QVariantMap{
                {QStringLiteral("id"), id},
                {QStringLiteral("name"), name},
                {QStringLiteral("locked"), playlistDao.isPlaylistLocked(id)},
        });
    }
    return result;
}

int QmlMobileLibraryProxy::createNamedPlaylist(const QString& name) {
    VERIFY_OR_DEBUG_ASSERT(trackCollectionManager()) {
        return -1;
    }
    const QString trimmedName = name.trimmed();
    if (trimmedName.isEmpty()) {
        return -1;
    }
    PlaylistDAO& playlistDao =
            trackCollectionManager()->internalCollection()->getPlaylistDAO();
    if (playlistDao.getPlaylistIdFromName(trimmedName) != -1) {
        return -1;
    }
    return playlistDao.createPlaylist(trimmedName);
}

bool QmlMobileLibraryProxy::addTrackToPlaylist(int playlistId, const QUrl& trackUrl) {
    VERIFY_OR_DEBUG_ASSERT(trackCollectionManager()) {
        return false;
    }
    TrackCollectionManager* pTrackCollectionManager = trackCollectionManager();
    PlaylistDAO& playlistDao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    if (!playlistDao.playlistExists(playlistId) || playlistDao.isPlaylistLocked(playlistId)) {
        return false;
    }
    const TrackPointer pTrack = pTrackCollectionManager->getOrAddTrack(
            TrackRef::fromUrl(trackUrl));
    if (!pTrack || !pTrack->getId().isValid()) {
        qWarning() << "Unable to add" << trackUrl << "to playlist" << playlistId;
        return false;
    }
    return playlistDao.appendTrackToPlaylist(pTrack->getId(), playlistId);
}

bool QmlMobileLibraryProxy::hasFullStorageAccess() const {
#ifdef __ANDROID__
    return mixxx::android::hasFullExternalStorageAccess();
#else
    return true;
#endif
}

void QmlMobileLibraryProxy::requestFullStorageAccess() const {
#ifdef __ANDROID__
    mixxx::android::requestFullExternalStorageAccess();
#endif
}

} // namespace qml
} // namespace mixxx
