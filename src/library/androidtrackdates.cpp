#include "library/androidtrackdates.h"

#include <QFileInfo>
#include <QList>
#include <QPair>
#include <QSqlError>
#include <QSqlQuery>
#include <QtDebug>

#include "util/db/sqltransaction.h"

namespace mixxx {
namespace android {

QDateTime fileDateUtc(const QString& location) {
    const QDateTime lastModified = QFileInfo(location).lastModified().toUTC();
    if (!lastModified.isValid() || lastModified.toMSecsSinceEpoch() <= 0 ||
            lastModified > QDateTime::currentDateTimeUtc().addDays(1)) {
        return {};
    }
    return lastModified;
}

int useFileDatesAsDateAdded(QSqlDatabase database) {
    QSqlQuery select(database);
    if (!select.exec(QStringLiteral(
                "SELECT library.id, track_locations.location FROM library "
                "INNER JOIN track_locations ON library.location = track_locations.id"))) {
        qWarning() << "useFileDatesAsDateAdded:" << select.lastError();
        return -1;
    }
    QList<QPair<QVariant, QDateTime>> dates;
    while (select.next()) {
        const QDateTime fileDate = fileDateUtc(select.value(1).toString());
        if (fileDate.isValid()) {
            dates.append({select.value(0), fileDate});
        }
    }

    SqlTransaction transaction(database);
    QSqlQuery update(database);
    update.prepare(QStringLiteral("UPDATE library SET datetime_added=:date WHERE id=:id"));
    for (const auto& [id, date] : std::as_const(dates)) {
        update.bindValue(QStringLiteral(":date"), date);
        update.bindValue(QStringLiteral(":id"), id);
        if (!update.exec()) {
            qWarning() << "useFileDatesAsDateAdded:" << update.lastError();
            return -1;
        }
    }
    if (!transaction.commit()) {
        return -1;
    }
    qInfo() << "Set the date added of" << dates.size() << "tracks to their file date";
    return static_cast<int>(dates.size());
}

} // namespace android
} // namespace mixxx
