/*
  Doobie — analog dub delay  (Keinedelay/DFM hardware port)
  Copyright (C) 2026 DatanoiseTV

  GPL-3.0-or-later, WITHOUT ANY WARRANTY. Retain attribution to DatanoiseTV.
*/

#pragma once

#include <cmath>

namespace doobie
{
// Classic 6-stage first-order all-pass phaser with feedback. One instance per
// channel; give the right channel a phase offset for stereo motion. The sweep
// coefficient uses the bilinear map without prewarp (a = (1-w)/(1+w)) — for a
// moving notch that error is inaudible and it keeps tan() out of the sample
// loop entirely.
class Phaser
{
  public:
    static constexpr int kStages = 6;

    void prepare(double sampleRate)
    {
        sr_ = (float)sampleRate;
        reset();
    }

    void reset()
    {
        for(int i = 0; i < kStages; ++i)
            z_[i] = 0.0f;
        last_  = 0.0f;
        phase_ = phaseOffset_;
    }

    void setPhaseOffset(float cycles) noexcept { phaseOffset_ = cycles; }

    // per block: rate in Hz, depth/fb/mix 0..1 (fb capped below runaway)
    void setParams(float rateHz, float depth, float fb, float mix) noexcept
    {
        if(rateHz < 0.01f)
            rateHz = 0.01f;
        if(rateHz > 8.0f)
            rateHz = 8.0f;
        inc_   = rateHz / sr_;
        depth_ = depth < 0.0f ? 0.0f : (depth > 1.0f ? 1.0f : depth);
        fb_    = fb < 0.0f ? 0.0f : (fb > 0.9f ? 0.9f : fb);
        mix_   = mix < 0.0f ? 0.0f : (mix > 1.0f ? 1.0f : mix);
    }

    inline float process(float in) noexcept
    {
        phase_ += inc_;
        if(phase_ >= 1.0f)
            phase_ -= 1.0f;
        // triangle LFO (the classic phaser shape) sweeping the all-pass corner
        // around 800 Hz by up to +/-2 octaves (200 Hz .. 3.2 kHz at full depth)
        const float lfo = 4.0f * std::fabs(phase_ - 0.5f) - 1.0f;
        const float f   = 800.0f * exp2f(depth_ * 2.0f * lfo);
        const float w   = 3.14159265f * f / sr_;
        const float a   = (1.0f - w) / (1.0f + w);

        float x = in + fb_ * last_;
        for(int i = 0; i < kStages; ++i)
        {
            const float y = -a * x + z_[i];
            z_[i]         = x + a * y;
            x             = y;
        }
        if(!std::isfinite(x)) // never let the feedback path run away
        {
            x = 0.0f;
            reset();
        }
        last_ = x;
        return in * (1.0f - mix_) + x * mix_;
    }

  private:
    float sr_ = 48000.0f;
    float z_[kStages] = {};
    float last_ = 0.0f, phase_ = 0.0f, phaseOffset_ = 0.0f;
    float inc_ = 0.0f, depth_ = 0.7f, fb_ = 0.4f, mix_ = 0.5f;
};
} // namespace doobie
