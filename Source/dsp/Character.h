#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace vespera
{

//==============================================================================
//  Character stage: tape wow, saturation, bit-crush, noise, wave folding and
//  ring modulation.
//
//  This is the "RC-20" half of Vespera: it does not add space, it adds
//  *material* character.  Crush and noise use a squared response curve, so the
//  first part of the knob range stays subtle, and the noise is gated by an
//  envelope follower on the synth output - it only appears while notes sound.
//==============================================================================
struct CharacterParams
{
    float drive = 0.0f;   // 0..1
    float crush = 0.0f;   // 0..1
    float noise = 0.0f;   // 0..1
    float wow   = 0.0f;   // 0..1
    float fold  = 0.0f;   // 0..1
    float ring  = 0.0f;   // 0..1
};

class Character
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;

        const int len = (int) (sampleRate * 0.06) + 8;
        for (auto& b : wowLines)
            b.assign ((size_t) len, 0.0f);

        wowWrite = 0;
        wowPhase = 0.0;
        flutterPhase = 0.0;

        holdL = holdR = 0.0f;
        holdCounter = 0;

        noiseLp[0] = noiseLp[1] = 0.0f;
        rngState = 0x9e3779b9u;

        activityEnv = 0.0f;
        attackCoeff  = 1.0f - std::exp (-1.0f / (0.003f * (float) sampleRate));
        releaseCoeff = 1.0f - std::exp (-1.0f / (0.140f * (float) sampleRate));
    }

    void reset()
    {
        for (auto& b : wowLines)
            std::fill (b.begin(), b.end(), 0.0f);

        wowWrite = 0;
        wowPhase = 0.0;
        flutterPhase = 0.0;
        holdL = holdR = 0.0f;
        holdCounter = 0;
        noiseLp[0] = noiseLp[1] = 0.0f;
        activityEnv = 0.0f;
        ringPhase = 0.0;
    }

    void process (float* L, float* R, int numSamples, const CharacterParams& p)
    {
        const float drive = juce::jlimit (0.0f, 1.0f, p.drive);

        // Squared curves: the useful range spreads over the whole knob instead
        // of slamming the effect in as soon as the knob leaves zero.
        const float crush = juce::jlimit (0.0f, 1.0f, p.crush);
        const float crushAmount = crush * crush;
        const float noise = juce::jlimit (0.0f, 1.0f, p.noise);
        const float noiseAmount = noise * noise;
        const float wow   = juce::jlimit (0.0f, 1.0f, p.wow);
        const float fold  = juce::jlimit (0.0f, 1.0f, p.fold);
        const float ring  = juce::jlimit (0.0f, 1.0f, p.ring);

        const int   holdSamples = 1 + (int) std::round (crushAmount * 28.0f);
        const float bits = 16.0f - crushAmount * 13.0f;
        const float levels = std::pow (2.0f, bits - 1.0f);

        // Every effect is off: nothing to do, so keep the whole stage out of the
        // per-sample path (the default state of a fresh instance).
        if (drive < 0.001f && crushAmount < 0.001f && noiseAmount < 0.001f
            && wow < 0.001f && fold < 0.001f && ring < 0.001f)
        {
            activityEnv = 0.0f;
            return;
        }

        const float baseDelay  = 0.0020f * (float) sr;
        const float depth      = 0.0018f * (float) sr * wow;
        const double dt        = 1.0 / sr;

        // Ring modulation: low settings give a gentle tremolo, high settings a
        // metallic sideband cluster.
        const float ringDepth = ring;
        const float ringHz    = 35.0f + 900.0f * ring * ring;
        const double ringInc  = (double) ringHz * dt;

        for (int n = 0; n < numSamples; ++n)
        {
            float l = std::isfinite (L[n]) ? L[n] : 0.0f;
            float r = std::isfinite (R[n]) ? R[n] : 0.0f;

            // --- note activity follower (drives the noise gate) --------------
            if (noiseAmount > 0.001f)
            {
                const float mag = juce::jmax (std::abs (l), std::abs (r));
                activityEnv += (mag > activityEnv ? attackCoeff : releaseCoeff)
                                   * (mag - activityEnv);
            }

            // --- tape wow / flutter -----------------------------------------
            // The delay line is only used when wow is engaged, so the plugin
            // stays sample-aligned with the host at the default setting.
            if (wow > 0.001f)
            {
                wowPhase += 0.70 * dt;
                if (wowPhase >= 1.0) wowPhase -= 1.0;
                flutterPhase += 3.10 * dt;
                if (flutterPhase >= 1.0) flutterPhase -= 1.0;

                const float modL = 0.75f * std::sin (juce::MathConstants<float>::twoPi * (float) wowPhase)
                                 + 0.25f * std::sin (juce::MathConstants<float>::twoPi * (float) flutterPhase);
                const float modR = 0.75f * std::sin (juce::MathConstants<float>::twoPi * (float) (wowPhase + 0.27))
                                 + 0.25f * std::sin (juce::MathConstants<float>::twoPi * (float) (flutterPhase + 0.41));

                const float dL = baseDelay + depth * modL;
                const float dR = baseDelay + depth * modR;

                l = wowRead (0, l, dL);
                r = wowRead (1, r, dR);

                if (++wowWrite >= (int) wowLines[0].size())
                    wowWrite = 0;
            }

            // --- saturation -------------------------------------------------
            if (drive > 0.001f)
            {
                const float k = 1.0f + drive * 6.0f;
                const float makeup = 1.0f / (1.0f + drive * 2.0f);
                const float sl = std::tanh (l * k) / std::tanh (k) * makeup;
                const float sr2 = std::tanh (r * k) / std::tanh (k) * makeup;
                l += (sl - l) * drive;
                r += (sr2 - r) * drive;
            }

            // --- wave folding -----------------------------------------------
            if (fold > 0.001f)
            {
                l = foldSample (l, fold);
                r = foldSample (r, fold);
            }

            // --- ring modulation --------------------------------------------
            if (ringDepth > 0.001f)
            {
                ringPhase += ringInc;
                if (ringPhase >= 1.0) ringPhase -= 1.0;

                const float mod = std::sin (juce::MathConstants<float>::twoPi * (float) ringPhase);
                const float g = (1.0f - ringDepth) + mod * ringDepth;
                l *= g;
                r *= g;
            }

            // --- bit / sample-rate crush ------------------------------------
            if (crushAmount > 0.001f)
            {
                if (holdCounter <= 0)
                {
                    holdL = quantise (l, levels);
                    holdR = quantise (r, levels);
                    holdCounter = holdSamples;
                }
                --holdCounter;

                l += (holdL - l) * crushAmount;
                r += (holdR - r) * crushAmount;
            }

            // --- hiss and vinyl crackle, gated by the notes ------------------
            if (noiseAmount > 0.001f)
            {
                const float gate = juce::jlimit (0.0f, 1.0f, activityEnv * 6.0f);

                if (gate > 0.0001f)
                {
                    const float nl = white();
                    const float nr = white();
                    noiseLp[0] += 0.28f * (nl - noiseLp[0]);
                    noiseLp[1] += 0.28f * (nr - noiseLp[1]);

                    const float level = 0.05f * noiseAmount * gate;
                    l += noiseLp[0] * level;
                    r += noiseLp[1] * level;

                    if ((nextRandom() & 0xffffu) == 0u)
                    {
                        const float click = ((nextRandom() & 1u) ? 1.0f : -1.0f)
                                                * 0.05f * noiseAmount * gate;
                        l += click;
                        r += click;
                    }
                }
            }

            L[n] = l;
            R[n] = r;
        }
    }

private:
    float wowRead (int ch, float x, float delaySamples) noexcept
    {
        auto& buf = wowLines[ch];
        const int len = (int) buf.size();
        if (len < 8)
            return x;

        buf[(size_t) wowWrite] = x;

        float rp = (float) wowWrite - juce::jlimit (1.0f, (float) (len - 4), delaySamples);
        while (rp < 0.0f) rp += (float) len;
        while (rp >= (float) len) rp -= (float) len;

        const int i0 = (int) rp;
        const int i1 = (i0 + 1 < len) ? i0 + 1 : 0;
        const float frac = rp - (float) i0;

        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * frac;
    }

    static float quantise (float x, float levels) noexcept
    {
        return std::round (x * levels) / levels;
    }

    // Triangle wavefolder: transparent at 0, increasingly metallic as it rises.
    static float foldSample (float x, float amount) noexcept
    {
        const float drive = 1.0f + amount * 5.0f;
        float y = x * drive;

        y = std::fmod (y + 1.0f, 4.0f);
        if (y < 0.0f)
            y += 4.0f;
        y -= 1.0f;

        if (y > 1.0f)
            y = 2.0f - y;

        return y;
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
    std::vector<float> wowLines[2];
    int wowWrite = 0;
    double wowPhase = 0.0, flutterPhase = 0.0;

    float holdL = 0.0f, holdR = 0.0f;
    int   holdCounter = 0;

    float noiseLp[2] { 0.0f, 0.0f };
    uint32_t rngState = 0x9e3779b9u;

    double ringPhase = 0.0;

    float activityEnv = 0.0f;
    float attackCoeff = 0.01f, releaseCoeff = 0.0002f;
};

} // namespace vespera
