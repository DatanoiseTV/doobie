/*
  Doobie — analog dub delay
  Copyright (C) 2026 DatanoiseTV

  This program is free software: you can redistribute it and/or modify it under
  the terms of the GNU General Public License as published by the Free Software
  Foundation, either version 3 of the License, or (at your option) any later
  version, and distributed WITHOUT ANY WARRANTY. See <https://www.gnu.org/licenses/>.
  You must retain this notice and the attribution to DatanoiseTV in any
  redistributed or derivative version.

  Offline preset-demo renderer. Feeds a dry source loop through a curated set of
  factory presets and writes one audio file per preset (plus the untouched dry
  loop for A/B) into an output directory, along with a manifest.json the landing
  page player reads. No DAW, no realtime — pure offline processing.

      doobie_render_demo <source.wav> <outDir>

  Build the standalone target family first (this tool links the plugin code):
      cmake --build build --target doobie_render_demo
*/

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>

#include "PluginProcessor.h"
#include "ParameterIDs.h"

namespace
{
// Read the current index of a choice/float parameter from the APVTS.
int paramIndex (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
{
    if (auto* v = apvts.getRawParameterValue (id)) return (int) std::lround (v->load());
    return 0;
}

// Derive a musical category chip + one-line descriptor from the loaded engine
// state, so every preset is grouped and described without hand-authoring 80
// taglines. Returns {category, tagline}.
struct Info { juce::String category, tagline; };

// Index of the first preset in the authored "Live Vocal FX" bank (see the
// 64..79 comment in PresetManager.cpp). Those get their own category.
constexpr int kVocalBankStart = 64;

Info describe (juce::AudioProcessorValueTreeState& apvts, int index)
{
    const int rev   = paramIndex (apvts, dID::reverbMode);   // Off/Spring/Plate/Series/Parallel/Hall/Shimmer/Conv/Gated
    const int revR  = paramIndex (apvts, dID::reverbRoute);  // Post/Pre/InFeedback
    const int dly   = paramIndex (apvts, dID::delayMode);    // Digital/Tape/BBD/Diffuse/Pitch
    const bool phOn = paramIndex (apvts, dID::phaserOn) != 0;
    const int phR   = paramIndex (apvts, dID::phaserRoute);

    auto safe = [] (const juce::StringArray& a, int i) {
        return juce::isPositiveAndBelow (i, a.size()) ? a[i] : juce::String();
    };
    const juce::String revName = safe (dID::reverbModeChoices, rev);
    const juce::String dlyName = safe (dID::delayModeChoices, dly);
    const juce::String revRoute = safe (dID::reverbRouteChoices, revR).toLowerCase();
    const juce::String phRoute  = safe (dID::phaserRouteChoices, phR).toLowerCase();

    // category — the authored vocal bank first, then the most characterful
    // active stage. Reverb/space and motion take priority over the (very
    // common) tape character so the groups stay balanced instead of dumping
    // most presets under one label.
    juce::String cat;
    if      (index >= kVocalBankStart) cat = "Vocal FX";
    else if (rev == 6) cat = "Shimmer";
    else if (rev == 5) cat = "Hall";
    else if (rev == 8) cat = "Gated";
    else if (rev == 7) cat = "Convolution";
    else if (dly == 4) cat = "Pitch & harmony";
    else if (phOn)     cat = "Phaser & motion";
    else if (dly == 3) cat = "Ambient & wash";
    else if (rev == 1) cat = "Spring & dub";        // spring tank — the dub core
    else if (rev >= 2) cat = "Plate & space";       // plate / series / parallel
    else if (dly == 1 || dly == 2) cat = "Tape echo";
    else               cat = "Clean delay";

    // descriptor
    juce::String t = dlyName + " delay";
    if (rev >= 1) t << " · " << revName << " reverb (" << revRoute << ")";
    else          t << " · no reverb";
    if (phOn)     t << " · phaser (" << phRoute << ")";
    return { cat, t };
}

juce::String slugify (const juce::String& name)
{
    juce::String s;
    for (auto c : name)
    {
        if (juce::CharacterFunctions::isLetterOrDigit (c)) s << juce::String::charToString (c).toLowerCase();
        else if (s.isNotEmpty() && s.getLastCharacter() != '-')  s << '-';
    }
    return s.trimCharactersAtEnd ("-");
}

// Peak-normalise to a target ceiling so every clip sits at a comparable level.
void normalise (juce::AudioBuffer<float>& buf, float ceilingDb = -1.0f)
{
    float mag = buf.getMagnitude (0, buf.getNumSamples());
    if (mag < 1.0e-6f) return;
    const float target = juce::Decibels::decibelsToGain (ceilingDb);
    buf.applyGain (target / mag);
}

// Structured patch info for the player's info card. Raw parameter values from
// the APVTS are real units (ms, semitones, 0..1.2 feedback etc.), so the page
// can format them without knowing any ranges.
juce::var buildInfo (juce::AudioProcessorValueTreeState& apvts)
{
    auto raw = [&apvts] (const char* id) -> float
    {
        if (auto* v = apvts.getRawParameterValue (id)) return v->load();
        return 0.0f;
    };
    auto choice = [&raw] (const char* id, const juce::StringArray& names) -> juce::String
    {
        const int i = (int) std::lround (raw (id));
        return juce::isPositiveAndBelow (i, names.size()) ? names[i] : juce::String();
    };

    auto* o = new juce::DynamicObject();
    o->setProperty ("character", choice (dID::delayMode, dID::delayModeChoices));
    const bool sync = raw (dID::syncMode) > 0.5f;
    o->setProperty ("sync", sync);
    o->setProperty ("time", sync ? choice (dID::syncDiv, dID::syncDivChoices)
                                 : juce::String (juce::roundToInt (raw (dID::timeMs))) + " ms");
    o->setProperty ("feedback", raw (dID::feedback));            // 0..1.2
    o->setProperty ("mix", raw (dID::mix));

    juce::String heads;
    for (int h = 0; h < 4; ++h)
        if (raw (dID::headOn[(size_t) h]) > 0.5f) heads << (char) ('A' + h);
    o->setProperty ("heads", heads);

    o->setProperty ("reverb",   choice (dID::reverbMode, dID::reverbModeChoices));
    o->setProperty ("revRoute", choice (dID::reverbRoute, dID::reverbRouteChoices));
    o->setProperty ("revMix",   raw (dID::reverbMix));

    o->setProperty ("phaser", raw (dID::phaserOn) > 0.5f
                                  ? choice (dID::phaserRoute, dID::phaserRouteChoices)
                                  : juce::String());

    const bool pitchChar = (int) std::lround (raw (dID::delayMode)) == 4;
    if ((pitchChar && raw (dID::pitchOn) > 0.5f))
    {
        const int st = juce::roundToInt (raw (dID::pitchSemis));
        o->setProperty ("pitch", juce::String (st > 0 ? "+" : "") + juce::String (st) + " st · "
                                  + choice (dID::pitchRoute, dID::pitchRouteChoices).toLowerCase());
    }

    juce::StringArray extras;
    if (raw (dID::pingPong)  > 0.5f)  extras.add ("ping-pong");
    if (raw (dID::freeze)    > 0.5f)  extras.add ("freeze");
    if (raw (dID::inFilterOn) > 0.5f) extras.add ("filter");
    if (raw (dID::duck)  > 0.05f)     extras.add ("duck");
    if (raw (dID::wow)   > 0.35f)     extras.add ("wow");
    if (raw (dID::hiss)  > 0.15f)     extras.add ("aged tape");   // AGE macro (id "hiss")
    if (raw (dID::width) > 1.15f)     extras.add ("wide");
    o->setProperty ("extras", extras.joinIntoString (" · "));
    return juce::var (o);
}

// Minimal fixed-tempo transport so tempo-synced presets render at a musical
// BPM instead of whatever the processor's fallback happens to be.
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
    os.release(); // writer owns the stream now
    return w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
}
} // namespace

int main (int argc, char** argv)
{
    bool manifestOnly = false;
    for (int a = 1; a < argc; ++a)
        if (juce::String (argv[a]) == "--manifest-only") manifestOnly = true;

    if (argc < 3)
    {
        std::fprintf (stderr, "usage: doobie_render_demo <source.wav> <outDir> [bpm=120] [--manifest-only]\n");
        return 2;
    }
    const double bpm = argc >= 4 && argv[3][0] != '-'
        ? juce::jlimit (20.0, 300.0, juce::String (argv[3]).getDoubleValue()) : 120.0;

    juce::ScopedJuceInitialiser_GUI juceInit; // some JUCE bits want a message manager

    const juce::File sourceFile (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1]));
    const juce::File outDir     (juce::File::getCurrentWorkingDirectory().getChildFile (argv[2]));

    if (! sourceFile.existsAsFile())
    {
        std::fprintf (stderr, "error: source file not found: %s\n", sourceFile.getFullPathName().toRawUTF8());
        return 2;
    }
    outDir.createDirectory();

    // ---- load the dry source loop -----------------------------------------
    juce::AudioFormatManager fmtMgr;
    fmtMgr.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (fmtMgr.createReaderFor (sourceFile));
    if (reader == nullptr)
    {
        std::fprintf (stderr, "error: could not read source audio (need wav/aiff/flac).\n");
        return 2;
    }

    const double sr = reader->sampleRate;
    const int    srcLen = (int) reader->lengthInSamples;
    juce::AudioBuffer<float> dry (2, srcLen);
    dry.clear();
    reader->read (&dry, 0, srcLen, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1) dry.copyFrom (1, 0, dry, 0, 0, srcLen); // mono -> dual mono

    // Tail so echoes and reverb ring out past the source.
    const int tail = (int) (sr * 6.0);
    const int total = srcLen + tail;

    std::fprintf (stdout, "source: %d samples @ %.0f Hz, +%d s tail per clip\n",
                  srcLen, sr, (int) (tail / sr));

    // dry reference, normalised the same way as the wet clips
    if (! manifestOnly)
    {
        juce::AudioBuffer<float> dryOut (dry);
        normalise (dryOut);
        writeWav (outDir.getChildFile ("dry.wav"), dryOut, sr);
    }

    // ---- render each factory preset ---------------------------------------
    // Every factory preset except the MIDI-note ones (they need incoming notes
    // to make sound). A fresh processor per preset keeps delay/reverb tails
    // from bleeding between renders.
    juce::StringArray names;
    {
        DoobieAudioProcessor probe;
        names = probe.getPresetManager().getFactoryNames();
    }

    const int block = 512;
    juce::String entries;
    int rendered = 0, skipped = 0;

    std::fprintf (stdout, "host tempo: %.0f BPM\n", bpm);

    for (int index = 0; index < names.size(); ++index)
    {
        const juce::String name = names[index];
        FixedTempoPlayHead playHead (bpm);          // declared before proc: outlives it
        DoobieAudioProcessor proc;
        proc.setPlayHead (&playHead);
        proc.setPlayConfigDetails (2, 2, sr, block);
        proc.prepareToPlay (sr, block);

        auto& apvts = proc.getValueTreeState();
        proc.getPresetManager().loadByName (name);

        if (paramIndex (apvts, dID::midiPitchMode) != 0)
        {
            std::fprintf (stdout, "  skipped  %-18s (MIDI note preset)\n", name.toRawUTF8());
            ++skipped;
            continue;
        }

        const Info info = describe (apvts, index);
        const juce::var patch = buildInfo (apvts);

        // index prefix keeps duplicate preset names (e.g. two "Cathedral")
        // from colliding on the same filename.
        const juce::String slug = juce::String (index).paddedLeft ('0', 2) + "-" + slugify (name);
        const juce::String ext = manifestOnly ? ".mp3" : ".wav";

        if (! manifestOnly)
        {
            juce::AudioBuffer<float> out (2, total);
            out.clear();
            juce::MidiBuffer midi;

            // Adaptive tail: after the source ends, keep rendering only while
            // the output still carries energy. Cuts the dead air off short
            // presets (slapbacks, gated) instead of padding all to +6 s.
            const float silentLin = juce::Decibels::decibelsToGain (-60.0f);
            int quietRun = 0, written = 0;
            const int quietNeed = (int) (sr * 0.35);

            for (int pos = 0; pos < total; pos += block)
            {
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
                written = pos + n;

                if (pos >= srcLen)
                {
                    quietRun = chunk.getMagnitude (0, n) < silentLin ? quietRun + n : 0;
                    if (quietRun >= quietNeed) break;   // tail has died out
                }
            }
            out.setSize (2, juce::jmax (written, (int) sr), true);   // keep at least 1 s

            proc.setPlayHead (nullptr);
            proc.releaseResources();
            normalise (out);

            const juce::File wav = outDir.getChildFile (slug + ".wav");
            if (! writeWav (wav, out, sr))
            {
                std::fprintf (stderr, "  FAILED to write %s\n", wav.getFullPathName().toRawUTF8());
                continue;
            }
            std::fprintf (stdout, "  rendered %-18s [%-14s] -> %s\n",
                          name.toRawUTF8(), info.category.toRawUTF8(), wav.getFileName().toRawUTF8());
        }
        ++rendered;

        if (entries.isNotEmpty()) entries << ",\n";
        entries << "    { \"slug\": " << juce::JSON::toString (juce::var (slug))
                << ", \"name\": "     << juce::JSON::toString (juce::var (name))
                << ", \"category\": " << juce::JSON::toString (juce::var (info.category))
                << ", \"tagline\": "  << juce::JSON::toString (juce::var (info.tagline))
                << ", \"info\": "     << juce::JSON::toString (patch, true)
                << ", \"file\": "     << juce::JSON::toString (juce::var (slug + ext)) << " }";
    }

    std::fprintf (stdout, "rendered %d presets, skipped %d MIDI presets\n", rendered, skipped);

    // ---- manifest ----------------------------------------------------------
    juce::String manifest;
    manifest << "{\n  \"dry\": \"dry" << (manifestOnly ? ".mp3" : ".wav")
             << "\",\n  \"presets\": [\n" << entries << "\n  ]\n}\n";
    outDir.getChildFile ("manifest.json").replaceWithText (manifest);

    std::fprintf (stdout, "wrote manifest -> %s\n",
                  outDir.getChildFile ("manifest.json").getFullPathName().toRawUTF8());
    return 0;
}
