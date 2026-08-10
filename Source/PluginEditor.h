#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "SpectrumAnalyzer.h"
#include "MultiSqueezeLookAndFeel.h"

#if defined (JUCE_LAYOUT_TUNER) && JUCE_LAYOUT_TUNER
#include "Debug/JuceLayoutTuner.h"
#endif

class MultiSqueezeAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit MultiSqueezeAudioProcessorEditor (MultiSqueezeAudioProcessor&);
    ~MultiSqueezeAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void attachSelectedBand();
    void refreshBandReadout();
    void configureKnob (juce::Slider&, juce::Label&, const juce::String&, const juce::String&, int decimals);
    static float parseFrequency (const juce::String&);

    MultiSqueezeLookAndFeel lookAndFeel;
    MultiSqueezeAudioProcessor& audioProcessor;
    juce::Image backgroundImage, logoImage;
    juce::Component logoLayoutTarget, gainReductionMeterLayoutTarget;
    SpectrumAnalyzer spectrum;

    juce::Label makerLabel, titleLabel, subtitleLabel, versionLabel;
    juce::TextButton bypassButton { "BYPASS" };
    juce::TextButton lookaheadButton { "LOOKAHEAD" };
    std::unique_ptr<ButtonAttachment> bypassAttachment, lookaheadAttachment;

    juce::Label inspectLabel, bandLabel, rangeLabel, lowEdgeLabel, highEdgeLabel, detectorLabel, gainReductionLabel;
    juce::TextButton previousBandButton { "<" }, nextBandButton { ">" };
    juce::TextButton addBandButton { "+ ADD BAND" }, removeBandButton { "- REMOVE" };
    juce::TextButton compressorBypassButton { "COMP BYPASS" }, soloButton { "SOLO" }, muteButton { "MUTE" };
    juce::TextButton autoGainButton { "AUTO GAIN" };
    juce::ComboBox sidechainBox;

    std::array<juce::Slider, 6> bandSliders;
    std::array<juce::Label, 6> bandSliderLabels;
    std::array<std::unique_ptr<SliderAttachment>, 6> bandSliderAttachments;
    std::unique_ptr<ComboAttachment> sidechainAttachment;
    std::unique_ptr<ButtonAttachment> compressorBypassAttachment, soloAttachment, muteAttachment, autoGainAttachment;

    juce::Label spectrumTitle, spectrumHint, outputLabel;
    juce::Slider outputSlider;
    std::unique_ptr<SliderAttachment> outputAttachment;
    int attachedBand = -1;

#if defined (JUCE_LAYOUT_TUNER) && JUCE_LAYOUT_TUNER
    std::unique_ptr<juce_layout_tuner::Overlay> layoutTuner;
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiSqueezeAudioProcessorEditor)
};
