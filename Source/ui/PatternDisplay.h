#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../PluginProcessor.h"
#include "VesperaLookAndFeel.h"

namespace vespera
{

//==============================================================================
//  Pattern visualiser.
//
//  Shows the note pool the generator is working from, the generated steps (with
//  their pitches and velocities) and the current playhead.  This is the main
//  feedback surface of Vespera: you can see exactly which melody it invented
//  and how it evolves while it plays.
//==============================================================================
class PatternDisplay : public juce::Component,
                       private juce::Timer
{
public:
    explicit PatternDisplay (VesperaAudioProcessor& p);
    ~PatternDisplay() override;

    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    VesperaAudioProcessor& proc;
    PatternView view;
    bool haveView = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatternDisplay)
};

} // namespace vespera
