#pragma once

#include <vector>

#include "audio/types.h"
#include "track/beats.h"
#include "util/types.h"

namespace mixxx {
namespace android {

/// Lines a constant beatgrid up with the kicks of the track's intro.
///
/// The beat tracker often follows the off-beat hi-hats or claps of house and
/// techno rather than the kick, which leaves the grid half a beat (or a
/// fraction of a beat) off, and sometimes doubles the BPM. While the track
/// is analyzed, this keeps the onsets of its low frequencies (below about
/// 120 Hz) for the first 30 seconds. Afterwards the first strong ones, the
/// intro kicks, are compared with the grid:
///
/// - if they come every second beat and the BPM is 150 or more, the BPM is
///   halved;
/// - if they agree on one position between the beats, and the very first
///   onset of the track does too, the grid is moved there.
///
/// When they don't agree (e.g. a bass louder than the kick from the first
/// bar), the grid is left as the beat tracker made it.
class KickGrid {
  public:
    void initialize(audio::SampleRate sampleRate);
    /// Interleaved samples of the analysis
    void process(const CSAMPLE* pSamples, SINT frames, int channels);
    /// The corrected beats, or pBeats if nothing needs to change
    BeatsPointer correct(const BeatsPointer& pBeats) const;

  private:
    static constexpr SINT kHopFrames = 256;
    static constexpr double kSecondsKept = 30;

    audio::SampleRate m_sampleRate;
    double m_lowPassCoefficient = 0;
    double m_lowPass1 = 0;
    double m_lowPass2 = 0;
    double m_hopEnergy = 0;
    SINT m_hopFrames = 0;
    SINT m_maxHops = 0;
    /// RMS of the low frequencies per hop
    std::vector<float> m_envelope;
};

} // namespace android
} // namespace mixxx
