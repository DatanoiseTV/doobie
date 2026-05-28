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

// State/migration tests. Unlike the JUCE-free DSP tests, these link the full
// plugin so they can exercise the parameter tree and the legacy "modeSel" ->
// head-matrix conversion that keeps old sessions and user presets working.
#include "PluginProcessor.h"
#include "presets/PresetManager.h"
#include "ParameterIDs.h"

#include <cstdio>
#include <initializer_list>
#include <utility>

namespace
{
    int failures = 0;

    void check (bool cond, const char* what)
    {
        if (! cond)
        {
            std::printf ("FAIL: %s\n", what);
            ++failures;
        }
    }

    // A minimal APVTS-style state tree carrying just the named PARAM children.
    juce::ValueTree makeState (std::initializer_list<std::pair<const char*, float>> params)
    {
        juce::ValueTree state ("PARAMS");
        for (const auto& [id, value] : params)
        {
            juce::ValueTree p ("PARAM");
            p.setProperty ("id", id, nullptr);
            p.setProperty ("value", value, nullptr);
            state.addChild (p, -1, nullptr);
        }
        return state;
    }

    bool headOn (juce::AudioProcessorValueTreeState& s, int i)
    {
        return s.getRawParameterValue (dID::headOn[(size_t) i])->load() > 0.5f;
    }
}

// A pre-0.1.0 state stored a 12-position "modeSel". Loading it must light the
// matching heads (index 6 == A+C) and leave the others off.
static void testLegacyModeMaps()
{
    DoobieAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    auto& apvts = proc.getValueTreeState();

    // Seed deliberately wrong switch values so we know migration overwrote them.
    for (int i = 0; i < 4; ++i)
        apvts.getParameter (dID::headOn[(size_t) i])->setValueNotifyingHost (i % 2 == 0 ? 0.0f : 1.0f);

    PresetManager::migrateLegacyState (apvts, makeState ({ { dID::legacyModeSel, 6.0f } }));

    check (headOn (apvts, 0), "legacy modeSel 6 -> head A on");
    check (! headOn (apvts, 1), "legacy modeSel 6 -> head B off");
    check (headOn (apvts, 2), "legacy modeSel 6 -> head C on");
    check (! headOn (apvts, 3), "legacy modeSel 6 -> head D off");
}

// "All heads" (index 11) lights every pad.
static void testLegacyModeAll()
{
    DoobieAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    auto& apvts = proc.getValueTreeState();

    PresetManager::migrateLegacyState (apvts, makeState ({ { dID::legacyModeSel, 11.0f } }));
    check (headOn (apvts, 0) && headOn (apvts, 1) && headOn (apvts, 2) && headOn (apvts, 3),
           "legacy modeSel 11 -> all heads on");
}

// A current state already carries headOn switches; migration must NOT touch
// them even if a stray modeSel is present.
static void testCurrentStateUntouched()
{
    DoobieAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    auto& apvts = proc.getValueTreeState();

    // Set a distinctive matrix (only head D on) via the parameters.
    apvts.getParameter (dID::headOn[0])->setValueNotifyingHost (0.0f);
    apvts.getParameter (dID::headOn[1])->setValueNotifyingHost (0.0f);
    apvts.getParameter (dID::headOn[2])->setValueNotifyingHost (0.0f);
    apvts.getParameter (dID::headOn[3])->setValueNotifyingHost (1.0f);

    // A state that has the switches (and a stray legacy value) must be left as-is.
    PresetManager::migrateLegacyState (apvts, makeState ({
        { dID::legacyModeSel, 0.0f },
        { dID::headOn[0], 0.0f }, { dID::headOn[1], 0.0f },
        { dID::headOn[2], 0.0f }, { dID::headOn[3], 1.0f } }));

    check (! headOn (apvts, 0) && ! headOn (apvts, 1) && ! headOn (apvts, 2) && headOn (apvts, 3),
           "current state with headOn is not overwritten by migration");
}

// A full getState -> setState round-trip preserves the head matrix.
static void testStateRoundTrip()
{
    DoobieAudioProcessor proc;
    proc.prepareToPlay (44100.0, 512);
    auto& apvts = proc.getValueTreeState();

    apvts.getParameter (dID::headOn[0])->setValueNotifyingHost (0.0f); // A off
    apvts.getParameter (dID::headOn[1])->setValueNotifyingHost (1.0f); // B on
    apvts.getParameter (dID::headOn[2])->setValueNotifyingHost (1.0f); // C on
    apvts.getParameter (dID::headOn[3])->setValueNotifyingHost (0.0f); // D off

    juce::MemoryBlock blob;
    proc.getStateInformation (blob);

    // Scramble, then restore.
    for (int i = 0; i < 4; ++i)
        apvts.getParameter (dID::headOn[(size_t) i])->setValueNotifyingHost (1.0f);
    proc.setStateInformation (blob.getData(), (int) blob.getSize());

    check (! headOn (apvts, 0) && headOn (apvts, 1) && headOn (apvts, 2) && ! headOn (apvts, 3),
           "head matrix survives a getState/setState round-trip");
}

// Drive the engine in `delayMode` at the given FEEDBACK with reverb/age off,
// push a short noise burst, then measure the wet output energy in an early and
// a late window. If the loop is stable the late tail must be well below the
// early tail; if it self-oscillates the late tail grows or stays loud. This is
// the regression guard for BBD's pre-tweak runaway at low feedback (~0.32).
static void runDelayDecayCheck (int delayMode, float feedback, const char* label)
{
    DoobieAudioProcessor proc;
    constexpr int sr = 44100;
    constexpr int blockSize = 512;
    proc.prepareToPlay ((double) sr, blockSize);
    auto& apvts = proc.getValueTreeState();

    auto setReal = [&] (const char* id, float v)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    };

    setReal (dID::delayMode,  (float) delayMode);
    setReal (dID::syncMode,   0.0f);   // free
    setReal (dID::timeMs,     200.0f); // short enough that early/late windows are clearly separated
    setReal (dID::feedback,   feedback);
    setReal (dID::reverbMode, 0.0f);   // reverb off, so we only measure the delay loop
    setReal (dID::mix,        1.0f);   // wet only
    setReal (dID::hiss,       0.0f);   // AGE off (no hiss floor faking a tail)
    setReal (dID::wow,        0.0f);
    setReal (dID::flutter,    0.0f);
    setReal (dID::drive,      0.0f);

    juce::AudioBuffer<float> buf (2, blockSize);
    juce::MidiBuffer midi;
    juce::Random rng (0xBEEF);

    double earlySq = 0.0, lateSq = 0.0;
    int    earlyN  = 0,   lateN  = 0;
    bool   finite  = true;

    const int totalBlocks = (3 * sr) / blockSize;          // ~3 s
    const int burstBlocks = (sr / 10) / blockSize;         // 0.1 s burst

    for (int b = 0; b < totalBlocks; ++b)
    {
        const bool burst = b < burstBlocks;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getWritePointer (ch);
            for (int i = 0; i < blockSize; ++i)
                d[i] = burst ? (rng.nextFloat() * 2.0f - 1.0f) * 0.3f : 0.0f;
        }
        proc.processBlock (buf, midi);

        const int startSample = b * blockSize;
        for (int ch = 0; ch < 2; ++ch)
        {
            auto* d = buf.getReadPointer (ch);
            for (int i = 0; i < blockSize; ++i)
            {
                if (! std::isfinite (d[i])) finite = false;
                const int s = startSample + i;
                const double e = (double) d[i] * (double) d[i];
                if (s >= sr * 1 / 5 && s < sr * 3 / 5) { earlySq += e; ++earlyN; } // 0.2..0.6 s
                if (s >= sr * 2     && s < sr * 3)     { lateSq  += e; ++lateN;  } // 2.0..3.0 s
            }
        }
    }

    const double earlyRms = std::sqrt (earlySq / std::max (1, earlyN));
    const double lateRms  = std::sqrt (lateSq  / std::max (1, lateN));

    check (finite, label);
    check (earlyRms > 1.0e-4 && lateRms < 0.5 * earlyRms, label);
}

static void testDelayDecay()
{
    runDelayDecayCheck (0, 0.6f, "Digital delay decays at FB=0.6");
    runDelayDecayCheck (1, 0.6f, "Tape delay decays at FB=0.6");
    runDelayDecayCheck (2, 0.6f, "BBD delay decays at FB=0.6 (regression: no premature self-oscillation)");
    runDelayDecayCheck (3, 0.6f, "Diffuse delay decays at FB=0.6");
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    testLegacyModeMaps();
    testLegacyModeAll();
    testCurrentStateUntouched();
    testStateRoundTrip();
    testDelayDecay();

    if (failures == 0)
        std::printf ("All state tests passed.\n");
    return failures == 0 ? 0 : 1;
}
