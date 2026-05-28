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

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "ParameterIDs.h"

namespace
{
    juce::ParameterID pid (const char* id) { return { id, dID::kVersion }; }
}

juce::AudioProcessorValueTreeState::ParameterLayout DoobieAudioProcessor::createParameterLayout()
{
    using FloatParam  = juce::AudioParameterFloat;
    using ChoiceParam = juce::AudioParameterChoice;
    using BoolParam   = juce::AudioParameterBool;
    using Range       = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ---- I/O ----------------------------------------------------------------
    layout.add (std::make_unique<FloatParam> (pid (dID::inputDrive), "Input", Range (-24.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::mix),        "Mix",   Range (0.0f, 1.0f, 0.001f), 0.35f));
    layout.add (std::make_unique<FloatParam> (pid (dID::outputGain), "Output", Range (-24.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::width),      "Width", Range (0.0f, 2.0f, 0.001f), 1.0f));

    // ---- Delay --------------------------------------------------------------
    layout.add (std::make_unique<ChoiceParam> (pid (dID::delayMode), "Character", dID::delayModeChoices, 1));
    layout.add (std::make_unique<BoolParam> (pid (dID::syncMode), "Sync", true));
    layout.add (std::make_unique<FloatParam> (pid (dID::timeMs), "Time", Range (20.0f, 8000.0f, 0.1f, 0.30f), 375.0f));
    layout.add (std::make_unique<ChoiceParam> (pid (dID::syncDiv), "Division", dID::syncDivChoices, 10));
    layout.add (std::make_unique<FloatParam> (pid (dID::feedback), "Feedback", Range (0.0f, 1.2f, 0.001f), 0.4f));
    layout.add (std::make_unique<BoolParam> (pid (dID::pingPong), "Ping-Pong", false));
    layout.add (std::make_unique<BoolParam> (pid (dID::freeze), "Freeze", false));
    layout.add (std::make_unique<FloatParam> (pid (dID::duck), "Duck", Range (0.0f, 1.0f, 0.001f), 0.0f));

    // ---- Multi-head ---------------------------------------------------------
    // The head matrix: head 1 on by default (a single tap at the repeat time),
    // matching the previous default mode. Old presets map onto these switches.
    const std::array<bool, 4>  defOn    { true, false, false, false };
    const std::array<float, 4> defLevel { 0.7f, 0.7f, 0.7f, 0.7f };
    const std::array<float, 4> defPan   { 0.0f, -0.4f, 0.4f, 0.0f };
    const std::array<float, 4> defRatio  { 1.0f, 0.75f, 0.5f, 0.25f };
    for (int i = 0; i < 4; ++i)
    {
        const auto n = juce::String (i + 1);
        layout.add (std::make_unique<BoolParam> (pid (dID::headOn[(size_t) i]), "Head " + n + " On", defOn[(size_t) i]));
        layout.add (std::make_unique<FloatParam> (pid (dID::headLevel[(size_t) i]), "Head " + n + " Level", Range (0.0f, 1.0f, 0.001f), defLevel[(size_t) i]));
        layout.add (std::make_unique<FloatParam> (pid (dID::headPan[(size_t) i]),   "Head " + n + " Pan",   Range (-1.0f, 1.0f, 0.001f), defPan[(size_t) i]));
        layout.add (std::make_unique<FloatParam> (pid (dID::headRatio[(size_t) i]), "Head " + n + " Ratio", Range (0.05f, 1.0f, 0.001f), defRatio[(size_t) i]));
    }

    // ---- Tape character -----------------------------------------------------
    layout.add (std::make_unique<FloatParam> (pid (dID::wow),     "Wow",     Range (0.0f, 1.0f, 0.001f), 0.15f));
    layout.add (std::make_unique<FloatParam> (pid (dID::flutter), "Flutter", Range (0.0f, 1.0f, 0.001f), 0.10f));
    layout.add (std::make_unique<FloatParam> (pid (dID::drive),   "Saturation", Range (0.0f, 1.0f, 0.001f), 0.25f));
    layout.add (std::make_unique<FloatParam> (pid (dID::hiss),    "Tape Age", Range (0.0f, 1.0f, 0.001f), 0.0f));

    // ---- Input filter (on the signal entering the delay; default open) ------
    layout.add (std::make_unique<FloatParam> (pid (dID::preHpFreq), "Input Low Cut",  Range (20.0f, 1000.0f, 0.1f, 0.4f), 20.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::preLpFreq), "Input High Cut", Range (1000.0f, 18000.0f, 1.0f, 0.4f), 18000.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::preBass),   "Input Bass",   Range (-1.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::preTreble), "Input Treble", Range (-1.0f, 1.0f, 0.001f), 0.0f));

    // ---- Feedback tone ------------------------------------------------------
    layout.add (std::make_unique<FloatParam> (pid (dID::bass),   "Bass",   Range (-1.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::treble), "Treble", Range (-1.0f, 1.0f, 0.001f), 0.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::hpFreq), "Low Cut",  Range (20.0f, 1000.0f, 0.1f, 0.4f), 120.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::lpFreq), "High Cut", Range (1000.0f, 18000.0f, 1.0f, 0.4f), 6500.0f));

    // ---- Reverb -------------------------------------------------------------
    layout.add (std::make_unique<ChoiceParam> (pid (dID::reverbMode),  "Reverb", dID::reverbModeChoices, 1));
    layout.add (std::make_unique<ChoiceParam> (pid (dID::reverbRoute), "Reverb Route", dID::reverbRouteChoices, 0));
    layout.add (std::make_unique<FloatParam> (pid (dID::reverbMix),     "Reverb Mix", Range (0.0f, 1.0f, 0.001f), 0.25f));
    layout.add (std::make_unique<FloatParam> (pid (dID::springDecay),   "Spring Decay", Range (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<FloatParam> (pid (dID::springTone),    "Spring Tone", Range (0.0f, 1.0f, 0.001f), 0.5f));
    layout.add (std::make_unique<FloatParam> (pid (dID::plateDecay),    "Plate Decay", Range (0.0f, 1.0f, 0.001f), 0.6f));
    layout.add (std::make_unique<FloatParam> (pid (dID::plateSize),     "Plate Size", Range (0.0f, 1.0f, 0.001f), 0.6f));
    layout.add (std::make_unique<FloatParam> (pid (dID::plateDamp),     "Plate Damp", Range (0.0f, 1.0f, 0.001f), 0.4f));
    layout.add (std::make_unique<FloatParam> (pid (dID::platePredelay), "Plate Pre-delay", Range (0.0f, 200.0f, 0.1f), 20.0f));
    layout.add (std::make_unique<FloatParam> (pid (dID::reverbMod),     "Reverb Mod", Range (0.0f, 1.0f, 0.001f), 0.3f));

    return layout;
}

DoobieAudioProcessor::DoobieAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      presetManager (apvts)
{
}

bool DoobieAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void DoobieAudioProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    updateEngineParams();
    engine.prepare (sr, samplesPerBlock);
}

void DoobieAudioProcessor::updateEngineParams()
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    doobie::EngineParams p;
    p.inputGain = juce::Decibels::decibelsToGain (raw (dID::inputDrive));
    p.outGain   = juce::Decibels::decibelsToGain (raw (dID::outputGain));
    p.mix       = raw (dID::mix);
    p.width     = raw (dID::width);
    p.feedback  = raw (dID::feedback);
    p.delayMode = (int) raw (dID::delayMode);
    p.pingPong  = raw (dID::pingPong) > 0.5f;
    p.freeze    = raw (dID::freeze) > 0.5f;
    p.duck      = raw (dID::duck);

    // Resolve the master delay length first (heads reference it). Sync uses a
    // musical division against host tempo; free uses milliseconds.
    const bool synced = raw (dID::syncMode) > 0.5f;
    double masterQuarters = 0.0;
    if (synced)
    {
        const double bpm    = juce::jlimit (20.0, 300.0, currentBpm.load());
        const int    divIdx = juce::jlimit (0, (int) dID::syncDivQuarters.size() - 1, (int) raw (dID::syncDiv));
        masterQuarters = dID::syncDivQuarters[(size_t) divIdx];
        p.delaySamples = masterQuarters * (60.0 / bpm) * sampleRate;
    }
    else
    {
        p.delaySamples = (raw (dID::timeMs) * 0.001) * sampleRate;
    }

    for (int i = 0; i < 4; ++i)
    {
        p.headOn[(size_t) i]    = raw (dID::headOn[(size_t) i]) > 0.5f;
        p.headLevel[(size_t) i] = raw (dID::headLevel[(size_t) i]);
        p.headPan[(size_t) i]   = raw (dID::headPan[(size_t) i]);

        const float rawRatio = raw (dID::headRatio[(size_t) i]);
        if (synced && masterQuarters > 0.0)
        {
            // The head TIME control selects a musical division of the repeat.
            const double snapped = dID::snapHeadQuarters ((double) rawRatio * masterQuarters, masterQuarters);
            p.headRatio[(size_t) i] = (float) (snapped / masterQuarters);
        }
        else
        {
            p.headRatio[(size_t) i] = rawRatio; // free-running: continuous fraction
        }
    }

    p.wow = raw (dID::wow);
    p.flutter = raw (dID::flutter);
    p.drive = raw (dID::drive);
    p.age = raw (dID::hiss);   // "hiss" id is historical; now drives the AGE macro

    p.preHp = raw (dID::preHpFreq);
    p.preLp = raw (dID::preLpFreq);
    p.preBass = raw (dID::preBass);
    p.preTreble = raw (dID::preTreble);
    p.bass = raw (dID::bass);
    p.treble = raw (dID::treble);
    p.hpFreq = raw (dID::hpFreq);
    p.lpFreq = raw (dID::lpFreq);

    p.reverbMode  = (int) raw (dID::reverbMode);
    p.reverbRoute = (int) raw (dID::reverbRoute);
    p.reverbMix   = raw (dID::reverbMix);
    p.springDecay = raw (dID::springDecay);
    p.springTone  = raw (dID::springTone);
    p.plateDecay  = raw (dID::plateDecay);
    p.plateSize   = raw (dID::plateSize);
    p.plateDamp   = raw (dID::plateDamp);
    p.platePredelay = raw (dID::platePredelay);
    p.plateMod    = raw (dID::reverbMod);

    engine.setParams (p);
}

void DoobieAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalIn  = getTotalNumInputChannels();
    const auto totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                currentBpm.store (*bpm);

    updateEngineParams();
    engine.process (buffer);

    // Capture output peaks for the VU meters.
    for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        outputLevel[(size_t) ch].store (buffer.getMagnitude (ch, 0, buffer.getNumSamples()),
                                        std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DoobieAudioProcessor::createEditor()
{
    return new DoobieAudioProcessorEditor (*this);
}

void DoobieAudioProcessor::loadIR (const juce::File& f)
{
    if (! f.existsAsFile())
        return;
    engine.loadIR (f);
    apvts.state.setProperty (dID::irPathProperty, f.getFullPathName(), nullptr);
    apvts.state.setProperty (dID::factoryIrIndexProperty, -1, nullptr); // file IR wins
}

void DoobieAudioProcessor::loadFactoryIR (int index)
{
    if (! engine.loadFactoryIR (index))
        return;
    apvts.state.setProperty (dID::factoryIrIndexProperty, index, nullptr);
    apvts.state.setProperty (dID::irPathProperty, juce::String(), nullptr);  // factory IR wins
}

void DoobieAudioProcessor::clearIR()
{
    engine.clearIR();
    apvts.state.setProperty (dID::irPathProperty, juce::String(), nullptr);
    apvts.state.setProperty (dID::factoryIrIndexProperty, -1, nullptr);
}

void DoobieAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", presetManager.getCurrentName(), nullptr);
    // The IR path is already on apvts.state (set by loadIR), so copyState
    // carries it. No extra work needed here.
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void DoobieAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            auto tree = juce::ValueTree::fromXml (*xml);
            apvts.replaceState (tree);
            PresetManager::migrateLegacyState (apvts, tree); // old "modeSel" -> head matrix

            // Restore the IR. Factory takes precedence (no file dependencies);
            // fall back to a file path if no factory index was stored. If the
            // saved file no longer exists the engine just stays in bypass.
            const int factoryIdx = (int) apvts.state.getProperty (dID::factoryIrIndexProperty, -1);
            const auto irPath = apvts.state.getProperty (dID::irPathProperty).toString();
            if (factoryIdx >= 0)
            {
                engine.loadFactoryIR (factoryIdx);
            }
            else if (irPath.isNotEmpty())
            {
                const juce::File f (irPath);
                if (f.existsAsFile())
                    engine.loadIR (f);
            }
        }
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DoobieAudioProcessor();
}
