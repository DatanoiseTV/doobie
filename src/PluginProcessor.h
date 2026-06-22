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
#include "dsp/Lfo.h"
#include "dsp/EnvelopeFollower.h"
#include "dsp/ModMatrix.h"
#include "presets/PresetManager.h"
#include <unordered_map>

// Top-level plugin. Owns the parameter tree, resolves tempo-synced delay times
// from the host transport, converts raw parameters into engine units, and hands
// audio to the DubDelayEngine.
class DoobieAudioProcessor : public juce::AudioProcessor,
                             private juce::AudioProcessorValueTreeState::Listener,
                             private juce::ValueTree::Listener
{
public:
    // True when any APVTS param has changed since the last preset load
    // or user-save. Cleared by the preset manager's post-load hook.
    bool isCurrentPresetDirty() const { return presetDirty.load (std::memory_order_relaxed); }
    void clearPresetDirty()           { presetDirty.store (false, std::memory_order_relaxed); }
    DoobieAudioProcessor();
    ~DoobieAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Doobie"; }
    bool acceptsMidi() const override  { return true; }
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
    // Per-stage levels for the WebView meter bridge (IN / DELAY / REVERB).
    // IN is real (peak of input before engine.process); DELAY + REVERB are
    // weighted by mix / reverbMix so they reflect what the user perceives.
    float getInputLevel()  const { return inputLevel.load  (std::memory_order_relaxed); }
    float getDelayLevel()  const { return delayLevel.load  (std::memory_order_relaxed); }

    // Most recent MIDI note-on. Surfaced to the WebView so the user can see
    // whether MIDI is reaching the plugin at all (diagnostic when wiring up
    // a MIDI source in the host). -1 if no note has been received yet.
    int getLastMidiNote() const { return lastMidiNote.load (std::memory_order_relaxed); }

    // Output leveler's current gain-reduction in dB (negative = reducing).
    // Surfaced to the main VU meters so the user can see what auto-gain
    // is doing.
    float getOutputGrDb() const { return engine.getLeveler().getGainReductionDb(); }

    // Convolution IR: returns a downsampled stereo envelope (min/max pairs)
    // suitable for waveform display. binCount target = output bins. Returns
    // {samples, sampleRate, numChannels, lengthSeconds} so the UI can label
    // the time axis correctly.
    struct IrThumb {
        std::vector<float> peakL;   // per-bin absolute peak, 0..1
        std::vector<float> peakR;   // empty if mono
        double sampleRate = 0.0;
        int    numChannels = 0;
        int    numSamples = 0;
        float  lengthSec  = 0.0f;
    };
    IrThumb getIrThumbnail (int binCount = 256) const;
    float getReverbLevel() const { return reverbLevel.load (std::memory_order_relaxed); }

    // Latest LFO and envelope-follower values, for UI metering. Updated once
    // per audio block; read by the editor on its timer.
    float getLfo1Value() const   { return lfo1ValueUI.load   (std::memory_order_relaxed); }
    float getLfo2Value() const   { return lfo2ValueUI.load   (std::memory_order_relaxed); }
    float getLfo3Value() const   { return lfo3ValueUI.load   (std::memory_order_relaxed); }
    float getLfo4Value() const   { return lfo4ValueUI.load   (std::memory_order_relaxed); }
    float getEnvValue() const    { return envValueUI.load    (std::memory_order_relaxed); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    void updateEngineParams();

    // IR-speed changes need to re-load the IR with a different effective
    // sample rate. That allocates inside JUCE Convolution, so it runs from
    // the message thread via this listener rather than from processBlock.
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    // ValueTree::Listener — fires on any APVTS state change. Used to flag
    // the current preset as "dirty" so the UI can show an asterisk + ask
    // the user before they overwrite.
    void valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& prop) override;

    // IR Speed reload is expensive (it resamples the cached IR through a
    // JUCE Convolution rebuild). Doing that on every parameter tick during
    // a slider drag crackles audibly. The APVTS listener stores the latest
    // requested speed and arms a one-shot timer; the timer fires ~120 ms
    // after the LAST change and applies it once. Live IR Speed knob feel
    // stays smooth because the displayed value follows the param
    // immediately — only the convolver reload waits.
    struct IrSpeedDebouncer : private juce::Timer
    {
        DoobieAudioProcessor& owner;
        std::atomic<float>    pending { 1.0f };
        explicit IrSpeedDebouncer (DoobieAudioProcessor& o) : owner (o) {}
        void request (float v)  { pending.store (v, std::memory_order_relaxed); startTimer (120); }
        void timerCallback() override
        {
            stopTimer();
            owner.engine.setIRSpeed (pending.load (std::memory_order_relaxed));
        }
    };
    IrSpeedDebouncer irSpeedDebouncer { *this };
    std::atomic<bool> presetDirty { false };
    std::atomic<bool> suppressPresetDirty { false };
    // Snapshot of every parameter's normalised value at the last
    // load/save. The dirty flag is computed by diffing the live state
    // against this — "any state change" was too aggressive: smoothers
    // and the web-relay initial syncs after a load all fired the value-
    // tree listener and falsely re-marked the preset as dirty.
    std::unordered_map<juce::String, float> cleanSnapshot;
    void snapshotCurrentParams();
    bool currentMatchesSnapshot() const;

    juce::AudioProcessorValueTreeState apvts;
    doobie::DubDelayEngine engine;
    PresetManager presetManager;

    // Modulation: two LFOs, one envelope follower (fed the dry input on each
    // processBlock), four mod slots. See dsp/ModMatrix.h.
    doobie::Lfo              lfo1, lfo2, lfo3, lfo4;
    doobie::EnvelopeFollower envFollower;
    std::array<doobie::ModSlot, doobie::kNumModSlots> modSlots;

    // Building EngineParams is shared between processBlock (which adds the
    // mod-matrix overlay before sending) and other callers (state restore).
    doobie::EngineParams buildEngineParams();

    std::atomic<double> currentBpm { 120.0 };
    std::array<std::atomic<float>, 2> outputLevel { };
    // Per-stage levels for the WebView meter bridge.
    std::atomic<float> inputLevel  { 0.0f };
    std::atomic<float> delayLevel  { 0.0f };
    std::atomic<float> reverbLevel { 0.0f };
    // Latest mod-source values published from processBlock for UI metering.
    std::atomic<float> lfo1ValueUI { 0.0f }, lfo2ValueUI { 0.0f };
    std::atomic<float> lfo3ValueUI { 0.0f }, lfo4ValueUI { 0.0f };
    std::atomic<float> envValueUI  { 0.0f };
    // Most recent MIDI note-on; used by the "MIDI pitch mode" toggle to drive
    // the shimmer + delay pitch intervals from a keyboard. C3 (MIDI 60) is
    // the reference; semitone offset from 60 becomes the interval. Default
    // 72 (C4) so the engine sits at +12 st (the historical octave-up) until
    // the first note arrives.
    std::atomic<int> lastMidiNote { -1 };
    // MIDI pitch-bend (±2 st GM range mapped here). Last value seen on
    // the MidiBuffer; only matters while midiPitchMode is active.
    float pitchBendSemis = 0.0f;
    // Glided semitone value tracking lastMidiNote-60 over `midiPortaMs`.
    // Re-anchored from the first note we see so it doesn't snap from -60
    // at startup; updated each block in processBlock.
    float portaSemis = 0.0f;
    bool  portaInit  = false;
    double sampleRate = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoobieAudioProcessor)
};
