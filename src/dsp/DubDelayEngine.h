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

#include "DelayLine.h"
#include "WowFlutter.h"
#include "Saturation.h"
#include "ToneStack.h"
#include "SpringReverb.h"
#include "PlateReverb.h"
#include "FdnReverb.h"
#include "ShimmerReverb.h"
#include "Diffuser.h"
#include "OctaveShifter.h"

#include <juce_dsp/juce_dsp.h>
#include <array>

namespace doobie
{
// Friendly, already-converted control values handed to the engine each block.
// The processor is responsible for turning raw APVTS values (dB, Hz, note
// divisions, BPM) into these engine units, so the DSP never has to know about
// parameter ranges or the host transport.
struct EngineParams
{
    float inputGain  = 1.0f;   // linear
    float mix        = 0.5f;   // 0..1 dry/wet
    float outGain    = 1.0f;   // linear
    float width      = 1.0f;   // 0..2 stereo width of the wet path

    double delaySamples = 22050.0; // resolved length of the longest head (ratio 1.0)
    float  feedback     = 0.4f;    // 0..1.2 (above ~1 self-oscillates)
    int    delayMode    = 1;       // 0 digital, 1 tape, 2 BBD, 3 diffuse, 4 pitch
    int    mode         = 11;      // head mask index (the mode dial)
    bool   pingPong     = false;
    bool   freeze       = false;
    float  duck         = 0.0f;    // 0..1 wet ducking by dry level

    std::array<float, 4> headLevel { 0.9f, 0.0f, 0.0f, 0.7f };
    std::array<float, 4> headPan   { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 4> headRatio { 0.25f, 0.5f, 0.75f, 1.0f };

    float wow = 0.15f, flutter = 0.1f, drive = 0.25f, hiss = 0.0f;

    float preHp = 20.0f, preLp = 18000.0f;   // input filter cuts
    float preBass = 0.0f, preTreble = 0.0f;  // input filter shelves (-1..1)

    float bass = 0.0f, treble = 0.0f;   // feedback shelves (-1..1)
    float hpFreq = 120.0f, lpFreq = 6500.0f;

    int   reverbMode  = 1;   // 0 off, 1 spring, 2 plate, 3 series, 4 parallel
    int   reverbRoute = 0;   // 0 post, 1 pre, 2 in feedback
    float reverbMix   = 0.25f;
    float springDecay = 0.5f, springTone = 0.5f;
    float plateDecay = 0.6f, plateSize = 0.6f, plateDamp = 0.4f, platePredelay = 20.0f, plateMod = 0.3f;
};

// The complete dub delay: a stereo multi-head tape echo whose feedback path
// carries tone shaping, tape saturation and wow/flutter, with a spring/plate
// reverb that can sit before, after, or inside the feedback loop.
class DubDelayEngine
{
public:
    // Head-on bitmask for each of the 12 mode-dial positions (bit i = head i).
    static constexpr std::array<int, 12> kModeMask {
        0b0001, 0b0010, 0b0100, 0b1000,   // single heads A,B,C,D
        0b0011, 0b1100, 0b0101, 0b1010,   // pairs
        0b0111, 0b1110, 0b1001, 0b1111    // triples / all
    };

    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParams (const EngineParams& p) { params = p; }

    // Processes a stereo buffer in place.
    void process (juce::AudioBuffer<float>& buffer);

    // Latest per-head output magnitudes, for the echo visualiser (read by UI).
    const std::array<std::atomic<float>, 4>& headMagnitudes() const { return headMag; }
    float currentDelaySamples() const { return (float) smoothedDelay.getCurrentValue(); }

private:
    void applyReverb (float inL, float inR, float& outL, float& outR) noexcept;
    inline float whiteNoise() noexcept;

    double sampleRate = 44100.0;
    EngineParams params;

    DelayLine tapeL, tapeR;

    WowFlutter wowFlutter;
    Saturation satL, satR;
    ToneStack  toneL, toneR;       // in the feedback loop (every repeat)
    ToneStack  preToneL, preToneR; // pre-delay, on the signal entering the tape

    // Per-character feedback processors (used depending on the delay mode).
    Diffuser      diffuseL, diffuseR;   // Diffuse mode
    OctaveShifter pitchL, pitchR;       // Pitch mode
    float bbdLpL = 0.0f, bbdLpR = 0.0f; // BBD darkening one-pole

    SpringReverb  spring;
    PlateReverb   plate;
    FdnReverb     hall;
    ShimmerReverb shimmer;

    // Multiplicative smoothing glides the delay time at a constant ratio, which
    // sounds like a tape capstan easing to a new speed rather than a linear jump.
    juce::SmoothedValue<double, juce::ValueSmoothingTypes::Multiplicative> smoothedDelay;
    juce::SmoothedValue<float>  smoothedFeedback, smoothedMix, smoothedOut, smoothedInGain, smoothedWidth;

    float duckEnv = 0.0f;
    uint32_t rngState = 0x1234567u;

    std::array<std::atomic<float>, 4> headMag { };

    double maxDelaySeconds = 8.0;
};
} // namespace doobie
