#include "PresetManager.h"
#include "../ParameterIDs.h"

namespace
{
    using PV = std::pair<juce::String, float>;

    // Shorthand for head parameter overrides.
    PV hl (int i, float v) { return { dID::headLevel[(size_t) i], v }; }
    PV hp (int i, float v) { return { dID::headPan[(size_t) i],   v }; }

    // The factory bank. Each preset lists only the values that differ from the
    // parameter defaults; everything else is reset before a preset is applied,
    // so loads are deterministic. Values are in real units (Hz, ms, indices).
    std::vector<PresetManager::Preset> buildFactory()
    {
        return {
            { "Classic Dub", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.62f },
                { dID::drive, 0.40f }, { dID::lpFreq, 3200.0f }, { dID::bass, 0.20f },
                { dID::reverbMode, 1 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.35f },
                { dID::wow, 0.25f }, { dID::flutter, 0.15f }, { dID::mix, 0.40f } } },

            { "Dub Siren", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.95f },
                { dID::lpFreq, 2600.0f }, { dID::hpFreq, 220.0f }, { dID::drive, 0.50f },
                { dID::reverbMode, 1 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.40f },
                { dID::mix, 0.45f } } },

            { "Tape Slap", {
                { dID::syncMode, 0 }, { dID::timeMs, 110.0f }, { dID::feedback, 0.12f },
                { dID::reverbMode, 0 }, { dID::drive, 0.50f }, { dID::wow, 0.30f },
                { dID::flutter, 0.25f }, { dID::mix, 0.30f } } },

            { "Multihead Cascade", {
                { dID::syncMode, 1 }, { dID::syncDiv, 7 }, { dID::modeSel, 11 },
                { dID::feedback, 0.45f }, hl (0, 0.85f), hl (1, 0.6f), hl (2, 0.6f), hl (3, 0.7f),
                hp (0, 0.0f), hp (1, -0.5f), hp (2, 0.5f), hp (3, 0.0f),
                { dID::reverbMode, 2 }, { dID::reverbMix, 0.30f }, { dID::mix, 0.45f } } },

            { "Ambient Wash", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::reverbMode, 4 },
                { dID::reverbRoute, 1 }, { dID::reverbMix, 0.60f }, { dID::plateDecay, 0.85f },
                { dID::plateSize, 0.80f }, { dID::feedback, 0.50f }, { dID::lpFreq, 8000.0f },
                { dID::mix, 0.60f } } },

            { "Dotted Skank", {
                { dID::syncMode, 1 }, { dID::syncDiv, 9 }, { dID::pingPong, 1 },
                { dID::feedback, 0.50f }, { dID::reverbMode, 2 }, { dID::reverbMix, 0.25f },
                { dID::mix, 0.40f } } },

            { "Spring Splash", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::reverbMode, 1 },
                { dID::reverbMix, 0.50f }, { dID::springDecay, 0.70f }, { dID::feedback, 0.40f },
                { dID::mix, 0.45f } } },

            { "Triplet Skank", {
                { dID::syncMode, 1 }, { dID::syncDiv, 5 }, { dID::modeSel, 6 },
                { dID::feedback, 0.50f }, { dID::pingPong, 1 }, { dID::reverbMode, 1 },
                { dID::mix, 0.40f } } },

            { "Outer Space", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::feedback, 0.85f },
                { dID::reverbMode, 4 }, { dID::reverbRoute, 2 }, { dID::reverbMix, 0.50f },
                { dID::plateDecay, 0.90f }, { dID::plateSize, 0.90f }, { dID::reverbMod, 0.60f },
                { dID::wow, 0.40f }, { dID::lpFreq, 4200.0f }, { dID::mix, 0.55f } } },

            { "Vintage Echo", {
                { dID::syncMode, 0 }, { dID::timeMs, 320.0f }, { dID::wow, 0.50f },
                { dID::flutter, 0.45f }, { dID::drive, 0.60f }, { dID::lpFreq, 2800.0f },
                { dID::hiss, 0.25f }, { dID::feedback, 0.50f }, { dID::reverbMode, 1 },
                { dID::mix, 0.40f } } },

            { "Clean Quarter", {
                { dID::syncMode, 1 }, { dID::syncDiv, 10 }, { dID::feedback, 0.30f },
                { dID::drive, 0.10f }, { dID::wow, 0.05f }, { dID::flutter, 0.03f },
                { dID::reverbMode, 0 }, { dID::lpFreq, 12000.0f }, { dID::mix, 0.30f } } },

            { "Cavern Plate", {
                { dID::syncMode, 1 }, { dID::syncDiv, 13 }, { dID::reverbMode, 3 },
                { dID::reverbRoute, 0 }, { dID::reverbMix, 0.50f }, { dID::plateDecay, 0.90f },
                { dID::plateSize, 0.85f }, { dID::springDecay, 0.60f }, { dID::feedback, 0.55f },
                { dID::mix, 0.50f } } },
        };
    }
}

PresetManager::PresetManager (juce::AudioProcessorValueTreeState& state)
    : apvts (state), factory (buildFactory())
{
    currentName = factory.empty() ? juce::String() : factory.front().name;
}

juce::File PresetManager::userPresetDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Doobie")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir;
}

juce::StringArray PresetManager::getFactoryNames() const
{
    juce::StringArray names;
    for (const auto& p : factory)
        names.add (p.name);
    return names;
}

juce::StringArray PresetManager::getUserNames() const
{
    juce::StringArray names;
    for (const auto& f : userPresetDirectory().findChildFiles (juce::File::findFiles, false, "*.xml"))
        names.add (f.getFileNameWithoutExtension());
    names.sort (true);
    return names;
}

juce::StringArray PresetManager::getAllNames() const
{
    auto names = getFactoryNames();
    names.addArray (getUserNames());
    return names;
}

int PresetManager::getCurrentIndex() const
{
    return getAllNames().indexOf (currentName);
}

void PresetManager::resetToDefaults()
{
    for (const auto& child : apvts.state)
    {
        const auto id = child.getProperty ("id").toString();
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->getDefaultValue());
    }
}

void PresetManager::applyPreset (const Preset& preset)
{
    resetToDefaults();
    for (const auto& [id, value] : preset.values)
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));

    currentName = preset.name;
}

void PresetManager::loadFactory (int index)
{
    if (juce::isPositiveAndBelow (index, (int) factory.size()))
        applyPreset (factory[(size_t) index]);
}

void PresetManager::loadByName (const juce::String& name)
{
    for (int i = 0; i < (int) factory.size(); ++i)
        if (factory[(size_t) i].name == name)
        {
            applyPreset (factory[(size_t) i]);
            return;
        }

    auto file = userPresetDirectory().getChildFile (name + ".xml");
    if (file.existsAsFile())
    {
        if (auto xml = juce::XmlDocument::parse (file))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            currentName = name;
        }
    }
}

void PresetManager::saveUser (const juce::String& name)
{
    if (name.isEmpty())
        return;

    auto state = apvts.copyState();
    if (auto xml = state.createXml())
    {
        xml->writeTo (userPresetDirectory().getChildFile (name + ".xml"));
        currentName = name;
    }
}

bool PresetManager::deleteUser (const juce::String& name)
{
    auto file = userPresetDirectory().getChildFile (name + ".xml");
    return file.existsAsFile() && file.deleteFile();
}

void PresetManager::next()
{
    auto names = getAllNames();
    if (names.isEmpty())
        return;
    int idx = (names.indexOf (currentName) + 1) % names.size();
    loadByName (names[idx]);
}

void PresetManager::previous()
{
    auto names = getAllNames();
    if (names.isEmpty())
        return;
    int idx = (names.indexOf (currentName) - 1 + names.size()) % names.size();
    loadByName (names[idx]);
}
