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

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    testLegacyModeMaps();
    testLegacyModeAll();
    testCurrentStateUntouched();
    testStateRoundTrip();

    if (failures == 0)
        std::printf ("All state tests passed.\n");
    return failures == 0 ? 0 : 1;
}
