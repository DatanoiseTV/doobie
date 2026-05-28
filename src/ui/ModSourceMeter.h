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

#include "LookAndFeel.h"

#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>

namespace doobie
{
// Compact visual indicator for a modulation source.
//
//   * Bipolar  (LFOs): a horizontal bar centred at the mid-line, growing left
//     for negative values and right for positive — the user sees the current
//     LFO position relative to the unmodulated centre.
//   * Unipolar (env follower): a vertical bar filling from the bottom — the
//     classic VU shape, the user sees how loud the input is according to the
//     env settings.
//
// The value comes from a std::function fetched on a Timer, so the audio thread
// never touches a UI component — the editor publishes the value to an atomic
// and the meter polls it.
class ModSourceMeter : public juce::Component,
                      private juce::Timer
{
public:
    enum class Style { Bipolar, Unipolar };

    ModSourceMeter (Style s, std::function<float()> fetcher, juce::Colour c = colours::teal())
        : style (s), getValue (std::move (fetcher)), accent (c)
    {
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (colours::panelShadow());
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (colours::line());
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        const float v = juce::jlimit (style == Style::Bipolar ? -1.0f : 0.0f, 1.0f, displayValue);
        if (style == Style::Bipolar)
        {
            const float centre = r.getCentreX();
            const float halfW  = r.getWidth() * 0.5f - 2.0f;
            const float x = v >= 0.0f ? centre : centre + v * halfW;
            const float w = std::fabs (v) * halfW;
            g.setColour (accent.withAlpha (0.85f));
            g.fillRoundedRectangle (juce::Rectangle<float> (x, r.getY() + 2.0f, w, r.getHeight() - 4.0f), 2.0f);
            g.setColour (colours::cream().withAlpha (0.4f));
            g.drawVerticalLine ((int) centre, r.getY() + 2.0f, r.getBottom() - 2.0f);
        }
        else
        {
            const float fillH = (r.getHeight() - 4.0f) * v;
            g.setColour (accent.withAlpha (0.85f));
            g.fillRoundedRectangle (
                juce::Rectangle<float> (r.getX() + 2.0f, r.getBottom() - 2.0f - fillH,
                                        r.getWidth() - 4.0f, fillH),
                2.0f);
        }
    }

private:
    void timerCallback() override
    {
        const float v = getValue ? getValue() : 0.0f;
        // Light smoothing so the bar doesn't flicker on the 30 Hz poll.
        displayValue += 0.35f * (v - displayValue);
        repaint();
    }

    Style style;
    std::function<float()> getValue;
    juce::Colour accent;
    float displayValue = 0.0f;
};
} // namespace doobie
