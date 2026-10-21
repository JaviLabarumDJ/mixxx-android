#include "qml/qmllibrarysource.h"

#include <qalgorithms.h>
#include <qlist.h>
#include <qqmlengine.h>

#include <QAbstractListModel>
#include <QVariant>
#include <QtDebug>
#include <memory>

#include "library/browse/browsefeature.h"
#ifdef __ENGINEPRIME__
#include "library/enginedj/enginedjfeature.h"
#endif
#include "library/library.h"
#include "library/librarytablemodel.h"
#include "library/recording/recordingfeature.h"
#include "library/rekordbox/rekordboxfeature.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackset/crate/cratefeature.h"
#include "library/trackset/crate/cratesummary.h"
#include "library/trackset/playlistfeature.h"
#include "library/trackset/setlogfeature.h"
#include "library/treeitemmodel.h"
#include "moc_qmllibrarysource.cpp"
#include "qmllibraryproxy.h"
#include "track/track.h"
#include "util/assert.h"

AllTrackLibraryFeature::AllTrackLibraryFeature(Library* pLibrary, UserSettingsPointer pConfig)
        : LibraryFeature(pLibrary, pConfig, QStringLiteral("tracks")),
          m_pSidebarModel(make_parented<TreeItemModel>(this)),
          m_pLibraryTableModel(pLibrary->trackTableModel()) {
    m_pSidebarModel->setRootItem(TreeItem::newRoot(this));
}

void AllTrackLibraryFeature::activate() {
    emit showTrackModel(m_pLibraryTableModel);
}

namespace mixxx {
namespace qml {

QmlLibraryAbstractSource::QmlLibraryAbstractSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QObject(parent),
          m_columns(columns) {
}

void QmlLibraryAbstractSource::slotShowTrackModel(QAbstractItemModel* pModel) {
    emit requestTrackModel(std::make_shared<QmlLibraryTrackListModel>(columns(), pModel));
}

QmlLibraryAllTrackSource::QmlLibraryAllTrackSource(
        QObject* parent, const QList<QmlLibraryTrackListColumn*>& columns)
        : QmlLibraryAbstractSource(parent, columns),
          m_pLibraryFeature(std::make_unique<AllTrackLibraryFeature>(
                  QmlLibraryProxy::get(), QmlConfigProxy::get())) {
    connect(m_pLibraryFeature.get(),
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibraryAbstractSource::slotShowTrackModel);
}

QmlLibraryFeatureSource::QmlLibraryFeatureSource(QObject* parent)
        : QmlLibraryAbstractSource(parent),
          m_feature(Feature::Playlists) {
}

LibraryFeature* QmlLibraryFeatureSource::internal() {
    if (m_pLibraryFeature) {
        return m_pLibraryFeature;
    }
    Library* pLibrary = QmlLibraryProxy::get();
    VERIFY_OR_DEBUG_ASSERT(pLibrary) {
        return nullptr;
    }
    for (LibraryFeature* pFeature : pLibrary->features()) {
        bool match = false;
        switch (m_feature) {
        case Feature::Playlists:
            match = qobject_cast<PlaylistFeature*>(pFeature) != nullptr;
            break;
        case Feature::Crates:
            match = qobject_cast<CrateFeature*>(pFeature) != nullptr;
            break;
        case Feature::Browse:
            match = qobject_cast<BrowseFeature*>(pFeature) != nullptr;
            break;
        case Feature::History:
            match = qobject_cast<SetlogFeature*>(pFeature) != nullptr;
            break;
        case Feature::Recordings:
            match = qobject_cast<RecordingFeature*>(pFeature) != nullptr;
            break;
        case Feature::Rekordbox:
            match = qobject_cast<RekordboxFeature*>(pFeature) != nullptr;
            break;
        case Feature::EngineDJ:
#ifdef __ENGINEPRIME__
            match = qobject_cast<EngineDJFeature*>(pFeature) != nullptr;
#endif
            break;
        }
        if (match) {
            m_pLibraryFeature = pFeature;
            break;
        }
    }
    if (!m_pLibraryFeature) {
        // Disabled in the preferences or not available in this build
        qInfo() << "Library feature" << m_feature << "is not available";
        return nullptr;
    }
    connect(m_pLibraryFeature,
            &LibraryFeature::showTrackModel,
            this,
            &QmlLibraryAbstractSource::slotShowTrackModel);
    return m_pLibraryFeature;
}

} // namespace qml
} // namespace mixxx
