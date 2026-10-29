#include "DebugLogger.h"
#include "PresetManager.h"

namespace vespera
{

//==============================================================================
DebugLogger::DebugLogger()
    : juce::Thread ("Vespera debug logger")
{
}

DebugLogger::~DebugLogger()
{
    stop();
}

void DebugLogger::start()
{
    if (! isThreadRunning())
    {
        if (! sessionStarted)
        {
            clearLog();
            sessionStarted = true;
        }

        startThread (juce::Thread::Priority::low);
    }
}

void DebugLogger::stop()
{
    if (isThreadRunning())
    {
        signalThreadShouldExit();
        notify();
        stopThread (1000);
    }
}

//==============================================================================
void DebugLogger::push (const BlockSnapshot& s) noexcept
{
    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (1, start1, size1, start2, size2);

    if (size1 > 0)
    {
        fifoData[(size_t) start1] = s;
        fifo.finishedWrite (1);
    }
}

//==============================================================================
juce::File DebugLogger::getLogFile() const
{
   #ifdef VESPERA_TEST_BUILD
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
               .getChildFile ("vespera_debug.log");
   #else
    return PresetManager::getAppDir().getChildFile ("vespera_debug.log");
   #endif
}

void DebugLogger::clearLog()
{
    auto f = getLogFile();
    if (f.existsAsFile())
        f.deleteFile();
    fileStarted = false;
    lastLoggedSec = -1.0e9;
    lastLoggedFlags = 0;
    suppressedSinceLastLog = 0;
}

//==============================================================================
void DebugLogger::run()
{
    while (! threadShouldExit())
    {
        drain();
        wait (50);
    }
    drain();
}

void DebugLogger::drain()
{
    for (int guard = 0; guard < kFifoSize; ++guard)
    {
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (1, start1, size1, start2, size2);

        if (size1 <= 0)
            break;

        const BlockSnapshot s = fifoData[(size_t) start1];
        fifo.finishedRead (1);

        history[(size_t) historyWrite] = s;
        historyWrite = (historyWrite + 1) % kHistory;
        if (historyCount < kHistory)
            ++historyCount;

        if (s.flags != 0)
        {
            lastFlags.store (s.flags, std::memory_order_relaxed);
            lastAnomalySec.store (s.timeSec, std::memory_order_relaxed);
            anomalyCount.fetch_add (1, std::memory_order_relaxed);

            const bool flagsChanged    = (s.flags != lastLoggedFlags);
            const bool cooldownElapsed = (s.timeSec - lastLoggedSec >= kAnomalyCooldownSec);
            const bool firstEver       = (lastLoggedSec < -1.0e8);

            if (firstEver || flagsChanged || cooldownElapsed)
            {
                juce::String batch;
                batch << "\n"
                      << "============================================================\n"
                      << "*** ANOMALY @" << timestamp() << "  flags: "
                      << describeFlags (s.flags) << "\n"
                      << "============================================================\n";

                if (suppressedSinceLastLog > 0)
                    batch << "  (+" << suppressedSinceLastLog
                          << " similar block(s) suppressed since the last entry)\n";

                const int n = historyCount;
                for (int i = 0; i < n; ++i)
                {
                    const int idx = (historyWrite - n + i + kHistory * 2) % kHistory;
                    batch << formatLine (history[(size_t) idx], true) << "\n";
                }
                batch << "---- trigger block ----\n";
                batch << formatLine (s, true) << "\n";
                appendLine (batch);

                lastLoggedFlags = s.flags;
                lastLoggedSec = s.timeSec;
                suppressedSinceLastLog = 0;
            }
            else
            {
                ++suppressedSinceLastLog;
            }
        }
        else if (s.timeSec - lastHeartbeatSec >= 2.0)
        {
            lastHeartbeatSec = s.timeSec;
            appendLine (formatLine (s, false) + "\n");
        }
    }
}

//==============================================================================
void DebugLogger::appendLine (const juce::String& line)
{
    auto f = getLogFile();
    f.getParentDirectory().createDirectory();

    if (f.existsAsFile() && f.getSize() > kMaxLogBytes)
    {
        auto prev = f.getSiblingFile ("vespera_debug.prev.log");
        prev.deleteFile();
        f.moveFileTo (prev);
        fileStarted = false;
    }

    if (! fileStarted)
    {
        f.appendText (juce::String ("# Vespera debug log - started ")
                          + timestamp() + "\n");
        fileStarted = true;
    }

    f.appendText (line);
}

//==============================================================================
juce::String DebugLogger::timestamp()
{
    const auto now = juce::Time::getCurrentTime();
    return now.toString (true, true, true, true) + "."
             + juce::String (now.getMilliseconds()).paddedLeft ('0', 3);
}

juce::String DebugLogger::describeFlags (unsigned flags)
{
    if (flags == 0)
        return "none";

    juce::StringArray parts;
    if (flags & kFlagNonFinite)   parts.add ("NONFINITE");
    if (flags & kFlagInputHot)    parts.add ("SYNTH_HOT");
    if (flags & kFlagOutHot)      parts.add ("OUT_HOT");
    if (flags & kFlagHeavyGR)     parts.add ("HEAVY_GR");
    return parts.joinIntoString ("|");
}

juce::String DebugLogger::formatLine (const BlockSnapshot& s, bool anomaly)
{
    juce::String line;
    line << (anomaly ? "  " : "")
         << "t=" << juce::String (s.timeSec, 2) << "s"
         << "  synth=" << juce::String (s.inPeak, 4)
         << " char=" << juce::String (s.charPeak, 4)
         << " out=" << juce::String (s.outPeak, 4)
         << "  bpm=" << juce::String (s.bpm, 1)
         << " voices=" << s.activeVoices
         << " step=" << s.currentStep
         << "  key=" << s.key
         << " scale=" << s.scale
         << " pool=" << s.poolSize
         << " steps=" << s.steps
         << " dens=" << juce::String (s.density, 2)
         << " oct=" << s.octaves
         << " contour=" << s.contour
         << " mutate=" << juce::String (s.mutate, 2)
         << " transpose=" << s.transpose
         << " swing=" << juce::String (s.swing, 2)
         << " gate=" << juce::String (s.gate, 2)
         << " accent=" << juce::String (s.accent, 2)
         << "  wave=" << s.wave
         << " A=" << juce::String (s.attack, 3)
         << " D=" << juce::String (s.decay, 3)
         << " S=" << juce::String (s.sustain, 2)
         << " R=" << juce::String (s.release, 3)
         << " det=" << juce::String (s.detune, 2)
         << "  filt=" << s.filterType
         << "/" << juce::String (s.filterCutoff, 0)
         << "/" << juce::String (s.filterReso, 2)
         << "/" << juce::String (s.filterEnv, 2)
         << " lfo=" << juce::String (s.lfoRate, 2)
         << (s.lfoSynced ? "sync" : "hz")
         << " shape=" << s.lfoShape
         << " motion=" << juce::String (s.motion, 2)
         << "  drive=" << juce::String (s.drive, 2)
         << " crush=" << juce::String (s.crush, 2)
         << " noise=" << juce::String (s.noise, 2)
         << " wow=" << juce::String (s.wow, 2)
         << " fold=" << juce::String (s.fold, 2)
         << " ring=" << juce::String (s.ring, 2)
         << "  out=" << juce::String (s.output, 1)
         << " LIM=" << (s.limiterOn ? 1 : 0)
         << " GR=" << juce::String (s.grDb, 1)
         << " LT=" << (s.latchOn ? 1 : 0)
         << " MON=" << (s.monitorOn ? 1 : 0)
         << " KEYLOCK=" << (s.keyLockOn ? 1 : 0);
    return line;
}

} // namespace vespera
