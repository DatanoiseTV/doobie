#pragma once

#include "DelayLine.h"
#include <array>

namespace doobie
{
// A smooth plate/hall reverb built as a feedback delay network (FDN). Eight
// modulated delay lines are mixed every sample through an 8x8 Hadamard matrix,
// which is lossless and energy-preserving, so the tail decays purely by the
// chosen feedback gain and per-line damping rather than by colouration. Two
// all-pass diffusers up front smear transients into the characteristic dense
// wash of a plate. Modulation keeps the tail from ringing metallically.
class PlateReverb
{
public:
    void prepare (double sampleRate);
    void reset();

    // decay/size/damp/mod in 0..1, predelay in milliseconds.
    void setParams (float decay, float size, float damp, float predelayMs, float mod) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    static constexpr int N = 8;

    double sampleRate = 44100.0;

    std::array<DelayLine, N> lines;
    std::array<float, N> baseLen {};   // samples at unit size
    std::array<float, N> lpZ {};       // damping low-pass state
    std::array<float, N> modPhase {};
    std::array<float, N> modRate {};

    // Input diffusers.
    std::array<DelayLine, 4> ap;
    std::array<float, 4> apLen {};
    std::array<float, 4> apG { 0.72f, 0.72f, 0.625f, 0.625f };

    DelayLine predelay;

    // Smoothed control values.
    float g = 0.85f;          // FDN feedback gain
    float sizeScale = 1.0f;
    float dampCoef = 0.5f;
    float modDepth = 0.3f;
    float preLenSamples = 0.0f;
};
} // namespace doobie
