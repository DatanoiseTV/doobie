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

#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Manages factory and user presets on top of the APVTS. Factory presets are
// defined in code (see PresetManager.cpp); user presets are stored as XML in
// the platform's application-data folder so they survive plugin updates.
class PresetManager
{
public:
    // One named bundle of parameter values (in real, un-normalised units).
    struct Preset
    {
        juce::String name;
        std::vector<std::pair<juce::String, float>> values;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& state);

    juce::StringArray getFactoryNames() const;
    juce::StringArray getUserNames() const;

    void loadFactory (int index);
    void loadByName (const juce::String& name);   // factory or user
    void saveUser (const juce::String& name);
    bool deleteUser (const juce::String& name);

    void next();
    void previous();

    juce::String getCurrentName() const { return currentName; }
    int getCurrentIndex() const;           // index into the combined list
    juce::StringArray getAllNames() const; // factory first, then user

    static juce::File userPresetDirectory();

private:
    void applyPreset (const Preset& preset);
    void resetToDefaults();

    juce::AudioProcessorValueTreeState& apvts;
    std::vector<Preset> factory;
    juce::String currentName;
};
