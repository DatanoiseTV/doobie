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

#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <vector>

namespace doobie
{
// Convolution-reverb wrapper that fits the per-sample API the engine's reverb
// switch uses, on top of JUCE's block-oriented partitioned dsp::Convolution.
//
// The engine processes one sample at a time; partitioned convolution is much
// happier with a block. We buffer kBlockSamples of incoming samples, run a
// single convolution per block, and return one sample at a time from the
// previous block's output. The cost is one block of latency on the reverb tail
// (~1.5 ms at 44.1 kHz) — inaudible against a reverb tail of hundreds of ms,
// and only on the wet path (the dry signal is unaffected).
//
// When no IR is loaded the wrapper returns the dry signal instantaneously, so
// the engine's wet/dry mix collapses to dry rather than dropping level or
// comb-filtering against a delayed pass-through.
class ConvolutionReverb
{
public:
    static constexpr int kBlockSamples = 64;

    ConvolutionReverb()
        : conv (juce::dsp::Convolution::NonUniform { 256 }) {}

    void prepare (double sr, int /*hostBlockSize*/) noexcept
    {
        sampleRate = sr;
        juce::dsp::ProcessSpec spec {
            sr, (juce::uint32) kBlockSamples, 2u };
        conv.prepare (spec);
        conv.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            inBuf[(size_t) ch].assign ((size_t) kBlockSamples, 0.0f);
            outBuf[(size_t) ch].assign ((size_t) kBlockSamples, 0.0f);
        }
        pos = 0;
    }

    void reset() noexcept
    {
        conv.reset();
        for (int ch = 0; ch < 2; ++ch)
        {
            std::fill (inBuf[(size_t) ch].begin(), inBuf[(size_t) ch].end(), 0.0f);
            std::fill (outBuf[(size_t) ch].begin(), outBuf[(size_t) ch].end(), 0.0f);
        }
        pos = 0;
    }

    enum class Source { None, Factory, File };

    // Async / thread-safe: JUCE's Convolution swaps the IR atomically on a
    // background loader thread, so this can be called from the message thread
    // while audio is processing.
    void loadFromFile (const juce::File& f)
    {
        conv.loadImpulseResponse (f,
                                  juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::yes,
                                  0,
                                  juce::dsp::Convolution::Normalise::yes);
        loadedFile  = f;
        factoryIdx  = -1;
        displayName = f.getFileName();
        source.store (Source::File, std::memory_order_release);
        loaded.store (true,         std::memory_order_release);
    }

    // Synchronous load from a generated AudioBuffer (factory IRs).
    void loadFromBuffer (juce::AudioBuffer<float>&& buffer, double bufferSampleRate,
                         int factoryIndex, const juce::String& label)
    {
        conv.loadImpulseResponse (std::move (buffer), bufferSampleRate,
                                  juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::no,
                                  juce::dsp::Convolution::Normalise::yes);
        loadedFile  = juce::File();
        factoryIdx  = factoryIndex;
        displayName = label;
        source.store (Source::Factory, std::memory_order_release);
        loaded.store (true,            std::memory_order_release);
    }

    void clear() noexcept
    {
        loaded.store (false, std::memory_order_release);
        source.store (Source::None, std::memory_order_release);
        loadedFile  = juce::File();
        factoryIdx  = -1;
        displayName = juce::String();
    }

    bool         hasIR()           const noexcept { return loaded.load (std::memory_order_acquire); }
    Source       getSource()       const noexcept { return source.load (std::memory_order_acquire); }
    int          getFactoryIndex() const noexcept { return factoryIdx; }
    juce::File   getLoadedFile()   const          { return loadedFile; }
    juce::String getDisplayName()  const          { return displayName; }

    inline void process (float l, float r, float& outL, float& outR) noexcept
    {
        if (! loaded.load (std::memory_order_relaxed))
        {
            // No IR: dry pass-through with zero latency so the engine's mix
            // formula collapses to dry instead of dropping level / comb-filtering.
            outL = l;
            outR = r;
            return;
        }

        inBuf[0][(size_t) pos] = l;
        inBuf[1][(size_t) pos] = r;
        outL = outBuf[0][(size_t) pos];
        outR = outBuf[1][(size_t) pos];
        ++pos;

        if (pos >= kBlockSamples)
        {
            float* channels[2] = { inBuf[0].data(), inBuf[1].data() };
            juce::dsp::AudioBlock<float> block (channels, 2, (size_t) kBlockSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            conv.process (ctx);
            // The block was processed in-place; swap so next round overwrites it.
            std::swap (inBuf[0], outBuf[0]);
            std::swap (inBuf[1], outBuf[1]);
            pos = 0;
        }
    }

private:
    juce::dsp::Convolution conv;
    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> inBuf, outBuf;
    int pos = 0;
    std::atomic<bool>   loaded { false };
    std::atomic<Source> source { Source::None };
    juce::File          loadedFile;
    int                 factoryIdx  = -1;
    juce::String        displayName;
};
} // namespace doobie
