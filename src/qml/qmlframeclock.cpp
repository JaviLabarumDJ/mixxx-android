#include "qml/qmlframeclock.h"

#include <QElapsedTimer>
#include <algorithm>
#include <atomic>
#include <cmath>

namespace mixxx {
namespace qml {
namespace frameclock {

namespace {

// A 60 Hz display at 60 frames per second until a waveform tells better
std::atomic<qint64> s_periodMicros{16667};
std::atomic<qint64> s_intervalMicros{16667};
// The time of the last frame shown, in microseconds of epoch()
std::atomic<qint64> s_lastFrameMicros{0};

const QElapsedTimer& epoch() {
    static const QElapsedTimer s_epoch = []() {
        QElapsedTimer timer;
        timer.start();
        return timer;
    }();
    return s_epoch;
}

qint64 nowMicros() {
    return epoch().nsecsElapsed() / 1000;
}

} // namespace

void setDisplay(std::chrono::microseconds refreshPeriod, int frameRate) {
    const qint64 period = std::max<qint64>(1000, refreshPeriod.count());
    const double wanted = 1'000'000.0 / std::max(1, frameRate);
    // Not faster than wanted: 60 fps on a 90 Hz display is 45 fps, every
    // second refresh
    const qint64 periods = std::max<qint64>(1,
            static_cast<qint64>(std::ceil(wanted / static_cast<double>(period) - 0.01)));
    s_periodMicros = period;
    s_intervalMicros = periods * period;
}

std::chrono::microseconds frameInterval() {
    return std::chrono::microseconds(s_intervalMicros.load());
}

void frameShown() {
    s_lastFrameMicros = nowMicros();
}

int millisToNextTick(std::chrono::microseconds minDelay) {
    const qint64 interval = s_intervalMicros.load();
    const qint64 halfPeriod = s_periodMicros.load() / 2;
    const qint64 lastFrame = s_lastFrameMicros.load();
    const qint64 now = nowMicros();
    const qint64 earliest = now + std::max<qint64>(0, minDelay.count());
    // Ticks are half a period before the refreshes a whole number of
    // intervals after the last frame shown: an update there is drawn on
    // that refresh. Not the frame shown itself (k >= 1).
    const qint64 k = std::max<qint64>(1,
            (earliest - lastFrame + halfPeriod + interval - 1) / interval);
    const qint64 tick = lastFrame + k * interval - halfPeriod;
    const qint64 delayMicros = tick - now;
    return static_cast<int>(std::max<qint64>(1, (delayMicros + 999) / 1000));
}

} // namespace frameclock
} // namespace qml
} // namespace mixxx
