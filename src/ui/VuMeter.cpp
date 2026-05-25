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
    const auto face = b.reduced (3.0f);
    g.setColour (juce::Colour (0xff141210));
    g.fillRoundedRectangle (face, 3.0f);
    g.setColour (colours::line());
    g.drawRoundedRectangle (face, 3.0f, 1.0f);

    const float pivX = face.getCentreX();
    const float pivY = face.getBottom() - 5.0f;
    const float rad  = face.getHeight() * 0.92f;
    const float a0   = juce::degreesToRadians (-52.0f);
    const float a1   = juce::degreesToRadians (52.0f);

    // Arc with tick marks; the top right of the sweep is the red "over" zone.
    juce::Path arc;
    arc.addCentredArc (pivX, pivY, rad, rad, 0.0f, a0, a1, true);
    g.setColour (colours::amberDim());
    g.strokePath (arc, juce::PathStrokeType (1.5f));

    juce::Path redArc;
    redArc.addCentredArc (pivX, pivY, rad, rad, 0.0f, juce::degreesToRadians (28.0f), a1, true);
    g.setColour (colours::red());
    g.strokePath (redArc, juce::PathStrokeType (2.0f));

    for (int i = 0; i <= 8; ++i)
    {
        const float t   = (float) i / 8.0f;
        const float ang = a0 + t * (a1 - a0);
        const auto  dir = juce::Point<float> (std::sin (ang), -std::cos (ang));
        const auto  p1  = juce::Point<float> (pivX, pivY) + dir * rad;
        const auto  p2  = juce::Point<float> (pivX, pivY) + dir * (rad - 5.0f);
        g.setColour (t > 0.78f ? colours::red() : colours::cream().withAlpha (0.6f));
        g.drawLine ({ p1, p2 }, 1.2f);
    }

    // Needle.
    const float ang = a0 + level * (a1 - a0);
    const auto  dir = juce::Point<float> (std::sin (ang), -std::cos (ang));
    const auto  tip = juce::Point<float> (pivX, pivY) + dir * (rad - 2.0f);
    g.setColour (colours::amber().withAlpha (0.30f));
    g.drawLine ({ pivX, pivY, tip.x, tip.y }, 4.0f);
    g.setColour (colours::amber());
    g.drawLine ({ pivX, pivY, tip.x, tip.y }, 1.8f);
    g.setColour (colours::cream());
    g.fillEllipse (pivX - 3.0f, pivY - 3.0f, 6.0f, 6.0f);

    // Peak-hold dot riding the arc.
    const float pang = a0 + peak * (a1 - a0);
    const auto  pdir = juce::Point<float> (std::sin (pang), -std::cos (pang));
    const auto  pp   = juce::Point<float> (pivX, pivY) + pdir * rad;
    g.setColour (peak > 0.78f ? colours::red() : colours::amber());
    g.fillEllipse (pp.x - 2.0f, pp.y - 2.0f, 4.0f, 4.0f);

    g.setColour (colours::cream().withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (10.0f)).withExtraKerningFactor (0.08f));
    g.drawText (caption.toUpperCase(), face.withHeight (13.0f), juce::Justification::centred);
}
} // namespace doobie
