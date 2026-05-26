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

// Offline UI snapshot tool. Instantiates the processor and its editor, lays it
// out, renders it to an image with the software renderer, and writes a PNG.
// Useful for verifying the layout without a running DAW or screen access.
//
// Usage: doobie_snapshot [output.png] [presetIndex] [scale]
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    const juce::String outPath = argc > 1 ? juce::String (argv[1]) : juce::String ("/tmp/doobie_shots/ui.png");
    const int   presetIndex = argc > 2 ? juce::String (argv[2]).getIntValue() : -1;
    const float scale       = argc > 3 ? juce::jlimit (1.0f, 4.0f, (float) juce::String (argv[3]).getDoubleValue()) : 2.0f;

    DoobieAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);
    if (presetIndex >= 0)
        processor.getPresetManager().loadFactory (presetIndex);

    // Audio health probe: push ~1 s of white noise (then silence) through the
    // processor and report output level / finiteness, so DSP-heavy presets
    // (Pitch, Shimmer) can be checked without a DAW.
    {
        juce::Random rng;
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> buf (2, 512);
        double sumSq = 0.0; int counted = 0; bool finite = true;
        for (int block = 0; block < 160; ++block) // ~1.85 s at 44.1k/512
        {
            const bool noiseOn = block < 86;
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getWritePointer (ch);
                for (int i = 0; i < 512; ++i)
                    d[i] = noiseOn ? (rng.nextFloat() * 2.0f - 1.0f) * 0.3f : 0.0f;
            }
            processor.processBlock (buf, midi);
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = buf.getReadPointer (ch);
                for (int i = 0; i < 512; ++i)
                {
                    if (! std::isfinite (d[i])) finite = false;
                    if (block >= 30) { sumSq += (double) d[i] * d[i]; ++counted; }
                }
            }
        }
        const double rms = counted > 0 ? std::sqrt (sumSq / counted) : 0.0;
        std::printf ("audio probe: finite=%s rms=%.4f\n", finite ? "yes" : "NO", rms);
        processor.reset();
        processor.prepareToPlay (44100.0, 512);
        if (presetIndex >= 0)
            processor.getPresetManager().loadFactory (presetIndex);
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    editor->setBounds (0, 0, editor->getWidth() > 0 ? editor->getWidth() : 1100,
                             editor->getHeight() > 0 ? editor->getHeight() : 720);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), false, scale);

    juce::File out (outPath);
    out.getParentDirectory().createDirectory();
    out.deleteFile();

    juce::FileOutputStream stream (out);
    if (! stream.openedOk())
    {
        std::fprintf (stderr, "cannot open %s\n", outPath.toRawUTF8());
        return 1;
    }
    juce::PNGImageFormat png;
    if (! png.writeImageToStream (image, stream))
    {
        std::fprintf (stderr, "png write failed\n");
        return 1;
    }

    std::printf ("wrote %s (%d x %d)\n", outPath.toRawUTF8(), image.getWidth(), image.getHeight());
    return 0;
}
