#include "qml/qmlmobileappproxy.h"

#include <QDir>
#include <QFileInfo>

#ifdef Q_OS_ANDROID
#include <QCoreApplication>
#include <QJniObject>
#endif

#include "moc_qmlmobileappproxy.cpp"
#include "qml/qmlconfigproxy.h"
#include "util/versionstore.h"

namespace {

const ConfigKey kSkinConfigKey(QStringLiteral("[Config]"), QStringLiteral("AndroidSkin"));
const QString kClassicSkin = QStringLiteral("Classic");

} // namespace

namespace mixxx {
namespace qml {

QmlMobileAppProxy::QmlMobileAppProxy(QObject* pParent)
        : QObject(pParent) {
}

// static
QmlMobileAppProxy* QmlMobileAppProxy::create(QQmlEngine* pQmlEngine, QJSEngine*) {
    return new QmlMobileAppProxy(pQmlEngine);
}

QString QmlMobileAppProxy::packageVersion() const {
#ifdef Q_OS_ANDROID
    QJniObject context = QNativeInterface::QAndroidApplication::context();
    QJniObject packageManager = context.callObjectMethod(
            "getPackageManager", "()Landroid/content/pm/PackageManager;");
    QJniObject packageName = context.callObjectMethod("getPackageName", "()Ljava/lang/String;");
    QJniObject packageInfo = packageManager.callObjectMethod("getPackageInfo",
            "(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;",
            packageName.object<jstring>(),
            jint(0));
    if (packageInfo.isValid()) {
        const QString versionName =
                packageInfo.getObjectField<jstring>("versionName").toString();
        if (!versionName.isEmpty()) {
            return versionName;
        }
    }
#endif
    return VersionStore::version();
}

QString QmlMobileAppProxy::skin() const {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    return pConfig ? pConfig->getValue(kSkinConfigKey, kClassicSkin) : kClassicSkin;
}

void QmlMobileAppProxy::setSkin(const QString& skin) {
    const UserSettingsPointer pConfig = QmlConfigProxy::get();
    if (!pConfig || skin.isEmpty() || skin == this->skin()) {
        return;
    }
    pConfig->setValue(kSkinConfigKey, skin);
    pConfig->save();
    emit skinChanged();
}

QStringList QmlMobileAppProxy::skinsIn(const QUrl& skinsFolder) const {
    QStringList skins{kClassicSkin};
    const QDir dir(skinsFolder.isLocalFile() ? skinsFolder.toLocalFile() : skinsFolder.path());
    const QStringList folders = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QString& folder : folders) {
        if (folder != kClassicSkin &&
                QFileInfo::exists(dir.filePath(folder + QStringLiteral("/MainWindow.qml")))) {
            skins.append(folder);
        }
    }
    return skins;
}

} // namespace qml
} // namespace mixxx
