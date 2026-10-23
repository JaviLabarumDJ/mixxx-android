#include "qml/qmlmobilesingletons.h"

#include "coreservices.h"
#include "qml/qmlstreamingproxy.h"
#include "track/androidautocue.h"

namespace mixxx {
namespace qml {

void registerMobileQmlSingletons(CoreServices* pCoreServices) {
    QmlStreamingProxy::registerPlayerManager(pCoreServices->getPlayerManager());
    QmlStreamingProxy::registerUserSettings(pCoreServices->getSettings());
    android::snapAutoCuesOnLoad(pCoreServices->getPlayerManager().get());
}

void unregisterMobileQmlSingletons() {
    QmlStreamingProxy::registerPlayerManager(nullptr);
    QmlStreamingProxy::registerUserSettings(nullptr);
}

} // namespace qml
} // namespace mixxx
