#include "util/androidbackup.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QtDebug>
#include <algorithm>

namespace mixxx {
namespace android {

namespace {

const QString kDatabaseFileName = QStringLiteral("mixxxdb.sqlite");
const QString kPendingDirName = QStringLiteral("restore_pending");
const QString kPreviousDirName = QStringLiteral("restore_previous");
/// Written last when staging, so an interrupted copy is never applied
const QString kCompleteMarker = QStringLiteral(".complete");
const QString kInfoFileName = QStringLiteral("backup-info.txt");

/// Entries of the settings folder that are not backed up
bool isExcluded(const QString& name) {
    return name.startsWith(kDatabaseFileName) || // copied separately
            name.startsWith(QStringLiteral("mixxx.log")) ||
            name == QStringLiteral("analysis") || // waveforms, regenerated
            name == kPendingDirName || name == kPreviousDirName;
}

bool copyRecursively(const QString& source, const QString& target) {
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isDir()) {
        QFile::remove(target);
        return QFile::copy(source, target);
    }
    if (!QDir().mkpath(target)) {
        return false;
    }
    const QStringList entries =
            QDir(source).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const auto& entry : entries) {
        if (!copyRecursively(source + QChar('/') + entry, target + QChar('/') + entry)) {
            return false;
        }
    }
    return true;
}

bool removePath(const QString& path) {
    const QFileInfo info(path);
    if (!info.exists() && !info.isSymLink()) {
        return true;
    }
    return info.isDir() ? QDir(path).removeRecursively() : QFile::remove(path);
}

} // namespace

QString backupRootDirectory() {
    return QStringLiteral("/storage/emulated/0/Mixxx/backup");
}

QString createBackup(const QString& settingsPath, QSqlDatabase database, QString* pError) {
    const QString name = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HH-mm-ss"));
    const QString backupDir = backupRootDirectory() + QChar('/') + name;
    if (!QDir().mkpath(backupDir)) {
        *pError = QCoreApplication::translate("AndroidBackup", "Cannot create %1").arg(backupDir);
        return {};
    }

    QSqlQuery query(database);
    query.prepare(QStringLiteral("VACUUM INTO :path"));
    query.bindValue(QStringLiteral(":path"), QString(backupDir + QChar('/') + kDatabaseFileName));
    if (!query.exec()) {
        *pError = QCoreApplication::translate("AndroidBackup", "Cannot copy the library database: %1")
                          .arg(query.lastError().text());
        QDir(backupDir).removeRecursively();
        return {};
    }

    const QStringList entries =
            QDir(settingsPath).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    for (const auto& entry : entries) {
        if (isExcluded(entry)) {
            continue;
        }
        if (!copyRecursively(settingsPath + QChar('/') + entry, backupDir + QChar('/') + entry)) {
            *pError = QCoreApplication::translate("AndroidBackup", "Cannot copy %1").arg(entry);
            QDir(backupDir).removeRecursively();
            return {};
        }
    }

    QFile info(backupDir + QChar('/') + kInfoFileName);
    if (info.open(QIODevice::WriteOnly | QIODevice::Text)) {
        info.write(QStringLiteral("Mixxx %1 backup, created %2\n")
                        .arg(QCoreApplication::applicationVersion(),
                                QDateTime::currentDateTime().toString(Qt::ISODate))
                        .toUtf8());
    }
    qInfo() << "Created backup" << backupDir;
    return backupDir;
}

QStringList listBackups() {
    QStringList names;
    const QDir root(backupRootDirectory());
    const QStringList dirs = root.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const auto& dir : dirs) {
        // Only folders that contain a database are usable backups
        if (QFileInfo::exists(root.filePath(dir + QChar('/') + kDatabaseFileName))) {
            names.append(dir);
        }
    }
    // The names are timestamps, so they sort chronologically
    std::sort(names.begin(), names.end(), std::greater<>());
    return names;
}

bool stageRestore(const QString& settingsPath, const QString& backupName, QString* pError) {
    const QString backupDir = backupRootDirectory() + QChar('/') + backupName;
    if (backupName.isEmpty() || backupName.contains(QChar('/')) ||
            !QFileInfo::exists(backupDir + QChar('/') + kDatabaseFileName)) {
        *pError = QCoreApplication::translate("AndroidBackup", "Backup %1 not found").arg(backupName);
        return false;
    }
    const QString pendingDir = settingsPath + QChar('/') + kPendingDirName;
    removePath(pendingDir);
    if (!copyRecursively(backupDir, pendingDir)) {
        *pError = QCoreApplication::translate("AndroidBackup", "Cannot copy backup %1").arg(backupName);
        removePath(pendingDir);
        return false;
    }
    QFile::remove(pendingDir + QChar('/') + kInfoFileName);
    QFile marker(pendingDir + QChar('/') + kCompleteMarker);
    if (!marker.open(QIODevice::WriteOnly)) {
        *pError = QCoreApplication::translate("AndroidBackup", "Cannot copy backup %1").arg(backupName);
        removePath(pendingDir);
        return false;
    }
    qInfo() << "Staged backup" << backupName << "for restoring at the next start";
    return true;
}

void applyPendingRestore(const QString& settingsPath) {
    const QDir settingsDir(settingsPath);
    const QString pendingDir = settingsDir.filePath(kPendingDirName);
    if (!QFileInfo::exists(pendingDir)) {
        return;
    }
    if (!QFileInfo::exists(pendingDir + QChar('/') + kCompleteMarker)) {
        qWarning() << "Discarding incomplete backup restore in" << pendingDir;
        removePath(pendingDir);
        return;
    }
    QFile::remove(pendingDir + QChar('/') + kCompleteMarker);

    const QString previousDir = settingsDir.filePath(kPreviousDirName);
    removePath(previousDir);
    QDir().mkpath(previousDir);

    // A journal left over from the replaced database must not be applied
    // to the restored one
    QStringList names = QDir(pendingDir).entryList(QDir::AllEntries | QDir::Hidden | QDir::NoDotAndDotDot);
    names.append(kDatabaseFileName + QStringLiteral("-journal"));
    for (const auto& name : std::as_const(names)) {
        const QString target = settingsDir.filePath(name);
        if (QFileInfo::exists(target) &&
                !QDir().rename(target, previousDir + QChar('/') + name)) {
            qWarning() << "Restore: cannot move away" << target;
            removePath(target);
        }
        const QString source = pendingDir + QChar('/') + name;
        if (QFileInfo::exists(source) && !QDir().rename(source, target)) {
            qWarning() << "Restore: cannot move" << source << "to" << target;
        }
    }
    removePath(pendingDir);
    qInfo() << "Restored a backup, the previous files are in" << previousDir;
}

} // namespace android
} // namespace mixxx
