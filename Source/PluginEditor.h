#pragma once

#include "PluginProcessor.h"
#include <memory>
#include <vector>

//==============================================================================
// Small struct describing a factory preset (parameter values by ID).
struct QSubFactoryPreset
{
    const char* name;
    float subLevel, waveBlend, drive, cutoff, attack, release;
    bool pitchDetect;
    int keyNote;
};

//==============================================================================
// Custom look-and-feel: hero knob (large, glowing arc) + small knobs.
class QSubLookAndFeel : public juce::LookAndFeel_V4
{
public:
    bool heroMode = false; // set true when drawing the hero knob

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;
};

//==============================================================================
class QSubEditor : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    explicit QSubEditor (QSubProcessor&);
    ~QSubEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override; // 15 Hz: refresh detected-note readout

    // ---- preset system ----
    void buildPresetMenu();
    void applyPresetValues (float subLevel, float waveBlend, float drive, float cutoff,
                            float attack, float release, bool pitchDetect, int keyNote);
    void applyFactoryPreset (int index);
    void promptSavePreset();
    void saveUserPreset (const juce::String& name);
    void toggleFavorite();
    void refreshFavoriteButton();
    juce::File getPresetDir() const;
    juce::File getFavoritesFile() const;
    juce::StringArray loadFavorites() const;
    void saveFavorites (const juce::StringArray&) const;
    juce::String currentPresetName() const;

    QSubProcessor& processor;
    QSubLookAndFeel lookAndFeel;
    QSubLookAndFeel heroLookAndFeel;

    juce::Image logoImage;

    // Header
    juce::Label titleLabel, subtitleLabel;

    // Preset bar
    juce::ComboBox presetBox;
    juce::TextButton prevButton { "<" }, nextButton { ">" };
    juce::TextButton heartButton { juce::String::fromUTF8 ("\xe2\x99\xa5") }; // ♥
    juce::TextButton saveButton { "SAVE" };

    // Hero
    juce::Slider heroKnob;
    juce::Label heroLabel, heroValueLabel;
    juce::Label detectedLabel; // "♪ A1 · 55.0 Hz"

    // Supporting knobs (5 max — Cymatics minimal)
    juce::Slider waveKnob, driveKnob, cutoffKnob, attackKnob, releaseKnob;
    juce::Label waveLabel, driveLabel, cutoffLabel, attackLabel, releaseLabel;

    // Bottom row
    juce::ToggleButton detectButton { "PITCH DETECT" };
    juce::ComboBox keyBox;
    juce::Label keyLabel { {}, "KEY" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> heroAtt, waveAtt, driveAtt, cutoffAtt, attackAtt, releaseAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> detectAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> keyAtt;

    std::vector<QSubFactoryPreset> factoryPresets;
    juce::StringArray userPresetNames; // parallel to dropdown IDs after factories

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QSubEditor)
};
