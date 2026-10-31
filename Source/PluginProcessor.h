#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <array>
#include <vector>

#include "dsp/Scale.h"
#include "dsp/Chord.h"
#include "dsp/NotePool.h"
#include "dsp/PatternEngine.h"
#include "dsp/PolySynth.h"
#include "dsp/ResonantFilter.h"
#include "dsp/Character.h"
#include "dsp/Lfo.h"
#include "dsp/Limiter.h"
#include "DebugLogger.h"

//==============================================================================
namespace vespera
{
    namespace ids
    {
        // Notes / scale.
        static constexpr const char* key       = "key";
        static constexpr const char* scale     = "scale";
        static constexpr const char* poolSize  = "poolSize";
        static constexpr const char* latch     = "latch";
        static constexpr const char* monitor   = "monitor";
        static constexpr const char* keyLock   = "keyLock";

        // Pattern generator.
        static constexpr const char* rate     = "rate";
        static constexpr const char* steps    = "steps";
        static constexpr const char* density  = "density";
        static constexpr const char* octaves  = "octaves";
        static constexpr const char* contour  = "contour";
        static constexpr const char* mutate   = "mutate";
        static constexpr const char* swing    = "swing";
        static constexpr const char* gate     = "gate";
        static constexpr const char* accent   = "accent";
        static constexpr const char* transpose = "transpose";

        // Synth voice.
        static constexpr const char* wave    = "wave";
        static constexpr const char* attack  = "attack";
        static constexpr const char* decay   = "decay";
        static constexpr const char* sustain = "sustain";
        static constexpr const char* release = "release";
        static constexpr const char* detune  = "detune";

        // Filter / motion.
        static constexpr const char* filterType   = "filterType";
        static constexpr const char* filterCutoff = "filterCutoff";
        static constexpr const char* filterReso   = "filterReso";
        static constexpr const char* filterEnv    = "filterEnv";
        static constexpr const char* lfoRate      = "lfoRate";
        static constexpr const char* lfoRateHz    = "lfoRateHz";
        static constexpr const char* lfoSync      = "lfoSync";
        static constexpr const char* lfoShape     = "lfoShape";
        static constexpr const char* motion       = "motion";

        // Character.
        static constexpr const char* drive = "drive";
        static constexpr const char* crush = "crush";
        static constexpr const char* noise = "noise";
        static constexpr const char* wow   = "wow";
        static constexpr const char* fold  = "fold";
        static constexpr const char* ring  = "ring";

        // Output.
        static constexpr const char* output  = "output";
        static constexpr const char* limiter = "limiter";
        static constexpr const char* chords  = "chords";
        static constexpr const char* transportSync = "transportSync";
    }

    //==========================================================================
    //  Snapshot of the generated pattern for the UI.
    struct PatternView
    {
        int  steps = 8;
        int  currentStep = 0;
        int  poolSize = 0;
        int  poolNotes[NotePool::kMaxNotes] {};
        bool active[PatternEngine::kMaxSteps] {};
        int  note[PatternEngine::kMaxSteps] {};
        float velocity[PatternEngine::kMaxSteps] {};
        juce::uint8 chordMask[PatternEngine::kMaxSteps] {};
        juce::uint8 chordInversion[PatternEngine::kMaxSteps] {};
        int  key = 0;
        int  scale = 1;
        int  transpose = 0;
        int  contour = 2;
        int  rate = 4;
        bool running = true;
        float density = 0.7f;
        float mutate = 0.0f;
    };
}

//==============================================================================
class VesperaAudioProcessor : public juce::AudioProcessor
{
public:
    VesperaAudioProcessor();
    ~VesperaAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Randomises the pattern / voice / character parameters and seeds the note
    // pool with a chord in the current key.  Message thread only.
    void randomizeParameters();

    // Drops every note from the pool so the generator stops until new notes
    // arrive.  Safe to call from the message thread.
    void clearNotePool();

    // Writes the current pattern as a Standard MIDI File so it can be dropped
    // into a sequencer.  Message thread only.
    bool writePatternMidiFile (const juce::File& destination) const;

    // --- UI ----------------------------------------------------------------
    bool getPatternView (vespera::PatternView& view) const;

    int   getActiveVoiceCount() const noexcept { return displayVoices.load (std::memory_order_relaxed); }
    float getDisplayBpm() const noexcept       { return displayBpm.load (std::memory_order_relaxed); }

    // True while the sequencer is actually advancing (i.e. the transport is
    // playing, or transport sync is off).
    bool isSequencerRunning() const noexcept
    {
        return displayRunning.load (std::memory_order_relaxed);
    }

    // Step length of the pattern, expressed in quarter-note beats.
    static double rateInBeats (int rateIndex);
    static int    numRates();

    vespera::DebugLogger& getDebugLogger() noexcept { return debugLogger; }

    bool wasStateRestored() const noexcept { return stateRestored.load (std::memory_order_relaxed); }
    bool wasDefaultPresetApplied() const noexcept { return defaultPresetApplied.load (std::memory_order_relaxed); }
    void markDefaultPresetApplied() noexcept { defaultPresetApplied.store (true, std::memory_order_relaxed); }

    const juce::String& getLastPresetName() const noexcept { return lastPresetName; }
    void setLastPresetName (const juce::String& n) { lastPresetName = n; }

   #if VESPERA_TEST_BUILD
    // Offline tests compare two instances (e.g. the UP and REVERSE contours), so
    // they need the generator to start from the same random state.
    void seedRandomForTests (juce::int64 seed) { rng.setSeed (seed); }
   #endif

    juce::AudioProcessorValueTreeState apvts;

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct RawParams
    {
        std::atomic<float>* key      = nullptr;
        std::atomic<float>* scale    = nullptr;
        std::atomic<float>* poolSize = nullptr;
        std::atomic<float>* latch    = nullptr;
        std::atomic<float>* monitor  = nullptr;
        std::atomic<float>* keyLock  = nullptr;

        std::atomic<float>* rate    = nullptr;
        std::atomic<float>* steps   = nullptr;
        std::atomic<float>* density = nullptr;
        std::atomic<float>* octaves = nullptr;
        std::atomic<float>* contour = nullptr;
        std::atomic<float>* mutate  = nullptr;
        std::atomic<float>* swing   = nullptr;
        std::atomic<float>* gate    = nullptr;
        std::atomic<float>* accent  = nullptr;
        std::atomic<float>* transpose = nullptr;

        std::atomic<float>* wave    = nullptr;
        std::atomic<float>* attack  = nullptr;
        std::atomic<float>* decay   = nullptr;
        std::atomic<float>* sustain = nullptr;
        std::atomic<float>* release = nullptr;
        std::atomic<float>* detune  = nullptr;

        std::atomic<float>* filterType   = nullptr;
        std::atomic<float>* filterCutoff = nullptr;
        std::atomic<float>* filterReso   = nullptr;
        std::atomic<float>* filterEnv    = nullptr;
        std::atomic<float>* lfoRate      = nullptr;
        std::atomic<float>* lfoRateHz    = nullptr;
        std::atomic<float>* lfoSync      = nullptr;
        std::atomic<float>* lfoShape     = nullptr;
        std::atomic<float>* motion       = nullptr;

        std::atomic<float>* drive = nullptr;
        std::atomic<float>* crush = nullptr;
        std::atomic<float>* noise = nullptr;
        std::atomic<float>* wow   = nullptr;
        std::atomic<float>* fold  = nullptr;
        std::atomic<float>* ring  = nullptr;

        std::atomic<float>* output = nullptr;
        std::atomic<float>* limiter = nullptr;
        std::atomic<float>* chords = nullptr;
        std::atomic<float>* transportSync = nullptr;
    };

    void bindRawParams();
    RawParams rawParams;

    double triggerStep (double samplesPerStep, float gate, float swing);
    void updatePatternView (const int* poolNotes, int poolSize);
    vespera::PatternParams currentPatternParams() const;
    static double lfoRateInBeats (int rateIndex);

    double currentSampleRate = 48000.0;
    double currentBpm = 120.0;

    vespera::NotePool      notePool;
    vespera::PatternEngine pattern;
    vespera::PolySynth     synth;
    vespera::ResonantFilter filter;
    vespera::Character      character;
    vespera::Lfo            lfo;
    vespera::Limiter        outputLimiter;

    // Seeded from the system random so two instances do not invent the same
    // pattern, while RANDOM still keeps its own deterministic feel.
    juce::Random rng { juce::Random::getSystemRandom().nextInt64() };

    double samplesToNextStep = 0.0;
    int    currentStep = 0;
    bool   patternDirty = true;
    int    lastKey = -1;
    int    lastScale = -1;
    int    lastPoolSizeParam = -1;
    float  lastDensity = -1.0f;
    int    lastOctaves = -1;
    int    lastContour = -1;
    float  lastAccent = -1.0f;
    std::atomic<bool> pendingPoolClear { false };

    // RANDOM publishes the pool it wants; the audio thread applies it at the top
    // of the next block.  The editor also shows it immediately, so the pool is
    // right even while the host is not processing audio.
    juce::SpinLock seedLock;
    int  pendingSeedNotes[vespera::NotePool::kMaxNotes] {};
    std::atomic<int> pendingSeedCount { 0 };

    void buildSeedNotes (int key, int scaleIndex, int poolSize, int* dest, int& count) const;
    void publishSeed (const int* notes, int count);
    bool applyPendingSeed (int poolSize);

    // Notes currently sounding through MONITOR, so a key change between note-on
    // and note-off cannot leave a voice hanging on the old pitch.  The raw MIDI
    // note is remembered next to the pitch that was actually played.
    struct MonitorNote { int raw = -1; int played = -1; };
    std::array<MonitorNote, 16> monitorNotes {};
    int monitorNoteCount = 0;
    void monitorNoteOn (int rawNote, int playedNote, float velocity);
    void monitorNoteOff (int rawNote);
    void monitorAllNotesOff();

    vespera::PatternView view;
    mutable juce::SpinLock viewLock;

    double lfoPhase = 0.0;
    vespera::DebugLogger debugLogger;
    double logTimeSec = 0.0;

    juce::AudioBuffer<float> outBuffer;

    juce::SmoothedValue<float> outputSmoothed;

    std::atomic<int>   displayVoices { 0 };
    std::atomic<float> displayBpm    { 120.0f };
    std::atomic<bool>  displayRunning { true };

    bool lastSequencerRunning = true;

    std::atomic<bool> stateRestored { false };
    std::atomic<bool> defaultPresetApplied { false };

    juce::String lastPresetName;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VesperaAudioProcessor)
};
