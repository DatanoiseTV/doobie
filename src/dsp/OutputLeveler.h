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
#include <algorithm>

namespace doobie
{
// Two-stage output leveler designed for the live-use case where feedback
// is driven near self-oscillation. A naive output limiter would either
// clip (brick-wall too aggressive) or pump (single-stage compressor too
// hard). Stacking a SLOW program leveler with a FAST ceiling limiter
// gives you "auto-gain" feel — long-term loudness is held constant
// without crushing transient detail.
//
// Stage 1 — program leveler (LA-2A-style):
//   - Linked-stereo peak follower with slow attack (50 ms) and slow
//     release (500 ms). Hears the *level* not the *transients*.
//   - Soft-knee 2:1 above -6 dBFS so the gain reduction eases in
//     gradually rather than slamming. Floor at -12 dB (max GR) so we
//     never crush the signal to a flat squashed mess.
//
// Stage 2 — ceiling catcher:
//   - Fast (1 ms attack / 30 ms release) peak limiter at -0.3 dBFS.
//   - Only does anything for transients the leveler hasn't tamed —
//     usually inaudible.
//
// Both stages are linked-stereo (peak of L+R drives both channels'
// gain) so the stereo image stays put when one channel runs hot.
//
// `enabled = false` is a true bypass (no envelope work, no gain
// multiplication) so a user can toggle it off for raw-feel testing
// without leaving residual state.
class OutputLeveler
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        // Coefficients for one-pole rms-style envelope followers.
        // tau = -1 / (sr * ln(coef)) — solved for coef = exp(-1 / (sr * tau)).
        levAtt = 1.0f - std::exp (-1.0f / (0.050f * (float) sr));   // 50 ms attack
        levRel = 1.0f - std::exp (-1.0f / (0.500f * (float) sr));   // 500 ms release
        clAtt  = 1.0f - std::exp (-1.0f / (0.001f * (float) sr));   // 1 ms attack
        clRel  = 1.0f - std::exp (-1.0f / (0.030f * (float) sr));   // 30 ms release
        reset();
    }

    void reset() noexcept
    {
        levEnv = 0.0f;
        clEnv  = 0.0f;
    }

    // Process one stereo sample in-place. Linked GR — the louder channel
    // controls the gain of both.
    void processSample (float& L, float& R) noexcept
    {
        if (! enabled) return;

        const float pk = std::max (std::fabs (L), std::fabs (R));

        // ---- Stage 1: program leveler --------------------------------------
        levEnv += (pk - levEnv) * (pk > levEnv ? levAtt : levRel);
        float gLev = 1.0f;
        if (levEnv > levThresh)
        {
            // 2:1 over-threshold compression, in linear domain so we
            // skip the dB conversion: ratio of 2:1 means each doubling
            // of overshoot becomes a doubling of root.
            const float over = levEnv / levThresh;     // > 1
            gLev = levThresh / (levThresh * std::sqrt (over));
            // Floor at -12 dB (≈ 0.25) so the leveler never crushes
            // below a quarter of original level even at insane feedback.
            gLev = std::max (gLev, levFloor);
        }

        L *= gLev;
        R *= gLev;

        // ---- Stage 2: ceiling catcher --------------------------------------
        const float pkC = std::max (std::fabs (L), std::fabs (R));
        clEnv += (pkC - clEnv) * (pkC > clEnv ? clAtt : clRel);
        if (clEnv > ceiling)
        {
            const float gC = ceiling / clEnv;
            L *= gC;
            R *= gC;
        }
    }

    void setEnabled (bool on) noexcept { enabled = on; }
    bool isEnabled() const noexcept    { return enabled; }

    // Live GR read-out for the UI in dB. Negative = reduction (e.g. -3 dB).
    // Combined leveler + ceiling reduction.
    float getGainReductionDb() const noexcept
    {
        if (! enabled) return 0.0f;
        // Reconstructible from the envelopes alone — same maths as in
        // processSample, just rolled out so the UI can poll cheaply.
        float gLev = 1.0f;
        if (levEnv > levThresh)
        {
            const float over = levEnv / levThresh;
            gLev = std::max (levFloor, 1.0f / std::sqrt (over));
        }
        const float ceilGr = clEnv > ceiling ? (ceiling / clEnv) : 1.0f;
        const float g = gLev * ceilGr;
        return g > 1.0e-4f ? 20.0f * std::log10 (g) : -40.0f;
    }

private:
    static constexpr float levThresh = 0.5f;   // -6 dBFS, leveler threshold
    static constexpr float levFloor  = 0.25f;  // -12 dB, max GR floor
    static constexpr float ceiling   = 0.97f;  // -0.26 dBFS, ceiling

    double sampleRate = 44100.0;
    float  levAtt = 0.0f, levRel = 0.0f;
    float  clAtt  = 0.0f, clRel  = 0.0f;
    float  levEnv = 0.0f, clEnv  = 0.0f;
    bool   enabled = true;
};
} // namespace doobie
