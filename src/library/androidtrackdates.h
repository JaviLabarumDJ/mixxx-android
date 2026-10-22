#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>

namespace mixxx {
namespace android {

/// "Date added" of the tracks on Android: Mixxx normally stores when a
/// track was added to the library, which is the same moment for all tracks
/// of the first scan. The Android build uses the date of the file instead,
/// which tells when the track was downloaded or copied.

/// The last modification time of the file in UTC, or an invalid QDateTime
/// if it is missing or bogus (1970 or in the future)
QDateTime fileDateUtc(const QString& location);

/// Sets the "date added" of all tracks in the library to fileDateUtc(),
/// for libraries that were scanned before. Tracks without a usable file
/// date keep their date. Returns the number of updated tracks, or -1 on a
/// database error.
int useFileDatesAsDateAdded(QSqlDatabase database);

} // namespace android
} // namespace mixxx
