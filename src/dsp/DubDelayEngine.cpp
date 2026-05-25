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

    spring.prepare (sr);
    plate.prepare (sr);

    smoothedDelay.reset (sr, 0.30);
    smoothedFeedback.reset (sr, 0.03);
    smoothedMix.reset (sr, 0.02);
    smoothedOut.reset (sr, 0.02);
    smoothedInGain.reset (sr, 0.02);
    smoothedWidth.reset (sr, 0.05);

    smoothedDelay.setCurrentAndTargetValue (params.delaySamples);
    smoothedFeedback.setCurrentAndTargetValue (params.feedback);
    smoothedMix.setCurrentAndTargetValue (params.mix);
    smoothedOut.setCurrentAndTargetValue (params.outGain);
    smoothedInGain.setCurrentAndTargetValue (params.inputGain);
    smoothedWidth.setCurrentAndTargetValue (params.width);

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
    spring.reset();
    plate.reset();
    wowFlutter.reset();
    duckEnv = 0.0f;
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
    satL.setDrive (params.drive);
    satR.setDrive (params.drive);

    wowFlutter.setAmounts (params.wow, params.flutter);

    spring.setParams (params.springDecay, params.springTone, params.plateMod);
    plate.setParams (params.plateDecay, params.plateSize, params.plateDamp, params.platePredelay, params.plateMod);

    const int   mask      = kModeMask[(size_t) std::clamp (params.mode, 0, 11)];
    const bool  reverbOn  = params.reverbMode != 0;
    const float revMix    = params.reverbMix;
    const float hissLevel = params.hiss * 0.015f;

    // Ducking ballistics (fast attack, slow release).
    const float atk = 1.0f - std::exp (-1.0f / (0.005f * (float) sampleRate));
    const float rel = 1.0f - std::exp (-1.0f / (0.200f * (float) sampleRate));

    std::array<float, 4> headPeak { 0.0f, 0.0f, 0.0f, 0.0f };

    for (int n = 0; n < numSamples; ++n)
    {
        const float m   = wowFlutter.next();
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

        // Tone shaping + tape saturation in the feedback path.
        float fbL = satL.process (toneL.process (fbReadL));
        float fbR = satR.process (toneR.process (fbReadR));

        // Reverb sitting inside the feedback loop (washes build up over repeats).
        if (reverbOn && params.reverbRoute == 2)
        {
            float rL, rR;
            applyReverb (fbL, fbR, rL, rR);
            fbL = fbL * (1.0f - revMix) + rL * revMix;
            fbR = fbR * (1.0f - revMix) + rR * revMix;
        }

        const float fbGain = params.freeze ? 1.0f : fb;
        float fbContribL = fbL * fbGain;
        float fbContribR = fbR * fbGain;
        if (params.pingPong)
            std::swap (fbContribL, fbContribR);

        // Input into the loop (optionally reverberated before the delay).
        float inL = dryL * ig;
        float inR = dryR * ig;
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
            if ((mask & (1 << i)) == 0)
                continue;
            const float lvl = params.headLevel[(size_t) i];
            if (lvl <= 0.0001f)
                continue;

            const float hd = std::max (2.0f, (float) (dly * params.headRatio[(size_t) i] * m));
            const float tL = tapeL.read (hd);
            const float tR = tapeR.read (hd);

            const float p    = params.headPan[(size_t) i];
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
