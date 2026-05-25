#pragma once

#include <juce_dsp/juce_dsp.h>

namespace doobie
{
// The tone shaping that lives inside the feedback loop. Because it is applied
// on every pass, a small low-pass cut makes each successive repeat darker — the
// signature dub "echoes dissolving into the mix" behaviour — while the shelves
// and the high-pass let you ride the repeats without muddying the dry signal.
//
// One instance handles a single channel. Coefficients are shared (computed once
// in update) so left and right stay phase-matched.
class ToneStack
{
public:
    void prepare (const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        for (auto* f : { &hp, &lp, &low, &high })
            f->reset();
    }

    void reset()
    {
        for (auto* f : { &hp, &lp, &low, &high })
            f->reset();
    }

    // bass/treble in -1..1 (shelf gain), hp/lp in Hz.
    void update (float bass, float treble, float hpHz, float lpHz)
    {
        using Coefs = juce::dsp::IIR::Coefficients<float>;

        const auto lowGain  = juce::Decibels::decibelsToGain (bass   * 12.0f);
        const auto highGain = juce::Decibels::decibelsToGain (treble * 12.0f);

        hp.coefficients   = Coefs::makeHighPass  (sampleRate, juce::jlimit (20.0f, 2000.0f, hpHz));
        lp.coefficients   = Coefs::makeLowPass   (sampleRate, juce::jlimit (400.0f, 20000.0f, lpHz));
        low.coefficients  = Coefs::makeLowShelf  (sampleRate, 220.0f,  0.5f, lowGain);
        high.coefficients = Coefs::makeHighShelf (sampleRate, 3200.0f, 0.5f, highGain);
    }

    inline float process (float x) noexcept
    {
        x = hp.processSample (x);
        x = lp.processSample (x);
        x = low.processSample (x);
        x = high.processSample (x);
        return x;
    }

private:
    double sampleRate = 44100.0;
    juce::dsp::IIR::Filter<float> hp, lp, low, high;
};
} // namespace doobie
