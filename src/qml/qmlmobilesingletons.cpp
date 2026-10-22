#include "qml/qmlmobilesingletons.h"

#include "coreservices.h"
#include "qml/qmlstreamingproxy.h"

namespace mixxx {
namespace qml {

void registerMobileQmlSingletons(CoreServices* pCoreServices) {
    QmlStreamingProxy::registerPlayerManager(pCoreServices->getPlayerManager());
    QmlStreamingProxy::registerUserSettings(pCoreServices->getSettings());
}

void unregisterMobileQmlSingletons() {
    QmlStreamingProxy::registerPlayerManager(nullptr);
    QmlStreamingProxy::registerUserSettings(nullptr);
}

} // namespace qml
} // namespace mixxx
