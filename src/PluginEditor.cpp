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
    kMode.attach     (*this, state, dID::modeSel,  "MODE",     amber());
    kTime.attach     (*this, state, dID::timeMs,   "TIME",     amber());
    kFeedback.attach (*this, state, dID::feedback, "FEEDBACK", amber());
    cbDivision.attach (*this, state, dID::syncDiv, "DIVISION", dID::syncDivChoices);

    for (auto* t : { &tgSync, &tgPingPong, &tgFreeze })
        addAndMakeVisible (t);
    tgSync.getProperties().set     ("accent", (int) amber().getARGB());
    tgPingPong.getProperties().set ("accent", (int) amber().getARGB());
    tgFreeze.getProperties().set   ("accent", (int) doobie::colours::red().getARGB());
    aSync     = std::make_unique<APVTS::ButtonAttachment> (state, dID::syncMode, tgSync);
    aPingPong = std::make_unique<APVTS::ButtonAttachment> (state, dID::pingPong, tgPingPong);
    aFreeze   = std::make_unique<APVTS::ButtonAttachment> (state, dID::freeze,   tgFreeze);

    modeReadout.setJustificationType (juce::Justification::centred);
    modeReadout.setColour (juce::Label::textColourId, amber());
    modeReadout.setFont (juce::Font (juce::FontOptions (13.0f)).withExtraKerningFactor (0.04f));
    addAndMakeVisible (modeReadout);

    // Heads. Captions act as column headers only on the first row.
    static const char* heads[4] = { "A", "B", "C", "D" };
    for (int i = 0; i < 4; ++i)
    {
        headLevel[(size_t) i].attach (*this, state, dID::headLevel[(size_t) i], heads[i], amber());
        headPan[(size_t) i].attach   (*this, state, dID::headPan[(size_t) i],   i == 0 ? "PAN"  : "", amber());
        headRatio[(size_t) i].attach (*this, state, dID::headRatio[(size_t) i], i == 0 ? "TIME" : "", amber());
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

    // Output.
    kInput.attach  (*this, state, dID::inputDrive, "INPUT",  amber());
    kMix.attach    (*this, state, dID::mix,        "MIX",    amber());
    kOutput.attach (*this, state, dID::outputGain, "OUTPUT", amber());
    kWidth.attach  (*this, state, dID::width,      "WIDTH",  teal());
    kDuck.attach   (*this, state, dID::duck,       "DUCK",   amber());

    // Visualisers + meters.
    addAndMakeVisible (echoView);
    addAndMakeVisible (reverbView);
    addAndMakeVisible (vuL);
    addAndMakeVisible (vuR);
    vuL.setLevelSource ([this] { return audioProcessor.getOutputLevel (0); });
    vuR.setLevelSource ([this] { return audioProcessor.getOutputLevel (1); });

    refreshPresetBox();
    timerCallback();          // initialise dynamic labels / enabled state at once
    startTimerHz (12);
    setSize (1100, 720);
}

DoobieAudioProcessorEditor::~DoobieAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
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

    const int modeIdx = juce::jlimit (0, dID::modeChoices.size() - 1,
                                      (int) state.getRawParameterValue (dID::modeSel)->load());
    modeReadout.setText (dID::modeChoices[modeIdx], juce::dontSendNotification);

    const bool synced = state.getRawParameterValue (dID::syncMode)->load() > 0.5f;
    cbDivision.box.setEnabled (synced);
    kTime.slider.setEnabled (! synced);
    cbDivision.box.setAlpha (synced ? 1.0f : 0.4f);
    kTime.slider.setAlpha (synced ? 0.4f : 1.0f);

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

    panel (rMode,    "MODE",   doobie::colours::line());
    panel (rHeads,   "PLAYBACK HEADS", doobie::colours::line());
    panel (rDelay,   "DELAY",  doobie::colours::line());
    panel (rTape,    "TAPE",   doobie::colours::line());
    panel (rFilters, "FILTERS", doobie::colours::line());
    panel (rReverb,  "REVERB", teal().withAlpha (0.6f));
    panel (rOutput,  "OUTPUT", doobie::colours::line());

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
        auto m = rMode.reduced (10).withTrimmedTop (22);
        modeReadout.setBounds (m.removeFromBottom (22));
        kMode.place (m);

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
        echoView.setBounds (d.removeFromTop (116));
        d.removeFromTop (8);

        auto bigRow = d.removeFromTop (150);
        kTime.place     (bigRow.removeFromLeft (bigRow.getWidth() / 2).reduced (6, 0));
        kFeedback.place (bigRow.reduced (6, 0));

        d.removeFromTop (6);
        auto syncRow = d.removeFromTop (44);
        tgSync.setBounds (syncRow.removeFromLeft (110).reduced (4, 8));
        cbDivision.place (syncRow.reduced (6, 2));

        auto togRow = d.removeFromTop (40);
        tgPingPong.setBounds (togRow.removeFromLeft (togRow.getWidth() / 2).reduced (8, 6));
        tgFreeze.setBounds   (togRow.reduced (8, 6));
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

        reverbView.setBounds (rv.removeFromBottom (70));
        rv.removeFromBottom (6);

        auto r1 = rv.removeFromTop (rv.getHeight() / 2);
        rowOf (r1, { &kRevMix, &kSpringDecay, &kSpringTone, &kRevMod });
        rowOf (rv, { &kPlateDecay, &kPlateSize, &kPlateDamp, &kPlatePre });
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
