#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
    const juce::Colour kBg       = juce::Colour (0xff0a0a10); // near-black
    const juce::Colour kText     = juce::Colour (0xfff0f0f5);
    const juce::Colour kTextDim  = juce::Colour (0xff8a8a99);
    const juce::Colour kAccent   = juce::Colour (0xff8b7cf6); // deep violet
    const juce::Colour kAccentHi = juce::Colour (0xffb3a6ff);
    const juce::Colour kGlowA    = juce::Colour (0xff2a1a6e); // deep purple glow
    const juce::Colour kGlowB    = juce::Colour (0xff1a1a4e); // deep blue glow
    const juce::Colour kKnobTrack= juce::Colour (0xff23232e);
    const juce::Colour kHeartOn  = juce::Colour (0xffff4d6d);

    std::vector<QSubFactoryPreset> makeFactoryPresets()
    {
        return {
            { "Init",           70,  0, 10, 120,   5, 200, true,  9 },
            { "808 Boom",       90, 20, 25, 100,   2, 400, true,  9 },
            { "Subtle Weight",  40,  0,  5,  80,  10, 300, true,  9 },
            { "Distorted 808",  85, 60, 70, 200,   2, 250, true,  9 },
            { "Sine Punch",     80,  0, 15, 150,   1, 150, true,  9 },
            { "Fixed C1",       75, 10, 10, 120,   5, 200, false, 0 },
        };
    }
}

//==============================================================================
void QSubLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                       float sliderPos, float rotaryStartAngle,
                                       float rotaryEndAngle, juce::Slider&)
{
    const float pad = heroMode ? 14.0f : 8.0f;
    auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (pad);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const float arcThick = heroMode ? 12.0f : 5.0f;

    // Track arc.
    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (kKnobTrack);
    g.strokePath (track, juce::PathStrokeType (arcThick, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Value arc with glow for the hero.
    if (sliderPos > 0.001f)
    {
        if (heroMode)
        {
            g.setColour (kAccent.withAlpha (0.35f));
            juce::Path glow;
            glow.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                rotaryStartAngle, angle, true);
            g.strokePath (glow, juce::PathStrokeType (arcThick + 10.0f,
                          juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                             rotaryStartAngle, angle, true);
        g.setColour (heroMode ? kAccentHi : kAccent);
        g.strokePath (value, juce::PathStrokeType (arcThick, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // Knob body.
    const float bodyR = radius - arcThick - (heroMode ? 6.0f : 4.0f);
    g.setColour (juce::Colour (0xff14141c));
    g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colour (0xff2e2e3a));
    g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.5f);

    // Pointer.
    const float pr = bodyR * 0.75f;
    juce::Path pointer;
    pointer.addRectangle (-1.5f, -pr, 3.0f, pr * 0.55f);
    pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
    g.setColour (heroMode ? kAccentHi : kText);
    g.fillPath (pointer);
}

//==============================================================================
QSubEditor::QSubEditor (QSubProcessor& p)
    : AudioProcessorEditor (p), processor (p)
{
    setSize (560, 640);
    setLookAndFeel (&lookAndFeel);
    heroLookAndFeel.heroMode = true;

    logoImage = juce::ImageCache::getFromMemory (BinaryData::headroomlogo_png,
                                                BinaryData::headroomlogo_pngSize);

    // ---- header ----
    titleLabel.setText ("QSUB", juce::dontSendNotification);
    titleLabel.setFont (juce::Font (30.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, kText);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("808 SUB GENERATOR", juce::dontSendNotification);
    subtitleLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    subtitleLabel.setColour (juce::Label::textColourId, kTextDim);
    addAndMakeVisible (subtitleLabel);

    // ---- preset bar ----
    presetBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff14141c));
    presetBox.setColour (juce::ComboBox::textColourId, kText);
    presetBox.setColour (juce::ComboBox::arrowColourId, kAccent);
    presetBox.onChange = [this]
    {
        const int id = presetBox.getSelectedId();
        if (id >= 900 && id < 1000)
        {
            // Favorite shortcut: resolve by name (strip the ♥ prefix).
            auto name = presetBox.getText().removeCharacters ("\xE2\x99\xA5 ").trim();
            for (int i = 0; i < (int) factoryPresets.size(); ++i)
            {
                if (juce::String (factoryPresets[(size_t) i].name) == name)
                {
                    applyFactoryPreset (i);
                    break;
                }
            }
            for (int i = 0; i < userPresetNames.size(); ++i)
            {
                if (userPresetNames[i] == name)
                {
                    auto f = getPresetDir().getChildFile (name + ".qsubpreset");
                    if (f.existsAsFile())
                        if (auto xml = juce::XmlDocument::parse (f))
                            processor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
                    break;
                }
            }
        }
        else if (id >= 1 && id <= (int) factoryPresets.size())
            applyFactoryPreset (id - 1);
        else if (id > 1000)
        {
            const int ui = id - 1001;
            if (ui >= 0 && ui < userPresetNames.size())
            {
                auto f = getPresetDir().getChildFile (userPresetNames[ui] + ".qsubpreset");
                if (f.existsAsFile())
                    if (auto xml = juce::XmlDocument::parse (f))
                        processor.apvts.replaceState (juce::ValueTree::fromXml (*xml));
            }
        }
        refreshFavoriteButton();
    };
    addAndMakeVisible (presetBox);

    heartButton.setColour (juce::TextButton::textColourOffId, kTextDim);
    heartButton.setColour (juce::TextButton::textColourOnId, kHeartOn);
    heartButton.setClickingTogglesState (true);
    heartButton.onClick = [this] { toggleFavorite(); };
    addAndMakeVisible (heartButton);

    prevButton.onClick = [this]
    {
        const int n = presetBox.getNumItems();
        if (n > 0) presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() - 1 + n) % n);
    };
    nextButton.onClick = [this]
    {
        const int n = presetBox.getNumItems();
        if (n > 0) presetBox.setSelectedItemIndex ((presetBox.getSelectedItemIndex() + 1) % n);
    };
    addAndMakeVisible (prevButton);
    addAndMakeVisible (nextButton);

    saveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1c1c26));
    saveButton.setColour (juce::TextButton::textColourOffId, kAccentHi);
    saveButton.onClick = [this] { promptSavePreset(); };
    addAndMakeVisible (saveButton);

    factoryPresets = makeFactoryPresets();
    buildPresetMenu();

    // ---- hero knob ----
    heroKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    heroKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    heroKnob.setLookAndFeel (&heroLookAndFeel);
    heroKnob.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                  juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (heroKnob);

    heroLabel.setText ("SUB LEVEL", juce::dontSendNotification);
    heroLabel.setFont (juce::Font (13.0f, juce::Font::bold));
    heroLabel.setColour (juce::Label::textColourId, kTextDim);
    heroLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (heroLabel);

    heroValueLabel.setFont (juce::Font (22.0f, juce::Font::bold));
    heroValueLabel.setColour (juce::Label::textColourId, kText);
    heroValueLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (heroValueLabel);

    detectedLabel.setFont (juce::Font (13.0f));
    detectedLabel.setColour (juce::Label::textColourId, kAccentHi);
    detectedLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (detectedLabel);

    // ---- 5 small supporting knobs ----
    auto setupSmall = [this] (juce::Slider& s, juce::Label& l, const juce::String& text)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setLookAndFeel (&lookAndFeel);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
        addAndMakeVisible (s);
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::Font (10.0f, juce::Font::bold));
        l.setColour (juce::Label::textColourId, kTextDim);
        l.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (l);
    };
    setupSmall (waveKnob, waveLabel, "WAVEFORM");
    setupSmall (driveKnob, driveLabel, "DRIVE");
    setupSmall (cutoffKnob, cutoffLabel, "CUTOFF");
    setupSmall (attackKnob, attackLabel, "ATTACK");
    setupSmall (releaseKnob, releaseLabel, "RELEASE");

    // ---- bottom row ----
    detectButton.setClickingTogglesState (true);
    detectButton.setColour (juce::ToggleButton::textColourId, kText);
    detectButton.setColour (juce::ToggleButton::tickColourId, kAccent);
    detectButton.onClick = [this] { keyBox.setEnabled (! detectButton.getToggleState()); };
    addAndMakeVisible (detectButton);

    keyLabel.setFont (juce::Font (11.0f, juce::Font::bold));
    keyLabel.setColour (juce::Label::textColourId, kTextDim);
    addAndMakeVisible (keyLabel);

    for (int n = 0; n < 12; ++n)
        keyBox.addItem (juce::String (QSubProcessor::noteNames[n]) + "1", n + 1);
    keyBox.setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff14141c));
    keyBox.setColour (juce::ComboBox::textColourId, kText);
    addAndMakeVisible (keyBox);

    // ---- attachments ----
    heroAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QSubProcessor::kSubLevel, heroKnob);
    waveAtt    = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QSubProcessor::kWaveBlend, waveKnob);
    driveAtt   = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QSubProcessor::kDrive, driveKnob);
    cutoffAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QSubProcessor::kCutoff, cutoffKnob);
    attackAtt  = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QSubProcessor::kAttack, attackKnob);
    releaseAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        processor.apvts, QSubProcessor::kRelease, releaseKnob);
    detectAtt  = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        processor.apvts, QSubProcessor::kPitchDetect, detectButton);
    keyAtt     = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
        processor.apvts, QSubProcessor::kKeyNote, keyBox);
    keyBox.setEnabled (! detectButton.getToggleState());

    startTimerHz (15);
}

QSubEditor::~QSubEditor()
{
    setLookAndFeel (nullptr);
    heroKnob.setLookAndFeel (nullptr);
    waveKnob.setLookAndFeel (nullptr);
    driveKnob.setLookAndFeel (nullptr);
    cutoffKnob.setLookAndFeel (nullptr);
    attackKnob.setLookAndFeel (nullptr);
    releaseKnob.setLookAndFeel (nullptr);
}

void QSubEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Deep blue/purple radial glow behind the hero knob.
    const float cx = 280.0f, cy = 245.0f, r = 175.0f;
    juce::ColourGradient glow (kGlowA.withAlpha (0.55f), { cx, cy },
                               kBg.withAlpha (0.0f), { cx + r, cy }, true);
    glow.addColour (0.55, kGlowB.withAlpha (0.30f));
    g.setGradientFill (glow);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);

    // Small logo, top-left.
    if (logoImage.isValid())
        g.drawImage (logoImage, 16.0f, 14.0f, 34.0f, 34.0f,
                     0, 0, logoImage.getWidth(), logoImage.getHeight());

    // Thin divider under the preset bar.
    g.setColour (juce::Colour (0xff1c1c26));
    g.fillRect (0, 104, getWidth(), 1);
}

void QSubEditor::resized()
{
    // Header
    titleLabel.setBounds (58, 8, 200, 34);
    subtitleLabel.setBounds (60, 40, 200, 16);

    // Preset bar
    presetBox.setBounds (16, 66, 220, 30);
    heartButton.setBounds (244, 66, 38, 30);
    prevButton.setBounds (288, 66, 34, 30);
    nextButton.setBounds (326, 66, 34, 30);
    saveButton.setBounds (368, 66, 70, 30);

    // Hero knob — centered, dominating.
    heroKnob.setBounds (180, 140, 200, 200);
    heroLabel.setBounds (180, 344, 200, 18);
    heroValueLabel.setBounds (180, 362, 200, 28);
    detectedLabel.setBounds (130, 394, 300, 20);

    // 5 small knobs.
    const int kw = 96, kx0 = 28, ky = 430;
    waveKnob.setBounds (kx0, ky, kw, kw);
    waveLabel.setBounds (kx0, ky + kw + 2, kw, 16);
    driveKnob.setBounds (kx0 + 104, ky, kw, kw);
    driveLabel.setBounds (kx0 + 104, ky + kw + 2, kw, 16);
    cutoffKnob.setBounds (kx0 + 208, ky, kw, kw);
    cutoffLabel.setBounds (kx0 + 208, ky + kw + 2, kw, 16);
    attackKnob.setBounds (kx0 + 312, ky, kw, kw);
    attackLabel.setBounds (kx0 + 312, ky + kw + 2, kw, 16);
    releaseKnob.setBounds (kx0 + 416, ky, kw, kw);
    releaseLabel.setBounds (kx0 + 416, ky + kw + 2, kw, 16);

    // Bottom row.
    detectButton.setBounds (28, 576, 150, 30);
    keyLabel.setBounds (200, 576, 40, 30);
    keyBox.setBounds (240, 576, 110, 30);
}

void QSubEditor::timerCallback()
{
    const float v = processor.apvts.getRawParameterValue (QSubProcessor::kSubLevel)->load();
    heroValueLabel.setText (juce::String (v, 1) + " %", juce::dontSendNotification);

    const float f = processor.getDetectedFrequency();
    if (f > 20.0f)
        detectedLabel.setText ("\xE2\x99\xAA " + processor.getDetectedNoteName()
                               + "  ·  " + juce::String (f, 1) + " Hz",
                               juce::dontSendNotification);
    else
        detectedLabel.setText ("--", juce::dontSendNotification);
}

//==============================================================================
// Preset system
juce::File QSubEditor::getPresetDir() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Headroom Audio").getChildFile ("QSub").getChildFile ("Presets");
}

juce::File QSubEditor::getFavoritesFile() const
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Headroom Audio").getChildFile ("QSub").getChildFile ("favorites.txt");
}

juce::StringArray QSubEditor::loadFavorites() const
{
    juce::StringArray favs;
    auto f = getFavoritesFile();
    if (f.existsAsFile())
        favs.addLines (f.loadFileAsString());
    favs.removeEmptyStrings();
    return favs;
}

void QSubEditor::saveFavorites (const juce::StringArray& favs) const
{
    auto f = getFavoritesFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (favs.joinIntoString ("\n"));
}

juce::String QSubEditor::currentPresetName() const
{
    return presetBox.getText().trim();
}

void QSubEditor::buildPresetMenu()
{
    presetBox.clear();
    userPresetNames.clear();
    int id = 1;

    // Favorites section first.
    const auto favs = loadFavorites();
    juce::StringArray allNames;
    for (auto& fp : factoryPresets) allNames.add (fp.name);
    auto dir = getPresetDir();
    juce::Array<juce::File> userFiles;
    if (dir.isDirectory())
        dir.findChildFiles (userFiles, juce::File::findFiles, false, "*.qsubpreset");
    for (auto& uf : userFiles) allNames.add (uf.getFileNameWithoutExtension());

    juce::StringArray favSection;
    for (auto& n : allNames)
        if (favs.contains (n)) favSection.add (n);
    if (favSection.size() > 0)
    {
        presetBox.addSectionHeading ("♥ Favorites");
        for (auto& n : favSection)
        {
            presetBox.addItem ("\xE2\x99\xA5 " + n, 900 + favSection.indexOf (n));
        }
        presetBox.addSeparator();
    }

    presetBox.addSectionHeading ("Factory");
    for (auto& fp : factoryPresets)
        presetBox.addItem (fp.name, id++);

    presetBox.addSeparator();
    presetBox.addSectionHeading ("User");
    int uid = 1001;
    for (auto& uf : userFiles)
    {
        userPresetNames.add (uf.getFileNameWithoutExtension());
        presetBox.addItem (uf.getFileNameWithoutExtension(), uid++);
    }

    presetBox.setSelectedId (1, juce::dontSendNotification);
    refreshFavoriteButton();
}

void QSubEditor::applyPresetValues (float subLevel, float waveBlend, float drive,
                                   float cutoff, float attack, float release,
                                   bool pitchDetect, int keyNote)
{
    auto setFloat = [this] (const char* pid, float v)
    {
        if (auto* p = processor.apvts.getParameter (pid))
            p->setValueNotifyingHost (p->convertTo0to1 (v));
    };
    setFloat (QSubProcessor::kSubLevel, subLevel);
    setFloat (QSubProcessor::kWaveBlend, waveBlend);
    setFloat (QSubProcessor::kDrive, drive);
    setFloat (QSubProcessor::kCutoff, cutoff);
    setFloat (QSubProcessor::kAttack, attack);
    setFloat (QSubProcessor::kRelease, release);
    if (auto* p = processor.apvts.getParameter (QSubProcessor::kPitchDetect))
        p->setValueNotifyingHost (pitchDetect ? 1.0f : 0.0f);
    if (auto* p = processor.apvts.getParameter (QSubProcessor::kKeyNote))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) keyNote));
}

void QSubEditor::applyFactoryPreset (int index)
{
    if (index < 0 || index >= (int) factoryPresets.size()) return;
    const auto& fp = factoryPresets[(size_t) index];
    applyPresetValues (fp.subLevel, fp.waveBlend, fp.drive, fp.cutoff,
                       fp.attack, fp.release, fp.pitchDetect, fp.keyNote);
}

void QSubEditor::promptSavePreset()
{
    auto* aw = new juce::AlertWindow ("Save Preset", "Name your preset:",
                                      juce::AlertWindow::NoIcon);
    aw->addTextEditor ("presetName", "My Preset");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<QSubEditor> safeThis (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [aw, safeThis] (int result)
        {
            const juce::String name = aw->getTextEditorContents ("presetName").trim();
            const bool doSave = (result == 1 && safeThis != nullptr && name.isNotEmpty());
            delete aw;
            if (doSave)
                safeThis->saveUserPreset (name);
        }));
}

void QSubEditor::saveUserPreset (const juce::String& name)
{
    auto dir = getPresetDir();
    dir.createDirectory();
    juce::String safe = name.replaceCharacter ('/', '_').replaceCharacter ('\\', '_');
    auto f = dir.getChildFile (safe + ".qsubpreset");
    if (auto xml = processor.apvts.copyState().createXml())
        f.replaceWithText (xml->toString());

    buildPresetMenu();
    for (int i = 0; i < presetBox.getNumItems(); ++i)
    {
        if (presetBox.getItemText (i) == safe)
        {
            presetBox.setSelectedItemIndex (i);
            break;
        }
    }
}

void QSubEditor::toggleFavorite()
{
    const auto name = currentPresetName().removeCharacters ("\xE2\x99\xA5 ").trim();
    if (name.isEmpty())
        return;

    auto favs = loadFavorites();
    if (favs.contains (name))
        favs.removeString (name);
    else
        favs.add (name);
    saveFavorites (favs);

    const int selId = presetBox.getSelectedId();
    buildPresetMenu();
    presetBox.setSelectedId (selId, juce::dontSendNotification);
    refreshFavoriteButton();
}

void QSubEditor::refreshFavoriteButton()
{
    const auto name = currentPresetName().removeCharacters ("\xE2\x99\xA5 ").trim();
    heartButton.setToggleState (loadFavorites().contains (name) && name.isNotEmpty(),
                                juce::dontSendNotification);
}
