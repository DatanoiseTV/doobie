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

#include "FactoryIRs.h"

#include <BinaryData.h>
#include <algorithm>
#include <vector>

namespace doobie
{
namespace
{
    struct Entry
    {
        juce::String display;     // shown in the editor combo, e.g. "Block Inside"
        const char*  binaryName;  // identifier for BinaryData::getNamedResource
    };

    // Built once on first access, then immutable. Auto-discovers every WAV in
    // the BinaryData target so the IR set is whatever CMake glob picked up.
    const std::vector<Entry>& entries()
    {
        static const std::vector<Entry> list = []
        {
            std::vector<Entry> v;
            v.reserve ((size_t) BinaryData::namedResourceListSize);
            for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            {
                const auto raw = juce::String (BinaryData::originalFilenames[i]);
                if (raw.endsWithIgnoreCase (".wav"))
                    v.push_back ({ raw.dropLastCharacters (4),
                                   BinaryData::namedResourceList[i] });
            }
            std::sort (v.begin(), v.end(),
                       [] (const Entry& a, const Entry& b)
                       { return a.display.compareNatural (b.display) < 0; });
            return v;
        }();
        return list;
    }
}

int numFactoryIRs() { return (int) entries().size(); }

juce::String factoryIRName (int index)
{
    const auto& e = entries();
    if (! juce::isPositiveAndBelow (index, (int) e.size()))
        return {};
    return e[(size_t) index].display;
}

juce::StringArray factoryIRNames()
{
    juce::StringArray names;
    for (const auto& e : entries())
        names.add (e.display);
    return names;
}

juce::MemoryBlock factoryIRData (int index)
{
    const auto& e = entries();
    if (! juce::isPositiveAndBelow (index, (int) e.size()))
        return {};
    int dataSize = 0;
    const char* data = BinaryData::getNamedResource (e[(size_t) index].binaryName, dataSize);
    if (data == nullptr || dataSize <= 0)
        return {};
    return juce::MemoryBlock (data, (size_t) dataSize);
}
} // namespace doobie
