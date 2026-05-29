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
// The cassette-deck look: near-black panels, cream-rim controls echoing the
// cassette's reel flanges + idler rollers, amber kept only as a small
// "lit LED" accent (knob value-tick, toggle dot, head-pad fill) so live
// state still pops against the monochrome chrome.

DoobieLookAndFeel::DoobieLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, colours::panel());
    setColour (juce::Slider::textBoxTextColourId, colours::cream());
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId, colours::cream());
    setColour (juce::ComboBox::textColourId, colours::cream());
    setColour (juce::ComboBox::backgroundColourId, colours::panelLight());
    setColour (juce::ComboBox::outlineColourId, colours::line());
    setColour (juce::ComboBox::arrowColourId, colours::cream());
    setColour (juce::PopupMenu::backgroundColourId, colours::panelLight());
    setColour (juce::PopupMenu::highlightedBackgroundColourId,
               colours::cream().withAlpha (0.18f));
    setColour (juce::PopupMenu::textColourId, colours::cream());
    setColour (juce::PopupMenu::highlightedTextColourId, colours::cream());
}

juce::Colour DoobieLookAndFeel::accentFor (const juce::Component& c)
{
    // The accent is the lit-LED highlight. Default is amber (= a cassette
    // deck's PLAY/REC LED); per-control overrides come from a "accent"
    // property still set on the head pads so they keep their per-head tint.
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
    juce::ignoreUnused (slider);

    // ---- Value arc (faint outside the rim) ----------------------------------
    const float arcR = radius - 1.5f;
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (colours::line());
    g.strokePath (track, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, arcR, arcR, 0.0f, startAngle, angle, true);
    g.setColour (colours::cream().withAlpha (0.85f));
    g.strokePath (value, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ---- Knob body: cassette-style cream rim with a dark inset --------------
    const float knobR = radius * 0.78f;
    g.setColour (colours::panel());
    g.fillEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f);

    // Slight dome shading so the knob doesn't read as a flat hole — very subtle.
    juce::ColourGradient dome (juce::Colour (0xff1a1814), centre.x - knobR * 0.3f, centre.y - knobR * 0.6f,
                               juce::Colour (0xff050403), centre.x, centre.y + knobR, true);
    g.setGradientFill (dome);
    g.fillEllipse (centre.x - knobR * 0.95f, centre.y - knobR * 0.95f, knobR * 1.9f, knobR * 1.9f);

    g.setColour (colours::cream());
    g.drawEllipse (centre.x - knobR, centre.y - knobR, knobR * 2.0f, knobR * 2.0f, 1.2f);
    g.setColour (colours::cream().withAlpha (0.18f));
    g.drawEllipse (centre.x - knobR * 0.88f, centre.y - knobR * 0.88f, knobR * 1.76f, knobR * 1.76f, 0.8f);

    // ---- Pointer (cream line + small amber lit tip) ------------------------
    juce::Path pointer;
    const float pl  = knobR * 0.86f;
    const float pin = knobR * 0.30f;
    pointer.startNewSubPath (0.0f, -pin);
    pointer.lineTo (0.0f, -pl);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre));
    g.setColour (colours::cream());
    g.strokePath (pointer, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // Centre dot, matching the cassette spindle / roller centres.
    const float ctrR = 1.6f;
    g.setColour (colours::cream());
    g.fillEllipse (centre.x - ctrR, centre.y - ctrR, ctrR * 2.0f, ctrR * 2.0f);
}

void DoobieLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool,
                                      int, int, int, int, juce::ComboBox& box)
{
    const auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (1.0f);
    g.setColour (colours::panelLight());
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (box.hasKeyboardFocus (false) ? colours::cream() : colours::line());
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

    // Small cream drop arrow — no amber here, this is a passive selector.
    const float cx = (float) width - 14.0f;
    const float cy = (float) height * 0.5f;
    juce::Path arrow;
    arrow.addTriangle (cx - 4.0f, cy - 2.5f, cx + 4.0f, cy - 2.5f, cx, cy + 3.5f);
    g.setColour (colours::cream());
    g.fillPath (arrow);
}

juce::Font DoobieLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (13.0f)).withExtraKerningFactor (0.06f);
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
    // Head-matrix pads: square button, off = thin cream outline on black,
    // on = solid cream fill with dark letter (mirrors how a cassette deck
    // shows a selected source — flipped contrast, no amber wash).
    if (button.getProperties().contains ("pad"))
    {
        auto       pad    = button.getLocalBounds().toFloat().reduced (3.0f);
        const auto accent = accentFor (button);
        const bool on     = button.getToggleState();

        g.setColour (colours::panel());
        g.fillRoundedRectangle (pad, 4.0f);

        if (on)
        {
            // Solid cream face with a subtle amber inner glow at the top edge
            // (= the "lit" LED reading through the face).
            g.setColour (colours::cream());
            g.fillRoundedRectangle (pad, 4.0f);
            g.setColour (accent.withAlpha (0.18f));
            auto top = pad;
            g.fillRoundedRectangle (top.removeFromTop (pad.getHeight() * 0.4f), 4.0f);
        }

        g.setColour (shouldDrawButtonAsHighlighted ? colours::cream()
                                                   : (on ? colours::panel() : colours::line()));
        g.drawRoundedRectangle (button.getLocalBounds().toFloat().reduced (3.0f), 4.0f, 1.2f);

        const auto letter = button.getProperties().getWithDefault ("headLetter", button.getButtonText()).toString();
        const auto fullPad = button.getLocalBounds().toFloat().reduced (3.0f);
        g.setColour (on ? colours::panel() : colours::cream().withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions (fullPad.getHeight() * 0.42f)).withExtraKerningFactor (0.04f).boldened());
        g.drawText (letter, fullPad, juce::Justification::centred, false);

        g.setColour (on ? colours::panel().withAlpha (0.7f) : colours::cream().withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (9.0f)).withExtraKerningFactor (0.14f));
        g.drawText (on ? "ON" : "OFF", fullPad.reduced (0.0f, 6.0f).removeFromBottom (12.0f),
                    juce::Justification::centred, false);
        return;
    }

    // Indicator-lamp toggles: small cassette-style roller — cream rim circle,
    // dark inside when off, amber-lit dot when on. Label in cream beside it.
    const auto bounds = button.getLocalBounds().toFloat();
    const float lampD = juce::jmin (14.0f, bounds.getHeight() - 4.0f);
    const auto  lamp  = juce::Rectangle<float> (bounds.getX(), bounds.getCentreY() - lampD * 0.5f, lampD, lampD);
    const auto  accent = accentFor (button);
    const bool  on     = button.getToggleState();

    g.setColour (colours::panel());
    g.fillEllipse (lamp);
    g.setColour (colours::cream().withAlpha (on ? 1.0f : 0.65f));
    g.drawEllipse (lamp, 1.2f);

    if (on)
    {
        const float dotD = lampD * 0.55f;
        const auto dot = juce::Rectangle<float> (lamp.getCentreX() - dotD * 0.5f,
                                                 lamp.getCentreY() - dotD * 0.5f, dotD, dotD);
        g.setColour (accent.withAlpha (0.45f));
        g.fillEllipse (dot.expanded (2.0f));
        g.setColour (accent);
        g.fillEllipse (dot);
    }

    g.setColour (shouldDrawButtonAsHighlighted ? colours::cream() : colours::cream().withAlpha (on ? 1.0f : 0.7f));
    g.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.08f));
    g.drawText (button.getButtonText().toUpperCase(),
                bounds.withTrimmedLeft (lampD + 8.0f), juce::Justification::centredLeft, false);
}

void DoobieLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                              const juce::Colour&, bool highlighted, bool down)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? colours::panelLight().brighter (0.10f) : colours::panelLight());
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (highlighted ? colours::cream() : colours::line());
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

juce::Label* DoobieLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (slider);
    l->setColour (juce::Label::textColourId, colours::cream());
    l->setFont (juce::Font (juce::FontOptions (12.0f)));
    l->setJustificationType (juce::Justification::centred);
    return l;
}

juce::Font DoobieLookAndFeel::getLabelFont (juce::Label& label)
{
    return juce::Font (juce::FontOptions ((float) label.getFont().getHeight())).withExtraKerningFactor (0.06f);
}
} // namespace doobie
