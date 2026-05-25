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

#include <cmath>
#include <random>

namespace doobie
{
// Generates the slow "wow" and faster "flutter" pitch instabilities of a tape
// transport. Returns a multiplicative factor close to 1.0 that the delay engine
// applies to its read offsets, so the whole repeat drifts in pitch the way a
// worn capstan would. A little filtered noise is mixed into both rates so the
// movement never sounds like a pure LFO.
class WowFlutter
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        reset();
    }

    void reset()
    {
        wowPhase = flutPhase = 0.0f;
        drift = driftTarget = 0.0f;
        driftCounter = 0;
    }

    // wow/flutter in 0..1.
    void setAmounts (float wow, float flutter) noexcept
    {
        wowAmt  = wow;
        flutAmt = flutter;
    }

    inline float next() noexcept
    {
        constexpr float twoPi = 6.28318530718f;

        // Slowly wandering target so the wow rate itself breathes a little.
        if (--driftCounter <= 0)
        {
            driftTarget  = (dist (rng) * 2.0f - 1.0f);
            driftCounter = (int) (sampleRate * 0.5);
        }
        drift += 0.0002f * (driftTarget - drift);

        const float wowRate  = 0.55f + 0.25f * drift;   // ~0.3..0.8 Hz
        const float flutRate = 6.3f  + 1.5f  * drift;    // ~5..8 Hz

        wowPhase  += wowRate  / (float) sampleRate;
        flutPhase += flutRate / (float) sampleRate;
        if (wowPhase  >= 1.0f) wowPhase  -= 1.0f;
        if (flutPhase >= 1.0f) flutPhase -= 1.0f;

        const float wow  = std::sin (twoPi * wowPhase);
        const float flut = std::sin (twoPi * flutPhase);

        // Wow reaches ~1.2 % time deviation, flutter ~0.35 %.
        const float depth = wowAmt * 0.012f * wow
                          +  flutAmt * 0.0035f * flut
                          +  (wowAmt + flutAmt) * 0.0015f * drift;
        return 1.0f + depth;
    }

private:
    double sampleRate = 44100.0;
    float wowAmt = 0.0f, flutAmt = 0.0f;
    float wowPhase = 0.0f, flutPhase = 0.0f;
    float drift = 0.0f, driftTarget = 0.0f;
    int   driftCounter = 0;

    std::mt19937 rng { 0xD00B1E };
    std::uniform_real_distribution<float> dist { 0.0f, 1.0f };
};
} // namespace doobie
