#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <cmath>
#include <cstdint>
#include <algorithm>

#include "VoiceFilter.h"

namespace vespera
{

//==============================================================================
//  Compact polyphonic synth voice engine.
//
//  Two detuned oscillators per voice (sine / triangle / saw / square with
//  PolyBLEP anti-aliasing on the discontinuous waveforms), a standard ADSR, a
//  per-voice resonant filter and a small per-note pan spread.  It is
//  deliberately simple and clean: the character comes from the pattern
//  generator, the Tape macro and the second voice, not from a heavy oscillator
//  stack.
//==============================================================================
class PolySynth
{
public:
    // 24 voices leave enough headroom for triads / seventh chords with a long
    // release, so a sustained chord is not immediately eaten by voice stealing.
    static constexpr int kMaxVoices = 24;

    void prepare (double sampleRate)
    {
        sr = sampleRate;

        // reset() prepares every voice filter with the new sample rate.
        reset();

        // reset() clears the voices, so the envelope parameters have to be
        // pushed again; otherwise the first block would run on the JUCE defaults
        // until the user touches a voice knob.
        setParams (wave, attack, decay, sustain, release, detune);
    }

    void reset()
    {
        for (auto& v : voices)
        {
            v = {};
            // A default-constructed juce::ADSR assumes 44.1 kHz; without this the
            // amp envelope would run at the wrong speed on any other rate.
            v.adsr.setSampleRate (sr);
            v.filter.prepare (sr);
        }

        // The voices now hold default ADSRs, so the next setParams() must not
        // take its "nothing changed" early exit.
        paramsInitialised = false;
        ageCounter = 0;
    }

    void setParams (int newWave, float newAttack, float newDecay,
                    float newSustain, float newRelease, float newDetune)
    {
        const int   w  = juce::jlimit (0, 3, newWave);
        const float a  = juce::jlimit (0.001f, 4.0f, newAttack);
        const float d  = juce::jlimit (0.001f, 4.0f, newDecay);
        const float s  = juce::jlimit (0.0f, 1.0f, newSustain);
        const float r  = juce::jlimit (0.01f, 8.0f, newRelease);
        const float dt = juce::jlimit (0.0f, 1.0f, newDetune);

        // Called every block: skip the work when nothing actually moved.
        if (paramsInitialised && w == wave
            && ! changed (a, attack) && ! changed (d, decay)
            && ! changed (s, sustain) && ! changed (r, release)
            && ! changed (dt, detune))
            return;

        paramsInitialised = true;
        wave = w;
        attack = a;
        decay = d;
        sustain = s;
        release = r;
        detune = dt;

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

    // Per-voice filter settings.  The LFO is global, so it is passed in already
    // scaled and simply added to each voice's cutoff.
    void setFilterParams (const VoiceFilterParams& p)
    {
        filterParams = p;

        // Idle voices pick the settings up in startVoice(); updating them here
        // would cost 24 copies per render chunk for nothing.
        for (auto& v : voices)
            if (v.active)
                v.filter.setParams (filterParams);
    }

    // Only the modulation moves inside a block, so it gets its own cheap path.
    // A voice started after this call still gets the fresh value through
    // filterParams in startVoice().
    void setFilterModulation (float lfoOctaves) noexcept
    {
        filterParams.lfoOctaves = lfoOctaves;

        for (auto& v : voices)
            if (v.active)
                v.filter.setLfoOctaves (lfoOctaves);
    }

    void noteOff (int note)
    {
        for (auto& v : voices)
            if (v.active && v.note == note && v.remaining < 0)
            {
                v.adsr.noteOff();
                v.filter.noteOff();
            }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            if (v.active)
            {
                v.adsr.noteOff();
                v.filter.noteOff();
            }
    }

    void render (float* L, float* R, int numSamples)
    {
        if (numSamples <= 0)
            return;

        const double invSr = 1.0 / sr;

        // Detune is the same for every voice, so the ratio is computed once per
        // chunk instead of once per voice.
        const double detuneRatio = std::pow (2.0, (double) detune * 22.0 / 1200.0);

        for (auto& v : voices)
        {
            if (! v.active)
                continue;

            const double inc1 = v.freq * invSr;
            const double inc2 = inc1 * detuneRatio;

            for (int n = 0; n < numSamples; ++n)
            {
                if (v.remaining > 0)
                {
                    if (--v.remaining == 0)
                    {
                        v.adsr.noteOff();
                        v.filter.noteOff();
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
                const float s = 0.5f * (a + b) * env * v.level;

                // Filter the mono voice before the pan spread: a resonant peak
                // belongs to the note, not to the stereo image.
                const float f = v.filter.process (s);

                L[n] += f * v.gainL;
                R[n] += f * v.gainR;

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

        // Pitch, pan and level only depend on the note, so they are resolved
        // once at note-on instead of on every render chunk.
        double freq = 440.0;
        float  gainL = 0.7071068f, gainR = 0.7071068f;
        float  level = 0.128f;

        juce::ADSR adsr;
        VoiceFilter filter;
        uint64_t age = 0;
    };

    // Resolves the per-note values (pitch, pan law, voice level).
    static void voiceSetup (Voice& v) noexcept
    {
        v.freq = 440.0 * std::pow (2.0, (double) (v.note - 69) / 12.0);

        const float pan = juce::jlimit (-0.7f, 0.7f,
                                        ((float) (v.note % 12) / 11.0f - 0.5f) * 0.8f);
        v.gainL = std::cos ((pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);
        v.gainR = std::sin ((pan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi);
        v.level = 0.16f * v.velocity;
    }

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

        voiceSetup (*slot);

        // A stolen or freshly started voice has to pick up the current filter
        // settings before its first sample; setFilterParams() only reaches the
        // voices that existed at the time of the call.
        slot->filter.setParams (filterParams);
        slot->filter.noteOn (slot->note, slot->velocity);
    }

    static bool changed (float a, float b) noexcept { return std::abs (a - b) > 1.0e-7f; }

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
    bool   paramsInitialised = false;

    VoiceFilterParams filterParams;

    std::array<Voice, kMaxVoices> voices {};
    uint64_t ageCounter = 0;
};

} // namespace vespera
