#include "analyzer/androidkickgrid.h"

#include <QDebug>
#include <algorithm>
#include <cmath>

namespace mixxx {
namespace android {

namespace {

constexpr double kLowPassHz = 120;
// Onsets weaker than this part of the strongest one of the first seconds
// after the track starts are not kicks
constexpr double kStrongOnset = 0.5;
constexpr double kTrackStartOnset = 0.1;
constexpr double kStrongOnsetWindowSeconds = 4;
constexpr std::size_t kMaxKicks = 8;
constexpr std::size_t kMinKicks = 4;
// Phases in beats
constexpr double kAgreement = 0.06;
constexpr double kMinMove = 0.03;
constexpr double kMinBpmToHalve = 150;

// The phase of a position on the grid, between -0.5 and 0.5 beats
double phaseOf(double position, double firstBeat, double beatLength) {
    const double beats = (position - firstBeat) / beatLength + 0.5;
    return beats - std::floor(beats) - 0.5;
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    return values.size() % 2 ? values[middle] : (values[middle - 1] + values[middle]) / 2;
}

} // namespace

void KickGrid::initialize(audio::SampleRate sampleRate) {
    m_sampleRate = sampleRate;
    m_lowPassCoefficient = std::exp(-2 * M_PI * kLowPassHz / sampleRate.value());
    m_lowPass1 = 0;
    m_lowPass2 = 0;
    m_hopEnergy = 0;
    m_hopFrames = 0;
    m_maxHops = static_cast<SINT>(kSecondsKept * sampleRate.value() / kHopFrames);
    m_envelope.clear();
    m_envelope.reserve(m_maxHops);
}

void KickGrid::process(const CSAMPLE* pSamples, SINT frames, int channels) {
    if (channels < 1 || static_cast<SINT>(m_envelope.size()) >= m_maxHops) {
        return;
    }
    const double a = m_lowPassCoefficient;
    for (SINT frame = 0; frame < frames; ++frame) {
        double mono = 0;
        for (int channel = 0; channel < channels; ++channel) {
            mono += pSamples[frame * channels + channel];
        }
        mono /= channels;
        // Two one-pole low-passes
        m_lowPass1 = (1 - a) * mono + a * m_lowPass1;
        m_lowPass2 = (1 - a) * m_lowPass1 + a * m_lowPass2;
        m_hopEnergy += m_lowPass2 * m_lowPass2;
        if (++m_hopFrames == kHopFrames) {
            m_envelope.push_back(static_cast<float>(std::sqrt(m_hopEnergy / kHopFrames)));
            m_hopEnergy = 0;
            m_hopFrames = 0;
            if (static_cast<SINT>(m_envelope.size()) >= m_maxHops) {
                return;
            }
        }
    }
}

BeatsPointer KickGrid::correct(const BeatsPointer& pBeats) const {
    if (!pBeats || m_envelope.size() < 3 || !m_sampleRate.isValid()) {
        return pBeats;
    }
    // Onsets: the rise of the low frequencies from one hop to the next
    std::vector<double> onset(m_envelope.size(), 0);
    double strongest = 0;
    for (std::size_t i = 1; i < m_envelope.size(); ++i) {
        onset[i] = std::max(0.0, static_cast<double>(m_envelope[i] - m_envelope[i - 1]));
        strongest = std::max(strongest, onset[i]);
    }
    if (strongest <= 0) {
        return pBeats;
    }
    // Where the track starts: the first onset of some strength, at its peak
    std::size_t start = 1;
    while (start < onset.size() && onset[start] <= kTrackStartOnset * strongest) {
        ++start;
    }
    while (start + 1 < onset.size() && onset[start + 1] >= onset[start]) {
        ++start;
    }
    if (start + 1 >= onset.size()) {
        return pBeats;
    }
    // The first strong onsets from there: the intro kicks
    const std::size_t windowEnd = std::min(onset.size(),
            start + static_cast<std::size_t>(kStrongOnsetWindowSeconds * m_sampleRate.value() / kHopFrames));
    const double threshold = kStrongOnset *
            *std::max_element(onset.begin() + start, onset.begin() + windowEnd);
    std::vector<double> kicks;
    for (std::size_t i = start; i + 1 < onset.size() && kicks.size() < kMaxKicks; ++i) {
        if (onset[i] >= threshold && onset[i] >= onset[i - 1] && onset[i] > onset[i + 1]) {
            kicks.push_back(i * kHopFrames + kHopFrames / 2.0);
        }
    }
    if (kicks.size() < kMinKicks) {
        return pBeats;
    }
    const double trackStart = start * kHopFrames + kHopFrames / 2.0;

    BeatsPointer pResult = pBeats;
    const Bpm bpm = pBeats->getBpmInRange(audio::kStartFramePos,
            audio::FramePos(m_envelope.size() * static_cast<double>(kHopFrames)));
    if (!bpm.isValid()) {
        return pBeats;
    }
    double beatLength = 60.0 * m_sampleRate.value() / bpm.value();

    // A doubled BPM: the intro kicks come every second beat
    std::vector<double> intervals;
    for (std::size_t i = 1; i < kicks.size(); ++i) {
        intervals.push_back(kicks[i] - kicks[i - 1]);
    }
    if (bpm.value() >= kMinBpmToHalve &&
            std::abs(median(intervals) / beatLength - 2) < 0.1) {
        const auto halved = pResult->tryScale(Beats::BpmScale::Halve);
        if (halved) {
            pResult = *halved;
            beatLength *= 2;
            qDebug() << "KickGrid: halved the BPM of" << bpm.value();
        }
    }

    // Move the grid onto the kicks if they and the track start agree
    if (!pResult->firstBeat().isValid()) {
        return pResult;
    }
    const double firstBeat = pResult->firstBeat().value();
    std::vector<double> phases;
    for (double kick : kicks) {
        phases.push_back(phaseOf(kick, firstBeat, beatLength));
    }
    const double move = median(phases);
    const auto agreeing = std::count_if(phases.begin(), phases.end(), [move](double phase) {
        return std::abs(phase - move) < kAgreement;
    });
    const bool startAgrees =
            std::abs(phaseOf(trackStart, firstBeat, beatLength) - move) < kAgreement;
    if (agreeing >= 0.75 * phases.size() && startAgrees && std::abs(move) > kMinMove) {
        const auto moved = pResult->tryTranslateBeats(move);
        if (moved) {
            pResult = *moved;
            qDebug() << "KickGrid: moved the beatgrid by" << move << "beats onto the kicks";
        }
    }
    return pResult;
}

} // namespace android
} // namespace mixxx
