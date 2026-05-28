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

#include "PluginEditor.h"
#include "ParameterIDs.h"
#include "ui/ModMatrixPopup.h"

using doobie::colours::amber;
using doobie::colours::teal;
using doobie::colours::cream;

// Total width of the header's preset cluster: [<] [combo] [>] [SAVE].
// Shared by paint() (to keep the version text clear of it) and resized().
static constexpr int kPresetClusterW = 30 + 4 + 170 + 4 + 30 + 8 + 62;

// ----------------------------------------------------------------------------
// Knob / Combo helpers
// ----------------------------------------------------------------------------
void DoobieAudioProcessorEditor::Knob::attach (juce::Component& parent, APVTS& state,
                                               const juce::String& id, const juce::String& text,
                                               juce::Colour accent)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setPopupDisplayEnabled (true, true, &parent);
    slider.getProperties().set ("accent", (int) accent.getARGB());
    parent.addAndMakeVisible (slider);

    caption.setText (text, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setColour (juce::Label::textColourId, cream().withAlpha (0.8f));
    caption.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.05f));
    parent.addAndMakeVisible (caption);

    attachment = std::make_unique<APVTS::SliderAttachment> (state, id, slider);
}

void DoobieAudioProcessorEditor::Knob::place (juce::Rectangle<int> area)
{
    caption.setBounds (area.removeFromBottom (15));
    slider.setBounds (area);
}

void DoobieAudioProcessorEditor::Combo::attach (juce::Component& parent, APVTS& state,
                                                const juce::String& id, const juce::String& text,
                                                const juce::StringArray& items)
{
    box.addItemList (items, 1);
    parent.addAndMakeVisible (box);

    caption.setText (text, juce::dontSendNotification);
    caption.setJustificationType (juce::Justification::centred);
    caption.setColour (juce::Label::textColourId, cream().withAlpha (0.8f));
    caption.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.05f));
    parent.addAndMakeVisible (caption);

    attachment = std::make_unique<APVTS::ComboBoxAttachment> (state, id, box);
}

void DoobieAudioProcessorEditor::Combo::place (juce::Rectangle<int> area)
{
    caption.setBounds (area.removeFromBottom (15));
    box.setBounds (area.withSizeKeepingCentre (area.getWidth(), juce::jmin (28, area.getHeight())));
}

// ----------------------------------------------------------------------------
// Editor
// ----------------------------------------------------------------------------
DoobieAudioProcessorEditor::DoobieAudioProcessorEditor (DoobieAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), echoView (p), reverbView (p)
{
    setLookAndFeel (&lnf);
    setOpaque (true);   // paint() fills the whole area; avoids host compositing artifacts
    auto& state = audioProcessor.getValueTreeState();

    // Presets.
    presetBox.setJustificationType (juce::Justification::centred);
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && name != audioProcessor.getPresetManager().getCurrentName())
            audioProcessor.getPresetManager().loadByName (name);
    };
    addAndMakeVisible (presetBox);

    for (auto* b : { &btnPrev, &btnNext, &btnSave })
        addAndMakeVisible (b);
    btnPrev.onClick = [this] { audioProcessor.getPresetManager().previous(); };
    btnNext.onClick = [this] { audioProcessor.getPresetManager().next(); };
    btnSave.onClick = [this] { showSaveDialog(); };

    // Delay block.
    kTime.attach     (*this, state, dID::timeMs,   "TIME",     amber());
    kFeedback.attach (*this, state, dID::feedback, "FEEDBACK", amber());

    // Show the TIME knob's value as a friendly string: "0.50 ms" for flanger
    // / chorus territory, "375 ms" for slapback / dub, "1.20 s" for long
    // tails. Sub-ms gets two decimal places so the dial reads meaningfully
    // down to 0.5 ms (the minimum since v0.10).
    kTime.slider.textFromValueFunction = [] (double ms)
    {
        if (ms < 1.0)    return juce::String (ms, 2) + " ms";
        if (ms < 1000.0) return juce::String ((int) std::round (ms)) + " ms";
        return juce::String (ms / 1000.0, 2) + " s";
    };
    kTime.slider.updateText();
    cbDivision.attach (*this, state, dID::syncDiv, "DIVISION", dID::syncDivChoices);
    cbCharacter.attach (*this, state, dID::delayMode, "CHARACTER", dID::delayModeChoices);

    for (auto* t : { &tgSync, &tgPingPong, &tgFreeze, &tgBypass })
        addAndMakeVisible (t);
    tgSync.getProperties().set     ("accent", (int) amber().getARGB());
    tgPingPong.getProperties().set ("accent", (int) amber().getARGB());
    tgFreeze.getProperties().set   ("accent", (int) doobie::colours::red().getARGB());
    tgBypass.getProperties().set   ("accent", (int) doobie::colours::red().getARGB());
    aSync     = std::make_unique<APVTS::ButtonAttachment> (state, dID::syncMode,    tgSync);
    aPingPong = std::make_unique<APVTS::ButtonAttachment> (state, dID::pingPong,    tgPingPong);
    aFreeze   = std::make_unique<APVTS::ButtonAttachment> (state, dID::freeze,      tgFreeze);
    aBypass   = std::make_unique<APVTS::ButtonAttachment> (state, dID::delayBypass, tgBypass);

    // Head matrix: one lit pad per head, switching it in or out of the echo.
    static const char* headLetters[4] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i)
    {
        auto& pad = headPads[(size_t) i];
        pad.setButtonText (headLetters[i]);
        pad.getProperties().set ("pad", true);
        pad.getProperties().set ("headLetter", headLetters[i]);
        pad.getProperties().set ("accent", (int) amber().getARGB());
        addAndMakeVisible (pad);
        headPadAtt[(size_t) i] = std::make_unique<APVTS::ButtonAttachment> (state, dID::headOn[(size_t) i], pad);
    }

    // Heads. Captions act as column headers only on the first row.
    static const char* heads[4] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i)
    {
        headLevel[(size_t) i].attach (*this, state, dID::headLevel[(size_t) i], heads[i], amber());
        headPan[(size_t) i].attach   (*this, state, dID::headPan[(size_t) i],   i == 0 ? "PAN"  : "", amber());
        headRatio[(size_t) i].attach (*this, state, dID::headRatio[(size_t) i], i == 0 ? "TIME" : "", amber());

        // Show the resolved division (sync) or fraction (free) in the drag popup.
        headRatio[(size_t) i].slider.textFromValueFunction = [this] (double v) { return headTimeText (v); };
        headRatio[(size_t) i].slider.updateText();
    }

    // Tape + tone.
    kWow.attach     (*this, state, dID::wow,     "WOW",     amber());
    kFlutter.attach (*this, state, dID::flutter, "FLUTTER", amber());
    kSat.attach     (*this, state, dID::drive,   "SAT",     amber());
    kAge.attach     (*this, state, dID::hiss,    "AGE",     amber());
    // Input filter (teal) and feedback filter (amber), same controls on each.
    kPreHp.attach     (*this, state, dID::preHpFreq, "LOW CUT",  teal());
    kPreLp.attach     (*this, state, dID::preLpFreq, "HIGH CUT", teal());
    kPreBass.attach   (*this, state, dID::preBass,   "BASS",     teal());
    kPreTreble.attach (*this, state, dID::preTreble, "TREBLE",   teal());
    kLowCut.attach   (*this, state, dID::hpFreq,  "LOW CUT",  amber());
    kHighCut.attach  (*this, state, dID::lpFreq,  "HIGH CUT", amber());
    kBass.attach     (*this, state, dID::bass,    "BASS",     amber());
    kTreble.attach   (*this, state, dID::treble,  "TREBLE",   amber());

    // Reverb (teal accents).
    cbReverbMode.attach  (*this, state, dID::reverbMode,  "REVERB", dID::reverbModeChoices);
    cbReverbRoute.attach (*this, state, dID::reverbRoute, "ROUTE",  dID::reverbRouteChoices);
    kRevMix.attach      (*this, state, dID::reverbMix,     "MIX",     teal());
    kSpringDecay.attach (*this, state, dID::springDecay,   "SPRING",  teal());
    kSpringTone.attach  (*this, state, dID::springTone,    "S.TONE",  teal());
    kPlateDecay.attach  (*this, state, dID::plateDecay,    "PLATE",   teal());
    kPlateSize.attach   (*this, state, dID::plateSize,     "SIZE",    teal());
    kPlateDamp.attach   (*this, state, dID::plateDamp,     "DAMP",    teal());
    kPlatePre.attach    (*this, state, dID::platePredelay, "PRE",     teal());
    kRevMod.attach      (*this, state, dID::reverbMod,     "MOD",     teal());
    kIrGain.attach      (*this, state, dID::irGain,        "IR GAIN", teal());
    kIrSpeed.attach     (*this, state, dID::irSpeed,       "IR SPEED", teal());

    // Output.
    // IR controls for the Convolution reverb mode.
    //   * cbFactoryIr  — picks a built-in synthesised IR or "(none)".
    //   * btnLoadIr    — opens a file chooser to load a custom WAV/AIFF/FLAC.
    //   * btnClearIr   — clears the current IR (any kind).
    //   * irLabel      — shows the currently-loaded IR name.
    cbFactoryIr.addItem ("(none)", 1);
    {
        const auto names = doobie::factoryIRNames();
        for (int i = 0; i < names.size(); ++i)
            cbFactoryIr.addItem (names[i], i + 2); // ids start at 1; reserve 1 for (none)
    }
    cbFactoryIr.setSelectedId (1, juce::dontSendNotification);
    addChildComponent (cbFactoryIr);
    addChildComponent (btnLoadIr);
    addChildComponent (btnClearIr);
    addChildComponent (irLabel);
    irLabel.setJustificationType (juce::Justification::centredLeft);
    irLabel.setColour (juce::Label::textColourId, teal());
    irLabel.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.04f));

    cbFactoryIr.onChange = [this]
    {
        const int id = cbFactoryIr.getSelectedId();
        if (id <= 1) audioProcessor.clearIR();
        else         audioProcessor.loadFactoryIR (id - 2);
    };

    btnLoadIr.onClick = [this]
    {
        irChooser = std::make_unique<juce::FileChooser> (
            "Choose an impulse response",
            juce::File::getSpecialLocation (juce::File::userMusicDirectory),
            "*.wav;*.aif;*.aiff;*.flac");
        irChooser->launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                const auto f = fc.getResult();
                if (f.existsAsFile())
                {
                    audioProcessor.loadIR (f);
                    // A custom file IR isn't in the factory list, so deselect.
                    cbFactoryIr.setSelectedId (0, juce::dontSendNotification);
                }
            });
    };
    btnClearIr.onClick = [this]
    {
        audioProcessor.clearIR();
        cbFactoryIr.setSelectedId (1, juce::dontSendNotification); // back to (none)
    };

    // ---- Modulation matrix ------------------------------------------------
    // Two LFOs, an envelope follower, and four slot rows that route any source
    // to any destination with a bipolar amount.
    kLfo1Rate.attach    (*this, state, dID::lfo1Rate,   "RATE",  teal());
    kLfo1Depth.attach   (*this, state, dID::lfo1Depth,  "DEPTH", teal());
    cbLfo1Wave.attach   (*this, state, dID::lfo1Wave,   "WAVE",  dID::lfoWaveChoices);
    kLfo2Rate.attach    (*this, state, dID::lfo2Rate,   "RATE",  teal());
    kLfo2Depth.attach   (*this, state, dID::lfo2Depth,  "DEPTH", teal());
    cbLfo2Wave.attach   (*this, state, dID::lfo2Wave,   "WAVE",  dID::lfoWaveChoices);

    // Friendly rate display: Hz for fast LFOs, seconds / minutes for the very
    // slow end (where reading "0.005 Hz" is less useful than "3.3 min").
    auto rateFormat = [] (double hz)
    {
        if (hz >= 1.0)  return juce::String (hz, 2) + " Hz";
        if (hz >= 0.1)  return juce::String (hz, 3) + " Hz";
        if (hz >= 0.01) return juce::String (hz, 4) + " Hz";
        const double periodSec = 1.0 / juce::jmax (hz, 1.0e-5);
        if (periodSec < 60.0) return juce::String (periodSec, 1) + " s";
        return juce::String (periodSec / 60.0, 1) + " min";
    };
    kLfo1Rate.slider.textFromValueFunction = rateFormat;
    kLfo2Rate.slider.textFromValueFunction = rateFormat;
    kLfo1Rate.slider.updateText();
    kLfo2Rate.slider.updateText();
    kEnvAttack.attach   (*this, state, dID::envAttack,  "ATK",   teal());
    kEnvRelease.attach  (*this, state, dID::envRelease, "REL",   teal());
    kEnvSens.attach     (*this, state, dID::envSens,    "SENS",  teal());

    // MATRIX button opens the slot configuration in a popup, keeping the main
    // mod panel focused on the always-on sources (LFOs + envelope follower).
    addAndMakeVisible (btnMatrix);
    btnMatrix.getProperties().set ("accent", (int) teal().getARGB());
    btnMatrix.onClick = [this]
    {
        auto popup = std::make_unique<doobie::ModMatrixPopup> (audioProcessor.getValueTreeState(), lnf);
        const auto trigger = btnMatrix.getScreenBounds();
        juce::CallOutBox::launchAsynchronously (std::move (popup), trigger, nullptr);
    };

    // Visual indicators that show what the mod sources are doing right now.
    lfo1Meter = std::make_unique<doobie::ModSourceMeter> (
        doobie::ModSourceMeter::Style::Bipolar,
        [this] { return audioProcessor.getLfo1Value(); }, teal());
    lfo2Meter = std::make_unique<doobie::ModSourceMeter> (
        doobie::ModSourceMeter::Style::Bipolar,
        [this] { return audioProcessor.getLfo2Value(); }, teal());
    envMeter = std::make_unique<doobie::ModSourceMeter> (
        doobie::ModSourceMeter::Style::Unipolar,
        [this] { return audioProcessor.getEnvValue(); }, amber());
    addAndMakeVisible (lfo1Meter.get());
    addAndMakeVisible (lfo2Meter.get());
    addAndMakeVisible (envMeter.get());

    kInput.attach  (*this, state, dID::inputDrive, "INPUT",  amber());
    kMix.attach    (*this, state, dID::mix,        "MIX",    amber());
    kOutput.attach (*this, state, dID::outputGain, "OUTPUT", amber());
    kWidth.attach  (*this, state, dID::width,      "WIDTH",  teal());
    kDuck.attach   (*this, state, dID::duck,       "DUCK",   amber());

    // Visualisers + meters.
    addAndMakeVisible (cassetteView);
    // echoView is kept as a member but not added to the layout for now —
    // the cassette replaces it at the top of the DELAY panel (1:1 port from
    // Recordy). Re-instate by swapping bounds in resized() if needed.
    addAndMakeVisible (reverbView);
    addAndMakeVisible (vuL);
    addAndMakeVisible (vuR);
    vuL.setLevelSource ([this] { return audioProcessor.getOutputLevel (0); });
    vuR.setLevelSource ([this] { return audioProcessor.getOutputLevel (1); });

    refreshPresetBox();
    timerCallback();          // initialise dynamic labels / enabled state at once
    startTimerHz (12);
    setSize (1100, 960);
}

DoobieAudioProcessorEditor::~DoobieAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
}

juce::String DoobieAudioProcessorEditor::headTimeText (double value) const
{
    auto& state = audioProcessor.getValueTreeState();
    const bool synced = state.getRawParameterValue (dID::syncMode)->load() > 0.5f;
    if (! synced)
        return juce::String (value, 2); // continuous fraction of the repeat

    const int divIdx = juce::jlimit (0, (int) dID::syncDivQuarters.size() - 1,
                                     (int) state.getRawParameterValue (dID::syncDiv)->load());
    const double masterQ  = dID::syncDivQuarters[(size_t) divIdx];
    const double snappedQ = dID::snapHeadQuarters (value * masterQ, masterQ);
    for (int i = 0; i < dID::headDivNames.size(); ++i)
        if (std::abs (dID::headDivQuarters[(size_t) i] - snappedQ) < 1.0e-4)
            return dID::headDivNames[i];
    return dID::syncDivChoices[divIdx]; // head equals the repeat division
}

void DoobieAudioProcessorEditor::refreshPresetBox()
{
    presetBox.clear (juce::dontSendNotification);
    presetBox.addItemList (audioProcessor.getPresetManager().getAllNames(), 1);
    presetBox.setText (audioProcessor.getPresetManager().getCurrentName(), juce::dontSendNotification);
}

void DoobieAudioProcessorEditor::showSaveDialog()
{
    auto* w = new juce::AlertWindow ("Save Preset", "Name this preset:",
                                     juce::MessageBoxIconType::NoIcon);
    w->addTextEditor ("name", "My Preset");
    w->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int result)
    {
        if (result == 1)
        {
            const auto name = w->getTextEditorContents ("name").trim();
            if (name.isNotEmpty())
            {
                audioProcessor.getPresetManager().saveUser (name);
                refreshPresetBox();
            }
        }
    }), true);
}

void DoobieAudioProcessorEditor::timerCallback()
{
    auto& state = audioProcessor.getValueTreeState();

    const bool synced = state.getRawParameterValue (dID::syncMode)->load() > 0.5f;
    cbDivision.box.setEnabled (synced);
    kTime.slider.setEnabled (! synced);
    cbDivision.box.setAlpha (synced ? 1.0f : 0.4f);
    kTime.slider.setAlpha (synced ? 0.4f : 1.0f);

    // Convolution mode swaps the reverb decay view for an IR loader. The
    // algorithmic knobs stay visible but are greyed since they do nothing here.
    const bool conv = state.getRawParameterValue (dID::reverbMode)->load() > 6.5f;
    cbFactoryIr.setVisible (conv);
    btnLoadIr.setVisible (conv);
    btnClearIr.setVisible (conv && audioProcessor.hasIR());
    irLabel.setVisible (conv);
    reverbView.setVisible (! conv);
    if (conv)
    {
        const juce::String name = audioProcessor.getIRDisplayName();
        irLabel.setText (audioProcessor.hasIR() ? name : "(no IR loaded)",
                         juce::dontSendNotification);
        // Reflect the engine's current selection so a session-restored factory
        // IR or programmatic load shows in the combo without re-triggering its
        // onChange.
        int target = 1; // (none)
        if (audioProcessor.irIsFactory()) target = 2 + audioProcessor.getFactoryIRIndex();
        else if (audioProcessor.irIsFile()) target = 0; // deselected (custom isn't in the list)
        if (cbFactoryIr.getSelectedId() != target)
            cbFactoryIr.setSelectedId (target, juce::dontSendNotification);
    }
    const float convAlpha = conv ? 0.4f : 1.0f;
    for (auto* k : { &kSpringDecay, &kSpringTone, &kPlateDecay, &kPlateSize, &kPlateDamp })
    {
        k->slider.setEnabled (! conv);
        k->slider.setAlpha (convAlpha);
    }
    // The MOD / PRE slots host the convolution-mode IR GAIN / IR SPEED knobs —
    // swap visibility instead of just greying.
    for (auto* k : { &kRevMod, &kPlatePre })
    {
        k->slider.setVisible (! conv);
        k->caption.setVisible (! conv);
    }
    for (auto* k : { &kIrGain, &kIrSpeed })
    {
        k->slider.setVisible (conv);
        k->caption.setVisible (conv);
    }

    // Keep the preset box reflecting external/program changes.
    const auto current = audioProcessor.getPresetManager().getCurrentName();
    if (presetBox.getText() != current)
        presetBox.setText (current, juce::dontSendNotification);
}

// ----------------------------------------------------------------------------
// Painting
// ----------------------------------------------------------------------------
void DoobieAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Brushed-metal panel: vertical gradient with faint horizontal grain.
    juce::ColourGradient bg (doobie::colours::panel().brighter (0.04f), 0.0f, 0.0f,
                             doobie::colours::panel().darker (0.18f), 0.0f, (float) getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (getLocalBounds());

    g.setColour (juce::Colours::black.withAlpha (0.04f));
    for (int y = 0; y < getHeight(); y += 3)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    // Header bar.
    g.setColour (doobie::colours::panelShadow());
    g.fillRect (rHeader);
    g.setColour (doobie::colours::line());
    g.drawLine (0.0f, (float) rHeader.getBottom(), (float) getWidth(), (float) rHeader.getBottom(), 1.0f);

    auto header = rHeader.reduced (16, 0);
    g.setColour (amber());
    g.setFont (juce::Font (juce::FontOptions (30.0f)).withExtraKerningFactor (0.08f).boldened());
    g.drawText ("DOOBIE", header.removeFromLeft (175), juce::Justification::centredLeft);
    g.setColour (cream().withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (12.5f)).withExtraKerningFactor (0.18f));
    g.drawText ("ANALOG DUB DELAY", header.removeFromLeft (190), juce::Justification::centredLeft);

    // Version sits in the gap to the LEFT of the preset cluster (kPresetClusterW
    // must match resized()), so it never overlaps the combo / SAVE button.
    header.removeFromRight (kPresetClusterW + 12);
    g.setColour (cream().withAlpha (0.30f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)));
    g.drawText (juce::String ("v") + DOOBIE_VERSION + "  " + DOOBIE_GIT_BRANCH + "@" + DOOBIE_GIT_HASH,
                header.removeFromRight (210), juce::Justification::centredRight);

    // Section panels.
    auto panel = [&] (juce::Rectangle<int> r, const juce::String& title, juce::Colour border)
    {
        g.setColour (doobie::colours::panelLight());
        g.fillRoundedRectangle (r.toFloat(), 6.0f);
        g.setColour (border);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 6.0f, 1.2f);
        g.setColour (cream().withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.14f).boldened());
        g.drawText (title, r.withTrimmedLeft (12).removeFromTop (24), juce::Justification::centredLeft);

        // Corner screws.
        for (auto corner : { r.getTopLeft(), r.getTopRight(), r.getBottomLeft(), r.getBottomRight() })
        {
            const juce::Point<float> c ((float) corner.x + (corner.x == r.getX() ? 7.0f : -7.0f),
                                        (float) corner.y + (corner.y == r.getY() ? 7.0f : -7.0f));
            g.setColour (doobie::colours::panelShadow());
            g.fillEllipse (c.x - 3.0f, c.y - 3.0f, 6.0f, 6.0f);
            g.setColour (doobie::colours::line());
            g.drawLine (c.x - 2.0f, c.y, c.x + 2.0f, c.y, 1.0f);
        }
    };

    panel (rMode,    "HEAD MATRIX", doobie::colours::line());
    panel (rHeads,   "PLAYBACK HEADS", doobie::colours::line());
    panel (rDelay,   "DELAY",  doobie::colours::line());
    panel (rTape,    "TAPE",   doobie::colours::line());
    panel (rFilters, "FILTERS", doobie::colours::line());
    panel (rReverb,  "REVERB", teal().withAlpha (0.6f));
    panel (rMod,     "MODULATION", teal().withAlpha (0.6f));
    panel (rOutput,  "OUTPUT", doobie::colours::line());

    // Sub-headers and dividers inside the MOD panel. The MATRIX... button on
    // the right opens a popup with the slot configuration; the three left
    // sub-sections (LFO 1 / LFO 2 / ENV) are always visible.
    {
        auto m = rMod.reduced (10).withTrimmedTop (24);
        m.removeFromRight (110); // matrix-button column
        const int thirdW = m.getWidth() / 3;
        auto lfo1Area = m.removeFromLeft (thirdW);
        auto lfo2Area = m.removeFromLeft (thirdW);
        auto envArea  = m;

        g.setColour (doobie::colours::line().withAlpha (0.6f));
        g.drawVerticalLine (lfo2Area.getX(), (float) lfo2Area.getY() + 4.0f, (float) lfo2Area.getBottom() - 4.0f);
        g.drawVerticalLine (envArea.getX(),  (float) envArea.getY()  + 4.0f, (float) envArea.getBottom()  - 4.0f);

        g.setColour (teal().withAlpha (0.85f));
        g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.12f));
        g.drawText ("LFO 1",    lfo1Area.removeFromTop (12).withTrimmedLeft (4), juce::Justification::centredLeft);
        g.drawText ("LFO 2",    lfo2Area.removeFromTop (12).withTrimmedLeft (8), juce::Justification::centredLeft);
        g.drawText ("ENVELOPE", envArea.removeFromTop (12).withTrimmedLeft (8),  juce::Justification::centredLeft);
    }

    // Column titles inside the heads panel.
    g.setColour (cream().withAlpha (0.45f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.08f));
    auto htitle = rHeads.reduced (10, 0).withTrimmedTop (26);
    htitle = htitle.removeFromTop (14).withTrimmedLeft (22);
    const int colw = htitle.getWidth() / 3;
    g.drawText ("LEVEL", htitle.removeFromLeft (colw), juce::Justification::centred);
    g.drawText ("PAN",   htitle.removeFromLeft (colw), juce::Justification::centred);
    g.drawText ("TIME",  htitle, juce::Justification::centred);

    // INPUT / FEEDBACK sub-headers inside the FILTERS panel (kept in sync with
    // resized()). A divider line separates the two stages.
    {
        auto f = rFilters.reduced (8).withTrimmedTop (22);
        const int half = f.getHeight() / 2;
        auto inArea = f.removeFromTop (half);
        auto fbArea = f;

        g.setColour (doobie::colours::line());
        g.drawHorizontalLine (inArea.getBottom(), (float) rFilters.getX() + 8.0f, (float) rFilters.getRight() - 8.0f);

        g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.1f));
        g.setColour (teal().withAlpha (0.85f));
        g.drawText ("INPUT", inArea.removeFromTop (12).withTrimmedLeft (2), juce::Justification::centredLeft);
        g.setColour (amber().withAlpha (0.85f));
        g.drawText ("FEEDBACK", fbArea.removeFromTop (12).withTrimmedLeft (2), juce::Justification::centredLeft);
    }
}

// ----------------------------------------------------------------------------
// Layout
// ----------------------------------------------------------------------------
void DoobieAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    rHeader = area.removeFromTop (56);

    // Header controls (right side): prev / preset / next / save.
    // Preset cluster anchored to the right: [<] [combo] [>] [SAVE]. Widths must
    // sum to kPresetClusterW so the version text (drawn in paint) stays clear.
    auto hc = rHeader.reduced (16, 11);
    btnSave.setBounds (hc.removeFromRight (62));
    hc.removeFromRight (8);
    btnNext.setBounds (hc.removeFromRight (30));
    hc.removeFromRight (4);
    presetBox.setBounds (hc.removeFromRight (170));
    hc.removeFromRight (4);
    btnPrev.setBounds (hc.removeFromRight (30));

    area.reduce (12, 10);
    const int gap = 12;

    // Bottom output bar (full width).
    rOutput = area.removeFromBottom (104);
    area.removeFromBottom (gap);

    // Modulation panel sits above the output bar, spanning the full width.
    // Compact layout: three sections (LFO 1 | LFO 2 | ENVELOPE) each with its
    // own meter, plus a MATRIX... button for the slot configuration popup.
    rMod = area.removeFromBottom (130);
    area.removeFromBottom (gap);
    {
        auto m = rMod.reduced (10).withTrimmedTop (24);

        // Right-edge: MATRIX button column (compact).
        const int btnColW = 110;
        auto btnCol = m.removeFromRight (btnColW).reduced (4, 2);
        btnMatrix.setBounds (btnCol.removeFromTop (32));

        // Three sub-sections (LFO 1 | LFO 2 | ENV) split the remaining space.
        const int thirdW = m.getWidth() / 3;

        auto layoutLfo = [] (juce::Rectangle<int> r, Knob& rate, Knob& depth, Combo& wave,
                             doobie::ModSourceMeter* meter)
        {
            r.removeFromTop (12); // section header drawn in paint()
            if (meter != nullptr)
                meter->setBounds (r.removeFromBottom (10).reduced (4, 0));
            const int knobW = r.getWidth() / 3;
            rate.place  (r.removeFromLeft (knobW).reduced (4, 0));
            depth.place (r.removeFromLeft (knobW).reduced (4, 0));
            wave.place  (r.reduced (4, 14));
        };
        auto layoutEnv = [] (juce::Rectangle<int> r, Knob& atk, Knob& rel, Knob& sens,
                             doobie::ModSourceMeter* meter)
        {
            r.removeFromTop (12);
            // Meter sits to the right of the knobs (unipolar = vertical bar).
            if (meter != nullptr)
                meter->setBounds (r.removeFromRight (16).reduced (2, 4));
            const int knobW = r.getWidth() / 3;
            atk.place  (r.removeFromLeft (knobW).reduced (4, 0));
            rel.place  (r.removeFromLeft (knobW).reduced (4, 0));
            sens.place (r.reduced (4, 0));
        };

        layoutLfo (m.removeFromLeft (thirdW), kLfo1Rate, kLfo1Depth, cbLfo1Wave, lfo1Meter.get());
        layoutLfo (m.removeFromLeft (thirdW), kLfo2Rate, kLfo2Depth, cbLfo2Wave, lfo2Meter.get());
        layoutEnv (m,                          kEnvAttack, kEnvRelease, kEnvSens, envMeter.get());
    }

    // Three columns.
    const int colAw = 252;
    const int colBw = 360;
    auto colA = area.removeFromLeft (colAw); area.removeFromLeft (gap);
    auto colB = area.removeFromLeft (colBw); area.removeFromLeft (gap);
    auto colC = area;

    // ---- Column A: mode + heads --------------------------------------------
    rMode = colA.removeFromTop (210);
    colA.removeFromTop (gap);
    rHeads = colA;
    {
        // 2x2 grid of head pads (A B / C D) filling the matrix panel.
        auto m = rMode.reduced (14).withTrimmedTop (24);
        const int padGap = 10;
        const int cellH = (m.getHeight() - padGap) / 2;
        auto topRow = m.removeFromTop (cellH);
        m.removeFromTop (padGap);
        auto botRow = m;
        const int cellW = (topRow.getWidth() - padGap) / 2;
        headPads[0].setBounds (topRow.removeFromLeft (cellW));
        headPads[1].setBounds (topRow.removeFromRight (cellW));
        headPads[2].setBounds (botRow.removeFromLeft (cellW));
        headPads[3].setBounds (botRow.removeFromRight (cellW));

        auto h = rHeads.reduced (10).withTrimmedTop (42); // title + column header row
        const int rowH = h.getHeight() / 4;
        for (int i = 0; i < 4; ++i)
        {
            auto row = h.removeFromTop (rowH);
            row.removeFromLeft (22); // head-letter gutter (drawn by Level caption)
            const int kw = row.getWidth() / 3;
            headLevel[(size_t) i].place (row.removeFromLeft (kw));
            headPan[(size_t) i].place   (row.removeFromLeft (kw));
            headRatio[(size_t) i].place (row);
        }
    }

    auto rowOf = [] (juce::Rectangle<int> r, std::initializer_list<Knob*> knobs)
    {
        const int kw = r.getWidth() / (int) knobs.size();
        for (auto* k : knobs)
            k->place (r.removeFromLeft (kw).reduced (3, 0));
    };

    // ---- Column B: delay (top) + tape character (bottom) -------------------
    rTape = colB.removeFromBottom (104);
    colB.removeFromBottom (gap);
    rDelay = colB;
    {
        auto d = rDelay.reduced (10).withTrimmedTop (24);
        // Cassette is the centerpiece of the DELAY panel — give it real
        // vertical space so the reels and tape path are readable, not a thin
        // strip above the knobs.
        cassetteView.setBounds (d.removeFromTop (180));
        d.removeFromTop (6);

        auto bigRow = d.removeFromTop (132);
        kTime.place     (bigRow.removeFromLeft (bigRow.getWidth() / 2).reduced (6, 0));
        kFeedback.place (bigRow.reduced (6, 0));

        d.removeFromTop (6);
        auto syncRow = d.removeFromTop (40);
        tgSync.setBounds (syncRow.removeFromLeft (104).reduced (4, 7));
        cbDivision.place (syncRow.reduced (6, 0));

        cbCharacter.place (d.removeFromTop (40).reduced (6, 0));

        auto togRow = d.removeFromTop (34);
        const int tw = togRow.getWidth() / 3;
        tgPingPong.setBounds (togRow.removeFromLeft (tw).reduced (6, 5));
        tgFreeze.setBounds   (togRow.removeFromLeft (tw).reduced (6, 5));
        tgBypass.setBounds   (togRow.reduced (6, 5));
    }
    rowOf (rTape.reduced (10).withTrimmedTop (22), { &kWow, &kFlutter, &kSat, &kAge });

    // ---- Column C: two filter stages + reverb ------------------------------
    rFilters = colC.removeFromTop (206);
    colC.removeFromTop (gap);
    rReverb = colC;
    {
        auto f = rFilters.reduced (8).withTrimmedTop (22);
        const int half = f.getHeight() / 2;
        auto inArea = f.removeFromTop (half);
        auto fbArea = f;
        inArea.removeFromTop (12);   // INPUT sub-header (drawn in paint)
        fbArea.removeFromTop (12);   // FEEDBACK sub-header
        rowOf (inArea, { &kPreHp, &kPreLp, &kPreBass, &kPreTreble });
        rowOf (fbArea, { &kLowCut, &kHighCut, &kBass, &kTreble });
    }

    {
        auto rv = rReverb.reduced (10).withTrimmedTop (24);
        auto comboRow = rv.removeFromTop (46);
        cbReverbMode.place  (comboRow.removeFromLeft (comboRow.getWidth() / 2).reduced (4, 2));
        cbReverbRoute.place (comboRow.reduced (4, 2));

        // The bottom block of the reverb panel hosts the visualiser and (in
        // Convolution mode) the IR controls below it. Visualiser is always
        // visible — for Convolution it now draws the actual loaded IR
        // waveform; for algorithmic modes it shows the ER pattern + tail.
        auto bottomArea = rv.removeFromBottom (110);
        reverbView.setBounds (bottomArea.removeFromTop (60));
        bottomArea.removeFromTop (4);
        // The IR controls sit below the visualiser, visible only when
        // REVERB == Convolution (toggled in timerCallback).
        {
            auto irArea = bottomArea.reduced (6, 0);
            const int rowH = (irArea.getHeight() - 4) / 2;
            cbFactoryIr.setBounds (irArea.removeFromTop (rowH));
            irArea.removeFromTop (4);
            btnLoadIr.setBounds (irArea.removeFromLeft (118));
            irArea.removeFromLeft (6);
            btnClearIr.setBounds (irArea.removeFromRight (28));
            irArea.removeFromRight (6);
            irLabel.setBounds (irArea);
        }
        rv.removeFromBottom (6);

        auto r1 = rv.removeFromTop (rv.getHeight() / 2);
        // Place the algorithmic knobs and the convolution-mode knobs in the
        // same slots — visibility (toggled in timerCallback) picks which one
        // renders. The kIrGain knob shares kRevMod's slot, kIrSpeed shares
        // kPlatePre's. The two pairs never display simultaneously.
        const int slotW1 = r1.getWidth() / 4;
        kRevMix.place      (r1.removeFromLeft (slotW1).reduced (3, 0));
        kSpringDecay.place (r1.removeFromLeft (slotW1).reduced (3, 0));
        kSpringTone.place  (r1.removeFromLeft (slotW1).reduced (3, 0));
        {
            auto slot = r1.reduced (3, 0);
            kRevMod.place  (slot);
            kIrGain.place  (slot);
        }
        const int slotW2 = rv.getWidth() / 4;
        kPlateDecay.place (rv.removeFromLeft (slotW2).reduced (3, 0));
        kPlateSize.place  (rv.removeFromLeft (slotW2).reduced (3, 0));
        kPlateDamp.place  (rv.removeFromLeft (slotW2).reduced (3, 0));
        {
            auto slot = rv.reduced (3, 0);
            kPlatePre.place (slot);
            kIrSpeed.place  (slot);
        }
    }

    // ---- Output bar ---------------------------------------------------------
    {
        auto o = rOutput.reduced (10).withTrimmedTop (22);
        auto meters = o.removeFromRight (150);
        vuL.setBounds (meters.removeFromLeft (72).reduced (4));
        vuR.setBounds (meters.reduced (4));
        rowOf (o, { &kInput, &kMix, &kOutput, &kWidth, &kDuck });
    }
}
