// Offline tests for the polyphonic synth voice engine:
//   (1) notes produce sound and release to silence,
//   (2) extreme settings stay finite and bounded,
//   (3) voice stealing never exceeds the voice limit.

#include <juce_audio_basics/juce_audio_basics.h>
#include <iostream>
#include <vector>
#include <cmath>

#include "../Source/dsp/PolySynth.h"

namespace
{
    double renderMax (vespera::PolySynth& synth, int blocks, int blockSize)
    {
        juce::AudioBuffer<float> buf (2, blockSize);
        double maxAbs = 0.0;
        bool finite = true;

        for (int b = 0; b < blocks; ++b)
        {
            buf.clear();
            synth.render (buf.getWritePointer (0), buf.getWritePointer (1), blockSize);

            for (int ch = 0; ch < 2; ++ch)
            {
                const float* p = buf.getReadPointer (ch);
                for (int n = 0; n < blockSize; ++n)
                {
                    if (! std::isfinite (p[n])) finite = false;
                    else maxAbs = juce::jmax (maxAbs, (double) std::abs (p[n]));
                }
            }
        }

        if (! finite)
            maxAbs = 1.0e9;

        return maxAbs;
    }
}

int main()
{
    const double sr = 48000.0;
    const int block = 512;
    int failures = 0;

    // --- (1) note on / release ----------------------------------------------
    {
        vespera::PolySynth synth;
        synth.prepare (sr);
        synth.setParams (1, 0.005f, 0.2f, 0.6f, 0.3f, 0.15f);

        synth.noteOn (60, 0.8f);
        const double maxAbs = renderMax (synth, (int) (sr / block), block);
        const int voices = synth.getActiveVoiceCount();

        synth.noteOff (60);
        const double tail = renderMax (synth, (int) (sr * 2.0 / block), block);
        const int voicesAfter = synth.getActiveVoiceCount();

        std::cout << "synth: maxAbs=" << maxAbs
                  << " voices=" << voices
                  << " tail=" << tail
                  << " voicesAfter=" << voicesAfter << std::endl;

        if (! (maxAbs > 1.0e-4 && maxAbs < 1.5 && voices > 0 && voicesAfter == 0))
        {
            ++failures;
            std::cout << "FAIL: synth note did not sound or did not release" << std::endl;
        }
    }

    // --- (2) extreme polyphony ----------------------------------------------
    {
        vespera::PolySynth synth;
        synth.prepare (sr);
        synth.setParams (3, 0.001f, 0.1f, 1.0f, 0.1f, 1.0f);

        for (int i = 0; i < 64; ++i)
            synth.noteOnTimed (36 + (i % 60), 1.0f, (int) (sr * 0.05));

        const double maxAbs = renderMax (synth, (int) (sr * 4.0 / block), block);
        const int voices = synth.getActiveVoiceCount();

        std::cout << "synth extreme: maxAbs=" << maxAbs
                  << " voices=" << voices << std::endl;

        if (! (maxAbs < 4.0 && voices <= vespera::PolySynth::kMaxVoices))
        {
            ++failures;
            std::cout << "FAIL: synth ran away or exceeded the voice limit" << std::endl;
        }
    }

    if (failures > 0)
    {
        std::cout << "FAIL: synth tests" << std::endl;
        return 1;
    }

    std::cout << "PASS: synth tests" << std::endl;
    return 0;
}
