#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include "PresetManager.h"
#include "ui/VesperaLookAndFeel.h"
#include "ui/PatternDisplay.h"

//==============================================================================
//  A button that exports the current pattern as a MIDI file and starts an
//  external drag, so it can be dropped straight into a DAW track.
//==============================================================================
class MidiDragButton : public juce::TextButton
{
public:
    explicit MidiDragButton (VesperaAudioProcessor& p);

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    juce::File writeTempFile();

private:
    VesperaAudioProcessor& proc;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiDragButton)
};

//==============================================================================
class VesperaAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    private juce::Timer
{
public:
    explicit VesperaAudioProcessorEditor (VesperaAudioProcessor&);
    ~VesperaAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void updateLfoRateUi();
    void setCompact (bool shouldBeCompact);

    void refreshPresets();
    void loadPresetByName (const juce::String& name);
    void showSavePresetDialog();
    void applyTheme (bool shouldBeDark);
    void loadThemePreference();
    void refreshThemedColours();

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attach;
    };

    struct Combo
    {
        juce::ComboBox box;
        juce::Label    label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attach;
    };

    void addKnob (Knob& k, const juce::String& paramId, const juce::String& name,
                  const juce::String& tooltip);
    void addCombo (Combo& c, const juce::String& paramId, const juce::String& name,
                   const juce::StringArray& items, const juce::String& tooltip);
    void paintSectionLabel (juce::Graphics&, juce::Rectangle<int>,
                            const juce::String& title, juce::Colour colour);
    void paintScrew (juce::Graphics&, juce::Point<float> centre);

    VesperaAudioProcessor& processorRef;
    vespera::VesperaLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow;
    PresetManager presetManager;

    // NOTES
    Combo keyCombo, scaleCombo;
    Knob poolSize;
    juce::TextButton clearPoolButton { "CLEAR" };

    // PATTERN
    Combo rateCombo, contourCombo;
    Knob steps, density, octaves, mutate, swing, gate, accent, transpose;

    // VOICE
    Combo waveCombo;
    Knob attack, decay, sustain, release, detune;

    // FILTER
    Combo filterTypeCombo, filterShapeCombo, filterRateCombo;
    Knob cutoff, filterReso, filterEnv, motion, lfoRateHz;
    juce::ToggleButton lfoSyncButton { "Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> lfoSyncAttach;

    // CHARACTER
    Knob drive, crush, noise, wow, fold, ring;

    // OUTPUT
    Knob output;
    juce::ToggleButton limiterButton { "Limiter" };
    juce::ToggleButton chordsButton  { "Chords" };
    juce::ToggleButton playSyncButton { "Play Sync" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limiterAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chordsAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playSyncAttach;

    juce::ToggleButton latchButton    { "Latch" };
    juce::ToggleButton monitorButton  { "Monitor" };
    juce::ToggleButton keyLockButton  { "Lock Key" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> latchAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> monitorAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> keyLockAttach;

    juce::ComboBox presetBox;
    vespera::IconButton saveButton  { vespera::IconButton::Icon::Save, "Save preset" };
    vespera::IconButton themeButton { vespera::IconButton::Icon::Moon, "Toggle theme" };
    vespera::IconButton logButton   { vespera::IconButton::Icon::Log,  "Open the debug log" };

    juce::TextButton randomButton  { "RANDOM" };
    juce::TextButton compactButton { "COMPACT" };
    MidiDragButton   midiButton;

    vespera::PatternDisplay patternDisplay;

    bool compact = false;
    bool darkTheme = true;
    bool lfoSynced = false;

    static constexpr int kWidth         = 1280;
    static constexpr int kFullHeight    = 820;
    static constexpr int kCompactHeight = 600;

    juce::Rectangle<int> enclosure;
    juce::Rectangle<int> notesArea, patternArea, voiceArea, filterArea, charArea, outArea;
    juce::Rectangle<int> displayArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VesperaAudioProcessorEditor)
};
