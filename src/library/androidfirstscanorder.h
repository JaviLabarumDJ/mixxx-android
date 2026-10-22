#pragma once

#include "util/db/dbconnectionpool.h"

class LibraryScanner;

namespace mixxx {
namespace android {

/// Orders the tracks of the first library scan by the age of their files.
///
/// The scanner adds the files alphabetically, folder by folder, and each
/// track gets the time it was added as "date added". After the very first
/// scan (the library was empty when it started), the "date added" values of
/// that scan are handed out again in the order of the file dates: the oldest
/// file gets the earliest value. All values stay times of that scan, so
/// sorting by "date added" lists the oldest files first, and the tracks of
/// later scans follow with their own times as usual.
///
/// Must be called before anything else is connected to the scanner's
/// scanFinished() signal, so the dates are changed before the track lists
/// are refreshed.
void connectFirstScanOrder(
        LibraryScanner* pScanner,
        const mixxx::DbConnectionPoolPtr& pDbConnectionPool);

} // namespace android
} // namespace mixxx
