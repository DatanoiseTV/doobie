/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version, and distributed WITHOUT ANY WARRANTY. See <https://www.gnu.org/licenses/>.
  You must retain this notice and the attribution to DatanoiseTV in any
  redistributed or derivative version.
*/

#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <vector>
#include <complex>

namespace doobie
{
// A phase-vocoder pitch shifter (the classic STFT analysis / bin-shift /
// resynthesis method). It tracks each bin's true frequency from the phase
// advance between hops, moves the partials to ratio*frequency, and rebuilds a
// coherent phase on synthesis, so an octave shift stays smooth and harmonic
// rather than warbling like a granular delay-line shifter. Output level is
// auto-calibrated at prepare() so it does not depend on the FFT's internal
// scaling convention. Latency is N-hop samples; only the wet/pitched path sees
// it, so the dry signal is unaffected.
class FftPitchShifter
{
public:
    void prepare (double sr);
    void reset();

    void setRatio (float r) noexcept { ratio = r; }

    float process (float x) noexcept;

private:
    void processFrame() noexcept;

    static constexpr int order = 10;
    static constexpr int N     = 1 << order; // 1024
    static constexpr int hop   = N / 4;       // 256 (4x overlap)
    static constexpr int bins  = N / 2 + 1;

    juce::dsp::FFT fft { order };

    std::array<float, N> window {};
    std::array<float, N> inFIFO {};
    std::array<float, N> outFIFO {};
    std::array<float, N> outAccum {};
    std::array<float, bins> lastPhase {};
    std::array<float, bins> sumPhase {};
    std::array<float, bins> anaMag {};
    std::array<float, bins> anaFreq {};
    std::array<float, bins> synMag {};
    std::array<float, bins> synFreq {};

    std::vector<std::complex<float>> spec, time;

    int   rover = N - hop;
    float ratio = 2.0f;
    double sampleRate = 44100.0;
    float freqPerBin = 1.0f;
    float expct = 0.0f;
    float normGain = 1.0f;
};
} // namespace doobie
