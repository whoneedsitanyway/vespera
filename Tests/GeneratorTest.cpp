// Offline tests for the melody generator building blocks:
//   (1) scale quantisation always lands on a scale degree,
//   (2) the note pool keeps the most recent notes and respects latch,
//   (3) the pattern engine only uses notes from the pool and stays musical.

#include <juce_core/juce_core.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "../Source/dsp/Scale.h"
#include "../Source/dsp/NotePool.h"
#include "../Source/dsp/PatternEngine.h"
#include "../Source/dsp/Character.h"
#include "../Source/dsp/Lfo.h"
#include "../Source/dsp/Limiter.h"

int main()
{
    int failures = 0;

    // --- (1) scale quantisation ---------------------------------------------
    {
        bool allInScale = true;
        int worstDistance = 0;

        for (int key = 0; key < 12; ++key)
        {
            for (int scale = 0; scale < 10; ++scale)
            {
                for (int note = 0; note < 128; ++note)
                {
                    const int q = vespera::Scale::quantize (note, key, scale);
                    if (! vespera::Scale::isInScale (q, key, scale))
                        allInScale = false;
                    worstDistance = juce::jmax (worstDistance, std::abs (q - note));
                }
            }
        }

        std::cout << "scale: allInScale=" << (allInScale ? "yes" : "NO")
                  << " worstDistance=" << worstDistance << std::endl;

        if (! allInScale || worstDistance > 2)
        {
            ++failures;
            std::cout << "FAIL: scale quantisation produced a note outside the scale" << std::endl;
        }
    }

    // --- (2) note pool -------------------------------------------------------
    {
        vespera::NotePool pool;
        pool.add (60, 4);
        pool.add (64, 4);
        pool.add (67, 4);
        pool.add (72, 4);
        pool.add (76, 4);

        int notes[vespera::NotePool::kMaxNotes] {};
        const int n = pool.getSorted (notes, 4);

        const bool keptRecent = n == 4
                             && notes[0] == 64 && notes[1] == 67
                             && notes[2] == 72 && notes[3] == 76;

        pool.setLatch (true);
        const bool latchIgnored = ! pool.add (50, 4);
        const int n2 = pool.getSorted (notes, 4);

        pool.setLatch (false);
        const bool resumed = pool.add (50, 4);

        // Shrinking then growing the pool must not resurrect stale notes.
        vespera::NotePool resized;
        for (int note : { 48, 52, 55, 59, 62 })
            resized.add (note, 4);
        resized.add (67, 2);                  // pool now holds only 62, 67
        int shrunk[vespera::NotePool::kMaxNotes] {};
        const int shrunkN = resized.getSorted (shrunk, 2);
        const bool shrinkOk = shrunkN == 2 && shrunk[0] == 62 && shrunk[1] == 67;

        const bool growOk = resized.add (72, 4)
                         && resized.getSorted (shrunk, 4) == 3
                         && shrunk[0] == 62 && shrunk[1] == 67 && shrunk[2] == 72;

        std::cout << "pool: keptRecent=" << keptRecent
                  << " latchIgnored=" << latchIgnored
                  << " resumed=" << resumed
                  << " shrink=" << shrinkOk
                  << " grow=" << growOk << std::endl;

        if (! (keptRecent && latchIgnored && resumed && n2 == 4 && shrinkOk && growOk))
        {
            ++failures;
            std::cout << "FAIL: note pool behaviour" << std::endl;
        }
    }

    // --- (3) pattern engine --------------------------------------------------
    {
        const int poolNotes[3] = { 60, 63, 67 };   // C minor triad
        const int key = 0;
        const int scale = 1;                       // C minor

        vespera::PatternEngine pattern;
        vespera::PatternParams p;
        p.steps = 8;
        p.density = 1.0f;
        p.octaves = 3;
        p.contour = 3;                             // random
        p.mutate = 0.5f;

        juce::Random rng (1234);
        pattern.regenerate (3, p, rng);

        bool allActive = true;
        bool allFromPool = true;
        bool allInScale = true;

        for (int i = 0; i < p.steps; ++i)
        {
            const auto& s = pattern.getStep (i);
            if (! s.active)
                allActive = false;

            if (s.poolIndex < 0 || s.poolIndex >= 3)
                allFromPool = false;

            if (s.octave < -1 || s.octave > 1)
                allFromPool = false;

            const int note = poolNotes[juce::jlimit (0, 2, s.poolIndex)] + 12 * s.octave;
            if (! vespera::Scale::isInScale (note, key, scale))
                allInScale = false;
        }

        // Density 0 must produce an empty pattern.
        p.density = 0.0f;
        pattern.regenerate (3, p, rng);
        bool empty = true;
        for (int i = 0; i < p.steps; ++i)
            if (pattern.getStep (i).active)
                empty = false;

        // Mutation must actually change pitches, even for the ordered contours.
        p.density = 1.0f;
        p.contour = 0;                             // up
        p.mutate = 1.0f;
        pattern.regenerate (3, p, rng);

        int before[vespera::PatternEngine::kMaxSteps] {};
        for (int i = 0; i < p.steps; ++i)
            before[i] = pattern.getStep (i).poolIndex;

        bool mutated = false;
        for (int cycle = 0; cycle < 20 && ! mutated; ++cycle)
        {
            pattern.mutate (3, p, rng);
            for (int i = 0; i < p.steps; ++i)
                if (pattern.getStep (i).poolIndex != before[i])
                    mutated = true;
        }

        std::cout << "pattern: allActive=" << allActive
                  << " allFromPool=" << allFromPool
                  << " allInScale=" << allInScale
                  << " emptyAtZeroDensity=" << empty
                  << " mutates=" << mutated << std::endl;

        if (! (allActive && allFromPool && allInScale && empty && mutated))
        {
            ++failures;
            std::cout << "FAIL: pattern engine invariants" << std::endl;
        }
    }

    // --- (4) character: noise is gated by the notes --------------------------
    {
        const double sr = 48000.0;
        const int block = 512;
        const int blocks = 40;

        juce::AudioBuffer<float> dry (2, block);
        juce::AudioBuffer<float> noisy (2, block);
        juce::AudioBuffer<float> quiet (2, block);

        vespera::Character withNoise, withoutNoise;
        withNoise.prepare (sr);
        withoutNoise.prepare (sr);

        vespera::CharacterParams noisyParams;
        noisyParams.noise = 1.0f;

        vespera::CharacterParams cleanParams;

        double silencePeak = 0.0;
        double noiseEnergy = 0.0;
        double noteEnergy = 0.0;

        for (int b = 0; b < blocks; ++b)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = dry.getWritePointer (ch);
                for (int n = 0; n < block; ++n)
                {
                    const int i = b * block + n;
                    // First half: silence, second half: a steady tone.
                    d[n] = (b < blocks / 2)
                               ? 0.0f
                               : 0.3f * std::sin (juce::MathConstants<float>::twoPi
                                                      * 220.0f * (float) i / (float) sr);
                }
            }

            noisy.makeCopyOf (dry);
            quiet.makeCopyOf (dry);

            withNoise.process (noisy.getWritePointer (0), noisy.getWritePointer (1), block, noisyParams);
            withoutNoise.process (quiet.getWritePointer (0), quiet.getWritePointer (1), block, cleanParams);

            for (int ch = 0; ch < 2; ++ch)
            {
                for (int n = 0; n < block; ++n)
                {
                    const double diff = (double) noisy.getSample (ch, n) - (double) quiet.getSample (ch, n);

                    if (b < blocks / 2)
                        silencePeak = juce::jmax (silencePeak, std::abs (diff));
                    else
                        noiseEnergy += diff * diff;

                    noteEnergy += (double) quiet.getSample (ch, n) * (double) quiet.getSample (ch, n);
                }
            }
        }

        const double noiseRms = std::sqrt (noiseEnergy / (double) (blocks / 2 * block * 2));
        const bool gated = silencePeak < 1.0e-6 && noiseRms > 1.0e-4 && noteEnergy > 1.0;

        std::cout << "character: silencePeak=" << silencePeak
                  << " noiseRms=" << noiseRms
                  << " gated=" << gated << std::endl;

        if (! gated)
        {
            ++failures;
            std::cout << "FAIL: noise is not gated by the notes" << std::endl;
        }
    }

    // --- (5) character: crush stays gentle at the bottom of the range --------
    {
        const double sr = 48000.0;
        const int block = 512;
        const int blocks = 20;

        juce::AudioBuffer<float> dry (2, block);
        juce::AudioBuffer<float> crushed (2, block);
        juce::AudioBuffer<float> clean (2, block);

        vespera::Character ch, ref;
        ch.prepare (sr);
        ref.prepare (sr);

        vespera::CharacterParams p;
        p.crush = 0.15f;

        vespera::CharacterParams cleanParams;

        double diffEnergy = 0.0, sigEnergy = 0.0;

        for (int b = 0; b < blocks; ++b)
        {
            for (int c = 0; c < 2; ++c)
            {
                auto* d = dry.getWritePointer (c);
                for (int n = 0; n < block; ++n)
                {
                    const int i = b * block + n;
                    d[n] = 0.4f * std::sin (juce::MathConstants<float>::twoPi
                                                * 440.0f * (float) i / (float) sr);
                }
            }

            crushed.makeCopyOf (dry);
            clean.makeCopyOf (dry);

            ch.process (crushed.getWritePointer (0), crushed.getWritePointer (1), block, p);
            ref.process (clean.getWritePointer (0), clean.getWritePointer (1), block, cleanParams);

            for (int c = 0; c < 2; ++c)
                for (int n = 0; n < block; ++n)
                {
                    const double d = (double) crushed.getSample (c, n) - (double) clean.getSample (c, n);
                    diffEnergy += d * d;
                    sigEnergy  += (double) clean.getSample (c, n) * (double) clean.getSample (c, n);
                }
        }

        const double diffRatio = std::sqrt (diffEnergy / juce::jmax (1.0e-12, sigEnergy));

        std::cout << "character: crushDiffRatio=" << diffRatio << std::endl;

        if (diffRatio > 0.05)
        {
            ++failures;
            std::cout << "FAIL: crush is too aggressive at a low setting" << std::endl;
        }
    }

    // --- (6) diatonic transpose stays in key --------------------------------
    {
        bool allInScale = true;
        bool shapeKept = true;

        for (int key = 0; key < 12; ++key)
        {
            for (int scale = 0; scale < 10; ++scale)
            {
                for (int degrees = -7; degrees <= 7; ++degrees)
                {
                    for (int note = 24; note < 108; note += 7)
                    {
                        const int inKey = vespera::Scale::quantize (note, key, scale);
                        const int moved = vespera::Scale::transposeDegrees (inKey, degrees, key, scale);

                        if (! vespera::Scale::isInScale (moved, key, scale))
                            allInScale = false;

                        // Transposing up then back down must return the note.
                        const int back = vespera::Scale::transposeDegrees (moved, -degrees, key, scale);
                        if (back != inKey)
                            shapeKept = false;
                    }
                }
            }
        }

        std::cout << "transpose: inScale=" << (allInScale ? "yes" : "NO")
                  << " reversible=" << shapeKept << std::endl;

        if (! allInScale || ! shapeKept)
        {
            ++failures;
            std::cout << "FAIL: diatonic transpose left the scale or was not reversible" << std::endl;
        }
    }

    // --- (7) reverse contour mirrors the pattern ----------------------------
    {
        vespera::PatternEngine pattern;
        vespera::PatternParams p;
        p.steps = 8;
        p.density = 1.0f;
        p.octaves = 1;
        p.mutate = 0.0f;

        juce::Random rngA (99), rngB (99);

        p.contour = vespera::kContourUp;
        pattern.regenerate (4, p, rngA);

        int forward[8] {};
        for (int i = 0; i < p.steps; ++i)
            forward[i] = pattern.getStep (i).poolIndex;

        p.contour = vespera::kContourReverse;
        pattern.regenerate (4, p, rngB);

        bool mirrored = true;
        for (int i = 0; i < p.steps; ++i)
            if (pattern.getStep (i).poolIndex != forward[p.steps - 1 - i])
                mirrored = false;

        std::cout << "reverse: mirrored=" << mirrored << std::endl;

        if (! mirrored)
        {
            ++failures;
            std::cout << "FAIL: reverse contour did not mirror the pattern" << std::endl;
        }
    }

    // --- (8) LFO shapes stay in range ---------------------------------------
    {
        bool inRange = true;
        bool varies = false;

        for (int shape = 0; shape < (int) vespera::Lfo::numShapes; ++shape)
        {
            vespera::Lfo lfo;
            juce::Random rng (7);

            float lo = 2.0f, hi = -2.0f;

            for (int i = 0; i < 2000; ++i)
            {
                const float v = lfo.advance (0.01, shape, rng);
                if (! std::isfinite (v) || v < -1.001f || v > 1.001f)
                    inRange = false;

                lo = juce::jmin (lo, v);
                hi = juce::jmax (hi, v);
            }

            if (hi - lo > 0.1f)
                varies = true;
        }

        std::cout << "lfo: inRange=" << inRange << " varies=" << varies << std::endl;

        if (! inRange || ! varies)
        {
            ++failures;
            std::cout << "FAIL: LFO shapes are out of range or static" << std::endl;
        }
    }

    // --- (9) fold and ring are transparent at zero and bounded at max -------
    {
        const double sr = 48000.0;
        const int block = 512;

        juce::AudioBuffer<float> a (2, block), b (2, block), c (2, block);

        vespera::Character clean, folded, ringed;
        clean.prepare (sr);
        folded.prepare (sr);
        ringed.prepare (sr);

        vespera::CharacterParams pFold;
        pFold.fold = 1.0f;

        vespera::CharacterParams pRing;
        pRing.ring = 1.0f;

        double zeroDiff = 0.0, foldPeak = 0.0, ringPeak = 0.0;

        for (int blk = 0; blk < 30; ++blk)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                auto* d = a.getWritePointer (ch);
                for (int n = 0; n < block; ++n)
                {
                    const int i = blk * block + n;
                    d[n] = 0.8f * std::sin (juce::MathConstants<float>::twoPi
                                                * 220.0f * (float) i / (float) sr);
                }
            }

            b.makeCopyOf (a);
            c.makeCopyOf (a);

            clean.process (a.getWritePointer (0), a.getWritePointer (1), block, {});
            folded.process (b.getWritePointer (0), b.getWritePointer (1), block, pFold);
            ringed.process (c.getWritePointer (0), c.getWritePointer (1), block, pRing);

            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < block; ++n)
                {
                    zeroDiff = juce::jmax (zeroDiff,
                                           (double) std::abs (b.getSample (ch, n)
                                                                  - c.getSample (ch, n)));
                    foldPeak = juce::jmax (foldPeak, (double) std::abs (b.getSample (ch, n)));
                    ringPeak = juce::jmax (ringPeak, (double) std::abs (c.getSample (ch, n)));
                }
        }

        std::cout << "character: foldPeak=" << foldPeak
                  << " ringPeak=" << ringPeak << std::endl;

        if (! (foldPeak < 1.2 && ringPeak < 1.2 && zeroDiff > 0.01))
        {
            ++failures;
            std::cout << "FAIL: fold / ring are silent, unbounded or identical" << std::endl;
        }
    }

    // --- (9) output limiter ---------------------------------------------------
    {
        const double sr = 48000.0;
        const int block = 512;

        vespera::Limiter limiter;
        limiter.prepare (sr);

        vespera::Limiter quietLimiter;
        quietLimiter.prepare (sr);

        juce::AudioBuffer<float> hot (2, block);
        juce::AudioBuffer<float> quiet (2, block);

        double hotPeak = 0.0, quietPeak = 0.0, quietDiff = 0.0;
        float grDb = 0.0f;
        bool limiting = false;

        for (int blk = 0; blk < 40; ++blk)
        {
            for (int n = 0; n < block; ++n)
            {
                const int i = blk * block + n;
                const float loud  = 2.0f * std::sin (juce::MathConstants<float>::twoPi
                                                         * 220.0f * (float) i / (float) sr);
                const float soft  = 0.2f * std::sin (juce::MathConstants<float>::twoPi
                                                         * 220.0f * (float) i / (float) sr);
                hot.setSample (0, n, loud);
                hot.setSample (1, n, loud);
                quiet.setSample (0, n, soft);
                quiet.setSample (1, n, soft);
            }

            limiter.process (hot.getWritePointer (0), hot.getWritePointer (1), block, 0.98f);
            quietLimiter.process (quiet.getWritePointer (0), quiet.getWritePointer (1), block, 0.98f);

            grDb = limiter.getGainReductionDb();
            limiting = limiting || limiter.isLimiting();

            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < block; ++n)
                {
                    hotPeak   = juce::jmax (hotPeak,   (double) std::abs (hot.getSample (ch, n)));
                    quietPeak = juce::jmax (quietPeak, (double) std::abs (quiet.getSample (ch, n)));
                    quietDiff = juce::jmax (quietDiff,
                                            (double) std::abs (quiet.getSample (ch, n)
                                                                   - 0.2f * std::sin (juce::MathConstants<float>::twoPi
                                                                                          * 220.0f * (float) (blk * block + n)
                                                                                          / (float) sr)));
                }
        }

        const bool bounded  = hotPeak <= 1.0;
        const bool reducing = grDb > 3.0f && limiting;
        const bool clean    = quietPeak <= 0.2001 && quietDiff < 1.0e-5;

        std::cout << "limiter: hotPeak=" << hotPeak
                  << " grDb=" << grDb
                  << " quietPeak=" << quietPeak
                  << " quietDiff=" << quietDiff << std::endl;

        if (! (bounded && reducing && clean))
        {
            ++failures;
            std::cout << "FAIL: limiter is not bounding / not transparent" << std::endl;
        }
    }

    if (failures > 0)
    {
        std::cout << "FAIL: generator tests" << std::endl;
        return 1;
    }
    std::cout << "PASS: generator tests" << std::endl;
    return 0;
}