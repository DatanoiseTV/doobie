// Offline UI snapshot tool. Instantiates the processor and its editor, lays it
// out, renders it to an image with the software renderer, and writes a PNG.
// Useful for verifying the layout without a running DAW or screen access.
//
// Usage: doobie_snapshot [output.png] [presetIndex]
#include "PluginProcessor.h"
#include "PluginEditor.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    const juce::String outPath = argc > 1 ? juce::String (argv[1]) : juce::String ("/tmp/doobie_shots/ui.png");
    const int presetIndex = argc > 2 ? juce::String (argv[2]).getIntValue() : -1;

    DoobieAudioProcessor processor;
    processor.prepareToPlay (44100.0, 512);
    if (presetIndex >= 0)
        processor.getPresetManager().loadFactory (presetIndex);

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    if (editor == nullptr)
    {
        std::fprintf (stderr, "no editor\n");
        return 1;
    }

    editor->setBounds (0, 0, editor->getWidth() > 0 ? editor->getWidth() : 1100,
                             editor->getHeight() > 0 ? editor->getHeight() : 720);

    const auto image = editor->createComponentSnapshot (editor->getLocalBounds(), false, 1.0f);

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
