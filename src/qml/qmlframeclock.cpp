#include "qml/qmlframeclock.h"

#include <QElapsedTimer>
#include <algorithm>
#include <atomic>

namespace mixxx {
namespace qml {
namespace frameclock {

namespace {

// 60 frames per second until a waveform sets its frame rate
std::atomic<qint64> s_intervalMicros{16667};

const QElapsedTimer& epoch() {
    static const QElapsedTimer s_epoch = []() {
        QElapsedTimer timer;
        timer.start();
        return timer;
    }();
    return s_epoch;
}

} // namespace

void setFrameInterval(std::chrono::microseconds interval) {
    s_intervalMicros = std::max<qint64>(1000, interval.count());
}

std::chrono::microseconds frameInterval() {
    return std::chrono::microseconds(s_intervalMicros.load());
}

int millisToNextTick(std::chrono::microseconds minDelay) {
    const qint64 interval = s_intervalMicros.load();
    const qint64 now = epoch().nsecsElapsed() / 1000;
    const qint64 earliest = now + std::max<qint64>(0, minDelay.count());
    // The first tick at or after the earliest time
    const qint64 tick = ((earliest + interval - 1) / interval) * interval;
    const qint64 delayMicros = tick - now;
    // Timers have millisecond resolution: rounding up keeps every source
    // on the same side of the tick
    return static_cast<int>(std::max<qint64>(1, (delayMicros + 999) / 1000));
}

} // namespace frameclock
} // namespace qml
} // namespace mixxx
