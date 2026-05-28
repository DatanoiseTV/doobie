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
#include <cmath>
#include <cstdint>

namespace doobie
{
// Block-rate low-frequency oscillator for the mod matrix. Outputs a bipolar
// value in [-depth, +depth] each block. Waveform-switchable; S&H (Random)
// holds a new uniform value every period.
class Lfo
{
public:
    enum class Wave { Sine = 0, Triangle, SawUp, SawDown, Square, Random };

    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0f;
        held  = 0.0f;
        previousPhase = 0.0f;
        rngState = 0xC0FFEEu;
    }

    void setRate     (float hz)    noexcept { rate = juce::jmax (0.001f, hz); }
    void setDepth    (float d)     noexcept { depth = juce::jlimit (0.0f, 1.0f, d); }
    void setWaveform (Wave w)      noexcept { waveform = w; }

    // Advance the LFO by `numSamples` and return the post-advance value, in
    // [-depth, +depth]. Called once per block from the mod matrix.
    float advance (int numSamples) noexcept
    {
        const float deltaPhase = (float) (rate / sampleRate) * (float) numSamples;
        const float prev = phase;
        phase = std::fmod (phase + deltaPhase, 1.0f);
        previousPhase = prev;

        // Sample-and-hold: pick a new uniform value whenever the phase wraps.
        if (waveform == Wave::Random && (phase < prev || numSamples == 0))
            held = whiteNoise();

        return depth * compute();
    }

    // For UIs / tests.
    float currentPhase() const noexcept { return phase; }

private:
    float compute() const noexcept
    {
        constexpr float twoPi = 6.2831853f;
        switch (waveform)
        {
            case Wave::Sine:     return std::sin (twoPi * phase);
            case Wave::Triangle: return phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
            case Wave::SawUp:    return 2.0f * phase - 1.0f;
            case Wave::SawDown:  return 1.0f - 2.0f * phase;
            case Wave::Square:   return phase < 0.5f ? 1.0f : -1.0f;
            case Wave::Random:   return held;
        }
        return 0.0f;
    }

    inline float whiteNoise() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return (float) (int32_t) rngState * (1.0f / 2147483648.0f);
    }

    double sampleRate = 44100.0;
    float rate = 1.0f;
    float depth = 1.0f;
    Wave  waveform = Wave::Sine;
    float phase = 0.0f;
    float previousPhase = 0.0f;
    float held = 0.0f;
    uint32_t rngState = 0xC0FFEEu;
};
} // namespace doobie
