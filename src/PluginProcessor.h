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

#include <juce_audio_processors/juce_audio_processors.h>
#include "dsp/DubDelayEngine.h"
#include "presets/PresetManager.h"

// Top-level plugin. Owns the parameter tree, resolves tempo-synced delay times
// from the host transport, converts raw parameters into engine units, and hands
// audio to the DubDelayEngine.
class DoobieAudioProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener
{
public:
    DoobieAudioProcessor();
    ~DoobieAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Doobie"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }
    PresetManager& getPresetManager() { return presetManager; }

    // Convolution IR control (called from the editor's LOAD / CLEAR buttons and
    // from setStateInformation when restoring a session). Loading is async via
    // JUCE's Convolution background loader, so safe to call while audio runs.
    void loadIR (const juce::File& f);
    void clearIR();
    bool hasIR() const            { return engine.hasIR(); }
    juce::File getLoadedIR() const { return engine.getLoadedIRFile(); }

    // Factory (built-in) IR selection. Factory IRs are synthesised at runtime
    // (see FactoryIRs.h) so they cost no binary and have no third-party
    // licensing. Index -1 means "no factory IR loaded".
    void         loadFactoryIR (int index);
    int          getFactoryIRIndex() const  { return engine.getFactoryIRIndex(); }
    juce::String getIRDisplayName() const   { return engine.getIRDisplayName(); }
    bool         irIsFactory() const        { return engine.irIsFactory(); }
    bool         irIsFile() const           { return engine.irIsFile(); }

    // For the editor's echo visualiser.
    const doobie::DubDelayEngine& getEngine() const { return engine; }
    double getCurrentBpm() const { return currentBpm.load(); }
    double getSampleRateForUI() const { return sampleRate; }

    // Post-processing output level per channel (0..1), for the VU meters.
    float getOutputLevel (int channel) const
    {
        return outputLevel[(size_t) juce::jlimit (0, 1, channel)].load (std::memory_order_relaxed);
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateEngineParams();

    // IR-speed changes need to re-load the IR with a different effective
    // sample rate. That allocates inside JUCE Convolution, so it runs from
    // the message thread via this listener rather than from processBlock.
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    juce::AudioProcessorValueTreeState apvts;
    doobie::DubDelayEngine engine;
    PresetManager presetManager;

    std::atomic<double> currentBpm { 120.0 };
    std::array<std::atomic<float>, 2> outputLevel { };
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoobieAudioProcessor)
};
