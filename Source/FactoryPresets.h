#pragma once

#include <juce_core/juce_core.h>

//==============================================================================
//  Factory presets bundled with the plugin.
//
//  Each preset is a complete "idea machine" setup: a key/scale, a pattern
//  character, a synth voice and a light character treatment.  The XML state is
//  embedded in the binary so the presets are always available, installed to the
//  user preset directory and used as a fallback when a file is missing.
//==============================================================================
namespace vespera::factory
{
    inline constexpr int kFactoryPresetVersion = 6;

    struct Preset
    {
        const char* name;
        const char* xml;
    };

    inline const Preset* presets (int& count)
    {
        static const Preset kPresets[] =
        {
            { "First Light", R"VESPERA(
<PARAMETERS presetName="First Light">
  <PARAM id="accent" value="0.3"/>
  <PARAM id="attack" value="0.005"/>
  <PARAM id="contour" value="2.0"/>
  <PARAM id="crush" value="0.0"/>
  <PARAM id="decay" value="0.25"/>
  <PARAM id="density" value="0.7"/>
  <PARAM id="detune" value="0.15"/>
  <PARAM id="drive" value="0.1"/>
  <PARAM id="filterCutoff" value="6000.0"/>
  <PARAM id="filterEnv" value="0.2"/>
  <PARAM id="filterReso" value="0.15"/>
  <PARAM id="filterType" value="1.0"/>
  <PARAM id="gate" value="0.45"/>
  <PARAM id="key" value="0.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="2.0"/>
  <PARAM id="lfoRateHz" value="1.0"/>
  <PARAM id="lfoShape" value="0.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.0"/>
  <PARAM id="ring" value="0.0"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.1"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.0"/>
  <PARAM id="octaves" value="2.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="4.0"/>
  <PARAM id="rate" value="4.0"/>
  <PARAM id="release" value="0.4"/>
  <PARAM id="scale" value="1.0"/>
  <PARAM id="steps" value="8.0"/>
  <PARAM id="sustain" value="0.55"/>
  <PARAM id="swing" value="0.1"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="1.0"/>
  <PARAM id="wow" value="0.1"/>
</PARAMETERS>
)VESPERA" },

            { "Rain Arp", R"VESPERA(
<PARAMETERS presetName="Rain Arp">
  <PARAM id="accent" value="0.4"/>
  <PARAM id="attack" value="0.002"/>
  <PARAM id="contour" value="0.0"/>
  <PARAM id="crush" value="0.0"/>
  <PARAM id="decay" value="0.12"/>
  <PARAM id="density" value="0.85"/>
  <PARAM id="detune" value="0.08"/>
  <PARAM id="drive" value="0.0"/>
  <PARAM id="filterCutoff" value="9000.0"/>
  <PARAM id="filterEnv" value="0.35"/>
  <PARAM id="filterReso" value="0.2"/>
  <PARAM id="filterType" value="1.0"/>
  <PARAM id="gate" value="0.3"/>
  <PARAM id="key" value="4.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="3.0"/>
  <PARAM id="lfoRateHz" value="1.5"/>
  <PARAM id="lfoShape" value="1.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.0"/>
  <PARAM id="ring" value="0.2"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.15"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.05"/>
  <PARAM id="octaves" value="2.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="5.0"/>
  <PARAM id="rate" value="4.0"/>
  <PARAM id="release" value="0.25"/>
  <PARAM id="scale" value="8.0"/>
  <PARAM id="steps" value="12.0"/>
  <PARAM id="sustain" value="0.35"/>
  <PARAM id="swing" value="0.0"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="0.0"/>
  <PARAM id="wow" value="0.05"/>
</PARAMETERS>
)VESPERA" },

            { "Broken Music Box", R"VESPERA(
<PARAMETERS presetName="Broken Music Box">
  <PARAM id="accent" value="0.25"/>
  <PARAM id="attack" value="0.004"/>
  <PARAM id="contour" value="4.0"/>
  <PARAM id="crush" value="0.25"/>
  <PARAM id="decay" value="0.35"/>
  <PARAM id="density" value="0.6"/>
  <PARAM id="detune" value="0.3"/>
  <PARAM id="drive" value="0.2"/>
  <PARAM id="filterCutoff" value="4500.0"/>
  <PARAM id="filterEnv" value="0.15"/>
  <PARAM id="filterReso" value="0.25"/>
  <PARAM id="filterType" value="1.0"/>
  <PARAM id="gate" value="0.7"/>
  <PARAM id="key" value="9.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="2.0"/>
  <PARAM id="lfoRateHz" value="0.6"/>
  <PARAM id="lfoShape" value="5.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.2"/>
  <PARAM id="ring" value="0.0"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.2"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.12"/>
  <PARAM id="octaves" value="2.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="5.0"/>
  <PARAM id="rate" value="3.0"/>
  <PARAM id="release" value="0.6"/>
  <PARAM id="scale" value="1.0"/>
  <PARAM id="steps" value="6.0"/>
  <PARAM id="sustain" value="0.45"/>
  <PARAM id="swing" value="0.3"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="1.0"/>
  <PARAM id="wow" value="0.35"/>
</PARAMETERS>
)VESPERA" },

            { "Deep Pulse", R"VESPERA(
<PARAMETERS presetName="Deep Pulse">
  <PARAM id="accent" value="0.5"/>
  <PARAM id="attack" value="0.01"/>
  <PARAM id="contour" value="1.0"/>
  <PARAM id="crush" value="0.0"/>
  <PARAM id="decay" value="0.4"/>
  <PARAM id="density" value="0.55"/>
  <PARAM id="detune" value="0.12"/>
  <PARAM id="drive" value="0.35"/>
  <PARAM id="filterCutoff" value="900.0"/>
  <PARAM id="filterEnv" value="0.4"/>
  <PARAM id="filterReso" value="0.3"/>
  <PARAM id="filterType" value="1.0"/>
  <PARAM id="gate" value="0.8"/>
  <PARAM id="key" value="2.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="1.0"/>
  <PARAM id="lfoRateHz" value="0.5"/>
  <PARAM id="lfoShape" value="1.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.0"/>
  <PARAM id="ring" value="0.15"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.25"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.0"/>
  <PARAM id="octaves" value="1.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="3.0"/>
  <PARAM id="rate" value="3.0"/>
  <PARAM id="release" value="0.8"/>
  <PARAM id="scale" value="2.0"/>
  <PARAM id="steps" value="8.0"/>
  <PARAM id="sustain" value="0.7"/>
  <PARAM id="swing" value="0.15"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="2.0"/>
  <PARAM id="wow" value="0.1"/>
</PARAMETERS>
)VESPERA" },

            { "Glass Steps", R"VESPERA(
<PARAMETERS presetName="Glass Steps">
  <PARAM id="accent" value="0.2"/>
  <PARAM id="attack" value="0.01"/>
  <PARAM id="contour" value="2.0"/>
  <PARAM id="crush" value="0.0"/>
  <PARAM id="decay" value="0.5"/>
  <PARAM id="density" value="0.5"/>
  <PARAM id="detune" value="0.05"/>
  <PARAM id="drive" value="0.0"/>
  <PARAM id="filterCutoff" value="3000.0"/>
  <PARAM id="filterEnv" value="0.1"/>
  <PARAM id="filterReso" value="0.2"/>
  <PARAM id="filterType" value="3.0"/>
  <PARAM id="gate" value="0.25"/>
  <PARAM id="key" value="7.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="3.0"/>
  <PARAM id="lfoRateHz" value="2.0"/>
  <PARAM id="lfoShape" value="2.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.1"/>
  <PARAM id="ring" value="0.0"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.15"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.03"/>
  <PARAM id="octaves" value="3.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="6.0"/>
  <PARAM id="rate" value="4.0"/>
  <PARAM id="release" value="0.8"/>
  <PARAM id="scale" value="4.0"/>
  <PARAM id="steps" value="16.0"/>
  <PARAM id="sustain" value="0.25"/>
  <PARAM id="swing" value="0.0"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="0.0"/>
  <PARAM id="wow" value="0.08"/>
</PARAMETERS>
)VESPERA" },

            { "Tape Lullaby", R"VESPERA(
<PARAMETERS presetName="Tape Lullaby">
  <PARAM id="accent" value="0.15"/>
  <PARAM id="attack" value="0.08"/>
  <PARAM id="contour" value="0.0"/>
  <PARAM id="crush" value="0.08"/>
  <PARAM id="decay" value="0.6"/>
  <PARAM id="density" value="0.9"/>
  <PARAM id="detune" value="0.25"/>
  <PARAM id="drive" value="0.2"/>
  <PARAM id="filterCutoff" value="3000.0"/>
  <PARAM id="filterEnv" value="0.0"/>
  <PARAM id="filterReso" value="0.1"/>
  <PARAM id="filterType" value="1.0"/>
  <PARAM id="gate" value="0.9"/>
  <PARAM id="key" value="5.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="1.0"/>
  <PARAM id="lfoRateHz" value="0.15"/>
  <PARAM id="lfoShape" value="0.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.15"/>
  <PARAM id="ring" value="0.1"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.2"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.15"/>
  <PARAM id="octaves" value="1.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="3.0"/>
  <PARAM id="rate" value="2.0"/>
  <PARAM id="release" value="1.2"/>
  <PARAM id="scale" value="0.0"/>
  <PARAM id="steps" value="4.0"/>
  <PARAM id="sustain" value="0.8"/>
  <PARAM id="swing" value="0.4"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="1.0"/>
  <PARAM id="wow" value="0.6"/>
</PARAMETERS>
)VESPERA" },

            { "Acid Walk", R"VESPERA(
<PARAMETERS presetName="Acid Walk">
  <PARAM id="accent" value="0.6"/>
  <PARAM id="attack" value="0.002"/>
  <PARAM id="contour" value="4.0"/>
  <PARAM id="crush" value="0.0"/>
  <PARAM id="decay" value="0.18"/>
  <PARAM id="density" value="0.75"/>
  <PARAM id="detune" value="0.05"/>
  <PARAM id="drive" value="0.5"/>
  <PARAM id="filterCutoff" value="1200.0"/>
  <PARAM id="filterEnv" value="0.6"/>
  <PARAM id="filterReso" value="0.7"/>
  <PARAM id="filterType" value="1.0"/>
  <PARAM id="gate" value="0.4"/>
  <PARAM id="key" value="9.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="4.0"/>
  <PARAM id="lfoRateHz" value="3.0"/>
  <PARAM id="lfoShape" value="4.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.3"/>
  <PARAM id="ring" value="0.0"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.3"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.0"/>
  <PARAM id="octaves" value="1.0"/>
  <PARAM id="output" value="-2.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="4.0"/>
  <PARAM id="rate" value="4.0"/>
  <PARAM id="release" value="0.15"/>
  <PARAM id="scale" value="3.0"/>
  <PARAM id="steps" value="8.0"/>
  <PARAM id="sustain" value="0.3"/>
  <PARAM id="swing" value="0.0"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="2.0"/>
  <PARAM id="wow" value="0.0"/>
</PARAMETERS>
)VESPERA" },

            { "Frozen Motif", R"VESPERA(
<PARAMETERS presetName="Frozen Motif">
  <PARAM id="accent" value="0.2"/>
  <PARAM id="attack" value="0.2"/>
  <PARAM id="contour" value="3.0"/>
  <PARAM id="crush" value="0.0"/>
  <PARAM id="decay" value="0.8"/>
  <PARAM id="density" value="0.65"/>
  <PARAM id="detune" value="0.2"/>
  <PARAM id="drive" value="0.1"/>
  <PARAM id="filterCutoff" value="2000.0"/>
  <PARAM id="filterEnv" value="0.0"/>
  <PARAM id="filterReso" value="0.45"/>
  <PARAM id="filterType" value="2.0"/>
  <PARAM id="gate" value="0.5"/>
  <PARAM id="key" value="11.0"/>
  <PARAM id="latch" value="0.0"/>
  <PARAM id="lfoRate" value="0.0"/>
  <PARAM id="lfoRateHz" value="0.08"/>
  <PARAM id="lfoShape" value="5.0"/>
  <PARAM id="lfoSync" value="0.0"/>
  <PARAM id="fold" value="0.0"/>
  <PARAM id="ring" value="0.25"/>
  <PARAM id="keyLock" value="0.0"/>
  <PARAM id="monitor" value="0.0"/>
  <PARAM id="motion" value="0.35"/>
  <PARAM id="mutate" value="0.0"/>
  <PARAM id="noise" value="0.02"/>
  <PARAM id="octaves" value="2.0"/>
  <PARAM id="output" value="0.0"/>
  <PARAM id="limiter" value="1.0"/>
  <PARAM id="poolSize" value="5.0"/>
  <PARAM id="rate" value="3.0"/>
  <PARAM id="release" value="1.5"/>
  <PARAM id="scale" value="1.0"/>
  <PARAM id="steps" value="5.0"/>
  <PARAM id="sustain" value="0.5"/>
  <PARAM id="swing" value="0.2"/>
  <PARAM id="transpose" value="0.0"/>
  <PARAM id="wave" value="3.0"/>
  <PARAM id="wow" value="0.2"/>
</PARAMETERS>
)VESPERA" }
        };

        count = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
        return kPresets;
    }

    inline const char* defaultPresetName() { return "First Light"; }

    inline const char* findXml (const juce::String& name)
    {
        int n = 0;
        const auto* p = presets (n);
        for (int i = 0; i < n; ++i)
            if (name == p[i].name)
                return p[i].xml;
        return nullptr;
    }
}
