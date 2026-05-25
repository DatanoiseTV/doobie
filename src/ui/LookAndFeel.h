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

#include <juce_gui_basics/juce_gui_basics.h>

namespace doobie
{
// Brand palette (see BRANDING.md). Kept as functions so they can be used in
// static contexts without ordering worries.
namespace colours
{
    inline juce::Colour panel()       { return juce::Colour (0xff1c1a17); }
    inline juce::Colour panelLight()  { return juce::Colour (0xff2a2622); }
    inline juce::Colour panelShadow() { return juce::Colour (0xff100f0d); }
    inline juce::Colour cream()       { return juce::Colour (0xffe8dcc0); }
    inline juce::Colour amber()       { return juce::Colour (0xfff4a024); }
    inline juce::Colour amberDim()    { return juce::Colour (0xff8a5e1e); }
    inline juce::Colour teal()        { return juce::Colour (0xff3fb6a8); }
    inline juce::Colour red()         { return juce::Colour (0xffd8492e); }
    inline juce::Colour metal()       { return juce::Colour (0xff3a3631); }
    inline juce::Colour line()        { return juce::Colour (0xff4a443c); }
}

// The vintage tape-echo look: brushed-metal knobs with an amber pointer and a
// value arc that glows, screen-printed cream labels, recessed combo boxes and
// small indicator-lamp toggles.
class DoobieLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DoobieLookAndFeel();

    // Per-control accent colour, set via Slider/Component property "accent".
    static juce::Colour accentFor (const juce::Component& c);

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool isDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool, bool) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getLabelFont (juce::Label&) override;
};
} // namespace doobie
