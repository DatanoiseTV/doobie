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
#include <juce_audio_formats/juce_audio_formats.h>
#include <atomic>
#include <array>
#include <memory>
#include <vector>

namespace doobie
{
// Convolution-reverb wrapper that fits the per-sample API the engine's reverb
// switch uses, on top of JUCE's block-oriented partitioned dsp::Convolution.
//
// The wrapper internally buffers kBlockSamples of incoming samples and runs a
// single convolution per block, returning one sample at a time from the
// previous block's output. That introduces one block of latency on the reverb
// tail (~1.5 ms at 44.1 kHz) — inaudible against a reverb tail of hundreds of
// ms, and only on the wet path (the dry signal is unaffected).
//
// Extras over a plain Convolution:
//   * IR caching. The decoded IR samples + source rate are kept in memory so
//     the speed control can re-load the IR without re-decoding the source.
//   * IR gain. A smoothed multiplier on the wet output, so a quiet
//     peak-normalised IR can be brought up to a useful level without leaning
//     hard on the reverb MIX knob.
//   * IR speed. The convolution is re-loaded with a lying source sample rate
//     so JUCE's resampler stretches or compresses the IR (-2 .. +2 octaves):
//     longer / shorter, lower / higher pitched. Cool FX, dirt-cheap.
//
// When no IR is loaded the wrapper returns the dry signal instantaneously, so
// the engine's wet/dry mix collapses to dry rather than dropping level.
class ConvolutionReverb
{
public:
    enum class Source { None, Factory, File };

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

        smoothedGain.reset (sr, 0.02);            // ~20 ms: click-free gain ramps
        smoothedGain.setCurrentAndTargetValue (1.0f);

        if (! formatsRegistered)
        {
            formatManager.registerBasicFormats();  // WAV / AIFF / FLAC
            formatsRegistered = true;
        }
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

    // ---- IR loading ---------------------------------------------------------

    bool loadFromFile (const juce::File& f)
    {
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (f));
        if (reader == nullptr || ! decodeFrom (*reader))
            return false;
        loadedFile  = f;
        factoryIdx  = -1;
        displayName = f.getFileName();
        source.store (Source::File, std::memory_order_release);
        reloadFromCache();
        return true;
    }

    bool loadFromBinary (const void* data, size_t size,
                         int factoryIndex, const juce::String& label)
    {
        auto stream = std::make_unique<juce::MemoryInputStream> (data, size, false);
        std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (std::move (stream)));
        if (reader == nullptr || ! decodeFrom (*reader))
            return false;
        loadedFile  = juce::File();
        factoryIdx  = factoryIndex;
        displayName = label;
        source.store (Source::Factory, std::memory_order_release);
        reloadFromCache();
        return true;
    }

    void clear() noexcept
    {
        source.store (Source::None, std::memory_order_release);
        loadedFile  = juce::File();
        factoryIdx  = -1;
        displayName = juce::String();
        cachedBuffer.setSize (0, 0);
    }

    // Reload the current IR with a different playback speed multiplier
    // (0.25 = half speed = an octave down + double length; 4.0 = quad speed =
    // two octaves up + quarter length). 1.0 is normal. Not real-time safe: do
    // not call from the audio thread — wire it through an APVTS listener.
    bool setSpeed (float speed)
    {
        const float clamped = juce::jlimit (0.25f, 4.0f, speed);
        if (std::abs (clamped - currentSpeed) < 1.0e-4f)
            return false;
        currentSpeed = clamped;
        if (source.load (std::memory_order_acquire) == Source::None)
            return false;
        reloadFromCache();
        return true;
    }
    float getSpeed() const noexcept { return currentSpeed; }

    // Output gain on the wet (post-convolution) signal, in linear units.
    // Smoothed per-sample so knob twiddling doesn't click. Real-time safe.
    void setGain (float linearGain) noexcept
    {
        smoothedGain.setTargetValue (linearGain);
    }

    // ---- Queries ------------------------------------------------------------

    bool         hasIR()           const noexcept { return source.load (std::memory_order_acquire) != Source::None; }
    Source       getSource()       const noexcept { return source.load (std::memory_order_acquire); }
    int          getFactoryIndex() const noexcept { return factoryIdx; }
    juce::File   getLoadedFile()   const          { return loadedFile; }
    juce::String getDisplayName()  const          { return displayName; }

    // The decoded IR data the visualiser draws. Lives until the next load /
    // clear; safe to read from the message thread (loads only mutate it on
    // the same thread that calls loadFrom*).
    const juce::AudioBuffer<float>& getCachedIR()       const noexcept { return cachedBuffer; }
    double                          getCachedSourceSr() const noexcept { return cachedSourceSr; }

    // ---- Audio process ------------------------------------------------------

    inline void process (float l, float r, float& outL, float& outR) noexcept
    {
        // Advance the gain smoother every sample regardless, so it tracks the
        // host's parameter changes even when bypassed.
        const float g = smoothedGain.getNextValue();

        if (source.load (std::memory_order_relaxed) == Source::None)
        {
            // No IR: dry pass-through with zero latency.
            outL = l;
            outR = r;
            return;
        }

        inBuf[0][(size_t) pos] = l;
        inBuf[1][(size_t) pos] = r;
        outL = outBuf[0][(size_t) pos] * g;
        outR = outBuf[1][(size_t) pos] * g;
        ++pos;

        if (pos >= kBlockSamples)
        {
            float* channels[2] = { inBuf[0].data(), inBuf[1].data() };
            juce::dsp::AudioBlock<float> block (channels, 2, (size_t) kBlockSamples);
            juce::dsp::ProcessContextReplacing<float> ctx (block);
            conv.process (ctx);
            // Result is in-place in inBuf; swap so the next round overwrites it.
            std::swap (inBuf[0], outBuf[0]);
            std::swap (inBuf[1], outBuf[1]);
            pos = 0;
        }
    }

private:
    bool decodeFrom (juce::AudioFormatReader& reader)
    {
        const auto len   = (int) juce::jmax<juce::int64> (0, reader.lengthInSamples);
        const int  numCh = (int) juce::jmin<unsigned int> (2u, reader.numChannels);
        if (len <= 0 || numCh < 1)
            return false;
        cachedBuffer.setSize (numCh, len, false, true, true);
        reader.read (&cachedBuffer, 0, len, 0, true, numCh > 1);
        cachedSourceSr = reader.sampleRate;
        return true;
    }

    void reloadFromCache()
    {
        if (cachedBuffer.getNumSamples() == 0)
            return;
        // loadImpulseResponse takes the buffer by rvalue (consumes it); we want
        // to keep the cache so the next speed change can re-load.
        juce::AudioBuffer<float> copy (cachedBuffer.getNumChannels(), cachedBuffer.getNumSamples());
        copy.makeCopyOf (cachedBuffer);
        const double effectiveSr = cachedSourceSr * (double) currentSpeed;
        conv.loadImpulseResponse (std::move (copy), effectiveSr,
                                  juce::dsp::Convolution::Stereo::yes,
                                  juce::dsp::Convolution::Trim::yes,
                                  juce::dsp::Convolution::Normalise::yes);
    }

    juce::dsp::Convolution conv;
    double sampleRate = 44100.0;
    std::array<std::vector<float>, 2> inBuf, outBuf;
    int pos = 0;

    juce::AudioFormatManager formatManager;
    bool formatsRegistered = false;

    juce::AudioBuffer<float> cachedBuffer;
    double cachedSourceSr = 44100.0;
    float  currentSpeed   = 1.0f;

    juce::SmoothedValue<float> smoothedGain { 1.0f };

    std::atomic<Source> source { Source::None };
    juce::File          loadedFile;
    int                 factoryIdx  = -1;
    juce::String        displayName;
};
} // namespace doobie
