#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include <algorithm>

namespace vespera
{

//==============================================================================
//  Resonant multimode filter (TPT state-variable, Zavalishin).
//
//  Low-pass / band-pass / high-pass with a musical resonance range.  The cutoff
//  is modulated by an envelope follower and by an external LFO value, which is
//  what gives the synth voice its "breathing" character.  The output is softly
//  clipped at a high threshold, so extreme resonance stays bounded without
//  affecting the passband.
//==============================================================================
class ResonantFilter
{
public:
    enum Type { Off = 0, LowPass = 1, BandPass = 2, HighPass = 3 };

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        attackCoeff  = 1.0f - std::exp (-1.0f / (0.002f * (float) sr));
        releaseCoeff = 1.0f - std::exp (-1.0f / (0.180f * (float) sr));
        reset();
    }

    void reset()
    {
        for (auto& s : st)
            s = {};
        env = 0.0f;
    }

    // lfoOctaves is already scaled by the caller (e.g. motion * 2.5).
    void process (float* L, float* R, int numSamples, int type,
                  float cutoffHz, float resonance, float envAmount,
                  float lfoOctaves)
    {
        if (type <= Off || numSamples <= 0)
            return;

        const float q = 0.707f + juce::jlimit (0.0f, 1.0f, resonance)
                                * juce::jlimit (0.0f, 1.0f, resonance) * 15.3f;
        const float envAmt = juce::jlimit (0.0f, 1.0f, envAmount);
        const float baseCutoff = juce::jlimit (20.0f, (float) sr * 0.45f, cutoffHz);

        int nextUpdate = 0;

        for (int n = 0; n < numSamples; ++n)
        {
            const float peak = juce::jmax (std::abs (L[n]), std::abs (R[n]));
            const float coeff = (peak > env) ? attackCoeff : releaseCoeff;
            env += coeff * (peak - env);

            if (! std::isfinite (env))
                env = 0.0f;

            if (n >= nextUpdate)
            {
                const float octaves = lfoOctaves + envAmt * 3.0f * env;
                const float fc = juce::jlimit (20.0f, (float) sr * 0.45f,
                                               baseCutoff * std::exp2 (octaves));
                updateCoeffs (fc, q);
                nextUpdate += 16;
            }

            L[n] = softClip (processSample (st[0], L[n], type));
            R[n] = softClip (processSample (st[1], R[n], type));
        }
    }

    float getEnvelope() const noexcept { return env; }

private:
    struct State { float ic1 = 0.0f, ic2 = 0.0f; };

    float processSample (State& s, float v0, int type) const noexcept
    {
        const float v3 = v0 - s.ic2;
        const float v1 = a1 * s.ic1 + a2 * v3;
        const float v2 = s.ic2 + a2 * s.ic1 + a3 * v3;
        s.ic1 = 2.0f * v1 - s.ic1;
        s.ic2 = 2.0f * v2 - s.ic2;

        if (type == BandPass)
            return v1;
        if (type == HighPass)
            return v0 - k * v1 - v2;
        return v2;
    }

    void updateCoeffs (float fc, float q) noexcept
    {
        g  = std::tan (juce::MathConstants<float>::pi * fc / (float) sr);
        k  = 1.0f / juce::jmax (0.1f, q);
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    static float softClip (float x) noexcept
    {
        if (! std::isfinite (x)) return 0.0f;
        if (x >  1.2f) return  1.2f + 0.8f * std::tanh ((x - 1.2f) / 0.8f);
        if (x < -1.2f) return -1.2f + 0.8f * std::tanh ((x + 1.2f) / 0.8f);
        return x;
    }

    double sr = 48000.0;
    float attackCoeff = 0.0f, releaseCoeff = 0.0f;
    float env = 0.0f;
    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    State st[2];
};

} // namespace vespera
