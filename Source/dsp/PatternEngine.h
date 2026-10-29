#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <algorithm>

namespace vespera
{

//==============================================================================
//  Pattern generator.
//
//  Turns a small pool of notes into a repeating melodic pattern.  The contour
//  decides how the generator walks through the pool (up, down, ping-pong,
//  random or a random walk), density decides how many steps play, and mutate
//  slowly evolves the pattern from cycle to cycle so it never feels looped.
//==============================================================================
struct PatternParams
{
    int   steps    = 8;      // 2 .. 16
    float density  = 0.7f;   // 0 .. 1
    int   octaves  = 2;      // 1 .. 3
    int   contour  = 2;      // see Contour
    float mutate   = 0.0f;   // 0 .. 1
    float gate     = 0.5f;   // 0.05 .. 1
    float swing    = 0.0f;   // 0 .. 1
    float accent   = 0.3f;   // 0 .. 1
};

enum Contour
{
    kContourUp = 0,
    kContourDown,
    kContourUpDown,
    kContourRandom,
    kContourWalk,
    kContourReverse,
    kNumContours
};

inline juce::StringArray getContourNames()
{
    return { "Up", "Down", "Up-Down", "Random", "Walk", "Reverse" };
}

// Step length in note values.  The first six entries keep their original order
// (so saved presets keep their tempo), the triplets and dotted values follow.
inline juce::StringArray getRateNames()
{
    return { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
             "1/4T", "1/8T", "1/16T", "1/4D", "1/8D", "1/16D" };
}

// Step length expressed in quarter-note beats, index order matches getRateNames().
inline double rateInBeats (int rateIndex)
{
    static constexpr double beats[] =
    {
        4.0, 2.0, 1.0, 0.5, 0.25, 0.125,       // 1/1   1/2   1/4   1/8  1/16  1/32
        2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0,       // 1/4T  1/8T  1/16T
        1.5, 0.75, 0.375                       // 1/4D  1/8D  1/16D
    };

    const int n = (int) (sizeof (beats) / sizeof (beats[0]));
    return beats[juce::jlimit (0, n - 1, rateIndex)];
}

inline int numRates() { return 12; }

struct PatternStep
{
    bool  active    = false;
    int   poolIndex = 0;
    int   octave    = 0;     // -1, 0, +1
    float velocity  = 0.8f;
};

class PatternEngine
{
public:
    static constexpr int kMaxSteps = 16;

    void reset()
    {
        length = 8;
        for (auto& s : steps)
            s = {};
        lastIndex = 0;
        walkDirection = 1;
    }

    void regenerate (int poolSize, const PatternParams& p, juce::Random& rng)
    {
        length = juce::jlimit (2, kMaxSteps, p.steps);
        lastIndex = 0;
        walkDirection = 1;

        for (int i = 0; i < kMaxSteps; ++i)
            steps[(size_t) i] = {};

        if (poolSize <= 0)
            return;

        if (p.contour == kContourReverse)
        {
            // Build the ascending figure first, then play it backwards, so the
            // rhythm and the contour are both mirrored.
            PatternParams forward = p;
            forward.contour = kContourUp;

            for (int i = 0; i < length; ++i)
                fillStep (steps[(size_t) i], i, poolSize, forward, rng);

            std::reverse (steps.begin(), steps.begin() + length);
        }
        else
        {
            for (int i = 0; i < length; ++i)
                fillStep (steps[(size_t) i], i, poolSize, p, rng);
        }

        ensurePlayable (poolSize, p);
    }

    // Gently evolves the pattern at the end of a cycle.
    void mutate (int poolSize, const PatternParams& p, juce::Random& rng)
    {
        if (poolSize <= 0)
            return;

        const float amount = juce::jlimit (0.0f, 1.0f, p.mutate);
        if (amount <= 0.001f)
            return;

        for (int i = 0; i < length; ++i)
        {
            if (rng.nextFloat() < amount * 0.35f)
                fillStep (steps[(size_t) i], i, poolSize, p, rng, true);
        }

        ensurePlayable (poolSize, p);
    }

    const PatternStep& getStep (int index) const
    {
        return steps[(size_t) juce::jlimit (0, kMaxSteps - 1, index)];
    }

    int getLength() const noexcept { return length; }

private:
    // A generator that hands back an empty pattern is a dead instrument: with a
    // low density and a short pattern that happens often enough to be noticed
    // after pressing RANDOM, so the first step is always kept alive.  It also
    // means a fresh pattern is audible immediately instead of waiting for the
    // first active step somewhere in the middle.
    void ensurePlayable (int poolSize, const PatternParams& p)
    {
        if (poolSize <= 0 || length <= 0)
            return;

        // Density at zero means silence on purpose, so leave it alone.
        if (p.density <= 0.001f)
            return;

        auto& first = steps[0];
        first.active = true;
        first.poolIndex = juce::jlimit (0, poolSize - 1, first.poolIndex);
        first.octave = juce::jlimit (-1, 1, first.octave);
        first.velocity = juce::jlimit (0.1f, 1.0f, juce::jmax (first.velocity, 0.7f));
    }

    int pickPoolIndex (int step, int poolSize, int contour, juce::Random& rng)
    {
        poolSize = juce::jmax (1, poolSize);

        int index = 0;

        switch (contour)
        {
            case kContourUp:
                index = step % poolSize;
                break;

            case kContourDown:
                index = poolSize - 1 - (step % poolSize);
                break;

            case kContourUpDown:
            {
                if (poolSize == 1)
                {
                    index = 0;
                    break;
                }
                const int cycle = 2 * poolSize - 2;
                const int pos = step % cycle;
                index = (pos < poolSize) ? pos : cycle - pos;
                break;
            }

            case kContourRandom:
                index = rng.nextInt (poolSize);
                if (poolSize > 1 && index == lastIndex)
                    index = (index + 1 + rng.nextInt (poolSize - 1)) % poolSize;
                break;

            default: // walk (and reverse, which fills forwards and is mirrored later)
                index = lastIndex + walkDirection;
                if (index >= poolSize)
                {
                    index = poolSize - 2;
                    walkDirection = -1;
                }
                else if (index < 0)
                {
                    index = 1;
                    walkDirection = 1;
                }
                index = juce::jlimit (0, poolSize - 1, index);
                break;
        }

        lastIndex = index;
        return index;
    }

    static int pickOctave (int octaves, juce::Random& rng)
    {
        if (octaves <= 1)
            return 0;

        if (octaves == 2)
            return rng.nextFloat() < 0.30f ? 1 : 0;

        const float r = rng.nextFloat();
        if (r < 0.15f)
            return -1;
        if (r < 0.40f)
            return 1;
        return 0;
    }

    void fillStep (PatternStep& s, int step, int poolSize,
                   const PatternParams& p, juce::Random& rng,
                   bool randomisePitch = false)
    {
        s.active = rng.nextFloat() < juce::jlimit (0.0f, 1.0f, p.density);

        if (! s.active)
        {
            s.poolIndex = 0;
            s.octave = 0;
            s.velocity = 0.0f;
            return;
        }

        // Mutation deliberately picks a fresh pool note even for the ordered
        // contours, otherwise Up / Down would always regenerate the same pitch
        // and the pattern would never actually evolve.
        if (randomisePitch)
        {
            const int oldIndex = juce::jlimit (0, juce::jmax (0, poolSize - 1), s.poolIndex);
            int index = rng.nextInt (juce::jmax (1, poolSize));

            if (poolSize > 1 && index == oldIndex)
                index = (index + 1 + rng.nextInt (poolSize - 1)) % poolSize;

            s.poolIndex = index;
        }
        else
        {
            s.poolIndex = pickPoolIndex (step, poolSize, p.contour, rng);
        }

        s.octave = pickOctave (p.octaves, rng);

        const float accent = (step % 4 == 0) ? p.accent : 0.0f;
        s.velocity = juce::jlimit (0.1f, 1.0f, 0.65f + accent * 0.3f + rng.nextFloat() * 0.12f);
    }

    std::array<PatternStep, kMaxSteps> steps {};
    int length = 8;
    int lastIndex = 0;
    int walkDirection = 1;
};

} // namespace vespera
