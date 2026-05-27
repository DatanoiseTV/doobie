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

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace doobie
{
// "Tape age": one knob that models a worn, old tape rather than just a noise
// floor. The amount drives four facets, all derived from the same control so
// "more age" sounds like a single coherent thing:
//
//   * hiss          — the noise floor (what AGE used to be on its own),
//   * dropouts      — a slow level wobble with occasional dips, as oxide sheds,
//   * HF loss       — progressive dulling, worse each recirculation,
//   * transport     — extra wow/flutter from a tired capstan/pinch roller.
//
// The level processing is applied to the recirculating feedback signal so the
// wear compounds across repeats the way a real tape echo ages. Everything is
// bounded (gain <= 1, stable one-pole) so it is safe inside the feedback loop,
// and at amount 0 it is a true bypass.
class TapeAge
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate  = sr;
        holdSamples = std::max (1, (int) (sr * 0.12));                 // new dropout target ~120 ms
        dropSmooth  = 1.0f - std::exp (-6.2831853f * 5.0f    / (float) sr); // ~5 Hz glide between targets
        hfCoef      = 1.0f - std::exp (-6.2831853f * 7000.0f / (float) sr); // ~7 kHz HF-loss pole
        reset();
    }

    void reset() noexcept
    {
        mod = target = 0.0f;
        counter = 0;
        lpL = lpR = 0.0f;
        rng = 0x9E3779B9u;
    }

    void setAmount (float a) noexcept { age = std::clamp (a, 0.0f, 1.0f); }

    // Macro fan-out: extra instability and noise floor the engine folds into the
    // wow/flutter generator and the hiss it writes to tape.
    float wowBoost()     const noexcept { return age * 0.10f; }
    float flutterBoost() const noexcept { return age * 0.18f; }
    float hissLevel()    const noexcept { return age * 0.015f; }

    // White noise (own stream, so hiss stays decorrelated from the engine's).
    inline float noise() noexcept
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (int32_t) rng * (1.0f / 2147483648.0f);
    }

    // Play a stereo feedback sample "through worn tape" in place: a wandering
    // dropout gain (shared L/R — one transport) and progressive HF loss.
    inline void process (float& l, float& r) noexcept
    {
        if (age <= 0.0f)
            return;

        // Sample-and-hold a new dropout target, then glide toward it, giving a
        // slow control signal in ~[-1, 1].
        if (--counter <= 0)
        {
            target  = noise();
            counter = holdSamples;
        }
        mod += dropSmooth * (target - mod);
        const float gain = 1.0f - age * 0.45f * (0.5f - 0.5f * mod); // in [1 - 0.45*age, 1]

        // Progressive high-frequency loss, compounding through the feedback path.
        lpL += hfCoef * (l - lpL);
        lpR += hfCoef * (r - lpR);
        const float blend = age * 0.5f;
        l = (l * (1.0f - blend) + lpL * blend) * gain;
        r = (r * (1.0f - blend) + lpR * blend) * gain;
    }

private:
    double sampleRate = 44100.0;
    float  age = 0.0f;
    int    holdSamples = 1, counter = 0;
    float  dropSmooth = 0.001f, hfCoef = 0.5f;
    float  mod = 0.0f, target = 0.0f, lpL = 0.0f, lpR = 0.0f;
    uint32_t rng = 0x9E3779B9u;
};
} // namespace doobie
