#pragma once

#include <fftw3.h>

#include <complex>
#include <vector>

#include "stems/stemseparator.h"

namespace mixxx {
namespace stems {

/// Spleeter 4 stems (Deezer, MIT): one model per stem estimates its
/// magnitude spectrogram, then a multichannel Wiener filter (as in
/// Spleeter's "mwf" mode, one EM iteration like norbert) builds the stems
/// from the mix. The bass of the stems is taken from the low end of the mix
/// minus the drums, which is where Spleeter is weakest.
///
/// Fast and light: the audio is processed in 11.9 s blocks, streaming the
/// stems out.
class SpleeterSeparator : public StemSeparator {
  public:
    SpleeterSeparator();
    ~SpleeterSeparator() override;

    bool load(const QString& modelDir, const Options& options) override;
    bool separate(const float* pStereo,
            SINT frameCount,
            const StemSink& sink,
            const ProgressCallback& progress) override;

  private:
    using Complex = std::complex<float>;

    /// Runs the models on the magnitudes of a block (m_modelInput) into
    /// m_estimates
    bool estimateBlock();
    /// Filters the spectra of the block into the stems and overlap-adds
    /// them to the output
    void filterBlock(SINT firstFrame, int frames);
    void addOutputFrame(SINT frame);
    /// Hands the final output samples over to the sink
    bool emitOutput(SINT finalPaddedSample, SINT frameCount, const StemSink& sink);

    std::array<std::unique_ptr<Ort::Session>, kStemCount> m_sessions;

    // STFT
    std::vector<float> m_window;
    float* m_pFftTime;
    fftwf_complex* m_pFftBins;
    fftwf_plan m_forwardPlan;
    fftwf_plan m_inversePlan;

    /// Spectra of the mix in the block: [channel][frame][bin]
    std::vector<Complex> m_spectra;
    /// Model input: [channel][frame][model bin]
    std::vector<float> m_modelInput;
    /// Estimated magnitudes: [stem][channel][frame][model bin]
    std::vector<float> m_estimates;
    /// The mean estimate of a frame, used above the model bins:
    /// [stem][channel][frame]
    std::vector<float> m_estimateTails;
    /// Spectra of the stems of the current frame: [stem][channel][bin]
    std::vector<Complex> m_frameStems;

    /// Overlap-add of the stems: [stem * 2 + channel][padded sample - origin]
    std::array<std::vector<float>, kStemCount * 2> m_output;
    std::vector<float> m_windowSum;
    SINT m_outputOrigin;
    SINT m_emittedFrames;
    std::array<std::vector<float>, kStemCount> m_emitBuffers;
};

} // namespace stems
} // namespace mixxx
