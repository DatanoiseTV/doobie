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

#include "DelayLine.h"
#include <array>

namespace doobie
{
// A spring-tank emulation. The metallic "boing" of a real spring comes from
// dispersion: high frequencies travel through the spring faster than low ones.
// We recreate that with a long cascade of first-order all-pass filters (a
// frequency-dependent group delay) sitting inside a short feedback delay loop,
// band-limited the way a physical tank is. A resonant band-pass on the wet
// output adds the characteristic high "ting"/chirp, and slow modulation keeps
// the tail alive. Two detuned lines give a stereo tank.
class SpringReverb
{
public:
    void prepare (double sampleRate);
    void reset();

    // decay 0..1 (ring time), tone 0..1 (dark..bright), mod 0..1 (movement).
    void setParams (float decay, float tone, float mod) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    // Resonant band-pass (RBJ biquad) used to colour the wet output.
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        float x1 = 0, x2 = 0, y1 = 0, y2 = 0;
        void set (double sr, float fc, float q) noexcept;
        inline float process (float x) noexcept
        {
            const float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = x;
            y2 = y1; y1 = y;
            return y;
        }
        void reset() noexcept { x1 = x2 = y1 = y2 = 0.0f; }
    };

    // One spring line: band-limit -> [delay -> dispersion -> damping] loop.
    struct Line
    {
        DelayLine delay;
        float baseDelay = 0.0f;     // samples

        static constexpr int kStages = 80;
        std::array<float, kStages> apX {};
        std::array<float, kStages> apY {};
        float apCoef = 0.62f;

        float fb = 0.6f;
        float dampZ = 0.0f, dampCoef = 0.4f;
        float dcX = 0.0f, dcY = 0.0f;
        float modPhase = 0.0f, modRate = 0.0f, modDepth = 0.0f;

        Biquad ting;

        void prepare (double sr, float delaySeconds, float modHz);
        void reset();
        float process (float x, double sr) noexcept;
    };

    Line lineL, lineR;
    double sampleRate = 44100.0;

    // Input band-pass: springs barely pass deep lows or high treble.
    float hpL = 0.0f, hpR = 0.0f;
    float lpL = 0.0f, lpR = 0.0f;
    float lpCoef = 0.5f;

    float tingMix = 0.35f;
    float outGain = 1.6f;
};
} // namespace doobie
