// Offline tests for the Vespera processor, presets and editor.
//
//   (1) Feeding notes generates a musical pattern and audible, bounded audio.
//   (2) The generated pattern stays in the selected scale.
//   (3) Factory presets are listed, installed and loaded.
//   (4) Host state round-trips.
//   (5) The editor builds, selects the default preset and paints.

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <cmath>
#include <functional>
#include <vector>

#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include "../Source/PresetManager.h"

namespace
{
    void setParam (VesperaAudioProcessor& proc, const char* id, float real)
    {
        if (auto* p = proc.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (real));
    }

    float getParam (VesperaAudioProcessor& proc, const char* id)
    {
        return proc.apvts.getRawParameterValue (id)->load();
    }

    struct RenderStats
    {
        double maxAbs = 0.0;
        bool finite = true;
        bool heardSound = false;
        int  maxVoices = 0;
        vespera::PatternView view;
    };

    RenderStats renderGenerator()
    {
        const double sr = 48000.0;
        const int block = 512;

        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);

        setParam (proc, "key", 0.0f);          // C
        setParam (proc, "scale", 1.0f);        // minor
        setParam (proc, "poolSize", 4.0f);
        setParam (proc, "monitor", 1.0f);
        setParam (proc, "rate", 4.0f);         // 1/16
        setParam (proc, "steps", 4.0f);
        setParam (proc, "density", 1.0f);
        setParam (proc, "octaves", 2.0f);
        setParam (proc, "contour", 2.0f);
        setParam (proc, "mutate", 0.0f);
        setParam (proc, "gate", 0.4f);
        setParam (proc, "wave", 1.0f);
        setParam (proc, "filterType", 1.0f);
        setParam (proc, "filterCutoff", 12000.0f);
        setParam (proc, "output", 0.0f);

        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;

        RenderStats stats;

        const int inputBlocks = (int) (sr * 2.0 / block);
        const int tailBlocks  = (int) (sr * 2.0 / block);

        for (int b = 0; b < inputBlocks + tailBlocks; ++b)
        {
            midi.clear();

            if (b == 0)
            {
                midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 63, 0.7f), 0);
                midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.7f), 0);
            }
            else if (b == inputBlocks)
            {
                midi.addEvent (juce::MidiMessage::noteOff (1, 60), 0);
                midi.addEvent (juce::MidiMessage::noteOff (1, 63), 0);
                midi.addEvent (juce::MidiMessage::noteOff (1, 67), 0);
            }

            buffer.clear();
            proc.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
            {
                const float* p = buffer.getReadPointer (ch);
                for (int n = 0; n < block; ++n)
                {
                    if (! std::isfinite (p[n])) stats.finite = false;
                    else
                    {
                        const double a = std::abs ((double) p[n]);
                        stats.maxAbs = juce::jmax (stats.maxAbs, a);
                        if (a > 1.0e-4) stats.heardSound = true;
                    }
                }
            }

            stats.maxVoices = juce::jmax (stats.maxVoices, proc.getActiveVoiceCount());
        }

        proc.getPatternView (stats.view);
        return stats;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI guiInit;

    int failures = 0;

    // --- (1) render / pattern ------------------------------------------------
    {
        const auto stats = renderGenerator();

        bool inScale = true;
        int activeSteps = 0;

        for (int i = 0; i < stats.view.steps; ++i)
        {
            if (stats.view.active[i])
            {
                ++activeSteps;
                if (! vespera::Scale::isInScale (stats.view.note[i],
                                                 stats.view.key, stats.view.scale))
                    inScale = false;
            }
        }

        std::cout << "render: maxAbs=" << stats.maxAbs
                  << " finite=" << (stats.finite ? "yes" : "NO")
                  << " sound=" << (stats.heardSound ? "yes" : "NO")
                  << " maxVoices=" << stats.maxVoices
                  << " pool=" << stats.view.poolSize
                  << " steps=" << stats.view.steps
                  << " active=" << activeSteps
                  << " inScale=" << (inScale ? "yes" : "NO") << std::endl;

        if (! stats.finite || stats.maxAbs > 1.05 || ! stats.heardSound)
        {
            ++failures;
            std::cout << "FAIL: generator did not produce bounded, audible audio" << std::endl;
        }

        if (stats.view.poolSize < 3 || stats.view.steps != 4 || activeSteps != 4 || ! inScale)
        {
            ++failures;
            std::cout << "FAIL: generated pattern is not using the note pool / scale" << std::endl;
        }
    }

    // --- (1b) changing key re-quantises the existing pool --------------------
    {
        const double sr = 48000.0;
        const int block = 512;

        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);

        setParam (proc, "key", 0.0f);          // C
        setParam (proc, "scale", 0.0f);        // major
        setParam (proc, "poolSize", 4.0f);
        setParam (proc, "steps", 8.0f);
        setParam (proc, "density", 1.0f);
        setParam (proc, "mutate", 0.0f);
        setParam (proc, "rate", 4.0f);
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 61, 0.8f), 0);   // C# -> C
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);   // E
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);   // G

        buffer.clear();
        proc.processBlock (buffer, midi);

        setParam (proc, "key", 2.0f);          // D major
        midi.clear();
        for (int b = 0; b < 8; ++b)
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
        }

        vespera::PatternView view;
        proc.getPatternView (view);

        bool requantised = view.poolSize > 0;
        for (int i = 0; i < view.steps; ++i)
            if (view.active[i] && ! vespera::Scale::isInScale (view.note[i], 2, 0))
                requantised = false;

        std::cout << "key change: pool=" << view.poolSize
                  << " inScale=" << (requantised ? "yes" : "NO") << std::endl;

        if (! requantised)
        {
            ++failures;
            std::cout << "FAIL: changing key left the pool outside the new scale" << std::endl;
        }
    }

    // --- (1c) chromatic transpose -------------------------------------------
    {
        const double sr = 48000.0;
        const int block = 512;

        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);

        setParam (proc, "key", 0.0f);          // C
        setParam (proc, "scale", 1.0f);        // minor
        setParam (proc, "poolSize", 4.0f);
        setParam (proc, "steps", 8.0f);
        setParam (proc, "density", 1.0f);
        setParam (proc, "mutate", 0.0f);
        setParam (proc, "octaves", 1.0f);
        setParam (proc, "rate", 4.0f);
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 63, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);

        buffer.clear();
        proc.processBlock (buffer, midi);

        vespera::PatternView before;
        proc.getPatternView (before);

        setParam (proc, "transpose", 3.0f);
        midi.clear();
        for (int b = 0; b < 80; ++b)
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
        }

        vespera::PatternView after;
        proc.getPatternView (after);

        bool shifted = after.transpose == 3 && after.steps == before.steps;
        bool inScale = true;

        for (int i = 0; i < after.steps; ++i)
        {
            if (! (before.active[i] && after.active[i]))
                continue;

            const int expected = vespera::Scale::transposeDegrees (before.note[i], 3,
                                                                  before.key, before.scale);

            if (after.note[i] != expected)
                shifted = false;

            if (! vespera::Scale::isInScale (after.note[i], before.key, before.scale))
                inScale = false;
        }

        std::cout << "transpose: before=" << before.note[0]
                  << " after=" << after.note[0]
                  << " shifted=" << shifted
                  << " inScale=" << (inScale ? "yes" : "NO") << std::endl;

        if (! shifted || ! inScale)
        {
            ++failures;
            std::cout << "FAIL: transpose did not move the pattern within the scale" << std::endl;
        }
    }

    // --- (1c) reverse contour reaches the pattern view mirrored --------------
    {
        // Renders a fresh instance with the given contour and returns the notes
        // of the generated steps.
        auto renderContour = [] (float contour)
        {
            VesperaAudioProcessor proc;
            proc.setPlayConfigDetails (0, 2, 48000.0, 512);

            setParam (proc, "key", 0.0f);          // C
            setParam (proc, "scale", 0.0f);        // major
            setParam (proc, "poolSize", 4.0f);
            setParam (proc, "steps", 8.0f);
            setParam (proc, "density", 1.0f);
            setParam (proc, "octaves", 1.0f);
            setParam (proc, "mutate", 0.0f);
            setParam (proc, "rate", 4.0f);
            setParam (proc, "contour", contour);
            proc.prepareToPlay (48000.0, 512);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);

            buffer.clear();
            proc.processBlock (buffer, midi);

            midi.clear();
            for (int b = 0; b < 4; ++b)
            {
                buffer.clear();
                proc.processBlock (buffer, midi);
            }

            vespera::PatternView v;
            proc.getPatternView (v);
            return v;
        };

        const auto up = renderContour (0.0f);
        const auto rev = renderContour (5.0f);

        bool mirrored = rev.contour == 5 && rev.steps == up.steps;
        for (int i = 0; i < up.steps && mirrored; ++i)
            if (rev.note[i] != up.note[up.steps - 1 - i])
                mirrored = false;

        std::cout << "reverse view: contour=" << rev.contour
                  << " up=";
        for (int i = 0; i < up.steps; ++i)
            std::cout << up.note[i] << " ";
        std::cout << " rev=";
        for (int i = 0; i < rev.steps; ++i)
            std::cout << rev.note[i] << " ";
        std::cout << " mirrored=" << mirrored << std::endl;

        if (! mirrored)
        {
            ++failures;
            std::cout << "FAIL: reverse contour is not visible in the pattern view" << std::endl;
        }
    }

    // --- (1d) clear pool -----------------------------------------------------
    {        const double sr = 48000.0;
        const int block = 512;

        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);
        setParam (proc, "monitor", 0.0f);
        setParam (proc, "density", 1.0f);
        setParam (proc, "steps", 4.0f);
        setParam (proc, "rate", 4.0f);
        setParam (proc, "mutate", 0.0f);
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 64, 0.8f), 0);

        buffer.clear();
        proc.processBlock (buffer, midi);

        proc.clearNotePool();
        midi.clear();

        for (int b = 0; b < 140; ++b)
        {
            buffer.clear();
            proc.processBlock (buffer, midi);
        }

        vespera::PatternView view;
        proc.getPatternView (view);

        double peak = 0.0;
        for (int ch = 0; ch < 2; ++ch)
            for (int n = 0; n < block; ++n)
                peak = juce::jmax (peak, (double) std::abs (buffer.getSample (ch, n)));

        const bool cleared = view.poolSize == 0 && proc.getActiveVoiceCount() == 0 && peak < 1.0e-4;

        std::cout << "clear pool: size=" << view.poolSize
                  << " voices=" << proc.getActiveVoiceCount()
                  << " peak=" << peak
                  << " cleared=" << cleared << std::endl;

        if (! cleared)
        {
            ++failures;
            std::cout << "FAIL: clearing the pool did not stop the generator" << std::endl;
        }
    }

    // --- (1e) RANDOM leaves MUTATE and NOISE alone ---------------------------
    {
        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);

        const bool mutateDefaultsOff = std::abs (getParam (proc, "mutate")) < 0.001f;

        setParam (proc, "mutate", 0.0f);
        setParam (proc, "noise", 0.42f);

        proc.randomizeParameters();

        const bool keptMutate = std::abs (getParam (proc, "mutate")) < 0.001f;
        const bool keptNoise  = std::abs (getParam (proc, "noise") - 0.42f) < 0.001f;

        std::cout << "random: mutateDefault=" << mutateDefaultsOff
                  << " keptMutate=" << keptMutate
                  << " keptNoise=" << keptNoise << std::endl;

        if (! (mutateDefaultsOff && keptMutate && keptNoise))
        {
            ++failures;
            std::cout << "FAIL: RANDOM changed MUTATE or NOISE" << std::endl;
        }
    }

    // --- (1f) RANDOM always leaves an audible patch --------------------------
    // The cutoff window depends on the filter type, so a high-pass can no longer
    // land at 18 kHz and make the instrument silent.
    {
        const double sr = 48000.0;
        const int block = 512;
        constexpr int kRolls = 24;

        double quietest = 1.0e9;
        int silent = 0;
        int hpCount = 0;

        for (int roll = 0; roll < kRolls; ++roll)
        {
            VesperaAudioProcessor proc;
            proc.setPlayConfigDetails (0, 2, sr, block);
            proc.prepareToPlay (sr, block);
            proc.randomizeParameters();

            if ((int) getParam (proc, "filterType") == 3)
                ++hpCount;

            juce::AudioBuffer<float> buffer (2, block);
            juce::MidiBuffer midi;

            double peak = 0.0;

            for (int b = 0; b < 220; ++b)   // ~2.3 s
            {
                buffer.clear();
                proc.processBlock (buffer, midi);
                midi.clear();

                for (int ch = 0; ch < 2; ++ch)
                {
                    const float* p = buffer.getReadPointer (ch);
                    for (int n = 0; n < block; ++n)
                        peak = juce::jmax (peak, std::abs ((double) p[n]));
                }
            }

            quietest = juce::jmin (quietest, peak);

            if (peak < 0.02)
            {
                ++silent;
                vespera::PatternView v;
                proc.getPatternView (v);
                int act = 0;
                for (int i = 0; i < v.steps; ++i)
                    act += v.active[i] ? 1 : 0;

                std::cout << "  silent roll: pool=" << v.poolSize
                          << " steps=" << v.steps << " active=" << act
                          << " type=" << (int) getParam (proc, "filterType")
                          << " wave=" << (int) getParam (proc, "wave")
                          << " cutoff=" << getParam (proc, "filterCutoff")
                          << " reso=" << getParam (proc, "filterReso")
                          << " env=" << getParam (proc, "filterEnv")
                          << " motion=" << getParam (proc, "motion")
                          << " density=" << getParam (proc, "density")
                          << " gate=" << getParam (proc, "gate")
                          << " oct=" << (int) getParam (proc, "octaves")
                          << " rate=" << (int) getParam (proc, "rate")
                          << " steps=" << (int) getParam (proc, "steps")
                          << " attack=" << getParam (proc, "attack")
                          << " decay=" << getParam (proc, "decay")
                          << " sustain=" << getParam (proc, "sustain")
                          << std::endl;
            }
        }

        std::cout << "random audible: rolls=" << kRolls
                  << " quietestPeak=" << quietest
                  << " silent=" << silent
                  << " highPassPicks=" << hpCount << std::endl;

        if (silent > 0)
        {
            ++failures;
            std::cout << "FAIL: RANDOM produced a silent patch" << std::endl;
        }
    }

    // --- (1b) dense render never clips ---------------------------------------
    {
        const double sr = 48000.0;
        const int block = 512;

        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);

        setParam (proc, "poolSize", 8.0f);
        setParam (proc, "steps", 16.0f);
        setParam (proc, "density", 1.0f);
        setParam (proc, "octaves", 3.0f);
        setParam (proc, "gate", 1.0f);
        setParam (proc, "release", 2.0f);
        setParam (proc, "rate", 5.0f);         // 1/32
        setParam (proc, "monitor", 1.0f);
        setParam (proc, "output", 6.0f);
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;

        double peak = 0.0;
        bool finite = true;

        for (int b = 0; b < 400; ++b)
        {
            midi.clear();
            if (b == 0)
                for (int i = 0; i < 8; ++i)
                    midi.addEvent (juce::MidiMessage::noteOn (1, 48 + i * 3, 1.0f), 0);

            buffer.clear();
            proc.processBlock (buffer, midi);

            for (int ch = 0; ch < 2; ++ch)
                for (int n = 0; n < block; ++n)
                {
                    const float s = buffer.getSample (ch, n);
                    if (! std::isfinite (s)) finite = false;
                    else peak = juce::jmax (peak, (double) std::abs (s));
                }
        }

        std::cout << "dense render: peak=" << peak
                  << " finite=" << (finite ? "yes" : "NO") << std::endl;

        if (! finite || peak > 1.0)
        {
            ++failures;
            std::cout << "FAIL: dense render clipped or produced invalid samples" << std::endl;
        }
    }

    // --- (1f) key lock keeps the tonality on RANDOM -------------------------
    {
        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);

        setParam (proc, "key", 5.0f);
        setParam (proc, "scale", 2.0f);
        setParam (proc, "keyLock", 1.0f);

        bool locked = true;
        for (int i = 0; i < 8; ++i)
        {
            proc.randomizeParameters();
            if (std::abs (getParam (proc, "key") - 5.0f) > 0.01f
                || std::abs (getParam (proc, "scale") - 2.0f) > 0.01f)
                locked = false;
        }

        setParam (proc, "keyLock", 0.0f);

        bool changed = false;
        for (int i = 0; i < 20 && ! changed; ++i)
        {
            proc.randomizeParameters();
            if (std::abs (getParam (proc, "key") - 5.0f) > 0.01f)
                changed = true;
        }

        const bool monitorOff = std::abs (getParam (proc, "monitor")) < 0.01f;

        std::cout << "key lock: locked=" << locked
                  << " unlockedRandomises=" << changed
                  << " monitorDefaultOff=" << monitorOff << std::endl;

        if (! locked || ! changed || ! monitorOff)
        {
            ++failures;
            std::cout << "FAIL: key lock / monitor default behaviour" << std::endl;
        }
    }

    // --- (1g) MIDI export ----------------------------------------------------
    {
        const double sr = 48000.0;
        const int block = 512;

        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, sr, block);

        setParam (proc, "key", 0.0f);
        setParam (proc, "scale", 1.0f);
        setParam (proc, "steps", 8.0f);
        setParam (proc, "density", 1.0f);
        setParam (proc, "mutate", 0.0f);
        setParam (proc, "octaves", 1.0f);
        setParam (proc, "rate", 4.0f);
        setParam (proc, "monitor", 0.0f);
        proc.prepareToPlay (sr, block);

        juce::AudioBuffer<float> buffer (2, block);
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 63, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.8f), 0);

        buffer.clear();
        proc.processBlock (buffer, midi);

        auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("vespera_test_pattern.mid");
        file.deleteFile();

        const bool written = proc.writePatternMidiFile (file);

        int noteOns = 0;
        bool readBack = false;
        int noteCount = 0;

        if (written && file.existsAsFile())
        {
            juce::FileInputStream in (file);
            if (in.openedOk())
            {
                juce::MidiFile mf;
                if (mf.readFrom (in))
                {
                    readBack = mf.getNumTracks() == 1;

                    if (readBack)
                    {
                        const auto* track = mf.getTrack (0);
                        for (int i = 0; i < track->getNumEvents(); ++i)
                            if (track->getEventPointer (i)->message.isNoteOn())
                                ++noteOns;

                        noteCount = track->getNumEvents();
                    }
                }
            }
        }

        file.deleteFile();

        std::cout << "midi export: written=" << written
                  << " readBack=" << readBack
                  << " noteOns=" << noteOns
                  << " events=" << noteCount << std::endl;

        if (! written || ! readBack || noteOns <= 0)
        {
            ++failures;
            std::cout << "FAIL: MIDI export did not produce a readable clip" << std::endl;
        }
    }

    // --- (2) factory presets -------------------------------------------------
    {
        auto tmpDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("vespera_generator_test");
        tmpDir.deleteRecursively();
        tmpDir.createDirectory();

        VesperaAudioProcessor proc;
        PresetManager pm (proc.apvts, tmpDir);

        const auto names = pm.getPresetNames();
        const bool listed = names.contains ("First Light")
                         && names.contains ("Rain Arp")
                         && names.contains ("Broken Music Box")
                         && names.contains ("Acid Walk");

        pm.installFactoryPresets();
        const bool installed = tmpDir.getChildFile ("First Light.vesperapreset").existsAsFile()
                            && tmpDir.getChildFile ("Acid Walk.vesperapreset").existsAsFile();

        setParam (proc, "key", 0.0f);
        setParam (proc, "wave", 1.0f);
        setParam (proc, "steps", 8.0f);

        const bool loaded = pm.loadPreset ("Frozen Motif");
        const bool applied = loaded
                          && std::abs (getParam (proc, "key") - 11.0f) < 0.01f
                          && std::abs (getParam (proc, "wave") - 3.0f) < 0.01f
                          && std::abs (getParam (proc, "steps") - 5.0f) < 0.01f;

        tmpDir.getChildFile ("Frozen Motif.vesperapreset").deleteFile();
        setParam (proc, "steps", 8.0f);
        const bool fallback = pm.loadPreset ("Frozen Motif")
                           && std::abs (getParam (proc, "steps") - 5.0f) < 0.01f;

        const bool defaultName = PresetManager::getDefaultPresetName() == "First Light";

        std::cout << "presets: listed=" << listed << " installed=" << installed
                  << " loaded=" << loaded << " applied=" << applied
                  << " fallback=" << fallback << " default=" << defaultName << std::endl;

        if (! (listed && installed && applied && fallback && defaultName))
        {
            ++failures;
            std::cout << "FAIL: factory preset handling" << std::endl;
        }

        tmpDir.deleteRecursively();
    }

    // --- (3) state round-trip ------------------------------------------------
    {
        VesperaAudioProcessor proc;
        setParam (proc, "steps", 12.0f);
        setParam (proc, "key", 7.0f);
        setParam (proc, "density", 0.33f);

        juce::MemoryBlock mb;
        proc.getStateInformation (mb);

        setParam (proc, "steps", 4.0f);
        setParam (proc, "key", 0.0f);
        setParam (proc, "density", 0.9f);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());

        const bool restored = std::abs (getParam (proc, "steps") - 12.0f) < 0.01f
                           && std::abs (getParam (proc, "key") - 7.0f) < 0.01f
                           && std::abs (getParam (proc, "density") - 0.33f) < 0.01f
                           && proc.wasStateRestored();

        std::cout << "state round-trip=" << restored << std::endl;
        if (! restored)
        {
            ++failures;
            std::cout << "FAIL: state did not round-trip" << std::endl;
        }
    }

    // --- (4) editor default preset / re-open / restored project --------------
    {
        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);

        bool comboShowsDefault = false;
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
            std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
            {
                for (auto* child : c.getChildren())
                {
                    if (auto* box = dynamic_cast<juce::ComboBox*> (child))
                        if (box->getText() == "First Light")
                            comboShowsDefault = true;
                    walk (*child);
                }
            };
            walk (*editor);
        }
        const bool appliedFlag = proc.wasDefaultPresetApplied();

        setParam (proc, "steps", 12.0f);
        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        }
        const bool editSurvived = std::abs (getParam (proc, "steps") - 12.0f) < 0.01f;

        std::cout << "editor: combo=" << comboShowsDefault
                  << " appliedFlag=" << appliedFlag
                  << " editSurvived=" << editSurvived << std::endl;

        if (! (comboShowsDefault && appliedFlag && editSurvived))
        {
            ++failures;
            std::cout << "FAIL: default preset / editor re-open behaviour" << std::endl;
        }
    }

    // --- restored project is never overridden --------------------------------
    {
        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);
        setParam (proc, "steps", 6.0f);

        juce::MemoryBlock mb;
        proc.getStateInformation (mb);
        proc.setStateInformation (mb.getData(), (int) mb.getSize());

        {
            std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        }
        const bool kept = std::abs (getParam (proc, "steps") - 6.0f) < 0.01f;

        std::cout << "restored kept=" << kept << std::endl;
        if (! kept)
        {
            ++failures;
            std::cout << "FAIL: restored project was overridden by the default preset" << std::endl;
        }
    }

    // --- (5) editor paints ---------------------------------------------------
    {
        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
        editor->setSize (editor->getWidth(), editor->getHeight());
        const auto snapshot = editor->createComponentSnapshot (editor->getLocalBounds(), false, 1.0f);

        const bool painted = snapshot.isValid()
                          && snapshot.getWidth() > 100
                          && snapshot.getHeight() > 100;

        std::cout << "editor snapshot: " << snapshot.getWidth() << "x"
                  << snapshot.getHeight() << " ok=" << painted << std::endl;

        if (! painted)
        {
            ++failures;
            std::cout << "FAIL: editor did not paint" << std::endl;
        }
    }

    // --- (6) filter SYNC button locks the RATE knob --------------------------
    {
        VesperaAudioProcessor proc;
        proc.setPlayConfigDetails (0, 2, 48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());

        juce::Slider* rateKnob = nullptr;
        juce::Button* syncButton = nullptr;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* s = dynamic_cast<juce::Slider*> (child))
                    if (s->getTooltip().startsWith ("RATE - LFO speed in Hz"))
                        rateKnob = s;

                if (auto* b = dynamic_cast<juce::Button*> (child))
                    if (b->getButtonText() == "Sync")
                        syncButton = b;

                walk (*child);
            }
        };
        walk (*editor);

        auto refresh = [&editor] { editor->resized(); };

        refresh();

        const bool freeByDefault = getParam (proc, "lfoSync") < 0.5f;
        const bool foundControls = rateKnob != nullptr && syncButton != nullptr;
        const bool knobActiveInFree = foundControls && rateKnob->isEnabled();

        setParam (proc, "lfoSync", 1.0f);
        refresh();

        const bool knobLockedInSync = foundControls && ! rateKnob->isEnabled();

        setParam (proc, "lfoSync", 0.0f);
        refresh();

        const bool knobBackInFree = foundControls && rateKnob->isEnabled();

        const bool laidOut = foundControls
                          && rateKnob->isVisible() && syncButton->isVisible()
                          && rateKnob->getBounds().getHeight() >= 40
                          && syncButton->getBounds().getWidth() >= 20
                          && ! rateKnob->getBounds().intersects (syncButton->getBounds());

        std::cout << "filter sync: freeDefault=" << freeByDefault
                  << " found=" << foundControls
                  << " activeFree=" << knobActiveInFree
                  << " lockedSync=" << knobLockedInSync
                  << " backFree=" << knobBackInFree
                  << " laidOut=" << laidOut << std::endl;

        if (! (freeByDefault && foundControls && knobActiveInFree
               && knobLockedInSync && knobBackInFree && laidOut))
        {
            ++failures;
            std::cout << "FAIL: filter SYNC does not lock the RATE knob" << std::endl;
        }
    }

    // --- (7) output limiter --------------------------------------------------
    {
        struct HotStats
        {
            double peak = 0.0;
            int    clipped = 0;      // samples that reached (or passed) full scale
        };

        auto renderHot = [] (bool limiterOn, bool& found)
        {
            const double srLocal = 48000.0;
            const int blockLocal = 512;

            VesperaAudioProcessor proc;
            proc.setPlayConfigDetails (0, 2, srLocal, blockLocal);

            setParam (proc, "key", 0.0f);
            setParam (proc, "scale", 1.0f);
            setParam (proc, "poolSize", 8.0f);
            setParam (proc, "monitor", 1.0f);
            setParam (proc, "rate", 4.0f);
            setParam (proc, "steps", 16.0f);
            setParam (proc, "density", 1.0f);
            setParam (proc, "octaves", 3.0f);
            setParam (proc, "mutate", 0.0f);
            setParam (proc, "gate", 1.0f);
            setParam (proc, "wave", 2.0f);          // saw
            setParam (proc, "filterType", 0.0f);    // no filtering
            setParam (proc, "drive", 0.8f);
            setParam (proc, "fold", 0.5f);
            setParam (proc, "output", 9.0f);
            setParam (proc, "limiter", limiterOn ? 1.0f : 0.0f);

            found = (getParam (proc, "limiter") > 0.5f) == limiterOn;

            proc.prepareToPlay (srLocal, blockLocal);

            juce::AudioBuffer<float> buffer (2, blockLocal);
            juce::MidiBuffer midi;
            midi.addEvent (juce::MidiMessage::noteOn (1, 60, 1.0f), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 64, 1.0f), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 67, 1.0f), 0);
            midi.addEvent (juce::MidiMessage::noteOn (1, 72, 1.0f), 0);

            HotStats stats;
            const int blocks = (int) (srLocal * 1.5 / blockLocal);

            for (int b = 0; b < blocks; ++b)
            {
                buffer.clear();
                proc.processBlock (buffer, midi);
                midi.clear();

                for (int ch = 0; ch < 2; ++ch)
                {
                    const float* p = buffer.getReadPointer (ch);
                    for (int n = 0; n < blockLocal; ++n)
                    {
                        const double a = std::abs ((double) p[n]);
                        stats.peak = juce::jmax (stats.peak, a);
                        if (a >= 0.9999) ++stats.clipped;
                    }
                }
            }

            return stats;
        };

        bool foundOn = false, foundOff = false;
        const auto on  = renderHot (true,  foundOn);
        const auto off = renderHot (false, foundOff);

        VesperaAudioProcessor fresh;
        const bool onByDefault = getParam (fresh, "limiter") > 0.5f;

        // The limiter trades a few attack overshoots for a huge reduction in
        // full-scale samples; bypassed, the safety clamp clips instead.
        const bool limited   = foundOn && on.clipped * 10 < off.clipped;
        const bool bypassed  = foundOff && off.clipped > 0;
        const bool anyTaming = on.clipped < off.clipped;

        std::cout << "limiter: defaultOn=" << onByDefault
                  << " peakOn=" << on.peak << " clippedOn=" << on.clipped
                  << " peakOff=" << off.peak << " clippedOff=" << off.clipped
                  << " limited=" << limited
                  << " bypass=" << bypassed
                  << " difference=" << anyTaming << std::endl;

        if (! (onByDefault && limited && bypassed && anyTaming))
        {
            ++failures;
            std::cout << "FAIL: output limiter does not bound the output" << std::endl;
        }

        // The OUTPUT column carries the Limiter checkbox.
        std::unique_ptr<juce::AudioProcessorEditor> editor (fresh.createEditor());
        juce::Button* limiterButton = nullptr;
        std::function<void (juce::Component&)> walk = [&] (juce::Component& c)
        {
            for (auto* child : c.getChildren())
            {
                if (auto* b = dynamic_cast<juce::Button*> (child))
                    if (b->getButtonText() == "Limiter")
                        limiterButton = b;
                walk (*child);
            }
        };
        walk (*editor);

        const bool uiOk = limiterButton != nullptr
                       && limiterButton->isVisible()
                       && limiterButton->getToggleState()
                       && limiterButton->getBounds().getWidth() > 20;

        std::cout << "limiter ui: found=" << (limiterButton != nullptr)
                  << " ok=" << uiOk << std::endl;

        if (! uiOk)
        {
            ++failures;
            std::cout << "FAIL: limiter checkbox is missing from OUTPUT" << std::endl;
        }
    }

    if (failures > 0)
    {
        std::cout << "FAIL: Vespera plugin tests" << std::endl;
        return 1;
    }

    std::cout << "PASS: Vespera plugin tests" << std::endl;
    return 0;
}
