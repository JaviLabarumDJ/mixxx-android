#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QString>

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
    QML_NAMED_ELEMENT(MobileApp)
    QML_SINGLETON

  public:
    explicit QmlMobileAppProxy(QObject* pParent = nullptr);

    static QmlMobileAppProxy* create(QQmlEngine* pQmlEngine, QJSEngine* pJsEngine);

    QString packageVersion() const;
};

} // namespace qml
} // namespace mixxx
