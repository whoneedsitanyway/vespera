#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <algorithm>

namespace vespera
{

//==============================================================================
//  Recent-note memory.
//
//  Keeps the last N distinct notes the user played.  The pattern generator
//  picks from this pool, so whatever chord or phrase you feed Vespera becomes
//  the raw material for the melodies it invents.  "Latch" freezes the pool so
//  you can take your hands off the keyboard and let the generator keep going.
//==============================================================================
class NotePool
{
public:
    static constexpr int kMaxNotes = 16;

    void reset()
    {
        count = 0;
        changed = false;
    }

    void setLatch (bool shouldLatch) noexcept { latch = shouldLatch; }
    bool isLatched() const noexcept          { return latch; }

    // Returns true when the pool actually changed.
    bool add (int note, int maxSize)
    {
        if (latch)
            return false;

        note = juce::jlimit (0, 127, note);
        maxSize = juce::jlimit (1, kMaxNotes, maxSize);

        for (int i = 0; i < count; ++i)
            if (recent[(size_t) i] == note)
                return false;

        // Keep the array ordered from oldest to newest and never longer than
        // the requested pool size.  This also handles the user shrinking the
        // pool after having played more notes than it can hold.
        trimTo (maxSize);

        if (count >= maxSize)
        {
            for (int i = 1; i < count; ++i)
                recent[(size_t) (i - 1)] = recent[(size_t) i];
            --count;
        }

        recent[(size_t) count++] = note;
        changed = true;
        return true;
    }

    int size (int maxSize) const
    {
        return juce::jmin (count, juce::jlimit (1, kMaxNotes, maxSize));
    }

    // Copies the most recent `maxSize` notes in ascending pitch order.
    int getSorted (int* dest, int maxSize) const
    {
        const int n = size (maxSize);
        if (n <= 0)
            return 0;

        int tmp[kMaxNotes];
        const int start = count - n;
        for (int i = 0; i < n; ++i)
            tmp[i] = recent[(size_t) (start + i)];

        std::sort (tmp, tmp + n);
        for (int i = 0; i < n; ++i)
            dest[i] = tmp[i];

        return n;
    }

    bool consumeChanged() noexcept
    {
        const bool c = changed;
        changed = false;
        return c;
    }

private:
    void trimTo (int maxSize)
    {
        if (count <= maxSize)
            return;

        const int drop = count - maxSize;
        for (int i = drop; i < count; ++i)
            recent[(size_t) (i - drop)] = recent[(size_t) i];

        count = maxSize;
    }

    std::array<int, kMaxNotes> recent {};
    int  count = 0;
    bool latch = false;
    bool changed = false;
};

} // namespace vespera
