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

#include "../ParameterIDs.h"
#include "../dsp/ModMatrix.h"
#include "LookAndFeel.h"

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include <memory>

namespace doobie
{
// The mod matrix lives in a popup (CallOutBox) launched from the MOD MATRIX
// button in the main editor — keeps the main UI clean while giving the slots
// room to breathe.
class ModMatrixPopup : public juce::Component
{
public:
    ModMatrixPopup (juce::AudioProcessorValueTreeState& state, DoobieLookAndFeel& lnf)
    {
        setLookAndFeel (&lnf);

        const auto srcChoices  = doobie::modSourceNames();
        const auto destChoices = doobie::modDestNames();

        for (int i = 0; i < doobie::kNumModSlots; ++i)
        {
            auto& row = rows[(size_t) i];
            row.src.addItemList  (srcChoices, 1);
            row.dest.addItemList (destChoices, 1);
            row.amount.setRange (-1.0, 1.0, 0.001);
            row.amount.setSliderStyle (juce::Slider::LinearHorizontal);
            row.amount.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 18);
            row.amount.setDoubleClickReturnValue (true, 0.0);
            row.amount.getProperties().set ("accent", (int) colours::amber().getARGB());

            row.label.setText (juce::String (i + 1), juce::dontSendNotification);
            row.label.setJustificationType (juce::Justification::centred);
            row.label.setColour (juce::Label::textColourId, colours::amber());
            row.label.setFont (juce::Font (juce::FontOptions (14.0f)).boldened());

            row.arrow.setText (juce::String::fromUTF8 ("\xe2\x86\x92"), juce::dontSendNotification);
            row.arrow.setJustificationType (juce::Justification::centred);
            row.arrow.setColour (juce::Label::textColourId, colours::cream().withAlpha (0.6f));
            row.arrow.setFont (juce::Font (juce::FontOptions (14.0f)));

            addAndMakeVisible (row.label);
            addAndMakeVisible (row.src);
            addAndMakeVisible (row.arrow);
            addAndMakeVisible (row.dest);
            addAndMakeVisible (row.amount);

            row.srcAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                              (state, dID::modSlotSrc[(size_t) i], row.src);
            row.destAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
                              (state, dID::modSlotDst[(size_t) i], row.dest);
            row.amtAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>
                              (state, dID::modSlotAmt[(size_t) i], row.amount);
        }

        setSize (700, 4 * rowHeight + 2 * padding + titleHeight);
    }

    ~ModMatrixPopup() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel());
        g.setColour (colours::cream());
        g.setFont (juce::Font (juce::FontOptions (13.0f)).withExtraKerningFactor (0.14f).boldened());
        g.drawText ("MOD MATRIX", juce::Rectangle<int> (padding, 4, getWidth() - 2 * padding, titleHeight - 4),
                    juce::Justification::centredLeft);
        g.setColour (colours::line());
        g.drawHorizontalLine (titleHeight, (float) padding, (float) (getWidth() - padding));
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (padding).withTrimmedTop (titleHeight - padding);
        for (auto& row : rows)
        {
            auto r = area.removeFromTop (rowHeight).reduced (0, 3);
            row.label.setBounds  (r.removeFromLeft (28));
            row.src.setBounds    (r.removeFromLeft (140).reduced (2, 0));
            row.arrow.setBounds  (r.removeFromLeft (24));
            row.dest.setBounds   (r.removeFromLeft (200).reduced (2, 0));
            r.removeFromLeft (8);
            row.amount.setBounds (r.reduced (4, 0));
        }
    }

private:
    static constexpr int padding     = 14;
    static constexpr int titleHeight = 28;
    static constexpr int rowHeight   = 36;

    struct Row
    {
        juce::Label    label, arrow;
        juce::ComboBox src, dest;
        juce::Slider   amount;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> srcAtt, destAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>   amtAtt;
    };
    std::array<Row, kNumModSlots> rows;
};
} // namespace doobie
