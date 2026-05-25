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

#include "PlateReverb.h"
#include <cmath>
#include <algorithm>

namespace doobie
{
namespace
{
    // In-place 8-point Walsh-Hadamard transform. This applies the (unnormalised)
    // Hadamard mixing matrix in 24 add/subtracts instead of 64 multiplies.
    inline void hadamard8 (std::array<float, 8>& a) noexcept
    {
        for (int len = 1; len < 8; len <<= 1)
            for (int i = 0; i < 8; i += len << 1)
                for (int j = i; j < i + len; ++j)
                {
                    const float x = a[(size_t) j];
                    const float y = a[(size_t) (j + len)];
                    a[(size_t) j]         = x + y;
                    a[(size_t) (j + len)] = x - y;
                }
    }

    // Output tap sign patterns: two near-orthogonal columns decorrelate L/R.
    constexpr std::array<float, 8> cL { 1.f, -1.f,  1.f, -1.f,  1.f, -1.f,  1.f, -1.f };
    constexpr std::array<float, 8> cR { 1.f,  1.f, -1.f, -1.f,  1.f,  1.f, -1.f, -1.f };
}

void PlateReverb::prepare (double sr)
{
    sampleRate = sr;

    // Mutually incommensurate line lengths (ms) avoid coincident echoes.
    constexpr std::array<float, N> ms { 18.0f, 22.7f, 27.3f, 31.1f, 36.7f, 40.9f, 44.3f, 49.7f };
    constexpr std::array<float, N> rateHz { 0.61f, 0.73f, 0.82f, 0.94f, 1.07f, 1.18f, 1.29f, 1.41f };

    for (int i = 0; i < N; ++i)
    {
        lines[(size_t) i].prepare (sr, 0.25);     // headroom for size + modulation
        baseLen[(size_t) i]  = ms[(size_t) i] * 0.001f * (float) sr;
        modRate[(size_t) i]  = rateHz[(size_t) i];
        modPhase[(size_t) i] = (float) i * 0.1f;
    }

    constexpr std::array<float, 4> apMs { 4.77f, 3.59f, 12.7f, 9.31f };
    for (int i = 0; i < 4; ++i)
    {
        ap[(size_t) i].prepare (sr, 0.05);
        apLen[(size_t) i] = apMs[(size_t) i] * 0.001f * (float) sr;
    }

    predelay.prepare (sr, 0.25);
    reset();
}

void PlateReverb::reset()
{
    for (auto& l : lines) l.reset();
    for (auto& a : ap)    a.reset();
    predelay.reset();
    lpZ.fill (0.0f);
}

void PlateReverb::setParams (float decay, float size, float damp, float predelayMs, float mod) noexcept
{
    decay = std::clamp (decay, 0.0f, 1.0f);
    size  = std::clamp (size,  0.0f, 1.0f);
    damp  = std::clamp (damp,  0.0f, 1.0f);
    mod   = std::clamp (mod,   0.0f, 1.0f);

    g         = 0.5f + 0.49f * decay;             // 0.5..0.99, always stable
    sizeScale = 0.6f + 0.9f * size;               // 0.6..1.5
    dampCoef  = 0.15f + (1.0f - damp) * 0.8f;     // brighter when damp is low
    modDepth  = mod;
    preLenSamples = std::clamp (predelayMs, 0.0f, 200.0f) * 0.001f * (float) sampleRate;

    // The recirculating energy grows like 1/(1-g^2); compensating with
    // sqrt(1-g^2) keeps the wet return at a roughly constant level whatever the
    // decay time, so the REVERB MIX control behaves predictably.
    outGain = 0.4f * std::sqrt (std::max (0.02f, 1.0f - g * g));
}

void PlateReverb::process (float inL, float inR, float& outL, float& outR) noexcept
{
    constexpr float twoPi = 6.28318530718f;

    // Pre-delay and input diffusion on the mono sum feeding the plate.
    float x = 0.5f * (inL + inR);
    predelay.write (x);
    x = predelay.read (std::max (1.0f, preLenSamples));

    for (int i = 0; i < 4; ++i)
    {
        const float z = ap[(size_t) i].read (apLen[(size_t) i]);
        const float y = -apG[(size_t) i] * x + z;
        ap[(size_t) i].write (x + apG[(size_t) i] * y);
        x = y;
    }

    // Read every line (with per-line modulation) and damp it.
    std::array<float, N> s {};
    for (int i = 0; i < N; ++i)
    {
        modPhase[(size_t) i] += modRate[(size_t) i] / (float) sampleRate;
        if (modPhase[(size_t) i] >= 1.0f) modPhase[(size_t) i] -= 1.0f;

        const float len = baseLen[(size_t) i] * sizeScale
                        * (1.0f + modDepth * 0.004f * std::sin (twoPi * modPhase[(size_t) i]));

        float v = lines[(size_t) i].read (std::max (2.0f, len));
        lpZ[(size_t) i] += dampCoef * (v - lpZ[(size_t) i]);
        s[(size_t) i] = lpZ[(size_t) i];
    }

    // Output taps before mixing (so the wet reflects the current tank state).
    float oL = 0.0f, oR = 0.0f;
    for (int i = 0; i < N; ++i)
    {
        oL += cL[(size_t) i] * s[(size_t) i];
        oR += cR[(size_t) i] * s[(size_t) i];
    }
    outL = oL * outGain;
    outR = oR * outGain;

    // Mix through the Hadamard matrix and feed back with the decay gain.
    hadamard8 (s);
    constexpr float norm = 0.35355339f; // 1/sqrt(8)
    for (int i = 0; i < N; ++i)
    {
        const float inject = (i & 1) ? -x : x;  // alternate sign for decorrelation
        lines[(size_t) i].write (inject + g * norm * s[(size_t) i]);
    }
}
} // namespace doobie
