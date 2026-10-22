#include "library/rekordbox/rekordboxdrivebackup.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>
#include <QVariantMap>
#include <QtConcurrentRun>
#include <QtDebug>
#include <algorithm>
#include <cstdlib>

#include "library/rekordbox/rekordboxandroid.h"
#include "moc_rekordboxdrivebackup.cpp"
#include "util/removablestorage.h"
#ifdef __ANDROID_USB_DRIVE__
#include "library/usbdrive/androidusbdrive.h"
#endif

namespace {

const QString kTargetRoot = QStringLiteral("/storage/emulated/0/Music/Mixxx/Rekordbox");
const QString kInternalStorage = QStringLiteral("/storage/emulated/0");
const QStringList kPdbPaths = {
        QStringLiteral("PIONEER/rekordbox/export.pdb"),
        QStringLiteral(".PIONEER/rekordbox/export.pdb"),
};
/// The folders of a Rekordbox export: the database and the analysis, and
/// the audio files
const QStringList kExportFolders = {
        QStringLiteral("PIONEER"),
        QStringLiteral(".PIONEER"),
        QStringLiteral("Contents"),
};
/// Kept free on the device
constexpr qint64 kReservedBytes = 200LL * 1024 * 1024;
constexpr int kProgressIntervalMs = 250;

struct File {
    QString relativePath;
    qint64 size;
    QDateTime modified;
};

/// FAT stores the time with 2 s resolution
bool sameTime(const QDateTime& a, const QDateTime& b) {
    return a.isValid() && b.isValid() && std::abs(a.secsTo(b)) <= 2;
}

QList<File> listMountedFiles(const QString& root) {
    QList<File> files;
    const QStringList topLevel = QDir(root).entryList(
            QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QString& folder : topLevel) {
        if (!kExportFolders.contains(folder, Qt::CaseInsensitive)) {
            continue;
        }
        QDirIterator it(root + QChar('/') + folder,
                QDir::Files | QDir::Hidden,
                QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            files.append({QDir(root).relativeFilePath(info.filePath()),
                    info.size(),
                    info.lastModified()});
        }
    }
    return files;
}

/// Deletes the files in the export folders of the backup `target` that are
/// not in `stickFiles` (relative paths in lower case), and the folders that
/// become empty. Returns the number of deleted files.
int removeDeletedFiles(const QString& target, const QSet<QString>& stickFiles) {
    int deleted = 0;
    const QDir targetDir(target);
    const QStringList topLevel =
            targetDir.entryList(QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const QString& folder : topLevel) {
        if (!kExportFolders.contains(folder, Qt::CaseInsensitive)) {
            continue;
        }
        QStringList folders;
        QDirIterator it(target + QChar('/') + folder,
                QDir::Files | QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot,
                QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QFileInfo info(it.next());
            if (info.isDir()) {
                folders.append(info.filePath());
                continue;
            }
            const QString relativePath = targetDir.relativeFilePath(info.filePath());
            if (!stickFiles.contains(relativePath.toLower()) && QFile::remove(info.filePath())) {
                ++deleted;
            }
        }
        // Deepest first; rmdir() only removes empty folders
        std::sort(folders.begin(), folders.end(), [](const QString& a, const QString& b) {
            return a.size() > b.size();
        });
        for (const QString& path : std::as_const(folders)) {
            QDir().rmdir(path);
        }
    }
    return deleted;
}

void setModified(const QString& path, const QDateTime& modified) {
    if (!modified.isValid()) {
        return;
    }
    QFile file(path);
    if (file.open(QIODevice::ReadWrite)) {
        file.setFileTime(modified, QFileDevice::FileModificationTime);
    }
}

} // namespace

namespace mixxx {
namespace android {

RekordboxDriveBackup::RekordboxDriveBackup(QObject* pParent)
        : QObject(pParent) {
}

RekordboxDriveBackup::~RekordboxDriveBackup() {
    m_canceled = true;
    m_future.waitForFinished();
}

// static
QVariantList RekordboxDriveBackup::findDrives() {
    QVariantList drives;
    const QFileInfoList roots = mixxx::removableStorageRoots();
    for (const QFileInfo& root : roots) {
        const QString path = root.filePath();
        // Neither the internal storage nor the backups themselves
        if (path == kInternalStorage || path.startsWith(kTargetRoot + QChar('/'))) {
            continue;
        }
#ifdef __ANDROID_USB_DRIVE__
        // The database of drives that Mixxx reads itself is in their local
        // folder once it was copied
        mixxx::usbdrive::prepareFiles(path, kPdbPaths);
#endif
        const bool hasExport = std::any_of(kPdbPaths.cbegin(),
                kPdbPaths.cend(),
                [&path](const QString& pdbPath) {
                    return QFileInfo(path + QChar('/') + pdbPath).isFile();
                });
        if (hasExport) {
            drives.append(QVariantMap{
                    {QStringLiteral("path"), path},
                    {QStringLiteral("name"), root.fileName()},
            });
        }
    }
    return drives;
}

// static
QString RekordboxDriveBackup::targetDirectory(const QString& name) {
    QString folderName = safeFileName(name);
    if (folderName.isEmpty()) {
        folderName = QStringLiteral("USB");
    }
    return kTargetRoot + QChar('/') + folderName;
}

QString RekordboxDriveBackup::start(const QString& drivePath, const QString& name) {
    if (m_running) {
        return tr("A stick is already being backed up");
    }
    if (name.trimmed().isEmpty()) {
        return tr("Enter a name for the backup");
    }
    const QString target = targetDirectory(name.trimmed());
    if (!QDir().mkpath(target)) {
        return tr(
                "Could not create the folder %1. Grant Mixxx access to all files and "
                "try again.")
                .arg(target);
    }
    m_filesDone = 0;
    m_filesTotal = 0;
    m_bytesDone = 0;
    m_bytesTotal = 0;
    m_canceled = false;
    m_running = true;
    emit progressChanged();
    qInfo() << "Backing up the Rekordbox stick" << drivePath << "to" << target;
    m_future = QtConcurrent::run([this, drivePath, target]() {
        run(drivePath, target);
    });
    return QString();
}

void RekordboxDriveBackup::cancel() {
    m_canceled = true;
}

// Runs on the worker thread
void RekordboxDriveBackup::run(const QString& drivePath, const QString& target) {
    const auto finish = [this](const QString& message, bool error) {
        QMetaObject::invokeMethod(
                this,
                [this, message, error]() {
                    m_running = false;
                    emit progressChanged();
                    emit finished(message, error);
                },
                Qt::QueuedConnection);
    };
    const auto reportProgress = [this]() {
        QMetaObject::invokeMethod(
                this,
                [this]() {
                    emit progressChanged();
                },
                Qt::QueuedConnection);
    };

    QList<File> files;
    bool isDrive = false;
    // Whether every folder could be read, so that a file missing from the
    // list was really removed from the stick
    bool complete = true;
#ifdef __ANDROID_USB_DRIVE__
    if (const auto driveFiles =
                    mixxx::usbdrive::listFiles(drivePath, kExportFolders, &complete)) {
        isDrive = true;
        for (const auto& file : *driveFiles) {
            files.append({file.relativePath, file.size, file.modified});
        }
    }
#endif
    if (!isDrive) {
        files = listMountedFiles(drivePath);
    }
    if (files.isEmpty()) {
        finish(tr("No Rekordbox export found on the stick"), true);
        return;
    }

    // Only what is new or changed. The copies get the date of the file on
    // the stick, so a changed date means the file was changed.
    QList<File> pending;
    qint64 pendingBytes = 0;
    QSet<QString> stickFiles;
    for (const File& file : std::as_const(files)) {
        stickFiles.insert(file.relativePath.toLower());
        const QFileInfo existing(target + QChar('/') + file.relativePath);
        const bool upToDate = existing.isFile() && existing.size() == file.size &&
                sameTime(existing.lastModified(), file.modified);
        if (!upToDate) {
            pending.append(file);
            pendingBytes += file.size;
        }
    }
    const qint64 available = QStorageInfo(kInternalStorage).bytesAvailable();
    if (available >= 0 && pendingBytes + kReservedBytes > available) {
        finish(tr("Not enough space: the backup needs %1 MB, %2 MB are free")
                        .arg(pendingBytes / (1024 * 1024))
                        .arg(available / (1024 * 1024)),
                true);
        return;
    }
    m_filesTotal = static_cast<int>(pending.size());
    m_bytesTotal = pendingBytes;
    reportProgress();

    int failed = 0;
    QElapsedTimer progressTimer;
    progressTimer.start();
    for (const File& file : std::as_const(pending)) {
        if (m_canceled) {
            break;
        }
        const QString destination = target + QChar('/') + file.relativePath;
        QDir().mkpath(QFileInfo(destination).absolutePath());
        bool ok = false;
#ifdef __ANDROID_USB_DRIVE__
        if (isDrive) {
            ok = mixxx::usbdrive::copyFileTo(drivePath, file.relativePath, destination);
        }
#endif
        if (!isDrive) {
            const QString partialPath = destination + QStringLiteral(".part");
            QFile::remove(partialPath);
            ok = QFile::copy(drivePath + QChar('/') + file.relativePath, partialPath) &&
                    (!QFile::exists(destination) || QFile::remove(destination)) &&
                    QFile::rename(partialPath, destination);
            if (!ok) {
                QFile::remove(partialPath);
            }
        }
        if (ok) {
            // Compared on the next backup
            setModified(destination, file.modified);
        } else {
            qWarning() << "Unable to back up" << file.relativePath;
            ++failed;
        }
        ++m_filesDone;
        m_bytesDone += file.size;
        if (progressTimer.elapsed() >= kProgressIntervalMs) {
            progressTimer.restart();
            reportProgress();
        }
    }

    const int copied = m_filesDone.load() - failed;
    // Mirrors the stick only if it was read completely and everything was
    // copied, so a read error never deletes files of the backup
    int deleted = 0;
    if (!m_canceled && failed == 0 && complete) {
        deleted = removeDeletedFiles(target, stickFiles);
    } else if (!complete) {
        qWarning() << "The stick" << drivePath
                   << "could not be read completely, no files are deleted from the backup";
    }
    const QString deletedText = deleted > 0
            ? QStringLiteral(" ") + tr("%1 files that are no longer on the stick were deleted.").arg(deleted)
            : QString();
    if (m_canceled) {
        finish(tr("Backup stopped after %1 of %2 files. Back up again to continue.")
                        .arg(m_filesDone.load())
                        .arg(m_filesTotal.load()),
                true);
    } else if (failed > 0) {
        finish(tr("%1 files backed up, %2 failed. Back up again to retry them.")
                        .arg(copied)
                        .arg(failed),
                true);
    } else if (copied == 0 && deleted == 0) {
        finish(tr("The backup is up to date"), false);
    } else {
        finish(tr("%1 files backed up to %2.").arg(copied).arg(target) + deletedText, false);
    }
    qInfo() << "Backup of" << drivePath << "done:" << copied << "copied," << failed << "failed,"
            << deleted << "deleted";
}

} // namespace android
} // namespace mixxx
