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
//
// Optional sidechain TPT state-variable filter (LP / HP / BP) shapes the
// signal that drives the follower. Lets the env source react only to a
// chosen frequency band — bass kick (LP ~150 Hz), hi-hat (HP ~5 kHz),
// vocal sibilance (BP), etc. Off = follower sees the raw input.
class EnvelopeFollower
{
public:
    enum class FilterType : int { LP = 0, HP = 1, BP = 2 };

    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        recomputeCoefs();
        recomputeFilterCoefs();
        reset();
    }

    void reset() noexcept
    {
        env = 0.0f;
        scLpL = scLpR = scBpL = scBpR = 0.0f;
    }

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

    // Sidechain filter — `on` enables the SVF on the env-follower's input
    // tap (does not affect the main audio path). Useful for triggering
    // the follower only on a frequency band.
    void setFilterEnabled (bool on)   noexcept { filterOn = on; }
    void setFilterType    (FilterType t) noexcept { filterType = t; }
    void setFilterFreq    (float hz)  noexcept
    {
        filterFreqHz = juce::jlimit (20.0f, 18000.0f, hz);
        recomputeFilterCoefs();
    }
    void setFilterRes     (float r)   noexcept
    {
        filterRes = juce::jlimit (0.0f, 0.95f, r);
        recomputeFilterCoefs();
    }

    // Process a block; updates the internal envelope. Read the result with value().
    void processBlock (const float* L, const float* R, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            float l = L[i];
            float r = R[i];
            if (filterOn)
            {
                // TPT-SVF — same shape as the input filter in DubDelayEngine.
                // Computed per-sample so freq sweeps stay zipper-free.
                const float hpL = (l - (2.0f * filterRcoef + filterGcoef) * scBpL - scLpL) * filterDcoef;
                const float bpL = filterGcoef * hpL + scBpL;
                const float lpL = filterGcoef * bpL + scLpL;
                scBpL = filterGcoef * hpL + bpL;
                scLpL = filterGcoef * bpL + lpL;
                const float hpR = (r - (2.0f * filterRcoef + filterGcoef) * scBpR - scLpR) * filterDcoef;
                const float bpR = filterGcoef * hpR + scBpR;
                const float lpR = filterGcoef * bpR + scLpR;
                scBpR = filterGcoef * hpR + bpR;
                scLpR = filterGcoef * bpR + lpR;
                switch (filterType)
                {
                    case FilterType::HP: l = hpL; r = hpR; break;
                    case FilterType::BP: l = bpL; r = bpR; break;
                    case FilterType::LP: default: l = lpL; r = lpR; break;
                }
            }
            const float in = std::max (std::fabs (l), std::fabs (r)) * sens;
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
    void recomputeFilterCoefs() noexcept
    {
        // TPT-SVF prewarp.
        const float maxHz = 0.45f * (float) sampleRate;
        const float fc    = std::min (filterFreqHz, maxHz);
        const float g     = std::tan (3.14159265f * fc / (float) sampleRate);
        const float k     = 2.0f - 2.0f * filterRes;  // resonance scaler
        filterGcoef = g;
        filterRcoef = k * 0.5f;
        filterDcoef = 1.0f / (1.0f + g * (g + k));
    }

    double sampleRate = 44100.0;
    float attackMs   = 5.0f;
    float releaseMs  = 100.0f;
    float attackCoef = 0.5f, releaseCoef = 0.01f;
    float sens = 1.0f;
    float env  = 0.0f;

    bool       filterOn   = false;
    FilterType filterType = FilterType::LP;
    float      filterFreqHz = 1200.0f;
    float      filterRes    = 0.15f;
    float      filterGcoef = 0.0f, filterRcoef = 0.0f, filterDcoef = 1.0f;
    float      scLpL = 0.0f, scLpR = 0.0f, scBpL = 0.0f, scBpR = 0.0f;
};
} // namespace doobie
