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
#include "ui/WebEditor.h"
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
    {
        // 0.5..8000 ms covers flanger (sub-millisecond up to ~10 ms),
        // chorus (10..30 ms), slapback (~100 ms) and the long dub-delay
        // territory in one log-skewed knob. The skew is set so 100 ms sits
        // at the centre of the dial.
        Range timeRange (0.5f, 8000.0f, 0.01f);
        timeRange.setSkewForCentre (100.0f);
        layout.add (std::make_unique<FloatParam> (pid (dID::timeMs), "Time", timeRange, 375.0f));
    }
    layout.add (std::make_unique<ChoiceParam> (pid (dID::syncDiv), "Division", dID::syncDivChoices, 10));
    layout.add (std::make_unique<FloatParam> (pid (dID::feedback), "Feedback", Range (0.0f, 1.2f, 0.001f), 0.4f));
    layout.add (std::make_unique<BoolParam> (pid (dID::pingPong), "Ping-Pong", false));
    layout.add (std::make_unique<BoolParam> (pid (dID::freeze), "Freeze", false));
    layout.add (std::make_unique<BoolParam> (pid (dID::delayBypass), "Delay Bypass", false));
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
    // Gated-reverb controls. Threshold log-skewed toward sensitive end where
    // most users live (a snare hit at -28 dBFS is the canonical 80s setting).
    {
        Range gtRange (-60.0f, 0.0f, 0.1f);
        layout.add (std::make_unique<FloatParam> (pid (dID::gateThreshold), "Gate Threshold", gtRange, -28.0f));
    }
    {
        Range gholdRange (1.0f, 1000.0f, 0.5f); gholdRange.setSkewForCentre (120.0f);
        layout.add (std::make_unique<FloatParam> (pid (dID::gateHold),    "Gate Hold",    gholdRange, 180.0f));
    }
    {
        Range grelRange (0.5f, 500.0f, 0.1f); grelRange.setSkewForCentre (30.0f);
        layout.add (std::make_unique<FloatParam> (pid (dID::gateRelease), "Gate Release", grelRange, 6.0f));
    }

    // ---- Modulation matrix --------------------------------------------------
    {
        // 0.001..20 Hz: at the slow end one cycle takes ~17 minutes (multi-
        // minute pad-style sweeps), at the fast end 50 ms (tremolo / fast
        // flanger LFO). 1 Hz sits at the centre of the dial.
        Range lfoRateRange  (0.001f, 20.0f, 0.0001f);
        lfoRateRange.setSkewForCentre (1.0f);
        Range envAtkRange   (0.1f, 500.0f, 0.1f);
        envAtkRange.setSkewForCentre (20.0f);
        Range envRelRange   (1.0f, 2000.0f, 0.1f);
        envRelRange.setSkewForCentre (150.0f);

        layout.add (std::make_unique<FloatParam>  (pid (dID::lfo1Rate),   "LFO 1 Rate",   lfoRateRange, 1.0f));
        layout.add (std::make_unique<FloatParam>  (pid (dID::lfo1Depth),  "LFO 1 Depth",  Range (0.0f, 1.0f, 0.001f), 0.5f));
        layout.add (std::make_unique<ChoiceParam> (pid (dID::lfo1Wave),   "LFO 1 Wave",   dID::lfoWaveChoices, 0));
        layout.add (std::make_unique<FloatParam>  (pid (dID::lfo2Rate),   "LFO 2 Rate",   lfoRateRange, 0.5f));
        layout.add (std::make_unique<FloatParam>  (pid (dID::lfo2Depth),  "LFO 2 Depth",  Range (0.0f, 1.0f, 0.001f), 0.5f));
        layout.add (std::make_unique<ChoiceParam> (pid (dID::lfo2Wave),   "LFO 2 Wave",   dID::lfoWaveChoices, 1));
        layout.add (std::make_unique<FloatParam>  (pid (dID::envAttack), "Env Attack",   envAtkRange, 10.0f));
        layout.add (std::make_unique<FloatParam>  (pid (dID::envRelease),"Env Release",  envRelRange, 200.0f));
        layout.add (std::make_unique<FloatParam>  (pid (dID::envSens),   "Env Sensitivity", Range (-24.0f, 24.0f, 0.1f), 0.0f));

        const juce::StringArray sourceChoices = doobie::modSourceNames();
        const juce::StringArray destChoices   = doobie::modDestNames();
        for (int i = 0; i < doobie::kNumModSlots; ++i)
        {
            const auto n = juce::String (i + 1);
            layout.add (std::make_unique<ChoiceParam> (pid (dID::modSlotSrc[(size_t) i]), "Mod " + n + " Source",      sourceChoices, 0));
            layout.add (std::make_unique<ChoiceParam> (pid (dID::modSlotDst[(size_t) i]), "Mod " + n + " Destination", destChoices,   0));
            layout.add (std::make_unique<FloatParam>  (pid (dID::modSlotAmt[(size_t) i]), "Mod " + n + " Amount",      Range (-1.0f, 1.0f, 0.001f), 0.0f));
        }
    }

    // ---- IR controls (Convolution mode) -------------------------------------
    // Gain compensates for peak-normalised IRs that sound quiet against the
    // dry signal; speed lies about the source sample rate to stretch / squash
    // the IR through JUCE's resampler — a -2 / +2 octave IR-playback effect.
    {
        Range irSpeedRange (0.25f, 4.0f, 0.001f);
        irSpeedRange.setSkewForCentre (1.0f);
        layout.add (std::make_unique<FloatParam> (pid (dID::irGain),  "IR Gain",  Range (-24.0f, 24.0f, 0.1f), 6.0f));
        layout.add (std::make_unique<FloatParam> (pid (dID::irSpeed), "IR Speed", irSpeedRange, 1.0f));
    }

    return layout;
}

DoobieAudioProcessor::DoobieAudioProcessor()
    : AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createParameterLayout()),
      presetManager (apvts)
{
    apvts.addParameterListener (dID::irSpeed, this);
}

DoobieAudioProcessor::~DoobieAudioProcessor()
{
    apvts.removeParameterListener (dID::irSpeed, this);
}

void DoobieAudioProcessor::parameterChanged (const juce::String& parameterID, float newValue)
{
    // Called on the message thread when irSpeed moves. Reload the IR at the
    // new effective sample rate so the rest of the chain hears the new length.
    if (parameterID == dID::irSpeed)
        engine.setIRSpeed (newValue);
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
    lfo1.prepare (sr);
    lfo2.prepare (sr);
    envFollower.prepare (sr);
    updateEngineParams();
    engine.prepare (sr, samplesPerBlock);
}

doobie::EngineParams DoobieAudioProcessor::buildEngineParams()
{
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    doobie::EngineParams p;
    p.inputGain = juce::Decibels::decibelsToGain (raw (dID::inputDrive));
    p.outGain   = juce::Decibels::decibelsToGain (raw (dID::outputGain));
    p.mix       = raw (dID::mix);
    p.width     = raw (dID::width);
    p.feedback  = raw (dID::feedback);
    p.delayMode = (int) raw (dID::delayMode);
    p.pingPong    = raw (dID::pingPong) > 0.5f;
    p.freeze      = raw (dID::freeze) > 0.5f;
    p.delayBypass = raw (dID::delayBypass) > 0.5f;
    p.duck        = raw (dID::duck);

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

    p.irGain      = juce::Decibels::decibelsToGain (raw (dID::irGain));
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

    p.gateThresholdDb = raw (dID::gateThreshold);
    p.gateHoldMs      = raw (dID::gateHold);
    p.gateReleaseMs   = raw (dID::gateRelease);

    return p;
}

void DoobieAudioProcessor::updateEngineParams()
{
    engine.setParams (buildEngineParams());
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

    // --- Modulation matrix --------------------------------------------------
    // Pull LFO / env / slot settings from APVTS, run the env follower over the
    // dry input before the engine mutates the buffer, advance the LFOs to get
    // this block's mod-source values, then overlay them on the base
    // EngineParams. The engine's own per-sample smoothers ramp toward the
    // modulated targets, so even fast modulation stays click-free.
    auto raw = [this] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    lfo1.setRate (raw (dID::lfo1Rate));
    lfo1.setDepth (raw (dID::lfo1Depth));
    lfo1.setWaveform ((doobie::Lfo::Wave) (int) raw (dID::lfo1Wave));
    lfo2.setRate (raw (dID::lfo2Rate));
    lfo2.setDepth (raw (dID::lfo2Depth));
    lfo2.setWaveform ((doobie::Lfo::Wave) (int) raw (dID::lfo2Wave));
    envFollower.setAttack (raw (dID::envAttack));
    envFollower.setRelease (raw (dID::envRelease));
    envFollower.setSensitivity (raw (dID::envSens));

    const int numSamples = buffer.getNumSamples();
    if (numSamples > 0 && buffer.getNumChannels() > 0)
    {
        const float* L = buffer.getReadPointer (0);
        const float* R = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : L;
        envFollower.processBlock (L, R, numSamples);
        // Pre-engine peak for the IN meter on the WebView UI.
        const float magL = buffer.getMagnitude (0, 0, numSamples);
        const float magR = buffer.getNumChannels() > 1 ? buffer.getMagnitude (1, 0, numSamples) : magL;
        inputLevel.store (juce::jmax (magL, magR), std::memory_order_relaxed);
    }

    const doobie::ModSourceValues sources {
        lfo1.advance (numSamples),
        lfo2.advance (numSamples),
        envFollower.value()
    };

    // Publish to the UI metering atomics. Editor reads these on a timer.
    lfo1ValueUI.store (sources.lfo1, std::memory_order_relaxed);
    lfo2ValueUI.store (sources.lfo2, std::memory_order_relaxed);
    envValueUI.store  (sources.env,  std::memory_order_relaxed);

    for (int i = 0; i < doobie::kNumModSlots; ++i)
    {
        modSlots[(size_t) i].source = (doobie::ModSource) (int) raw (dID::modSlotSrc[(size_t) i]);
        modSlots[(size_t) i].dest   = (doobie::ModDest)   (int) raw (dID::modSlotDst[(size_t) i]);
        modSlots[(size_t) i].amount = raw (dID::modSlotAmt[(size_t) i]);
    }

    auto p = buildEngineParams();
    doobie::applyModMatrix (p, modSlots, sources);
    engine.setParams (p);
    engine.process (buffer);

    // Capture output peaks for the VU meters.
    for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        outputLevel[(size_t) ch].store (buffer.getMagnitude (ch, 0, buffer.getNumSamples()),
                                        std::memory_order_relaxed);

    // Approximate stage levels for the meter bridge. The engine doesn't tap
    // intermediate signals (would cost an extra accumulator per stage), so
    // we infer them from the post-output peak weighted by the relevant
    // routing knobs. Not laboratory-grade but matches what the user hears:
    // delay activity tracks output × mix when delay is unbypassed; reverb
    // activity tracks output × reverbMix when the reverb is on.
    const float outMono = 0.5f * (outputLevel[0].load (std::memory_order_relaxed)
                                  + outputLevel[1].load (std::memory_order_relaxed));
    const float dlyOn = p.delayBypass ? 0.0f : 1.0f;
    delayLevel.store  (outMono * dlyOn * juce::jlimit (0.0f, 1.0f, p.mix), std::memory_order_relaxed);
    reverbLevel.store (outMono *         juce::jlimit (0.0f, 1.0f, p.reverbMix), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* DoobieAudioProcessor::createEditor()
{
    return new doobie::WebEditor (*this);
}

void DoobieAudioProcessor::loadIR (const juce::File& f)
{
    if (! f.existsAsFile())
        return;
    engine.loadIR (f);
    // Apply the current speed setting to the freshly-loaded IR.
    if (auto* p = apvts.getRawParameterValue (dID::irSpeed))
        engine.setIRSpeed (p->load());
    apvts.state.setProperty (dID::irPathProperty, f.getFullPathName(), nullptr);
    apvts.state.setProperty (dID::factoryIrIndexProperty, -1, nullptr); // file IR wins
}

void DoobieAudioProcessor::loadFactoryIR (int index)
{
    if (! engine.loadFactoryIR (index))
        return;
    if (auto* p = apvts.getRawParameterValue (dID::irSpeed))
        engine.setIRSpeed (p->load());
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
