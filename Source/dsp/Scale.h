#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <algorithm>
#include <cmath>

namespace vespera
{

//==============================================================================
//  Musical scales and quantisation.
//
//  Every note that enters the generator (played by the user or produced by the
//  pattern engine) is snapped to the selected scale, so the random melodies
//  always stay in key instead of wandering into wrong pitches.
//==============================================================================
class Scale
{
public:
    static const juce::StringArray& getNames()
    {
        static const juce::StringArray names { "Major", "Minor", "Dorian", "Phrygian", "Lydian",
                                               "Mixolydian", "Harm. Minor", "Pent. Major",
                                               "Pent. Minor", "Chromatic" };
        return names;
    }

    static const juce::StringArray& getKeyNames()
    {
        static const juce::StringArray names { "C", "C#", "D", "D#", "E", "F",
                                               "F#", "G", "G#", "A", "A#", "B" };
        return names;
    }

    static const std::vector<int>& getIntervals (int scaleIndex)
    {
        static const std::vector<int> major       { 0, 2, 4, 5, 7, 9, 11 };
        static const std::vector<int> minor       { 0, 2, 3, 5, 7, 8, 10 };
        static const std::vector<int> dorian      { 0, 2, 3, 5, 7, 9, 10 };
        static const std::vector<int> phrygian    { 0, 1, 3, 5, 7, 8, 10 };
        static const std::vector<int> lydian      { 0, 2, 4, 6, 7, 9, 11 };
        static const std::vector<int> mixolydian  { 0, 2, 4, 5, 7, 9, 10 };
        static const std::vector<int> harmMinor   { 0, 2, 3, 5, 7, 8, 11 };
        static const std::vector<int> pentMajor   { 0, 2, 4, 7, 9 };
        static const std::vector<int> pentMinor   { 0, 3, 5, 7, 10 };
        static const std::vector<int> chromatic   { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

        switch (juce::jlimit (0, 9, scaleIndex))
        {
            case 0:  return major;
            case 1:  return minor;
            case 2:  return dorian;
            case 3:  return phrygian;
            case 4:  return lydian;
            case 5:  return mixolydian;
            case 6:  return harmMinor;
            case 7:  return pentMajor;
            case 8:  return pentMinor;
            default: return chromatic;
        }
    }

    // Snaps a MIDI note to the nearest degree of the selected scale.
    static int quantize (int midiNote, int key, int scaleIndex)
    {
        key = juce::jlimit (0, 11, key);

        const auto& intervals = getIntervals (scaleIndex);
        if (intervals.empty())
            return juce::jlimit (0, 127, midiNote);

        // Work relative to the tonic and search a small window around the note.
        int bestNote = midiNote;
        int bestDistance = 128;

        // Keep the search inside the valid MIDI range so clamping can never
        // push the result back onto a note that is outside the scale.
        const int lo = juce::jmax (0, midiNote - 6);
        const int hi = juce::jmin (127, midiNote + 6);

        for (int candidate = lo; candidate <= hi; ++candidate)
        {
            const int pc = ((candidate - key) % 12 + 12) % 12;
            const bool inScale = std::find (intervals.begin(), intervals.end(), pc) != intervals.end();

            if (inScale)
            {
                const int distance = std::abs (candidate - midiNote);
                if (distance < bestDistance)
                {
                    bestDistance = distance;
                    bestNote = candidate;
                }
            }
        }

        return juce::jlimit (0, 127, bestNote);
    }

    static bool isInScale (int midiNote, int key, int scaleIndex)
    {
        const int pc = ((midiNote - juce::jlimit (0, 11, key)) % 12 + 12) % 12;
        const auto& intervals = getIntervals (scaleIndex);
        return std::find (intervals.begin(), intervals.end(), pc) != intervals.end();
    }

    // Moves a note by a number of *scale degrees* instead of semitones, so a
    // transposed pattern keeps its melodic shape but always stays in key.
    static int transposeDegrees (int midiNote, int degrees, int key, int scaleIndex)
    {
        key = juce::jlimit (0, 11, key);

        if (degrees == 0)
            return juce::jlimit (0, 127, midiNote);

        const auto& intervals = getIntervals (scaleIndex);
        const int n = (int) intervals.size();
        if (n <= 0)
            return juce::jlimit (0, 127, midiNote);

        midiNote = quantize (midiNote, key, scaleIndex);

        const int relative = midiNote - key;
        const int octave = (int) std::floor ((double) relative / 12.0);
        const int pc = ((relative % 12) + 12) % 12;

        int degree = 0;
        for (int i = 0; i < n; ++i)
            if (intervals[(size_t) i] == pc)
            {
                degree = i;
                break;
            }

        const long long absoluteDegree = (long long) octave * n + degree + degrees;
        long long newOctave = absoluteDegree / n;
        long long newDegree = absoluteDegree % n;

        if (newDegree < 0)
        {
            newDegree += n;
            --newOctave;
        }

        const int transposed = key + (int) newOctave * 12 + intervals[(size_t) newDegree];
        return juce::jlimit (0, 127, transposed);
    }

    static juce::String noteName (int midiNote)
    {
        return juce::MidiMessage::getMidiNoteName (juce::jlimit (0, 127, midiNote),
                                                   true, true, 3);
    }
};

} // namespace vespera
