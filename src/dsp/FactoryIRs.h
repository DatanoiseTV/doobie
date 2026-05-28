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

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <vector>

namespace doobie
{
// Factory impulse responses generated at runtime. Real-space WAV IRs would mean
// bundling MBs of third-party data (and its licensing); we synthesise instead,
// which costs zero binary and is unambiguously the project's own code under the
// same GPLv3.
//
// Each IR is built from two parts:
//   * a handful of discrete early reflections (level decreasing with time,
//     spaced semi-randomly in the first 60..150 ms),
//   * a decorrelated stereo diffuse tail of exponentially-decaying noise, with
//     a slowly-closing low-pass so HF dies faster than LF (the "warming" of a
//     real room's late tail).
//
// At amount-zero seed values from std::hash on the name, results are
// reproducible — so two instances of the plugin or a session restore generate
// the same IR audio.
struct FactoryIRSpec
{
    const char* name;
    float lengthSec;
    float predelayMs;
    int   numER;
    float erMaxMs;
    float erLevel;          // initial ER amplitude (0..1)
    float tailStartHfHz;    // initial LP cutoff for the tail
    float tailEndHfHz;      // final LP cutoff (closed by the end of the tail)
};

inline juce::AudioBuffer<float> generateFactoryIR (const FactoryIRSpec& s, double sampleRate)
{
    const int len = juce::jmax (8, (int) std::ceil (sampleRate * (double) s.lengthSec));
    juce::AudioBuffer<float> buf (2, len);
    buf.clear();

    // Deterministic per-IR seeds so the same name yields the same audio.
    const juce::int64 seed = juce::String (s.name).hashCode64();
    juce::Random rngL (seed);
    juce::Random rngR (seed ^ static_cast<juce::int64> (0x9E3779B97F4A7C15ULL));

    const int preStart = juce::jlimit (0, len - 1, (int) std::round (sampleRate * s.predelayMs / 1000.0));
    const int erEnd    = juce::jlimit (preStart + 1, len, preStart + (int) std::round (sampleRate * s.erMaxMs / 1000.0));

    // Discrete early reflections.
    for (int i = 0; i < s.numER; ++i)
    {
        const float t   = (float) (i + 1) / (float) s.numER;
        const int   pos = juce::jlimit (preStart, len - 1,
                                        preStart + (int) ((float) (erEnd - preStart) * t * (0.6f + 0.4f * rngL.nextFloat())));
        const float amp = s.erLevel * (1.0f - 0.7f * t) * (rngL.nextFloat() > 0.5f ? 1.0f : -1.0f);
        buf.addSample (0, pos, amp);

        const int posR = juce::jlimit (preStart, len - 1, pos + (int) (rngR.nextFloat() * 12.0f - 6.0f));
        buf.addSample (1, posR, amp * 0.85f);
    }

    // Diffuse tail: exponentially-decaying low-passed white noise, decorrelated.
    const float decay = std::exp (std::log (0.001f) / (float) juce::jmax (1, len - preStart)); // ~-60 dB at end
    float envL = 1.0f, envR = 1.0f;
    float lpL  = 0.0f, lpR  = 0.0f;

    for (int i = preStart; i < len; ++i)
    {
        const float t    = (float) (i - preStart) / (float) juce::jmax (1, len - preStart);
        const float hf   = s.tailStartHfHz + (s.tailEndHfHz - s.tailStartHfHz) * t;
        const float coef = 1.0f - std::exp (-6.2831853f * hf / (float) sampleRate);

        const float nL = rngL.nextFloat() * 2.0f - 1.0f;
        const float nR = rngR.nextFloat() * 2.0f - 1.0f;
        lpL += coef * (nL - lpL);
        lpR += coef * (nR - lpR);

        envL *= decay;
        envR *= decay;

        buf.addSample (0, i, lpL * envL * 0.35f);
        buf.addSample (1, i, lpR * envR * 0.35f);
    }

    return buf;
}

// Factory catalogue. Keep names short — they appear in the editor's combo and
// in saved state. Index 0 must remain "Small Room"; adding new entries goes at
// the end so saved states keep their reference.
inline const std::vector<FactoryIRSpec>& factoryIRSpecs()
{
    static const std::vector<FactoryIRSpec> list {
        { "Small Room", 0.55f,  8.0f,  8, 60.0f,  0.30f, 8000.0f, 2800.0f },
        { "Big Room",   1.10f, 14.0f, 10, 90.0f,  0.25f, 6500.0f, 2000.0f },
        { "Hall",       1.80f, 22.0f,  8, 120.0f, 0.22f, 5500.0f, 1500.0f },
        { "Cathedral",  3.40f, 36.0f,  6, 160.0f, 0.18f, 4500.0f, 1100.0f },
        { "Cave",       2.20f, 18.0f,  8, 130.0f, 0.22f, 3500.0f,  900.0f },
        { "Tunnel",     1.50f, 12.0f, 14, 110.0f, 0.40f, 4500.0f, 1500.0f },
    };
    return list;
}

inline juce::StringArray factoryIRNames()
{
    juce::StringArray names;
    for (const auto& s : factoryIRSpecs())
        names.add (s.name);
    return names;
}

inline int numFactoryIRs() { return (int) factoryIRSpecs().size(); }
} // namespace doobie
