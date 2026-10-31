#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

namespace vespera
{

//==============================================================================
//  Lightweight "black box" recorder for hunting rare audio bugs (runaway,
//  NaN leaks, output overload).  The audio thread only pushes small POD
//  snapshots into a lock-free FIFO; a background thread drains it, keeps a
//  rolling history and appends to a log file.  When an anomaly flag is set the
//  whole recent history is dumped, so the lead-up is visible.
//==============================================================================
struct BlockSnapshot
{
    double timeSec = 0.0;

    float inPeak   = 0.0f;
    float charPeak = 0.0f;
    float outPeak  = 0.0f;

    int   key = 0, scale = 1, poolSize = 0, steps = 8, octaves = 2, contour = 2;
    int   wave = 1, filterType = 1, activeVoices = 0, currentStep = 0;
    int   transpose = 0, lfoShape = 0;

    float density = 0.0f, mutate = 0.0f, swing = 0.0f, gate = 0.0f, accent = 0.0f;
    float attack = 0.0f, decay = 0.0f, sustain = 0.0f, release = 0.0f, detune = 0.0f;
    float filterCutoff = 0.0f, filterReso = 0.0f, filterEnv = 0.0f;
    float lfoRate = 0.0f, motion = 0.0f;
    float drive = 0.0f, crush = 0.0f, noise = 0.0f, wow = 0.0f;
    float fold = 0.0f, ring = 0.0f;
    float output = 0.0f, bpm = 120.0f;
    float grDb = 0.0f;             // limiter gain reduction

    bool latchOn     = false;
    bool monitorOn   = false;
    bool keyLockOn   = false;
    bool lfoSynced   = false;
    bool limiterOn   = true;
    bool playSyncOn  = true;
    bool seqRunning  = true;

    unsigned flags = 0;
};

enum SnapshotFlags : unsigned
{
    kFlagNonFinite = 1u << 0,
    kFlagInputHot  = 1u << 1,
    kFlagOutHot    = 1u << 2,
    kFlagHeavyGR   = 1u << 3     // limiter reducing more than 12 dB
};

class DebugLogger : private juce::Thread
{
public:
    DebugLogger();
    ~DebugLogger() override;

    void start();
    void stop();

    void push (const BlockSnapshot& s) noexcept;

    juce::File getLogFile() const;

    bool hasAnomaly() const noexcept { return anomalyCount.load (std::memory_order_relaxed) > 0; }

    void clearLog();

    static juce::String describeFlags (unsigned flags);

private:
    void run() override;
    void drain();
    void appendLine (const juce::String& line);
    static juce::String formatLine (const BlockSnapshot& s, bool anomaly);
    static juce::String timestamp();

    static constexpr int kFifoSize = 8192;
    static constexpr int kHistory  = 256;
    static constexpr int kMaxLogBytes = 4 * 1024 * 1024;
    static constexpr double kAnomalyCooldownSec = 5.0;

    juce::AbstractFifo fifo { kFifoSize };
    std::array<BlockSnapshot, kFifoSize> fifoData;

    std::array<BlockSnapshot, kHistory> history {};
    int    historyCount = 0;
    int    historyWrite = 0;
    double lastHeartbeatSec = -1000.0;
    bool   fileStarted = false;
    bool   sessionStarted = false;

    double   lastLoggedSec = -1.0e9;
    unsigned lastLoggedFlags = 0;
    int      suppressedSinceLastLog = 0;

    std::atomic<int>      anomalyCount   { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DebugLogger)
};

} // namespace vespera
