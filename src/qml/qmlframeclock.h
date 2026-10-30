#pragma once

#include <chrono>

namespace mixxx {
namespace qml {

/// A common grid of frame times for everything that redraws periodically
/// (the scrolling waveforms, and the controls with an updateInterval, e.g.
/// playposition, see QmlControlProxy). Each update of an item redraws the
/// whole window: when every source picks its own moments, the window is
/// redrawn as often as the display allows (e.g. 90 Hz) whatever the frame
/// rate of the waveforms is. Updates on the same tick of the grid are drawn
/// in one frame.
namespace frameclock {

/// The interval of the grid, set by the waveforms from their frame rate
void setFrameInterval(std::chrono::microseconds interval);
std::chrono::microseconds frameInterval();

/// Milliseconds (at least 1) until the first tick of the grid that is at
/// least minDelay away
int millisToNextTick(std::chrono::microseconds minDelay = std::chrono::microseconds(0));

} // namespace frameclock
} // namespace qml
} // namespace mixxx
