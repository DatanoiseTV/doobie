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

#include "DubDelayEngine.h"
#include <cmath>
#include <algorithm>

namespace doobie
{
void DubDelayEngine::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;

    tapeL.prepare (sr, maxDelaySeconds);
    tapeR.prepare (sr, maxDelaySeconds);

    wowFlutter.prepare (sr);
    satL.reset();
    satR.reset();

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (1, maxBlockSize), 2 };
    toneL.prepare (spec);
    toneR.prepare (spec);
    preToneL.prepare (spec);
    preToneR.prepare (spec);

    spring.prepare (sr);
    plate.prepare (sr);
    hall.prepare (sr, 40.0f, 115.0f);
    shimmer.prepare (sr);

    diffuseL.prepare (sr);
    diffuseR.prepare (sr);
    pitchL.prepare (sr);
    pitchR.prepare (sr);

    // A longer glide on the master time makes large jumps (a division change, a
    // big TIME sweep) ease in like a tape capstan instead of zipping.
    smoothedDelay.reset (sr, 0.45);
    smoothedFeedback.reset (sr, 0.03);
    smoothedMix.reset (sr, 0.02);
    smoothedOut.reset (sr, 0.02);
    smoothedInGain.reset (sr, 0.02);
    smoothedWidth.reset (sr, 0.05);

    for (int i = 0; i < 4; ++i)
    {
        smoothedHeadGain[(size_t) i].reset (sr, 0.03);  // ~30 ms: click-free on/off
        smoothedHeadPan[(size_t) i].reset  (sr, 0.03);
        smoothedHeadRatio[(size_t) i].reset (sr, 0.12); // capstan-style time glide
    }

    smoothedDelay.setCurrentAndTargetValue (params.delaySamples);
    smoothedFeedback.setCurrentAndTargetValue (params.feedback);
    smoothedMix.setCurrentAndTargetValue (params.mix);
    smoothedOut.setCurrentAndTargetValue (params.outGain);
    smoothedInGain.setCurrentAndTargetValue (params.inputGain);
    smoothedWidth.setCurrentAndTargetValue (params.width);

    // Gentle (5 Hz) in the feedback loop so dub sub-bass survives many passes
    // while DC still can't accumulate; the wet output is single-pass, so 8 Hz.
    dcFbL.prepare (sr, 5.0f);
    dcFbR.prepare (sr, 5.0f);
    dcOutL.prepare (sr);
    dcOutR.prepare (sr);

    tapeAge.prepare (sr);

    reset();
}

void DubDelayEngine::reset()
{
    tapeL.reset();
    tapeR.reset();
    satL.reset();
    satR.reset();
    toneL.reset();
    toneR.reset();
    preToneL.reset();
    preToneR.reset();
    spring.reset();
    plate.reset();
    hall.reset();
    shimmer.reset();
    diffuseL.reset();
    diffuseR.reset();
    pitchL.reset();
    pitchR.reset();
    tapeWarmL = tapeWarmR = tapeDarkL = tapeDarkR = 0.0f;
    bbdLpL = bbdLpR = bbdBpL = bbdBpR = 0.0f;
    wowFlutter.reset();
    duckEnv = 0.0f;

    dcFbL.reset();
    dcFbR.reset();
    dcOutL.reset();
    dcOutR.reset();
    tapeAge.reset();

    // Snap the per-head smoothers to their current targets so a reset doesn't
    // ramp from stale values.
    for (int i = 0; i < 4; ++i)
    {
        const float gain = params.headOn[(size_t) i] ? params.headLevel[(size_t) i] : 0.0f;
        smoothedHeadGain[(size_t) i].setCurrentAndTargetValue (gain);
        smoothedHeadPan[(size_t) i].setCurrentAndTargetValue (params.headPan[(size_t) i]);
        smoothedHeadRatio[(size_t) i].setCurrentAndTargetValue (
            std::clamp ((double) params.headRatio[(size_t) i], 0.05, 1.0));
    }

    for (auto& m : headMag) m.store (0.0f);
}

inline float DubDelayEngine::whiteNoise() noexcept
{
    rngState ^= rngState << 13;
    rngState ^= rngState >> 17;
    rngState ^= rngState << 5;
    return (float) (int32_t) rngState * (1.0f / 2147483648.0f);
}

void DubDelayEngine::applyReverb (float inL, float inR, float& outL, float& outR) noexcept
{
    switch (params.reverbMode)
    {
        case 1: spring.process (inL, inR, outL, outR); break;
        case 2: plate.process  (inL, inR, outL, outR); break;
        case 3: // series: spring feeds plate
        {
            float sL, sR;
            spring.process (inL, inR, sL, sR);
            plate.process  (sL, sR, outL, outR);
            break;
        }
        case 4: // parallel: both from the same input, summed
        {
            float sL, sR, pL, pR;
            spring.process (inL, inR, sL, sR);
            plate.process  (inL, inR, pL, pR);
            outL = 0.5f * (sL + pL);
            outR = 0.5f * (sR + pR);
            break;
        }
        case 5: hall.process    (inL, inR, outL, outR); break;
        case 6: shimmer.process (inL, inR, outL, outR); break;
        default: outL = inL; outR = inR; break;
    }
}

void DubDelayEngine::process (juce::AudioBuffer<float>& buffer)
{
    const int numCh      = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    if (numCh == 0)
        return;

    const bool stereo = numCh > 1;
    auto* L = buffer.getWritePointer (0);
    auto* R = stereo ? buffer.getWritePointer (1) : L;

    // ---- Per-block control updates -----------------------------------------
    const float maxSamp = (float) tapeL.maxSamples() - 4.0f;
    smoothedDelay.setTargetValue (std::clamp (params.delaySamples, 2.0, (double) maxSamp));
    smoothedFeedback.setTargetValue (params.feedback);
    smoothedMix.setTargetValue (params.mix);
    smoothedOut.setTargetValue (params.outGain);
    smoothedInGain.setTargetValue (params.inputGain);
    smoothedWidth.setTargetValue (params.width);

    toneL.update (params.bass, params.treble, params.hpFreq, params.lpFreq);
    toneR.update (params.bass, params.treble, params.hpFreq, params.lpFreq);
    preToneL.update (params.preBass, params.preTreble, params.preHp, params.preLp);
    preToneR.update (params.preBass, params.preTreble, params.preHp, params.preLp);
    satL.setDrive (params.drive);
    satR.setDrive (params.drive);

    // AGE feeds extra transport instability into wow/flutter on top of their
    // own knobs, so an old tape wobbles even when wow/flutter are low.
    tapeAge.setAmount (params.age);
    wowFlutter.setAmounts (std::clamp (params.wow     + tapeAge.wowBoost(),     0.0f, 1.5f),
                           std::clamp (params.flutter + tapeAge.flutterBoost(), 0.0f, 1.5f));

    spring.setParams (params.springDecay, params.springTone, params.plateMod);
    plate.setParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);
    hall.setParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);
    // In shimmer mode the MOD control sets the octave regeneration amount.
    shimmer.setParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);

    // Per-head targets. A head that is off (or at zero level) targets zero gain
    // and the smoother ramps it out instead of cutting it dead.
    for (int i = 0; i < 4; ++i)
    {
        const float gain = params.headOn[(size_t) i] ? params.headLevel[(size_t) i] : 0.0f;
        smoothedHeadGain[(size_t) i].setTargetValue (gain);
        smoothedHeadPan[(size_t) i].setTargetValue (params.headPan[(size_t) i]);
        smoothedHeadRatio[(size_t) i].setTargetValue (
            std::clamp ((double) params.headRatio[(size_t) i], 0.05, 1.0));
    }

    const bool  reverbOn  = params.reverbMode != 0;
    const float revMix    = params.reverbMix;
    const float hissLevel = tapeAge.hissLevel();

    // Per-character filter coefficients.
    const float tapeWarmCoef = 1.0f - std::exp (-6.2831853f * 180.0f  / (float) sampleRate); // head-bump band
    const float tapeDarkCoef = 1.0f - std::exp (-6.2831853f * 5500.0f / (float) sampleRate); // tape HF loss
    const float bbdF = 2.0f * std::sin (3.14159265f * 2500.0f / (float) sampleRate);          // SVF cutoff
    const float bbdQ = 0.85f;                                                                  // low value = resonant (0.85 -> mild lift, no shriek)

    // Ducking ballistics (fast attack, slow release).
    const float atk = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate));
    const float rel = 1.0f - std::exp (-1.0f / (0.200f * (float) sampleRate));

    std::array<float, 4> headPeak { 0.0f, 0.0f, 0.0f, 0.0f };

    for (int n = 0; n < numSamples; ++n)
    {
        const float wf  = wowFlutter.next();
        const float m   = (params.delayMode == 0) ? 1.0f : wf; // Digital: no wobble
        const double dly = smoothedDelay.getNextValue();
        const float fb  = smoothedFeedback.getNextValue();
        const float mix = smoothedMix.getNextValue();
        const float og  = smoothedOut.getNextValue();
        const float ig  = smoothedInGain.getNextValue();
        const float w   = smoothedWidth.getNextValue();

        const float dryL = L[n];
        const float dryR = stereo ? R[n] : L[n];

        // ---- Reads happen before the write so every tap sees prior state ----
        const float fbDelay = std::max (2.0f, (float) (dly * m));
        float fbReadL = tapeL.read (fbDelay);
        float fbReadR = tapeR.read (fbDelay);

        // Feedback tone, then the delay-character processing. Saturation is
        // applied for every analog character but bypassed for clean Digital.
        float fbL = toneL.process (fbReadL);
        float fbR = toneR.process (fbReadR);
        switch (params.delayMode)
        {
            case 0: // Digital: clean repeats, no saturation, full bandwidth
                break;

            case 2: // BBD: a touch of drive into a dark, mildly resonant low-pass
            {
                // The pre-multiplier and SVF resonance both add gain inside the
                // feedback loop. Earlier values (1.4 + Q=0.45) pushed the round
                // trip to ~3x, which made BBD self-oscillate around FB=0.3 while
                // the other characters needed FB~1.0. These are tuned so the BBD
                // threshold lands around FB~0.8 — still a hint hotter than Tape
                // (the character of an MN3005-era BBD), but no runaway at low FB.
                fbL = satL.process (fbL * 1.1f);
                fbR = satR.process (fbR * 1.1f);
                bbdLpL += bbdF * bbdBpL;
                bbdBpL += bbdF * (fbL - bbdLpL - bbdQ * bbdBpL);
                bbdLpR += bbdF * bbdBpR;
                bbdBpR += bbdF * (fbR - bbdLpR - bbdQ * bbdBpR);
                fbL = bbdLpL + whiteNoise() * 0.006f;
                fbR = bbdLpR + whiteNoise() * 0.006f;
                break;
            }

            case 3: // Diffuse: a long all-pass cascade smears each repeat
                fbL = diffuseL.process (satL.process (fbL));
                fbR = diffuseR.process (satR.process (fbR));
                break;

            case 4: // Pitch: an FFT shifter lifts every repeat an octave
                fbL = pitchL.process (satL.process (fbL));
                fbR = pitchR.process (satR.process (fbR));
                break;

            default: // Tape: saturation + low-mid head bump + high-frequency loss
                fbL = satL.process (fbL);
                fbR = satR.process (fbR);
                tapeWarmL += tapeWarmCoef * (fbL - tapeWarmL);
                tapeWarmR += tapeWarmCoef * (fbR - tapeWarmR);
                fbL += 0.45f * tapeWarmL;
                fbR += 0.45f * tapeWarmR;
                tapeDarkL += tapeDarkCoef * (fbL - tapeDarkL);
                tapeDarkR += tapeDarkCoef * (fbR - tapeDarkR);
                fbL = 0.6f * fbL + 0.4f * tapeDarkL;
                fbR = 0.6f * fbR + 0.4f * tapeDarkR;
                break;
        }

        // Tape wear: dropouts and progressive HF loss on the recirculating
        // signal (hiss is added at the tape write below). Bypassed at AGE 0.
        tapeAge.process (fbL, fbR);

        // Reverb sitting inside the feedback loop (washes build up over repeats).
        if (reverbOn && params.reverbRoute == 2)
        {
            float rL, rR;
            applyReverb (fbL, fbR, rL, rR);
            fbL = fbL * (1.0f - revMix) + rL * revMix;
            fbR = fbR * (1.0f - revMix) + rR * revMix;
        }

        // Block DC before it recirculates, so no character (Digital included) or
        // in-loop reverb can pump a growing offset around the feedback path.
        fbL = dcFbL.process (fbL);
        fbR = dcFbR.process (fbR);

        const float fbGain = params.freeze ? 1.0f : fb;
        float fbContribL = fbL * fbGain;
        float fbContribR = fbR * fbGain;
        if (params.pingPong)
            std::swap (fbContribL, fbContribR);

        // Input into the loop: gain, then the pre-delay filters shape only the
        // signal that gets echoed (the dry output stays untouched), then an
        // optional reverb before the delay.
        float inL = preToneL.process (dryL * ig);
        float inR = preToneR.process (dryR * ig);
        if (reverbOn && params.reverbRoute == 1)
        {
            float rL, rR;
            applyReverb (inL, inR, rL, rR);
            inL = inL * (1.0f - revMix) + rL * revMix;
            inR = inR * (1.0f - revMix) + rR * revMix;
        }
        if (params.freeze)
        {
            inL = 0.0f;
            inR = 0.0f;
        }

        const float noiseL = whiteNoise() * hissLevel;
        const float noiseR = whiteNoise() * hissLevel;

        tapeL.write (inL + fbContribL + noiseL);
        tapeR.write (inR + fbContribR + noiseR);

        // ---- Multi-head output taps ----------------------------------------
        float wetL = 0.0f, wetR = 0.0f;
        for (int i = 0; i < 4; ++i)
        {
            // Advance every head's smoothers each sample so a head ramps cleanly
            // back in when re-enabled; skip the read only once it is silent.
            const float  lvl   = smoothedHeadGain[(size_t) i].getNextValue();
            const float  p     = smoothedHeadPan[(size_t) i].getNextValue();
            const double ratio = smoothedHeadRatio[(size_t) i].getNextValue();
            if (lvl <= 1.0e-5f)
                continue;

            const float hd = std::max (2.0f, (float) (dly * ratio * m));
            const float tL = tapeL.read (hd);
            const float tR = tapeR.read (hd);

            const float balL = (p <= 0.0f) ? 1.0f : 1.0f - p;
            const float balR = (p >= 0.0f) ? 1.0f : 1.0f + p;

            wetL += tL * lvl * balL;
            wetR += tR * lvl * balR;

            headPeak[(size_t) i] = std::max (headPeak[(size_t) i], 0.5f * (std::abs (tL) + std::abs (tR)) * lvl);
        }

        // Reverb after the delay (the usual send-style dub reverb).
        if (reverbOn && params.reverbRoute == 0)
        {
            float rL, rR;
            applyReverb (wetL, wetR, rL, rR);
            wetL = wetL * (1.0f - revMix) + rL * revMix;
            wetR = wetR * (1.0f - revMix) + rR * revMix;
        }

        // Stereo width (mid/side) on the wet signal only.
        const float mid  = 0.5f * (wetL + wetR);
        const float side = 0.5f * (wetL - wetR) * w;
        wetL = mid + side;
        wetR = mid - side;

        // Wet ducking driven by the dry input level.
        const float dryAbs = std::max (std::abs (dryL), std::abs (dryR));
        duckEnv += (dryAbs > duckEnv ? atk : rel) * (dryAbs - duckEnv);
        const float duckGain = std::clamp (1.0f - params.duck * duckEnv * 2.0f, 0.0f, 1.0f);
        wetL *= duckGain;
        wetR *= duckGain;

        // Keep the wet path centred (reverb/character can introduce a small
        // offset); the dry signal is passed through untouched.
        wetL = dcOutL.process (wetL);
        wetR = dcOutR.process (wetR);

        // Dry/wet crossfade and output trim.
        const float outL = (dryL * (1.0f - mix) + wetL * mix) * og;
        const float outR = (dryR * (1.0f - mix) + wetR * mix) * og;

        L[n] = outL;
        if (stereo)
            R[n] = outR;
    }

    for (int i = 0; i < 4; ++i)
        headMag[(size_t) i].store (headPeak[(size_t) i], std::memory_order_relaxed);
}
} // namespace doobie
