#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace vespera
{

//==============================================================================
//  Compact polyphonic synth voice engine.
//
//  Two detuned oscillators per voice (sine / triangle / saw / square with
//  PolyBLEP anti-aliasing on the discontinuous waveforms), a standard ADSR and
//  a small per-note pan spread.  It is deliberately simple and clean: the
//  character comes from the pattern generator and the Character section, not
//  from a heavy oscillator stack.
//==============================================================================
class PolySynth
{
public:
    static constexpr int kMaxVoices = 16;

    void prepare (double sampleRate)
    {
        sr = sampleRate;
        setParams (wave, attack, decay, sustain, release, detune);
        reset();
    }

    void reset()
    {
        for (auto& v : voices)
            v = {};
        ageCounter = 0;
    }

    void setParams (int newWave, float newAttack, float newDecay,
                    float newSustain, float newRelease, float newDetune)
    {
        wave    = juce::jlimit (0, 3, newWave);
        attack  = juce::jlimit (0.001f, 4.0f, newAttack);
        decay   = juce::jlimit (0.001f, 4.0f, newDecay);
        sustain = juce::jlimit (0.0f, 1.0f, newSustain);
        release = juce::jlimit (0.01f, 8.0f, newRelease);
        detune  = juce::jlimit (0.0f, 1.0f, newDetune);

        juce::ADSR::Parameters p;
        p.attack  = attack;
        p.decay   = decay;
        p.sustain = sustain;
        p.release = release;

        for (auto& v : voices)
            v.adsr.setParameters (p);
    }

    void noteOn (int note, float velocity)
    {
        startVoice (note, velocity, -1);
    }

    void noteOnTimed (int note, float velocity, int durationSamples)
    {
        startVoice (note, velocity, juce::jmax (1, durationSamples));
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.remaining < 0)
                v.adsr.noteOff();
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active)
                v.adsr.noteOff();
    }

    void render (float* L, float* R, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const double invSr = 1.0 / sr;

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const double freq1 = 440.0 * std::pow (2.0, (double) (v.note - 69) / 12.0);
            const double cents = (double) detune * 22.0;
            const double freq2 = freq1 * std::pow (2.0, cents / 1200.0);

            const double inc1 = freq1 * invSr;
            const double inc2 = freq2 * invSr;

            const float pan = juce::jlimit (-0.7f, 0.7f,
                                            ((float) (v.note % 12) / 11.0f - 0.5f) * 0.8f);
            const float gainL = std::cos ((pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);
            const float gainR = std::sin ((pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);
            const float level = 0.16f * v.velocity;

            for (int n = 0; n < numSamples; ++n)
            {
                if (v.remaining > 0)
                {
                    if (--v.remaining == 0)
                    {
                        v.adsr.noteOff();
                        v.remaining = -1;
                    }
                }

                const float env = v.adsr.getNextSample();
                if (! v.adsr.isActive())
                {
                    v.active = false;
                    break;
                }

                const float a = oscillator (v.phase1, (float) inc1, wave);
                const float b = oscillator (v.phase2, (float) inc2, wave);
                const float s = 0.5f * (a + b) * env * level;

                L[n] += s * gainL;
                R[n] += s * gainR;

                v.phase1 += inc1;
                if (v.phase1 >= 1.0) v.phase1 -= 1.0;
                v.phase2 += inc2;
                if (v.phase2 >= 1.0) v.phase2 -= 1.0;
            }
        }
    }

    int getActiveVoiceCount() const noexcept
    {
        int n = 0;
        for (const auto& v : voices)
            if (v.active)
                ++n;
        return n;
    }

private:
    struct Voice
    {
        bool active = false;
        int  note = 60;
        int  remaining = -1;       // -1 = held, >0 = timed, 0 = released
        float velocity = 0.8f;
        double phase1 = 0.0;
        double phase2 = 0.25;
        juce::ADSR adsr;
        uint64_t age = 0;
    };

    void startVoice (int note, float velocity, int durationSamples)
    {
        Voice* slot = nullptr;

        for (auto& v : voices)
            if (! v.active) { slot = &v; break; }

        if (slot == nullptr)
        {
            // Steal the oldest active voice.
            slot = &voices[0];
            for (auto& v : voices)
                if (v.age < slot->age)
                    slot = &v;
        }

        slot->active = true;
        slot->note = juce::jlimit (0, 127, note);
        slot->velocity = juce::jlimit (0.0f, 1.0f, velocity);
        slot->remaining = durationSamples;
        slot->phase1 = 0.0;
        slot->phase2 = 0.25;
        slot->age = ++ageCounter;
        slot->adsr.reset();
        slot->adsr.noteOn();
    }

    static float polyBlep (float t, float dt) noexcept
    {
        if (dt <= 0.0f)
            return 0.0f;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0f;
        }

        if (t > 1.0f - dt)
        {
            t = (t - 1.0f) / dt;
            return t * t + t + t + 1.0f;
        }

        return 0.0f;
    }

    static float oscillator (double phase, float inc, int waveIndex) noexcept
    {
        const float t = (float) phase;

        switch (waveIndex)
        {
            case 0: // sine
                return std::sin (juce::MathConstants<float>::twoPi * t);

            case 1: // triangle
                return 1.0f - 4.0f * std::abs (t - 0.5f);

            case 2: // saw
                return 2.0f * t - 1.0f - polyBlep (t, inc);

            default: // square
            {
                float s = t < 0.5f ? 1.0f : -1.0f;
                s += polyBlep (t, inc);
                float t2 = t + 0.5f;
                if (t2 >= 1.0f) t2 -= 1.0f;
                s -= polyBlep (t2, inc);
                return s;
            }
        }
    }

    double sr = 48000.0;
    int    wave = 1;
    float  attack = 0.005f, decay = 0.25f, sustain = 0.6f, release = 0.4f, detune = 0.15f;

    std::array<Voice, kMaxVoices> voices {};
    uint64_t ageCounter = 0;
};

} // namespace vespera
