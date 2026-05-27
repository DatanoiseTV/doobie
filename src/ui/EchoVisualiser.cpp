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

#include "EchoVisualiser.h"
#include "LookAndFeel.h"
#include "../PluginProcessor.h"
#include "../ParameterIDs.h"

namespace doobie
{
EchoVisualiser::EchoVisualiser (DoobieAudioProcessor& p) : processor (p)
{
    startTimerHz (24);
}

void EchoVisualiser::timerCallback()
{
    repaint();
}

void EchoVisualiser::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff141210));
    g.fillRoundedRectangle (bounds, 5.0f);
    g.setColour (colours::line());
    g.drawRoundedRectangle (bounds.reduced (0.5f), 5.0f, 1.0f);

    auto& apvts = processor.getValueTreeState();
    auto raw = [&apvts] (const char* id) { return apvts.getRawParameterValue (id)->load(); };

    const float  delaySamples = processor.getEngine().currentDelaySamples();
    const double sampleRate   = processor.getSampleRateForUI();
    const double delaySec     = sampleRate > 0.0 ? (double) delaySamples / sampleRate : 0.25;
    const float  feedback     = juce::jlimit (0.0f, 1.2f, raw (dID::feedback));

    // In sync mode the head taps snap to musical divisions (mirror the engine).
    const bool synced = raw (dID::syncMode) > 0.5f;
    double masterQuarters = 0.0;
    if (synced)
    {
        const int divIdx = juce::jlimit (0, (int) dID::syncDivQuarters.size() - 1, (int) raw (dID::syncDiv));
        masterQuarters = dID::syncDivQuarters[(size_t) divIdx];
    }

    const auto& mags = processor.getEngine().headMagnitudes();

    const double windowSec = juce::jlimit (0.6, 4.0, delaySec * 5.0);
    const auto plot = bounds.reduced (10.0f, 8.0f);
    const float laneH = plot.getHeight() / 4.0f;

    // Time grid at each full repeat.
    g.setColour (colours::line().withAlpha (0.5f));
    for (double t = delaySec; t < windowSec; t += delaySec)
    {
        const float x = plot.getX() + (float) (t / windowSec) * plot.getWidth();
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }

    static const char* names[4] = { "A", "B", "C", "D" };
    for (int h = 0; h < 4; ++h)
    {
        const float laneY = plot.getY() + ((float) h + 0.5f) * laneH;

        g.setColour (colours::cream().withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (names[h], juce::Rectangle<float> (plot.getX() - 9.0f, laneY - 7.0f, 10.0f, 14.0f),
                    juce::Justification::centred);

        const bool on = raw (dID::headOn[(size_t) h]) > 0.5f;
        const float level = juce::jlimit (0.0f, 1.0f, raw (dID::headLevel[(size_t) h]));
        if (! on || level <= 0.001f)
            continue;

        float ratio = juce::jlimit (0.05f, 1.0f, raw (dID::headRatio[(size_t) h]));
        if (synced && masterQuarters > 0.0)
            ratio = (float) (dID::snapHeadQuarters ((double) ratio * masterQuarters, masterQuarters) / masterQuarters);
        const float live  = juce::jlimit (0.0f, 1.0f, mags[(size_t) h].load());
        const float glow  = 0.45f + 0.55f * live;

        for (int n = 0; n < 64; ++n)
        {
            const double t = (double) ratio * delaySec + (double) n * delaySec;
            if (t > windowSec)
                break;
            const float amp = level * std::pow (feedback, (float) n) * glow;
            if (amp < 0.02f && n > 0)
                break;

            const float x = plot.getX() + (float) (t / windowSec) * plot.getWidth();
            const float r = 2.0f + amp * 9.0f;
            g.setColour (colours::amber().withAlpha (juce::jlimit (0.08f, 1.0f, amp)));
            g.fillEllipse (x - r * 0.5f, laneY - r * 0.5f, r, r);
        }
    }
}
} // namespace doobie
