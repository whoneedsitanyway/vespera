#pragma once

#include "PatternEngine.h"
#include "Scale.h"

#include <juce_core/juce_core.h>

#include <algorithm>
#include <cmath>

namespace vespera
{

//==============================================================================
//  Second voice ("duet").
//
//  This is an arranging stage, not an effect: it turns the single generated
//  line into a small piece by adding a second part derived from the same pool
//  and the same key.  Every role is deliberately musical out of the box:
//
//    * Answer  - an independent line from a second PatternEngine, delayed by a
//                few steps, so the two voices trade phrases;
//    * Bass    - the root of the current step, one octave down, on strong beats;
//    * Harmony - a diatonic third / fourth / fifth / sixth below the lead;
//    * Octave  - the lead doubled an octave up;
//    * Drone   - a sustained tonic pedal.
//
//  The helper is pure and shared by the audio thread and the display, so what
//  you see is exactly what the synth plays (and what the MIDI export writes).
//==============================================================================
enum SecondRole
{
    kSecondOff = 0,
    kSecondAnswer,
    kSecondBass,
    kSecondHarmony,
    kSecondOctave,
    kSecondDrone,
    kNumSecondRoles
};

inline const juce::StringArray& getSecondRoleNames()
{
    static const juce::StringArray names { "Off", "Answer", "Bass", "Harmony", "Octave", "Drone" };
    return names;
}

inline const juce::StringArray& getSecondIntervalNames()
{
    static const juce::StringArray names { "3rd", "4th", "5th", "6th" };
    return names;
}

struct SecondVoiceParams
{
    int   role     = kSecondOff;   // see SecondRole
    int   octave   = 0;            // -2..+2
    int   offset   = 2;            // 0..8 steps (Answer)
    float density  = 0.5f;         // 0..1
    float level    = 0.6f;         // 0..1
    int   interval = 0;            // 0..3 -> 3rd .. 6th below (Harmony)
};

struct SecondNote
{
    bool  active    = false;
    int   note      = -1;
    float velocity  = 0.0f;
    float lengthSteps = 0.0f;   // 0 = use the lead GATE
};

// Density maps to "play every Nth step": 1 = every step, up to `maxPeriod`.
inline int secondPeriod (float density, int maxPeriod)
{
    const float d = juce::jlimit (0.0f, 1.0f, density);
    return juce::jlimit (1, maxPeriod,
                         1 + (int) std::lround ((1.0f - d) * (float) (maxPeriod - 1)));
}

inline SecondNote secondVoiceAt (int step, int length,
                                 const PatternStep& leadStep,
                                 const PatternEngine* answerPattern,
                                 const int* poolNotes, int poolCount,
                                 const SecondVoiceParams& p,
                                 int key, int scale, int transpose)
{
    SecondNote out;

    if (p.role == kSecondOff || poolCount <= 0 || length <= 0 || poolNotes == nullptr)
        return out;

    const float level = juce::jlimit (0.0f, 1.0f, p.level);
    if (level <= 0.001f)
        return out;

    const int reg       = juce::jlimit (-2, 2, p.octave);
    const int leadIndex = juce::jlimit (0, poolCount - 1, leadStep.poolIndex);
    const int leadPitch = juce::jlimit (0, 127, poolNotes[leadIndex] + 12 * leadStep.octave);

    int   note        = -1;
    float velocity    = 0.0f;
    float lengthSteps = 0.0f;

    switch (p.role)
    {
        case kSecondAnswer:
        {
            if (answerPattern == nullptr)
                return out;

            const int offset = juce::jlimit (0, 8, p.offset);
            const int index  = ((step - offset) % length + length) % length;
            const auto& s    = answerPattern->getStep (index);

            if (! s.active)
                return out;

            const int pi = juce::jlimit (0, poolCount - 1, s.poolIndex);
            note     = poolNotes[pi] + 12 * (s.octave + reg);
            velocity = s.velocity * level;
            break;
        }

        case kSecondBass:
        {
            if (p.density <= 0.001f)
                return out;

            const int period = secondPeriod (p.density, 4);
            if (step % period != 0)
                return out;

            // One octave below by default: REGISTER 0 already anchors the bass.
            note     = leadPitch + 12 * (reg - 1);
            velocity = (leadStep.active ? leadStep.velocity : 0.75f) * level * 0.9f;
            break;
        }

        case kSecondHarmony:
        {
            if (! leadStep.active || p.density <= 0.001f)
                return out;

            const int period = secondPeriod (p.density, 4);
            if (step % period != 0)
                return out;

            static constexpr int degrees[] = { 2, 3, 4, 5 };   // 3rd .. 6th below
            const int d = degrees[juce::jlimit (0, 3, p.interval)];

            note     = Scale::transposeDegrees (leadPitch, -d, key, scale) + 12 * reg;
            velocity = leadStep.velocity * level;
            break;
        }

        case kSecondOctave:
        {
            if (! leadStep.active || p.density <= 0.001f)
                return out;

            const int period = secondPeriod (p.density, 4);
            if (step % period != 0)
                return out;

            note     = leadPitch + 12 * (1 + reg);
            velocity = leadStep.velocity * level;
            break;
        }

        case kSecondDrone:
        {
            if (p.density <= 0.001f)
                return out;

            const int period = secondPeriod (p.density, 8);
            if (step % period != 0)
                return out;

            // The tonic of the key is a stable pedal regardless of the pool.
            note        = 48 + juce::jlimit (0, 11, key) + 12 * reg;
            velocity    = level * 0.8f;
            lengthSteps = (float) period + 0.9f;   // ring until the next trigger
            break;
        }

        default:
            return out;
    }

    if (transpose != 0)
        note = Scale::transposeDegrees (note, transpose, key, scale);

    out.active      = true;
    out.note        = juce::jlimit (0, 127, note);
    out.velocity    = juce::jlimit (0.05f, 1.0f, velocity);
    out.lengthSteps = lengthSteps;
    return out;
}

} // namespace vespera
