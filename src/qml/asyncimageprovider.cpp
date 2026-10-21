#include "qml/asyncimageprovider.h"

#include <QCache>
#include <QSqlQuery>
#include <mutex>
#include <optional>

#include "library/coverartcache.h"
#include "library/library.h"
#include "moc_asyncimageprovider.cpp"
#include "qml/qmllibraryproxy.h"
#include "track/track.h"
#include "util/db/dbconnectionpooled.h"
#include "util/db/dbconnectionpooler.h"

namespace {
const QString kCoverArtPrefix = QStringLiteral("coverart/");

// The covers shown recently, so scrolling back doesn't load them again
std::mutex s_imageCacheMutex;
QCache<QString, QImage> s_imageCache(32 * 1024); // KiB

/// Reads the cover info of a track in the library without loading the
/// track, which would have to happen on the main thread. Tracks that aren't
/// in the library get an empty cover info. Returns nothing if the database
/// can't be read.
std::optional<CoverInfoRelative> coverInfoFromLibrary(const QString& trackLocation) {
    Library* pLibrary = mixxx::qml::QmlLibraryProxy::get();
    if (!pLibrary) {
        return std::nullopt;
    }
    const mixxx::DbConnectionPooler dbConnectionPooler(pLibrary->dbConnectionPool());
    QSqlDatabase database = mixxx::DbConnectionPooled(pLibrary->dbConnectionPool());
    if (!database.isOpen()) {
        return std::nullopt;
    }
    QSqlQuery query(database);
    query.prepare(QStringLiteral(
            "SELECT library.coverart_source, library.coverart_type,"
            " library.coverart_location, library.coverart_digest, library.coverart_hash"
            " FROM library INNER JOIN track_locations"
            " ON library.location = track_locations.id"
            " WHERE track_locations.location = :location"));
    query.bindValue(QStringLiteral(":location"), trackLocation);
    if (!query.exec()) {
        return std::nullopt;
    }
    CoverInfoRelative info;
    if (!query.next()) {
        return info;
    }
    info.source = static_cast<CoverInfoRelative::Source>(query.value(0).toInt());
    info.type = static_cast<CoverInfoRelative::Type>(query.value(1).toInt());
    info.coverLocation = query.value(2).toString();
    info.setImageDigest(query.value(3).toByteArray(), static_cast<quint16>(query.value(4).toUInt()));
    return info;
}
} // namespace

namespace mixxx {

namespace qml {

AsyncImageResponse::AsyncImageResponse(
        QString id,
        QSize requestedSize,
        std::shared_ptr<TrackCollectionManager> pTrackCollectionManager)
        : m_id(std::move(id)),
          m_requestedSize(requestedSize),
          m_pTrackCollectionManager(std::move(pTrackCollectionManager)) {
    setAutoDelete(false);
}

QQuickTextureFactory* AsyncImageResponse::textureFactory() const {
    return QQuickTextureFactory::textureFactoryForImage(m_image);
}

void AsyncImageResponse::run() {
    if (!m_id.startsWith(kCoverArtPrefix)) {
        qWarning() << "ImageProvider: Unsupported ID" << m_id;
        emit finished();
        return;
    }
    const QString trackLocation = AsyncImageProvider::coverArtUrlIdToTrackLocation(m_id);
    const QString cacheKey = m_id + QChar('@') + QString::number(m_requestedSize.width()) +
            QChar('x') + QString::number(m_requestedSize.height());
    {
        std::lock_guard lock(s_imageCacheMutex);
        if (const QImage* pImage = s_imageCache.object(cacheKey)) {
            m_image = *pImage;
            emit finished();
            return;
        }
    }
    // Loading the whole track would block the main thread
    if (const auto coverInfo = coverInfoFromLibrary(trackLocation)) {
        const CoverInfo::LoadedImage loadedImage = CoverInfo(*coverInfo, trackLocation).loadImage();
        if (loadedImage.result == CoverInfo::LoadedImage::Result::Ok) {
            m_image = m_requestedSize.isValid() ? loadedImage.image.scaled(m_requestedSize)
                                                : loadedImage.image;
        }
        std::lock_guard lock(s_imageCacheMutex);
        s_imageCache.insert(cacheKey,
                new QImage(m_image),
                std::max<qsizetype>(1, m_image.sizeInBytes() / 1024));
        emit finished();
        return;
    }
    const auto trackRef = TrackRef::fromFilePath(trackLocation);

    // TODO: Only load CoverInfo from TrackCollectionManager
    // instead of the entire, stateful track object.
    TrackPointer pTrack;
    if (QThread::currentThread() != m_pTrackCollectionManager->thread()) {
        QMetaObject::invokeMethod(
                m_pTrackCollectionManager.get(),
                [this, trackRef] {
                    return m_pTrackCollectionManager->getTrackByRef(trackRef);
                },
                // This invocation will block the current thread!
                Qt::BlockingQueuedConnection,
                &pTrack);
    } else {
        pTrack = m_pTrackCollectionManager->getTrackByRef(trackRef);
    }
    if (!pTrack) {
        qWarning() << "ImageProvider: Failed to load track" << trackRef;
        emit finished();
        return;
    }
    const auto coverInfo = CoverInfo(pTrack->getCoverInfo(), trackLocation);
    // Release the track reference asap, i.e. before loading the image
    pTrack.reset();

    const CoverInfo::LoadedImage loadedImage = coverInfo.loadImage();
    switch (loadedImage.result) {
    case CoverInfo::LoadedImage::Result::NoImage:
        break;
    case CoverInfo::LoadedImage::Result::Ok:
        DEBUG_ASSERT(!loadedImage.image.isNull());
        if (m_requestedSize.isValid()) {
            m_image = loadedImage.image.scaled(m_requestedSize);
        } else {
            m_image = loadedImage.image;
        }
        break;
    default:
        qWarning() << "ImageProvider: Failed to load cover art" << trackRef;
        break;
    }

    emit finished();
}

AsyncImageProvider::AsyncImageProvider(
        std::shared_ptr<TrackCollectionManager> pTrackCollectionManager)
        : QQuickAsyncImageProvider(),
          m_pTrackCollectionManager(pTrackCollectionManager) {
}

QQuickImageResponse* AsyncImageProvider::requestImageResponse(
        const QString& id, const QSize& requestedSize) {
    AsyncImageResponse* response = new AsyncImageResponse(
            id, requestedSize, m_pTrackCollectionManager);
    pool.start(response);
    return response;
}

// static
const QString AsyncImageProvider::kProviderName = QStringLiteral("mixxx");

// static
QUrl AsyncImageProvider::trackLocationToCoverArtUrl(const QString& trackLocation) {
    QUrl url("image://" + kProviderName + "/" + kCoverArtPrefix);
    return url.resolved(
            QString::fromLatin1(trackLocation.toUtf8().toBase64(
                    QByteArray::Base64UrlEncoding)));
}

//static
QString AsyncImageProvider::coverArtUrlIdToTrackLocation(const QString& coverArtUrlId) {
    return QString::fromUtf8(QByteArray::fromBase64(
            coverArtUrlId.mid(kCoverArtPrefix.size()).toLatin1(), QByteArray::Base64UrlEncoding));
}

} // namespace qml
} // namespace mixxx
