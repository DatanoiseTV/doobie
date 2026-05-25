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

#include "VuMeter.h"
#include "LookAndFeel.h"
#include <cmath>

namespace doobie
{
VuMeter::VuMeter (juce::String cap) : caption (std::move (cap))
{
    startTimerHz (30);
}

void VuMeter::timerCallback()
{
    const float raw = source ? source() : 0.0f;
    const float db  = raw > 1.0e-6f ? 20.0f * std::log10 (raw) : -60.0f;
    const float target = juce::jlimit (0.0f, 1.0f, (db + 24.0f) / 27.0f); // -24..+3 dB

    // VU-like ballistics: comparable attack and release (~300 ms feel at 30 Hz).
    level += (target > level ? 0.45f : 0.22f) * (target - level);

    if (target > peak) { peak = target; peakHold = 22; }
    else if (--peakHold < 0) peak = juce::jmax (level, peak - 0.015f);

    repaint();
}

void VuMeter::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    g.setColour (colours::panelShadow());
    g.fillRoundedRectangle (b, 4.0f);
    auto face = b.reduced (3.0f);
    g.setColour (juce::Colour (0xff141210));
    g.fillRoundedRectangle (face, 3.0f);
    g.setColour (colours::line());
    g.drawRoundedRectangle (face, 3.0f, 1.0f);

    // Reserve a strip for the channel label so it never overlaps the scale.
    const auto labelArea = face.removeFromBottom (11.0f);

    // The pivot sits at the bottom-centre of the gauge. The radius is limited by
    // BOTH the width and the height, so in a short, wide meter the needle and the
    // peak dot stay inside the box instead of overshooting the sides.
    const float maxAngle = juce::degreesToRadians (48.0f);
    const float a0 = -maxAngle;
    const float a1 =  maxAngle;
    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() - 2.0f);
    const float rad = juce::jmin ((face.getWidth() * 0.5f - 5.0f) / std::sin (maxAngle),
                                  face.getHeight() - 5.0f);

    auto pointOnArc = [&] (float angle, float radius)
    {
        return pivot + juce::Point<float> (std::sin (angle), -std::cos (angle)) * radius;
    };

    // Scale arc; the upper-right portion is the red "over" zone.
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, a0, a1, true);
    g.setColour (colours::amberDim());
    g.strokePath (arc, juce::PathStrokeType (1.4f));

    juce::Path redArc;
    redArc.addCentredArc (pivot.x, pivot.y, rad, rad, 0.0f, maxAngle * 0.5f, a1, true);
    g.setColour (colours::red());
    g.strokePath (redArc, juce::PathStrokeType (1.8f));

    for (int i = 0; i <= 8; ++i)
    {
        const float t   = (float) i / 8.0f;
        const float ang = a0 + t * (a1 - a0);
        g.setColour (t > 0.75f ? colours::red() : colours::cream().withAlpha (0.55f));
        g.drawLine ({ pointOnArc (ang, rad), pointOnArc (ang, rad - 4.0f) }, 1.1f);
    }

    // Peak-hold dot riding the arc.
    const auto pp = pointOnArc (a0 + peak * (a1 - a0), rad);
    g.setColour (peak > 0.75f ? colours::red() : colours::amber());
    g.fillEllipse (pp.x - 2.0f, pp.y - 2.0f, 4.0f, 4.0f);

    // Needle.
    const auto tip = pointOnArc (a0 + level * (a1 - a0), rad - 2.0f);
    g.setColour (colours::amber().withAlpha (0.30f));
    g.drawLine ({ pivot.x, pivot.y, tip.x, tip.y }, 3.5f);
    g.setColour (colours::amber());
    g.drawLine ({ pivot.x, pivot.y, tip.x, tip.y }, 1.6f);
    g.setColour (colours::cream());
    g.fillEllipse (pivot.x - 2.5f, pivot.y - 2.5f, 5.0f, 5.0f);

    // Channel label, bottom-left, clear of the scale.
    g.setColour (colours::cream().withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.08f));
    g.drawText (caption.toUpperCase(), labelArea.withTrimmedLeft (5.0f), juce::Justification::centredLeft);
}
} // namespace doobie
