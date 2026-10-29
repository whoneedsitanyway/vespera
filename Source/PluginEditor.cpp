#include "PluginEditor.h"

using namespace vespera;

//==============================================================================
MidiDragButton::MidiDragButton (VesperaAudioProcessor& p) : proc (p)
{
    setButtonText ("MIDI");
    setTooltip ("MIDI - drag this button into your DAW to drop the pattern as a clip, "
                "or click to write it to a file");
    setConnectedEdges (0);
}

juce::File MidiDragButton::writeTempFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                   .getChildFile ("Vespera MIDI");
    dir.createDirectory();

    auto file = dir.getChildFile ("Vespera Pattern.mid");

    if (! proc.writePatternMidiFile (file))
        return {};

    return file;
}

void MidiDragButton::mouseDown (const juce::MouseEvent&)
{
    dragging = false;
}

void MidiDragButton::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5)
        return;

    dragging = true;

    const auto file = writeTempFile();
    if (! file.existsAsFile())
        return;

    juce::StringArray files;
    files.add (file.getFullPathName());
    juce::DragAndDropContainer::performExternalDragDropOfFiles (files, false, this);

    // The drag loop is modal: mouseUp may never reach us, so reset here as well.
    dragging = false;
}

void MidiDragButton::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

//==============================================================================
VesperaAudioProcessorEditor::VesperaAudioProcessorEditor (VesperaAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p),
      tooltipWindow (this, 650), presetManager (p.apvts),
      midiButton (p), patternDisplay (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (kWidth, kFullHeight);

    // --- NOTES ---------------------------------------------------------------
    addCombo (keyCombo,   ids::key,   "KEY",   Scale::getKeyNames(),
              "KEY - tonic of the scale the generator writes in");
    addCombo (scaleCombo, ids::scale, "SCALE", Scale::getNames(),
              "SCALE - all generated notes are snapped to this scale");

    addKnob (poolSize, ids::poolSize, "POOL",
             "POOL - how many of your most recent notes the generator can use");

    clearPoolButton.setTooltip ("CLEAR - forget every note in the pool and stop the pattern");
    clearPoolButton.setConnectedEdges (0);
    clearPoolButton.onClick = [this] { processorRef.clearNotePool(); };
    addAndMakeVisible (clearPoolButton);

    keyLockButton.setTooltip ("LOCK KEY - keep the key and scale when RANDOM is pressed");
    addAndMakeVisible (keyLockButton);
    keyLockAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::keyLock, keyLockButton);

    latchButton.setTooltip ("LATCH - freeze the note pool so the generator keeps playing after you let go");
    addAndMakeVisible (latchButton);
    latchAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::latch, latchButton);

    monitorButton.setTooltip ("MONITOR - also play the notes you feed in");
    addAndMakeVisible (monitorButton);
    monitorAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::monitor, monitorButton);

    // --- PATTERN -------------------------------------------------------------
    addCombo (rateCombo, ids::rate, "RATE",
              { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" },
              "RATE - step length of the generated pattern (synced to the host tempo)");
    addCombo (contourCombo, ids::contour, "CONTOUR",
              getContourNames(),
              "CONTOUR - how the generator walks through the note pool");

    addKnob (steps,   ids::steps,   "STEPS",
             "STEPS - pattern length in steps");
    addKnob (density, ids::density, "DENSITY",
             "DENSITY - probability that a step plays a note");
    addKnob (octaves, ids::octaves, "OCTAVES",
             "OCTAVES - range of octave jumps (1 = stay put, 3 = +/- 1 octave)");
    addKnob (mutate,  ids::mutate,  "MUTATE",
             "MUTATE - how much the pattern evolves at the end of every cycle");
    addKnob (swing,   ids::swing,   "SWING",
             "SWING - delays the off-beat steps for a shuffled feel");
    addKnob (gate,    ids::gate,    "GATE",
             "GATE - note length as a fraction of the step");
    addKnob (accent,  ids::accent,  "ACCENT",
             "ACCENT - velocity boost on the first step of each beat");
    addKnob (transpose, ids::transpose, "TRANSPOSE",
             "TRANSPOSE - moves the whole pattern by scale degrees, so it always stays in key");

    // --- VOICE ---------------------------------------------------------------
    addCombo (waveCombo, ids::wave, "WAVE",
              { "Sine", "Triangle", "Saw", "Square" },
              "WAVE - oscillator waveform of the synth voice");

    addKnob (attack,  ids::attack,  "ATTACK",  "ATTACK - envelope attack time");
    addKnob (decay,   ids::decay,   "DECAY",   "DECAY - envelope decay time");
    addKnob (sustain, ids::sustain, "SUSTAIN", "SUSTAIN - envelope sustain level");
    addKnob (release, ids::release, "RELEASE", "RELEASE - envelope release time");
    addKnob (detune,  ids::detune,  "DETUNE",  "DETUNE - spread between the two oscillators");

    // --- FILTER --------------------------------------------------------------
    addCombo (filterTypeCombo, ids::filterType, "TYPE",
              { "Off", "LP", "BP", "HP" },
              "TYPE - filter type for the synth voice");
    addCombo (filterShapeCombo, ids::lfoShape, "LFO",
              Lfo::getNames(),
              "LFO - modulation shape: sine, triangle, ramps, square, random or walk");
    addCombo (filterRateCombo, ids::lfoRate, "DIVISION",
              { "4/1", "2/1", "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" },
              "DIVISION - LFO speed in note values (used when Sync is on)");

    addKnob (lfoRateHz, ids::lfoRateHz, "RATE",
             "RATE - LFO speed in Hz (used when Sync is off)");
    addKnob (cutoff,     ids::filterCutoff, "CUTOFF", "CUTOFF - filter frequency");
    addKnob (filterReso, ids::filterReso,   "RESO",   "RESO - filter resonance");
    addKnob (filterEnv,  ids::filterEnv,    "ENV",    "ENV - envelope follower amount into the filter");
    addKnob (motion,     ids::motion,       "MOTION", "MOTION - LFO depth into the filter");

    lfoSyncButton.setTooltip ("SYNC - lock the LFO to the host tempo (disables RATE)");
    addAndMakeVisible (lfoSyncButton);
    lfoSyncAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::lfoSync, lfoSyncButton);
    lfoSyncButton.onClick = [this] { updateLfoRateUi(); };
    updateLfoRateUi();

    // --- CHARACTER -----------------------------------------------------------
    addKnob (drive, ids::drive, "DRIVE", "DRIVE - tape-style saturation");
    addKnob (crush, ids::crush, "CRUSH", "CRUSH - bit-depth and sample-rate reduction");
    addKnob (noise, ids::noise, "NOISE", "NOISE - hiss and vinyl crackle, only while notes sound");
    addKnob (wow,   ids::wow,   "WOW",   "WOW - tape wow and flutter");
    addKnob (fold,  ids::fold,  "FOLD",  "FOLD - wave folding, from soft harmonics to metallic");
    addKnob (ring,  ids::ring,  "RING",  "RING - ring modulation, tremolo at low settings");

    // --- OUTPUT --------------------------------------------------------------
    addKnob (output, ids::output, "OUTPUT", "OUTPUT - output level");

    limiterButton.setTooltip ("LIMITER - transparent peak limiter on the output (off = bypass)");
    addAndMakeVisible (limiterButton);
    limiterAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processorRef.apvts, ids::limiter, limiterButton);

    startTimerHz (10);

    // --- header buttons ------------------------------------------------------
    auto styleHeaderButton = [] (juce::TextButton& b, const juce::String& tip)
    {
        b.setTooltip (tip);
        b.setConnectedEdges (0);
    };
    styleHeaderButton (randomButton,  "RANDOM - new key, pattern, voice and character; seeds the pool with a chord");
    styleHeaderButton (compactButton, "Toggle compact mode (hide the pattern display)");

    randomButton.onClick  = [this] { processorRef.randomizeParameters(); };
    compactButton.onClick = [this] { setCompact (! compact); };
    addAndMakeVisible (randomButton);
    addAndMakeVisible (compactButton);
    addAndMakeVisible (midiButton);

    midiButton.onClick = [this]
    {
        const auto file = midiButton.writeTempFile();
        if (file.existsAsFile())
            file.revealToUser();
    };

    // --- presets -------------------------------------------------------------
    presetBox.setTooltip ("PRESET - select a saved preset");
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.setColour (juce::ComboBox::textColourId, findColour (VesperaLookAndFeel::accentColourId));
    presetBox.onChange = [this]
    {
        const auto name = presetBox.getText();
        if (name.isNotEmpty() && name != "(no presets)")
            loadPresetByName (name);
    };
    addAndMakeVisible (presetBox);

    saveButton.setTooltip ("SAVE - save the current settings as a preset");
    saveButton.onClick = [this] { showSavePresetDialog(); };
    addAndMakeVisible (saveButton);

    // --- theme ---------------------------------------------------------------
    themeButton.setTooltip ("Toggle light / dark theme");
    themeButton.onClick = [this] { applyTheme (! darkTheme); };
    addAndMakeVisible (themeButton);

    // --- debug log -----------------------------------------------------------
    logButton.setTooltip ("Open the debug log (records level anomalies, e.g. runaway)");
    logButton.onClick = [this]
    {
        auto& logger = processorRef.getDebugLogger();
        if (! logger.getLogFile().existsAsFile())
            logger.clearLog();
        logger.getLogFile().revealToUser();
    };
    addAndMakeVisible (logButton);

    loadThemePreference();
    presetManager.installFactoryPresets();
    refreshPresets();

    if (! processorRef.wasStateRestored() && ! processorRef.wasDefaultPresetApplied())
    {
        loadPresetByName (PresetManager::getDefaultPresetName());
        processorRef.markDefaultPresetApplied();
    }

    addAndMakeVisible (patternDisplay);
}

VesperaAudioProcessorEditor::~VesperaAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
void VesperaAudioProcessorEditor::timerCallback()
{
    updateLfoRateUi();
    logButton.setWarning (processorRef.getDebugLogger().hasAnomaly());
}

void VesperaAudioProcessorEditor::updateLfoRateUi()
{
    const bool synced = processorRef.apvts.getRawParameterValue (ids::lfoSync)->load() > 0.5f;
    if (synced == lfoSynced)
        return;

    lfoSynced = synced;

    // With Sync on the LFO follows the host tempo, so the Hz knob has no
    // effect: dim and disable it, exactly like the Delay TIME knob in Lunara.
    lfoRateHz.slider.setEnabled (! synced);
    lfoRateHz.slider.setAlpha (synced ? 0.35f : 1.0f);
    lfoRateHz.label.setAlpha (synced ? 0.45f : 1.0f);
}

//==============================================================================
void VesperaAudioProcessorEditor::setCompact (bool shouldBeCompact)
{
    compact = shouldBeCompact;
    patternDisplay.setVisible (! compact);
    compactButton.setButtonText (compact ? "FULL" : "COMPACT");
    setSize (getWidth(), compact ? kCompactHeight : kFullHeight);
    resized();
}

//==============================================================================
void VesperaAudioProcessorEditor::refreshPresets()
{
    presetBox.clear (juce::dontSendNotification);

    const auto names = presetManager.getPresetNames();
    if (names.isEmpty())
    {
        presetBox.addItem ("(no presets)", 1);
        presetBox.setText ("(no presets)", juce::dontSendNotification);
        return;
    }

    int id = 1;
    for (const auto& n : names)
        presetBox.addItem (n, id++);

    const auto last = processorRef.getLastPresetName();
    const auto def  = PresetManager::getDefaultPresetName();
    const auto toSelect = names.contains (last) ? last
                        : names.contains (def)  ? def
                                                : names[0];
    presetBox.setText (toSelect, juce::dontSendNotification);
    processorRef.setLastPresetName (toSelect);
}

void VesperaAudioProcessorEditor::loadPresetByName (const juce::String& name)
{
    if (presetManager.loadPreset (name))
    {
        processorRef.setLastPresetName (name);
        repaint();
    }
}

void VesperaAudioProcessorEditor::showSavePresetDialog()
{
    auto* aw = new juce::AlertWindow ("Save Preset",
                                      "Enter a name for this preset:",
                                      juce::MessageBoxIconType::NoIcon);
    aw->addTextEditor ("name", processorRef.getLastPresetName(), "Name:");
    aw->addButton ("Save",   1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [this, aw] (int result)
        {
            if (result == 1)
            {
                const auto name = aw->getTextEditorContents ("name").trim();
                if (name.isNotEmpty() && presetManager.savePreset (name))
                {
                    processorRef.setLastPresetName (name);
                    refreshPresets();
                    presetBox.setText (name, juce::dontSendNotification);
                }
            }
        }), true);
}

//==============================================================================
void VesperaAudioProcessorEditor::refreshThemedColours()
{
    const auto text = findColour (VesperaLookAndFeel::textColourId);
    const auto accentColour = findColour (VesperaLookAndFeel::accentColourId);

    for (auto* k : { &poolSize, &steps, &density, &octaves, &mutate, &swing, &gate, &accent,
                     &transpose,
                     &attack, &decay, &sustain, &release, &detune,
                     &cutoff, &filterReso, &filterEnv, &motion, &lfoRateHz,
                     &drive, &crush, &noise, &wow, &fold, &ring,
                     &output })
        k->label.setColour (juce::Label::textColourId, text);

    for (auto* c : { &keyCombo, &scaleCombo, &rateCombo, &contourCombo,
                     &waveCombo, &filterTypeCombo, &filterShapeCombo,
                     &filterRateCombo })
        c->label.setColour (juce::Label::textColourId, text);

    presetBox.setColour (juce::ComboBox::textColourId, accentColour);
    repaint();
}

//==============================================================================
void VesperaAudioProcessorEditor::applyTheme (bool shouldBeDark)
{
    darkTheme = shouldBeDark;
    lookAndFeel.setDark (shouldBeDark);
    themeButton.setIcon (shouldBeDark ? IconButton::Icon::Sun : IconButton::Icon::Moon);
    refreshThemedColours();
    sendLookAndFeelChange();
    repaint();

    auto dir = PresetManager::getAppDir();
    dir.createDirectory();
    dir.getChildFile ("theme.txt").replaceWithText (shouldBeDark ? "dark" : "light");
}

void VesperaAudioProcessorEditor::loadThemePreference()
{
    const auto f = PresetManager::getAppDir().getChildFile ("theme.txt");
    darkTheme = true;
    if (f.existsAsFile())
        darkTheme = f.loadFileAsString().trim().toLowerCase() != "light";

    lookAndFeel.setDark (darkTheme);
    themeButton.setIcon (darkTheme ? IconButton::Icon::Sun : IconButton::Icon::Moon);
    refreshThemedColours();
}

//==============================================================================
void VesperaAudioProcessorEditor::addKnob (Knob& k, const juce::String& paramId,
                                           const juce::String& name,
                                           const juce::String& tooltip)
{
    addAndMakeVisible (k.slider);

    k.slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 82, 16);
    k.slider.setTooltip (tooltip);

    k.label.setText (name, juce::dontSendNotification);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setFont (juce::Font (juce::FontOptions (10.5f)));
    addAndMakeVisible (k.label);

    k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processorRef.apvts, paramId, k.slider);
}

void VesperaAudioProcessorEditor::addCombo (Combo& c, const juce::String& paramId,
                                            const juce::String& name,
                                            const juce::StringArray& items,
                                            const juce::String& tooltip)
{
    c.label.setText (name, juce::dontSendNotification);
    c.label.setJustificationType (juce::Justification::centredLeft);
    c.label.setFont (juce::Font (juce::FontOptions (10.5f)));
    addAndMakeVisible (c.label);

    c.box.addItemList (items, 1);
    c.box.setJustificationType (juce::Justification::centredLeft);
    c.box.setTooltip (tooltip);
    addAndMakeVisible (c.box);

    c.attach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processorRef.apvts, paramId, c.box);
}

//==============================================================================
void VesperaAudioProcessorEditor::paintScrew (juce::Graphics& g, juce::Point<float> centre)
{
    const float r = 7.0f;
    juce::Rectangle<float> c (r * 2.0f, r * 2.0f);
    c.setCentre (centre);

    juce::ColourGradient grad (juce::Colours::white.withAlpha (0.22f), c.getX(), c.getY(),
                               juce::Colours::black.withAlpha (0.45f), c.getRight(), c.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (c);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (c, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawLine (centre.x - r * 0.55f, centre.y - r * 0.55f,
                centre.x + r * 0.55f, centre.y + r * 0.55f, 1.8f);
}

void VesperaAudioProcessorEditor::paintSectionLabel (juce::Graphics& g, juce::Rectangle<int> r,
                                                      const juce::String& title,
                                                      juce::Colour colour)
{
    auto header = r.reduced (14, 10).removeFromTop (16);
    g.setColour (colour);
    g.setFont (juce::Font (juce::FontOptions (11.0f).withStyle ("Bold")));
    g.drawText (title, header, juce::Justification::centredLeft, false);

    g.setColour (colour.withAlpha (0.35f));
    g.drawLine ((float) header.getX(), (float) header.getBottom() + 1.0f,
                (float) header.getRight(), (float) header.getBottom() + 1.0f, 1.0f);
}

//==============================================================================
void VesperaAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto encColour = findColour (VesperaLookAndFeel::enclosureColourId);
    g.fillAll (encColour);

    const auto encF = enclosure.toFloat();

    {
        juce::ColourGradient body (encColour.brighter (0.14f), encF.getX(), encF.getY(),
                                   encColour.darker (0.35f), encF.getRight(), encF.getBottom(), false);
        g.setGradientFill (body);
        g.fillRoundedRectangle (encF, 14.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (darkTheme ? 0.07f : 0.35f));
    g.drawRoundedRectangle (encF.reduced (1.5f), 12.5f, 1.5f);
    g.setColour (juce::Colours::black.withAlpha (darkTheme ? 0.45f : 0.20f));
    g.drawRoundedRectangle (encF.reduced (0.5f), 14.0f, 1.5f);

    paintScrew (g, { encF.getX() + 22.0f, encF.getY() + 22.0f });
    paintScrew (g, { encF.getRight() - 22.0f, encF.getY() + 22.0f });
    paintScrew (g, { encF.getX() + 22.0f, encF.getBottom() - 22.0f });
    paintScrew (g, { encF.getRight() - 22.0f, encF.getBottom() - 22.0f });

    g.setColour (findColour (VesperaLookAndFeel::textColourId));
    g.setFont (juce::Font (juce::FontOptions (34.0f).withStyle ("Bold")));
    g.drawText ("VESPERA", juce::roundToInt (encF.getX() + 34.0f), juce::roundToInt (encF.getY() + 20.0f),
                320, 36, juce::Justification::centredLeft, false);

    g.setColour (findColour (VesperaLookAndFeel::textDimColourId));
    g.setFont (juce::Font (juce::FontOptions (11.5f)));
    g.drawText ("MELODY IDEA GENERATOR",
                juce::roundToInt (encF.getX() + 36.0f), juce::roundToInt (encF.getY() + 56.0f),
                380, 16, juce::Justification::centredLeft, false);

   #ifdef JucePlugin_VersionString
    g.setColour (findColour (VesperaLookAndFeel::textDimColourId).withAlpha (0.75f));
    g.drawText ("v" JucePlugin_VersionString,
                juce::roundToInt (encF.getRight() - 34.0f - 120.0f),
                juce::roundToInt (encF.getY() + 56.0f),
                120, 16, juce::Justification::centredRight, false);
   #endif

    paintSectionLabel (g, notesArea,   "NOTES",
                       findColour (VesperaLookAndFeel::coolColourId));
    paintSectionLabel (g, patternArea, "PATTERN",
                       findColour (VesperaLookAndFeel::accentColourId));
    paintSectionLabel (g, voiceArea,   "VOICE",
                       findColour (VesperaLookAndFeel::coolColourId));
    paintSectionLabel (g, filterArea,  "FILTER",
                       findColour (VesperaLookAndFeel::accentColourId));
    paintSectionLabel (g, charArea,    "CHARACTER",
                       findColour (VesperaLookAndFeel::hotColourId));
    paintSectionLabel (g, outArea,     "OUTPUT",
                       findColour (VesperaLookAndFeel::accentDimColourId));
}

//==============================================================================
void VesperaAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    enclosure = area;

    auto inner = area.reduced (30, 26);

    inner.removeFromTop (44);

    auto row = inner.removeFromTop (30);

    presetBox.setBounds (row.removeFromLeft (230));
    row.removeFromLeft (8);
    saveButton.setBounds (row.removeFromLeft (30));
    row.removeFromLeft (6);
    themeButton.setBounds (row.removeFromLeft (30));
    row.removeFromLeft (6);
    logButton.setBounds (row.removeFromLeft (30));

    const int btnW = 100;
    const int btnGap = 12;
    compactButton.setBounds (row.removeFromRight (btnW));
    row.removeFromRight (btnGap);
    randomButton.setBounds (row.removeFromRight (btnW));
    row.removeFromRight (btnGap);
    midiButton.setBounds (row.removeFromRight (72));

    inner.removeFromTop (10);

    if (! compact)
    {
        displayArea = inner.removeFromBottom (170);
        inner.removeFromBottom (10);
        patternDisplay.setBounds (displayArea.reduced (4, 0));
    }
    else
    {
        displayArea = {};
    }

    const int gap = 12;
    const int colW = (inner.getWidth() - gap * 5) / 6;

    notesArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    patternArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    voiceArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    filterArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    charArea = inner.removeFromLeft (colW);
    inner.removeFromLeft (gap);
    outArea = inner;

    auto placeCombo = [] (Combo& c, juce::Rectangle<int>& col)
    {
        auto r = col.removeFromTop (38);
        c.label.setBounds (r.removeFromTop (14));
        c.box.setBounds (r);
        col.removeFromTop (4);
    };

    auto placeGrid = [] (std::initializer_list<Knob*> ks, juce::Rectangle<int> r, int cols)
    {
        const int n = (int) ks.size();
        const int rows = (n + cols - 1) / cols;
        const int cw = r.getWidth() / cols;
        const int ch = juce::jmin (r.getHeight() / rows, 150);
        const int totalH = ch * rows;
        const int y0 = r.getY() + juce::jmax (0, (r.getHeight() - totalH) / 2);
        int i = 0;
        for (auto* k : ks)
        {
            const int c = i % cols, rr = i / cols;
            juce::Rectangle<int> cell (r.getX() + c * cw, y0 + rr * ch, cw, ch);
            auto lab = cell.removeFromBottom (15);
            k->label.setBounds (lab);
            k->slider.setBounds (cell.reduced (6, 2));
            ++i;
        }
    };

    // NOTES
    {
        auto col = notesArea.reduced (10, 8);
        col.removeFromTop (20);
        placeCombo (keyCombo, col);
        placeCombo (scaleCombo, col);

        auto knobArea = col.removeFromTop (132);
        placeGrid ({ &poolSize }, knobArea, 1);

        col.removeFromTop (4);
        clearPoolButton.setBounds (col.removeFromTop (26));

        col.removeFromTop (8);
        auto toggleRow = col.removeFromTop (28);
        const int half = (toggleRow.getWidth() - 8) / 2;
        latchButton.setBounds (toggleRow.removeFromLeft (half));
        toggleRow.removeFromLeft (8);
        monitorButton.setBounds (toggleRow.removeFromLeft (half));

        col.removeFromTop (8);
        keyLockButton.setBounds (col.removeFromTop (28));
    }

    // PATTERN
    {
        auto col = patternArea.reduced (10, 8);
        col.removeFromTop (20);
        placeCombo (rateCombo, col);
        placeCombo (contourCombo, col);
        col.removeFromTop (2);
        placeGrid ({ &steps, &density, &octaves, &mutate,
                     &swing, &gate, &accent, &transpose }, col, 2);
    }

    // VOICE
    {
        auto col = voiceArea.reduced (10, 8);
        col.removeFromTop (20);
        placeCombo (waveCombo, col);
        col.removeFromTop (2);
        placeGrid ({ &attack, &decay, &sustain, &release, &detune }, col, 2);
    }

    // FILTER
    {
        auto col = filterArea.reduced (10, 8);
        col.removeFromTop (20);
        placeCombo (filterTypeCombo, col);
        placeCombo (filterShapeCombo, col);

        // RATE knob (Hz) and, below it, the DIVISION combo next to the SYNC
        // toggle: the knob is dimmed while Sync locks the LFO to the tempo.
        auto knobArea = col.removeFromTop (juce::jlimit (60, 120, col.getHeight() - 190));
        placeGrid ({ &lfoRateHz }, knobArea, 1);

        col.removeFromTop (4);
        auto divLabel = col.removeFromTop (14);
        filterRateCombo.label.setBounds (divLabel);

        auto syncRow = col.removeFromTop (30);
        lfoSyncButton.setBounds (syncRow.removeFromRight (60));
        syncRow.removeFromRight (8);
        filterRateCombo.box.setBounds (syncRow);

        col.removeFromTop (6);
        placeGrid ({ &cutoff, &filterReso, &filterEnv, &motion }, col, 2);
    }

    // CHARACTER
    {
        auto col = charArea.reduced (10, 8);
        col.removeFromTop (20);
        placeGrid ({ &drive, &crush, &noise, &wow, &fold, &ring }, col, 2);
    }

    // OUTPUT
    {
        auto col = outArea.reduced (10, 8);
        col.removeFromTop (20);
        auto bottom = col.removeFromBottom (26);
        col.removeFromBottom (8);   // small gap above the checkbox
        placeGrid ({ &output }, col, 1);
        limiterButton.setBounds (bottom);
    }

    updateLfoRateUi();
}
