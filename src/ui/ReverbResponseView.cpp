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

#include "ReverbResponseView.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../ParameterIDs.h"
#include <cmath>

namespace doobie
{
ReverbResponseView::ReverbResponseView (DoobieAudioProcessor& p) : processor (p)
{
    startTimerHz (8);
}

void ReverbResponseView::timerCallback()
{
    repaint();
}

// RT60 of a feedback loop: time for the level to drop 60 dB.
// rt = Tmean * ln(0.001) / ln(gain). These constants mirror the spring/plate
// DSP so the picture tracks what you actually hear.
static float loopRt (float gain, float meanDelaySec, float dampScale)
{
    gain = juce::jlimit (0.05f, 0.9995f, gain);
    const float rt = meanDelaySec * std::log (0.001f) / std::log (gain);
    return rt * dampScale;
}

float ReverbResponseView::estimateDecaySeconds() const
{
    auto& s = processor.getValueTreeState();
    auto raw = [&s] (const char* id) { return s.getRawParameterValue (id)->load(); };

    const int mode = (int) raw (dID::reverbMode);

    const float springG  = 0.55f + 0.43f * raw (dID::springDecay);
    const float springRt = loopRt (springG, 0.050f, 1.0f);

    const float plateG    = 0.5f + 0.49f * raw (dID::plateDecay);
    const float plateMean = 0.0338f * (0.6f + 0.9f * raw (dID::plateSize));
    const float plateRt   = loopRt (plateG, plateMean, 1.0f - 0.45f * raw (dID::plateDamp));

    // Hall and shimmer share the larger FDN core (longer mean delay, g 0.6..0.99).
    const float hallG    = 0.6f + 0.39f * raw (dID::plateDecay);
    const float hallMean = 0.075f * (0.7f + 0.7f * raw (dID::plateSize));
    const float hallRt   = loopRt (hallG, hallMean, 1.0f - 0.45f * raw (dID::plateDamp));

    switch (mode)
    {
        case 1: return springRt;
        case 2: return plateRt;
        case 3: return springRt + plateRt;          // series chains the tails
        case 4: return juce::jmax (springRt, plateRt);
        case 5: return hallRt;
        case 6: return hallRt * 1.6f;               // shimmer regenerates longer
        default: return 0.0f;
    }
}

float ReverbResponseView::predelaySeconds() const
{
    auto& s = processor.getValueTreeState();
    const int mode = (int) s.getRawParameterValue (dID::reverbMode)->load();
    if (mode >= 2)                                   // plate, series, parallel, hall, shimmer
        return s.getRawParameterValue (dID::platePredelay)->load() * 0.001f;
    return 0.0f;
}

void ReverbResponseView::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff141210));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (colours::teal().withAlpha (0.45f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    const int mode = (int) processor.getValueTreeState().getRawParameterValue (dID::reverbMode)->load();
    if (mode == 0)
    {
        g.setColour (colours::cream().withAlpha (0.35f));
        g.setFont (juce::Font (juce::FontOptions (12.0f)).withExtraKerningFactor (0.1f));
        g.drawText ("REVERB OFF", bounds, juce::Justification::centred);
        return;
    }

    const float rt  = juce::jmax (0.05f, estimateDecaySeconds());
    const float pre = predelaySeconds();
    const float maxT = juce::jlimit (0.6f, 9.0f, rt * 1.15f + pre);

    const auto plot = bounds.reduced (8.0f, 7.0f);

    // Second grid lines.
    g.setColour (colours::line().withAlpha (0.4f));
    for (float t = 1.0f; t < maxT; t += 1.0f)
    {
        const float x = plot.getX() + (t / maxT) * plot.getWidth();
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }

    // Decay envelope: flat through the pre-delay, then exponential to -inf dB.
    const float preX = plot.getX() + (pre / maxT) * plot.getWidth();
    juce::Path env;
    env.startNewSubPath (plot.getX(), plot.getBottom());
    env.lineTo (preX, plot.getBottom());

    const int steps = (int) plot.getWidth();
    for (int i = 0; i <= steps; ++i)
    {
        const float x = preX + (float) i / (float) steps * (plot.getRight() - preX);
        const float t = (x - preX) / plot.getWidth() * maxT;
        const float amp = std::exp (std::log (0.001f) * t / rt); // 1 -> 0.001 over rt
        const float y = plot.getBottom() - amp * plot.getHeight();
        env.lineTo (x, y);
    }
    env.lineTo (plot.getRight(), plot.getBottom());
    env.closeSubPath();

    juce::ColourGradient grad (colours::teal().withAlpha (0.55f), 0.0f, plot.getY(),
                               colours::teal().withAlpha (0.06f), 0.0f, plot.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (env);
    g.setColour (colours::teal());
    g.strokePath (env, juce::PathStrokeType (1.4f));

    // Predelay marker.
    if (pre > 0.001f)
    {
        g.setColour (colours::amber().withAlpha (0.7f));
        g.drawVerticalLine ((int) preX, plot.getY(), plot.getBottom());
    }

    // Readout.
    g.setColour (colours::cream().withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    const juce::String label = dID::reverbModeChoices[mode]
                             + "   decay ~" + juce::String (rt, 1) + " s";
    g.drawText (label, plot.withTrimmedTop (1).removeFromTop (14), juce::Justification::topRight);
}
} // namespace doobie
