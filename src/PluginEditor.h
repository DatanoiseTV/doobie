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
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/VuMeter.h"
#include "ui/EchoVisualiser.h"
#include "ui/ReverbResponseView.h"
#include "ui/ModSourceMeter.h"
#include "ui/VectorCassette.h"

// The main plugin window: a vintage tape-echo front panel laid out in zones
// (header, mode + heads, delay + visualiser, tape/tone, reverb, output).
class DoobieAudioProcessorEditor : public juce::AudioProcessorEditor,
                                   private juce::Timer
{
public:
    explicit DoobieAudioProcessorEditor (DoobieAudioProcessor&);
    ~DoobieAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    // A rotary knob with a caption and a drag-popup value readout.
    struct Knob
    {
        juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
        juce::Label  caption;
        std::unique_ptr<APVTS::SliderAttachment> attachment;

        void attach (juce::Component& parent, APVTS& state, const juce::String& id,
                     const juce::String& text, juce::Colour accent);
        void place (juce::Rectangle<int> area);
    };

    struct Combo
    {
        juce::ComboBox box;
        juce::Label    caption;
        std::unique_ptr<APVTS::ComboBoxAttachment> attachment;

        void attach (juce::Component& parent, APVTS& state, const juce::String& id,
                     const juce::String& text, const juce::StringArray& items);
        void place (juce::Rectangle<int> area);
    };

    void timerCallback() override;
    void refreshPresetBox();
    void showSaveDialog();

    // Popup readout for a head TIME knob: a musical division in sync mode, a
    // fraction of the repeat when free-running.
    juce::String headTimeText (double value) const;

    DoobieAudioProcessor& audioProcessor;
    doobie::DoobieLookAndFeel lnf;

    // Header / presets.
    juce::ComboBox  presetBox;
    juce::TextButton btnPrev { "<" }, btnNext { ">" }, btnSave { "SAVE" };

    // Delay.
    Knob kTime, kFeedback;
    Combo cbDivision, cbCharacter;
    juce::ToggleButton tgSync { "Sync" }, tgPingPong { "Ping-Pong" }, tgFreeze { "Freeze" }, tgBypass { "Bypass" };
    std::unique_ptr<APVTS::ButtonAttachment> aSync, aPingPong, aFreeze, aBypass;

    // Head matrix: four lit pads switching each playback head on/off.
    std::array<juce::ToggleButton, 4> headPads;
    std::array<std::unique_ptr<APVTS::ButtonAttachment>, 4> headPadAtt;

    // Multi-head.
    std::array<Knob, 4> headLevel, headPan, headRatio;

    // Tape character.
    Knob kWow, kFlutter, kSat, kAge;
    // Two independent filter stages, each with HP/LP cuts and bass/treble shelves.
    Knob kPreHp, kPreLp, kPreBass, kPreTreble;  // input (pre-delay)
    Knob kLowCut, kHighCut, kBass, kTreble;     // feedback (every repeat)

    // Reverb.
    Combo cbReverbMode, cbReverbRoute;
    Knob kRevMix, kSpringDecay, kSpringTone, kPlateDecay, kPlateSize, kPlateDamp, kPlatePre, kRevMod;
    // Convolution-mode knobs that swap into the kRevMod / kPlatePre slots when
    // REVERB == Convolution (those algorithmic knobs do nothing in that mode).
    Knob kIrGain, kIrSpeed;

    // Convolution IR controls — visible only when REVERB == Convolution.
    // The combo selects a built-in IR ("Small Room", "Hall", "Cathedral", ...)
    // and the LOAD CUSTOM... button opens a file chooser to use a user IR.
    juce::ComboBox   cbFactoryIr;
    juce::TextButton btnLoadIr   { "LOAD CUSTOM..." };
    juce::TextButton btnClearIr  { "X" };
    juce::Label      irLabel;
    std::unique_ptr<juce::FileChooser> irChooser;

    // Modulation matrix: 2 LFOs + envelope follower visible in the main panel,
    // slot matrix opens in a popup behind the MATRIX button.
    Knob  kLfo1Rate, kLfo1Depth, kLfo2Rate, kLfo2Depth;
    Combo cbLfo1Wave, cbLfo2Wave;
    Knob  kEnvAttack, kEnvRelease, kEnvSens;
    juce::TextButton btnMatrix { "MATRIX..." };
    std::unique_ptr<doobie::ModSourceMeter> lfo1Meter, lfo2Meter, envMeter;

    // Output.
    Knob kInput, kMix, kOutput, kWidth, kDuck;

    // Panel rectangles shared between paint() and resized().
    juce::Rectangle<int> rHeader, rMode, rHeads, rDelay, rTape, rFilters, rReverb, rMod, rOutput;

    // Visualisers. Cassette replaces the echo lane visualiser at the top of
    // the DELAY panel (1:1 port from the Recordy project, attribution in
    // VectorCassette.h); echoView is kept declared for now in case a future
    // iteration brings the lane view back as a second tab.
    doobie::EchoVisualiser     echoView;
    doobie::VectorCassette     cassetteView;
    doobie::ReverbResponseView reverbView;
    doobie::VuMeter vuL { "L" }, vuR { "R" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoobieAudioProcessorEditor)
};
