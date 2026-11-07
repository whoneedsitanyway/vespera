#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace vespera
{

//==============================================================================
//  Tape stage: one macro knob that makes the synth voice sound like it was
//  printed to tape.
//
//  The old Character section exposed drive / wow / noise as three independent
//  knobs.  Perceptually they overlapped, the first half of each range was
//  inaudible and the top of the range destroyed the melody.  This stage is
//  deliberately a *single* control with a musical response:
//
//    * a gentle high-frequency roll-off is audible from the first few percent,
//    * saturation is capped at a warm compression instead of a fuzz,
//    * wow / flutter never exceeds roughly 8 cents, so the pitch stays usable,
//    * hiss only appears in the upper third and is gated by the notes.
//
//  Amount 0 bypasses the stage completely (bit-transparent, no latency).
//==============================================================================
struct TapeParams
{
    float amount = 0.0f;   // 0..1
};

class Tape
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        const int len = (int) (sampleRate * 0.012) + 8;
        for (auto& b : lines)
            b.assign ((size_t) len, 0.0f);

        write = 0;
        wowPhase = 0.0;
        flutterPhase = 0.0;

        toneL = toneR = 0.0f;
        hissLp[0] = hissLp[1] = 0.0f;
        rngState = 0x9e3779b9u;

        activityEnv = 0.0f;
        attackCoeff  = 1.0f - std::exp (-1.0f / (0.003f * (float) sampleRate));
        releaseCoeff = 1.0f - std::exp (-1.0f / (0.140f * (float) sampleRate));
    }

    void reset()
    {
        for (auto& b : lines)
            std::fill (b.begin(), b.end(), 0.0f);

        write = 0;
        wowPhase = 0.0;
        flutterPhase = 0.0;
        toneL = toneR = 0.0f;
        hissLp[0] = hissLp[1] = 0.0f;
        activityEnv = 0.0f;
    }

    void process (float* L, float* R, int numSamples, const TapeParams& p)
    {
        const float amount = juce::jlimit (0.0f, 1.0f, p.amount);

        // Default state: keep the whole stage out of the per-sample path.
        if (amount < 0.001f)
        {
            activityEnv = 0.0f;
            return;
        }

        // Saturation rises quickly at the bottom of the range (so the knob is
        // useful immediately) but tops out at a warm, compressed tone.
        const float drive     = 0.60f * std::pow (amount, 0.55f);
        const float driveK    = 1.0f + drive * 3.0f;
        const float driveNorm = 1.0f / (std::tanh (driveK) * driveK);

        // The tone closes from 20 kHz to about 5 kHz across the range.  It is
        // the first thing you hear: even a small amount removes the digital
        // edge without sounding filtered.
        const float toneHz    = 20000.0f * std::pow (0.25f, amount);
        const float toneCoeff = 1.0f - std::exp (-juce::MathConstants<float>::twoPi
                                                 * toneHz / (float) sr);

        const float hiss      = juce::jmax (0.0f, (amount - 0.30f) / 0.70f);
        const float hissLevel = 0.022f * hiss * hiss;

        const float baseDelay = 0.0025f * (float) sr;
        const float depth     = 0.00055f * (float) sr * amount;
        const double dt       = 1.0 / sr;

        for (int n = 0; n < numSamples; ++n)
        {
            float l = std::isfinite (L[n]) ? L[n] : 0.0f;
            float r = std::isfinite (R[n]) ? R[n] : 0.0f;

            if (hissLevel > 0.0f)
            {
                const float mag = juce::jmax (std::abs (l), std::abs (r));
                activityEnv += (mag > activityEnv ? attackCoeff : releaseCoeff)
                                   * (mag - activityEnv);
            }

            // --- wow / flutter (pitch movement, never more than a few cents) --
            wowPhase += 0.55 * dt;
            if (wowPhase >= 1.0) wowPhase -= 1.0;
            flutterPhase += 4.20 * dt;
            if (flutterPhase >= 1.0) flutterPhase -= 1.0;

            const float modL = 0.80f * std::sin (juce::MathConstants<float>::twoPi
                                                     * (float) wowPhase)
                             + 0.20f * std::sin (juce::MathConstants<float>::twoPi
                                                     * (float) flutterPhase);
            const float modR = 0.80f * std::sin (juce::MathConstants<float>::twoPi
                                                     * (float) (wowPhase + 0.31))
                             + 0.20f * std::sin (juce::MathConstants<float>::twoPi
                                                     * (float) (flutterPhase + 0.47));

            l = readDelay (0, l, baseDelay + depth * modL);
            r = readDelay (1, r, baseDelay + depth * modR);

            if (++write >= (int) lines[0].size())
                write = 0;

            // --- saturation, level compensated ------------------------------
            const float sl = std::tanh (l * driveK) * driveNorm;
            const float sr2 = std::tanh (r * driveK) * driveNorm;
            l += (sl - l) * drive;
            r += (sr2 - r) * drive;

            // --- playback tone: smooths the new harmonics instead of fizz ----
            toneL += toneCoeff * (l - toneL);
            toneR += toneCoeff * (r - toneR);
            l = toneL;
            r = toneR;

            // --- hiss, gated by the notes and low-passed ---------------------
            if (hissLevel > 0.0f)
            {
                const float gate = juce::jlimit (0.0f, 1.0f, activityEnv * 6.0f);

                if (gate > 0.0001f)
                {
                    const float nl = white();
                    const float nr = white();
                    hissLp[0] += 0.12f * (nl - hissLp[0]);
                    hissLp[1] += 0.12f * (nr - hissLp[1]);

                    const float level = hissLevel * gate;
                    l += hissLp[0] * level;
                    r += hissLp[1] * level;
                }
            }

            L[n] = l;
            R[n] = r;
        }
    }

private:
    float readDelay (int ch, float x, float delaySamples) noexcept
    {
        auto& buf = lines[ch];
        const int len = (int) buf.size();
        if (len < 8)
            return x;

        buf[(size_t) write] = x;

        float rp = (float) write - juce::jlimit (1.0f, (float) (len - 4), delaySamples);
        while (rp < 0.0f) rp += (float) len;
        while (rp >= (float) len) rp -= (float) len;

        const int i0 = (int) rp;
        const int i1 = (i0 + 1 < len) ? i0 + 1 : 0;
        const float frac = rp - (float) i0;

        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
    }

    uint32_t nextRandom() noexcept
    {
        rngState ^= rngState << 13;
        rngState ^= rngState >> 17;
        rngState ^= rngState << 5;
        return rngState;
    }

    float white() noexcept
    {
        return (float) (nextRandom() & 0xffffffu) / 8388608.0f - 1.0f;
    }

    double sr = 48000.0;
    std::vector<float> lines[2];
    int write = 0;
    double wowPhase = 0.0, flutterPhase = 0.0;

    float toneL = 0.0f, toneR = 0.0f;
    float hissLp[2] { 0.0f, 0.0f };
    uint32_t rngState = 0x9e3779b9u;

    float activityEnv = 0.0f;
    float attackCoeff = 0.01f, releaseCoeff = 0.0002f;
};

} // namespace vespera
