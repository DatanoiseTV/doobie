#pragma once

#include "DelayLine.h"
#include <array>

namespace doobie
{
// A practical spring-tank emulation. The metallic "boing" of a real spring
// comes from dispersion: high frequencies travel through the spring faster than
// low ones. We recreate that with a long cascade of first-order all-pass
// filters (which add a frequency-dependent group delay) sitting inside a short
// feedback delay loop, band-limited the way a physical tank is. Two slightly
// detuned lines give a stereo image.
class SpringReverb
{
public:
    void prepare (double sampleRate);
    void reset();

    // decay 0..1 (ring time), tone 0..1 (dark..bright).
    void setParams (float decay, float tone) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    // One spring line: band-limit -> [delay -> dispersion -> damping] loop.
    struct Line
    {
        DelayLine delay;
        float baseDelay = 0.0f;     // samples

        static constexpr int kStages = 28;
        std::array<float, kStages> apX {};
        std::array<float, kStages> apY {};
        float apCoef = 0.66f;

        float fb = 0.6f;
        float dampZ = 0.0f, dampCoef = 0.4f;
        float dcX = 0.0f, dcY = 0.0f;
        float modPhase = 0.0f, modRate = 0.0f;

        void prepare (double sr, float delaySeconds, float modHz);
        void reset();
        float process (float x, double sr) noexcept;
    };

    Line lineL, lineR;
    double sampleRate = 44100.0;

    // Input band-pass: springs barely pass deep lows or high treble.
    float hpL = 0.0f, hpR = 0.0f; // one-pole high-pass state
    float lpL = 0.0f, lpR = 0.0f; // one-pole low-pass state
    float lpCoef = 0.5f;
};
} // namespace doobie
