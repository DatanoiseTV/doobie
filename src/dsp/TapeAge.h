/*
  Doobie — analog dub delay  (Keinedelay/DFM hardware port)
  Copyright (C) 2026 DatanoiseTV

  GPL-3.0-or-later, WITHOUT ANY WARRANTY. Retain attribution to DatanoiseTV.
*/

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace doobie
{
// ============================================================================
//  TapeAge — a coherent "worn, over-recorded tape" model on one macro (0..1).
//
//  Reimplemented from the original (which was essentially a hiss floor + a
//  one-pole). A real tape that has been recorded over many times degrades in
//  several distinct, correlated ways; AGE drives all of them so "more age"
//  reads as one physical thing rather than a stack of unrelated effects:
//
//   1. Generation-loss HF roll-off — repeated record/play passes erase the top
//      end. In an echo this `process()` runs on the recirculating feedback, so
//      the dullness compounds with every repeat, exactly like bouncing a track
//      down again and again.
//   2. Dropouts — oxide shedding. A slow level wobble underneath, plus random,
//      short, deeper dips that also momentarily smother the highs.
//   3. Asperity / modulation noise — the defining tape sound: noise that rides
//      the signal level (loud passages grit up, silence stays quiet), with a
//      band-limited spectrum (rolled-off lows, gently tamed highs) instead of
//      flat white hiss. A small constant bias floor sits underneath.
//   4. Headroom loss — worn tape compresses earlier; a gentle, slow,
//      program-dependent gain reduction.
//   5. Crackle — sparse impulsive clicks from physical damage, audible only at
//      high age (scales ~age^3).
//   6. Transport instability — extra wow/flutter, folded into the engine's
//      wow/flutter generator via wowBoost()/flutterBoost().
//
//  Everything is bounded to unity gain or less, so it is safe to run inside the
//  delay feedback loop, and AGE 0 is a true bypass.
//
//  NOTE: the coefficients below are tuned by ear on the desktop plugin; this
//  port mirrors them. They are the right knobs to adjust when auditioning.
// ============================================================================
class TapeAge
{
  public:
    void prepare(double sr) noexcept
    {
        sampleRate = sr;

        // Envelope follower (drives modulation noise + compression).
        envAtk = 1.0f - std::exp(-1.0f / (0.003f * (float)sr)); // ~3 ms
        envRel = 1.0f - std::exp(-1.0f / (0.080f * (float)sr)); // ~80 ms

        // Slow dropout wobble.
        holdSamples = std::max(1, (int)(sr * 0.12));
        dropSmooth  = 1.0f - std::exp(-6.2831853f * 4.0f / (float)sr);

        // Noise shaping one-poles (give the hiss a band-limited "tape" colour
        // rather than flat white): remove rumble, tame the very top.
        noiseHpCoef = std::exp(-6.2831853f * 120.0f / (float)sr);   // HP ~120 Hz
        noiseLpCoef = 1.0f - std::exp(-6.2831853f * 9000.0f / (float)sr); // LP ~9 kHz

        // Crackle "tick" tone: a one-pole low-pass (~3.5 kHz) so each grain is a
        // soft filtered transient with body, not a full-scale single-sample
        // spike (which would read as a digital click / buffer underrun).
        crkLpCoef = 1.0f - std::exp(-6.2831853f * 3500.0f / (float)sr);

        // Compression ballistics (slow, so it breathes rather than pumps).
        compSmooth = 1.0f - std::exp(-1.0f / (0.050f * (float)sr));

        setAmount(age);
        reset();
    }

    void reset() noexcept
    {
        env = 0.0f;
        dropMod = dropTarget = 0.0f;
        dropCounter = 0;
        evtActive = false;
        evtPos = evtLen = 0;
        evtCounter = (int)sampleRate; // first event ~1 s in
        evtGain = 1.0f;
        lpL = lpR = 0.0f;
        nHpL = nHpR = 0.0f;
        nLpL = nLpR = 0.0f;
        compGain = 1.0f;
        crkEnv = crkLpL = crkLpR = 0.0f;
        crkDecay = 0.0f;
        rng      = 0x9E3779B9u;
    }

    void setAmount(float a) noexcept
    {
        age = std::clamp(a, 0.0f, 1.0f);

        // HF-loss pole: ~9 kHz when fresh sliding down to ~2.8 kHz when worn.
        const float cutoff = 9000.0f - 6200.0f * age;
        hfCoef             = 1.0f - std::exp(-6.2831853f * cutoff / (float)sampleRate);
    }

    // Folded into the engine's wow/flutter generator (a tired transport adds
    // pitch instability on top of the user's wow/flutter settings).
    float wowBoost() const noexcept { return age * 0.12f; }
    float flutterBoost() const noexcept { return age * 0.22f; }

    // Constant bias-hiss floor the engine may write to tape so silence still
    // "runs"; kept small because process() supplies the characterful noise.
    float hissLevel() const noexcept { return age * age * 0.004f; }

    // Play a stereo feedback sample "through worn tape", in place.
    inline void process(float& l, float& r) noexcept
    {
        if(age <= 0.0f)
            return;

        // --- signal envelope (mono) -----------------------------------------
        const float rect = std::max(std::fabs(l), std::fabs(r));
        env += (rect > env ? envAtk : envRel) * (rect - env);

        // --- 1. slow dropout wobble -----------------------------------------
        if(--dropCounter <= 0)
        {
            dropTarget  = noise();
            dropCounter = holdSamples;
        }
        dropMod += dropSmooth * (dropTarget - dropMod);
        const float slowGain = 1.0f - age * 0.20f * (0.5f - 0.5f * dropMod);

        // --- 2. discrete dropout events (oxide shedding) --------------------
        if(!evtActive && --evtCounter <= 0)
        {
            evtActive = true;
            evtPos    = 0;
            // Length 20..120 ms; depth grows with age.
            evtLen   = 1 + (int)((0.02f + 0.10f * rand01()) * (float)sampleRate);
            evtDepth = age * (0.30f + 0.45f * rand01());
            // Mean time to the next event shortens with age (~2 s -> ~0.35 s).
            const float mean = (2.0f - 1.65f * age) * (float)sampleRate;
            evtCounter       = (int)(mean * (0.4f + 1.2f * rand01())) + 1;
        }
        float evtGainNow = 1.0f, evtHF = 0.0f;
        if(evtActive)
        {
            const float t   = (float)evtPos / (float)evtLen;
            const float dip = std::sin(3.14159265f * t); // 0 -> 1 -> 0
            evtGainNow      = 1.0f - evtDepth * dip;
            evtHF           = dip; // momentary extra HF loss during the dip
            if(++evtPos >= evtLen)
                evtActive = false;
        }

        // --- 3. generation-loss HF roll-off (compounds in feedback) ---------
        const float hf = std::clamp(hfCoef + 0.45f * evtHF * hfCoef, 0.0f, 1.0f);
        lpL += hf * (l - lpL);
        lpR += hf * (r - lpR);
        const float blend = std::clamp(age * 0.55f + 0.30f * evtHF, 0.0f, 1.0f);
        l = l * (1.0f - blend) + lpL * blend;
        r = r * (1.0f - blend) + lpR * blend;

        // --- 4. headroom loss / gentle program compression ------------------
        const float over       = std::max(0.0f, env - 0.25f);
        const float targetComp = 1.0f - std::clamp(age * 0.6f * over, 0.0f, 0.5f);
        compGain += compSmooth * (targetComp - compGain);

        const float g = slowGain * evtGainNow * compGain;
        l *= g;
        r *= g;

        // --- 5. asperity / modulation noise (band-limited, rides the level) -
        const float nAmt = age * (0.004f + 0.030f * env); // floor + signal grit
        l += shapedNoise(nHpL, nLpL) * nAmt;
        r += shapedNoise(nHpR, nLpR) * nAmt;

        // --- 6. crackle: sparse analog "ticks", only at high age ------------
        // A real worn-tape crackle is a short, band-limited, fast-decaying
        // transient with body — NOT a one-sample spike (that sounds like a
        // digital click / buffer underrun). Each event seeds a grain: filtered
        // noise through a few-millisecond exponential decay envelope. Random
        // (Poisson-ish) timing, so it never reads as periodic underrun glitches.
        const float crackleProb = age * age * age * 0.0009f;
        if(crkEnv < 0.001f && rand01() < crackleProb)
        {
            crkEnv             = (0.04f + 0.10f * rand01()) * age; // modest level
            const float tauMs  = 2.0f + 4.0f * rand01();          // 2..6 ms decay
            crkDecay           = std::exp(-1.0f / (tauMs * 0.001f * (float)sampleRate));
        }
        if(crkEnv > 0.001f)
        {
            crkLpL += crkLpCoef * (noise() - crkLpL);
            crkLpR += crkLpCoef * (noise() - crkLpR);
            l += crkLpL * crkEnv;
            r += crkLpR * crkEnv;
            crkEnv *= crkDecay;
        }
    }

  private:
    // White xorshift in [-1, 1).
    inline float noise() noexcept
    {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return (float)(int32_t)rng * (1.0f / 2147483648.0f);
    }

    // Band-limited noise: one-pole HP (remove rumble) then one-pole LP (tame the
    // top) so the hiss sits in a tape-like band instead of sounding like flat
    // white noise. Per-channel filter state passed in.
    inline float shapedNoise(float& hp, float& lp) noexcept
    {
        const float w  = noise();
        hp             = noiseHpCoef * (hp + w);
        const float h  = w - hp * (1.0f - noiseHpCoef);
        lp += noiseLpCoef * (h - lp);
        return lp;
    }

    inline float rand01() noexcept { return 0.5f * (noise() + 1.0f); }

    double sampleRate = 44100.0;
    float  age        = 0.0f;

    float envAtk = 0.01f, envRel = 0.001f, env = 0.0f;

    int   holdSamples = 1, dropCounter = 0;
    float dropSmooth = 0.001f, dropMod = 0.0f, dropTarget = 0.0f;

    bool  evtActive = false;
    int   evtPos = 0, evtLen = 0, evtCounter = 0;
    float evtDepth = 0.0f, evtGain = 1.0f;

    float hfCoef = 0.5f, lpL = 0.0f, lpR = 0.0f;

    float compSmooth = 0.01f, compGain = 1.0f;

    float noiseHpCoef = 0.99f, noiseLpCoef = 0.5f;
    float nHpL = 0.0f, nHpR = 0.0f, nLpL = 0.0f, nLpR = 0.0f;

    // crackle grain (band-limited decaying tick)
    float crkLpCoef = 0.3f, crkEnv = 0.0f, crkLpL = 0.0f, crkLpR = 0.0f, crkDecay = 0.0f;

    uint32_t rng = 0x9E3779B9u;
};
} // namespace doobie
