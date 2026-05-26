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

#include "DelayLine.h"
#include <array>

namespace doobie
{
// A dense feedback-delay-network reverb: sixteen modulated delay lines mixed
// every sample through a 16x16 Hadamard matrix (lossless, so the tail decays
// only by the chosen gain and damping), fed by a chain of input diffusers and
// gently modulated. Sixteen lines plus diffusion give the thick, smooth,
// slowly-evolving tail of a big hall. The length range passed to prepare()
// sets the character (short = plate-ish room, long = cathedral).
class FdnReverb
{
public:
    void prepare (double sampleRate, float minMs, float maxMs);
    void reset();

    // decay/size/damp/mod in 0..1, predelay in ms.
    void setParams (float decay, float size, float damp, float predelayMs, float mod) noexcept;

    void process (float inL, float inR, float& outL, float& outR) noexcept;

private:
    static constexpr int N = 16;

    double sampleRate = 44100.0;

    std::array<DelayLine, N> lines;
    std::array<float, N> baseLen {};
    std::array<float, N> lpZ {};
    std::array<float, N> modPhase {};
    std::array<float, N> modRate {};
    std::array<float, N> cL {};   // output tap signs (left)
    std::array<float, N> cR {};   // output tap signs (right)
    std::array<float, N> bIn {};  // input injection signs

    std::array<DelayLine, 6> ap;
    std::array<float, 6> apLen {};
    std::array<float, 6> apG { 0.72f, 0.70f, 0.68f, 0.66f, 0.64f, 0.62f };

    DelayLine predelay;

    float g = 0.85f;
    float sizeScale = 1.0f;
    float dampCoef = 0.5f;
    float modDepth = 0.3f;
    float preLenSamples = 0.0f;
    float outGain = 0.4f;

    // Gentle input high-pass to keep the tail from turning to mud.
    float hpX = 0.0f, hpY = 0.0f, hpCoef = 0.995f;
};
} // namespace doobie
