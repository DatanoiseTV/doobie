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

#include "DubDelayEngine.h"

#include <juce_core/juce_core.h>
#include <array>
#include <algorithm>

namespace doobie
{
// Modulation matrix: a small fixed number of slots, each routing one source
// (off / LFO1 / LFO2 / Env) to one destination (a curated subset of the
// engine's parameters) with a bipolar amount. Modulation is applied each
// block to the base EngineParams the processor computes from APVTS, then the
// engine's own smoothers ramp toward the modulated targets — so fast LFO
// modulation gets gently low-passed by the smoothing, which is the right
// behaviour (no zipper, no clicks).
//
// Sources are bipolar for the LFOs (-1..+1, depth-scaled) and unipolar for
// the envelope follower (0..1). The amount knob (-1..+1) flips polarity for
// the env source, so a positive amount on Env → Duck gives the obvious
// sidechain feel: louder input ⇒ more duck ⇒ quieter wet.
constexpr int kNumModSlots = 8;

enum class ModSource : int { Off = 0, Lfo1, Lfo2, Env, Count };

// Curated subset of useful destinations. Adding entries here is the main way
// to extend the matrix — appending keeps stored slot indices stable.
enum class ModDest : int
{
    Off = 0,
    DelayTime,         // multiplicative ±50% of base delay
    Feedback,          // ±0.6
    Mix,               // ±0.5
    Width,             // ±1.0
    Duck,              // ±0.5  (sidechain-friendly with Env source)
    Drive,             // ±0.5
    Wow,               // ±0.5
    Flutter,           // ±0.5
    Age,               // ±0.5
    PreHpFreq,         // ±2 octaves multiplicative
    PreLpFreq,         // ±2 octaves multiplicative
    HpFreq,            // ±2 octaves multiplicative
    LpFreq,            // ±2 octaves multiplicative
    Bass,              // ±0.7
    Treble,            // ±0.7
    Head1Level, Head2Level, Head3Level, Head4Level,  // ±0.5
    ReverbMix,         // ±0.5
    ReverbMod,         // ±0.5
    PlateDecay,        // ±0.5
    PlateSize,         // ±0.5
    PlateDamp,         // ±0.5
    PlatePredelay,     // ±80 ms
    SpringDecay,       // ±0.5
    SpringTone,        // ±0.5
    IRGain,            // ±12 dB
    // Per-head pan / time appended last so existing saved slot indices
    // (Off=0..IRGain) keep their meaning.
    Head1Pan,   Head2Pan,   Head3Pan,   Head4Pan,    // ±1.0
    Head1Ratio, Head2Ratio, Head3Ratio, Head4Ratio,  // ±0.3 around base, clamped
    Count
};

inline juce::StringArray modSourceNames()
{
    return { "Off", "LFO 1", "LFO 2", "Env" };
}

inline juce::StringArray modDestNames()
{
    return {
        "Off",
        "Delay Time", "Feedback", "Mix", "Width", "Duck",
        "Drive", "Wow", "Flutter", "Age",
        "Pre Low Cut", "Pre High Cut", "Low Cut", "High Cut",
        "Bass", "Treble",
        "Head 1 Level", "Head 2 Level", "Head 3 Level", "Head 4 Level",
        "Reverb Mix", "Reverb Mod",
        "Plate Decay", "Plate Size", "Plate Damp", "Plate Predelay",
        "Spring Decay", "Spring Tone", "IR Gain",
        "Head 1 Pan", "Head 2 Pan", "Head 3 Pan", "Head 4 Pan",
        "Head 1 Time", "Head 2 Time", "Head 3 Time", "Head 4 Time"
    };
}

struct ModSlot
{
    ModSource source = ModSource::Off;
    ModDest   dest   = ModDest::Off;
    float     amount = 0.0f;   // -1..+1
};

struct ModSourceValues
{
    float lfo1 = 0.0f;  // bipolar, [-1, 1]
    float lfo2 = 0.0f;
    float env  = 0.0f;  // unipolar, [0, 1]
};

// Modulate p in place by `amount * sourceValue * destinationRange`.
inline void applyModSlot (EngineParams& p, const ModSlot& slot, const ModSourceValues& s) noexcept
{
    if (slot.source == ModSource::Off || slot.dest == ModDest::Off || slot.amount == 0.0f)
        return;

    float src = 0.0f;
    switch (slot.source)
    {
        case ModSource::Lfo1: src = s.lfo1; break;
        case ModSource::Lfo2: src = s.lfo2; break;
        case ModSource::Env:  src = s.env;  break;
        case ModSource::Off:
        case ModSource::Count:
        default: return;
    }

    const float k = src * slot.amount;            // signed modulation in roughly [-1, 1]
    const auto add = [] (float& v, float delta, float lo, float hi)
    { v = juce::jlimit (lo, hi, v + delta); };
    const auto mulOctaves = [] (float& v, float octaves, float lo, float hi)
    { v = juce::jlimit (lo, hi, v * std::pow (2.0f, octaves)); };

    switch (slot.dest)
    {
        case ModDest::DelayTime:     p.delaySamples = std::clamp (p.delaySamples * (1.0 + 0.5 * (double) k), 2.0, p.delaySamples * 10.0); break;
        case ModDest::Feedback:      add (p.feedback,  0.6f * k, 0.0f, 1.2f); break;
        case ModDest::Mix:           add (p.mix,       0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Width:         add (p.width,     1.0f * k, 0.0f, 2.0f); break;
        case ModDest::Duck:          add (p.duck,      0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Drive:         add (p.drive,     0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Wow:           add (p.wow,       0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Flutter:       add (p.flutter,   0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Age:           add (p.age,       0.5f * k, 0.0f, 1.0f); break;
        case ModDest::PreHpFreq:     mulOctaves (p.preHp,  2.0f * k, 20.0f,    1000.0f);  break;
        case ModDest::PreLpFreq:     mulOctaves (p.preLp,  2.0f * k, 1000.0f,  18000.0f); break;
        case ModDest::HpFreq:        mulOctaves (p.hpFreq, 2.0f * k, 20.0f,    1000.0f);  break;
        case ModDest::LpFreq:        mulOctaves (p.lpFreq, 2.0f * k, 1000.0f,  18000.0f); break;
        case ModDest::Bass:          add (p.bass,        0.7f * k, -1.0f, 1.0f); break;
        case ModDest::Treble:        add (p.treble,      0.7f * k, -1.0f, 1.0f); break;
        case ModDest::Head1Level:    add (p.headLevel[0], 0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Head2Level:    add (p.headLevel[1], 0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Head3Level:    add (p.headLevel[2], 0.5f * k, 0.0f, 1.0f); break;
        case ModDest::Head4Level:    add (p.headLevel[3], 0.5f * k, 0.0f, 1.0f); break;
        case ModDest::ReverbMix:     add (p.reverbMix,   0.5f * k, 0.0f, 1.0f); break;
        case ModDest::ReverbMod:     add (p.plateMod,    0.5f * k, 0.0f, 1.0f); break;
        case ModDest::PlateDecay:    add (p.plateDecay,  0.5f * k, 0.0f, 1.0f); break;
        case ModDest::PlateSize:     add (p.plateSize,   0.5f * k, 0.0f, 1.0f); break;
        case ModDest::PlateDamp:     add (p.plateDamp,   0.5f * k, 0.0f, 1.0f); break;
        case ModDest::PlatePredelay: add (p.platePredelay, 80.0f * k, 0.0f, 200.0f); break;
        case ModDest::SpringDecay:   add (p.springDecay, 0.5f * k, 0.0f, 1.0f); break;
        case ModDest::SpringTone:    add (p.springTone,  0.5f * k, 0.0f, 1.0f); break;
        case ModDest::IRGain:        p.irGain *= juce::Decibels::decibelsToGain (12.0f * k); break;
        case ModDest::Head1Pan:      add (p.headPan[0],  1.0f * k, -1.0f, 1.0f); break;
        case ModDest::Head2Pan:      add (p.headPan[1],  1.0f * k, -1.0f, 1.0f); break;
        case ModDest::Head3Pan:      add (p.headPan[2],  1.0f * k, -1.0f, 1.0f); break;
        case ModDest::Head4Pan:      add (p.headPan[3],  1.0f * k, -1.0f, 1.0f); break;
        case ModDest::Head1Ratio:    add (p.headRatio[0], 0.3f * k, 0.05f, 1.0f); break;
        case ModDest::Head2Ratio:    add (p.headRatio[1], 0.3f * k, 0.05f, 1.0f); break;
        case ModDest::Head3Ratio:    add (p.headRatio[2], 0.3f * k, 0.05f, 1.0f); break;
        case ModDest::Head4Ratio:    add (p.headRatio[3], 0.3f * k, 0.05f, 1.0f); break;
        case ModDest::Off:
        case ModDest::Count:
        default: break;
    }
}

inline void applyModMatrix (EngineParams& p,
                            const std::array<ModSlot, kNumModSlots>& slots,
                            const ModSourceValues& sources) noexcept
{
    for (const auto& slot : slots)
        applyModSlot (p, slot, sources);
}
} // namespace doobie
