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
    conv.prepare (sr, maxBlockSize);
    gated.prepare (sr);
    inFilterL.prepare (sr); inFilterR.prepare (sr);
    phaserL.prepare (sr);   phaserR.prepare (sr);
    // Stereo phaser: right channel sweeps 1/4 cycle out of phase so the
    // notch motion paints across the image instead of summing to mono.
    phaserL.setPhaseOffset (0.0f); phaserR.setPhaseOffset (0.25f);

    // If a factory IR was loaded at a previous sample rate, regenerate it at
    // the new one — preferable to letting JUCE resample a stale buffer. A
    // user-loaded file IR is left alone; JUCE handles its resampling from the
    // file's own sample rate.
    if (conv.getSource() == ConvolutionReverb::Source::Factory)
    {
        const int idx = conv.getFactoryIndex();
        if (idx >= 0)
            (void) loadFactoryIR (idx);
    }

    diffuseL.prepare (sr);
    diffuseR.prepare (sr);
    pitchL.prepare (sr);
    pitchR.prepare (sr);
    granPitchL.prepare (sr);
    granPitchR.prepare (sr);

    // A longer glide on the master time makes large jumps (a division change, a
    // big TIME sweep) ease in like a tape capstan instead of zipping.
    smoothedDelay.reset (sr, 0.45);
    smoothedFeedback.reset (sr, 0.03);
    smoothedMix.reset (sr, 0.02);
    smoothedOut.reset (sr, 0.02);
    smoothedInGain.reset (sr, 0.02);
    smoothedWidth.reset (sr, 0.05);
    smoothedRevMix.reset (sr, 0.02);   // reverb wet/dry knob — click-prone
    smoothedDrive.reset  (sr, 0.03);   // saturation drive — tanh curve step
    smoothedAge.reset    (sr, 0.05);   // AGE macro fans out to several places

    for (int i = 0; i < 4; ++i)
    {
        smoothedHeadGain[(size_t) i].reset (sr, 0.03);  // ~30 ms: click-free on/off
        smoothedHeadPan[(size_t) i].reset  (sr, 0.03);
        smoothedHeadRatio[(size_t) i].reset (sr, 0.12); // capstan-style time glide
        smoothedHeadOffset[(size_t) i].reset (sr, 0.05); // 50 ms — kills crackle
    }

    smoothedDelay.setCurrentAndTargetValue (params.delaySamples);
    smoothedFeedback.setCurrentAndTargetValue (params.feedback);
    smoothedMix.setCurrentAndTargetValue (params.mix);
    smoothedOut.setCurrentAndTargetValue (params.outGain);
    smoothedInGain.setCurrentAndTargetValue (params.inputGain);
    smoothedWidth.setCurrentAndTargetValue (params.width);
    smoothedRevMix.setCurrentAndTargetValue (params.reverbMix);
    smoothedDrive.setCurrentAndTargetValue  (params.drive);
    smoothedAge.setCurrentAndTargetValue    (params.age);

    // Gentle (5 Hz) in the feedback loop so dub sub-bass survives many passes
    // while DC still can't accumulate; the wet output is single-pass, so 8 Hz.
    dcFbL.prepare (sr, 5.0f);
    dcFbR.prepare (sr, 5.0f);
    dcOutL.prepare (sr);
    dcOutR.prepare (sr);

    tapeAge.prepare (sr);
    outLeveler.prepare (sr);

    // Feedback-limiter time constants (2 ms attack / 250 ms release) — held
    // here so we don't recompute them per sample. Matches the hardware port.
    fbLimAttCoeff = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
    fbLimRelCoeff = 1.0f - std::exp (-1.0f / (0.250f * (float) sr));
    reloadCoeff   = 1.0f - std::exp (-1.0f / (0.012f * (float) sr));
    killCoeff     = 1.0f - std::exp (-1.0f / (0.008f * (float) sr));  // ~8 ms

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
    conv.reset();
    gated.reset();
    inFilterL.reset(); inFilterR.reset();
    phaserL.reset();   phaserR.reset();
    diffuseL.reset();
    diffuseR.reset();
    pitchL.reset();
    pitchR.reset();
    granPitchL.reset();
    granPitchR.reset();
    tapeWarmL = tapeWarmR = tapeDarkL = tapeDarkR = 0.0f;
    bbdLpL = bbdLpR = bbdBpL = bbdBpR = 0.0f;
    fbLimEnv = 0.0f;
    outLeveler.reset();
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
        smoothedHeadOffset[(size_t) i].setCurrentAndTargetValue (params.headOffsetMs[(size_t) i]);
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
        case 7: conv.process    (inL, inR, outL, outR); break; // user-loaded IR
        case 8: gated.process   (inL, inR, outL, outR); break; // classic 80s gated reverb
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
    smoothedRevMix.setTargetValue (params.reverbMix);
    smoothedDrive.setTargetValue  (params.drive);
    smoothedAge.setTargetValue    (params.age);

    toneL.update (params.bass, params.treble, params.hpFreq, params.lpFreq);
    toneR.update (params.bass, params.treble, params.hpFreq, params.lpFreq);
    preToneL.update (params.preBass, params.preTreble, params.preHp, params.preLp);
    preToneR.update (params.preBass, params.preTreble, params.preHp, params.preLp);
    // satL/R drive and tapeAge amount are pulled from their smoothers inside
    // the per-sample loop so knob twiddles ramp instead of stepping. The
    // wowFlutter targets are set per block (its own internal smoothing is
    // slow enough that block-rate updates are imperceptible).
    tapeAge.setAmount (params.age);  // initialise for wowBoost computation below
    wowFlutter.setAmounts (std::clamp (params.wow     + tapeAge.wowBoost(),     0.0f, 1.5f),
                           std::clamp (params.flutter + tapeAge.flutterBoost(), 0.0f, 1.5f));

    spring.setParams (params.springDecay, params.springTone, params.plateMod);
    plate.setParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);
    hall.setParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);
    // In shimmer mode the MOD control sets the octave regeneration amount;
    // the interval (in semitones) is a separate, user-selectable param.
    shimmer.setParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);
    shimmer.setIntervalSemitones (params.shimmerSemis);

    // Delay's Pitch character uses the same shifter type but a separate
    // interval — and runs on the recirculating path, so each repeat shifts
    // by `pitchSemis`, compounding into the climbing-octave classic. The
    // stereo SPREAD detunes the two channels symmetrically (cents → semis)
    // so unison + spread = chorus, harmony + spread = wide harmony. At
    // spread == 0 both shifters get exactly the same interval, so the
    // feature is a true no-op (sub-cent deadband to guarantee that even
    // when a tiny float epsilon sneaks in from the smoother).
    const float spreadSemis = params.pitchSpread < 0.5f ? 0.0f : (params.pitchSpread * 0.01f);
    pitchL.setIntervalSemitones (params.pitchSemis - spreadSemis);
    pitchR.setIntervalSemitones (params.pitchSemis + spreadSemis);
    granPitchL.setIntervalSemitones (params.pitchSemis - spreadSemis);
    granPitchR.setIntervalSemitones (params.pitchSemis + spreadSemis);
    gated.setPlateParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);
    gated.setGateParams (params.gateThresholdDb, params.gateHoldMs, params.gateReleaseMs);
    // IR makeup gain (per-sample smoothed inside the wrapper).
    conv.setGain (params.irGain);

    // Per-head targets. A head that is off (or at zero level) targets zero gain
    // and the smoother ramps it out instead of cutting it dead.
    for (int i = 0; i < 4; ++i)
    {
        const float gain = params.headOn[(size_t) i] ? params.headLevel[(size_t) i] : 0.0f;
        smoothedHeadGain[(size_t) i].setTargetValue (gain);
        smoothedHeadPan[(size_t) i].setTargetValue (params.headPan[(size_t) i]);
        smoothedHeadRatio[(size_t) i].setTargetValue (
            std::clamp ((double) params.headRatio[(size_t) i], 0.05, 1.0));
        smoothedHeadOffset[(size_t) i].setTargetValue (params.headOffsetMs[(size_t) i]);
    }

    const bool  reverbOn  = params.reverbMode != 0;

    // Ported from hardware: phaser block params (per-sample is overkill for a
    // sweep this slow, per-block is zipper-free in practice). Right channel
    // already has a 1/4-cycle phase offset from prepare().
    if (params.phaserOn)
    {
        phaserL.setParams (params.phaserRate, params.phaserDepth, params.phaserFb, params.phaserMix);
        phaserR.setParams (params.phaserRate, params.phaserDepth, params.phaserFb, params.phaserMix);
    }
    // revMix and hissLevel are now pulled per-sample (the smoothed reverb mix
    // and the AGE-driven hiss level) so they ramp on knob changes.

    // Per-character filter coefficients.
    const float tapeWarmCoef = 1.0f - std::exp (-6.2831853f * 180.0f  / (float) sampleRate); // head-bump band
    const float tapeDarkCoef = 1.0f - std::exp (-6.2831853f * 5500.0f / (float) sampleRate); // tape HF loss
    const float bbdF = 2.0f * std::sin (3.14159265f * 2500.0f / (float) sampleRate);          // SVF cutoff
    const float bbdQ = 0.85f;                                                                  // low value = resonant (0.85 -> mild lift, no shriek)

    // Ducking ballistics (fast attack, slow release).
    const float atk = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate));
    const float rel = 1.0f - std::exp (-1.0f / (0.200f * (float) sampleRate));

    std::array<float, 4> headPeak { 0.0f, 0.0f, 0.0f, 0.0f };

    // The character chain (saturation + per-mode tone shaping) is shared
    // between the feedback path and the delay-bypassed path, so the user gets
    // the same tape/BBD/diffuse/pitch flavour whichever they choose. Captures
    // engine state by reference; block-scope coefficients by value.
    auto applyCharacter = [this, tapeWarmCoef, tapeDarkCoef, bbdF, bbdQ] (float& l, float& r) noexcept
    {
        switch (params.delayMode)
        {
            case 0: // Digital: clean
                break;

            case 2: // BBD: drive into a dark, mildly resonant low-pass
            {
                // The pre-multiplier and SVF resonance both add gain. Tuned
                // so the BBD self-oscillation threshold lands around FB~0.8 —
                // still a hint hotter than Tape (an MN3005-era BBD's hot
                // character) but no runaway at moderate feedback.
                l = satL.process (l * 1.1f);
                r = satR.process (r * 1.1f);
                bbdLpL += bbdF * bbdBpL;
                bbdBpL += bbdF * (l - bbdLpL - bbdQ * bbdBpL);
                bbdLpR += bbdF * bbdBpR;
                bbdBpR += bbdF * (r - bbdLpR - bbdQ * bbdBpR);
                // BBD SVF non-finite latch guard (ported from hardware fix
                // cc5e2a4). With aggressive feedback + a wow/flutter spike,
                // the SVF state can occasionally rail to a non-finite value;
                // once latched it recirculates through the feedback path
                // forever (silence + a constant DC offset / NaN cascade).
                // Cheap one-time check per sample; bypassed when sane.
                if (! std::isfinite (bbdLpL) || ! std::isfinite (bbdBpL))
                    bbdLpL = bbdBpL = 0.0f;
                if (! std::isfinite (bbdLpR) || ! std::isfinite (bbdBpR))
                    bbdLpR = bbdBpR = 0.0f;
                // BBD clock noise is now AGE-scaled (was a fixed -44 dBFS that
                // was audible even at AGE=0 and made BBD feel "hissy by
                // default"). 0.0005 floor keeps a hint of MN3005-era clock
                // residue at AGE=0; the rest fades in as AGE rises so the
                // user can dial the noise level via the existing AGE knob.
                const float bbdNoise = 0.0005f + 0.003f * tapeAge.hissLevel();
                l = bbdLpL + whiteNoise() * bbdNoise;
                r = bbdLpR + whiteNoise() * bbdNoise;
                break;
            }

            case 3: // Diffuse: an all-pass cascade smears each repeat
                l = diffuseL.process (satL.process (l));
                r = diffuseR.process (satR.process (r));
                break;

            case 4: // Pitch character — shifter selected by pitchAlgo
                // pitchOn=false bypasses the shifter entirely.
                // pitchRoute=Pre runs the shift on the INPUT (above) so the
                // feedback loop just saturates the already-pitched signal —
                // skip the shifter here in that case.
                if (params.pitchOn && params.pitchRoute == 0)
                {
                    if (params.pitchAlgo == 1)
                    {
                        l = granPitchL.process (satL.process (l));
                        r = granPitchR.process (satR.process (r));
                    }
                    else
                    {
                        l = pitchL.process (satL.process (l));
                        r = pitchR.process (satR.process (r));
                    }
                }
                else
                {
                    l = satL.process (l);
                    r = satR.process (r);
                }
                break;

            default: // Tape: saturation + low-mid head bump + HF loss
                l = satL.process (l);
                r = satR.process (r);
                tapeWarmL += tapeWarmCoef * (l - tapeWarmL);
                tapeWarmR += tapeWarmCoef * (r - tapeWarmR);
                l += 0.45f * tapeWarmL;
                r += 0.45f * tapeWarmR;
                tapeDarkL += tapeDarkCoef * (l - tapeDarkL);
                tapeDarkR += tapeDarkCoef * (r - tapeDarkR);
                l = 0.6f * l + 0.4f * tapeDarkL;
                r = 0.6f * r + 0.4f * tapeDarkR;
                break;
        }
    };

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

        // Pull the per-sample smoothed values for the click-prone params and
        // push them into their consumers, so knob changes ramp instead of
        // stepping. setDrive / setAmount are cheap (a handful of float ops).
        const float driveNow = smoothedDrive.getNextValue();
        satL.setDrive (driveNow);
        satR.setDrive (driveNow);
        tapeAge.setAmount (smoothedAge.getNextValue());
        const float revMix    = smoothedRevMix.getNextValue();
        const float hissLevel = tapeAge.hissLevel();

        // Pre-delay tone shaping applies in both modes: it shapes the input
        // going into the delay, or directly into the character chain when the
        // delay is bypassed.
        float inL = preToneL.process (dryL * ig);
        float inR = preToneR.process (dryR * ig);

        // Input multimode filter (TPT-SVF, ported from hardware). Per-sample
        // setParams tracks knob/mod sweeps zipper-free. One tan() shared
        // between L+R via copyCoefsFrom; OFF = true bypass.
        if (params.inFilterOn)
        {
            inFilterL.setParams (params.inFilterCutoff, params.inFilterRes);
            inFilterR.copyCoefsFrom (inFilterL);
            inL = inFilterL.process (inL, params.inFilterType);
            inR = inFilterR.process (inR, params.inFilterType);
        }

        // Pre-route pitch shift: when delayMode == Pitch and pitchRoute == Pre,
        // Pitch shifter on the INPUT signal. Engages in two cases:
        //   1. pitchOn + route == Pre (regardless of delayMode) — the
        //      stand-alone pitch effect, works even with delay bypassed.
        //   2. pitchOn + delayMode != Pitch (any route) — when the delay
        //      character isn't Pitch the in-feedback path is silent for
        //      the shifter, so we fall back to input processing so the
        //      Pitch Shifter card always does something audible while
        //      pitchOn is true.
        // route=Feedback combined with delayMode=Pitch keeps its in-loop
        // behaviour (case 4 below), so the shifter stays inside the
        // feedback path for climbing-octave effects.
        const bool pitchOnInput = params.pitchOn
                                  && (params.pitchRoute == 1 || params.delayMode != 4);
        if (pitchOnInput)
        {
            if (params.pitchAlgo == 1)
            {
                inL = granPitchL.process (inL);
                inR = granPitchR.process (inR);
            }
            else
            {
                inL = pitchL.process (inL);
                inR = pitchR.process (inR);
            }
        }

        float wetL = 0.0f, wetR = 0.0f;

        // Pitch-shifter latency compensation. Both the FFT and granular
        // shifters introduce inherent algorithm latency (half-window). When
        // Pitch character is active and the shifter ON, subtract that
        // many samples from every tape-read distance — the recirculating
        // signal then passes through the shifter (adding latency back),
        // and the round trip lands at exactly the knob-set delay time per
        // repeat. Without this, each repeat compounds the latency and the
        // wet drifts late vs the dry by hundreds of ms after a few echoes.
        float pitchComp = 0.0f;
        // Compensate whenever the shifter is engaged anywhere in the
        // chain — either inside the character loop (delayMode == Pitch
        // + route Feedback) or on the input via the stand-alone path.
        const bool pitchAnyPath = params.pitchOn
                                  && (params.delayMode == 4 || params.pitchRoute == 1
                                      || params.delayMode != 4);
        if (pitchAnyPath)
            pitchComp = (float) (params.pitchAlgo == 1 ? granPitchL.getLatencySamples()
                                                       : pitchL.getLatencySamples());

        if (! params.delayBypass)
        {
            // ---- Reads happen before the write so every tap sees prior state ----
            const float fbDelay = std::max (2.0f, (float) (dly * m) - pitchComp);
            float fbReadL = tapeL.read (fbDelay);
            float fbReadR = tapeR.read (fbDelay);

            float fbL = toneL.process (fbReadL);
            float fbR = toneR.process (fbReadR);
            applyCharacter (fbL, fbR);

            // Tape wear: dropouts and progressive HF loss on the recirculating
            // signal (hiss is added at the tape write below). Bypassed at AGE 0.
            tapeAge.process (fbL, fbR);

            // Phaser in feedback (route=2): every repeat deepens through the
            // same all-pass chain. Sits BEFORE the in-loop reverb so the wash
            // gets phased then reverbed, not the other way round.
            if (params.phaserOn && params.phaserRoute == 2)
            {
                fbL = phaserL.process (fbL);
                fbR = phaserR.process (fbR);
            }

            // Reverb sitting inside the feedback loop (washes build up over repeats).
            if (reverbOn && params.reverbRoute == 2)
            {
                float rL, rR;
                applyReverb (fbL, fbR, rL, rR);
                fbL = fbL * (1.0f - revMix) + rL * revMix;
                fbR = fbR * (1.0f - revMix) + rR * revMix;
            }

            // Block DC before it recirculates.
            fbL = dcFbL.process (fbL);
            fbR = dcFbR.process (fbR);

            // Momentary kill: ramp killGain toward 0 while held, toward 1
            // on release. ~8 ms slope kills the click that a hard mute
            // would otherwise put on the recirculating signal. The
            // existing tail (already written to tape) rings out naturally.
            const float killTarget = params.feedbackKill ? 0.0f : 1.0f;
            killGain += (killTarget - killGain) * killCoeff;

            const float fbGain = (params.freeze ? 1.0f : fb) * killGain;
            float fbContribL = fbL * fbGain;
            float fbContribR = fbR * fbGain;

            // Feedback limiter (linked stereo, ported from hardware): track
            // the recirculating peak and, once it crosses the threshold,
            // scale the re-injected signal so the loop plateaus there
            // instead of running away. The linear characters (BBD
            // especially) have no self-limiting; without this, fb>1 rails
            // the output soft-clip. Below threshold = unity gain so the
            // tone is unaffected for normal feedback amounts.
            {
                const float pk = std::max (std::fabs (fbContribL), std::fabs (fbContribR));
                fbLimEnv += (pk - fbLimEnv) * (pk > fbLimEnv ? fbLimAttCoeff : fbLimRelCoeff);
                if (fbLimEnv > kFbLimThresh)
                {
                    const float g = kFbLimThresh / fbLimEnv;
                    fbContribL *= g;
                    fbContribR *= g;
                }
            }

            if (params.pingPong)
                std::swap (fbContribL, fbContribR);

            // What gets written to tape: input + pre-route reverb + feedback + hiss.
            float wrL = inL, wrR = inR;
            // Phaser pre (route=1): into the delay input. Phased signal goes
            // to tape; dry passes through untouched. The "pre" reverb sits
            // after the phaser so the whole pre-stage stays before the wet.
            if (params.phaserOn && params.phaserRoute == 1)
            {
                wrL = phaserL.process (wrL);
                wrR = phaserR.process (wrR);
            }
            if (reverbOn && params.reverbRoute == 1)
            {
                float rL, rR;
                applyReverb (wrL, wrR, rL, rR);
                wrL = wrL * (1.0f - revMix) + rL * revMix;
                wrR = wrR * (1.0f - revMix) + rR * revMix;
            }
            if (params.freeze)
            {
                wrL = 0.0f;
                wrR = 0.0f;
            }

            const float noiseL = whiteNoise() * hissLevel;
            const float noiseR = whiteNoise() * hissLevel;

            tapeL.write (wrL + fbContribL + noiseL);
            tapeR.write (wrR + fbContribR + noiseR);

            // ---- Multi-head output taps ----------------------------------------
            for (int i = 0; i < 4; ++i)
            {
                // Advance every head's smoothers each sample so a head ramps cleanly
                // back in when re-enabled; skip the read only once it is silent.
                const float  lvl   = smoothedHeadGain[(size_t) i].getNextValue();
                const float  p     = smoothedHeadPan[(size_t) i].getNextValue();
                const double ratio = smoothedHeadRatio[(size_t) i].getNextValue();
                // Always pull the smoother — even when the head is silent —
                // so its phase stays in sync and a head re-enables without a
                // glitch.
                const float  offMs = smoothedHeadOffset[(size_t) i].getNextValue();
                if (lvl <= 1.0e-5f)
                    continue;

                // Ratio-driven base position + additive ms offset (signed,
                // ramp-smoothed over 50 ms so live knob moves don't crackle).
                // The offset is NOT modulated by wow/flutter — it's the
                // user's chosen alignment shift, not part of the tape
                // capstan's pitch variation.
                const float offsetSamples = offMs * 0.001f * (float) sampleRate;
                // Same shifter-latency compensation as the feedback tap
                // above — keeps each head's wet aligned to its ratio
                // target regardless of the shifter's internal delay.
                const float hd = std::max (2.0f, (float) (dly * ratio * m) + offsetSamples - pitchComp);
                const float tL = tapeL.read (hd);
                const float tR = tapeR.read (hd);

                const float balL = (p <= 0.0f) ? 1.0f : 1.0f - p;
                const float balR = (p >= 0.0f) ? 1.0f : 1.0f + p;

                wetL += tL * lvl * balL;
                wetR += tR * lvl * balR;

                headPeak[(size_t) i] = std::max (headPeak[(size_t) i], 0.5f * (std::abs (tL) + std::abs (tR)) * lvl);
            }
        }
        else
        {
            // ---- Delay bypassed -------------------------------------------------
            // Tape buffer untouched (no read, no write, no head taps). The "wet"
            // signal is the input pushed through the character chain + AGE + DC
            // blocker — so the plugin works as a tape/BBD/diffuse/pitch
            // saturator. Reverb (post route) and the mix knob still apply.
            // Advance the head smoothers so re-enabling delay later doesn't pop.
            for (int i = 0; i < 4; ++i)
            {
                (void) smoothedHeadGain[(size_t) i].getNextValue();
                (void) smoothedHeadPan[(size_t) i].getNextValue();
                (void) smoothedHeadRatio[(size_t) i].getNextValue();
            }

            float dL = inL, dR = inR;
            applyCharacter (dL, dR);
            tapeAge.process (dL, dR);
            dL = dcFbL.process (dL);
            dR = dcFbR.process (dR);
            wetL = dL;
            wetR = dR;
        }

        // Phaser post (route=0): on the wet echoes, BEFORE post-reverb. Same
        // routing semantics as hardware: phaser sits at the same insert point
        // as the reverb but always one stage earlier.
        if (params.phaserOn && params.phaserRoute == 0)
        {
            wetL = phaserL.process (wetL);
            wetR = phaserR.process (wetR);
        }

        // Reverb after the delay (or after the bypass path's character chain).
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
        // Dry-delay: align the dry signal with the shifter's algorithm
        // latency so dry and wet stay phase-coherent at the mix. Writes
        // the current dry sample, reads from `pitchAnyPath ? lat : 0`
        // samples ago. When the shifter isn't engaged, lat=0 → dryDelayed
        // == dryL, so no extra latency added to the dry path.
        const int dryLat = pitchAnyPath
            ? juce::jmin (kDryDelayMax - 1,
                          (int) (params.pitchAlgo == 1 ? granPitchL.getLatencySamples()
                                                       : pitchL.getLatencySamples()))
            : 0;
        dryDelayBufL[(size_t) dryDelayWrite] = dryL;
        dryDelayBufR[(size_t) dryDelayWrite] = dryR;
        const int dryReadIdx = (dryDelayWrite - dryLat + kDryDelayMax) % kDryDelayMax;
        const float dryDelayedL = dryDelayBufL[(size_t) dryReadIdx];
        const float dryDelayedR = dryDelayBufR[(size_t) dryReadIdx];
        dryDelayWrite = (dryDelayWrite + 1) % kDryDelayMax;

        // Preset-swap fade-in (ported from hardware): fadeForReload() resets
        // reloadGain to 0 and we ramp back to unity over ~12 ms here, masking
        // the param-swap discontinuity ("DC pulse" click / loud pop on
        // preset reload). Steady-state: reloadGain == 1, gain is unaffected.
        reloadGain += (1.0f - reloadGain) * reloadCoeff;
        const float rg = reloadGain;

        // Use the latency-compensated dry samples so wet and dry stay
        // phase-coherent when the pitch shifter is engaged.
        float outL = (dryDelayedL * (1.0f - mix) + wetL * mix) * og * rg;
        float outR = (dryDelayedR * (1.0f - mix) + wetR * mix) * og * rg;

        // Final safety net (ported from hardware): if anything in the chain
        // (a denorm not flushed, a BBD-style runaway not caught, a freezing
        // reverb tail) produced a non-finite sample, mute it rather than
        // hand the host a NaN that could propagate into the rest of its
        // bus and need a session reload.
        if (! std::isfinite (outL)) outL = 0.0f;
        if (! std::isfinite (outR)) outR = 0.0f;

        // Two-stage output leveler — slow program-dependent gain reduction
        // followed by a fast ceiling catcher. Tames feedback runaway for
        // live use without crushing dynamics like a brick-wall limiter.
        // True bypass when disabled by the user.
        outLeveler.processSample (outL, outR);

        L[n] = outL;
        if (stereo)
            R[n] = outR;
    }

    for (int i = 0; i < 4; ++i)
        headMag[(size_t) i].store (headPeak[(size_t) i], std::memory_order_relaxed);
}
} // namespace doobie
