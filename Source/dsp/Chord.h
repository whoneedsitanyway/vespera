#pragma once

#include "Scale.h"

#include <algorithm>

namespace vespera
{

//==============================================================================
//  Diatonic chord voicing.
//
//  The pattern engine describes a chord with a bit mask and an inversion; the
//  actual pitches are derived from the selected scale here.  Keeping the
//  voicing in one place means the synth, the pattern display and the exported
//  MIDI file always agree on what is playing.
//
//  Mask bits:
//    0x1  root        (always set)
//    0x2  third       (diatonic degree +2)
//    0x4  fifth       (diatonic degree +4)
//    0x8  seventh     (diatonic degree +6, heptatonic scales only)
//    0x10 minor colour (chromatic scale only; ignored by diatonic scales)
//==============================================================================
struct ChordVoicing
{
    int notes[4] {};
    int count = 0;
};

inline ChordVoicing buildChord (int root, int key, int scaleIndex,
                                int mask, int inversion)
{
    const auto& intervals = Scale::getIntervals (scaleIndex);
    const int degrees = (int) intervals.size();
    const bool chromatic = degrees >= 12;

    int tones[4] {};
    int count = 0;

    tones[count++] = juce::jlimit (0, 127, root);

    // Third.  On the diatonic and pentatonic scales the chord is built from
    // scale degrees, so every voicing is in key by construction.  The
    // chromatic scale has no diatonic thirds, so a major or minor third is
    // used instead (the minor colour is carried by bit 0x10).
    if ((mask & 0x2) != 0)
        tones[count++] = chromatic ? root + (((mask & 0x10) != 0) ? 3 : 4)
                                   : Scale::transposeDegrees (root, 2, key, scaleIndex);

    // Fifth.  In the pentatonic modes degree +4 is a sixth, which keeps the
    // chord open instead of turning it into a stack of seconds.
    if ((mask & 0x4) != 0)
        tones[count++] = chromatic ? root + 7
                                   : Scale::transposeDegrees (root, 4, key, scaleIndex);

    // Seventh: only where the scale actually has one.  In the pentatonic modes
    // degree +6 would wrap into a second, so the bit is ignored there.
    if ((mask & 0x8) != 0 && degrees >= 7)
        tones[count++] = chromatic ? root + (((mask & 0x10) != 0) ? 10 : 11)
                                   : Scale::transposeDegrees (root, 6, key, scaleIndex);

    // Inversion: lift the lowest notes by an octave.  A voicing that would run
    // past the MIDI range simply loses the offending note instead of folding it
    // back onto an existing pitch (which would double that note's level).
    const int inv = juce::jlimit (0, count - 1, inversion);
    for (int i = 0; i < inv; ++i)
        tones[i] += 12;

    std::sort (tones, tones + count);

    ChordVoicing voicing;
    for (int i = 0; i < count; ++i)
    {
        if (tones[i] < 0 || tones[i] > 127)
            continue;

        if (voicing.count == 0 || tones[i] != voicing.notes[voicing.count - 1])
            voicing.notes[voicing.count++] = tones[i];
    }

    return voicing;
}

} // namespace vespera
