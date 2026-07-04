/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version, and distributed WITHOUT ANY WARRANTY. See <https://www.gnu.org/licenses/>.
  You must retain this notice and the attribution to DatanoiseTV in any
  redistributed or derivative version.

  Offline preset stability check. Excites every factory preset with a 1 s
  broadband burst, then renders 30 s of silence and watches the output-level
  trajectory. It answers one question per preset: after the input stops, does
  the tail decay, or does the feedback loop keep singing?

  A self-oscillating preset (feedback >= ~1.0) that lacks enough AGE (the tape-
  wear macro: dropouts + progressive HF loss + instability, applied inside the
  feedback loop) will hold a near-constant runaway tone to the end of the
  window. The point of this tool is to FIND those so their AGE can be raised
  until the oscillation breaks up and darkens instead of sustaining.

  Also doubles as a finite-output guard: any NaN/Inf or absurd peak is a hard
  failure. Exit code is non-zero if any preset produces a non-finite sample.

      doobie_stability_check [seconds=30]

  Build:  cmake --build build --target doobie_stability_check
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "PluginProcessor.h"
#include "ParameterIDs.h"

#include <cstdio>
#include <cmath>
#include <vector>

namespace
{
float rawParam (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* v = apvts.getRawParameterValue (id)) return v->load();
    return 0.0f;
}

// dBFS of a linear magnitude, floored so log never blows up.
float toDb (float lin) { return lin < 1.0e-6f ? -120.0f : juce::Decibels::gainToDecibels (lin); }

struct FixedTempoPlayHead : juce::AudioPlayHead
{
    explicit FixedTempoPlayHead (double bpmIn) : bpm (bpmIn) {}
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo pos;
        pos.setBpm (bpm);
        pos.setIsPlaying (true);
        pos.setTimeInSamples (samples);
        return pos;
    }
    double bpm = 120.0;
    juce::int64 samples = 0;
};
} // namespace

int main (int argc, char** argv)
{
    const double seconds = argc >= 2 ? juce::jlimit (5.0, 120.0, juce::String (argv[1]).getDoubleValue()) : 30.0;
    const double sr    = 48000.0;
    const int    block = 512;
    const double bpm   = 120.0;
    const int    total = (int) (sr * seconds);
    const int    burst = (int) (sr * 1.0);     // 1 s of excitation, then silence

    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray names;
    {
        DoobieAudioProcessor probe;
        names = probe.getPresetManager().getFactoryNames();
    }

    std::printf ("Stability check: %d presets, %.0f s each @ %.0f Hz, 1 s burst then silence\n\n",
                 names.size(), seconds, sr);
    std::printf ("%-3s %-22s %5s %5s %-6s | %7s %8s | %6s %6s | %s\n",
                 "#", "preset", "fb", "age", "freeze",
                 "peakDb", "t=end", "jitter", "flat", "verdict");
    std::printf ("%s\n", juce::String::repeatedString ("-", 96).toRawUTF8());

    juce::Random rng (0x51AB1E);   // deterministic excitation across runs
    const float kCeiling = juce::Decibels::decibelsToGain (6.0f);  // +6 dBFS hard limit
    // FFT over the tail for spectral flatness — the leveler-immune measure of
    // whether a sustained preset is a tonal oscillation or an AGE-broken wash.
    constexpr int fftOrder = 14;
    const int fftSize = 1 << fftOrder;
    juce::dsp::FFT fft (fftOrder);
    juce::dsp::WindowingFunction<float> window ((size_t) fftSize,
                                                juce::dsp::WindowingFunction<float>::hann);
    int nonFinite = 0, sustained = 0, unbounded = 0;

    for (int index = 0; index < names.size(); ++index)
    {
        const juce::String name = names[index];

        FixedTempoPlayHead playHead (bpm);
        DoobieAudioProcessor proc;
        proc.setPlayHead (&playHead);
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.prepareToPlay (sr, block);

        auto& apvts = proc.getValueTreeState();
        proc.getPresetManager().loadByName (name);

        // Skip MIDI-note presets: they make no sound without incoming notes,
        // so their "tail" is trivially silent and tells us nothing.
        const bool midiNote = rawParam (apvts, dID::midiPitchMode) > 0.5f;
        const float fb     = rawParam (apvts, dID::feedback);
        const float age    = rawParam (apvts, dID::hiss);
        const bool  freeze = rawParam (apvts, dID::freeze) > 0.5f;

        // Level trajectory: peak magnitude within each 25 ms window. Fine
        // enough that AGE's sub-second dropouts show up as level dips — a
        // coarser window (0.5 s) is smoothed back to the leveler ceiling by a
        // net-positive loop and hides them.
        const double winSec = 0.025;
        const int winLen = (int) (sr * winSec);
        std::vector<float> winPeak;
        float curPeak = 0.0f;
        int   curCount = 0;
        float burstPeak = 0.0f;
        float maxPeak = 0.0f;       // absolute output ceiling over the whole render
        bool  finite = true;

        // Ring of the most recent fftSize mono samples. Spectral flatness is
        // invariant to circular shift (magnitude spectrum ignores it), so the
        // ring can be transformed as-is without unwrapping.
        std::vector<float> tail ((size_t) fftSize, 0.0f);
        int tailPos = 0;

        juce::MidiBuffer midi;
        for (int pos = 0; pos < total; pos += block)
        {
            const int n = juce::jmin (block, total - pos);
            juce::AudioBuffer<float> chunk (2, n);
            chunk.clear();
            if (pos < burst)
            {
                const int copy = juce::jmin (n, burst - pos);
                for (int ch = 0; ch < 2; ++ch)
                {
                    auto* d = chunk.getWritePointer (ch);
                    for (int i = 0; i < copy; ++i)
                        d[i] = (rng.nextFloat() * 2.0f - 1.0f) * 0.35f;   // broadband burst
                }
            }
            midi.clear();
            proc.processBlock (chunk, midi);
            playHead.samples += n;

            for (int i = 0; i < n; ++i)
            {
                const float s = std::max (std::abs (chunk.getSample (0, i)),
                                          std::abs (chunk.getSample (1, i)));
                if (! std::isfinite (s)) finite = false;
                maxPeak = std::max (maxPeak, s);
                curPeak = std::max (curPeak, s);
                tail[(size_t) (tailPos & (fftSize - 1))] =
                    0.5f * (chunk.getSample (0, i) + chunk.getSample (1, i));
                ++tailPos;
                if (pos + i < burst) burstPeak = std::max (burstPeak, s);
                if (++curCount >= winLen) { winPeak.push_back (curPeak); curPeak = 0.0f; curCount = 0; }
            }
        }
        if (curCount > 0) winPeak.push_back (curPeak);

        proc.setPlayHead (nullptr);
        proc.releaseResources();

        auto winDbAt = [&] (double t) -> float
        {
            const int w = juce::jlimit (0, (int) winPeak.size() - 1, (int) (t / winSec));
            return toDb (winPeak[(size_t) w]);
        };
        const float dbEnd   = winDbAt (seconds - 0.5);
        juce::ignoreUnused (burstPeak);

        // Tail character over the last third of the window: mean level plus the
        // level's jitter (population std-dev of the per-window dB). This is what
        // separates a CLEAN self-oscillation (steady tone the leveler pins at a
        // fixed level -> low jitter) from an AGED wash (AGE's dropouts + HF-loss
        // + instability make the level wander -> high jitter). A dub preset that
        // sustains is fine IF it's broken up; the failure mode is a sterile held
        // tone. Windows at the -120 floor are excluded from the statistics.
        const int startW = (int) (winPeak.size() * 2 / 3);
        float sum = 0.0f; int cnt = 0;
        for (int w = startW; w < (int) winPeak.size(); ++w)
        {
            const float d = toDb (winPeak[(size_t) w]);
            if (d > -60.0f) { sum += d; ++cnt; }
        }
        const float tailMean = cnt > 0 ? sum / (float) cnt : -120.0f;
        float var = 0.0f;
        for (int w = startW; w < (int) winPeak.size(); ++w)
        {
            const float d = toDb (winPeak[(size_t) w]);
            if (d > -60.0f) { const float e = d - tailMean; var += e * e; }
        }
        const float tailJitter = cnt > 0 ? std::sqrt (var / (float) cnt) : 0.0f;

        // Spectral flatness of the tail (geometric mean / arithmetic mean of the
        // magnitude spectrum). Near 0 = a pure tone (sterile oscillation); toward
        // 1 = broadband noise (AGE has shredded the tone into a wash). Because it
        // reads the SPECTRUM, not the level, the output leveler doesn't blur it —
        // the way the level-jitter measure gets blurred. Only meaningful when the
        // tail actually has energy.
        float flatness = 0.0f;
        if (finite && dbEnd > -40.0f)
        {
            std::vector<float> fftBuf ((size_t) (fftSize * 2), 0.0f);
            for (int k = 0; k < fftSize; ++k) fftBuf[(size_t) k] = tail[(size_t) k];
            window.multiplyWithWindowingTable (fftBuf.data(), (size_t) fftSize);
            fft.performFrequencyOnlyForwardTransform (fftBuf.data());
            double logSum = 0.0, linSum = 0.0; int bins = 0;
            for (int k = 1; k < fftSize / 2; ++k)   // skip DC
            {
                const double m = (double) fftBuf[(size_t) k] + 1.0e-9;
                logSum += std::log (m); linSum += m; ++bins;
            }
            if (bins > 0)
                flatness = (float) (std::exp (logSum / bins) / (linSum / bins));
        }

        // Verdict.
        //  - HARD FAILURES (exit non-zero): a non-finite sample, or output that
        //    breaks the ceiling — the leveler must keep every preset bounded.
        //  - Everything else is informational. Freeze presets hold on purpose.
        //    "decays" = tail dies. "aged wash" = sustains but AGE has broken it
        //    up (jitter high). "steady tone" = sustains as a near-constant tone
        //    (bounded, but a candidate for more AGE / less feedback if it's not
        //    meant to be a drone).
        const float dbPeak = toDb (maxPeak);
        const bool  overCeiling = maxPeak > kCeiling;
        // A sustained tail counts as AGE-tamed if it's either level-broken
        // (jitter) OR spectrally shredded (flatness) — either one means it's no
        // longer a sterile tone.
        const bool  brokenUp = tailJitter >= 3.0f || flatness >= 0.10f;
        juce::String verdict;
        if (! finite)                       { verdict = "*** NON-FINITE ***"; ++nonFinite; }
        else if (overCeiling)               { verdict = "*** OVER CEILING ***"; ++unbounded; }
        else if (midiNote)                    verdict = "midi (skipped)";
        else if (freeze)                      verdict = "freeze (held)";
        else if (tailMean <= -30.0f)          verdict = "decays";
        else if (brokenUp)                    verdict = "aged wash";
        else                                { verdict = "tonal sustain"; ++sustained; }

        std::printf ("%-3d %-22s %5.2f %5.2f %-6s | %7.1f %8.1f | %6.1f %6.3f | %s\n",
                     index, name.toRawUTF8(), fb, age, freeze ? "yes" : "no",
                     dbPeak, dbEnd, tailJitter, flatness, verdict.toRawUTF8());
    }

    // The pass criterion is CONTAINMENT, not silence: sustained musical
    // feedback is fine (dub lives on it) as long as the output stays finite and
    // under the ceiling. A tonal-sustain count is informational only.
    const bool contained = (nonFinite == 0 && unbounded == 0);
    std::printf ("\n%d tonal-sustain preset(s) (musical feedback, contained); "
                 "%d non-finite, %d over ceiling (+%.0f dBFS).\n",
                 sustained, nonFinite, unbounded, juce::Decibels::gainToDecibels (kCeiling));
    std::printf ("%s\n", contained ? "PASS: all presets contained."
                                    : "FAIL: uncontained preset(s) above.");
    return contained ? 0 : 1;
}
