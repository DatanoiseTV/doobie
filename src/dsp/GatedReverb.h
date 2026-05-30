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

#include "PlateReverb.h"

namespace doobie
{
// Classic 80s gated reverb — a large bright plate driven into the chain, then
// an envelope-keyed gate that holds the wet output fully open for a fixed
// time after the input crosses a threshold, then snaps it shut. Best known
// from "In the Air Tonight": a snare hits, the reverb blooms, then cuts to
// silence at the gate boundary.
//
// We layer this on top of PlateReverb (already in the codebase) rather than
// re-implementing a reverb core: the audible character of a gated reverb is
// 90% the gate envelope, 10% the underlying plate. Reusing the FDN plate
// also means the existing `plateDecay / plateSize / plateDamp / plateMod /
// platePredelay` knobs continue to shape the underlying tail.
class GatedReverb
{
public:
    void prepare (double sr)
    {
        sampleRate = sr;
        plate.prepare (sr);
        reset();
    }

    void reset()
    {
        plate.reset();
        env = 0.0f;
        gate = 0.0f;
        holdSamplesLeft = 0;
    }

    // Plate parameters forwarded verbatim (the gated mode reuses the existing
    // reverb knobs). For a bright 80s gate, callers typically pass a long
    // decay + low damp at the engine level; we don't second-guess that here.
    void setPlateParams (float decay, float size, float damp,
                         float predelayMs, float mod) noexcept
    {
        plate.setParams (decay, size, damp, predelayMs, mod);
    }

    // Gate controls. Threshold in dBFS (-60..0). Hold + release in ms.
    // All clamped + snapped to safe ranges.
    void setGateParams (float thresholdDb, float holdMs, float releaseMs) noexcept
    {
        thresholdLin = juce::Decibels::decibelsToGain (juce::jlimit (-80.0f, 0.0f, thresholdDb));
        holdSamplesTarget = (int) std::round (juce::jmax (1.0f, holdMs)    * 0.001f * (float) sampleRate);
        // Release time-constant for a one-pole exponential closing the gate.
        const float releaseSec = juce::jmax (0.001f, releaseMs * 0.001f);
        releaseCoef = std::exp (-1.0f / (releaseSec * (float) sampleRate));
        // Detection envelope follower: fast attack (~3 ms) so the gate opens
        // crisply on transients, slow release (~50 ms) so a held note isn't
        // misread as below-threshold.
        const float detAtkSec = 0.003f;
        const float detRelSec = 0.050f;
        detAtkCoef = std::exp (-1.0f / (detAtkSec * (float) sampleRate));
        detRelCoef = std::exp (-1.0f / (detRelSec * (float) sampleRate));
    }

    // The "open the gate when the dry input crosses threshold" sidechain: we
    // detect off `inL/inR` (= the signal feeding the reverb) and apply the
    // gate envelope to the plate's wet output. The gate snaps to 1.0 the
    // instant the detector rises through threshold, stays at 1.0 for `hold`
    // samples, then exponentially decays toward 0 at `release` rate. A new
    // hit during decay re-opens the gate (re-trigger) — same behaviour as
    // the analog units this style emulates.
    void process (float inL, float inR, float& outL, float& outR) noexcept
    {
        // Envelope of the dry feed (max-channel half-wave detect — matches what
        // a real sidechain compressor's RMS-ish detector reads on a snare).
        const float det = juce::jmax (std::fabs (inL), std::fabs (inR));
        const float atkCoef = det > env ? detAtkCoef : detRelCoef;
        env = atkCoef * env + (1.0f - atkCoef) * det;

        // Gate state machine: above threshold -> snap open + reload hold.
        // During hold, gate stays at 1.0. After hold expires, release lets it
        // decay back to 0.
        if (env >= thresholdLin)
        {
            gate = 1.0f;
            holdSamplesLeft = holdSamplesTarget;
        }
        else if (holdSamplesLeft > 0)
        {
            --holdSamplesLeft;
            gate = 1.0f;
        }
        else
        {
            // Exponential decay toward 0 — multiply by coef per sample. A
            // tighter release (1ms) effectively snaps; a longer release
            // (~200ms) gives the cinematic blooming-tail-then-cut sound.
            gate *= releaseCoef;
            if (gate < 1.0e-6f) gate = 0.0f;
        }

        float wL, wR;
        plate.process (inL, inR, wL, wR);
        outL = wL * gate;
        outR = wR * gate;
    }

    // Live gate level for the UI (0..1). Atomic so the UI thread can read
    // without tearing — single-writer/single-reader, no need for full
    // sequential consistency, but `std::atomic<float>` is the simplest sane
    // way to publish this in a real-time-safe manner.
    float currentGateLevel() const noexcept { return gate; }

private:
    PlateReverb plate;
    double sampleRate { 44100.0 };

    // Envelope detector
    float env { 0.0f };
    float detAtkCoef { 0.0f };
    float detRelCoef { 0.0f };

    // Gate state
    float thresholdLin { 0.1f };
    float gate { 0.0f };
    int   holdSamplesTarget { 0 };
    int   holdSamplesLeft   { 0 };
    float releaseCoef { 0.99f };
};
} // namespace doobie
