#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace vespera
{

//==============================================================================
//  Per-voice resonant filter.
//
//  The old filter sat after the mix, so one cutoff envelope reacted to the sum
//  of every note: accents, chords and the second voice all blurred into a single
//  "loudness" value.  Musical articulation needs the opposite — every note opens
//  its own filter and closes it again, and the brightness follows the pitch and
//  the velocity of *that* note.
//
//  This stage therefore lives inside the voice: one TPT state-variable filter
//  plus one ADSR per note.  The LFO stays global (it is a slow, shared movement)
//  and is added on top in octaves.
//==============================================================================
struct VoiceFilterParams
{
    int   type       = 1;        // 0 Off, 1 LP, 2 BP, 3 HP
    float cutoffHz   = 8000.0f;
    float resonance  = 0.15f;    // 0..1
    float envAmount  = 0.35f;    // 0..1 -> up to 3 octaves
    float keyTrack   = 0.5f;     // 0..1 -> cutoff follows the note
    float velAmount  = 0.4f;     // 0..1 -> hard notes are brighter
    float lfoOctaves = 0.0f;     // global modulation, already scaled
    float attack     = 0.004f;
    float decay      = 0.30f;
    float sustain    = 0.25f;
    float release    = 0.20f;
};

class VoiceFilter
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        adsr.setSampleRate (sampleRate);
        applyEnvelope();
        reset();
    }

    void reset()
    {
        ic1 = ic2 = 0.0f;
        env = 0.0f;
        countdown = 0;
        note = 60;
        velocity = 0.8f;
        adsr.reset();
    }

    // Called on every note-on: the cutoff envelope starts from zero, so each
    // note has its own attack instead of inheriting the previous one.
    void noteOn (int midiNote, float vel)
    {
        note = juce::jlimit (0, 127, midiNote);
        velocity = juce::jlimit (0.0f, 1.0f, vel);
        ic1 = ic2 = 0.0f;
        env = 0.0f;
        countdown = 0;
        adsr.reset();
        adsr.noteOn();
    }

    void noteOff() { adsr.noteOff(); }

    void setParams (const VoiceFilterParams& p)
    {
        type       = juce::jlimit (0, 3, p.type);
        cutoffHz   = juce::jlimit (20.0f, 18000.0f, p.cutoffHz);
        resonance  = juce::jlimit (0.0f, 1.0f, p.resonance);
        envAmount  = juce::jlimit (0.0f, 1.0f, p.envAmount);
        keyTrack   = juce::jlimit (0.0f, 1.0f, p.keyTrack);
        velAmount  = juce::jlimit (0.0f, 1.0f, p.velAmount);
        lfoOctaves = juce::jlimit (-6.0f, 6.0f, p.lfoOctaves);

        // The ADSR parameters are only pushed when they actually move: the UI
        // can leave these knobs untouched for a whole session, and rebuilding
        // the envelope coefficients for 24 voices every chunk is wasted work.
        if (! envelopeInitialised
            || std::abs (p.attack  - adsrAttack)  > 1.0e-6f
            || std::abs (p.decay   - adsrDecay)   > 1.0e-6f
            || std::abs (p.sustain - adsrSustain) > 1.0e-6f
            || std::abs (p.release - adsrRelease) > 1.0e-6f)
        {
            adsrAttack  = juce::jlimit (0.001f, 2.0f, p.attack);
            adsrDecay   = juce::jlimit (0.001f, 2.0f, p.decay);
            adsrSustain = juce::jlimit (0.0f, 1.0f, p.sustain);
            adsrRelease = juce::jlimit (0.01f, 2.0f, p.release);
            applyEnvelope();
        }
    }

    float process (float x) noexcept
    {
        // The envelope has to advance even when the filter is bypassed: turning
        // the type back on must not resume from a stale cutoff value.
        env = adsr.getNextSample();

        if (type <= 0)
        {
            // Keep the filter state at rest while bypassed: switching the type
            // back on must not replay an old resonant tail.
            ic1 = ic2 = 0.0f;
            return x;
        }

        if (--countdown <= 0)
        {
            updateCoefficients();
            countdown = 16;
        }

        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;

        const float y = (type == 2) ? v1
                      : (type == 3) ? (x - k * v1 - v2)
                                    : v2;

        return softClip (y);
    }

private:
    void applyEnvelope()
    {
        juce::ADSR::Parameters p;
        p.attack  = adsrAttack;
        p.decay   = adsrDecay;
        p.sustain = adsrSustain;
        p.release = adsrRelease;
        adsr.setParameters (p);
        envelopeInitialised = true;
    }

    void updateCoefficients() noexcept
    {
        const float q = 0.707f + resonance * resonance * 15.3f;

        // Three musical offsets in octaves: the note itself, how hard it was
        // played, and where the cutoff envelope currently sits.
        const float trackOct = keyTrack * ((float) (note - 60) / 12.0f);
        const float velOct   = velAmount * (velocity - 0.6f) * 2.0f;
        const float envOct   = envAmount * 3.0f * env;

        const float fc = juce::jlimit (20.0f, (float) sr * 0.45f,
                                       cutoffHz * std::exp2 (trackOct + velOct
                                                             + envOct + lfoOctaves));

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

    int   type = 1;
    float cutoffHz = 8000.0f, resonance = 0.15f;
    float envAmount = 0.35f, keyTrack = 0.5f, velAmount = 0.4f;
    float lfoOctaves = 0.0f;

    float adsrAttack = 0.004f, adsrDecay = 0.30f, adsrSustain = 0.25f, adsrRelease = 0.20f;
    bool  envelopeInitialised = false;

    juce::ADSR adsr;
    float env = 0.0f;
    int   countdown = 0;

    int   note = 60;
    float velocity = 0.8f;

    float g = 0.0f, k = 1.0f, a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

} // namespace vespera
