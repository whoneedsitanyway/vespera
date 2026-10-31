#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <algorithm>

//==============================================================================
namespace
{
    using APVTS = juce::AudioProcessorValueTreeState;

    float peakMagnitude (const juce::AudioBuffer<float>& b, int numSamples) noexcept
    {
        float p = 0.0f;
        const int chans = juce::jmin (2, b.getNumChannels());
        for (int ch = 0; ch < chans; ++ch)
        {
            const float* d = b.getReadPointer (ch);
            for (int n = 0; n < numSamples; ++n)
                p = juce::jmax (p, std::abs (d[n]));
        }
        return p;
    }
}

//==============================================================================
VesperaAudioProcessor::VesperaAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    bindRawParams();
}

VesperaAudioProcessor::~VesperaAudioProcessor() = default;

//==============================================================================
void VesperaAudioProcessor::bindRawParams()
{
    using namespace vespera;
    auto bind = [this] (const char* id) { return apvts.getRawParameterValue (id); };

    rawParams.key      = bind (ids::key);
    rawParams.scale    = bind (ids::scale);
    rawParams.poolSize = bind (ids::poolSize);
    rawParams.latch    = bind (ids::latch);
    rawParams.monitor  = bind (ids::monitor);
    rawParams.keyLock  = bind (ids::keyLock);

    rawParams.rate    = bind (ids::rate);
    rawParams.steps   = bind (ids::steps);
    rawParams.density = bind (ids::density);
    rawParams.octaves = bind (ids::octaves);
    rawParams.contour = bind (ids::contour);
    rawParams.mutate  = bind (ids::mutate);
    rawParams.swing   = bind (ids::swing);
    rawParams.gate    = bind (ids::gate);
    rawParams.accent  = bind (ids::accent);
    rawParams.transpose = bind (ids::transpose);

    rawParams.wave    = bind (ids::wave);
    rawParams.attack  = bind (ids::attack);
    rawParams.decay   = bind (ids::decay);
    rawParams.sustain = bind (ids::sustain);
    rawParams.release = bind (ids::release);
    rawParams.detune  = bind (ids::detune);

    rawParams.filterType   = bind (ids::filterType);
    rawParams.filterCutoff = bind (ids::filterCutoff);
    rawParams.filterReso   = bind (ids::filterReso);
    rawParams.filterEnv    = bind (ids::filterEnv);
    rawParams.lfoRate      = bind (ids::lfoRate);
    rawParams.lfoRateHz    = bind (ids::lfoRateHz);
    rawParams.lfoSync      = bind (ids::lfoSync);
    rawParams.lfoShape     = bind (ids::lfoShape);
    rawParams.motion       = bind (ids::motion);

    rawParams.drive = bind (ids::drive);
    rawParams.crush = bind (ids::crush);
    rawParams.noise = bind (ids::noise);
    rawParams.wow   = bind (ids::wow);
    rawParams.fold  = bind (ids::fold);
    rawParams.ring  = bind (ids::ring);

    rawParams.output  = bind (ids::output);
    rawParams.limiter = bind (ids::limiter);
    rawParams.chords  = bind (ids::chords);
    rawParams.transportSync = bind (ids::transportSync);
}

//==============================================================================
APVTS::ParameterLayout VesperaAudioProcessor::createParameterLayout()
{
    using namespace vespera;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    auto addFloat = [&params] (const char* id, const juce::String& name,
                               float min, float max, float def,
                               const juce::String& suffix, int decimals,
                               float skewCentre = 0.0f)
    {
        juce::NormalisableRange<float> range (min, max);
        if (skewCentre > 0.0f)
            range.setSkewForCentre (skewCentre);

        auto attrs = juce::AudioParameterFloatAttributes()
                         .withLabel (suffix)
                         .withStringFromValueFunction ([decimals] (float v, int)
                         {
                             auto s = juce::String (v, decimals);
                             if (s.containsChar ('.'))
                             {
                                 s = s.trimCharactersAtEnd ("0");
                                 s = s.trimCharactersAtEnd (".");
                             }
                             return s;
                         });

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            id, name, range, def, attrs));
    };

    auto addInt = [&params] (const char* id, const juce::String& name,
                             int min, int max, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            id, name, min, max, def));
    };

    auto addBool = [&params] (const char* id, const juce::String& name, bool def)
    {
        params.push_back (std::make_unique<juce::AudioParameterBool> (id, name, def));
    };

    auto addChoice = [&params] (const char* id, const juce::String& name,
                                const juce::StringArray& choices, int def)
    {
        params.push_back (std::make_unique<juce::AudioParameterChoice> (id, name, choices, def));
    };

    // --- Notes ---------------------------------------------------------------
    addChoice (ids::key,   "Key",   Scale::getKeyNames(), 0);
    addChoice (ids::scale, "Scale", Scale::getNames(), 1);
    addInt    (ids::poolSize, "Pool", 1, 8, 4);
    addBool   (ids::latch,   "Latch",   false);
    addBool   (ids::monitor, "Monitor", false);
    addBool   (ids::keyLock, "Lock Key", false);

    // --- Pattern -------------------------------------------------------------
    addChoice (ids::rate,    "Rate", getRateNames(), 4);
    addInt    (ids::steps,   "Steps", 2, 16, 8);
    addFloat  (ids::density, "Density", 0.0f, 1.0f, 0.7f, "", 2);
    addInt    (ids::octaves, "Octaves", 1, 3, 2);
    addChoice (ids::contour, "Contour", getContourNames(), 2);
    addFloat  (ids::mutate,  "Mutate", 0.0f, 1.0f, 0.0f, "", 2);
    addFloat  (ids::swing,   "Swing",  0.0f, 1.0f, 0.0f, "", 2);
    addFloat  (ids::gate,    "Gate",   0.05f, 1.0f, 0.5f, "", 2);
    addFloat  (ids::accent,  "Accent", 0.0f, 1.0f, 0.3f, "", 2);

    {
        auto attrs = juce::AudioParameterIntAttributes()
                         .withStringFromValueFunction ([] (int v, int)
                         {
                             return (v > 0 ? "+" : "") + juce::String (v);
                         });

        params.push_back (std::make_unique<juce::AudioParameterInt> (
            ids::transpose, "Transpose", -7, 7, 0, attrs));
    }

    // --- Voice ---------------------------------------------------------------
    addChoice (ids::wave,    "Wave", { "Sine", "Triangle", "Saw", "Square" }, 1);
    addFloat  (ids::attack,  "Attack",  0.001f, 4.0f, 0.005f, "s", 3, 0.05f);
    addFloat  (ids::decay,   "Decay",   0.001f, 4.0f, 0.25f, "s", 3, 0.3f);
    addFloat  (ids::sustain, "Sustain", 0.0f, 1.0f, 0.6f, "", 2);
    addFloat  (ids::release, "Release", 0.01f, 8.0f, 0.4f, "s", 3, 0.5f);
    addFloat  (ids::detune,  "Detune",  0.0f, 1.0f, 0.15f, "", 2);

    // --- Filter --------------------------------------------------------------
    addChoice (ids::filterType,   "Filter", { "Off", "LP", "BP", "HP" }, 1);
    addFloat  (ids::filterCutoff, "Cutoff", 30.0f, 18000.0f, 8000.0f, "Hz", 0, 1200.0f);
    addFloat  (ids::filterReso,   "Reso",   0.0f, 1.0f, 0.15f, "", 2);
    addFloat  (ids::filterEnv,    "Env",    0.0f, 1.0f, 0.2f, "", 2);
    addChoice (ids::lfoShape,     "LFO Shape", Lfo::getNames(), 0);
    addBool   (ids::lfoSync,      "LFO Sync",  false);
    addChoice (ids::lfoRate,      "Division",  { "4/1", "2/1", "1/1", "1/2",
                                                 "1/4", "1/8", "1/16", "1/32" }, 2);
    addFloat  (ids::lfoRateHz,    "Rate", 0.02f, 20.0f, 1.0f, "Hz", 2, 1.0f);
    addFloat  (ids::motion,       "Motion", 0.0f, 1.0f, 0.1f, "", 2);

    // --- Character -----------------------------------------------------------
    addFloat (ids::drive, "Drive", 0.0f, 1.0f, 0.0f, "", 2);
    addFloat (ids::crush, "Crush", 0.0f, 1.0f, 0.0f, "", 2);
    addFloat (ids::noise, "Noise", 0.0f, 1.0f, 0.0f, "", 2);
    addFloat (ids::wow,   "Wow",   0.0f, 1.0f, 0.0f, "", 2);
    addFloat (ids::fold,  "Fold",  0.0f, 1.0f, 0.0f, "", 2);
    addFloat (ids::ring,  "Ring",  0.0f, 1.0f, 0.0f, "", 2);

    // --- Output --------------------------------------------------------------
    addFloat (ids::output, "Output", -24.0f, 12.0f, 0.0f, "dB", 1);
    addBool  (ids::limiter, "Limiter", true);

    // Every step plays a chord from the selected scale instead of a single note.
    addBool  (ids::chords, "Chords", false);

    // Follows the host transport: the generator only runs while the DAW plays.
    // Hosts (and the standalone app) that report no transport keep free-running.
    addBool  (ids::transportSync, "Play Sync", true);

    return { params.begin(), params.end() };
}

//==============================================================================
double VesperaAudioProcessor::lfoRateInBeats (int rateIndex)
{
    static constexpr double beats[] = { 16.0, 8.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125 };
    return beats[juce::jlimit (0, 7, rateIndex)];
}

//==============================================================================
void VesperaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    setLatencySamples (0);

    synth.prepare (sampleRate);
    filter.prepare (sampleRate);
    character.prepare (sampleRate);
    outputLimiter.prepare (sampleRate);

    outBuffer.setSize (2, samplesPerBlock, false, false, true);

    notePool.reset();
    pattern.reset();
    currentStep = 0;
    samplesToNextStep = 0.0;
    patternDirty = true;
    lastKey = -1;
    lastScale = -1;
    lastPoolSizeParam = -1;
    lastDensity = -1.0f;
    lastOctaves = -1;
    lastContour = -1;
    lastAccent = -1.0f;
    pendingPoolClear.store (false, std::memory_order_relaxed);
    lastSequencerRunning = true;
    displayRunning.store (true, std::memory_order_relaxed);

    {
        const juce::SpinLock::ScopedLockType lock (seedLock);
        pendingSeedCount.store (0, std::memory_order_relaxed);
    }

    monitorNoteCount = 0;

    {
        const juce::SpinLock::ScopedLockType lock (viewLock);
        view = {};
        view.steps = pattern.getLength();
    }

    outputSmoothed.reset (sampleRate, 0.02);
    outputSmoothed.setCurrentAndTargetValue (
        juce::Decibels::decibelsToGain (rawParams.output->load()));

    lfo.reset();
    logTimeSec = 0.0;
    debugLogger.start();
}

void VesperaAudioProcessor::releaseResources()
{
    debugLogger.stop();
}

bool VesperaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

//==============================================================================
void VesperaAudioProcessor::monitorNoteOn (int rawNote, int playedNote, float velocity)
{
    // A repeated note-on for a key that is already held restarts the note.
    // Release the old voice first, otherwise the matching note-off would only
    // stop one of the two and leave the other hanging.
    monitorNoteOff (rawNote);

    if (monitorNoteCount >= (int) monitorNotes.size())
        monitorNoteOff (monitorNotes[0].raw);   // drop the oldest, never grow

    monitorNotes[(size_t) monitorNoteCount++] = { rawNote, playedNote };
    synth.noteOn (playedNote, velocity);
}

void VesperaAudioProcessor::monitorNoteOff (int rawNote)
{
    for (int i = 0; i < monitorNoteCount; ++i)
    {
        if (monitorNotes[(size_t) i].raw != rawNote)
            continue;

        synth.noteOff (monitorNotes[(size_t) i].played);

        for (int j = i + 1; j < monitorNoteCount; ++j)
            monitorNotes[(size_t) (j - 1)] = monitorNotes[(size_t) j];

        --monitorNoteCount;
        return;
    }
}

void VesperaAudioProcessor::monitorAllNotesOff()
{
    // All Notes Off silences the whole instrument (pattern voices included),
    // so there is nothing left to track.
    monitorNoteCount = 0;
    synth.allNotesOff();
}

//==============================================================================
//  Picks a fresh pool from the current scale: the tonic plus random degrees,
//  spread over two octaves, so every RANDOM press gives a different but always
//  in-key set of notes.  Runs on the message thread.
void VesperaAudioProcessor::buildSeedNotes (int key, int scaleIndex, int poolSize,
                                            int* dest, int& count) const
{
    using namespace vespera;

    const auto& intervals = Scale::getIntervals (scaleIndex);
    const int degrees = (int) intervals.size();
    const int tonic = 48 + juce::jlimit (0, 11, key);

    constexpr int kMaxCandidates = NotePool::kMaxNotes * 2;
    int candidates[kMaxCandidates] {};
    int n = 0;

    // The tonic is always present, so the pool has a tonal centre.
    candidates[n++] = tonic;

    for (int octave = 0; octave < 2; ++octave)
        for (int d = 1; d < degrees && n < kMaxCandidates; ++d)
            candidates[n++] = tonic + octave * 12 + intervals[(size_t) d];

    // Shuffle everything after the tonic: the tonic stays first, the rest is
    // random, so repeated presses do not produce the same chord.
    auto& random = juce::Random::getSystemRandom();
    for (int i = n - 1; i > 1; --i)
    {
        const int j = 1 + random.nextInt (i);
        std::swap (candidates[i], candidates[j]);
    }

    count = juce::jlimit (1, juce::jmax (1, juce::jmin (poolSize, n)), poolSize);

    for (int i = 0; i < count; ++i)
        dest[i] = juce::jlimit (0, 127, candidates[i]);

    std::sort (dest, dest + count);
}

void VesperaAudioProcessor::publishSeed (const int* notes, int count)
{
    const juce::SpinLock::ScopedLockType lock (seedLock);

    count = juce::jlimit (0, vespera::NotePool::kMaxNotes, count);

    for (int i = 0; i < count; ++i)
        pendingSeedNotes[i] = notes[i];

    pendingSeedCount.store (count, std::memory_order_release);
}

bool VesperaAudioProcessor::applyPendingSeed (int poolSize)
{
    if (pendingSeedCount.load (std::memory_order_acquire) <= 0)
        return false;

    // The message thread only holds the lock for a few writes; if it is busy,
    // try again on the next block instead of blocking the audio thread.
    const juce::SpinLock::ScopedTryLockType lock (seedLock);
    if (! lock.isLocked())
        return false;

    const int count = juce::jlimit (0, vespera::NotePool::kMaxNotes,
                                    pendingSeedCount.load (std::memory_order_relaxed));
    pendingSeedCount.store (0, std::memory_order_relaxed);

    const bool wasLatched = notePool.isLatched();
    notePool.setLatch (false);
    notePool.reset();

    for (int i = 0; i < count; ++i)
        notePool.add (pendingSeedNotes[i], poolSize);

    notePool.setLatch (wasLatched);

    patternDirty = true;
    return true;
}

//==============================================================================
vespera::PatternParams VesperaAudioProcessor::currentPatternParams() const
{
    vespera::PatternParams pp;
    pp.steps   = (int) rawParams.steps->load();
    pp.density = rawParams.density->load();
    pp.octaves = (int) rawParams.octaves->load();
    pp.contour = (int) rawParams.contour->load();
    pp.mutate  = rawParams.mutate->load();
    pp.accent  = rawParams.accent->load();
    pp.key     = (int) rawParams.key->load();
    pp.scale   = (int) rawParams.scale->load();
    return pp;
}

//==============================================================================
void VesperaAudioProcessor::ensurePattern (const int* poolNotes, int n,
                                           const vespera::PatternParams& pp)
{
    // Rebuild the pattern immediately when a generator control moves, so the
    // knobs feel live instead of waiting for the next mutation cycle.
    const int poolSizeParam = (int) rawParams.poolSize->load();

    if (std::abs (pp.density - lastDensity) > 1.0e-6f
        || pp.octaves != lastOctaves
        || pp.contour != lastContour
        || std::abs (pp.accent - lastAccent) > 1.0e-6f
        || poolSizeParam != lastPoolSizeParam)
    {
        lastDensity = pp.density;
        lastOctaves = pp.octaves;
        lastContour = pp.contour;
        lastAccent = pp.accent;
        lastPoolSizeParam = poolSizeParam;
        patternDirty = true;
    }

    if (patternDirty || pattern.getLength() != pp.steps)
    {
        pattern.regenerate (poolNotes, n, pp, rng);
        patternDirty = false;
        if (currentStep >= pattern.getLength())
            currentStep = 0;
    }
}

//==============================================================================
double VesperaAudioProcessor::triggerStep (double samplesPerStep, float gate, float swing)
{
    using namespace vespera;

    const PatternParams pp = currentPatternParams();

    int poolNotes[NotePool::kMaxNotes] {};
    const int poolSize = (int) rawParams.poolSize->load();
    const int n = notePool.getSorted (poolNotes, poolSize);

    ensurePattern (poolNotes, n, pp);

    const int triggeredStep = currentStep;
    const int transpose = (int) rawParams.transpose->load();
    const int key = pp.key;
    const int scale = pp.scale;
    const bool chordsOn = rawParams.chords->load() > 0.5f;

    if (n > 0)
    {
        const auto& step = pattern.getStep (triggeredStep);
        if (step.active)
        {
            const int baseNote = poolNotes[juce::jlimit (0, n - 1, step.poolIndex)]
                                     + 12 * step.octave;
            const int root = juce::jlimit (0, 127,
                                           Scale::transposeDegrees (baseNote, transpose, key, scale));
            const int duration = juce::jmax (1, (int) (samplesPerStep * gate));

            if (chordsOn)
            {
                // The chord is built from scale degrees, so every note stays in
                // key.  Equal-power scaling keeps a triad from jumping ~10 dB
                // louder than the single-note pattern.
                const auto chord = buildChord (root, key, scale,
                                               step.chordMask, step.chordInversion);
                const float voiceGain = 1.0f / std::sqrt ((float) juce::jmax (1, chord.count));

                for (int i = 0; i < chord.count; ++i)
                    synth.noteOnTimed (chord.notes[i], step.velocity * voiceGain, duration);
            }
            else
            {
                synth.noteOnTimed (root, step.velocity, duration);
            }
        }
    }

    updatePatternView (poolNotes, n);

    currentStep = (currentStep + 1) % pattern.getLength();

    if (currentStep == 0 && pp.mutate > 0.001f)
        pattern.mutate (poolNotes, n, pp, rng);

    const double factor = (triggeredStep % 2 == 1) ? (1.0 + swing * 0.5)
                                                   : (1.0 - swing * 0.5);
    return juce::jmax (1.0, samplesPerStep * factor);
}

//==============================================================================
void VesperaAudioProcessor::updatePatternView (const int* poolNotes, int poolSize)
{
    vespera::PatternView v;
    v.steps = pattern.getLength();
    v.currentStep = currentStep;
    v.poolSize = juce::jlimit (0, vespera::NotePool::kMaxNotes, poolSize);
    v.key = (int) rawParams.key->load();
    v.scale = (int) rawParams.scale->load();
    v.transpose = (int) rawParams.transpose->load();
    v.contour = (int) rawParams.contour->load();
    v.rate = (int) rawParams.rate->load();
    v.running = isSequencerRunning();
    v.density = rawParams.density->load();
    v.mutate = rawParams.mutate->load();

    const int key = v.key;
    const int scale = v.scale;
    for (int i = 0; i < vespera::NotePool::kMaxNotes; ++i)
        v.poolNotes[i] = (i < v.poolSize) ? poolNotes[i] : 0;

    for (int i = 0; i < vespera::PatternEngine::kMaxSteps; ++i)
    {
        const auto& s = pattern.getStep (i);
        v.active[i] = (i < v.steps) && s.active;
        v.velocity[i] = s.velocity;
        v.chordMask[i] = s.chordMask;
        v.chordInversion[i] = s.chordInversion;
        v.note[i] = (v.active[i] && v.poolSize > 0)
                        ? juce::jlimit (0, 127,
                                        vespera::Scale::transposeDegrees (
                                            poolNotes[juce::jlimit (0, v.poolSize - 1, s.poolIndex)]
                                                + 12 * s.octave,
                                            v.transpose, key, scale))
                        : -1;
    }

    const juce::SpinLock::ScopedLockType lock (viewLock);
    view = v;
}

bool VesperaAudioProcessor::getPatternView (vespera::PatternView& out) const
{
    const juce::SpinLock::ScopedTryLockType lock (viewLock);
    if (! lock.isLocked())
        return false;

    out = view;
    return true;
}

//==============================================================================
void VesperaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = buffer.getNumChannels();

    if (numSamples == 0)
        return;

    using namespace vespera;
    auto& rp = rawParams;

    // --- transport -----------------------------------------------------------
    // The host may or may not report a transport at all (the standalone app has
    // no playhead).  Only when it does can we follow its play/stop state.
    double bpm = currentBpm;
    bool transportKnown = false;
    bool transportPlaying = false;

    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            transportKnown = true;
            transportPlaying = pos->getIsPlaying();

            if (auto hostBpm = pos->getBpm())
                bpm = *hostBpm;
        }
    }

    currentBpm = juce::jlimit (20.0, 300.0, bpm);
    displayBpm.store ((float) currentBpm, std::memory_order_relaxed);

    // Play Sync: run the generator only while the DAW is playing.  A host that
    // reports no transport (standalone) always free-runs, so the instrument is
    // never mysteriously silent.
    const bool playSync = rp.transportSync->load() > 0.5f;
    const bool sequencerRunning = ! (playSync && transportKnown) || transportPlaying;

    if (sequencerRunning != lastSequencerRunning)
    {
        // Restart the figure from its first step so every take begins the same
        // way, and drop the pending step so playback starts on the downbeat.
        lastSequencerRunning = sequencerRunning;
        currentStep = 0;
        samplesToNextStep = 0.0;

        // Publish the new state so the display can show "waiting for transport".
        int notes[NotePool::kMaxNotes] {};
        const int n = notePool.getSorted (notes, (int) rp.poolSize->load());
        updatePatternView (notes, n);
    }

    displayRunning.store (sequencerRunning, std::memory_order_relaxed);

    // --- parameter snapshots -------------------------------------------------
    const int keyIndex   = (int) rp.key->load();
    const int scaleIndex = (int) rp.scale->load();
    const int poolSize   = (int) rp.poolSize->load();
    const bool latchOn   = rp.latch->load() > 0.5f;
    const bool monitorOn = rp.monitor->load() > 0.5f;

    notePool.setLatch (latchOn);

    // --- pool updates requested by the editor --------------------------------
    bool poolChanged = false;

    // The UI clear button empties the pool on the audio thread.
    if (pendingPoolClear.exchange (false, std::memory_order_relaxed))
    {
        notePool.reset();
        pattern.reset();
        currentStep = 0;
        patternDirty = true;
        updatePatternView (nullptr, 0);
    }

    // Changing the key or scale re-quantises the notes already in the pool so
    // the generator never keeps playing pitches from the previous scale.
    if (keyIndex != lastKey || scaleIndex != lastScale)
    {
        int notes[NotePool::kMaxNotes] {};
        const int n = notePool.getSorted (notes, poolSize);

        const bool wasLatched = notePool.isLatched();
        notePool.setLatch (false);
        notePool.reset();

        for (int i = 0; i < n; ++i)
            notePool.add (Scale::quantize (notes[i], keyIndex, scaleIndex), poolSize);

        notePool.setLatch (wasLatched);

        lastKey = keyIndex;
        lastScale = scaleIndex;
        patternDirty = true;
        poolChanged = true;
    }

    // A RANDOM press asks for a brand new pool.  Applied after the re-quantise,
    // so the pool ends up exactly the notes RANDOM chose from the new scale.
    // The pattern is rebuilt straight away as well: otherwise the old notes
    // would keep sounding (and be shown) until the next step boundary.
    if (applyPendingSeed (poolSize))
    {
        lastKey = keyIndex;
        lastScale = scaleIndex;
        currentStep = 0;
        poolChanged = true;
    }

    for (const auto metadata : midi)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const int note = Scale::quantize (msg.getNoteNumber(), keyIndex, scaleIndex);
            if (notePool.add (note, poolSize))
                poolChanged = true;

            if (monitorOn)
                monitorNoteOn (msg.getNoteNumber(), note, msg.getFloatVelocity());
        }
        else if (msg.isNoteOff())
        {
            // MONITOR tracks the quantised pitch, so a key change while a note is
            // held still finds the voice it started.
            monitorNoteOff (msg.getNoteNumber());
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            monitorAllNotesOff();
        }
    }

    if (poolChanged)
    {
        patternDirty = true;

        int notes[NotePool::kMaxNotes] {};
        const int n = notePool.getSorted (notes, poolSize);

        // Rebuild right here as well: while the transport is stopped the
        // generator never reaches triggerStep(), and the display would show the
        // previous pattern over the new pool.
        ensurePattern (notes, n, currentPatternParams());
        updatePatternView (notes, n);
    }

    // --- synth + pattern -----------------------------------------------------
    // The buffer is sized once in prepareToPlay; only grow it if the host asks
    // for a bigger block than advertised, so the audio thread never reallocates.
    if (outBuffer.getNumSamples() < numSamples)
        outBuffer.setSize (2, numSamples, false, false, true);

    outBuffer.clear (0, numSamples);

    synth.setParams ((int) rp.wave->load(), rp.attack->load(), rp.decay->load(),
                     rp.sustain->load(), rp.release->load(), rp.detune->load());

    const double stepBeats = rateInBeats ((int) rp.rate->load());
    const double samplesPerStep = juce::jmax (1.0, (60.0 / currentBpm)
                                                     * currentSampleRate * stepBeats);
    const float gate = rp.gate->load();
    const float swing = rp.swing->load();

    int pos = 0;

    if (! sequencerRunning)
    {
        // Transport is stopped: keep rendering so MONITOR notes and release
        // tails still sound, but never advance the pattern.
        synth.render (outBuffer.getWritePointer (0),
                      outBuffer.getWritePointer (1), numSamples);
        pos = numSamples;
    }

    while (pos < numSamples)
    {
        if (samplesToNextStep <= 0.0)
            samplesToNextStep += triggerStep (samplesPerStep, gate, swing);

        int chunk = numSamples - pos;
        const int untilStep = (int) std::ceil (juce::jmax (0.0, samplesToNextStep));
        if (untilStep > 0)
            chunk = juce::jmin (chunk, untilStep);
        chunk = juce::jmax (1, chunk);

        synth.render (outBuffer.getWritePointer (0) + pos,
                      outBuffer.getWritePointer (1) + pos, chunk);

        pos += chunk;
        samplesToNextStep -= (double) chunk;
    }

    const int activeVoices = synth.getActiveVoiceCount();
    displayVoices.store (activeVoices, std::memory_order_relaxed);

    const float synthPeak = peakMagnitude (outBuffer, numSamples);

    // --- resonant filter -----------------------------------------------------
    // The LFO can run free (Hz) or locked to the host tempo (1/1 .. 1/32), and
    // its shape can be a sine, a ramp, a square or a random source.
    const bool lfoSynced = rp.lfoSync->load() > 0.5f;
    double lfoHz = (double) rp.lfoRateHz->load();

    if (lfoSynced)
    {
        const double lfoBeats = lfoRateInBeats ((int) rp.lfoRate->load());
        lfoHz = 1.0 / (lfoBeats * 60.0 / currentBpm);
    }

    lfoHz = juce::jlimit (0.01, 40.0, lfoHz);

    const float lfoValue = lfo.advance (lfoHz * (double) numSamples / currentSampleRate,
                                        (int) rp.lfoShape->load(), rng);
    const float lfoOctaves = rp.motion->load() * 2.0f * lfoValue;

    filter.process (outBuffer.getWritePointer (0), outBuffer.getWritePointer (1),
                    numSamples, (int) rp.filterType->load(), rp.filterCutoff->load(),
                    rp.filterReso->load(), rp.filterEnv->load(), lfoOctaves);

    // --- character -----------------------------------------------------------
    CharacterParams cp;
    cp.drive = rp.drive->load();
    cp.crush = rp.crush->load();
    cp.noise = rp.noise->load();
    cp.wow   = rp.wow->load();
    cp.fold  = rp.fold->load();
    cp.ring  = rp.ring->load();

    character.process (outBuffer.getWritePointer (0), outBuffer.getWritePointer (1),
                       numSamples, cp);

    const float charPeak = peakMagnitude (outBuffer, numSamples);

    // --- output level --------------------------------------------------------
    outputSmoothed.setTargetValue (juce::Decibels::decibelsToGain (rp.output->load()));
    {
        float* oL = outBuffer.getWritePointer (0);
        float* oR = outBuffer.getWritePointer (1);
        for (int n = 0; n < numSamples; ++n)
        {
            const float g = outputSmoothed.getNextValue();
            oL[n] *= g;
            oR[n] *= g;
        }
    }

    // --- output stage: an optional final peak limiter ------------------------
    // With Limiter off the stage is bypassed; the safety clamp below still keeps
    // the output bounded.  With it on it is the last musical stage and holds the
    // signal just under full scale (soft ceiling for the attack overshoot).
    const bool limiterOn = rp.limiter->load() > 0.5f;
    float grDb = 0.0f;

    if (limiterOn)
    {
        outputLimiter.process (outBuffer.getWritePointer (0), outBuffer.getWritePointer (1),
                               numSamples, 0.98f);
        grDb = outputLimiter.getGainReductionDb();
    }
    else
    {
        outputLimiter.reset();
    }

    // --- final safety: finite AND bounded ------------------------------------
    // The limiter (when enabled) already keeps the output within +/-1.0.  This
    // last stage catches anything else: non-finite values are zeroed, while a
    // finite over-range sample (e.g. with the limiter bypassed) is clamped to
    // +/-1.0 rather than zeroed, so it degrades gracefully instead of gating.
    constexpr float kSafetyCeil = 1.0f;
    float rawOutPeak = 0.0f;
    float outPeak    = 0.0f;
    bool  bad        = false;
    {
        float* oL = outBuffer.getWritePointer (0);
        float* oR = outBuffer.getWritePointer (1);
        auto sanitise = [&] (float& s)
        {
            if (! std::isfinite (s))
            {
                bad = true;
                s = 0.0f;
            }
            else
            {
                rawOutPeak = juce::jmax (rawOutPeak, std::abs (s));
                s = juce::jlimit (-kSafetyCeil, kSafetyCeil, s);
            }
            outPeak = juce::jmax (outPeak, std::abs (s));
        };
        for (int n = 0; n < numSamples; ++n)
        {
            sanitise (oL[n]);
            sanitise (oR[n]);
        }
    }

    const bool runaway = bad || charPeak > 32.0f;

    // --- black box -----------------------------------------------------------
    {
        BlockSnapshot snap;
        snap.timeSec    = logTimeSec;
        snap.inPeak     = synthPeak;
        snap.charPeak   = charPeak;
        snap.outPeak    = outPeak;

        snap.key = keyIndex;
        snap.scale = scaleIndex;
        snap.poolSize = poolSize;
        snap.steps = (int) rp.steps->load();
        snap.density = rp.density->load();
        snap.octaves = (int) rp.octaves->load();
        snap.contour = (int) rp.contour->load();
        snap.mutate = rp.mutate->load();
        snap.swing = rp.swing->load();
        snap.gate = rp.gate->load();
        snap.accent = rp.accent->load();
        snap.transpose = (int) rp.transpose->load();

        snap.wave = (int) rp.wave->load();
        snap.attack = rp.attack->load();
        snap.decay = rp.decay->load();
        snap.sustain = rp.sustain->load();
        snap.release = rp.release->load();
        snap.detune = rp.detune->load();

        snap.filterType = (int) rp.filterType->load();
        snap.filterCutoff = rp.filterCutoff->load();
        snap.filterReso = rp.filterReso->load();
        snap.filterEnv = rp.filterEnv->load();
        snap.lfoRate = (float) lfoHz;
        snap.lfoShape = (int) rp.lfoShape->load();
        snap.lfoSynced = lfoSynced;
        snap.motion = rp.motion->load();

        snap.drive = cp.drive;
        snap.crush = cp.crush;
        snap.noise = cp.noise;
        snap.wow = cp.wow;
        snap.fold = cp.fold;
        snap.ring = cp.ring;

        snap.output = rp.output->load();
        snap.limiterOn = limiterOn;
        snap.grDb = grDb;
        snap.playSyncOn = playSync;
        snap.seqRunning = sequencerRunning;
        snap.bpm = (float) currentBpm;
        snap.activeVoices = activeVoices;
        snap.currentStep = currentStep;

        snap.latchOn = latchOn;
        snap.monitorOn = monitorOn;
        snap.keyLockOn = rp.keyLock->load() > 0.5f;

        if (bad)                    snap.flags |= kFlagNonFinite;
        if (synthPeak > 2.0f)       snap.flags |= kFlagInputHot;
        if (rawOutPeak > 1.0f)      snap.flags |= kFlagOutHot;
        if (grDb > 12.0f)           snap.flags |= kFlagHeavyGR;

        debugLogger.push (snap);
    }

    logTimeSec += (double) numSamples / currentSampleRate;

    if (runaway)
    {
        outBuffer.clear();
        synth.reset();
        filter.reset();
        character.reset();
    }

    // --- fold back into the host buffer --------------------------------------
    if (numCh >= 2)
    {
        buffer.copyFrom (0, 0, outBuffer, 0, 0, numSamples);
        buffer.copyFrom (1, 0, outBuffer, 1, 0, numSamples);
    }
    else if (numCh == 1)
    {
        for (int n = 0; n < numSamples; ++n)
            buffer.setSample (0, n, (outBuffer.getSample (0, n) + outBuffer.getSample (1, n)) * 0.5f);
    }
}

//==============================================================================
juce::AudioProcessorEditor* VesperaAudioProcessor::createEditor()
{
    return new VesperaAudioProcessorEditor (*this);
}

//==============================================================================
void VesperaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void VesperaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
        {
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
            stateRestored.store (true, std::memory_order_relaxed);
        }
}

//==============================================================================
void VesperaAudioProcessor::randomizeParameters()
{
    using namespace vespera;
    auto& random = juce::Random::getSystemRandom();

    auto setReal = [this] (const char* id, float realValue)
    {
        if (auto* p = apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    };

    auto pick = [&random] (std::initializer_list<float> options)
    {
        const int n = (int) options.size();
        const int idx = random.nextInt (juce::jmax (1, n));
        return *(options.begin() + juce::jlimit (0, n - 1, idx));
    };

    // Notes.  With KEY LOCK engaged the tonic and the scale are kept, so RANDOM
    // only reshapes the pattern and the sound.
    const bool keyLocked = rawParams.keyLock->load() > 0.5f;

    if (! keyLocked)
    {
        setReal (ids::key, (float) random.nextInt (12));
        setReal (ids::scale, pick ({ 0.0f, 1.0f, 2.0f, 4.0f, 5.0f, 7.0f, 8.0f }));
    }

    // Pattern.  MUTATE and TRANSPOSE are left alone on purpose: they are the
    // controls that let you keep a pattern you already like.
    setReal (ids::rate,    pick ({ 2.0f, 3.0f, 4.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f,
                                   9.0f, 10.0f, 11.0f }));
    setReal (ids::steps,   pick ({ 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 12.0f, 16.0f }));
    setReal (ids::density, 0.45f + random.nextFloat() * 0.5f);
    setReal (ids::octaves, pick ({ 1.0f, 2.0f, 2.0f, 3.0f }));
    setReal (ids::contour, pick ({ 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f }));
    setReal (ids::swing,   pick ({ 0.0f, 0.0f, 0.1f, 0.25f, 0.4f }));
    setReal (ids::gate,    0.25f + random.nextFloat() * 0.5f);
    setReal (ids::accent,  random.nextFloat() * 0.6f);

    // Filter.  The cutoff window depends on the type: a high-pass at 2 kHz (or a
    // band-pass at 18 kHz) simply removes the voice, which is what made RANDOM
    // land on silent settings.  Each type therefore gets its own musical range,
    // and the modulation depth is kept modest where a wide sweep would leave the
    // audible band.  The type is decided before the voice, because a sine has no
    // harmonics to survive a band- or high-pass.
    const float typeRoll = random.nextFloat();
    const int filterType = typeRoll < 0.55f ? 1        // LP
                         : typeRoll < 0.82f ? 2        // BP
                         : typeRoll < 0.93f ? 3        // HP
                                            : 0;       // Off

    float cutoffLo = 2000.0f, cutoffHi = 12000.0f;
    float maxReso = 0.45f, maxEnv = 0.5f, maxMotion = 0.4f;

    switch (filterType)
    {
        case 1:  cutoffLo = 700.0f;  cutoffHi = 8000.0f; break;   // LP
        case 2:  cutoffLo = 300.0f;  cutoffHi = 1500.0f;          // BP: around the
                 maxReso = 0.30f; maxEnv = 0.35f; maxMotion = 0.25f; break;  // notes
        case 3:  cutoffLo = 50.0f;   cutoffHi = 220.0f;           // HP: low cut
                 maxReso = 0.25f; maxEnv = 0.25f; maxMotion = 0.2f; break;
        default: break;                                           // Off
    }

    // Logarithmic spread so low cutoffs are not underrepresented.
    const float cutoff = cutoffLo * std::pow (cutoffHi / cutoffLo, random.nextFloat());

    // Voice.  A sine only survives the low-pass (and bypass), so the harmonic
    // shapes are forced whenever the filter is going to carve the spectrum.
    const bool needsHarmonics = filterType == 2 || filterType == 3;

    setReal (ids::wave, needsHarmonics ? pick ({ 1.0f, 2.0f, 2.0f, 3.0f })
                                       : pick ({ 0.0f, 1.0f, 1.0f, 2.0f, 3.0f }));
    setReal (ids::attack,  0.002f + random.nextFloat() * 0.06f);
    setReal (ids::decay,   0.05f + random.nextFloat() * 0.45f);
    setReal (ids::sustain, 0.3f + random.nextFloat() * 0.55f);
    setReal (ids::release, 0.1f + random.nextFloat() * 0.7f);
    setReal (ids::detune,  random.nextFloat() * 0.45f);

    setReal (ids::filterType,   (float) filterType);
    setReal (ids::filterCutoff, cutoff);
    setReal (ids::filterReso,   random.nextFloat() * maxReso);
    setReal (ids::filterEnv,    random.nextFloat() * maxEnv);
    setReal (ids::lfoShape,     pick ({ 0.0f, 0.0f, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f }));
    setReal (ids::lfoSync,      pick ({ 0.0f, 0.0f, 1.0f }));
    setReal (ids::lfoRate,      pick ({ 1.0f, 2.0f, 2.0f, 3.0f, 4.0f }));
    setReal (ids::lfoRateHz,    0.2f + random.nextFloat() * 4.0f);
    setReal (ids::motion,       random.nextFloat() * maxMotion);

    // Character.  Noise is never randomised: it is a deliberate colour, and
    // turning it on behind the user's back is never welcome.
    setReal (ids::drive, random.nextFloat() * 0.45f);
    setReal (ids::crush, random.nextFloat() * 0.3f);
    setReal (ids::wow,   random.nextFloat() * 0.35f);
    setReal (ids::fold,  random.nextFloat() < 0.5f ? 0.0f : random.nextFloat() * 0.35f);
    setReal (ids::ring,  random.nextFloat() < 0.6f ? 0.0f : random.nextFloat() * 0.3f);

    // Output.
    setReal (ids::output, 0.0f);

    // Seed the pool with random notes of the new key so RANDOM immediately plays
    // something musical.  The notes are published for the audio thread and also
    // shown right away, so the pool can never lag behind the key.
    const int finalKey = (int) rawParams.key->load();
    const int finalScale = (int) rawParams.scale->load();

    int seedNotes[NotePool::kMaxNotes] {};
    int seedCount = 0;
    buildSeedNotes (finalKey, finalScale, (int) rawParams.poolSize->load(),
                    seedNotes, seedCount);
    publishSeed (seedNotes, seedCount);

    {
        const juce::SpinLock::ScopedLockType lock (viewLock);
        view.key = finalKey;
        view.scale = finalScale;
        view.transpose = (int) rawParams.transpose->load();
        view.contour = (int) rawParams.contour->load();
        view.rate = (int) rawParams.rate->load();
        view.running = true;
        view.density = rawParams.density->load();
        view.mutate = rawParams.mutate->load();
        view.poolSize = seedCount;
        view.currentStep = 0;

        for (int i = 0; i < NotePool::kMaxNotes; ++i)
            view.poolNotes[i] = (i < seedCount) ? seedNotes[i] : 0;

        // The steps belong to the previous pattern; the audio thread rebuilds
        // them on the next block.  Until then show nothing rather than notes
        // from the old key.
        for (int i = 0; i < PatternEngine::kMaxSteps; ++i)
        {
            view.active[i] = false;
            view.note[i] = -1;
            view.velocity[i] = 0.0f;
        }
    }
}

//==============================================================================
void VesperaAudioProcessor::clearNotePool()
{
    pendingPoolClear.store (true, std::memory_order_relaxed);
}

//==============================================================================
bool VesperaAudioProcessor::writePatternMidiFile (const juce::File& destination) const
{
    using namespace vespera;

    PatternView v;
    if (! getPatternView (v) || v.poolSize <= 0 || v.steps <= 0)
        return false;

    constexpr int ticksPerQuarter = 480;

    const double beatsPerStep = rateInBeats ((int) rawParams.rate->load());
    const double stepTicks = juce::jmax (1.0, beatsPerStep * (double) ticksPerQuarter);
    const float  gate = rawParams.gate->load();
    const float  swing = rawParams.swing->load();
    const bool   chordsOn = rawParams.chords->load() > 0.5f;
    const double bpm = juce::jlimit (20.0f, 300.0f, displayBpm.load (std::memory_order_relaxed));

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (
                      juce::roundToInt (60000000.0 / (double) bpm)),
                  0.0);

    double t = 0.0;

    for (int i = 0; i < v.steps; ++i)
    {
        // The swing factor changes the length of this step, so the note-off has
        // to fit inside it: a gate of 1.0 on a short swing step would otherwise
        // emit events out of order.
        const double factor = (i % 2 == 1) ? (1.0 + swing * 0.5) : (1.0 - swing * 0.5);
        const double interval = stepTicks * factor;

        if (v.active[i] && v.note[i] >= 0)
        {
            int notes[4] {};
            int count = 0;

            if (chordsOn)
            {
                const auto chord = buildChord (v.note[i], v.key, v.scale,
                                               v.chordMask[i], v.chordInversion[i]);
                count = chord.count;
                for (int c = 0; c < count; ++c)
                    notes[c] = chord.notes[c];
            }
            else
            {
                notes[0] = v.note[i];
                count = 1;
            }

            // Match the synth's equal-power chord level, so the exported clip
            // sounds like what the plugin plays.
            const float velocity = v.velocity[i] / std::sqrt ((float) juce::jmax (1, count));
            const double length = juce::jlimit (1.0, juce::jmax (1.0, interval * 0.98),
                                                stepTicks * (double) gate);

            for (int c = 0; c < count; ++c)
            {
                seq.addEvent (juce::MidiMessage::noteOn (1, notes[c], velocity), t);
                seq.addEvent (juce::MidiMessage::noteOff (1, notes[c]), t + length);
            }
        }

        t += interval;
    }

    seq.updateMatchedPairs();
    seq.addEvent (juce::MidiMessage::endOfTrack(), t + 1.0);

    juce::MidiFile midiFile;
    midiFile.setTicksPerQuarterNote (ticksPerQuarter);
    midiFile.addTrack (seq);

    destination.deleteFile();

    juce::FileOutputStream out (destination);
    if (! out.openedOk())
        return false;

    return midiFile.writeTo (out);
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new VesperaAudioProcessor();
}
