#pragma once

#include <QFileInfoList>

namespace mixxx {

/// Root folders of the external drives (USB sticks, SD cards...) that are
/// currently mounted. They are scanned for databases exported by other DJ
/// software. On Android the internal shared storage is included as well, and
/// the drives Android didn't mount. Looking for those may wait for the USB
/// permission, so this must not be called on the main thread.
QFileInfoList removableStorageRoots();

} // namespace mixxx
