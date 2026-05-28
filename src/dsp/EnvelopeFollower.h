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

#include <juce_core/juce_core.h>
#include <cmath>
#include <algorithm>

namespace doobie
{
// Asymmetric peak-style envelope follower for the mod matrix. Asymmetric
// attack/release coefficients track louder input quickly then release slowly.
// Output is a unipolar [0, 1] envelope (sensitivity scales the input before
// the envelope clamp, so the user can compensate for quiet sources).
class EnvelopeFollower
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        recomputeCoefs();
        reset();
    }

    void reset() noexcept { env = 0.0f; }

    void setAttack (float ms) noexcept
    {
        attackMs = juce::jmax (0.1f, ms);
        recomputeCoefs();
    }

    void setRelease (float ms) noexcept
    {
        releaseMs = juce::jmax (1.0f, ms);
        recomputeCoefs();
    }

    void setSensitivity (float decibels) noexcept
    {
        sens = juce::Decibels::decibelsToGain (decibels);
    }

    // Process a block; updates the internal envelope. Read the result with value().
    void processBlock (const float* L, const float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            const float in = std::max (std::fabs (L[i]), std::fabs (R[i])) * sens;
            const float coef = in > env ? attackCoef : releaseCoef;
            env += coef * (in - env);
        }
    }

    float value() const noexcept { return juce::jlimit (0.0f, 1.0f, env); }

private:
    void recomputeCoefs() noexcept
    {
        attackCoef  = 1.0f - std::exp (-1.0f / (attackMs  * 0.001f * (float) sampleRate));
        releaseCoef = 1.0f - std::exp (-1.0f / (releaseMs * 0.001f * (float) sampleRate));
    }

    double sampleRate = 44100.0;
    float attackMs   = 5.0f;
    float releaseMs  = 100.0f;
    float attackCoef = 0.5f, releaseCoef = 0.01f;
    float sens = 1.0f;
    float env  = 0.0f;
};
} // namespace doobie
