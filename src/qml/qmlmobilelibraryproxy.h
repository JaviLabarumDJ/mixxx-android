#pragma once

#include <QAbstractItemModel>
#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace mixxx {
namespace qml {

/// Library functions used by the Android UI, exposed as the
/// `Mixxx.MobileLibrary` singleton. Kept apart from QmlLibraryProxy so that
/// merging upstream Mixxx stays conflict-free.
class QmlMobileLibraryProxy : public QObject {
    Q_OBJECT
    /// Whether the sidebar (playlists, crates...) of the library is shown.
    /// Remembered across restarts.
    Q_PROPERTY(bool sidebarVisible READ sidebarVisible WRITE setSidebarVisible NOTIFY
                    sidebarVisibleChanged)
    /// Where library backups are stored, see util/androidbackup.h
    Q_PROPERTY(QString backupDirectory READ backupDirectory CONSTANT)
    QML_NAMED_ELEMENT(MobileLibrary)
    QML_SINGLETON

  public:
    explicit QmlMobileLibraryProxy(QObject* pParent = nullptr);

    static QmlMobileLibraryProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QString backupDirectory() const;

    /// The available backups, newest first
    Q_INVOKABLE QStringList backups() const;
    /// Backs up the library database and the settings. Returns
    /// {ok, message}.
    Q_INVOKABLE QVariantMap createBackup();
    /// Prepares restoring the backup and closes Mixxx; the backup replaces
    /// the library and the settings when Mixxx starts again. Returns
    /// {ok, message}.
    Q_INVOKABLE QVariantMap restoreBackup(const QString& name);

    bool sidebarVisible() const;
    void setSidebarVisible(bool visible);

    /// The music directories as a list of {path, trackCount, totalSecond}
    Q_INVOKABLE QVariantList getSourcesList() const;
    Q_INVOKABLE bool addSourcePath(const QString& rawPath);
    Q_INVOKABLE bool removeSourcePath(const QString& rawPath);
    Q_INVOKABLE QStringList getSubDirectories(const QString& rawPath) const;
    Q_INVOKABLE QString defaultMusicDirectory() const;

    Q_INVOKABLE void analyzeAllTracks() const;
    Q_INVOKABLE void analyzeUnanalyzedTracks() const;

    /// The visible playlists as a list of {id, name, locked}
    Q_INVOKABLE QVariantList playlists() const;
    /// Creates a playlist without asking for the name. Returns its id, or -1
    /// if the name is empty or already used.
    Q_INVOKABLE int createNamedPlaylist(const QString& name);
    /// Appends the track, adding it to the library if needed
    Q_INVOKABLE bool addTrackToPlaylist(int playlistId, const QUrl& trackUrl);
    /// Appends all tracks of a track list, in its current order, adding them
    /// to the library if needed. Returns how many were added, or -1.
    Q_INVOKABLE int addAllTracksToPlaylist(int playlistId, QAbstractItemModel* pTrackList);

    /// Whether the databases on USB drives (Rekordbox, Engine DJ) can be
    /// read. On Android this needs "All files access".
    Q_INVOKABLE bool hasFullStorageAccess() const;
    /// Opens the system settings to grant "All files access"
    Q_INVOKABLE void requestFullStorageAccess() const;

  signals:
    void sourcesChanged();
    void sidebarVisibleChanged();

  private:
    void analyzeTracksMatching(const QString& sqlQuery) const;
};

} // namespace qml
} // namespace mixxx
