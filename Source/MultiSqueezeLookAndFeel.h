#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

class MultiSqueezeLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    MultiSqueezeLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    static constexpr const char* smallButtonProperty = "smallButton";
    static const juce::Colour background, panel, accent, textPrimary, textDim;

private:
    juce::Image knobImage;
};
