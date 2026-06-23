/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  GPL-3.0-or-later, WITHOUT ANY WARRANTY. Retain attribution to DatanoiseTV.
*/

#pragma once

#include <cmath>
#include <algorithm>
#include "Svf.h"

namespace doobie
{
// Three-band ducker. The dry input drives one envelope follower per band;
// the wet signal is split into the same three bands, each scaled by
// (1 - amount * env) for its own band, then summed back. Result: a kick
// (low band hot) ducks the wet's low band only and leaves the high band
// alone, so reverb / delay HF trails keep ringing under it. The two
// crossover frequencies (low/mid split, mid/high split) are user-tunable.
//
// Crossovers: two cascaded SVF-LP / SVF-HP at each split frequency. This is
// not phase-coherent Linkwitz-Riley (that needs matched 2nd-order pairs and
// a specific summation), but for ducking purposes — where we're shaping
// envelopes, not summing dry+wet — phase coherence isn't load-bearing. The
// summed wet rebuild stays close enough to flat that a 100 % bypass (amount
// = 0) is inaudible from the un-ducked path.
//
// Ballistics match the previous broadband ducker (5 ms attack / 200 ms
// release per band) so dialling `duck` from 0 -> X feels identical to the
// pre-multiband behaviour when both crossovers are extreme.
class MultiBandDucker
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        // Eight SVFs total: 4 dry (LP @ lowSplit + LP @ highSplit, stereo)
        // and 4 wet of the same shape. The high band is `input - LP@highSplit`,
        // the low band is `LP@lowSplit`, the mid band is the difference of
        // the two LPs — so we don't need a separate filter pair for "mid".
        for (auto* f : { &dryLowL, &dryLowR, &dryHighL, &dryHighR,
                         &wetLowL, &wetLowR, &wetHighL, &wetHighR })
            f->prepare (sr);

        recomputeBallistics();
        reset();
    }

    void reset() noexcept
    {
        for (auto* f : { &dryLowL, &dryLowR, &dryHighL, &dryHighR,
                         &wetLowL, &wetLowR, &wetHighL, &wetHighR })
            f->reset();
        envLow = envMid = envHigh = 0.0f;
    }

    // Set the two crossover frequencies. Caller clamps to whatever the
    // user-facing param range is; we re-clamp defensively here.
    void setCrossovers (float lowSplitHz, float highSplitHz) noexcept
    {
        lowSplit  = std::clamp (lowSplitHz,  20.0f, 1000.0f);
        highSplit = std::clamp (highSplitHz, lowSplit + 50.0f, 18000.0f);
    }

    // Process one stereo sample in place. `amount` = master duck depth
    // (0..1; matches the pre-existing `duck` knob semantics). dry is the
    // sidechain (the plugin's raw input); wet is what gets ducked
    // (delay + reverb post-mix).
    inline void processSample (float dryL, float dryR,
                               float& wetL, float& wetR,
                               float amount) noexcept
    {
        // --- Split DRY into three bands per channel ---------------------
        // We compute coefs only on the L side and copy to R (one tan() pair
        // per split rather than two). The cascade-of-two trick: an LP at
        // `lowSplit` gives "low + mid", subtracting it from the input gives
        // "high". Then within "low + mid", another LP at `lowSplit` would
        // be redundant — so for the "low" band we use the LP output of the
        // FIRST LP, and for "mid" we use a BP-ish region formed by
        // (LP at highSplit) - (LP at lowSplit). For "high", input minus LP
        // at highSplit. This avoids cascading and keeps sum-back close to
        // unity at the bypass extreme.
        dryLowL.setParams  (lowSplit,  0.0f); dryLowR.copyCoefsFrom (dryLowL);
        dryHighL.setParams (highSplit, 0.0f); dryHighR.copyCoefsFrom (dryHighL);

        const float dryLpLowL  = dryLowL.process  (dryL, 0); // 0 = LP
        const float dryLpLowR  = dryLowR.process  (dryR, 0);
        const float dryLpHighL = dryHighL.process (dryL, 0);
        const float dryLpHighR = dryHighR.process (dryR, 0);

        const float dryLoL  = dryLpLowL;
        const float dryLoR  = dryLpLowR;
        const float dryMidL = dryLpHighL - dryLpLowL;
        const float dryMidR = dryLpHighR - dryLpLowR;
        const float dryHiL  = dryL - dryLpHighL;
        const float dryHiR  = dryR - dryLpHighR;

        // --- Per-band envelope follower (max-abs of L/R) ----------------
        const float lAbs = std::max (std::fabs (dryLoL),  std::fabs (dryLoR));
        const float mAbs = std::max (std::fabs (dryMidL), std::fabs (dryMidR));
        const float hAbs = std::max (std::fabs (dryHiL),  std::fabs (dryHiR));

        envLow  += (lAbs > envLow  ? atk : rel) * (lAbs - envLow);
        envMid  += (mAbs > envMid  ? atk : rel) * (mAbs - envMid);
        envHigh += (hAbs > envHigh ? atk : rel) * (hAbs - envHigh);

        const float gainLow  = std::clamp (1.0f - amount * envLow  * 2.0f, 0.0f, 1.0f);
        const float gainMid  = std::clamp (1.0f - amount * envMid  * 2.0f, 0.0f, 1.0f);
        const float gainHigh = std::clamp (1.0f - amount * envHigh * 2.0f, 0.0f, 1.0f);

        // --- Split WET into the same bands and apply per-band gains -----
        wetLowL.setParams  (lowSplit,  0.0f); wetLowR.copyCoefsFrom (wetLowL);
        wetHighL.setParams (highSplit, 0.0f); wetHighR.copyCoefsFrom (wetHighL);

        const float wetLpLowL  = wetLowL.process  (wetL, 0);
        const float wetLpLowR  = wetLowR.process  (wetR, 0);
        const float wetLpHighL = wetHighL.process (wetL, 0);
        const float wetLpHighR = wetHighR.process (wetR, 0);

        const float wetLoL  = wetLpLowL;
        const float wetLoR  = wetLpLowR;
        const float wetMidL = wetLpHighL - wetLpLowL;
        const float wetMidR = wetLpHighR - wetLpLowR;
        const float wetHiL  = wetL - wetLpHighL;
        const float wetHiR  = wetR - wetLpHighR;

        wetL = wetLoL * gainLow + wetMidL * gainMid + wetHiL * gainHigh;
        wetR = wetLoR * gainLow + wetMidR * gainMid + wetHiR * gainHigh;
    }

private:
    void recomputeBallistics() noexcept
    {
        atk = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate));
        rel = 1.0f - std::exp (-1.0f / (0.200f * (float) sampleRate));
    }

    double sampleRate = 48000.0;
    float  atk = 0.5f, rel = 0.01f;
    float  envLow = 0.0f, envMid = 0.0f, envHigh = 0.0f;
    float  lowSplit  = 250.0f;
    float  highSplit = 2500.0f;

    // Per-channel SVFs for dry sidechain split (LP @ lowSplit for the L/M
    // boundary; LP @ highSplit for the M/H boundary).
    Svf dryLowL, dryLowR, dryHighL, dryHighR;
    // Same shape for wet split.
    Svf wetLowL, wetLowR, wetHighL, wetHighR;
};
} // namespace doobie
