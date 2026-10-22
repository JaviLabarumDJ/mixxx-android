// Included at the end of rekordboxfeature.cpp on Android, so the functions
// declared in rekordboxandroid.h can use the ANLZ reader of that file
// without changing it.

#include <QAbstractProxyModel>
#include <QFileInfo>
#include <QTimer>
#include <algorithm>
#include <exception>

#ifdef __HID__
#include "controllers/android.h"
#endif
#include "library/rekordbox/rekordboxandroid.h"

namespace mixxx {
namespace android {

namespace {

RekordboxPlaylistModel* rekordboxPlaylistModel(QAbstractItemModel* pModel) {
    // The track lists of the QML UI wrap the model of the feature
    while (pModel) {
        if (auto* pRekordboxModel = qobject_cast<RekordboxPlaylistModel*>(pModel)) {
            return pRekordboxModel;
        }
        auto* pProxyModel = qobject_cast<QAbstractProxyModel*>(pModel);
        pModel = pProxyModel ? pProxyModel->sourceModel() : nullptr;
    }
    return nullptr;
}

} // namespace

bool isRekordboxPlaylist(QAbstractItemModel* pModel) {
    return rekordboxPlaylistModel(pModel) != nullptr;
}

std::optional<QList<RekordboxTrack>> rekordboxPlaylistTracks(QAbstractItemModel* pModel) {
    RekordboxPlaylistModel* pRekordboxModel = rekordboxPlaylistModel(pModel);
    if (!pRekordboxModel) {
        return std::nullopt;
    }
    // All tracks, not only those matching the search
    const QString searchText = pRekordboxModel->currentSearch();
    if (!searchText.isEmpty()) {
        pRekordboxModel->search(QString());
    }
    while (pRekordboxModel->canFetchMore(QModelIndex())) {
        pRekordboxModel->fetchMore(QModelIndex());
    }
    QList<std::pair<int, RekordboxTrack>> tracks;
    for (int row = 0; row < pRekordboxModel->rowCount(); ++row) {
        const QModelIndex index = pRekordboxModel->index(row, 0);
        const auto field = [&](ColumnCache::Column column) {
            return pRekordboxModel->getFieldVariant(index, column);
        };
        bool ok = false;
        int position = field(ColumnCache::COLUMN_PLAYLISTTRACKSTABLE_POSITION).toInt(&ok);
        if (!ok) {
            position = row;
        }
        tracks.append({position,
                RekordboxTrack{
                        field(ColumnCache::COLUMN_TRACKLOCATIONSTABLE_LOCATION).toString(),
                        field(ColumnCache::COLUMN_REKORDBOX_ANALYZE_PATH).toString(),
                        field(ColumnCache::COLUMN_LIBRARYTABLE_KEY).toString(),
                        field(ColumnCache::COLUMN_LIBRARYTABLE_COLOR)}});
    }
    if (!searchText.isEmpty()) {
        pRekordboxModel->search(searchText);
    }
    // The order of the playlist, whatever the list is sorted by
    std::stable_sort(tracks.begin(), tracks.end(), [](const auto& a, const auto& b) {
        return a.first < b.first;
    });
    QList<RekordboxTrack> result;
    result.reserve(tracks.size());
    for (auto& track : tracks) {
        result.append(std::move(track.second));
    }
    return result;
}

void applyRekordboxData(const TrackPointer& pTrack, const RekordboxTrack& rekordboxTrack) {
    // Same as RekordboxPlaylistModel::getTrack(), for the copy of the file
    const QString location = pTrack->getLocation();
    int timingOffset = 0;
    if (location.endsWith(".mp3", Qt::CaseInsensitive)) {
        const int timingShiftCase = mp3guessenc_timing_shift_case(location.toStdString().c_str());
        switch (timingShiftCase) {
#if defined(__MAD__)
        case EXIT_CODE_CASE_A:
        case EXIT_CODE_CASE_D:
            timingOffset = 26;
            break;
#elif defined(__FFMPEG__)
        case EXIT_CODE_CASE_D:
            timingOffset = 26;
            break;
#endif
        default:
            break;
        }
    }

    const mixxx::audio::SampleRate sampleRate = pTrack->getSampleRate();
    const QString& anlzPath = rekordboxTrack.analyzePath;
    const QString anlzPathExt = anlzPath.left(anlzPath.length() - 3) + "EXT";
    // Tracks that were not analyzed have no ANLZ path, which would be the
    // root of the drive
    const bool hasAnlz = QFileInfo(anlzPath).isFile();
    const bool hasAnlzExt = QFileInfo(anlzPathExt).isFile();
    try {
        if (hasAnlzExt) {
            // Beatgrids appear to be only correct in legacy ANLZ file
            if (hasAnlz) {
                readAnalyze(pTrack, sampleRate, timingOffset, true, anlzPath);
            }
            readAnalyze(pTrack, sampleRate, timingOffset, false, anlzPathExt);
        } else if (hasAnlz) {
            readAnalyze(pTrack, sampleRate, timingOffset, false, anlzPath);
        }
    } catch (const std::exception& e) {
        // A damaged file shouldn't stop copying the playlist
        qWarning() << "Unable to read the Rekordbox analysis" << anlzPath << e.what();
    }

    // Unlike for a track on the drive, a missing key or color doesn't
    // replace what the file already has
    if (!rekordboxTrack.key.isEmpty()) {
        pTrack->setKeys(KeyFactory::makeBasicKeysNormalized(
                rekordboxTrack.key, mixxx::track::io::key::USER));
    }
    if (const auto color = mixxx::RgbColor::fromQVariant(rekordboxTrack.color);
            color && *color != kColorForIDNoColor) {
        pTrack->setColor(color);
    }
}

#ifdef __HID__
namespace {

/// Android mounts a drive a few seconds after it was plugged in
constexpr int kUsbChangeDelayMs = 4000;

/// Listens to USB changes as long as its feature exists
class UsbDeviceWatcher : public QObject {
  public:
    explicit UsbDeviceWatcher(RekordboxFeature* pFeature)
            : QObject(pFeature) {
        m_timer.setSingleShot(true);
        m_timer.setInterval(kUsbChangeDelayMs);
        QObject::connect(&m_timer, &QTimer::timeout, pFeature, &RekordboxFeature::refreshDevices);
        // Called on a Java thread
        m_listenerId = mixxx::android::addDevicesChangedListener([this]() {
            QMetaObject::invokeMethod(
                    this, [this]() { m_timer.start(); }, Qt::QueuedConnection);
        });
    }
    ~UsbDeviceWatcher() override {
        // Waits for a callback that is running
        mixxx::android::removeDevicesChangedListener(m_listenerId);
    }

  private:
    QTimer m_timer;
    int m_listenerId;
};

} // namespace

void watchUsbDevices(RekordboxFeature* pFeature) {
    new UsbDeviceWatcher(pFeature);
}
#else
// The USB notifications come with the controller support
void watchUsbDevices(RekordboxFeature*) {
}
#endif

} // namespace android
} // namespace mixxx

void RekordboxFeature::refreshDevices() {
    if (m_devicesFuture.isRunning()) {
        // Looks again once the current search is done
        QTimer::singleShot(1000, this, &RekordboxFeature::refreshDevices);
        return;
    }
    qInfo() << "USB devices changed, looking for Rekordbox devices again";
    m_devicesFuture = QtConcurrent::run(findRekordboxDevices);
    m_devicesFutureWatcher.setFuture(m_devicesFuture);
}
