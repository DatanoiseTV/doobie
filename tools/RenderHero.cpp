/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version, and distributed WITHOUT ANY WARRANTY. See <https://www.gnu.org/licenses/>.
  You must retain this notice and the attribution to DatanoiseTV in any
  redistributed or derivative version.

  Hero-video audio renderer. Plays a dry source through the real engine while
  driving parameters from tools/hero-timeline.json — the same timeline the UI
  recorder (tools/record-hero.mjs) animates on screen, so the published video's
  knob moves match its soundtrack sample-for-sample.

      doobie_render_hero <source.wav> <timeline.json> <out.wav> [presetName] [bpm=120]

  With presetName given, that factory preset is loaded first and the timeline
  automates on top of it — so everything not explicitly automated (drive,
  hiss, spring decay, mix, tone stack...) is the genuine preset. bpm drives
  tempo-synced delays via a fixed-tempo playhead.
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "PluginProcessor.h"

namespace
{
struct Breakpoints
{
    juce::String id;
    std::vector<std::pair<double, double>> pts;   // (seconds, value)

    double at (double t) const
    {
        if (pts.empty()) return 0.0;
        if (t <= pts.front().first) return pts.front().second;
        for (size_t i = 1; i < pts.size(); ++i)
            if (t < pts[i].first)
            {
                const auto& a = pts[i - 1];
                const auto& b = pts[i];
                const double f = (t - a.first) / juce::jmax (1.0e-9, b.first - a.first);
                return a.second + f * (b.second - a.second);
            }
        return pts.back().second;
    }

    // step semantics: value of the latest event at or before t
    double stepAt (double t) const
    {
        double v = pts.empty() ? 0.0 : 0.0;
        bool any = false;
        for (const auto& p : pts)
            if (p.first <= t) { v = p.second; any = true; }
        return any ? v : -1.0;   // -1 = leave the parameter at its default
    }
};

std::vector<Breakpoints> readSection (const juce::var& root, const char* section)
{
    std::vector<Breakpoints> out;
    const auto* obj = root.getDynamicObject();
    if (obj == nullptr) return out;
    const juce::var sec = obj->getProperty (section);
    const auto* secObj = sec.getDynamicObject();
    if (secObj == nullptr) return out;

    for (const auto& prop : secObj->getProperties())
    {
        Breakpoints bp;
        bp.id = prop.name.toString();
        if (const auto* arr = prop.value.getArray())
            for (const auto& pt : *arr)
                if (const auto* pair = pt.getArray(); pair != nullptr && pair->size() == 2)
                    bp.pts.emplace_back ((double) (*pair)[0], (double) (*pair)[1]);
        out.push_back (std::move (bp));
    }
    return out;
}

// Fixed-tempo transport so tempo-synced delays follow a musical BPM.
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

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& buf, double sr)
{
    file.deleteFile();
    juce::WavAudioFormat fmt;
    std::unique_ptr<juce::FileOutputStream> os (file.createOutputStream());
    if (os == nullptr) return false;
    std::unique_ptr<juce::AudioFormatWriter> w (
        fmt.createWriterFor (os.get(), sr, (unsigned) buf.getNumChannels(), 24, {}, 0));
    if (w == nullptr) return false;
    os.release();
    return w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}
} // namespace

int main (int argc, char** argv)
{
    if (argc < 4)
    {
        std::fprintf (stderr, "usage: doobie_render_hero <source.wav> <timeline.json> <out.wav>\n");
        return 2;
    }

    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const juce::File sourceFile = cwd.getChildFile (argv[1]);
    const juce::File tlFile     = cwd.getChildFile (argv[2]);
    const juce::File outFile    = cwd.getChildFile (argv[3]);

    const juce::var tl = juce::JSON::parse (tlFile.loadFileAsString());
    if (tl.isVoid())
    {
        std::fprintf (stderr, "error: could not parse timeline %s\n", tlFile.getFullPathName().toRawUTF8());
        return 2;
    }
    const double duration = tl.getProperty ("duration", 24.0);
    auto sliders = readSection (tl, "sliders");
    auto toggles = readSection (tl, "toggles");
    auto combos  = readSection (tl, "combos");

    juce::AudioFormatManager fmtMgr;
    fmtMgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fmtMgr.createReaderFor (sourceFile));
    if (reader == nullptr)
    {
        std::fprintf (stderr, "error: could not read %s\n", sourceFile.getFullPathName().toRawUTF8());
        return 2;
    }

    const double sr = reader->sampleRate;
    const int srcLen = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> dry (2, srcLen);
    dry.clear();
    reader->read (&dry, 0, srcLen, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1) dry.copyFrom (1, 0, dry, 0, 0, srcLen);

    const int total = (int) std::lround (duration * sr);
    const int block = 512;

    const double bpm = argc >= 6 ? juce::jlimit (20.0, 300.0, juce::String (argv[5]).getDoubleValue()) : 120.0;
    FixedTempoPlayHead playHead (bpm);

    DoobieAudioProcessor proc;
    proc.setPlayHead (&playHead);
    proc.setPlayConfigDetails (2, 2, sr, block);
    proc.prepareToPlay (sr, block);
    auto& apvts = proc.getValueTreeState();

    if (argc >= 5)
    {
        proc.getPresetManager().loadByName (argv[4]);
        std::fprintf (stdout, "loaded preset: %s (host %.0f BPM)\n",
                      proc.getPresetManager().getCurrentName().toRawUTF8(), bpm);
    }

    auto setNorm = [&apvts] (const juce::String& id, float norm)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
    };
    auto setStep = [&apvts] (const juce::String& id, double v)
    {
        if (v < 0.0) return;                       // no event yet -> keep default
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 ((float) v));
    };

    juce::AudioBuffer<float> out (2, total);
    out.clear();
    juce::MidiBuffer midi;

    for (int pos = 0; pos < total; pos += block)
    {
        const double t = pos / sr;
        for (const auto& bp : sliders) setNorm (bp.id, (float) bp.at (t));
        for (const auto& bp : toggles) setStep (bp.id, bp.stepAt (t));
        for (const auto& bp : combos)  setStep (bp.id, bp.stepAt (t));

        const int n = juce::jmin (block, total - pos);
        juce::AudioBuffer<float> chunk (2, n);
        chunk.clear();
        if (pos < srcLen)
        {
            const int copy = juce::jmin (n, srcLen - pos);
            chunk.copyFrom (0, 0, dry, 0, pos, copy);
            chunk.copyFrom (1, 0, dry, 1, pos, copy);
        }
        midi.clear();
        proc.processBlock (chunk, midi);
        playHead.samples += n;
        out.copyFrom (0, pos, chunk, 0, 0, n);
        out.copyFrom (1, pos, chunk, 1, 0, n);
    }
    proc.setPlayHead (nullptr);
    proc.releaseResources();

    // normalise to -1 dBFS like the demo clips
    const float mag = out.getMagnitude (0, total);
    if (mag > 1.0e-6f) out.applyGain (juce::Decibels::decibelsToGain (-1.0f) / mag);

    if (! writeWav (outFile, out, sr))
    {
        std::fprintf (stderr, "error: could not write %s\n", outFile.getFullPathName().toRawUTF8());
        return 1;
    }
    std::fprintf (stdout, "wrote %s (%.1f s @ %.0f Hz)\n", outFile.getFullPathName().toRawUTF8(), duration, sr);
    return 0;
}
