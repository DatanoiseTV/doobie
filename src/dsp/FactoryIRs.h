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

#include <juce_core/juce_core.h>

namespace doobie
{
// Factory impulse responses bundled with Doobie. The set comes from Voxengo's
// free royalty-free IR pack (© Aleksey Vaneev, see
// external/voxengo-irs/license.txt) — 38 real-room and effect IRs at
// 44.1 kHz / 16-bit stereo. JUCE Convolution resamples them to the host's
// sample rate on load, so any session rate works.
//
// The list is auto-discovered from JUCE BinaryData at runtime, so adding or
// removing WAV files in external/voxengo-irs/ requires no code change — only
// a CMake re-configure.

int               numFactoryIRs();
juce::String      factoryIRName  (int index);
juce::StringArray factoryIRNames();

// Raw WAV bytes for the IR at `index`, wrapping the BinaryData symbol. Empty
// block if out of range. JUCE's dsp::Convolution parses the WAV header itself.
juce::MemoryBlock factoryIRData  (int index);
} // namespace doobie
