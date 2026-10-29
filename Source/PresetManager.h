#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FactoryPresets.h"

//==============================================================================
//  Simple file-based preset manager.
//
//  Presets are stored as XML files (one per preset) in a per-user folder.
//  Saving copies the whole APVTS state; loading replaces it (which updates all
//  parameter attachments automatically).  Factory presets are embedded in the
//  binary and always available.
//==============================================================================
class PresetManager
{
public:
    static constexpr const char* extension = ".vesperapreset";

    explicit PresetManager (juce::AudioProcessorValueTreeState& state,
                            juce::File directory = {})
        : apvts (state)
    {
        dir = directory != juce::File() ? directory : getAppDir().getChildFile ("Presets");
    }

    static juce::File getAppDir()
    {
       #if JUCE_MAC
        return juce::File ("~/Library/Application Support/Vespera");
       #else
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("Vespera");
       #endif
    }

    juce::File getDirectory() const { return dir; }

    static juce::String getDefaultPresetName() { return vespera::factory::defaultPresetName(); }

    void installFactoryPresets() const
    {
        dir.createDirectory();

        auto stamp = dir.getChildFile ("factory_version.txt");
        const int installedVersion = stamp.existsAsFile()
                                         ? stamp.loadFileAsString().getIntValue()
                                         : 0;
        const bool refreshAll = installedVersion < vespera::factory::kFactoryPresetVersion;

        int count = 0;
        const auto* presets = vespera::factory::presets (count);
        for (int i = 0; i < count; ++i)
        {
            auto file = fileForName (presets[i].name);
            if (refreshAll || ! file.existsAsFile())
                file.replaceWithText (presets[i].xml);
        }

        stamp.replaceWithText (juce::String (vespera::factory::kFactoryPresetVersion));
    }

    juce::StringArray getPresetNames() const
    {
        juce::StringArray names;

        int count = 0;
        const auto* presets = vespera::factory::presets (count);
        for (int i = 0; i < count; ++i)
            names.addIfNotAlreadyThere (presets[i].name);

        if (dir.isDirectory())
            for (const auto& e : juce::RangedDirectoryIterator (dir, false, juce::String ("*") + extension))
                names.addIfNotAlreadyThere (e.getFile().getFileNameWithoutExtension());

        names.sort (true);
        return names;
    }

    bool savePreset (const juce::String& name)
    {
        const auto safe = sanitise (name);
        if (safe.isEmpty())
            return false;

        dir.createDirectory();
        auto xml = apvts.copyState().createXml();
        if (xml == nullptr)
            return false;

        xml->setAttribute ("presetName", safe);
        return xml->writeTo (fileForName (safe));
    }

    bool loadPreset (const juce::String& name)
    {
        const auto safe = sanitise (name);

        const auto file = fileForName (safe);
        if (file.existsAsFile())
            if (auto xml = juce::XmlDocument::parse (file))
                if (xml->hasTagName (apvts.state.getType()))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    return true;
                }

        if (const char* embedded = vespera::factory::findXml (safe))
            if (auto xml = juce::XmlDocument::parse (juce::String (embedded)))
                if (xml->hasTagName (apvts.state.getType()))
                {
                    apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    return true;
                }

        return false;
    }

    juce::String getLastPreset() const { return lastPreset; }
    void setLastPreset (const juce::String& n) { lastPreset = n; }

private:
    static juce::String sanitise (const juce::String& name)
    {
        return name.retainCharacters ("abcdefghijklmnopqrstuvwxyz"
                                      "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                      "0123456789 _-()[].");
    }

    juce::File fileForName (const juce::String& name) const
    {
        return dir.getChildFile (name + extension);
    }

    juce::AudioProcessorValueTreeState& apvts;
    juce::File dir;
    juce::String lastPreset;
};
