#include "PatternDisplay.h"

#include <cmath>

namespace vespera
{

PatternDisplay::PatternDisplay (VesperaAudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

PatternDisplay::~PatternDisplay()
{
    stopTimer();
}

void PatternDisplay::timerCallback()
{
    PatternView fresh;

    if (! proc.getPatternView (fresh))
        return;

    // Repaint only when something visible actually changed: at 30 Hz a full
    // repaint of the panel is wasted work while the generator is idle.
    const int voices = proc.getActiveVoiceCount();
    const int bpm = juce::roundToInt (proc.getDisplayBpm());

    if (haveView && voices == lastVoices && bpm == lastBpm && sameView (view, fresh))
        return;

    view = fresh;
    haveView = true;
    lastVoices = voices;
    lastBpm = bpm;
    repaint();
}

bool PatternDisplay::sameView (const PatternView& a, const PatternView& b)
{
    if (a.steps != b.steps || a.currentStep != b.currentStep
        || a.poolSize != b.poolSize || a.key != b.key || a.scale != b.scale
        || a.transpose != b.transpose || a.contour != b.contour
        || std::abs (a.density - b.density) > 1.0e-6f
        || std::abs (a.mutate - b.mutate) > 1.0e-6f)
        return false;

    for (int i = 0; i < a.poolSize && i < NotePool::kMaxNotes; ++i)
        if (a.poolNotes[i] != b.poolNotes[i])
            return false;

    for (int i = 0; i < PatternEngine::kMaxSteps; ++i)
        if (a.active[i] != b.active[i] || a.note[i] != b.note[i]
            || std::abs (a.velocity[i] - b.velocity[i]) > 1.0e-6f)
            return false;

    return true;
}

void PatternDisplay::paint (juce::Graphics& g)
{
    const auto accent    = findColour (VesperaLookAndFeel::accentColourId);
    const auto accentDim = findColour (VesperaLookAndFeel::accentDimColourId);
    const auto cool      = findColour (VesperaLookAndFeel::coolColourId);
    const auto hot       = findColour (VesperaLookAndFeel::hotColourId);
    const auto textDim   = findColour (VesperaLookAndFeel::textDimColourId);

    auto bounds = getLocalBounds().toFloat();
    const auto panel = bounds.reduced (0.5f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (panel, 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.72f));
    g.fillRoundedRectangle (panel.reduced (3.0f), 6.0f);
    g.setColour (cool.withAlpha (0.18f));
    g.drawRoundedRectangle (panel, 8.0f, 1.0f);

    auto inner = panel.reduced (12.0f, 8.0f);
    if (inner.getWidth() < 80.0f || inner.getHeight() < 40.0f)
        return;

    // ---- header: pool + key/scale ------------------------------------------
    auto header = inner.removeFromTop (18.0f);

    const juce::String keyName = Scale::getKeyNames()[juce::jlimit (0, 11, view.key)];
    const juce::String scaleName = Scale::getNames()[juce::jlimit (0, 9, view.scale)];

    juce::String poolText = "POOL: ";
    if (view.poolSize <= 0)
        poolText += "play some notes...";
    else
        for (int i = 0; i < view.poolSize; ++i)
            poolText += Scale::noteName (view.poolNotes[i]) + (i + 1 < view.poolSize ? "  " : "");

    g.setColour (cool.withAlpha (0.9f));
    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.drawText (poolText, header, juce::Justification::centredLeft, false);

    g.setColour (textDim);
    juce::String transposeText;
    if (view.transpose != 0)
        transposeText = (view.transpose > 0 ? "   |   +" : "   |   ")
                            + juce::String (view.transpose) + " st";

    g.drawText (keyName + " " + scaleName + transposeText + "   |   "
                    + juce::String (proc.getDisplayBpm(), 1) + " BPM   |   "
                    + juce::String (proc.getActiveVoiceCount()) + " voices",
                header, juce::Justification::centredRight, false);

    inner.removeFromTop (6.0f);

    // Reserve the footer before laying out the grid so the bars never run
    // underneath the text.
    auto footer = inner.removeFromBottom (14.0f);

    // ---- step grid ----------------------------------------------------------
    // Only the steps that exist are drawn, spread across the whole width, so the
    // pattern fills the panel instead of sitting in the left half of a 16-slot
    // grid.  In REVERSE the slots are mirrored: step 0 sits on the right, so the
    // playhead visibly travels right-to-left, matching the reversed figure.
    const int numSlots = juce::jlimit (1, PatternEngine::kMaxSteps, view.steps);
    const bool reversed = view.contour == kContourReverse;

    // Short patterns would otherwise turn into two enormous bars: cap the slot
    // width and centre the grid instead.
    const float slotW = juce::jmin (inner.getWidth() / (float) numSlots, 150.0f);
    const float gridW = slotW * (float) numSlots;
    const float gridX = inner.getX() + (inner.getWidth() - gridW) * 0.5f;
    const float gridH = inner.getHeight();

    for (int i = 0; i < numSlots; ++i)
    {
        const int slot = reversed ? (numSlots - 1 - i) : i;

        const bool active = view.active[slot];
        const bool current = slot == view.currentStep;

        const juce::Rectangle<float> cell (gridX + slotW * (float) i + 1.5f,
                                           inner.getY(),
                                           slotW - 3.0f,
                                           gridH);

        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRoundedRectangle (cell, 4.0f);

        auto content = cell;
        auto labelRow = content.removeFromTop (12.0f);

        if (active)
        {
            const int note = view.note[slot];
            const float norm = juce::jlimit (0.0f, 1.0f, (float) (note - 36) / 48.0f);
            const float barH = juce::jmax (6.0f, content.getHeight() * (0.18f + norm * 0.72f));

            auto bar = content.removeFromBottom (barH).reduced (2.0f, 0.0f);
            juce::Colour barColour = current ? accent : cool.withAlpha (0.55f);
            if (view.velocity[slot] > 0.9f)
                barColour = hot.withAlpha (current ? 0.95f : 0.6f);

            g.setColour (barColour);
            g.fillRoundedRectangle (bar, 3.0f);

            g.setColour (current ? juce::Colours::white : textDim.withAlpha (0.85f));
            g.setFont (juce::Font (juce::FontOptions (8.5f)));
            g.drawText (Scale::noteName (note), labelRow, juce::Justification::centred, false);
        }

        if (current)
        {
            g.setColour (accent);
            g.drawRoundedRectangle (cell.expanded (0.5f), 4.0f, 1.6f);
        }
        else
        {
            g.setColour (accentDim.withAlpha (0.35f));
            g.drawRoundedRectangle (cell, 4.0f, 1.0f);
        }
    }

    // ---- footer -------------------------------------------------------------
    g.setColour (textDim.withAlpha (0.7f));
    g.setFont (juce::Font (juce::FontOptions (9.5f)));

    const int densityPercent = juce::roundToInt (juce::jlimit (0.0f, 1.0f, view.density) * 100.0f);
    const juce::String mutateText = view.mutate > 0.001f
                                        ? "mutates every cycle"
                                        : "locked pattern";

    const auto contourNames = getContourNames();
    const juce::String contourName = contourNames[juce::jlimit (0, contourNames.size() - 1,
                                                               view.contour)];

    g.drawText ("steps " + juce::String (view.steps)
                    + "   |   " + contourName
                    + "   |   density " + juce::String (densityPercent) + "%"
                    + "   |   " + mutateText,
                footer, juce::Justification::centredLeft, false);
}

} // namespace vespera
