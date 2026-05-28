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
#include <random>

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

// Mode-dependent early-reflection profile: how many, how spread out, how the
// initial energy bunches before the diffuse tail takes over. Drawn as discrete
// vertical lines at deterministic times (per-mode seed), so the user sees
// distinct "rooms" instead of a single smooth blob.
struct ERProfile { int count; float spreadSec; float maxAmp; juce::uint32 seed; };

static ERProfile profileFor (int mode)
{
    switch (mode)
    {
        case 1: return {  3, 0.06f,  0.85f, 0xC0FFEEu };  // Spring — sparse, fast
        case 2: return {  8, 0.09f,  0.80f, 0x50AA50u };  // Plate — dense, snappy
        case 3: return { 11, 0.12f,  0.85f, 0xBADu };     // Spring -> Plate (chained)
        case 4: return { 10, 0.10f,  0.80f, 0xDEADBEu };  // Spring + Plate parallel
        case 5: return {  6, 0.14f,  0.75f, 0xFA110u };   // Hall — longer ERs
        case 6: return {  7, 0.14f,  0.70f, 0x511E20u };  // Shimmer
        default: return { 5, 0.10f,  0.7f,  0u };
    }
}

static void paintAlgorithmicReverb (juce::Graphics& g, juce::Rectangle<float> plot,
                                    int mode, float pre, float rt)
{
    const auto erProfile = profileFor (mode);
    const float erEnd = pre + erProfile.spreadSec;
    const float maxT  = juce::jlimit (0.6f, 9.0f, rt * 1.10f + pre);
    const auto xForT  = [&] (float t) { return plot.getX() + (t / maxT) * plot.getWidth(); };

    // Second grid lines.
    g.setColour (colours::line().withAlpha (0.35f));
    for (float t = 1.0f; t < maxT; t += 1.0f)
        g.drawVerticalLine ((int) xForT (t), plot.getY(), plot.getBottom());

    // Diffuse tail: exponential envelope from the end of the ER region.
    {
        juce::Path env;
        env.startNewSubPath (xForT (erEnd), plot.getBottom());
        const int steps = (int) plot.getWidth();
        for (int i = 0; i <= steps; ++i)
        {
            const float t = erEnd + (float) i / (float) steps * (maxT - erEnd);
            if (t > maxT) break;
            const float amp = std::exp (std::log (0.001f) * (t - erEnd) / juce::jmax (0.05f, rt));
            env.lineTo (xForT (t), plot.getBottom() - amp * plot.getHeight() * 0.85f);
        }
        env.lineTo (plot.getRight(), plot.getBottom());
        env.closeSubPath();

        juce::ColourGradient grad (colours::teal().withAlpha (0.50f), 0.0f, plot.getY(),
                                   colours::teal().withAlpha (0.04f), 0.0f, plot.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (env);
        g.setColour (colours::teal().withAlpha (0.9f));
        g.strokePath (env, juce::PathStrokeType (1.4f));
    }

    // Discrete early reflections (deterministic per-mode pattern).
    {
        std::mt19937 rng (erProfile.seed);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);

        g.setColour (colours::amber().withAlpha (0.95f));
        for (int i = 0; i < erProfile.count; ++i)
        {
            const float frac = (float) (i + 1) / (float) (erProfile.count + 1);
            const float jitter = (uni (rng) - 0.5f) * 0.4f / (float) erProfile.count;
            const float t = pre + erProfile.spreadSec * juce::jlimit (0.05f, 1.0f, frac + jitter);
            if (t >= maxT) break;

            // Amplitude tapers from maxAmp at the start toward the tail level.
            const float tNorm  = (t - pre) / juce::jmax (0.001f, erProfile.spreadSec);
            const float amp    = erProfile.maxAmp * (0.45f + 0.55f * (1.0f - tNorm));
            const float x = xForT (t);
            const float h = amp * plot.getHeight() * 0.85f;
            g.drawLine (x, plot.getBottom() - h, x, plot.getBottom(), 1.4f);
            g.fillEllipse (x - 1.6f, plot.getBottom() - h - 1.6f, 3.2f, 3.2f);
        }
    }

    // Predelay marker.
    if (pre > 0.001f)
    {
        g.setColour (colours::amber().withAlpha (0.4f));
        g.drawVerticalLine ((int) xForT (pre), plot.getY(), plot.getBottom());
    }
}

static void paintConvolutionIR (juce::Graphics& g, juce::Rectangle<float> plot,
                                const juce::AudioBuffer<float>& ir, double sourceSr,
                                float speed)
{
    const int numSamples = ir.getNumSamples();
    const int numCh      = ir.getNumChannels();
    if (numSamples <= 0 || numCh < 1 || sourceSr <= 0.0)
    {
        g.setColour (colours::cream().withAlpha (0.45f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)).withExtraKerningFactor (0.08f));
        g.drawText ("LOAD AN IR", plot, juce::Justification::centred);
        return;
    }

    // Effective playback time at the current speed multiplier.
    const float lengthSec = (float) numSamples / (float) (sourceSr * (double) speed);
    const float maxT      = juce::jmax (lengthSec, 0.2f);

    // Compute peak per pixel column from the source IR.
    const int pxW = juce::jmax (1, (int) plot.getWidth());
    std::vector<float> peaks ((size_t) pxW, 0.0f);
    for (int x = 0; x < pxW; ++x)
    {
        const int s0 = (int) ((float) x       / (float) pxW * (float) numSamples);
        const int s1 = (int) ((float) (x + 1) / (float) pxW * (float) numSamples);
        float pk = 0.0f;
        for (int ch = 0; ch < juce::jmin (2, numCh); ++ch)
        {
            const float* d = ir.getReadPointer (ch);
            for (int s = s0; s < s1 && s < numSamples; ++s)
                pk = std::fmax (pk, std::fabs (d[s]));
        }
        peaks[(size_t) x] = pk;
    }

    // Normalise to the column with the loudest peak (typically the start),
    // so the early reflections sit near the top and the tail is visible too.
    float maxPeak = 1.0e-6f;
    for (float v : peaks) maxPeak = std::fmax (maxPeak, v);

    // Filled waveform from baseline upward (peak-envelope style — the
    // direct-sound impulse + ERs spike at the start, the diffuse tail decays).
    juce::Path env;
    env.startNewSubPath (plot.getX(), plot.getBottom());
    for (int x = 0; x < pxW; ++x)
    {
        const float v = peaks[(size_t) x] / maxPeak;
        // Optional gamma so the visible dynamic range is closer to dB-feel.
        const float disp = std::pow (juce::jlimit (0.0f, 1.0f, v), 0.6f);
        env.lineTo (plot.getX() + (float) x, plot.getBottom() - disp * plot.getHeight() * 0.92f);
    }
    env.lineTo (plot.getRight(), plot.getBottom());
    env.closeSubPath();

    juce::ColourGradient grad (colours::teal().withAlpha (0.55f), 0.0f, plot.getY(),
                               colours::teal().withAlpha (0.04f), 0.0f, plot.getBottom(), false);
    g.setGradientFill (grad);
    g.fillPath (env);
    g.setColour (colours::teal());
    g.strokePath (env, juce::PathStrokeType (1.2f));

    // Second grid lines (effective time, after speed scaling).
    g.setColour (colours::line().withAlpha (0.35f));
    for (float t = 1.0f; t < maxT; t += 1.0f)
    {
        const float x = plot.getX() + (t / maxT) * plot.getWidth();
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
    }
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

    const auto plot = bounds.reduced (8.0f, 7.0f);
    juce::String label = dID::reverbModeChoices[mode];

    if (mode == 7) // Convolution
    {
        const float speed = processor.getEngine().getIRSpeed();
        const auto& ir    = processor.getEngine().getIRBuffer();
        const double sr   = processor.getEngine().getIRSourceSr();
        paintConvolutionIR (g, plot, ir, sr, speed);
        if (ir.getNumSamples() > 0 && sr > 0.0)
        {
            const float lenSec = (float) ir.getNumSamples() / (float) (sr * (double) speed);
            label += "   IR " + juce::String (lenSec, 2) + " s";
            if (std::abs (speed - 1.0f) > 0.005f)
                label += "   x" + juce::String (speed, 2);
        }
    }
    else
    {
        const float rt  = juce::jmax (0.05f, estimateDecaySeconds());
        const float pre = predelaySeconds();
        paintAlgorithmicReverb (g, plot, mode, pre, rt);
        label += "   decay ~" + juce::String (rt, 1) + " s";
    }

    // Readout.
    g.setColour (colours::cream().withAlpha (0.8f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (label, plot.withTrimmedTop (1).removeFromTop (14), juce::Justification::topRight);
}
} // namespace doobie
