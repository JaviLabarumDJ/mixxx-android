#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>
#include <QUrl>

namespace mixxx {
namespace qml {

/// Application information used by the Android UI, exposed as the
/// `Mixxx.MobileApp` singleton. Kept apart from QmlApplicationProxy so that
/// merging upstream Mixxx stays conflict-free.
class QmlMobileAppProxy : public QObject {
    Q_OBJECT
    /// The version of the installed package (versionName on Android), or
    /// the Mixxx version elsewhere
    Q_PROPERTY(QString packageVersion READ packageVersion CONSTANT)
    /// The skin of the user interface: "Classic", the main window in
    /// res/qml, or the name of a folder in res/qml/Skins with its own
    /// MainWindow.qml. Remembered across restarts.
    Q_PROPERTY(QString skin READ skin WRITE setSkin NOTIFY skinChanged)
    QML_NAMED_ELEMENT(MobileApp)
    QML_SINGLETON

  public:
    explicit QmlMobileAppProxy(QObject* pParent = nullptr);

    static QmlMobileAppProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QString packageVersion() const;

    QString skin() const;
    void setSkin(const QString& skin);

    /// "Classic" and the skins in `skinsFolder` (res/qml/Skins), i.e. its
    /// subfolders that contain a MainWindow.qml
    Q_INVOKABLE QStringList skinsIn(const QUrl& skinsFolder) const;

  signals:
    void skinChanged();
};

} // namespace qml
} // namespace mixxx
