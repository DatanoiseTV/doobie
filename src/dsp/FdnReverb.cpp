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

#include "FdnReverb.h"
#include <cmath>
#include <algorithm>

namespace doobie
{
namespace
{
    // In-place 16-point Walsh-Hadamard transform (the 16x16 Hadamard mixing
    // matrix in 64 add/subtracts instead of 256 multiplies).
    inline void hadamard16 (std::array<float, 16>& a) noexcept
    {
        for (int len = 1; len < 16; len <<= 1)
            for (int i = 0; i < 16; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float x = a[(size_t) j];
                    const float y = a[(size_t) (j + len)];
                    a[(size_t) j]         = x + y;
                    a[(size_t) (j + len)] = x - y;
                }
    }
}

void FdnReverb::prepare (double sr, float minMs, float maxMs)
{
    sampleRate = sr;

    // Spread sixteen incommensurate line lengths across the range. A small
    // irrational jitter per line keeps the modes from lining up into a ring.
    for (int i = 0; i < N; ++i)
    {
        const float t  = (float) i / (float) (N - 1);
        const float ms = minMs + (maxMs - minMs) * t + 0.37f * std::sin (2.39996f * (float) i);
        lines[(size_t) i].prepare (sr, 0.3);
        baseLen[(size_t) i] = ms * 0.001f * (float) sr;
        modRate[(size_t) i] = 0.5f + 0.9f * t + 0.05f * (float) (i % 3);
        modPhase[(size_t) i] = 0.063f * (float) i;

        // Two distinct Walsh columns for L/R taps and a third for injection.
        cL[(size_t) i]  = (i & 1) ? -1.0f : 1.0f;
        cR[(size_t) i]  = (i & 2) ? -1.0f : 1.0f;
        bIn[(size_t) i] = (i & 4) ? -1.0f : 1.0f;
    }

    constexpr std::array<float, 6> apMs { 7.3f, 11.9f, 16.7f, 22.1f, 27.7f, 33.5f };
    for (int i = 0; i < 6; ++i)
    {
        ap[(size_t) i].prepare (sr, 0.06);
        apLen[(size_t) i] = apMs[(size_t) i] * 0.001f * (float) sr;
    }

    predelay.prepare (sr, 0.25);
    hpCoef = 1.0f - (2.0f * 3.14159265f * 45.0f / (float) sr); // ~45 Hz high-pass
    reset();
}

void FdnReverb::reset()
{
    for (auto& l : lines) l.reset();
    for (auto& a : ap)    a.reset();
    predelay.reset();
    lpZ.fill (0.0f);
    hpX = hpY = 0.0f;
}

void FdnReverb::setParams (float decay, float size, float damp, float predelayMs, float mod) noexcept
{
    decay = std::clamp (decay, 0.0f, 1.0f);
    size  = std::clamp (size,  0.0f, 1.0f);
    damp  = std::clamp (damp,  0.0f, 1.0f);
    mod   = std::clamp (mod,   0.0f, 1.0f);

    g         = 0.6f + 0.39f * decay;             // 0.6..0.99
    sizeScale = 0.7f + 0.7f * size;               // 0.7..1.4
    dampCoef  = 0.12f + (1.0f - damp) * 0.7f;     // brighter when damp is low
    modDepth  = mod;
    preLenSamples = std::clamp (predelayMs, 0.0f, 200.0f) * 0.001f * (float) sampleRate;

    // Keep the wet level consistent across decay settings (energy ~ 1/(1-g^2)).
    outGain = 0.34f * std::sqrt (std::max (0.02f, 1.0f - g * g));
}

void FdnReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    constexpr float twoPi = 6.28318530718f;

    // Pre-delay + gentle high-pass on the mono sum feeding the network.
    float x = 0.5f * (inL + inR);
    predelay.write (x);
    x = predelay.read (std::max (1.0f, preLenSamples));

    const float hp = hpCoef * (hpY + x - hpX);
    hpX = x;
    hpY = hp;
    x = hp;

    // Input diffusion chain.
    for (int i = 0; i < 6; ++i)
    {
        const float z = ap[(size_t) i].read (apLen[(size_t) i]);
        const float y = -apG[(size_t) i] * x + z;
        ap[(size_t) i].write (x + apG[(size_t) i] * y);
        x = y;
    }

    std::array<float, N> s {};
    for (int i = 0; i < N; ++i)
    {
        modPhase[(size_t) i] += modRate[(size_t) i] / (float) sampleRate;
        if (modPhase[(size_t) i] >= 1.0f) modPhase[(size_t) i] -= 1.0f;

        const float len = baseLen[(size_t) i] * sizeScale
                        * (1.0f + modDepth * 0.003f * std::sin (twoPi * modPhase[(size_t) i]));

        const float v = lines[(size_t) i].read (std::max (2.0f, len));
        lpZ[(size_t) i] += dampCoef * (v - lpZ[(size_t) i]);
        s[(size_t) i] = lpZ[(size_t) i];
    }

    float oL = 0.0f, oR = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        oL += cL[(size_t) i] * s[(size_t) i];
        oR += cR[(size_t) i] * s[(size_t) i];
    }
    outL = oL * outGain;
    outR = oR * outGain;

    hadamard16 (s);
    constexpr float norm = 0.25f; // 1/sqrt(16)
    for (int i = 0; i < N; ++i)
        lines[(size_t) i].write (x * bIn[(size_t) i] + g * norm * s[(size_t) i]);
}
} // namespace doobie
