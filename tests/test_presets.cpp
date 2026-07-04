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

// Factory-preset containment test. Loads every factory preset, hits it with a
// broadband burst then silence, and asserts the output stays FINITE and BOUNDED
// under a ceiling. Sustained musical feedback is fine (dub lives on it); the
// failure this guards against is a preset that self-oscillates out of control
// into NaN/Inf or a level that runs away past the ceiling. The offline
// tools/StabilityCheck.cpp is the interactive, spectral version of this; this
// is the CI gate. Also asserts factory names are unique so loadByName can reach
// every preset.
#include "PluginProcessor.h"
#include "presets/PresetManager.h"
#include "ParameterIDs.h"

#include <cmath>
#include <cstdio>
#include <set>

namespace
{
    int failures = 0;

    void check (bool cond, const char* what)
    {
        if (! cond) { std::printf ("FAIL: %s\n", what); ++failures; }
    }

    struct FixedTempoPlayHead : juce::AudioPlayHead
    {
        juce::Optional<PositionInfo> getPosition() const override
        {
            PositionInfo pos;
            pos.setBpm (120.0);
            pos.setIsPlaying (true);
            pos.setTimeInSamples (samples);
            return pos;
        }
        juce::int64 samples = 0;
    };
}

// Every factory preset must stay finite and under the ceiling across a burst +
// silence render, including the deliberately self-oscillating "feedback drone"
// presets (feedback >= 1.0). Containment, not silence, is the bar.
static void testAllPresetsContained()
{
    constexpr double sr = 48000.0;
    constexpr int    block = 512;
    constexpr double seconds = 8.0;             // long enough for a runaway to blow up
    const int total = (int) (sr * seconds);
    const int burst = (int) (sr * 1.0);
    const float ceiling = juce::Decibels::decibelsToGain (12.0f);   // +12 dBFS hard limit

    juce::StringArray names;
    {
        DoobieAudioProcessor probe;
        names = probe.getPresetManager().getFactoryNames();
    }
    check (names.size() == 138, "factory bank has 138 presets");

    juce::Random rng (0x0D0B1E);
    int checked = 0;

    for (const auto& name : names)
    {
        FixedTempoPlayHead playHead;
        DoobieAudioProcessor proc;
        proc.setPlayHead (&playHead);
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.prepareToPlay (sr, block);
        proc.getPresetManager().loadByName (name);

        // MIDI-note presets make no sound without notes; skip.
        if (proc.getValueTreeState().getRawParameterValue (dID::midiPitchMode)->load() > 0.5f)
            continue;

        bool finite = true, bounded = true;
        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::AudioBuffer<float> chunk (2, n);
            chunk.clear();
            if (pos < burst)
            {
                const int copy = juce::jmin (n, burst - pos);
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = chunk.getWritePointer (ch);
                    for (int i = 0; i < copy; ++i)
                        d[i] = (rng.nextFloat() * 2.0f - 1.0f) * 0.35f;
                }
            }
            midi.clear();
            proc.processBlock (chunk, midi);
            playHead.samples += n;

            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = chunk.getReadPointer (ch);
                for (int i = 0; i < n; ++i)
                {
                    if (! std::isfinite (d[i]))       finite = false;
                    if (std::abs (d[i]) > ceiling)    bounded = false;
                }
            }
            if (! finite || ! bounded) break;
        }
        proc.setPlayHead (nullptr);

        if (! finite)  { std::printf ("  non-finite: %s\n", name.toRawUTF8()); ++failures; }
        if (! bounded) { std::printf ("  over ceiling: %s\n", name.toRawUTF8()); ++failures; }
        ++checked;
    }
    std::printf ("checked %d/%d presets (rest are MIDI-note presets)\n", checked, names.size());
}

// loadByName returns the FIRST match, so a duplicate name makes a preset
// unreachable. Regression guard for the "two Cathedral presets" bug.
static void testFactoryNamesUnique()
{
    DoobieAudioProcessor proc;
    const auto names = proc.getPresetManager().getFactoryNames();
    std::set<juce::String> seen;
    for (const auto& n : names)
    {
        if (seen.count (n) != 0)
            std::printf ("  duplicate name: %s\n", n.toRawUTF8());
        check (seen.count (n) == 0, "factory preset name is unique");
        seen.insert (n);
    }
}

// Every Convolution preset (reverbMode == 7) must actually resolve and load a
// factory IR when applied — i.e. its named IR exists and the IR-load hook fired
// and stamped factoryIrIndexProperty. A typo in a preset's IR name would leave
// the convolution engine empty (silent wet); this catches that.
static void testConvolutionPresetsLoadIR()
{
    juce::StringArray names;
    {
        DoobieAudioProcessor probe;
        names = probe.getPresetManager().getFactoryNames();
    }

    int convChecked = 0;
    for (const auto& name : names)
    {
        DoobieAudioProcessor proc;
        proc.prepareToPlay (48000.0, 512);
        proc.getPresetManager().loadByName (name);
        auto& apvts = proc.getValueTreeState();

        if ((int) std::lround (apvts.getRawParameterValue (dID::reverbMode)->load()) != 7)
            continue;   // not a convolution preset

        const int irIdx = (int) apvts.state.getProperty (dID::factoryIrIndexProperty, -1);
        if (irIdx < 0)
            std::printf ("  convolution preset loaded no IR: %s\n", name.toRawUTF8());
        check (irIdx >= 0, "convolution preset resolves + loads its factory IR");
        ++convChecked;
    }
    check (convChecked >= 10, "at least 10 convolution presets present");
    std::printf ("checked %d convolution presets\n", convChecked);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    testFactoryNamesUnique();
    testConvolutionPresetsLoadIR();
    testAllPresetsContained();

    if (failures == 0)
        std::printf ("All preset tests passed.\n");
    return failures == 0 ? 0 : 1;
}
