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

#include "LookAndFeel.h"

namespace doobie
{
DoobieLookAndFeel::DoobieLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::panel());
    setColour (juce::Slider::textBoxTextColourId, colours::amber());
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::cream());
    setColour (juce::ComboBox::textColourId, colours::amber());
    setColour (juce::ComboBox::backgroundColourId, colours::panelShadow());
    setColour (juce::PopupMenu::backgroundColourId, colours::panelLight());
    setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::amberDim());
    setColour (juce::PopupMenu::textColourId, colours::cream());
}

juce::Colour DoobieLookAndFeel::accentFor (const juce::Component& c)
{
    if (c.getProperties().contains ("accent"))
        return juce::Colour ((juce::uint32) (int) c.getProperties()["accent"]);
    return colours::amber();
}

void DoobieLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                          float sliderPos, float startAngle, float endAngle,
                                          juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (5.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto  centre = bounds.getCentre();
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);
    const auto  accent = accentFor (slider);

    // Recessed seat the knob sits in.
    g.setColour (colours::panelShadow());
    g.fillEllipse (centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);

    // Value arc: dim track, then the lit portion with a soft glow underneath.
    const float arcR = radius - 2.0f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::line());
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (accent.withAlpha (0.28f));
    g.strokePath (value, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (accent);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Brushed-metal knob body, lit from the top-left.
    const float knobR = radius * 0.72f;
    juce::ColourGradient grad (colours::metal().brighter (0.35f), centre.x - knobR, centre.y - knobR,
                               colours::metal().darker (0.55f),   centre.x + knobR, centre.y + knobR, false);
    g.setGradientFill (grad);
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillEllipse (centre.x - knobR * 0.8f, centre.y - knobR * 0.95f, knobR * 1.6f, knobR);

    g.setColour (colours::panelShadow());
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.5f);

    // Pointer with a faint glowing tip.
    juce::Path pointer;
    const float pl = knobR * 0.94f;
    pointer.startNewSubPath (0.0f, -knobR * 0.28f);
    pointer.lineTo (0.0f, -pl);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (accent.withAlpha (0.35f));
    g.strokePath (pointer, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (colours::cream());
    g.strokePath (pointer, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void DoobieLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (colours::panelShadow());
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (box.hasKeyboardFocus (false) ? colours::amberDim() : colours::line());
    g.drawRoundedRectangle (bounds, 4.0f, 1.2f);

    // Amber drop arrow.
    const float cx = (float) width - 14.0f;
    const float cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 5.0f, cy - 3.0f, cx + 5.0f, cy - 3.0f, cx, cy + 4.0f);
    g.setColour (colours::amber());
    g.fillPath (arrow);
}

juce::Font DoobieLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (14.0f)).withExtraKerningFactor (0.04f);
}

void DoobieLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 1, box.getWidth() - 24, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void DoobieLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                          bool shouldDrawButtonAsHighlighted, bool)
{
    // Head-matrix pads: a square lit button with a big centred letter, glowing
    // when the head is on. Flagged with the "pad" property by the editor.
    if (button.getProperties().contains ("pad"))
    {
        const auto pad    = button.getLocalBounds().toFloat().reduced (3.0f);
        const auto accent = accentFor (button);
        const bool on     = button.getToggleState();

        g.setColour (colours::panelShadow());
        g.fillRoundedRectangle (pad.expanded (1.5f), 7.0f);

        if (on)
        {
            g.setColour (accent.withAlpha (0.30f));
            g.fillRoundedRectangle (pad.expanded (2.5f), 8.0f); // outer glow
        }

        juce::ColourGradient face = on
            ? juce::ColourGradient (accent.brighter (0.30f), pad.getTopLeft(),
                                    accent.darker (0.30f),   pad.getBottomLeft(), false)
            : juce::ColourGradient (colours::metal().brighter (0.18f), pad.getTopLeft(),
                                    colours::metal().darker (0.45f),   pad.getBottomLeft(), false);
        g.setGradientFill (face);
        g.fillRoundedRectangle (pad, 6.0f);

        g.setColour (shouldDrawButtonAsHighlighted ? colours::cream()
                                                   : (on ? accent.brighter (0.4f) : colours::line()));
        g.drawRoundedRectangle (pad.reduced (0.5f), 6.0f, 1.4f);

        const auto letter = button.getProperties().getWithDefault ("headLetter", button.getButtonText()).toString();
        g.setColour (on ? colours::panelShadow() : colours::cream().withAlpha (0.55f));
        g.setFont (juce::Font (juce::FontOptions (pad.getHeight() * 0.42f)).withExtraKerningFactor (0.04f).boldened());
        g.drawText (letter, pad, juce::Justification::centred, false);

        g.setColour (on ? colours::panelShadow().withAlpha (0.7f) : colours::cream().withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.12f));
        g.drawText (on ? "ON" : "OFF", pad.reduced (0.0f, 6.0f).removeFromBottom (14.0f),
                    juce::Justification::centred, false);
        return;
    }

    const auto bounds = button.getLocalBounds().toFloat();
    const float lampD = juce::jmin (16.0f, bounds.getHeight() - 4.0f);
    const auto  lamp  = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - lampD * 0.5f, lampD, lampD);
    const auto  accent = accentFor (button);

    g.setColour (colours::panelShadow());
    g.fillEllipse (lamp.expanded (1.5f));

    if (button.getToggleState())
    {
        g.setColour (accent.withAlpha (0.30f));
        g.fillEllipse (lamp.expanded (4.0f));
        g.setColour (accent);
    }
    else
    {
        g.setColour (colours::metal().darker (0.2f));
    }
    g.fillEllipse (lamp);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillEllipse (lamp.reduced (lampD * 0.3f).translated (-1.0f, -1.0f));

    g.setColour (shouldDrawButtonAsHighlighted ? colours::cream() : colours::cream().withAlpha (0.85f));
    g.setFont (juce::Font (juce::FontOptions (13.0f)).withExtraKerningFactor (0.05f));
    g.drawText (button.getButtonText().toUpperCase(),
                bounds.withTrimmedLeft (lampD + 8.0f), juce::Justification::centredLeft, false);
}

void DoobieLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    juce::ColourGradient grad (colours::metal().brighter (down ? 0.0f : 0.25f), bounds.getTopLeft(),
                               colours::metal().darker (0.4f), bounds.getBottomLeft(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (highlighted ? colours::amber() : colours::line());
    g.drawRoundedRectangle (bounds, 4.0f, 1.2f);
}

juce::Label* DoobieLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::textColourId, colours::amber());
    l->setFont (juce::Font (juce::FontOptions (13.0f)));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

juce::Font DoobieLookAndFeel::getLabelFont (juce::Label& label)
{
    return juce::Font (juce::FontOptions ((float) label.getFont().getHeight())).withExtraKerningFactor (0.04f);
}
} // namespace doobie
