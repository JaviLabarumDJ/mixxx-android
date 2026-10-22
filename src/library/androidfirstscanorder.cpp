#include "library/androidfirstscanorder.h"

#include <QDateTime>
#include <QFileInfo>
#include <QList>
#include <QSqlError>
#include <QSqlQuery>
#include <QtDebug>
#include <algorithm>
#include <atomic>
#include <memory>

#include "library/scanner/libraryscanner.h"
#include "util/datetime.h"
#include "util/db/dbconnectionpooled.h"
#include "util/db/sqltransaction.h"

namespace {

struct ScannedTrack {
    QVariant id;
    QDateTime dateAdded;
    QDateTime fileDate;
};

bool isLibraryEmpty(QSqlDatabase database) {
    QSqlQuery query(database);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM library")) || !query.next()) {
        qWarning() << "First scan order:" << query.lastError();
        return false;
    }
    return query.value(0).toInt() == 0;
}

void orderTracksByFileDate(QSqlDatabase database) {
    QSqlQuery select(database);
    if (!select.exec(QStringLiteral(
                "SELECT library.id, library.datetime_added, track_locations.location "
                "FROM library "
                "INNER JOIN track_locations ON library.location = track_locations.id"))) {
        qWarning() << "First scan order:" << select.lastError();
        return;
    }
    QList<ScannedTrack> tracks;
    QList<QDateTime> datesAdded;
    while (select.next()) {
        const QDateTime dateAdded = mixxx::convertVariantToDateTime(select.value(1));
        if (!dateAdded.isValid()) {
            continue;
        }
        tracks.append({select.value(0),
                dateAdded,
                QFileInfo(select.value(2).toString()).lastModified().toUTC()});
        datesAdded.append(dateAdded);
    }
    if (tracks.size() < 2) {
        return;
    }

    // Oldest file first, files without a date at the end. Ties keep the
    // order of the scan.
    std::stable_sort(tracks.begin(), tracks.end(), [](const auto& a, const auto& b) {
        if (a.fileDate.isValid() != b.fileDate.isValid()) {
            return a.fileDate.isValid();
        }
        return a.fileDate.isValid() && a.fileDate < b.fileDate;
    });
    std::sort(datesAdded.begin(), datesAdded.end());
    // Tracks added within the same millisecond would get the same value and
    // their order would be undefined again
    for (qsizetype i = 1; i < datesAdded.size(); ++i) {
        if (datesAdded[i] <= datesAdded[i - 1]) {
            datesAdded[i] = datesAdded[i - 1].addMSecs(1);
        }
    }

    SqlTransaction transaction(database);
    QSqlQuery update(database);
    update.prepare(QStringLiteral("UPDATE library SET datetime_added=:date WHERE id=:id"));
    for (qsizetype i = 0; i < tracks.size(); ++i) {
        if (tracks[i].dateAdded == datesAdded[i]) {
            continue;
        }
        update.bindValue(QStringLiteral(":date"), datesAdded[i]);
        update.bindValue(QStringLiteral(":id"), tracks[i].id);
        if (!update.exec()) {
            qWarning() << "First scan order:" << update.lastError();
            return;
        }
    }
    if (transaction.commit()) {
        qInfo() << "Ordered the" << tracks.size()
                << "tracks of the first library scan by file date";
    }
}

} // namespace

namespace mixxx {
namespace android {

void connectFirstScanOrder(
        LibraryScanner* pScanner,
        const mixxx::DbConnectionPoolPtr& pDbConnectionPool) {
    // Both signals are emitted from the scanner thread, which holds a
    // connection of the pool
    auto pFirstScan = std::make_shared<std::atomic<bool>>(false);
    QObject::connect(
            pScanner,
            &LibraryScanner::scanStarted,
            pScanner,
            [pFirstScan, pDbConnectionPool]() {
                pFirstScan->store(isLibraryEmpty(
                        mixxx::DbConnectionPooled(pDbConnectionPool)));
            },
            Qt::DirectConnection);
    QObject::connect(
            pScanner,
            &LibraryScanner::scanFinished,
            pScanner,
            [pFirstScan, pDbConnectionPool]() {
                if (pFirstScan->exchange(false)) {
                    orderTracksByFileDate(mixxx::DbConnectionPooled(pDbConnectionPool));
                }
            },
            Qt::DirectConnection);
}

} // namespace android
} // namespace mixxx
