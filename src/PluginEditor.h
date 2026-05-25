#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "ui/LookAndFeel.h"
#include "ui/VuMeter.h"
#include "ui/EchoVisualiser.h"
#include "ui/SpringTankView.h"

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

    DoobieAudioProcessor& audioProcessor;
    doobie::DoobieLookAndFeel lnf;

    // Header / presets.
    juce::ComboBox  presetBox;
    juce::TextButton btnPrev { "<" }, btnNext { ">" }, btnSave { "SAVE" };

    // Delay.
    Knob kMode, kTime, kFeedback;
    Combo cbDivision;
    juce::ToggleButton tgSync { "Sync" }, tgPingPong { "Ping-Pong" }, tgFreeze { "Freeze" };
    std::unique_ptr<APVTS::ButtonAttachment> aSync, aPingPong, aFreeze;
    juce::Label modeReadout;

    // Multi-head.
    std::array<Knob, 4> headLevel, headPan, headRatio;

    // Tape character + tone.
    Knob kWow, kFlutter, kSat, kAge;
    Knob kBass, kTreble, kLowCut, kHighCut;

    // Reverb.
    Combo cbReverbMode, cbReverbRoute;
    Knob kRevMix, kSpringDecay, kSpringTone, kPlateDecay, kPlateSize, kPlateDamp, kPlatePre, kRevMod;

    // Output.
    Knob kInput, kMix, kOutput, kWidth, kDuck;

    // Panel rectangles shared between paint() and resized().
    juce::Rectangle<int> rHeader, rMode, rHeads, rDelay, rTape, rTone, rReverb, rOutput;

    // Visualisers.
    doobie::EchoVisualiser  echoView;
    doobie::SpringTankView  springView;
    doobie::VuMeter vuL { "L" }, vuR { "R" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DoobieAudioProcessorEditor)
};
