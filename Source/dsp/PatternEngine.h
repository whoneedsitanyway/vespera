#pragma once

#include <juce_core/juce_core.h>

#include "Scale.h"

#include <array>
#include <algorithm>
#include <cmath>

namespace vespera
{

//==============================================================================
//  Pattern generator.
//
//  Turns a small pool of notes into a repeating *phrase* instead of a stream of
//  independent random notes:
//
//    * a rhythm mask decides which steps speak, drawn from metric weights, so
//      a sparse figure leans on the downbeats;
//    * a short motif is invented first and the rest of the pattern is built
//      from it (repeat, diatonic transposition, rhythmic shift, retrograde), so
//      the listener hears a theme rather than noise;
//    * the register belongs to the phrase, never to a single note;
//    * leaps are limited, and a leap is answered by a step back;
//    * the phrase resolves onto a stable degree (tonic, third or fifth);
//    * harmony changes once per bar, not on every sixteenth.
//
//  The contour decides the shape of the motif (up, down, ping-pong, random or a
//  random walk), density now controls how many steps speak, and mutate evolves
//  the phrase by single degrees so it never falls apart.
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

    // Musical context: the generator needs it to resolve phrases onto stable
    // degrees and to transpose variations by scale steps, not semitones.
    int   key      = 0;
    int   scale    = 1;
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

    // Harmony description, turned into scale pitches by buildChord().  The
    // mask is generated even while Chords is off, so toggling the switch never
    // has to regenerate the pattern (and the display / MIDI export can always
    // reproduce exactly what the synth plays).
    juce::uint8 chordMask      = 0x7;   // root + third + fifth
    juce::uint8 chordInversion = 0;
};

class PatternEngine
{
public:
    static constexpr int kMaxSteps = 16;
    static constexpr int kMaxPool  = 16;

    void reset()
    {
        length = 8;
        motifLength = 4;
        for (auto& s : steps)
            s = {};
        for (auto& r : rhythm)
            r = false;
        poolCount = 0;
        lastIndex = 0;
        walkDirection = 1;
    }

    // `poolNotes` are the pitches the pool actually holds (ascending).  The
    // generator needs them for voice leading, cadences and diatonic
    // transposition, which a bare count cannot provide.
    void regenerate (const int* poolNotes, int poolSize, const PatternParams& p,
                     juce::Random& rng)
    {
        length = juce::jlimit (2, kMaxSteps, p.steps);
        lastIndex = 0;
        walkDirection = 1;

        for (int i = 0; i < kMaxSteps; ++i)
            steps[(size_t) i] = {};

        poolCount = juce::jlimit (0, kMaxPool, poolSize);
        for (int i = 0; i < poolCount; ++i)
            pool[i] = juce::jlimit (0, 127,
                                    poolNotes != nullptr ? poolNotes[i] : 60 + i * 2);

        if (poolCount <= 0)
            return;

        if (p.contour == kContourReverse)
        {
            // Build the figure first, then play it backwards, so the rhythm and
            // the contour are both mirrored.
            PatternParams forward = p;
            forward.contour = kContourUp;

            buildPattern (forward, rng);
            std::reverse (steps.begin(), steps.begin() + length);
        }
        else
        {
            buildPattern (p, rng);
        }

        // Chords are laid out after the mirroring so they stay bar-aligned.
        applyHarmony (rng);
    }

    // Gently evolves the pattern at the end of a cycle.
    void mutate (const int* poolNotes, int poolSize, const PatternParams& p,
                 juce::Random& rng)
    {
        if (poolSize <= 0)
            return;

        const float amount = juce::jlimit (0.0f, 1.0f, p.mutate);
        if (amount <= 0.001f)
            return;

        poolCount = juce::jlimit (0, kMaxPool, poolSize);
        for (int i = 0; i < poolCount; ++i)
            pool[i] = juce::jlimit (0, 127,
                                    poolNotes != nullptr ? poolNotes[i] : 60 + i * 2);

        for (int i = 0; i < length; ++i)
        {
            if (rng.nextFloat() >= amount * 0.35f)
                continue;

            auto& s = steps[(size_t) i];
            if (! s.active || poolCount <= 1)
                continue;

            // Evolve by a degree, not by a jump: a neighbour keeps the shape of
            // the phrase while still being a new pitch.
            const int oldIndex = juce::jlimit (0, poolCount - 1, s.poolIndex);
            const int dir = rng.nextBool() ? 1 : -1;
            int index = juce::jlimit (0, poolCount - 1, oldIndex + dir);

            if (index == oldIndex)
                index = juce::jlimit (0, poolCount - 1, oldIndex - dir);

            s.poolIndex = index;
        }

        ensurePlayable (p);
        applyCadence (p);
    }

    const PatternStep& getStep (int index) const
    {
        return steps[(size_t) juce::jlimit (0, kMaxSteps - 1, index)];
    }

    int getLength() const noexcept { return length; }

private:
    // A generator that hands back an empty pattern is a dead instrument: with a
    // low density and a short pattern that happens often enough to be noticed
    // after pressing RANDOM, so the first step is always kept alive.
    void ensurePlayable (const PatternParams& p)
    {
        if (poolCount <= 0 || length <= 0)
            return;

        // Density at zero means silence on purpose, so leave it alone.
        if (p.density <= 0.001f)
            return;

        auto& first = steps[0];
        first.active = true;
        first.poolIndex = juce::jlimit (0, poolCount - 1, first.poolIndex);
        first.octave = juce::jlimit (-1, 1, first.octave);
        first.velocity = juce::jlimit (0.1f, 1.0f, juce::jmax (first.velocity, 0.7f));

        // The phrase also has to end on a note: a cadence needs a landing step,
        // and a figure that stops on a rest never feels finished.
        auto& last = steps[(size_t) (length - 1)];
        if (! last.active)
        {
            last.active = true;
            last.poolIndex = juce::jlimit (0, poolCount - 1, last.poolIndex);
            last.octave = juce::jlimit (-1, 1, last.octave);
            last.velocity = juce::jlimit (0.1f, 1.0f, juce::jmax (last.velocity, 0.55f));
        }
    }

    void buildPattern (const PatternParams& p, juce::Random& rng)
    {
        motifLength = phraseLength (length);

        // The rhythm is drawn for the motif, not for the whole pattern: the
        // motif's rhythm is what repeats, so that is where the density belongs.
        buildRhythm (p, rng);

        const int baseOctave = pickSectionOctave (p.octaves, rng);

        // The motif, then its variations.  Every section is built from the motif
        // arrays, never from the previous section, so a variation cannot drift
        // away from the theme.
        int mActive[kMaxSteps] {}, mIndex[kMaxSteps] {};
        float mVelocity[kMaxSteps] {};

        buildMotif (motifLength, p, rng, mActive, mIndex, mVelocity);

        int section = 0;
        for (int start = 0; start < length; start += motifLength, ++section)
        {
            const int len = juce::jmin (motifLength, length - start);
            writeSection (start, len, section, p, baseOctave, rng,
                          mActive, mIndex, mVelocity);
        }

        ensurePlayable (p);
        applyCadence (p);
    }

    int pickPoolIndex (int step, int contour, juce::Random& rng)
    {
        const int poolSize = juce::jmax (1, poolCount);

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
            {
                if (poolSize <= 1)
                {
                    index = 0;
                    break;
                }

                // Mostly a step or two away from the previous note: a uniformly
                // random index is exactly what made the figure sound like noise.
                const float roll = rng.nextFloat();
                int delta = 0;

                if (roll < 0.18f)
                    delta = 0;
                else if (roll < 0.68f)
                    delta = rng.nextBool() ? 1 : -1;
                else if (roll < 0.92f)
                    delta = rng.nextBool() ? 2 : -2;
                else
                    delta = rng.nextInt (poolSize) - lastIndex;

                index = juce::jlimit (0, poolSize - 1, lastIndex + delta);

                // Never repeat the same note twice in a row.
                if (index == lastIndex)
                    index = juce::jlimit (0, poolSize - 1,
                                          lastIndex + (lastIndex + 1 < poolSize ? 1 : -1));
                break;
            }

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

    // The register is a property of the phrase: one octave offset per section,
    // never a jump between two neighbouring notes.
    static int pickSectionOctave (int octaves, juce::Random& rng)
    {
        if (octaves <= 1)
            return 0;

        if (octaves == 2)
            return rng.nextFloat() < 0.15f ? 1 : 0;

        const float r = rng.nextFloat();
        if (r < 0.12f)
            return -1;
        if (r < 0.28f)
            return 1;
        return 0;
    }

    //==========================================================================
    //  Rhythm
    //==========================================================================
    // Metric weight of a step: downbeat > beat > off-beat > sixteenth.  The mask
    // is drawn from these weights, so a sparse pattern leans on the strong
    // positions instead of scattering notes at random.
    static float metricWeight (int step)
    {
        if (step % 8 == 0) return 1.0f;
        if (step % 4 == 0) return 0.80f;
        if (step % 2 == 0) return 0.55f;
        return 0.30f;
    }

    void buildRhythm (const PatternParams& p, juce::Random& rng)
    {
        const int len = juce::jlimit (1, kMaxSteps, motifLength);
        const float density = juce::jlimit (0.0f, 1.0f, p.density);
        const int target = juce::jlimit (0, len,
                                         juce::roundToInt (density * (float) len));

        bool used[kMaxSteps] {};
        int chosen = 0;

        // The figure always starts on the downbeat.
        if (target > 0)
        {
            used[0] = true;
            chosen = 1;
        }

        // Weighted sampling without replacement: the requested density is met
        // exactly, while the positions stay metric.
        while (chosen < target)
        {
            float total = 0.0f;
            for (int i = 0; i < len; ++i)
                if (! used[i])
                    total += metricWeight (i);

            if (total <= 0.0f)
                break;

            float roll = rng.nextFloat() * total;
            int pick = -1;

            for (int i = 0; i < len; ++i)
            {
                if (used[i])
                    continue;

                pick = i;
                roll -= metricWeight (i);
                if (roll <= 0.0f)
                    break;
            }

            if (pick < 0)
                break;

            used[pick] = true;
            ++chosen;
        }

        for (int i = 0; i < kMaxSteps; ++i)
            rhythm[(size_t) i] = (i < len) && used[i];
    }

    //==========================================================================
    //  Pitch helpers
    //==========================================================================
    int pitchAt (int index, int octave) const
    {
        if (poolCount <= 0)
            return 60;

        return juce::jlimit (0, 127, pool[juce::jlimit (0, poolCount - 1, index)] + 12 * octave);
    }

    static bool isStableDegree (int pitch, int key, int scaleIndex)
    {
        const auto& intervals = Scale::getIntervals (scaleIndex);
        const int n = (int) intervals.size();
        if (n <= 0)
            return true;

        const int pc = ((pitch - juce::jlimit (0, 11, key)) % 12 + 12) % 12;
        int degree = -1;

        for (int i = 0; i < n; ++i)
            if (intervals[(size_t) i] == pc)
            {
                degree = i;
                break;
            }

        if (degree < 0)
            return false;

        if (n >= 12)                                          // chromatic: major triad
            return degree == 0 || degree == 4 || degree == 7;

        if (n == 5)                                           // pentatonic: tonic + third
            return degree == 0 || degree == 2;

        return degree == 0 || degree == 2 || degree == 4;      // tonic, third, fifth
    }

    // Moves a step onto a stable degree, but only when a chord tone is within
    // `maxDistance` semitones — the melody must keep its shape.
    void resolveToStable (PatternStep& s, const PatternParams& p, int maxDistance)
    {
        if (poolCount <= 0)
            return;

        const int pitch = pitchAt (s.poolIndex, s.octave);
        if (isStableDegree (pitch, p.key, p.scale))
            return;

        int best = -1;
        int bestDistance = maxDistance + 1;

        for (int i = 0; i < poolCount; ++i)
        {
            const int candidate = pitchAt (i, s.octave);
            if (! isStableDegree (candidate, p.key, p.scale))
                continue;

            const int d = std::abs (candidate - pitch);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = i;
            }
        }

        if (best >= 0)
            s.poolIndex = best;
    }

    // Nearest pool note to an arbitrary pitch; the remainder becomes an octave
    // offset, so a transposed variation stays inside the pool and in key.
    void fitToPool (int pitch, int& index, int& octaveOffset) const
    {
        index = 0;
        octaveOffset = 0;

        if (poolCount <= 0)
            return;

        int best = 0;
        int bestDistance = 1000;

        for (int i = 0; i < poolCount; ++i)
        {
            const int d = std::abs (pool[i] - pitch);
            if (d < bestDistance)
            {
                bestDistance = d;
                best = i;
            }
        }

        index = best;
        const int delta = pitch - pool[best];
        octaveOffset = juce::jlimit (-1, 1, (int) std::lround ((double) delta / 12.0));
    }

    // Keeps a line singable: no leap beyond a fifth for the free contours (an
    // octave for the ordered ones, whose shape has to survive), and a leap is
    // answered by a step back.
    int smoothIndex (int index, int prevPitch, int prevInterval, int contour) const
    {
        if (prevPitch < 0 || poolCount <= 1)
            return index;

        const bool ordered = contour == kContourUp || contour == kContourDown
                          || contour == kContourUpDown;
        const int maxLeap = ordered ? 12 : 7;

        const int pitch = pitchAt (index, 0);
        if (std::abs (pitch - prevPitch) <= maxLeap)
            return index;

        int dir = 0;
        if (contour == kContourUp)             dir = 1;
        else if (contour == kContourDown)      dir = -1;
        else if (std::abs (prevInterval) >= 5) dir = prevInterval > 0 ? -1 : 1;
        else                                   dir = pitch >= prevPitch ? 1 : -1;

        int best = -1;
        int bestDistance = 1000;

        for (int i = 0; i < poolCount; ++i)
        {
            const int candidate = pitchAt (i, 0);
            const int delta = candidate - prevPitch;

            if (dir > 0 && delta <= 0) continue;
            if (dir < 0 && delta >= 0) continue;

            const int d = std::abs (delta);
            if (d > maxLeap) continue;

            if (d < bestDistance)
            {
                bestDistance = d;
                best = i;
            }
        }

        if (best < 0)
        {
            // Nothing in that direction: fall back to the closest note overall.
            bestDistance = 1000;
            for (int i = 0; i < poolCount; ++i)
            {
                const int d = std::abs (pitchAt (i, 0) - prevPitch);
                if (d < bestDistance)
                {
                    bestDistance = d;
                    best = i;
                }
            }
        }

        return best >= 0 ? best : index;
    }

    float velocityFor (int step, const PatternParams& p, juce::Random& rng) const
    {
        float base = 0.62f;

        if (step % 4 == 0)
            base = 0.78f + p.accent * 0.18f;
        else if (step % 2 == 0)
            base = 0.70f;

        return juce::jlimit (0.1f, 1.0f, base + rng.nextFloat() * 0.06f);
    }

    //==========================================================================
    //  Motif and variations
    //==========================================================================
    static int phraseLength (int len)
    {
        if (len >= 12) return 4;              // A B A C
        if (len >= 6)  return (len + 1) / 2;
        return 2;
    }

    void buildMotif (int len, const PatternParams& p, juce::Random& rng,
                     int* mActive, int* mIndex, float* mVelocity)
    {
        int prevPitch = -1;
        int prevInterval = 0;

        for (int k = 0; k < len; ++k)
        {
            mActive[k] = rhythm[(size_t) k] ? 1 : 0;
            mIndex[k] = 0;
            mVelocity[k] = 0.0f;

            if (mActive[k] == 0)
                continue;

            int index = pickPoolIndex (k, p.contour, rng);
            index = smoothIndex (index, prevPitch, prevInterval, p.contour);

            mIndex[k] = index;
            mVelocity[k] = velocityFor (k, p, rng);

            const int pitch = pitchAt (index, 0);
            if (prevPitch >= 0)
                prevInterval = pitch - prevPitch;
            prevPitch = pitch;
        }
    }

    enum VariationKind
    {
        kVariationRepeat = 0,
        kVariationTranspose,
        kVariationShift,
        kVariationRetrograde
    };

    // A transposed variation only works while the figure stays inside the pool:
    // notes pushed past the pool's register would be squashed onto its edge by
    // fitToPool(), which turned a whole variation into one repeated note.
    int pickTranspose (VariationKind kind, const PatternParams& p,
                       const int* mActive, const int* mIndex) const
    {
        if (kind != kVariationTranspose || poolCount <= 0)
            return 0;

        const int lo = pool[0];
        const int hi = pool[poolCount - 1];
        const int dir = 2;

        auto countInside = [this, &p, mActive, mIndex, lo, hi] (int degrees)
        {
            int count = 0;
            for (int k = 0; k < motifLength; ++k)
            {
                if (mActive[k] == 0)
                    continue;

                const int pitch = Scale::transposeDegrees (pitchAt (mIndex[k], 0),
                                                           degrees, p.key, p.scale);
                if (pitch >= lo && pitch <= hi)
                    ++count;
            }
            return count;
        };

        const int up = countInside (dir);
        const int down = countInside (-dir);

        // Prefer the direction that keeps most of the motif in register; on a
        // tie the figure rises, which reads as an answer to the opening phrase.
        return up >= down ? dir : -dir;
    }

    void writeSection (int start, int len, int section, const PatternParams& p,
                       int baseOctave, juce::Random& rng,
                       const int* mActive, const int* mIndex, const float* mVelocity)
    {        VariationKind kind = kVariationRepeat;

        // A B A C: every odd section answers the motif.
        if (section > 0 && (section % 2) == 1)
        {
            const float roll = rng.nextFloat();
            kind = roll < 0.45f ? kVariationTranspose
                 : roll < 0.75f ? kVariationShift
                                : kVariationRetrograde;
        }

        // The register belongs to the phrase: a variation may lift it an octave,
        // but a single note never jumps.
        int sectionOctave = baseOctave;
        if (section > 0 && p.octaves >= 2 && rng.nextFloat() < 0.30f)
            sectionOctave = juce::jlimit (-1, 1, baseOctave + 1);

        const int degrees = pickTranspose (kind, p, mActive, mIndex);
        const int motifLen = juce::jmax (1, motifLength);

        for (int k = 0; k < len; ++k)
        {
            // Retrograde mirrors the motif (rhythm and pitches), a rhythmic
            // shift keeps the motif but moves the mask by one step.
            const int src = juce::jlimit (0, motifLen - 1,
                                          kind == kVariationRetrograde ? motifLen - 1 - k : k);

            const int rhythmSrc = kind == kVariationShift
                                      ? (src + motifLen - 1) % motifLen
                                      : src;

            // The pitch always comes from a step the mask actually plays: taking
            // it from a rest used to collapse whole variations onto one note.
            const int pitchSrc = kind == kVariationShift ? rhythmSrc : src;

            auto& s = steps[(size_t) (start + k)];

            s.active = mActive[rhythmSrc] != 0;
            s.velocity = s.active ? mVelocity[pitchSrc] : 0.0f;
            s.poolIndex = mIndex[pitchSrc];
            s.octave = sectionOctave;

            if (! s.active)
                continue;

            if (kind == kVariationTranspose && degrees != 0)
            {
                const int pitch = Scale::transposeDegrees (pitchAt (mIndex[pitchSrc], 0),
                                                           degrees, p.key, p.scale);

                // A note that leaves the pool's register keeps its original
                // pitch: a partial transposition still varies the phrase, while
                // a clamped one would just repeat the pool's edge note.
                if (pitch >= pool[0] && pitch <= pool[poolCount - 1])
                {
                    int index = 0;
                    int octaveOffset = 0;
                    fitToPool (pitch, index, octaveOffset);

                    s.poolIndex = index;
                    s.octave = juce::jlimit (-1, 1, sectionOctave + octaveOffset);
                }
            }
        }
    }

    //==========================================================================
    //  Cadence and harmony
    //==========================================================================
    void applyCadence (const PatternParams& p)
    {
        if (poolCount <= 0 || length <= 0)
            return;

        int lastActive = -1;
        for (int i = length - 1; i >= 0; --i)
            if (steps[(size_t) i].active)
            {
                lastActive = i;
                break;
            }

        // The phrase has to land somewhere.
        if (lastActive >= 0)
            resolveToStable (steps[(size_t) lastActive], p, 127);

        // Bar starts carry the harmony: an unstable degree is nudged onto a
        // chord tone, but only by a small interval.
        for (int i = 4; i < length; i += 4)
            if (steps[(size_t) i].active)
                resolveToStable (steps[(size_t) i], p, 2);
    }

    void applyHarmony (juce::Random& rng)
    {
        const int bars = (length + 3) / 4;

        for (int bar = 0; bar < bars; ++bar)
        {
            const bool lastBar = (bar == bars - 1);

            // One chord per bar: the harmony is heard as movement instead of
            // flickering on every sixteenth.  The last bar resolves — root
            // position, no seventh.
            juce::uint8 mask = 0x7;
            if (! lastBar && rng.nextFloat() < 0.30f)
                mask |= 0x8;

            // Only used by the chromatic scale, where a diatonic third does not
            // exist and the chord needs a major / minor colour instead.
            if (rng.nextFloat() < 0.5f)
                mask |= 0x10;

            const float roll = rng.nextFloat();
            const int inversion = (bar == 0 || lastBar) ? 0
                                : roll < 0.70f ? 0
                                : roll < 0.92f ? 1
                                               : 2;

            for (int i = bar * 4; i < juce::jmin (length, bar * 4 + 4); ++i)
            {
                auto& s = steps[(size_t) i];
                s.chordMask = mask;
                s.chordInversion = (juce::uint8) (s.octave > 0 ? 0 : inversion);
            }
        }
    }

    std::array<PatternStep, kMaxSteps> steps {};
    std::array<bool, kMaxSteps> rhythm {};
    int pool[kMaxPool] {};
    int poolCount = 0;
    int length = 8;
    int motifLength = 4;
    int lastIndex = 0;
    int walkDirection = 1;
};

} // namespace vespera
