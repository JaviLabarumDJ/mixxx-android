#include "qml/qmlmobilelibraryproxy.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QSet>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QtConcurrentRun>

#include "analyzer/analyzerscheduledtrack.h"
#include "control/controlobject.h"
#include "library/dao/directorydao.h"
#include "library/dao/playlistdao.h"
#include "library/library.h"
#include "library/rekordbox/rekordboxandroid.h"
#include "library/rekordbox/rekordboxdrivebackup.h"
#include "library/rekordbox/rekordboxplaylistcopy.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/playermanager.h"
#include "mixer/sampler.h"
#include "moc_qmlmobilelibraryproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "qml/qmllibraryproxy.h"
#include "qml/qmllibrarytracklistmodel.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/androidbackup.h"
#include "util/assert.h"
#include "util/cmdlineargs.h"
#include "util/fileinfo.h"
#include "util/sandbox.h"
#ifdef __ANDROID__
#include <QJniEnvironment>
#include <QJniObject>

#include "util/androidpermissions.h"
#include "util/removablestorage.h"
#endif
#ifdef __ANDROID_USB_DRIVE__
#include "library/usbdrive/androidusbdrive.h"
#endif

namespace mixxx {
namespace qml {

namespace {

const ConfigKey kSidebarVisibleKey(
        QStringLiteral("[Library]"), QStringLiteral("AndroidShowSidebar"));

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

namespace {

QVariantMap result(bool ok, const QString& message) {
    return QVariantMap{
            {QStringLiteral("ok"), ok},
            {QStringLiteral("message"), message},
    };
}

/// Backups are kept in public storage, which needs "All files access"
bool checkStorageAccess() {
#ifdef __ANDROID__
    if (!mixxx::android::hasFullExternalStorageAccess()) {
        mixxx::android::requestFullExternalStorageAccess();
        return false;
    }
#endif
    return true;
}

} // namespace

QString QmlMobileLibraryProxy::backupDirectory() const {
    return mixxx::android::backupRootDirectory();
}

QStringList QmlMobileLibraryProxy::backups() const {
    return mixxx::android::listBackups();
}

QVariantMap QmlMobileLibraryProxy::createBackup() {
    if (!checkStorageAccess()) {
        return result(false, tr("Backups need \"All files access\". Allow it, then try again."));
    }
    TrackCollectionManager* pTrackCollectionManager = trackCollectionManager();
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pTrackCollectionManager || !pConfig) {
        return result(false, tr("The library is not available."));
    }
    // Writes the current settings to mixxx.cfg first
    pConfig->save();
    QString error;
    const QString backupDir = mixxx::android::createBackup(
            CmdlineArgs::Instance().getSettingsPath(),
            pTrackCollectionManager->internalCollection()->database(),
            &error);
    if (backupDir.isEmpty()) {
        return result(false, error);
    }
    return result(true, tr("Backup saved to %1").arg(backupDir));
}

QVariantMap QmlMobileLibraryProxy::restoreBackup(const QString& name) {
    if (!checkStorageAccess()) {
        return result(false, tr("Restoring needs \"All files access\". Allow it, then try again."));
    }
    QString error;
    if (!mixxx::android::stageRestore(CmdlineArgs::Instance().getSettingsPath(), name, &error)) {
        return result(false, error);
    }
    // Gives the UI time to show the message
    QTimer::singleShot(3000, QCoreApplication::instance(), &QCoreApplication::quit);
    return result(true, tr("Mixxx closes now. Open it again to finish restoring %1.").arg(name));
}

bool QmlMobileLibraryProxy::sidebarVisible() const {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    return pConfig ? pConfig->getValue(kSidebarVisibleKey, true) : true;
}

void QmlMobileLibraryProxy::setSidebarVisible(bool visible) {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig || visible == sidebarVisible()) {
        return;
    }
    pConfig->setValue(kSidebarVisibleKey, visible);
    emit sidebarVisibleChanged();
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

QVariantList QmlMobileLibraryProxy::removableVolumes() const {
    QVariantList volumes;
#ifdef __ANDROID__
    for (const AndroidStorageVolume& volume : androidStorageVolumes()) {
        if (volume.kind == AndroidStorageVolume::Kind::Internal) {
            continue;
        }
        volumes.append(QVariantMap{
                {QStringLiteral("path"), volume.path},
                {QStringLiteral("name"), volume.name},
                {QStringLiteral("kind"),
                        volume.kind == AndroidStorageVolume::Kind::SdCard
                                ? QStringLiteral("sd")
                                : QStringLiteral("usb")},
        });
    }
#endif
    return volumes;
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

void QmlMobileLibraryProxy::analyzeTracksMatching(
        const QString& sqlQuery, AnalyzerTrack::Options options) const {
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
            tracks.append(AnalyzerScheduledTrack(id, options));
        }
    }
    if (!tracks.isEmpty()) {
        emit QmlLibraryProxy::get() -> analyzeTracks(tracks);
    }
}

void QmlMobileLibraryProxy::analyzeAllTracks(bool redoBeats) const {
    AnalyzerTrack::Options options;
    options.redoBeats = redoBeats;
    analyzeTracksMatching(QStringLiteral("SELECT id FROM library WHERE mixxx_deleted = 0"), options);
}

bool QmlMobileLibraryProxy::reanalyzeBeats(QmlTrackProxy* pTrackProxy) const {
    const TrackPointer pTrack = pTrackProxy ? pTrackProxy->internal() : TrackPointer();
    if (!pTrack || pTrack->isBpmLocked()) {
        return false;
    }
    // Like "Reanalyze" in the track menu of the desktop UI
    pTrack->trySetBeats(mixxx::BeatsPointer());
    emit QmlLibraryProxy::get() -> analyzeTracks({AnalyzerScheduledTrack(pTrack->getId())});
    return true;
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
    bool alreadyInLibrary = false;
    const TrackPointer pTrack = pTrackCollectionManager->getOrAddTrack(
            TrackRef::fromUrl(trackUrl), &alreadyInLibrary);
    if (!pTrack || !pTrack->getId().isValid()) {
        qWarning() << "Unable to add" << trackUrl << "to playlist" << playlistId;
        return false;
    }
    if (!alreadyInLibrary) {
        // A track of a Rekordbox drive keeps its date of the collection
        const QSqlDatabase database = pTrackCollectionManager->internalCollection()->database();
        android::setTrackDateAdded(database,
                pTrack->getId(),
                android::rekordboxDateAdded(database, pTrack->getLocation()));
    }
    return playlistDao.appendTrackToPlaylist(pTrack->getId(), playlistId);
}

int QmlMobileLibraryProxy::addAllTracksToPlaylist(int playlistId, QAbstractItemModel* pTrackList) {
    auto* pModel = qobject_cast<QmlLibraryTrackListModel*>(pTrackList);
    TrackCollectionManager* pTrackCollectionManager = trackCollectionManager();
    VERIFY_OR_DEBUG_ASSERT(pModel && pTrackCollectionManager) {
        return -1;
    }
    PlaylistDAO& playlistDao = pTrackCollectionManager->internalCollection()->getPlaylistDAO();
    if (!playlistDao.playlistExists(playlistId) || playlistDao.isPlaylistLocked(playlistId)) {
        return -1;
    }
    // The track lists load their rows on demand
    while (pModel->canFetchMore(QModelIndex())) {
        pModel->fetchMore(QModelIndex());
    }
    QList<QUrl> urls;
    urls.reserve(pModel->rowCount());
    for (int row = 0; row < pModel->rowCount(); ++row) {
        const QUrl url = pModel->getUrl(row);
        if (url.isValid()) {
            urls.append(url);
        }
    }
    // The tracks of a Rekordbox drive that are not in the library yet keep
    // their date of the Rekordbox collection
    const QSqlDatabase database = pTrackCollectionManager->internalCollection()->database();
    QList<std::pair<QString, QDateTime>> rekordboxDates;
    for (const QUrl& url : std::as_const(urls)) {
        const QString location = url.toLocalFile();
        const QDateTime dateAdded = android::rekordboxDateAdded(database, location);
        if (dateAdded.isValid() && !android::libraryTrackId(database, location).isValid()) {
            rekordboxDates.append({location, dateAdded});
        }
    }
    // Keeps the order of the list
    const QList<TrackId> trackIds = pTrackCollectionManager->resolveTrackIdsFromUrls(urls, true);
    for (const auto& [location, dateAdded] : std::as_const(rekordboxDates)) {
        android::setTrackDateAdded(database, android::libraryTrackId(database, location), dateAdded);
    }
    if (trackIds.isEmpty()) {
        return 0;
    }
    return playlistDao.appendTracksToPlaylist(trackIds, playlistId)
            ? static_cast<int>(trackIds.size())
            : -1;
}

bool QmlMobileLibraryProxy::isRekordboxPlaylist(QAbstractItemModel* pTrackList) const {
    return android::isRekordboxPlaylist(pTrackList);
}

QString QmlMobileLibraryProxy::rekordboxPlaylistCopyDirectory(const QString& name) const {
    return android::RekordboxPlaylistCopy::targetDirectory(name);
}

QString QmlMobileLibraryProxy::copyRekordboxPlaylist(
        QAbstractItemModel* pTrackList, const QString& name) {
    const auto tracks = android::rekordboxPlaylistTracks(pTrackList);
    if (!tracks) {
        return tr("This list is not a Rekordbox playlist");
    }
    if (!m_pPlaylistCopy) {
        TrackCollectionManager* pTrackCollectionManager = trackCollectionManager();
        VERIFY_OR_DEBUG_ASSERT(pTrackCollectionManager) {
            return tr("The library is not available");
        }
        m_pPlaylistCopy = new android::RekordboxPlaylistCopy(pTrackCollectionManager, this);
        connect(m_pPlaylistCopy,
                &android::RekordboxPlaylistCopy::progressChanged,
                this,
                &QmlMobileLibraryProxy::playlistCopyProgressChanged);
        connect(m_pPlaylistCopy,
                &android::RekordboxPlaylistCopy::finished,
                this,
                &QmlMobileLibraryProxy::playlistCopyFinished);
    }
    return m_pPlaylistCopy->start(*tracks, name);
}

void QmlMobileLibraryProxy::cancelPlaylistCopy() {
    if (m_pPlaylistCopy) {
        m_pPlaylistCopy->cancel();
    }
}

bool QmlMobileLibraryProxy::playlistCopyRunning() const {
    return m_pPlaylistCopy && m_pPlaylistCopy->isRunning();
}

int QmlMobileLibraryProxy::playlistCopyDone() const {
    return m_pPlaylistCopy ? m_pPlaylistCopy->done() : 0;
}

int QmlMobileLibraryProxy::playlistCopyTotal() const {
    return m_pPlaylistCopy ? m_pPlaylistCopy->total() : 0;
}

void QmlMobileLibraryProxy::refreshRekordboxDrives() {
    if (m_rekordboxDrivesLoading) {
        return;
    }
    m_rekordboxDrivesLoading = true;
    emit rekordboxDrivesChanged();
    auto* pWatcher = new QFutureWatcher<QVariantList>(this);
    connect(pWatcher, &QFutureWatcher<QVariantList>::finished, this, [this, pWatcher]() {
        m_rekordboxDrives = pWatcher->result();
        m_rekordboxDrivesLoading = false;
        emit rekordboxDrivesChanged();
        pWatcher->deleteLater();
    });
    pWatcher->setFuture(QtConcurrent::run(&android::RekordboxDriveBackup::findDrives));
}

QString QmlMobileLibraryProxy::rekordboxDriveBackupDirectory(const QString& name) const {
    return android::RekordboxDriveBackup::targetDirectory(name);
}

QString QmlMobileLibraryProxy::backUpRekordboxDrive(const QString& path, const QString& name) {
    if (!m_pDriveBackup) {
        m_pDriveBackup = new android::RekordboxDriveBackup(this);
        connect(m_pDriveBackup,
                &android::RekordboxDriveBackup::progressChanged,
                this,
                &QmlMobileLibraryProxy::driveBackupProgressChanged);
        connect(m_pDriveBackup,
                &android::RekordboxDriveBackup::finished,
                this,
                &QmlMobileLibraryProxy::driveBackupFinished);
    }
    return m_pDriveBackup->start(path, name);
}

void QmlMobileLibraryProxy::cancelDriveBackup() {
    if (m_pDriveBackup) {
        m_pDriveBackup->cancel();
    }
}

bool QmlMobileLibraryProxy::driveBackupRunning() const {
    return m_pDriveBackup && m_pDriveBackup->isRunning();
}

int QmlMobileLibraryProxy::driveBackupFilesDone() const {
    return m_pDriveBackup ? m_pDriveBackup->filesDone() : 0;
}

int QmlMobileLibraryProxy::driveBackupFilesTotal() const {
    return m_pDriveBackup ? m_pDriveBackup->filesTotal() : 0;
}

double QmlMobileLibraryProxy::driveBackupProgress() const {
    if (!m_pDriveBackup || m_pDriveBackup->bytesTotal() <= 0) {
        return 0;
    }
    return static_cast<double>(m_pDriveBackup->bytesDone()) /
            static_cast<double>(m_pDriveBackup->bytesTotal());
}

bool QmlMobileLibraryProxy::hasFullStorageAccess() const {
#ifdef __ANDROID__
    return mixxx::android::hasFullExternalStorageAccess();
#else
    return true;
#endif
}

void QmlMobileLibraryProxy::allowUsbPermissionRequests() const {
#ifdef __ANDROID_USB_DRIVE__
    mixxx::usbdrive::allowPermissionRequests();
#endif
}

void QmlMobileLibraryProxy::requestFullStorageAccess() const {
#ifdef __ANDROID__
    mixxx::android::requestFullExternalStorageAccess();
#endif
}

// [AI-GENERATED DISCLAIMER START: Autonomously generated by an AI agent]
namespace {

const QString kSamplerMetaGroup = QStringLiteral("[Samplers]");
constexpr qint64 kSampleCopyBufferBytes = 65536;

QString samplerDirPath() {
    return CmdlineArgs::Instance().getSettingsPath() + QStringLiteral("/samplers");
}

QString sanitizeSampleFileName(QString name) {
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]")),
            QStringLiteral("_"));
    return name.trimmed();
}

QString resolveContentDisplayName(const QUrl& url) {
#ifdef __ANDROID__
    const QJniObject jName = QJniObject::callStaticMethod<jstring>(
            "org/mixxx/StorageHelper",
            "contentFileName",
            "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
            QNativeInterface::QAndroidApplication::context().object(),
            QJniObject::fromString(url.toString()).object<jstring>());
    QJniEnvironment env;
    env.checkAndClearExceptions();
    if (jName.isValid()) {
        const QString name = jName.toString();
        if (!name.isEmpty()) {
            return name;
        }
    }
#endif
    return url.fileName();
}

void cleanupUnusedSamplerCopies(const PlayerManager& playerManager,
        int samplerNumber,
        const QString& keepPath) {
    QDir dir(samplerDirPath());
    if (!dir.exists()) {
        return;
    }
    QSet<QString> activePaths;
    if (!keepPath.isEmpty()) {
        activePaths.insert(QFileInfo(keepPath).absoluteFilePath());
    }
    for (int i = 0; i < playerManager.numberOfSamplers(); ++i) {
        if (i == samplerNumber - 1) {
            continue;
        }
        const Sampler* pSampler = playerManager.getSampler(i);
        if (!pSampler) {
            continue;
        }
        if (const TrackPointer pTrack = pSampler->getLoadedTrack()) {
            const QString loadedLoc = pTrack->getLocation();
            if (!loadedLoc.isEmpty()) {
                activePaths.insert(QFileInfo(loadedLoc).absoluteFilePath());
            }
        }
    }
    const QString prefix = QStringLiteral("sampler_%1_").arg(samplerNumber);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : entries) {
        if (info.fileName().startsWith(prefix) &&
                !activePaths.contains(info.absoluteFilePath())) {
            QFile::remove(info.absoluteFilePath());
        }
    }
}

QVariantMap copyUriToSamplersDir(const QUrl& sourceUrl, int samplerNumber) {
    QString rawName = resolveContentDisplayName(sourceUrl);
    rawName = sanitizeSampleFileName(rawName);
    if (rawName.isEmpty()) {
        rawName = QStringLiteral("sample_%1").arg(samplerNumber);
    }
    if (!SoundSourceProxy::isFileNameSupported(rawName)) {
        return QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"),
                        QmlMobileLibraryProxy::tr(
                                "Unsupported audio format: %1").arg(rawName)},
        };
    }
    const QFileInfo nameInfo(rawName);
    const QString ext = nameInfo.suffix().toLower();
    QString safeBase = sanitizeSampleFileName(nameInfo.completeBaseName());
    if (safeBase.isEmpty()) {
        safeBase = QStringLiteral("sample");
    }
    if (safeBase.length() > 48) {
        safeBase.truncate(48);
    }

    const QString dirPath = samplerDirPath();
    if (!QDir().mkpath(dirPath)) {
        return QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"),
                        QmlMobileLibraryProxy::tr("Could not create samplers folder.")},
        };
    }

    // Include timestamp so re-importing a replaced file always produces a distinct
    // path and reloads reliably while stale copies for this slot are cleaned up.
    const QString destPath = dirPath + QStringLiteral("/sampler_%1_%2_%3.%4")
                                               .arg(samplerNumber)
                                               .arg(QDateTime::currentMSecsSinceEpoch())
                                               .arg(safeBase, ext);

    QFile srcFile(sourceUrl.toString());
    if (!srcFile.open(QIODevice::ReadOnly)) {
        return QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"),
                        QmlMobileLibraryProxy::tr("Could not open selected audio file.")},
        };
    }
    QFile dstFile(destPath);
    if (!dstFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"),
                        QmlMobileLibraryProxy::tr("Could not save sample copy on device.")},
        };
    }

    QByteArray buffer;
    buffer.resize(kSampleCopyBufferBytes);
    bool copyOk = true;
    while (!srcFile.atEnd()) {
        const qint64 readBytes = srcFile.read(buffer.data(), buffer.size());
        if (readBytes < 0) {
            copyOk = false;
            break;
        }
        if (readBytes > 0 && dstFile.write(buffer.constData(), readBytes) != readBytes) {
            copyOk = false;
            break;
        }
    }
    dstFile.flush();
    dstFile.close();
    srcFile.close();

    if (!copyOk) {
        QFile::remove(destPath);
        return QVariantMap{
                {QStringLiteral("ok"), false},
                {QStringLiteral("message"),
                        QmlMobileLibraryProxy::tr("Error while copying audio sample.")},
        };
    }

    // The copy of the last sample of this pad is removed once this one is
    // loaded (see cleanupSamplerCopiesWhenLoaded()): it may still play
    return QVariantMap{
            {QStringLiteral("ok"), true},
            {QStringLiteral("location"), destPath},
            {QStringLiteral("message"), QString()},
    };
}

} // namespace

// static
void QmlMobileLibraryProxy::registerPlayerManager(std::shared_ptr<PlayerManager> pPlayerManager) {
    s_pPlayerManager = std::move(pPlayerManager);
}

void QmlMobileLibraryProxy::scheduleSamplerSave() {
    if (!s_pPlayerManager) {
        return;
    }
    if (!m_pSamplerSaveTimer) {
        m_pSamplerSaveTimer = new QTimer(this);
        m_pSamplerSaveTimer->setSingleShot(true);
        m_pSamplerSaveTimer->setInterval(250);
        connect(m_pSamplerSaveTimer, &QTimer::timeout, this, &QmlMobileLibraryProxy::saveSamplersNow);
    }
    m_pSamplerSaveTimer->start();
}

void QmlMobileLibraryProxy::ensureSamplerAutoSaveWatch(int samplerNumber) {
    if (!s_pPlayerManager || samplerNumber < 1 ||
            samplerNumber > s_pPlayerManager->numberOfSamplers()) {
        return;
    }
    Sampler* pSampler = s_pPlayerManager->getSampler(samplerNumber - 1);
    if (!pSampler || pSampler->property("mobileSamplerWatched").toBool()) {
        return;
    }
    pSampler->setProperty("mobileSamplerWatched", true);
    connect(pSampler,
            &BaseTrackPlayer::newTrackLoaded,
            this,
            [this](const TrackPointer&) {
                scheduleSamplerSave();
            });
    connect(pSampler,
            &BaseTrackPlayer::trackUnloaded,
            this,
            [this](const TrackPointer&) {
                scheduleSamplerSave();
            });
}

QVariantMap QmlMobileLibraryProxy::importSampleFileToSampler(
        const QUrl& fileUrl, int samplerNumber, bool play) {
    if (!s_pPlayerManager) {
        return result(false, tr("Player manager is not ready."));
    }
    if (samplerNumber < 1 || samplerNumber > s_pPlayerManager->numberOfSamplers()) {
        return result(false, tr("Invalid sampler pad %1.").arg(samplerNumber));
    }

    QString location;
    if (fileUrl.scheme() == QLatin1String("content")) {
        const QVariantMap copyResult = copyUriToSamplersDir(fileUrl, samplerNumber);
        if (!copyResult.value(QStringLiteral("ok")).toBool()) {
            return copyResult;
        }
        location = copyResult.value(QStringLiteral("location")).toString();
    } else if (fileUrl.isLocalFile()) {
        location = fileUrl.toLocalFile();
        if (location.isEmpty()) {
            return result(false, tr("Empty audio file path."));
        }
        bool isUsbVirtual = false;
#ifdef __ANDROID_USB_DRIVE__
        isUsbVirtual = mixxx::usbdrive::isOnDrive(location);
#endif
        if (!isUsbVirtual && !QFileInfo::exists(location)) {
            return result(false, tr("File does not exist: %1").arg(location));
        }
        if (!SoundSourceProxy::isFileNameSupported(location)) {
            return result(false,
                    tr("Unsupported audio format: %1").arg(QFileInfo(location).fileName()));
        }
    } else {
        return result(false, tr("Unsupported file URL: %1").arg(fileUrl.toString()));
    }

    ensureSamplerAutoSaveWatch(samplerNumber);
    cleanupSamplerCopiesWhenLoaded(samplerNumber, location);
    const QString group = PlayerManager::groupForSampler(samplerNumber - 1);

    // samplers.xml is saved once the sample is loaded (see
    // ensureSamplerAutoSaveWatch()), not before
    auto loadNow = [location, group, play]() {
        if (!s_pPlayerManager) {
            return;
        }
        mixxx::FileInfo fileInfo(location);
        Sandbox::createSecurityToken(&fileInfo);
        s_pPlayerManager->slotLoadLocationToPlayer(location, group, play);
    };
#ifdef __ANDROID_USB_DRIVE__
    mixxx::usbdrive::fetchTrack(location, this, loadNow);
#else
    loadNow();
#endif

    QVariantMap okResult = result(true, QString());
    okResult.insert(QStringLiteral("location"), location);
    return okResult;
}

bool QmlMobileLibraryProxy::loadUrlToSampler(
        const QUrl& trackUrl, int samplerNumber, bool play) {
    return importSampleFileToSampler(trackUrl, samplerNumber, play)
            .value(QStringLiteral("ok"))
            .toBool();
}

void QmlMobileLibraryProxy::ejectSampler(int samplerNumber) {
    if (!s_pPlayerManager || samplerNumber < 1 ||
            samplerNumber > s_pPlayerManager->numberOfSamplers()) {
        return;
    }
    const QString group = PlayerManager::groupForSampler(samplerNumber - 1);
    // Stop playing first so BaseTrackPlayerImpl permits unloading even during loops.
    if (ControlObject* pPlayControl =
                    ControlObject::getControl(ConfigKey(group, QStringLiteral("play")))) {
        pPlayControl->set(0.0);
    }
    // Saved and its copy removed once unloaded (see
    // ensureSamplerAutoSaveWatch() and cleanupSamplerCopiesWhenLoaded())
    ensureSamplerAutoSaveWatch(samplerNumber);
    cleanupSamplerCopiesWhenLoaded(samplerNumber, QString());
    if (ControlObject* pEject =
                    ControlObject::getControl(ConfigKey(group, QStringLiteral("eject")))) {
        pEject->set(1.0);
        pEject->set(0.0);
    }
}

void QmlMobileLibraryProxy::cleanupSamplerCopiesWhenLoaded(
        int samplerNumber, const QString& location) {
    Sampler* pSampler = s_pPlayerManager ? s_pPlayerManager->getSampler(samplerNumber - 1)
                                         : nullptr;
    if (!pSampler) {
        return;
    }
    // Only the last request of a pad counts (a failed load never fires)
    disconnect(m_pendingSamplerCleanups.value(samplerNumber));
    const auto cleanup = [this, samplerNumber, location]() {
        disconnect(m_pendingSamplerCleanups.take(samplerNumber));
        if (s_pPlayerManager) {
            cleanupUnusedSamplerCopies(*s_pPlayerManager, samplerNumber, location);
        }
    };
    if (location.isEmpty()) {
        m_pendingSamplerCleanups.insert(samplerNumber,
                connect(pSampler,
                        &BaseTrackPlayer::trackUnloaded,
                        this,
                        [cleanup](const TrackPointer&) {
                            cleanup();
                        }));
        return;
    }
    m_pendingSamplerCleanups.insert(samplerNumber,
            connect(pSampler,
                    &BaseTrackPlayer::newTrackLoaded,
                    this,
                    [cleanup, location](const TrackPointer& pTrack) {
                        if (pTrack && QFileInfo(pTrack->getLocation()) == QFileInfo(location)) {
                            cleanup();
                        }
                    }));
}

void QmlMobileLibraryProxy::stopAllSamplers() {
    if (!s_pPlayerManager) {
        return;
    }
    for (int i = 0; i < s_pPlayerManager->numberOfSamplers(); ++i) {
        const QString group = PlayerManager::groupForSampler(i);
        if (ControlObject* pPlayControl =
                        ControlObject::getControl(ConfigKey(group, QStringLiteral("play")))) {
            if (pPlayControl->toBool()) {
                pPlayControl->set(0.0);
            }
        }
    }
}

void QmlMobileLibraryProxy::saveSamplersNow() {
    if (s_pPlayerManager) {
        s_pPlayerManager->saveSamplers();
    }
}

QVariantMap QmlMobileLibraryProxy::samplerPadMeta(int samplerNumber) const {
    QString label;
    QString color;
    if (const UserSettingsPointer pConfig = QmlConfigProxy::get();
            pConfig && samplerNumber >= 1) {
        label = pConfig->getValueString(
                ConfigKey(kSamplerMetaGroup, QStringLiteral("PadLabel%1").arg(samplerNumber)));
        color = pConfig->getValueString(
                ConfigKey(kSamplerMetaGroup, QStringLiteral("PadColor%1").arg(samplerNumber)));
    }
    return QVariantMap{
            {QStringLiteral("label"), label},
            {QStringLiteral("color"), color},
    };
}

void QmlMobileLibraryProxy::setSamplerPadMeta(
        int samplerNumber, const QString& label, const QString& color) {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig || samplerNumber < 1) {
        return;
    }
    pConfig->setValue(
            ConfigKey(kSamplerMetaGroup, QStringLiteral("PadLabel%1").arg(samplerNumber)),
            label.trimmed());
    pConfig->setValue(
            ConfigKey(kSamplerMetaGroup, QStringLiteral("PadColor%1").arg(samplerNumber)),
            color.trimmed());
    pConfig->save();
    emit samplerMetaChanged(samplerNumber);
}
// [AI-GENERATED DISCLAIMER END: Autonomously generated by an AI agent]

} // namespace qml
} // namespace mixxx
