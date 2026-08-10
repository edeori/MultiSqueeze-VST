#include "PluginEditor.h"
#include "MultiSqueezeFonts.h"
#include "BinaryData.h"
#include <cmath>
#include <initializer_list>
#include <utility>

namespace
{
    constexpr float referenceWidth = 1180.0f;
    constexpr float referenceHeight = 720.0f;

    juce::String spacedUppercase (const juce::String& source)
    {
        juce::String result;
        const auto upper = source.toUpperCase();
        for (int i = 0; i < upper.length(); ++i)
        {
            result << upper[i];
            if (i + 1 < upper.length()) result << ' ';
        }
        return result;
    }

    void styleCaption (juce::Label& label, const juce::String& text)
    {
        label.setText (spacedUppercase (text), juce::dontSendNotification);
        label.setFont (MultiSqueezeFonts::uiLight (11.0f));
        label.setColour (juce::Label::textColourId, MultiSqueezeLookAndFeel::textDim);
        label.setJustificationType (juce::Justification::centredLeft);
    }
}

MultiSqueezeAudioProcessorEditor::MultiSqueezeAudioProcessorEditor (MultiSqueezeAudioProcessor& owner)
    : AudioProcessorEditor (&owner), audioProcessor (owner), spectrum (owner)
{
    setLookAndFeel (&lookAndFeel);
    backgroundImage = juce::ImageFileFormat::loadFrom (BinaryData::backgroundv2_png, (size_t) BinaryData::backgroundv2_pngSize);
    logoImage = juce::ImageFileFormat::loadFrom (BinaryData::moth_logo_png, (size_t) BinaryData::moth_logo_pngSize);

    for (auto* target : { &logoLayoutTarget, &gainReductionMeterLayoutTarget })
    {
        target->setInterceptsMouseClicks (false, false);
        addAndMakeVisible (*target);
    }

    makerLabel.setText (spacedUppercase ("Moth Production"), juce::dontSendNotification);
    makerLabel.setFont (MultiSqueezeFonts::uiLight (12.0f));
    makerLabel.setColour (juce::Label::textColourId, juce::Colour (0xffa8adb7));
    addAndMakeVisible (makerLabel);

    titleLabel.setText ("MultiSqueeze", juce::dontSendNotification);
    titleLabel.setFont (MultiSqueezeFonts::title (30.0f));
    addAndMakeVisible (titleLabel);
    subtitleLabel.setText (spacedUppercase ("Dynamic multiband compressor"), juce::dontSendNotification);
    subtitleLabel.setFont (MultiSqueezeFonts::uiLight (11.0f));
    subtitleLabel.setColour (juce::Label::textColourId, MultiSqueezeLookAndFeel::textDim);
    addAndMakeVisible (subtitleLabel);
    versionLabel.setText ("v" + juce::String (JucePlugin_VersionString), juce::dontSendNotification);
    versionLabel.setFont (MultiSqueezeFonts::uiLight (12.0f));
    versionLabel.setColour (juce::Label::textColourId, MultiSqueezeLookAndFeel::textDim);
    addAndMakeVisible (versionLabel);

    bypassButton.setClickingTogglesState (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::bypass, bypassButton);

    lookaheadButton.setClickingTogglesState (true);
    lookaheadButton.setButtonText ("LOOKAHEAD  5 ms");
    lookaheadButton.setTooltip ("Delays the audio a few milliseconds so every band's compressor can react just before a transient arrives, instead of after. Adds that same amount of processing latency while enabled.");
    addAndMakeVisible (lookaheadButton);
    lookaheadAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::lookahead, lookaheadButton);

    styleCaption (inspectLabel, "Inspect band");
    addAndMakeVisible (inspectLabel);
    bandLabel.setFont (MultiSqueezeFonts::title (24.0f));
    bandLabel.setColour (juce::Label::textColourId, MultiSqueezeLookAndFeel::accent);
    bandLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (bandLabel);
    rangeLabel.setFont (MultiSqueezeFonts::numeric (13.0f));
    rangeLabel.setColour (juce::Label::textColourId, MultiSqueezeLookAndFeel::textDim);
    rangeLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (rangeLabel);

    for (auto* button : { &previousBandButton, &nextBandButton, &addBandButton, &removeBandButton,
                          &compressorBypassButton, &soloButton, &muteButton, &autoGainButton })
        addAndMakeVisible (*button);
    compressorBypassButton.setClickingTogglesState (true);
    compressorBypassButton.setButtonText ("BYPASS");
    compressorBypassButton.setTooltip ("Bypass compression for the selected band");
    soloButton.setClickingTogglesState (true);
    muteButton.setClickingTogglesState (true);
    autoGainButton.setClickingTogglesState (true);
    autoGainButton.setButtonText ("AUTO");
    autoGainButton.getProperties().set (MultiSqueezeLookAndFeel::smallButtonProperty, true);
    autoGainButton.setTooltip ("Applies make-up gain scaled to how much this band is currently reducing (up to half the maximum, at 0 dBFS). Zero when nothing is being compressed. Perfectly synced to the compressor's own envelope, so it can't lag or pump. Combine with Makeup gain for extra trim.");
    addBandButton.onClick = [this] { audioProcessor.addBand(); refreshBandReadout(); };
    removeBandButton.onClick = [this] { audioProcessor.removeSelectedBand(); refreshBandReadout(); };
    previousBandButton.onClick = [this] { audioProcessor.setSelectedBand (audioProcessor.getSelectedBand() - 1); };
    nextBandButton.onClick = [this] { audioProcessor.setSelectedBand (audioProcessor.getSelectedBand() + 1); };

    configureKnob (bandSliders[0], bandSliderLabels[0], "Threshold", " dB", 1);
    configureKnob (bandSliders[1], bandSliderLabels[1], "Ratio", ":1", 2);
    configureKnob (bandSliders[2], bandSliderLabels[2], "Attack", " ms", 1);
    configureKnob (bandSliders[3], bandSliderLabels[3], "Release", " ms", 0);
    configureKnob (bandSliders[4], bandSliderLabels[4], "Makeup", " dB", 1);
    configureKnob (bandSliders[5], bandSliderLabels[5], "Knee", " dB", 1);
    bandSliders[0].setTooltip ("Lowering Threshold increases gain reduction. Raise Makeup gain, or enable Auto Gain, to trim the level back up.");
    bandSliders[4].setTooltip ("Manual trim applied after compression.");
    bandSliders[5].setTooltip ("Width of the soft-knee transition around Threshold. 0 dB is a hard knee (compression starts abruptly at Threshold); wider values blend in more gradually.");

    styleCaption (detectorLabel, "Detector input");
    addAndMakeVisible (detectorLabel);
    sidechainBox.addItemList ({ "Internal", "Sidechain 1", "Sidechain 2", "Sidechain 3", "Sidechain 4" }, 1);
    sidechainBox.setTooltip ("Each band may use its own external sidechain bus");
    addAndMakeVisible (sidechainBox);

    auto configureEdge = [this] (juce::Label& label)
    {
        label.setFont (MultiSqueezeFonts::numeric (13.0f));
        label.setColour (juce::Label::textColourId, MultiSqueezeLookAndFeel::accent);
        label.setColour (juce::Label::backgroundColourId, juce::Colour (0xff0c0c0e));
        label.setColour (juce::Label::outlineColourId, MultiSqueezeLookAndFeel::accent.withAlpha (0.35f));
        label.setColour (juce::Label::outlineWhenEditingColourId, MultiSqueezeLookAndFeel::accent);
        label.setJustificationType (juce::Justification::centred);
        addAndMakeVisible (label);
    };
    configureEdge (lowEdgeLabel);
    configureEdge (highEdgeLabel);
    lowEdgeLabel.onTextChange = [this]
    {
        if (audioProcessor.getSelectedBand() > 0)
            audioProcessor.setBandUpperFrequency (audioProcessor.getSelectedBand() - 1, parseFrequency (lowEdgeLabel.getText()));
    };
    highEdgeLabel.onTextChange = [this]
    {
        audioProcessor.setBandUpperFrequency (audioProcessor.getSelectedBand(), parseFrequency (highEdgeLabel.getText()));
    };

    gainReductionLabel.setFont (MultiSqueezeFonts::numeric (14.0f));
    gainReductionLabel.setColour (juce::Label::textColourId, juce::Colour (0xffdff47a));
    gainReductionLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (gainReductionLabel);

    styleCaption (spectrumTitle, "Spectrum / crossover");
    addAndMakeVisible (spectrumTitle);
    spectrumHint.setText ("Double-click to add/remove a split | drag split points | drag a band's level line",
                          juce::dontSendNotification);
    spectrumHint.setFont (MultiSqueezeFonts::uiLight (12.0f));
    spectrumHint.setColour (juce::Label::textColourId, MultiSqueezeLookAndFeel::textDim);
    spectrumHint.setJustificationType (juce::Justification::centredRight);
    addChildComponent (spectrumHint);
    addAndMakeVisible (spectrum);

    styleCaption (outputLabel, "Master output");
    addAndMakeVisible (outputLabel);
    outputSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    outputSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 75, 24);
    outputSlider.setTextValueSuffix (" dB");
    outputSlider.setNumDecimalPlacesToDisplay (1);
    addAndMakeVisible (outputSlider);
    outputAttachment = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::output, outputSlider);

    attachSelectedBand();
    setResizable (true, true);
    setResizeLimits (900, 549, 1600, 976);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio ((double) referenceWidth / (double) referenceHeight);
    setSize (1180, 720);

#if defined (JUCE_LAYOUT_TUNER) && JUCE_LAYOUT_TUNER
    layoutTuner = std::make_unique<juce_layout_tuner::Overlay> (
        *this, juce::Rectangle<int> (0, 0, (int) referenceWidth, (int) referenceHeight), 8);

    for (const auto& target : std::initializer_list<std::pair<juce::Component*, const char*>> {
             { &makerLabel, "makerLabel" }, { &titleLabel, "titleLabel" },
             { &subtitleLabel, "subtitleLabel" }, { &versionLabel, "versionLabel" },
             { &bypassButton, "bypassButton" }, { &lookaheadButton, "lookaheadButton" },
             { &inspectLabel, "inspectLabel" }, { &bandLabel, "bandLabel" },
             { &rangeLabel, "rangeLabel" }, { &lowEdgeLabel, "lowEdgeLabel" },
             { &highEdgeLabel, "highEdgeLabel" }, { &detectorLabel, "detectorLabel" },
             { &gainReductionLabel, "gainReductionLabel" }, { &previousBandButton, "previousBandButton" },
             { &nextBandButton, "nextBandButton" }, { &addBandButton, "addBandButton" },
             { &removeBandButton, "removeBandButton" }, { &compressorBypassButton, "compressorBypassButton" },
             { &soloButton, "soloButton" }, { &muteButton, "muteButton" },
             { &autoGainButton, "autoGainButton" }, { &sidechainBox, "sidechainBox" },
             { &spectrumTitle, "spectrumTitle" }, { &spectrum, "spectrum" },
             { &outputLabel, "outputLabel" }, { &outputSlider, "outputSlider" },
             { &logoLayoutTarget, "logoLayoutTarget" },
             { &gainReductionMeterLayoutTarget, "gainReductionMeterLayoutTarget" } })
        layoutTuner->addTarget (*target.first, target.second);

    for (int i = 0; i < (int) bandSliders.size(); ++i)
    {
        layoutTuner->addTarget (bandSliderLabels[(size_t) i], "bandSliderLabels[" + juce::String (i) + "]");
        layoutTuner->addTarget (bandSliders[(size_t) i], "bandSliders[" + juce::String (i) + "]");
    }

    addAndMakeVisible (*layoutTuner);
    layoutTuner->setBounds (getLocalBounds());
    layoutTuner->activate (true);
#endif

    startTimerHz (20);
}

MultiSqueezeAudioProcessorEditor::~MultiSqueezeAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MultiSqueezeAudioProcessorEditor::configureKnob (juce::Slider& slider, juce::Label& label,
                                                       const juce::String& name, const juce::String& suffix,
                                                       int decimals)
{
    styleCaption (label, name);
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 74, 23);
    slider.setTextValueSuffix (suffix);
    slider.setNumDecimalPlacesToDisplay (decimals);
    slider.setDoubleClickReturnValue (true, name == "Threshold" ? -18.0 : name == "Ratio" ? 4.0
                                      : name == "Knee" ? 6.0 : name == "Attack" ? 20.0
                                      : name == "Release" ? 120.0 : 0.0);
    addAndMakeVisible (slider);
}

float MultiSqueezeAudioProcessorEditor::parseFrequency (const juce::String& text)
{
    const auto trimmed = text.trim();
    const auto kilo = trimmed.endsWithIgnoreCase ("k") || trimmed.endsWithIgnoreCase ("khz");
    const auto value = trimmed.retainCharacters ("0123456789.,").replaceCharacter (',', '.').getFloatValue();
    return kilo ? value * 1000.0f : value;
}

void MultiSqueezeAudioProcessorEditor::attachSelectedBand()
{
    const auto band = audioProcessor.getSelectedBand();
    if (band == attachedBand)
        return;

    for (auto& attachment : bandSliderAttachments) attachment.reset();
    sidechainAttachment.reset();
    compressorBypassAttachment.reset();
    soloAttachment.reset();
    muteAttachment.reset();
    autoGainAttachment.reset();

    bandSliderAttachments[0] = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::threshold (band), bandSliders[0]);
    bandSliderAttachments[1] = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::ratio (band), bandSliders[1]);
    bandSliderAttachments[2] = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::attack (band), bandSliders[2]);
    bandSliderAttachments[3] = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::release (band), bandSliders[3]);
    bandSliderAttachments[4] = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::gain (band), bandSliders[4]);
    bandSliderAttachments[5] = std::make_unique<SliderAttachment> (audioProcessor.apvts, ParamIDs::knee (band), bandSliders[5]);
    sidechainAttachment = std::make_unique<ComboAttachment> (audioProcessor.apvts, ParamIDs::sidechain (band), sidechainBox);
    compressorBypassAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::compressorBypass (band), compressorBypassButton);
    soloAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::solo (band), soloButton);
    muteAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::mute (band), muteButton);
    autoGainAttachment = std::make_unique<ButtonAttachment> (audioProcessor.apvts, ParamIDs::autoGain (band), autoGainButton);
    attachedBand = band;
}

void MultiSqueezeAudioProcessorEditor::refreshBandReadout()
{
    attachSelectedBand();
    const auto band = audioProcessor.getSelectedBand();
    const auto count = audioProcessor.getNumBands();
    bandLabel.setText ("BAND " + juce::String (band + 1).paddedLeft ('0', 2) + " / "
                       + juce::String (count).paddedLeft ('0', 2), juce::dontSendNotification);
    rangeLabel.setText (SpectrumAnalyzer::formatFrequency (audioProcessor.getBandLowFrequency (band)) + "  –  "
                        + SpectrumAnalyzer::formatFrequency (audioProcessor.getBandHighFrequency (band)), juce::dontSendNotification);
    previousBandButton.setEnabled (band > 0);
    nextBandButton.setEnabled (band + 1 < count);
    addBandButton.setEnabled (count < MultiSqueezeConstants::maxBands);
    removeBandButton.setEnabled (count > 1);

    const auto first = band == 0;
    const auto last = band == count - 1;
    lowEdgeLabel.setEditable (false, ! first, false);
    highEdgeLabel.setEditable (false, ! last, false);
    if (! lowEdgeLabel.isBeingEdited())
        lowEdgeLabel.setText (SpectrumAnalyzer::formatFrequency (audioProcessor.getBandLowFrequency (band)), juce::dontSendNotification);
    if (! highEdgeLabel.isBeingEdited())
        highEdgeLabel.setText (SpectrumAnalyzer::formatFrequency (audioProcessor.getBandHighFrequency (band)), juce::dontSendNotification);
    const auto autoGainEnabled = audioProcessor.apvts.getRawParameterValue (ParamIDs::autoGain (band))->load() >= 0.5f;
    autoGainButton.setButtonText (autoGainEnabled
                                      ? "AUTO +" + juce::String (audioProcessor.getAutoMakeupDb (band), 1)
                                      : "AUTO");
    const auto readout = "GAIN REDUCTION  " + juce::String (audioProcessor.getGainReductionDb (band), 1) + " dB";
    gainReductionLabel.setText (readout, juce::dontSendNotification);
}

void MultiSqueezeAudioProcessorEditor::timerCallback()
{
    refreshBandReadout();
    repaint();
}

void MultiSqueezeAudioProcessorEditor::paint (juce::Graphics& g)
{
    if (backgroundImage.isValid())
        g.drawImage (backgroundImage, getLocalBounds().toFloat());
    else
        g.fillAll (MultiSqueezeLookAndFeel::background);

    const auto sx = (float) getWidth() / referenceWidth;
    const auto sy = (float) getHeight() / referenceHeight;
    const auto area = [sx, sy] (float x, float y, float w, float h)
    {
        return juce::Rectangle<float> (x * sx, y * sy, w * sx, h * sy);
    };

    const auto controlsPanel = area (295.0f, 418.0f, 871.0f, 216.0f);
    g.setColour (juce::Colour (0xff090d11).withAlpha (0.92f));
    g.fillRoundedRectangle (controlsPanel, 7.0f * sx);
    g.setColour (juce::Colour (0xff28323b));
    g.drawRoundedRectangle (controlsPanel.reduced (0.5f * sx), 7.0f * sx, 1.0f * sx);
    const auto numCells = (int) bandSliders.size();
    for (int i = 1; i < numCells; ++i)
    {
        const auto x = (295.0f + 871.0f * (float) i / (float) numCells) * sx;
        g.setColour (juce::Colour (0xff28323b).withAlpha (0.8f));
        g.drawVerticalLine (juce::roundToInt (x), 431.0f * sy, 622.0f * sy);
    }

    const auto meterGroup = gainReductionMeterLayoutTarget.getBounds().toFloat();
    const auto meterScaleX = meterGroup.getWidth() / 71.0f;
    const auto meterScaleY = meterGroup.getHeight() / 151.0f;
    const auto meter = juce::Rectangle<float> (meterGroup.getX() + 37.0f * meterScaleX,
                                                meterGroup.getY(), 34.0f * meterScaleX,
                                                meterGroup.getHeight());
    g.setColour (juce::Colour (0xff050709));
    g.fillRoundedRectangle (meter, 3.0f * juce::jmin (meterScaleX, meterScaleY));
    g.setColour (juce::Colour (0xff34414b));
    g.drawRoundedRectangle (meter.reduced (0.5f * meterScaleX),
                            3.0f * juce::jmin (meterScaleX, meterScaleY), meterScaleX);

    constexpr int meterSegments = 20;
    const auto reduction = juce::jlimit (0.0f, 60.0f,
                                         audioProcessor.getGainReductionDb (audioProcessor.getSelectedBand()));
    const auto activeSegments = juce::roundToInt (reduction / 60.0f * (float) meterSegments);
    const auto segmentArea = meter.reduced (7.0f * meterScaleX, 8.0f * meterScaleY);
    const auto gap = 2.0f * meterScaleY;
    const auto segmentHeight = (segmentArea.getHeight() - gap * (meterSegments - 1)) / meterSegments;
    for (int i = 0; i < meterSegments; ++i)
    {
        const auto y = segmentArea.getY() + (float) i * (segmentHeight + gap);
        const auto active = i < activeSegments;
        g.setColour (active ? juce::Colour (0xffd8f35a) : juce::Colour (0xff20282e));
        g.fillRect (segmentArea.getX(), y, segmentArea.getWidth(), segmentHeight);
    }

    g.setFont (MultiSqueezeFonts::numeric (10.0f * meterScaleY));
    g.setColour (MultiSqueezeLookAndFeel::textDim);
    for (const auto db : { 0, 12, 24, 36, 60 })
    {
        const auto y = juce::jmap ((float) db, 0.0f, 60.0f, meter.getY(), meter.getBottom());
        g.drawText (db == 0 ? "0" : "-" + juce::String (db),
                    juce::Rectangle<float> (meterGroup.getX(), y - 7.0f * meterScaleY,
                                             30.0f * meterScaleX, 14.0f * meterScaleY),
                    juce::Justification::centredRight);
    }

    if (logoImage.isValid())
    {
        const auto logo = logoLayoutTarget.getBounds();
        g.drawImageWithin (logoImage, logo.getX(), logo.getY(), logo.getWidth(), logo.getHeight(),
                           juce::RectanglePlacement::centred);
    }
}

void MultiSqueezeAudioProcessorEditor::resized()
{
    const auto sx = (float) getWidth() / referenceWidth;
    const auto sy = (float) getHeight() / referenceHeight;
    const auto bounds = [sx, sy] (float x, float y, float w, float h)
    {
        return juce::Rectangle<int> (juce::roundToInt (x * sx), juce::roundToInt (y * sy),
                                     juce::roundToInt (w * sx), juce::roundToInt (h * sy));
    };

    makerLabel.setBounds (bounds (55.0f, 13.0f, 190.0f, 27.0f));
    versionLabel.setBounds (bounds (960.0f, 24.0f, 64.0f, 24.0f));
    bypassButton.setBounds (bounds (1032.0f, 16.0f, 88.0f, 32.0f));
    logoLayoutTarget.setBounds (bounds (568.0f, 16.0f, 56.0f, 32.0f));

    titleLabel.setBounds (bounds (34.0f, 72.0f, 238.0f, 39.0f));
    subtitleLabel.setBounds (bounds (35.0f, 111.0f, 235.0f, 21.0f));
    inspectLabel.setBounds (bounds (35.0f, 140.0f, 220.0f, 16.0f));
    previousBandButton.setBounds (bounds (34.0f, 162.0f, 34.0f, 36.0f));
    bandLabel.setBounds (bounds (73.0f, 158.0f, 160.0f, 43.0f));
    nextBandButton.setBounds (bounds (238.0f, 162.0f, 34.0f, 36.0f));
    rangeLabel.setBounds (bounds (74.0f, 202.0f, 158.0f, 19.0f));
    lowEdgeLabel.setBounds (bounds (40.0f, 224.0f, 112.0f, 32.0f));
    highEdgeLabel.setBounds (bounds (160.0f, 224.0f, 112.0f, 32.0f));
    detectorLabel.setBounds (bounds (35.0f, 265.0f, 200.0f, 16.0f));
    sidechainBox.setBounds (bounds (34.0f, 282.0f, 238.0f, 34.0f));
    compressorBypassButton.setBounds (bounds (34.0f, 325.0f, 104.0f, 34.0f));
    soloButton.setBounds (bounds (144.0f, 325.0f, 60.0f, 34.0f));
    muteButton.setBounds (bounds (210.0f, 325.0f, 62.0f, 34.0f));
    gainReductionLabel.setBounds (bounds (44.0f, 370.0f, 218.0f, 24.0f));
    gainReductionMeterLayoutTarget.setBounds (bounds (96.0f, 408.0f, 71.0f, 151.0f));
    addBandButton.setBounds (bounds (34.0f, 585.0f, 112.0f, 34.0f));
    removeBandButton.setBounds (bounds (153.0f, 585.0f, 119.0f, 34.0f));

    spectrumTitle.setBounds (bounds (320.0f, 66.0f, 235.0f, 20.0f));
    spectrumHint.setBounds (bounds (640.0f, 66.0f, 508.0f, 20.0f));
    spectrum.setBounds (bounds (295.0f, 92.0f, 871.0f, 318.0f));

    constexpr float controlPanelX = 295.0f;
    constexpr float controlPanelWidth = 871.0f;
    const auto cellWidth = controlPanelWidth / (float) bandSliders.size();
    for (int i = 0; i < (int) bandSliders.size(); ++i)
    {
        const auto x = controlPanelX + cellWidth * (float) i;
        const auto isMakeup = i == 4;
        bandSliderLabels[(size_t) i].setBounds (isMakeup
                                                    ? bounds (x + 6.0f, 431.0f, 72.0f, 20.0f)
                                                    : bounds (x + 8.0f, 431.0f, cellWidth - 16.0f, 20.0f));
        bandSliders[(size_t) i].setBounds (bounds (x + 10.0f, 451.0f, cellWidth - 20.0f, 174.0f));
    }

    const auto makeupCellX = controlPanelX + cellWidth * 4.0f;
    autoGainButton.setBounds (bounds (makeupCellX + 80.0f, 431.0f, cellWidth - 88.0f, 20.0f));

    outputLabel.setBounds (bounds (488.0f, 696.0f, 96.0f, 16.0f));
    lookaheadButton.setBounds (bounds (296.0f, 656.0f, 112.0f, 32.0f));
    outputSlider.setBounds (bounds (490.0f, 650.0f, 623.0f, 42.0f));

#if defined (JUCE_LAYOUT_TUNER) && JUCE_LAYOUT_TUNER
    if (layoutTuner != nullptr)
    {
        layoutTuner->setBounds (getLocalBounds());
        layoutTuner->toFront (false);
    }
#endif
}
