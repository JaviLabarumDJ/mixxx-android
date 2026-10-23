#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>
#include <QStringList>

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
    /// The items of the top bar the user hid in the settings, e.g. "wave",
    /// "xy" (see res/qml/+android/MainWindow.qml)
    Q_PROPERTY(QStringList hiddenTopBarItems READ hiddenTopBarItems WRITE
                    setHiddenTopBarItems NOTIFY hiddenTopBarItemsChanged)
    QML_NAMED_ELEMENT(MobileApp)
    QML_SINGLETON

  public:
    explicit QmlMobileAppProxy(QObject* pParent = nullptr);

    static QmlMobileAppProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QString packageVersion() const;

    QStringList hiddenTopBarItems() const;
    void setHiddenTopBarItems(const QStringList& items);

  signals:
    void hiddenTopBarItemsChanged();
};

} // namespace qml
} // namespace mixxx
