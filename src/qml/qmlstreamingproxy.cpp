#include "qml/qmlstreamingproxy.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkCookie>
#include <QNetworkCookieJar>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrlQuery>
#include <openssl/blowfish.h>

#include "sources/metadatasourcetaglib.h"
#include "track/trackmetadata.h"
#include "util/fileinfo.h"
#include "util/sandbox.h"

#include "moc_qmlstreamingproxy.cpp"

namespace mixxx {
namespace qml {

namespace {
QByteArray computeDeezerBlowfishKey(const QString& sngId) {
    const char secret[] = "g4el58wc0zvf9na1";
    const QByteArray idBytes = sngId.toUtf8();
    const QByteArray md5Hash = QCryptographicHash::hash(idBytes, QCryptographicHash::Md5).toHex().toLower();
    QByteArray key(16, 0);
    for (int i = 0; i < 16; ++i) {
        key[i] = md5Hash.at(i) ^ md5Hash.at(i + 16) ^ secret[i];
    }
    return key;
}

void decryptDeezerDataInPlace(QByteArray& data, const QString& sngId) {
    const QByteArray key = computeDeezerBlowfishKey(sngId);
    BF_KEY bfKey;
    BF_set_key(&bfKey, 16, reinterpret_cast<const unsigned char*>(key.constData()));

    const int totalSize = data.size();
    const int chunkSize = 2048;
    int chunkIndex = 0;

    for (int offset = 0; offset + chunkSize <= totalSize; offset += chunkSize, ++chunkIndex) {
        if (chunkIndex % 3 == 0) {
            unsigned char iv[8] = {0, 1, 2, 3, 4, 5, 6, 7};
            unsigned char* chunkPtr = reinterpret_cast<unsigned char*>(data.data() + offset);
            BF_cbc_encrypt(chunkPtr, chunkPtr, chunkSize, &bfKey, iv, BF_DECRYPT);
        }
    }
}
const QString kStreamingGroup = QStringLiteral("[Streaming]");
const QString kSubsonicUrlKey = QStringLiteral("SubsonicUrl");
const QString kSubsonicUserKey = QStringLiteral("SubsonicUsername");
const QString kSubsonicPassKey = QStringLiteral("SubsonicPassword");
const QString kBeatportTokenKey = QStringLiteral("BeatportToken");
const QString kSoundCloudClientIdKey = QStringLiteral("SoundCloudClientId");
const QString kPreferFullTrackMatchingKey = QStringLiteral("PreferFullTrackMatching");
const QString kDeemixServerUrlKey = QStringLiteral("DeemixServerUrl");
const QString kDeezerArlKey = QStringLiteral("DeezerArl");
const QString kDefaultSoundCloudClientId = QStringLiteral("iN5ffSanbMS4rYYxVJVZcQU5jdq8Fjmz");
const QString kDefaultUserAgent = QStringLiteral("Mozilla/5.0 (Linux; Android 14; Mobile; Mixxx-DJ) AppleWebKit/537.36");
} // namespace

QmlStreamingProxy::QmlStreamingProxy(QObject* parent)
        : QObject(parent),
          m_pNetworkManager(new QNetworkAccessManager(this)) {
    // Determine the best cache directory for streaming tracks
#if defined(Q_OS_ANDROID)
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (baseDir.isEmpty()) {
        baseDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    }
    m_cacheDir = baseDir + QStringLiteral("/streaming_cache");
    QDir(m_cacheDir).mkpath(QStringLiteral("."));
#else
    m_cacheDir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/streaming");
    QDir(m_cacheDir).mkpath(QStringLiteral("."));
#endif

    m_soundCloudClientId = kDefaultSoundCloudClientId;

    // Load persisted settings if config is available
    if (s_pConfig) {
        m_subsonicUrl = s_pConfig->getValue(ConfigKey(kStreamingGroup, kSubsonicUrlKey), QString());
        m_subsonicUsername = s_pConfig->getValue(ConfigKey(kStreamingGroup, kSubsonicUserKey), QString());
        m_subsonicPassword = s_pConfig->getValue(ConfigKey(kStreamingGroup, kSubsonicPassKey), QString());
        m_beatportToken = s_pConfig->getValue(ConfigKey(kStreamingGroup, kBeatportTokenKey), QString());
        m_preferFullTrackMatching = s_pConfig->getValue(ConfigKey(kStreamingGroup, kPreferFullTrackMatchingKey), 1) != 0;
        m_deemixServerUrl = s_pConfig->getValue(ConfigKey(kStreamingGroup, kDeemixServerUrlKey), QString());
        m_deezerArl = s_pConfig->getValue(ConfigKey(kStreamingGroup, kDeezerArlKey), QString());
        if (m_deezerArl.isEmpty()) {
            QFile arlFile(QStringLiteral("/sdcard/Mixxx/deezer_arl.txt"));
            if (!arlFile.exists()) {
                arlFile.setFileName(QStringLiteral("/storage/emulated/0/Mixxx/deezer_arl.txt"));
            }
            if (arlFile.exists() && arlFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QString fileArl = QString::fromUtf8(arlFile.readAll()).trimmed();
                if (!fileArl.isEmpty()) {
                    setDeezerArl(fileArl);
                    qInfo() << "QmlStreamingProxy: ARL de Deezer cargada exitosamente desde archivo deezer_arl.txt";
                }
                arlFile.close();
            }
        }
        QString savedScId = s_pConfig->getValue(ConfigKey(kStreamingGroup, kSoundCloudClientIdKey), QString());
        if (!savedScId.isEmpty()) {
            m_soundCloudClientId = savedScId;
        }
    }
}

QmlStreamingProxy::~QmlStreamingProxy() {
    for (auto it = m_activeDownloads.begin(); it != m_activeDownloads.end(); ++it) {
        if (it.value()->pReply) {
            it.value()->pReply->abort();
        }
        if (it.value()->pFile && it.value()->pFile->isOpen()) {
            it.value()->pFile->close();
        }
        QFile::remove(it.value()->partPath);
    }
    m_activeDownloads.clear();
}

// static
QmlStreamingProxy* QmlStreamingProxy::create(QQmlEngine* pQmlEngine, [[maybe_unused]] QJSEngine* pJsEngine) {
    return new QmlStreamingProxy(pQmlEngine);
}

QString QmlStreamingProxy::subsonicUrl() const {
    return m_subsonicUrl;
}

void QmlStreamingProxy::setSubsonicUrl(const QString& url) {
    if (m_subsonicUrl != url) {
        m_subsonicUrl = url;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kSubsonicUrlKey), url);
        }
        emit subsonicUrlChanged();
    }
}

QString QmlStreamingProxy::subsonicUsername() const {
    return m_subsonicUsername;
}

void QmlStreamingProxy::setSubsonicUsername(const QString& username) {
    if (m_subsonicUsername != username) {
        m_subsonicUsername = username;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kSubsonicUserKey), username);
        }
        emit subsonicUsernameChanged();
    }
}

QString QmlStreamingProxy::subsonicPassword() const {
    return m_subsonicPassword;
}

void QmlStreamingProxy::setSubsonicPassword(const QString& password) {
    if (m_subsonicPassword != password) {
        m_subsonicPassword = password;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kSubsonicPassKey), password);
        }
        emit subsonicPasswordChanged();
    }
}

void QmlStreamingProxy::setBeatportToken(const QString& token) {
    if (m_beatportToken != token) {
        m_beatportToken = token;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kBeatportTokenKey), token);
        }
        emit beatportTokenChanged();
    }
}

void QmlStreamingProxy::setSoundCloudClientId(const QString& clientId) {
    if (m_soundCloudClientId != clientId) {
        m_soundCloudClientId = clientId;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kSoundCloudClientIdKey), clientId);
        }
        emit soundCloudClientIdChanged();
    }
}

void QmlStreamingProxy::setPreferFullTrackMatching(bool enable) {
    if (m_preferFullTrackMatching != enable) {
        m_preferFullTrackMatching = enable;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kPreferFullTrackMatchingKey), enable ? 1 : 0);
        }
        emit preferFullTrackMatchingChanged();
    }
}

void QmlStreamingProxy::setDeemixServerUrl(const QString& url) {
    if (m_deemixServerUrl != url) {
        m_deemixServerUrl = url;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kDeemixServerUrlKey), url);
        }
        emit deemixServerUrlChanged();
    }
}

void QmlStreamingProxy::setDeezerArl(const QString& arl) {
    if (m_deezerArl != arl) {
        m_deezerArl = arl;
        if (s_pConfig) {
            s_pConfig->setValue(ConfigKey(kStreamingGroup, kDeezerArlKey), arl);
        }
        emit deezerArlChanged();
    }
}

QString QmlStreamingProxy::sanitizeFileName(const QString& input) const {
    QString sanitized = input;
    sanitized.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    sanitized.replace(QRegularExpression(QStringLiteral("[\\x00-\\x1f]")), QStringLiteral(""));
    return sanitized.trimmed();
}

QString QmlStreamingProxy::buildTrackFileName(
        const QString& service,
        const QString& trackId,
        const QString& title,
        const QString& artist,
        const QString& ext) const {
    QString displayName;
    if (!artist.isEmpty() && !title.isEmpty()) {
        displayName = QStringLiteral("%1 - %2").arg(artist.trimmed(), title.trimmed());
    } else if (!title.isEmpty()) {
        displayName = title.trimmed();
    } else {
        displayName = QStringLiteral("%1_%2").arg(service, trackId);
    }
    const QString safeName = sanitizeFileName(displayName);
    const QString safeId = sanitizeFileName(trackId);
    return QStringLiteral("%1 [%2_%3]%4").arg(safeName, service, safeId, ext);
}

QString QmlStreamingProxy::buildSubsonicUrl(const QString& endpoint, const QString& extraParams) const {
    if (m_subsonicUrl.trimmed().isEmpty()) {
        return QString();
    }
    QString base = m_subsonicUrl.trimmed();
    while (base.endsWith(QLatin1Char('/'))) {
        base.chop(1);
    }
    if (!base.startsWith(QStringLiteral("http://")) && !base.startsWith(QStringLiteral("https://"))) {
        base.prepend(QStringLiteral("http://"));
    }

    const QString salt = QString::number(QRandomGenerator::global()->generate(), 16);
    const QString token = QString::fromLatin1(
            QCryptographicHash::hash((m_subsonicPassword + salt).toUtf8(), QCryptographicHash::Md5).toHex());

    QString fullUrl = QStringLiteral("%1/rest/%2.view?u=%3&t=%4&s=%5&v=1.16.1&c=MixxxAndroid&f=json")
            .arg(base, endpoint, QString::fromUtf8(QUrl::toPercentEncoding(m_subsonicUsername)), token, salt);

    if (!extraParams.isEmpty()) {
        fullUrl += QLatin1Char('&') + extraParams;
    }
    return fullUrl;
}

QString QmlStreamingProxy::getSubsonicStreamUrl(const QString& songId) const {
    return buildSubsonicUrl(QStringLiteral("stream"), QStringLiteral("id=") + songId);
}

QString QmlStreamingProxy::getSubsonicCoverArtUrl(const QString& coverArtId, int size) const {
    if (coverArtId.isEmpty()) {
        return QString();
    }
    return buildSubsonicUrl(QStringLiteral("getCoverArt"),
            QStringLiteral("id=%1&size=%2").arg(coverArtId).arg(size));
}

void QmlStreamingProxy::testSubsonicConnection() {
    const QString urlStr = buildSubsonicUrl(QStringLiteral("ping"));
    if (urlStr.isEmpty()) {
        emit subsonicConnectionTested(false, tr("The server URL is empty"), QString());
        return;
    }

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            m_isSubsonicConnected = false;
            emit subsonicConnectionChanged();
            emit subsonicConnectionTested(false, reply->errorString(), QString());
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject root = doc.object();
        QJsonObject resp = root.value(QStringLiteral("subsonic-response")).toObject();
        QString status = resp.value(QStringLiteral("status")).toString();

        if (status == QStringLiteral("ok")) {
            m_isSubsonicConnected = true;
            QString version = resp.value(QStringLiteral("serverVersion")).toString();
            if (version.isEmpty()) {
                version = resp.value(QStringLiteral("version")).toString();
            }
            m_subsonicServerVersion = version;
            emit subsonicConnectionChanged();
            emit subsonicConnectionTested(true, tr("Connection established successfully!"), version);
        } else {
            m_isSubsonicConnected = false;
            emit subsonicConnectionChanged();
            QJsonObject err = resp.value(QStringLiteral("error")).toObject();
            QString msg = err.value(QStringLiteral("message")).toString(tr("Authentication or server error"));
            emit subsonicConnectionTested(false, msg, QString());
        }
    });
}

void QmlStreamingProxy::searchSubsonic(const QString& query) {
    if (query.trimmed().isEmpty()) {
        emit subsonicSearchFinished(true, QVariantList(), QString());
        return;
    }

    const QString extra = QStringLiteral("query=") + QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed()));
    const QString urlStr = buildSubsonicUrl(QStringLiteral("search3"), extra);
    if (urlStr.isEmpty()) {
        emit subsonicSearchFinished(false, QVariantList(), tr("Set up the Navidrome/Subsonic URL"));
        return;
    }

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit subsonicSearchFinished(false, QVariantList(), reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject resp = doc.object().value(QStringLiteral("subsonic-response")).toObject();
        QJsonObject searchResult = resp.value(QStringLiteral("searchResult3")).toObject();
        QJsonArray songs = searchResult.value(QStringLiteral("song")).toArray();

        QVariantList resultList;
        for (const auto& songVal : songs) {
            QJsonObject song = songVal.toObject();
            QVariantMap item;
            QString id = song.value(QStringLiteral("id")).toString();
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = song.value(QStringLiteral("title")).toString(tr("Untitled"));
            item[QStringLiteral("artist")] = song.value(QStringLiteral("artist")).toString(tr("Unknown artist"));
            item[QStringLiteral("album")] = song.value(QStringLiteral("album")).toString();
            item[QStringLiteral("duration")] = song.value(QStringLiteral("duration")).toInt();
            item[QStringLiteral("suffix")] = song.value(QStringLiteral("suffix")).toString(QStringLiteral("mp3")).toUpper();
            item[QStringLiteral("bitRate")] = song.value(QStringLiteral("bitRate")).toInt();
            item[QStringLiteral("size")] = song.value(QStringLiteral("size")).toVariant().toLongLong();
            item[QStringLiteral("coverArt")] = song.value(QStringLiteral("coverArt")).toString();
            item[QStringLiteral("coverArtUrl")] = getSubsonicCoverArtUrl(song.value(QStringLiteral("coverArt")).toString(), 160);
            item[QStringLiteral("streamUrl")] = getSubsonicStreamUrl(id);
            item[QStringLiteral("service")] = QStringLiteral("subsonic");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("subsonic"), id);
            resultList.append(item);
        }

        emit subsonicSearchFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::getSubsonicPlaylists() {
    const QString urlStr = buildSubsonicUrl(QStringLiteral("getPlaylists"));
    if (urlStr.isEmpty()) {
        emit subsonicPlaylistsFinished(false, QVariantList(), tr("Set up the Navidrome/Subsonic URL"));
        return;
    }

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit subsonicPlaylistsFinished(false, QVariantList(), reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject resp = doc.object().value(QStringLiteral("subsonic-response")).toObject();
        QJsonObject playlistsObj = resp.value(QStringLiteral("playlists")).toObject();
        QJsonArray playlists = playlistsObj.value(QStringLiteral("playlist")).toArray();

        QVariantList resultList;
        for (const auto& plVal : playlists) {
            QJsonObject pl = plVal.toObject();
            QVariantMap item;
            item[QStringLiteral("id")] = pl.value(QStringLiteral("id")).toString();
            item[QStringLiteral("name")] = pl.value(QStringLiteral("name")).toString();
            item[QStringLiteral("songCount")] = pl.value(QStringLiteral("songCount")).toInt();
            item[QStringLiteral("duration")] = pl.value(QStringLiteral("duration")).toInt();
            resultList.append(item);
        }

        emit subsonicPlaylistsFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::getSubsonicPlaylistSongs(const QString& playlistId) {
    const QString extra = QStringLiteral("id=") + playlistId;
    const QString urlStr = buildSubsonicUrl(QStringLiteral("getPlaylist"), extra);
    if (urlStr.isEmpty()) {
        emit subsonicPlaylistSongsFinished(false, QVariantList(), tr("Set up the Navidrome/Subsonic URL"));
        return;
    }

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit subsonicPlaylistSongsFinished(false, QVariantList(), reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject resp = doc.object().value(QStringLiteral("subsonic-response")).toObject();
        QJsonObject playlistObj = resp.value(QStringLiteral("playlist")).toObject();
        QJsonArray entries = playlistObj.value(QStringLiteral("entry")).toArray();

        QVariantList resultList;
        for (const auto& songVal : entries) {
            QJsonObject song = songVal.toObject();
            QVariantMap item;
            QString id = song.value(QStringLiteral("id")).toString();
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = song.value(QStringLiteral("title")).toString(tr("Untitled"));
            item[QStringLiteral("artist")] = song.value(QStringLiteral("artist")).toString(tr("Unknown artist"));
            item[QStringLiteral("album")] = song.value(QStringLiteral("album")).toString();
            item[QStringLiteral("duration")] = song.value(QStringLiteral("duration")).toInt();
            item[QStringLiteral("suffix")] = song.value(QStringLiteral("suffix")).toString(QStringLiteral("mp3")).toUpper();
            item[QStringLiteral("bitRate")] = song.value(QStringLiteral("bitRate")).toInt();
            item[QStringLiteral("coverArt")] = song.value(QStringLiteral("coverArt")).toString();
            item[QStringLiteral("coverArtUrl")] = getSubsonicCoverArtUrl(song.value(QStringLiteral("coverArt")).toString(), 160);
            item[QStringLiteral("streamUrl")] = getSubsonicStreamUrl(id);
            item[QStringLiteral("service")] = QStringLiteral("subsonic");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("subsonic"), id);
            resultList.append(item);
        }

        emit subsonicPlaylistSongsFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::searchAudius(const QString& query) {
    if (query.trimmed().isEmpty()) {
        getAudiusTrending();
        return;
    }

    const QString urlStr = QStringLiteral("https://discoveryprovider.audius.co/v1/tracks/search?query=%1&app_name=MixxxAndroid")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())));

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit audiusSearchFinished(false, QVariantList(), reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray tracks = doc.object().value(QStringLiteral("data")).toArray();

        QVariantList resultList;
        for (const auto& tVal : tracks) {
            QJsonObject t = tVal.toObject();
            QJsonObject user = t.value(QStringLiteral("user")).toObject();
            QString id = t.value(QStringLiteral("id")).toString();
            if (id.isEmpty() && t.contains(QStringLiteral("track_id"))) {
                id = QString::number(t.value(QStringLiteral("track_id")).toInteger());
            }
            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("Audius track"));
            item[QStringLiteral("artist")] = user.value(QStringLiteral("name")).toString(tr("Audius artist"));
            item[QStringLiteral("duration")] = t.value(QStringLiteral("duration")).toInt();
            item[QStringLiteral("genre")] = t.value(QStringLiteral("genre")).toString(QStringLiteral("Electronic"));
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(126.0);
            item[QStringLiteral("key")] = t.value(QStringLiteral("musical_key")).toString();
            QJsonObject artwork = t.value(QStringLiteral("artwork")).toObject();
            QString artUrl = artwork.value(QStringLiteral("150x150")).toString();
            if (artUrl.isEmpty()) {
                artUrl = artwork.value(QStringLiteral("480x480")).toString();
            }
            item[QStringLiteral("coverArtUrl")] = artUrl;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = QStringLiteral("https://discoveryprovider.audius.co/v1/tracks/%1/stream?app_name=MixxxAndroid").arg(id);
            item[QStringLiteral("service")] = QStringLiteral("audius");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("audius"), id);
            resultList.append(item);
        }

        emit audiusSearchFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::getAudiusTrending(const QString& genre) {
    const QString urlStr = QStringLiteral("https://discoveryprovider.audius.co/v1/tracks/trending?genre=%1&app_name=MixxxAndroid")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(genre)));

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit audiusTrendingFinished(false, QVariantList(), reply->errorString());
            return;
        }

        const QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray tracks = doc.object().value(QStringLiteral("data")).toArray();

        QVariantList resultList;
        for (const auto& tVal : tracks) {
            QJsonObject t = tVal.toObject();
            QJsonObject user = t.value(QStringLiteral("user")).toObject();
            QString id = t.value(QStringLiteral("id")).toString();
            if (id.isEmpty() && t.contains(QStringLiteral("track_id"))) {
                id = QString::number(t.value(QStringLiteral("track_id")).toInteger());
            }
            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("Audius track"));
            item[QStringLiteral("artist")] = user.value(QStringLiteral("name")).toString(tr("Audius artist"));
            item[QStringLiteral("duration")] = t.value(QStringLiteral("duration")).toInt();
            item[QStringLiteral("genre")] = t.value(QStringLiteral("genre")).toString(QStringLiteral("Electronic"));
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(126.0);
            item[QStringLiteral("key")] = t.value(QStringLiteral("musical_key")).toString();
            QJsonObject artwork = t.value(QStringLiteral("artwork")).toObject();
            QString artUrl = artwork.value(QStringLiteral("150x150")).toString();
            if (artUrl.isEmpty()) {
                artUrl = artwork.value(QStringLiteral("480x480")).toString();
            }
            item[QStringLiteral("coverArtUrl")] = artUrl;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = QStringLiteral("https://discoveryprovider.audius.co/v1/tracks/%1/stream?app_name=MixxxAndroid").arg(id);
            item[QStringLiteral("service")] = QStringLiteral("audius");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("audius"), id);
            resultList.append(item);
        }

        emit audiusTrendingFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::fetchSoundCloudClientId(std::function<void(const QString&)> callback) {
    QNetworkRequest request{QUrl(QStringLiteral("https://soundcloud.com"))};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, callback]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            callback(m_soundCloudClientId);
            return;
        }
        QString html = QString::fromUtf8(reply->readAll());
        static const QRegularExpression scriptRe(QStringLiteral(R"raw(<script[^>]+src="(https?://[^"]+\.js)")raw"));
        QRegularExpressionMatchIterator it = scriptRe.globalMatch(html);
        QStringList scriptUrls;
        while (it.hasNext()) {
            scriptUrls.append(it.next().captured(1));
        }
        if (scriptUrls.isEmpty()) {
            callback(m_soundCloudClientId);
            return;
        }

        auto checkNextScript = std::make_shared<std::function<void(int)>>();
        *checkNextScript = [this, scriptUrls, checkNextScript, callback](int index) {
            if (index < 0 || index < scriptUrls.size() - 6) {
                callback(m_soundCloudClientId);
                return;
            }
            QNetworkRequest scriptReq{QUrl(scriptUrls[index])};
            scriptReq.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
            QNetworkReply* sReply = m_pNetworkManager->get(scriptReq);
            connect(sReply, &QNetworkReply::finished, this, [this, sReply, index, checkNextScript, callback]() {
                sReply->deleteLater();
                if (sReply->error() == QNetworkReply::NoError) {
                    QString js = QString::fromUtf8(sReply->readAll());
                    static const QRegularExpression clientIdRe(QStringLiteral(R"raw(client_id\s*[:=]\s*"([A-Za-z0-9]{32})")raw"));
                    QRegularExpressionMatch m = clientIdRe.match(js);
                    if (m.hasMatch()) {
                        setSoundCloudClientId(m.captured(1));
                        callback(m.captured(1));
                        return;
                    }
                }
                (*checkNextScript)(index - 1);
            });
        };
        (*checkNextScript)(scriptUrls.size() - 1);
    });
}

void QmlStreamingProxy::searchSoundCloud(const QString& query) {
    if (query.trimmed().isEmpty()) {
        getSoundCloudTrending();
        return;
    }

    QString clientId = m_soundCloudClientId.isEmpty() ? kDefaultSoundCloudClientId : m_soundCloudClientId;
    const QString urlStr = QStringLiteral("https://api-v2.soundcloud.com/search/tracks?q=%1&limit=50&client_id=%2")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())), clientId);

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, query]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            if (statusCode == 401 || statusCode == 403) {
                fetchSoundCloudClientId([this, query](const QString&) {
                    QString clientId = m_soundCloudClientId.isEmpty() ? kDefaultSoundCloudClientId : m_soundCloudClientId;
                    const QString retryUrl = QStringLiteral("https://api-v2.soundcloud.com/search/tracks?q=%1&limit=50&client_id=%2")
                            .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())), clientId);
                    QNetworkRequest req{QUrl(retryUrl)};
                    req.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
                    QNetworkReply* rReply = m_pNetworkManager->get(req);
                    connect(rReply, &QNetworkReply::finished, this, [this, rReply]() {
                        rReply->deleteLater();
                        if (rReply->error() != QNetworkReply::NoError) {
                            emit soundCloudSearchFinished(false, QVariantList(), rReply->errorString());
                            return;
                        }
                        QJsonObject rootObj = QJsonDocument::fromJson(rReply->readAll()).object();
                        QJsonArray collection = rootObj.value(QStringLiteral("collection")).toArray();
                        QVariantList resultList;
                        for (const auto& itemVal : collection) {
                            QJsonObject t = itemVal.toObject();
                            QString id = QString::number(t.value(QStringLiteral("id")).toInteger());
                            if (id == QStringLiteral("0") || id.isEmpty()) continue;
                            QJsonObject user = t.value(QStringLiteral("user")).toObject();
                            QJsonObject media = t.value(QStringLiteral("media")).toObject();
                            QJsonArray transcodings = media.value(QStringLiteral("transcodings")).toArray();
                            QString streamUrl;
                            for (const auto& tcVal : transcodings) {
                                QJsonObject tc = tcVal.toObject();
                                QJsonObject format = tc.value(QStringLiteral("format")).toObject();
                                QString protocol = format.value(QStringLiteral("protocol")).toString();
                                QString mimeType = format.value(QStringLiteral("mime_type")).toString();
                                if (protocol == QStringLiteral("progressive") && mimeType.startsWith(QStringLiteral("audio/mpeg"))) {
                                    streamUrl = tc.value(QStringLiteral("url")).toString();
                                    break;
                                }
                                if (streamUrl.isEmpty()) {
                                    streamUrl = tc.value(QStringLiteral("url")).toString();
                                }
                            }
                            QVariantMap item;
                            item[QStringLiteral("id")] = id;
                            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("SoundCloud track"));
                            item[QStringLiteral("artist")] = user.value(QStringLiteral("username")).toString(tr("SoundCloud artist"));
                            item[QStringLiteral("duration")] = static_cast<int>(t.value(QStringLiteral("duration")).toInteger() / 1000);
                            item[QStringLiteral("genre")] = t.value(QStringLiteral("genre")).toString(QStringLiteral("Electronic"));
                            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(0.0);
                            QString artUrl = t.value(QStringLiteral("artwork_url")).toString();
                            if (artUrl.isEmpty()) {
                                artUrl = user.value(QStringLiteral("avatar_url")).toString();
                            }
                            item[QStringLiteral("coverArtUrl")] = artUrl;
                            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
                            item[QStringLiteral("streamUrl")] = streamUrl;
                            item[QStringLiteral("service")] = QStringLiteral("soundcloud");
                            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("soundcloud"), id);
                            resultList.append(item);
                        }
                        emit soundCloudSearchFinished(true, resultList, QString());
                    });
                });
                return;
            }
            emit soundCloudSearchFinished(false, QVariantList(), reply->errorString());
            return;
        }

        QJsonObject rootObj = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray collection = rootObj.value(QStringLiteral("collection")).toArray();
        QVariantList resultList;
        for (const auto& itemVal : collection) {
            QJsonObject t = itemVal.toObject();
            QString id = QString::number(t.value(QStringLiteral("id")).toInteger());
            if (id == QStringLiteral("0") || id.isEmpty()) continue;
            QJsonObject user = t.value(QStringLiteral("user")).toObject();
            QJsonObject media = t.value(QStringLiteral("media")).toObject();
            QJsonArray transcodings = media.value(QStringLiteral("transcodings")).toArray();
            QString streamUrl;
            for (const auto& tcVal : transcodings) {
                QJsonObject tc = tcVal.toObject();
                QJsonObject format = tc.value(QStringLiteral("format")).toObject();
                QString protocol = format.value(QStringLiteral("protocol")).toString();
                QString mimeType = format.value(QStringLiteral("mime_type")).toString();
                if (protocol == QStringLiteral("progressive") && mimeType.startsWith(QStringLiteral("audio/mpeg"))) {
                    streamUrl = tc.value(QStringLiteral("url")).toString();
                    break;
                }
                if (streamUrl.isEmpty()) {
                    streamUrl = tc.value(QStringLiteral("url")).toString();
                }
            }
            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("SoundCloud track"));
            item[QStringLiteral("artist")] = user.value(QStringLiteral("username")).toString(tr("SoundCloud artist"));
            item[QStringLiteral("duration")] = static_cast<int>(t.value(QStringLiteral("duration")).toInteger() / 1000);
            item[QStringLiteral("genre")] = t.value(QStringLiteral("genre")).toString(QStringLiteral("Electronic"));
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(0.0);
            QString artUrl = t.value(QStringLiteral("artwork_url")).toString();
            if (artUrl.isEmpty()) {
                artUrl = user.value(QStringLiteral("avatar_url")).toString();
            }
            item[QStringLiteral("coverArtUrl")] = artUrl;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = streamUrl;
            item[QStringLiteral("service")] = QStringLiteral("soundcloud");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("soundcloud"), id);
            resultList.append(item);
        }
        emit soundCloudSearchFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::getSoundCloudTrending() {
    QString clientId = m_soundCloudClientId.isEmpty() ? kDefaultSoundCloudClientId : m_soundCloudClientId;
    const QString urlStr = QStringLiteral("https://api-v2.soundcloud.com/search/tracks?q=electronic+trending+remix&limit=50&client_id=%1")
            .arg(clientId);

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit soundCloudTrendingFinished(false, QVariantList(), reply->errorString());
            return;
        }

        QJsonObject rootObj = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray collection = rootObj.value(QStringLiteral("collection")).toArray();
        QVariantList resultList;
        for (const auto& itemVal : collection) {
            QJsonObject t = itemVal.toObject();
            QString id = QString::number(t.value(QStringLiteral("id")).toInteger());
            if (id == QStringLiteral("0") || id.isEmpty()) continue;
            QJsonObject user = t.value(QStringLiteral("user")).toObject();
            QJsonObject media = t.value(QStringLiteral("media")).toObject();
            QJsonArray transcodings = media.value(QStringLiteral("transcodings")).toArray();
            QString streamUrl;
            for (const auto& tcVal : transcodings) {
                QJsonObject tc = tcVal.toObject();
                QJsonObject format = tc.value(QStringLiteral("format")).toObject();
                QString protocol = format.value(QStringLiteral("protocol")).toString();
                QString mimeType = format.value(QStringLiteral("mime_type")).toString();
                if (protocol == QStringLiteral("progressive") && mimeType.startsWith(QStringLiteral("audio/mpeg"))) {
                    streamUrl = tc.value(QStringLiteral("url")).toString();
                    break;
                }
                if (streamUrl.isEmpty()) {
                    streamUrl = tc.value(QStringLiteral("url")).toString();
                }
            }
            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("SoundCloud track"));
            item[QStringLiteral("artist")] = user.value(QStringLiteral("username")).toString(tr("SoundCloud artist"));
            item[QStringLiteral("duration")] = static_cast<int>(t.value(QStringLiteral("duration")).toInteger() / 1000);
            item[QStringLiteral("genre")] = t.value(QStringLiteral("genre")).toString(QStringLiteral("Electronic"));
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(0.0);
            QString artUrl = t.value(QStringLiteral("artwork_url")).toString();
            if (artUrl.isEmpty()) {
                artUrl = user.value(QStringLiteral("avatar_url")).toString();
            }
            item[QStringLiteral("coverArtUrl")] = artUrl;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = streamUrl;
            item[QStringLiteral("service")] = QStringLiteral("soundcloud");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("soundcloud"), id);
            resultList.append(item);
        }
        emit soundCloudTrendingFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::searchDeezer(const QString& query) {
    if (query.trimmed().isEmpty()) {
        getDeezerChart();
        return;
    }

    const QString urlStr = QStringLiteral("https://api.deezer.com/search?q=%1&limit=50")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())));

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit deezerSearchFinished(false, QVariantList(), reply->errorString());
            return;
        }

        QJsonObject rootObj = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray tracks = rootObj.value(QStringLiteral("data")).toArray();
        QVariantList resultList;
        for (const auto& itemVal : tracks) {
            QJsonObject t = itemVal.toObject();
            QString id = QString::number(t.value(QStringLiteral("id")).toInteger());
            if (id == QStringLiteral("0") || id.isEmpty()) continue;
            QJsonObject artistObj = t.value(QStringLiteral("artist")).toObject();
            QJsonObject albumObj = t.value(QStringLiteral("album")).toObject();

            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("Deezer track"));
            item[QStringLiteral("artist")] = artistObj.value(QStringLiteral("name")).toString(tr("Deezer artist"));
            item[QStringLiteral("album")] = albumObj.value(QStringLiteral("title")).toString();
            item[QStringLiteral("duration")] = t.value(QStringLiteral("duration")).toInt();
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(0.0);
            QString cover = albumObj.value(QStringLiteral("cover_medium")).toString();
            if (cover.isEmpty()) {
                cover = albumObj.value(QStringLiteral("cover_big")).toString();
            }
            item[QStringLiteral("coverArtUrl")] = cover;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = t.value(QStringLiteral("preview")).toString();
            item[QStringLiteral("service")] = QStringLiteral("deezer");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("deezer"), id);
            resultList.append(item);
        }
        emit deezerSearchFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::getDeezerChart() {
    const QString urlStr = QStringLiteral("https://api.deezer.com/chart/0/tracks?limit=50");
    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit deezerChartFinished(false, QVariantList(), reply->errorString());
            return;
        }

        QJsonObject rootObj = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray tracks = rootObj.value(QStringLiteral("data")).toArray();
        QVariantList resultList;
        for (const auto& itemVal : tracks) {
            QJsonObject t = itemVal.toObject();
            QString id = QString::number(t.value(QStringLiteral("id")).toInteger());
            if (id == QStringLiteral("0") || id.isEmpty()) continue;
            QJsonObject artistObj = t.value(QStringLiteral("artist")).toObject();
            QJsonObject albumObj = t.value(QStringLiteral("album")).toObject();

            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("Deezer track"));
            item[QStringLiteral("artist")] = artistObj.value(QStringLiteral("name")).toString(tr("Deezer artist"));
            item[QStringLiteral("album")] = albumObj.value(QStringLiteral("title")).toString();
            item[QStringLiteral("duration")] = t.value(QStringLiteral("duration")).toInt();
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(0.0);
            QString cover = albumObj.value(QStringLiteral("cover_medium")).toString();
            if (cover.isEmpty()) {
                cover = albumObj.value(QStringLiteral("cover_big")).toString();
            }
            item[QStringLiteral("coverArtUrl")] = cover;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = t.value(QStringLiteral("preview")).toString();
            item[QStringLiteral("service")] = QStringLiteral("deezer");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("deezer"), id);
            resultList.append(item);
        }
        emit deezerChartFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::getDeezerPlaylist(const QString& playlistId) {
    const QString urlStr = QStringLiteral("https://api.deezer.com/playlist/%1").arg(playlistId);
    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit deezerPlaylistFinished(false, QString(), QVariantList(), reply->errorString());
            return;
        }

        QJsonObject rootObj = QJsonDocument::fromJson(reply->readAll()).object();
        QString playlistTitle = rootObj.value(QStringLiteral("title")).toString();
        QJsonObject tracksObj = rootObj.value(QStringLiteral("tracks")).toObject();
        QJsonArray tracks = tracksObj.value(QStringLiteral("data")).toArray();
        QVariantList resultList;
        for (const auto& itemVal : tracks) {
            QJsonObject t = itemVal.toObject();
            QString id = QString::number(t.value(QStringLiteral("id")).toInteger());
            if (id == QStringLiteral("0") || id.isEmpty()) continue;
            QJsonObject artistObj = t.value(QStringLiteral("artist")).toObject();
            QJsonObject albumObj = t.value(QStringLiteral("album")).toObject();

            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = t.value(QStringLiteral("title")).toString(tr("Deezer track"));
            item[QStringLiteral("artist")] = artistObj.value(QStringLiteral("name")).toString(tr("Deezer artist"));
            item[QStringLiteral("album")] = albumObj.value(QStringLiteral("title")).toString();
            item[QStringLiteral("duration")] = t.value(QStringLiteral("duration")).toInt();
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(0.0);
            QString cover = albumObj.value(QStringLiteral("cover_medium")).toString();
            if (cover.isEmpty()) {
                cover = albumObj.value(QStringLiteral("cover_big")).toString();
            }
            item[QStringLiteral("coverArtUrl")] = cover;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = t.value(QStringLiteral("preview")).toString();
            item[QStringLiteral("service")] = QStringLiteral("deezer");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("deezer"), id);
            resultList.append(item);
        }
        emit deezerPlaylistFinished(true, playlistTitle, resultList, QString());
    });
}

void QmlStreamingProxy::searchBeatport(const QString& query) {
    if (query.trimmed().isEmpty()) {
        getBeatportTopTracks();
        return;
    }

    const QString urlStr = QStringLiteral("https://api.beatport.com/v4/catalog/search/?q=%1&is_available_for_streaming=true&per_page=50")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(query.trimmed())));

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
    if (!m_beatportToken.trimmed().isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_beatportToken.trimmed().toUtf8());
    }

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit beatportSearchFinished(false, QVariantList(), reply->errorString());
            return;
        }

        QJsonObject rootObj = QJsonDocument::fromJson(reply->readAll()).object();
        QJsonArray tracks = rootObj.value(QStringLiteral("tracks")).toArray();
        QVariantList resultList;
        for (const auto& itemVal : tracks) {
            QJsonObject t = itemVal.toObject();
            QString id = QString::number(t.value(QStringLiteral("id")).toInteger());
            if (id == QStringLiteral("0") || id.isEmpty()) continue;

            QString title = t.value(QStringLiteral("name")).toString(tr("Beatport track"));
            QString mixName = t.value(QStringLiteral("mix_name")).toString();
            if (!mixName.isEmpty() && mixName != QStringLiteral("Original Mix")) {
                title += QStringLiteral(" (") + mixName + QStringLiteral(")");
            }

            QStringList artistNames;
            QJsonArray artists = t.value(QStringLiteral("artists")).toArray();
            for (const auto& aVal : artists) {
                artistNames.append(aVal.toObject().value(QStringLiteral("name")).toString());
            }
            QString artist = artistNames.join(QStringLiteral(", "));

            QJsonObject keyObj = t.value(QStringLiteral("key")).toObject();
            QString key = QString::number(keyObj.value(QStringLiteral("camelot_number")).toInt()) +
                          keyObj.value(QStringLiteral("camelot_letter")).toString();

            QJsonObject releaseObj = t.value(QStringLiteral("release")).toObject();
            QJsonObject imageObj = releaseObj.value(QStringLiteral("image")).toObject();
            QString cover = imageObj.value(QStringLiteral("dynamic_uri")).toString();
            cover.replace(QStringLiteral("{w}x{h}"), QStringLiteral("300x300"));

            QVariantMap item;
            item[QStringLiteral("id")] = id;
            item[QStringLiteral("title")] = title;
            item[QStringLiteral("artist")] = artist.isEmpty() ? tr("Beatport artist") : artist;
            item[QStringLiteral("album")] = releaseObj.value(QStringLiteral("name")).toString();
            item[QStringLiteral("duration")] = static_cast<int>(t.value(QStringLiteral("length_ms")).toInteger() / 1000);
            item[QStringLiteral("bpm")] = t.value(QStringLiteral("bpm")).toDouble(0.0);
            item[QStringLiteral("key")] = key;
            item[QStringLiteral("coverArtUrl")] = cover;
            item[QStringLiteral("suffix")] = QStringLiteral("MP3");
            item[QStringLiteral("streamUrl")] = t.value(QStringLiteral("sample_url")).toString();
            item[QStringLiteral("service")] = QStringLiteral("beatport");
            item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("beatport"), id);
            resultList.append(item);
        }
        emit beatportSearchFinished(true, resultList, QString());
    });
}

void QmlStreamingProxy::getBeatportTopTracks() {
    searchBeatport(QStringLiteral("Electronic Top"));
}

void QmlStreamingProxy::importPlaylistLink(const QString& url) {
    const QString trimmed = url.trimmed();
    if (trimmed.isEmpty()) {
        emit playlistImportFinished(false, QString(), QString(), QVariantList(), tr("URL is empty"));
        return;
    }

    if (trimmed.contains(QStringLiteral("spotify.com")) || trimmed.startsWith(QStringLiteral("spotify:"))) {
        static const QRegularExpression spRegex(QStringLiteral(R"((playlist|album)[/:]([0-9A-Za-z]{22}))"));
        QRegularExpressionMatch match = spRegex.match(trimmed);
        if (!match.hasMatch()) {
            emit playlistImportFinished(false, QStringLiteral("spotify"), QString(), QVariantList(), tr("Invalid Spotify playlist/album link"));
            return;
        }
        QString kind = match.captured(1);
        QString id = match.captured(2);
        QString embedUrl = QStringLiteral("https://open.spotify.com/embed/%1/%2").arg(kind, id);

        QNetworkRequest request{QUrl(embedUrl)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

        QNetworkReply* reply = m_pNetworkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit playlistImportFinished(false, QStringLiteral("spotify"), QString(), QVariantList(), reply->errorString());
                return;
            }

            QString html = QString::fromUtf8(reply->readAll());
            static const QRegularExpression nextDataRe(QStringLiteral(R"(<script id="__NEXT_DATA__"[^>]*>(.*?)</script>)"));
            QRegularExpressionMatch m = nextDataRe.match(html);
            if (!m.hasMatch()) {
                emit playlistImportFinished(false, QStringLiteral("spotify"), QString(), QVariantList(), tr("Could not read Spotify playlist metadata"));
                return;
            }

            QJsonDocument doc = QJsonDocument::fromJson(m.captured(1).toUtf8());
            QJsonObject entity = doc.object().value(QStringLiteral("props")).toObject()
                    .value(QStringLiteral("pageProps")).toObject()
                    .value(QStringLiteral("state")).toObject()
                    .value(QStringLiteral("data")).toObject()
                    .value(QStringLiteral("entity")).toObject();

            QString playlistTitle = entity.value(QStringLiteral("name")).toString();
            if (playlistTitle.isEmpty()) {
                playlistTitle = entity.value(QStringLiteral("title")).toString(tr("Spotify Playlist"));
            }

            QString defaultCover;
            QJsonObject coverArt = entity.value(QStringLiteral("coverArt")).toObject();
            QJsonArray sources = coverArt.value(QStringLiteral("sources")).toArray();
            if (!sources.isEmpty()) {
                defaultCover = sources.first().toObject().value(QStringLiteral("url")).toString();
            }

            QJsonArray trackList = entity.value(QStringLiteral("trackList")).toArray();
            QVariantList resultList;
            for (const auto& tVal : trackList) {
                QJsonObject t = tVal.toObject();
                QString trackId = t.value(QStringLiteral("uid")).toString();
                if (trackId.isEmpty()) {
                    trackId = t.value(QStringLiteral("uri")).toString();
                }
                QString title = t.value(QStringLiteral("title")).toString();
                QString artist = t.value(QStringLiteral("subtitle")).toString();
                int duration = static_cast<int>(t.value(QStringLiteral("duration")).toInteger() / 1000);
                QJsonObject audioPreview = t.value(QStringLiteral("audioPreview")).toObject();
                QString previewUrl = audioPreview.value(QStringLiteral("url")).toString();

                QVariantMap item;
                item[QStringLiteral("id")] = trackId;
                item[QStringLiteral("title")] = title;
                item[QStringLiteral("artist")] = artist;
                item[QStringLiteral("duration")] = duration;
                item[QStringLiteral("bpm")] = 0.0;
                item[QStringLiteral("coverArtUrl")] = defaultCover;
                item[QStringLiteral("suffix")] = QStringLiteral("MP3");
                item[QStringLiteral("streamUrl")] = previewUrl;
                item[QStringLiteral("service")] = QStringLiteral("spotify");
                item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("spotify"), trackId);
                resultList.append(item);
            }
            emit playlistImportFinished(true, QStringLiteral("spotify"), playlistTitle, resultList, QString());
        });
        return;
    }

    if (trimmed.contains(QStringLiteral("music.apple.com"))) {
        QNetworkRequest request{QUrl(trimmed)};
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
        request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

        QNetworkReply* reply = m_pNetworkManager->get(request);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) {
                emit playlistImportFinished(false, QStringLiteral("applemusic"), QString(), QVariantList(), reply->errorString());
                return;
            }

            QString html = QString::fromUtf8(reply->readAll());
            static const QRegularExpression titleRe(QStringLiteral(R"raw(<meta property="og:title" content="([^"]+)")raw"));
            QRegularExpressionMatch mTitle = titleRe.match(html);
            QString playlistTitle = mTitle.hasMatch() ? mTitle.captured(1) : tr("Apple Music Playlist");

            QVariantList resultList;
            static const QRegularExpression appleRe(QStringLiteral(R"(<script type="application/json" id="serialized-server-data">([^<]+)</script>)"));
            QRegularExpressionMatch m = appleRe.match(html);
            if (m.hasMatch()) {
                QJsonDocument doc = QJsonDocument::fromJson(m.captured(1).toUtf8());
                QJsonArray dataArr = doc.array();
                for (const auto& dVal : dataArr) {
                    QJsonObject dObj = dVal.toObject();
                    QJsonObject data = dObj.value(QStringLiteral("data")).toObject();
                    QJsonArray sections = data.value(QStringLiteral("sections")).toArray();
                    for (const auto& sVal : sections) {
                        QJsonArray items = sVal.toObject().value(QStringLiteral("items")).toArray();
                        for (const auto& itemVal : items) {
                            QJsonObject itObj = itemVal.toObject();
                            QString trackTitle = itObj.value(QStringLiteral("title")).toString();
                            QString trackArtist = itObj.value(QStringLiteral("artistName")).toString();
                            QString trackId = itObj.value(QStringLiteral("id")).toString();
                            if (!trackTitle.isEmpty() && !trackId.isEmpty()) {
                                QVariantMap item;
                                item[QStringLiteral("id")] = trackId;
                                item[QStringLiteral("title")] = trackTitle;
                                item[QStringLiteral("artist")] = trackArtist;
                                item[QStringLiteral("duration")] = 0;
                                item[QStringLiteral("bpm")] = 0.0;
                                item[QStringLiteral("coverArtUrl")] = QString();
                                item[QStringLiteral("suffix")] = QStringLiteral("MP3");
                                item[QStringLiteral("streamUrl")] = QString();
                                item[QStringLiteral("service")] = QStringLiteral("applemusic");
                                item[QStringLiteral("isCached")] = isTrackCached(QStringLiteral("applemusic"), trackId);
                                resultList.append(item);
                            }
                        }
                    }
                }
            }
            emit playlistImportFinished(true, QStringLiteral("applemusic"), playlistTitle, resultList, QString());
        });
        return;
    }

    emit playlistImportFinished(false, QString(), QString(), QVariantList(), tr("Unsupported link format. Use Spotify or Apple Music links."));
}

bool QmlStreamingProxy::isTrackCached(const QString& service, const QString& trackId) const {
    return !getCachedTrackPath(service, trackId).isEmpty();
}

QString QmlStreamingProxy::getCachedTrackPath(const QString& service, const QString& trackId) const {
    const QString safeId = sanitizeFileName(trackId);
    const QString marker = QStringLiteral("[%1_%2]").arg(service, safeId);
    const QString legacyPrefix = service + QLatin1Char('_') + safeId;

    QDir dir(m_cacheDir);
    const QStringList files = dir.entryList(QDir::Files);
    for (const QString& file : files) {
        if (!file.endsWith(QStringLiteral(".part"))) {
            if (file.contains(marker) || file.startsWith(legacyPrefix)) {
                const QString fullPath = dir.absoluteFilePath(file);
                QFileInfo fi(fullPath);
                if (fi.size() > 1024) {
                    return fullPath;
                }
            }
        }
    }
    return QString();
}

void QmlStreamingProxy::matchAndDownloadFullTrack(
        const QString& title,
        const QString& artist,
        const QString& fallbackStreamUrl,
        const QString& service,
        const QString& trackId,
        const QString& targetDeck,
        bool autoPlay) {
    qInfo() << "QmlStreamingProxy: Buscando pista completa para:" << title << "por" << artist;

    // 1. If Deemix server URL is configured, try Deemix API first
    if (!m_deemixServerUrl.trimmed().isEmpty()) {
        QString deemixBase = m_deemixServerUrl.trimmed();
        while (deemixBase.endsWith(QLatin1Char('/'))) {
            deemixBase.chop(1);
        }
        QString deemixReqUrl = QStringLiteral("%1/api/getTrack?id=%2").arg(deemixBase, trackId);
        QNetworkRequest req{QUrl(deemixReqUrl)};
        req.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
        QNetworkReply* dReply = m_pNetworkManager->get(req);
        connect(dReply, &QNetworkReply::finished, this, [this, dReply, title, artist, fallbackStreamUrl, service, trackId, targetDeck, autoPlay]() {
            dReply->deleteLater();
            if (dReply->error() == QNetworkReply::NoError) {
                QJsonObject rootObj = QJsonDocument::fromJson(dReply->readAll()).object();
                QString audioUrl = rootObj.value(QStringLiteral("downloadUrl")).toString();
                if (audioUrl.isEmpty()) {
                    audioUrl = rootObj.value(QStringLiteral("url")).toString();
                }
                if (!audioUrl.isEmpty()) {
                    qInfo() << "QmlStreamingProxy: Coincidencia en Deemix encontrada:" << audioUrl;
                    downloadAndLoad(audioUrl, service, trackId, title, artist, targetDeck, autoPlay);
                    return;
                }
            }
            // If Deemix didn't return an audio URL, fall through to SoundCloud matching
            matchAndDownloadFullTrack(title, artist, fallbackStreamUrl, service, trackId, targetDeck, autoPlay);
        });
        return;
    }

    // 2. SoundCloud cross-platform matching (downloads the complete full-length audio!)
    QString cleanTitle = title;
    cleanTitle.remove(QRegularExpression(QStringLiteral(R"raw(\([^)]*\)|\[[^]]*\])raw")));
    QString query = (cleanTitle.trimmed() + QLatin1Char(' ') + artist.trimmed()).trimmed();

    QString clientId = m_soundCloudClientId.isEmpty() ? kDefaultSoundCloudClientId : m_soundCloudClientId;
    const QString urlStr = QStringLiteral("https://api-v2.soundcloud.com/search/tracks?q=%1&limit=10&client_id=%2")
            .arg(QString::fromUtf8(QUrl::toPercentEncoding(query)), clientId);

    QNetworkRequest request{QUrl(urlStr)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, title, artist, fallbackStreamUrl, service, trackId, targetDeck, autoPlay]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            QJsonObject rootObj = QJsonDocument::fromJson(reply->readAll()).object();
            QJsonArray collection = rootObj.value(QStringLiteral("collection")).toArray();

            for (const auto& itemVal : collection) {
                QJsonObject t = itemVal.toObject();
                qint64 durationMs = t.value(QStringLiteral("duration")).toInteger();
                // Filter out short preview snippets (require full track > 75 seconds)
                if (durationMs > 75000) {
                    QJsonObject media = t.value(QStringLiteral("media")).toObject();
                    QJsonArray transcodings = media.value(QStringLiteral("transcodings")).toArray();
                    QString scStreamUrl;
                    for (const auto& tcVal : transcodings) {
                        QJsonObject tc = tcVal.toObject();
                        QJsonObject format = tc.value(QStringLiteral("format")).toObject();
                        QString protocol = format.value(QStringLiteral("protocol")).toString();
                        QString mimeType = format.value(QStringLiteral("mime_type")).toString();
                        if (protocol == QStringLiteral("progressive") && (mimeType.contains(QStringLiteral("audio/mpeg")) || mimeType.contains(QStringLiteral("mp3")))) {
                            scStreamUrl = tc.value(QStringLiteral("url")).toString();
                            break;
                        }
                    }

                    if (!scStreamUrl.isEmpty()) {
                        qInfo() << "QmlStreamingProxy: Coincidencia completa progresiva en SoundCloud encontrada para" << title << ":" << scStreamUrl;
                        downloadAndLoad(scStreamUrl, QStringLiteral("soundcloud"), trackId, title, artist, targetDeck, autoPlay);
                        return;
                    }
                }
            }
        }

        // If no full match found or error, fall back to the preview stream if available
        if (!fallbackStreamUrl.isEmpty()) {
            qInfo() << "QmlStreamingProxy: No se encontró audio completo, usando preview:" << fallbackStreamUrl;
            downloadAndLoad(fallbackStreamUrl, service, trackId, title, artist, targetDeck, autoPlay);
        } else {
            emit downloadFailed(trackId, tr("No matching full track found for '%1'").arg(title));
        }
    });
}

void QmlStreamingProxy::downloadDeezerWithArl(
        const QString& trackId,
        const QString& title,
        const QString& artist,
        const QString& targetDeck,
        bool autoPlay) {
    if (m_deezerArl.trimmed().isEmpty()) {
        qWarning() << "QmlStreamingProxy: ARL de Deezer no configurada, usando fallback";
        matchAndDownloadFullTrack(title, artist, QString(), QStringLiteral("deezer"), trackId, targetDeck, autoPlay);
        return;
    }

    qInfo() << "QmlStreamingProxy: Iniciando descarga directa Deezer con ARL para:" << trackId << title;

    if (!m_pNetworkManager->cookieJar()) {
        m_pNetworkManager->setCookieJar(new QNetworkCookieJar(m_pNetworkManager));
    }
    QList<QNetworkCookie> initialCookies;
    QNetworkCookie arlCookie(QByteArrayLiteral("arl"), m_deezerArl.trimmed().toUtf8());
    arlCookie.setDomain(QStringLiteral(".deezer.com"));
    arlCookie.setPath(QStringLiteral("/"));
    initialCookies.append(arlCookie);
    m_pNetworkManager->cookieJar()->setCookiesFromUrl(initialCookies, QUrl(QStringLiteral("https://www.deezer.com")));

    // Step 1: Obtain checkForm and license_token via deezer.getUserData with ARL cookie
    QNetworkRequest userReq{QUrl(QStringLiteral("https://www.deezer.com/ajax/gw-light.php?method=deezer.getUserData&input=3&api_version=1.0&api_token=null"))};
    userReq.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
    userReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    userReq.setRawHeader("Cookie", QStringLiteral("arl=%1").arg(m_deezerArl.trimmed()).toUtf8());

    QNetworkReply* userReply = m_pNetworkManager->post(userReq, QByteArrayLiteral("{}"));
    connect(userReply, &QNetworkReply::finished, this, [this, userReply, trackId, title, artist, targetDeck, autoPlay]() {
        userReply->deleteLater();
        if (userReply->error() != QNetworkReply::NoError) {
            qWarning() << "QmlStreamingProxy: Error obteniendo sesión Deezer:" << userReply->errorString();
            matchAndDownloadFullTrack(title, artist, QString(), QStringLiteral("deezer"), trackId, targetDeck, autoPlay);
            return;
        }

        const QByteArray userData = userReply->readAll();
        QJsonObject userObj = QJsonDocument::fromJson(userData).object();
        QJsonObject results = userObj.value(QStringLiteral("results")).toObject();
        QString apiToken = results.value(QStringLiteral("checkForm")).toString();
        QJsonObject userSubObj = results.value(QStringLiteral("USER")).toObject();
        QJsonObject options = userSubObj.value(QStringLiteral("OPTIONS")).toObject();
        QString licenseToken = options.value(QStringLiteral("license_token")).toString();

        QVariant userId = userSubObj.value(QStringLiteral("USER_ID")).toVariant();
        qInfo() << "QmlStreamingProxy: Sesión Deezer obtenida para USER_ID:" << userId << "checkForm:" << apiToken;

        if (apiToken.isEmpty() || licenseToken.isEmpty()) {
            qWarning() << "QmlStreamingProxy: Tokens Deezer inválidos, usando fallback";
            matchAndDownloadFullTrack(title, artist, QString(), QStringLiteral("deezer"), trackId, targetDeck, autoPlay);
            return;
        }

        // Determine quality rights based on account options
        QJsonObject soundQuality = options.value(QStringLiteral("mobile_sound_quality")).toObject();
        bool canLossless = soundQuality.value(QStringLiteral("lossless")).toBool();
        bool canHigh = soundQuality.value(QStringLiteral("high")).toBool();

        // Extract session cookies (sid, etc.) from response headers
        QByteArray cookieHeader = QStringLiteral("arl=%1").arg(m_deezerArl.trimmed()).toUtf8();
        QVariant cookieVar = userReply->header(QNetworkRequest::SetCookieHeader);
        if (cookieVar.isValid()) {
            const QList<QNetworkCookie> cookies = qvariant_cast<QList<QNetworkCookie>>(cookieVar);
            for (const auto& c : cookies) {
                m_pNetworkManager->cookieJar()->insertCookie(c);
                cookieHeader += "; " + c.name() + "=" + c.value();
            }
        }

        // Step 2: Request song.getData to obtain TRACK_TOKEN
        const QString songUrl = QStringLiteral("https://www.deezer.com/ajax/gw-light.php?method=song.getData&input=3&api_version=1.0&api_token=%1").arg(apiToken);
        QNetworkRequest songReq{QUrl(songUrl)};
        songReq.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
        songReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        songReq.setRawHeader("Cookie", cookieHeader);

        QJsonObject songPostJson;
        songPostJson[QStringLiteral("sng_id")] = trackId;
        QNetworkReply* songReply = m_pNetworkManager->post(songReq, QJsonDocument(songPostJson).toJson(QJsonDocument::Compact));

        connect(songReply, &QNetworkReply::finished, this, [this, songReply, licenseToken, canHigh, canLossless, trackId, title, artist, targetDeck, autoPlay]() {
            songReply->deleteLater();
            if (songReply->error() != QNetworkReply::NoError) {
                qWarning() << "QmlStreamingProxy: Error en song.getData Deezer:" << songReply->errorString();
                matchAndDownloadFullTrack(title, artist, QString(), QStringLiteral("deezer"), trackId, targetDeck, autoPlay);
                return;
            }

            QJsonObject songObj = QJsonDocument::fromJson(songReply->readAll()).object();
            QJsonObject sResults = songObj.value(QStringLiteral("results")).toObject();
            QString trackToken = sResults.value(QStringLiteral("TRACK_TOKEN")).toString();
            if (trackToken.isEmpty() && sResults.contains(QStringLiteral("FALLBACK"))) {
                QJsonObject fallback = sResults.value(QStringLiteral("FALLBACK")).toObject();
                trackToken = fallback.value(QStringLiteral("TRACK_TOKEN")).toString();
            }

            if (trackToken.isEmpty()) {
                qWarning() << "QmlStreamingProxy: TRACK_TOKEN vacío en Deezer, usando fallback";
                matchAndDownloadFullTrack(title, artist, QString(), QStringLiteral("deezer"), trackId, targetDeck, autoPlay);
                return;
            }

            // Step 3: Request media.deezer.com/v1/get_url for CDN media stream
            QNetworkRequest mediaReq{QUrl(QStringLiteral("https://media.deezer.com/v1/get_url"))};
            mediaReq.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
            mediaReq.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
            // Do NOT send Cookie header to media.deezer.com; license_token handles auth

            QJsonObject mediaPost;
            mediaPost[QStringLiteral("license_token")] = licenseToken;
            QJsonArray formatsArr;
            if (canLossless) {
                QJsonObject f; f[QStringLiteral("cipher")] = QStringLiteral("BF_CBC_STRIPE"); f[QStringLiteral("format")] = QStringLiteral("FLAC"); formatsArr.append(f);
            }
            if (canHigh) {
                QJsonObject f; f[QStringLiteral("cipher")] = QStringLiteral("BF_CBC_STRIPE"); f[QStringLiteral("format")] = QStringLiteral("MP3_320"); formatsArr.append(f);
            }
            // Always request MP3_128 (supported on Free and Premium tiers)
            QJsonObject f128; f128[QStringLiteral("cipher")] = QStringLiteral("BF_CBC_STRIPE"); f128[QStringLiteral("format")] = QStringLiteral("MP3_128"); formatsArr.append(f128);

            QJsonObject mediaItem;
            mediaItem[QStringLiteral("type")] = QStringLiteral("FULL");
            mediaItem[QStringLiteral("formats")] = formatsArr;
            QJsonArray mediaArr; mediaArr.append(mediaItem);
            mediaPost[QStringLiteral("media")] = mediaArr;
            QJsonArray tokensArr; tokensArr.append(trackToken);
            mediaPost[QStringLiteral("track_tokens")] = tokensArr;

            QNetworkReply* mediaReply = m_pNetworkManager->post(mediaReq, QJsonDocument(mediaPost).toJson(QJsonDocument::Compact));

            connect(mediaReply, &QNetworkReply::finished, this, [this, mediaReply, trackId, title, artist, targetDeck, autoPlay]() {
                mediaReply->deleteLater();
                if (mediaReply->error() != QNetworkReply::NoError) {
                    const QByteArray errBody = mediaReply->readAll();
                    qWarning() << "QmlStreamingProxy: Error en get_url Deezer:" << mediaReply->errorString() << "Body:" << errBody;
                    matchAndDownloadFullTrack(title, artist, QString(), QStringLiteral("deezer"), trackId, targetDeck, autoPlay);
                    return;
                }

                QJsonObject mediaRes = QJsonDocument::fromJson(mediaReply->readAll()).object();
                QJsonArray dataArr = mediaRes.value(QStringLiteral("data")).toArray();
                QString cdnUrl;
                QString formatName;

                if (!dataArr.isEmpty()) {
                    QJsonObject firstData = dataArr.first().toObject();
                    QJsonArray mArr = firstData.value(QStringLiteral("media")).toArray();
                    if (!mArr.isEmpty()) {
                        QJsonObject firstMedia = mArr.first().toObject();
                        formatName = firstMedia.value(QStringLiteral("format")).toString();
                        QJsonArray sources = firstMedia.value(QStringLiteral("sources")).toArray();
                        if (!sources.isEmpty()) {
                            cdnUrl = sources.first().toObject().value(QStringLiteral("url")).toString();
                        }
                    }
                }

                if (cdnUrl.isEmpty()) {
                    qWarning() << "QmlStreamingProxy: No se obtuvo URL de audio en CDN de Deezer, usando matching";
                    matchAndDownloadFullTrack(title, artist, QString(), QStringLiteral("deezer"), trackId, targetDeck, autoPlay);
                    return;
                }

                qInfo() << "QmlStreamingProxy: Descargando audio cifrado desde CDN Deezer:" << cdnUrl;

                // Step 4: Download encrypted audio stream and decrypt in-place
                QNetworkRequest dlReq{QUrl(cdnUrl)};
                dlReq.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
                QNetworkReply* dlReply = m_pNetworkManager->get(dlReq);

                connect(dlReply, &QNetworkReply::downloadProgress, this, [this, trackId](qint64 received, qint64 total) {
                    if (total > 0) {
                        emit downloadProgress(trackId, static_cast<double>(received) / total, received, total);
                    }
                });

                connect(dlReply, &QNetworkReply::finished, this, [this, dlReply, trackId, title, artist, formatName, targetDeck, autoPlay]() {
                    dlReply->deleteLater();
                    if (dlReply->error() != QNetworkReply::NoError) {
                        qWarning() << "QmlStreamingProxy: Error descargando de CDN Deezer:" << dlReply->errorString();
                        emit downloadFailed(trackId, tr("Error downloading Deezer audio: %1").arg(dlReply->errorString()));
                        return;
                    }

                    QByteArray encryptedAudio = dlReply->readAll();
                    if (encryptedAudio.isEmpty()) {
                        emit downloadFailed(trackId, tr("Empty audio received from Deezer"));
                        return;
                    }

                    // Decrypt Blowfish CBC striped audio
                    decryptDeezerDataInPlace(encryptedAudio, trackId);

                    QString ext = (formatName == QStringLiteral("FLAC")) ? QStringLiteral(".flac") : QStringLiteral(".mp3");
                    QString cleanArtist = sanitizeFileName(artist.isEmpty() ? QStringLiteral("Unknown") : artist);
                    QString cleanTitle = sanitizeFileName(title.isEmpty() ? QStringLiteral("Unknown") : title);
                    QString targetFilePath = m_cacheDir + QLatin1Char('/') + cleanArtist + QStringLiteral(" - ") + cleanTitle + QStringLiteral(" [deezer_") + trackId + QStringLiteral("]") + ext;

                    QFile outFile(targetFilePath);
                    if (!outFile.open(QIODevice::WriteOnly)) {
                        emit downloadFailed(trackId, tr("Could not write audio to cache file"));
                        return;
                    }
                    outFile.write(encryptedAudio);
                    outFile.close();

                    // Apply TagLib metadata
                    mixxx::TrackMetadata meta;
                    meta.refTrackInfo().setTitle(title);
                    meta.refTrackInfo().setArtist(artist);
                    mixxx::MetadataSourceTagLib tagSource(targetFilePath, QStringLiteral("mp3"));
                    tagSource.exportTrackMetadata(meta);

                    qInfo() << "QmlStreamingProxy: Audio descifrado y guardado con éxito:" << targetFilePath;
                    emit downloadProgress(trackId, 1.0, encryptedAudio.size(), encryptedAudio.size());
                    emit cacheSizeChanged();

                    if (!targetDeck.isEmpty() && s_pPlayerManager) {
                        mixxx::FileInfo fileInfo(targetFilePath);
                        Sandbox::createSecurityToken(&fileInfo);
                        QMetaObject::invokeMethod(s_pPlayerManager.get(), [targetFilePath, targetDeck, autoPlay]() {
                            if (s_pPlayerManager) {
                                s_pPlayerManager->slotLoadLocationToPlayer(targetFilePath, targetDeck, autoPlay);
                            }
                        }, Qt::QueuedConnection);
                    }

                    emit downloadFinished(trackId, targetFilePath, targetDeck, autoPlay);
                });
            });
        });
    });
}

void QmlStreamingProxy::downloadAndLoad(
        const QString& streamUrl,
        const QString& service,
        const QString& trackId,
        const QString& title,
        const QString& artist,
        const QString& targetDeck,
        bool autoPlay) {
    if (trackId.isEmpty()) {
        emit downloadFailed(trackId, tr("Invalid track ID"));
        return;
    }

    // 1. Check if already cached on disk
    const QString cachedPath = getCachedTrackPath(service, trackId);
    if (!cachedPath.isEmpty()) {
        qInfo() << "QmlStreamingProxy: Pista en caché encontrada:" << cachedPath;
        emit downloadProgress(trackId, 1.0, 100, 100);
        if (!targetDeck.isEmpty() && s_pPlayerManager) {
            mixxx::FileInfo fileInfo(cachedPath);
            Sandbox::createSecurityToken(&fileInfo);
            QMetaObject::invokeMethod(s_pPlayerManager.get(), [cachedPath, targetDeck, autoPlay]() {
                if (s_pPlayerManager) {
                    s_pPlayerManager->slotLoadLocationToPlayer(cachedPath, targetDeck, autoPlay);
                }
            }, Qt::QueuedConnection);
        }
        emit downloadFinished(trackId, cachedPath, targetDeck, autoPlay);
        return;
    }

    // 2. If Deezer track and ARL is present, download directly from Deezer using ARL!
    if (service == QStringLiteral("deezer") && !m_deezerArl.trimmed().isEmpty()) {
        downloadDeezerWithArl(trackId, title, artist, targetDeck, autoPlay);
        return;
    }

    // 3. If preferFullTrackMatching is enabled and this is a preview/empty stream from Spotify/Apple/Deezer,
    // match against SoundCloud/Deemix to download the complete full-length audio!
    if (m_preferFullTrackMatching && (service == QStringLiteral("spotify") || service == QStringLiteral("applemusic") || service == QStringLiteral("deezer"))) {
        bool isPreviewOrEmpty = streamUrl.isEmpty() || streamUrl.contains(QStringLiteral("preview")) || streamUrl.contains(QStringLiteral("p.scdn.co"));
        if (isPreviewOrEmpty && !title.isEmpty()) {
            matchAndDownloadFullTrack(title, artist, streamUrl, service, trackId, targetDeck, autoPlay);
            return;
        }
    }

    // 2. If it's a SoundCloud transcoding URL, resolve to direct stream URL first
    if (service == QStringLiteral("soundcloud") && streamUrl.contains(QStringLiteral("api-v2.soundcloud.com/media"))) {
        QString clientId = m_soundCloudClientId.isEmpty() ? kDefaultSoundCloudClientId : m_soundCloudClientId;
        QString resolveUrl = streamUrl + (streamUrl.contains(QLatin1Char('?')) ? QStringLiteral("&client_id=") : QStringLiteral("?client_id=")) + clientId;
        QNetworkRequest req{QUrl(resolveUrl)};
        req.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);
        QNetworkReply* resolveReply = m_pNetworkManager->get(req);
        connect(resolveReply, &QNetworkReply::finished, this, [this, resolveReply, service, trackId, title, artist, targetDeck, autoPlay]() {
            resolveReply->deleteLater();
            if (resolveReply->error() == QNetworkReply::NoError) {
                QJsonObject obj = QJsonDocument::fromJson(resolveReply->readAll()).object();
                QString finalStreamUrl = obj.value(QStringLiteral("url")).toString();
                if (!finalStreamUrl.isEmpty()) {
                    downloadAndLoad(finalStreamUrl, service, trackId, title, artist, targetDeck, autoPlay);
                    return;
                }
            }
            emit downloadFailed(trackId, tr("Could not resolve SoundCloud audio stream"));
        });
        return;
    }

    if (streamUrl.isEmpty()) {
        emit downloadFailed(trackId, tr("No stream URL available"));
        return;
    }
    if (!cachedPath.isEmpty()) {
        qInfo() << "QmlStreamingProxy: Pista en caché encontrada:" << cachedPath;
        emit downloadProgress(trackId, 1.0, 100, 100);
        if (!targetDeck.isEmpty() && s_pPlayerManager) {
            mixxx::FileInfo fileInfo(cachedPath);
            Sandbox::createSecurityToken(&fileInfo);
            QMetaObject::invokeMethod(s_pPlayerManager.get(), [cachedPath, targetDeck, autoPlay]() {
                if (s_pPlayerManager) {
                    s_pPlayerManager->slotLoadLocationToPlayer(cachedPath, targetDeck, autoPlay);
                }
            }, Qt::QueuedConnection);
        }
        emit downloadFinished(trackId, cachedPath, targetDeck, autoPlay);
        return;
    }

    // 2. Check if currently being downloaded
    if (m_activeDownloads.contains(trackId)) {
        qInfo() << "QmlStreamingProxy: La pista ya se está descargando:" << trackId;
        return;
    }

    // Determine extension
    QString ext = QStringLiteral(".mp3");
    if (streamUrl.contains(QStringLiteral(".flac"), Qt::CaseInsensitive)) {
        ext = QStringLiteral(".flac");
    } else if (streamUrl.contains(QStringLiteral(".m4a"), Qt::CaseInsensitive) || streamUrl.contains(QStringLiteral(".aac"), Qt::CaseInsensitive)) {
        ext = QStringLiteral(".m4a");
    } else if (streamUrl.contains(QStringLiteral(".ogg"), Qt::CaseInsensitive)) {
        ext = QStringLiteral(".ogg");
    } else if (streamUrl.contains(QStringLiteral(".wav"), Qt::CaseInsensitive)) {
        ext = QStringLiteral(".wav");
    }

    const QString finalFileName = buildTrackFileName(service, trackId, title, artist, ext);
    const QString finalPath = m_cacheDir + QLatin1Char('/') + finalFileName;
    const QString partPath = finalPath + QStringLiteral(".part");

    auto pFile = std::make_shared<QFile>(partPath);
    if (!pFile->open(QIODevice::WriteOnly)) {
        emit downloadFailed(trackId, tr("Cannot create the temporary cache file"));
        return;
    }

    QNetworkRequest request{QUrl(streamUrl)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, kDefaultUserAgent);

    QNetworkReply* reply = m_pNetworkManager->get(request);
    connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError>& errors) {
        Q_UNUSED(errors);
        reply->ignoreSslErrors();
    });
    reply->ignoreSslErrors();

    auto job = std::make_shared<StreamingDownloadJob>();
    job->trackId = trackId;
    job->service = service;
    job->title = title;
    job->artist = artist;
    job->targetDeck = targetDeck;
    job->autoPlay = autoPlay;
    job->ext = ext;
    job->finalPath = finalPath;
    job->partPath = partPath;
    job->pFile = pFile;
    job->pReply = reply;

    m_activeDownloads.insert(trackId, job);
    emit activeDownloadsChanged();

    connect(reply, &QNetworkReply::downloadProgress, this, [this, trackId](qint64 received, qint64 total) {
        qreal progress = total > 0 ? static_cast<qreal>(received) / static_cast<qreal>(total) : 0.0;
        emit downloadProgress(trackId, progress, received, total);
    });

    connect(reply, &QIODevice::readyRead, this, [job]() {
        if (job->pFile && job->pFile->isOpen()) {
            job->pFile->write(job->pReply->readAll());
        }
    });

    connect(reply, &QNetworkReply::finished, this, [this, job]() {
        job->pReply->deleteLater();
        if (job->pFile && job->pFile->isOpen()) {
            job->pFile->write(job->pReply->readAll());
            job->pFile->flush();
            job->pFile->close();
        }

        const bool hasError = (job->pReply->error() != QNetworkReply::NoError);
        const QString errorMsg = job->pReply->errorString();
        const QString trackId = job->trackId;
        const QString targetDeck = job->targetDeck;
        const QString finalPath = job->finalPath;
        const QString partPath = job->partPath;
        const bool autoPlay = job->autoPlay;

        m_activeDownloads.remove(trackId);
        emit activeDownloadsChanged();

        if (hasError) {
            qWarning() << "QmlStreamingProxy: Error al descargar:" << errorMsg;
            QFile::remove(partPath);
            emit downloadFailed(trackId, errorMsg);
            return;
        }

        // Rename part file to final file
        QFile::remove(finalPath);
        if (!QFile::rename(partPath, finalPath)) {
            emit downloadFailed(trackId, tr("Could not save the downloaded track"));
            return;
        }

        // Write ID3 metadata tags (Title, Artist) before loading so Mixxx will never display file fallback names
        mixxx::TrackMetadata trackMetadata;
        if (!job->title.isEmpty()) {
            trackMetadata.refTrackInfo().setTitle(job->title);
        }
        if (!job->artist.isEmpty()) {
            trackMetadata.refTrackInfo().setArtist(job->artist);
        }
        QString extFormat = job->ext.startsWith(QLatin1Char('.')) ? job->ext.mid(1) : job->ext;
        mixxx::MetadataSourceTagLib tagSource(finalPath, extFormat);
        tagSource.exportTrackMetadata(trackMetadata);

        qInfo() << "QmlStreamingProxy: Descarga completada:" << finalPath << "para" << targetDeck;
        emit refreshCacheSize();

        // Automatically load into Deck if targetDeck is requested (safely queued to main thread)
        if (!targetDeck.isEmpty() && s_pPlayerManager) {
            mixxx::FileInfo fileInfo(finalPath);
            Sandbox::createSecurityToken(&fileInfo);
            QMetaObject::invokeMethod(s_pPlayerManager.get(), [finalPath, targetDeck, autoPlay]() {
                if (s_pPlayerManager) {
                    s_pPlayerManager->slotLoadLocationToPlayer(finalPath, targetDeck, autoPlay);
                }
            }, Qt::QueuedConnection);
        }

        emit downloadFinished(trackId, finalPath, targetDeck, autoPlay);
    });
}

void QmlStreamingProxy::cancelDownload(const QString& trackId) {
    if (m_activeDownloads.contains(trackId)) {
        auto job = m_activeDownloads.take(trackId);
        emit activeDownloadsChanged();
        if (job->pReply) {
            job->pReply->abort();
        }
        if (job->pFile && job->pFile->isOpen()) {
            job->pFile->close();
        }
        QFile::remove(job->partPath);
    }
}

qint64 QmlStreamingProxy::cacheSizeBytes() const {
    qint64 total = 0;
    QDir dir(m_cacheDir);
    const QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : list) {
        total += fi.size();
    }
    return total;
}

void QmlStreamingProxy::refreshCacheSize() {
    emit cacheSizeChanged();
}

void QmlStreamingProxy::clearCache() {
    QDir dir(m_cacheDir);
    const QFileInfoList list = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& fi : list) {
        QFile::remove(fi.absoluteFilePath());
    }
    emit cacheSizeChanged();
}

} // namespace qml
} // namespace mixxx
