#pragma once

#include <juce_core/juce_core.h>
#include <cmath>

namespace vespera
{

//==============================================================================
//  Filter modulation source.
//
//  Same idea as the pattern contours: instead of a single sine, the LFO can
//  run as a triangle, a ramp, a square, a stepped random value or a smooth
//  random walk.  The phase is advanced once per block, which is plenty for a
//  filter sweep.
//==============================================================================
class Lfo
{
public:
    enum Shape
    {
        sine = 0,
        triangle,
        sawUp,
        sawDown,
        square,
        random,
        walk,
        numShapes
    };

    static const juce::StringArray& getNames()
    {
        static const juce::StringArray names { "Sine", "Triangle", "Saw Up", "Saw Down",
                                               "Square", "Random", "Walk" };
        return names;
    }

    void reset() noexcept
    {
        phase = 0.0;
        hold = 0.0f;
        walkValue = 0.0f;
        walkTarget = 0.0f;
        started = false;
        lastShape = -1;
    }

    // Advances the phase by `cycles` (may be fractional) and returns -1 .. 1.
    float advance (double cycles, int shape, juce::Random& rng) noexcept
    {
        shape = juce::jlimit (0, (int) numShapes - 1, shape);

        // Switching shape restarts the random sources so they never inherit the
        // state of the previous shape.
        if (shape != lastShape)
        {
            started = false;
            lastShape = shape;
        }

        phase += cycles;

        bool wrapped = false;
        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            wrapped = true;
        }

        const float t = (float) phase;

        switch (shape)
        {
            case sine:
                return std::sin (juce::MathConstants<float>::twoPi * t);

            case triangle:
                return 1.0f - 4.0f * std::abs (t - 0.5f);

            case sawUp:
                return 2.0f * t - 1.0f;

            case sawDown:
                return 1.0f - 2.0f * t;

            case square:
                return t < 0.5f ? 1.0f : -1.0f;

            case random:
                if (wrapped || ! started)
                {
                    hold = rng.nextFloat() * 2.0f - 1.0f;
                    started = true;
                }
                return hold;

            default: // walk
                if (wrapped || ! started)
                {
                    walkTarget = rng.nextFloat() * 2.0f - 1.0f;
                    started = true;
                }
                walkValue += (walkTarget - walkValue) * 0.25f;
                return juce::jlimit (-1.0f, 1.0f, walkValue);
        }
    }

private:
    double phase = 0.0;
    float  hold = 0.0f;
    float  walkValue = 0.0f;
    float  walkTarget = 0.0f;
    bool   started = false;
    int    lastShape = -1;
};

} // namespace vespera
