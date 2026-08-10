#include "MultiSqueezeLookAndFeel.h"
#include "MultiSqueezeFonts.h"
#include "BinaryData.h"
#include <cmath>

const juce::Colour MultiSqueezeLookAndFeel::background { 0xff07090b };
const juce::Colour MultiSqueezeLookAndFeel::panel { 0xff0d1115 };
const juce::Colour MultiSqueezeLookAndFeel::accent { 0xff36d7ff };
const juce::Colour MultiSqueezeLookAndFeel::textPrimary { 0xffeaf2f5 };
const juce::Colour MultiSqueezeLookAndFeel::textDim { 0xff84919a };

MultiSqueezeLookAndFeel::MultiSqueezeLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, accent);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0xff0c0c0e));
    setColour (juce::Slider::textBoxOutlineColourId, accent.withAlpha (0.45f));
    setColour (juce::ComboBox::backgroundColourId, panel);
    setColour (juce::ComboBox::textColourId, textPrimary);
    setColour (juce::ComboBox::outlineColourId, accent.withAlpha (0.35f));
    setColour (juce::ComboBox::arrowColourId, accent);
    setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff141517));
    setColour (juce::PopupMenu::textColourId, textPrimary);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha (0.3f));
    setColour (juce::Label::textColourId, textPrimary);
    knobImage = juce::ImageFileFormat::loadFrom (BinaryData::knobv2_png, (size_t) BinaryData::knobv2_pngSize);
}

void MultiSqueezeLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                                 float position, float start, float end, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const auto centre = bounds.getCentre();
    const auto fullRadius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto radius = fullRadius * 0.76f;
    const auto angle = start + position * (end - start);

    juce::Path inactiveArc;
    inactiveArc.addCentredArc (centre.x, centre.y, fullRadius * 0.82f, fullRadius * 0.82f,
                               0.0f, start, end, true);
    g.setColour (juce::Colour (0xff28323b));
    g.strokePath (inactiveArc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

    if (slider.isEnabled())
    {
        juce::Path valueArc;
        valueArc.addCentredArc (centre.x, centre.y, fullRadius * 0.82f, fullRadius * 0.82f,
                                0.0f, start, angle, true);
        g.setColour (accent);
        g.strokePath (valueArc, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    for (int tick = 0; tick < 21; ++tick)
    {
        const auto tickAngle = start + (float) tick / 20.0f * (end - start);
        const auto inner = juce::Point<float> (centre.x + fullRadius * 0.87f * std::sin (tickAngle),
                                               centre.y - fullRadius * 0.87f * std::cos (tickAngle));
        const auto outer = juce::Point<float> (centre.x + fullRadius * std::sin (tickAngle),
                                               centre.y - fullRadius * std::cos (tickAngle));
        g.setColour (tickAngle <= angle && slider.isEnabled() ? accent : juce::Colour (0xff3a444d));
        g.drawLine ({ inner, outer }, 1.4f);
    }

    auto knobBounds = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);
    if (knobImage.isValid())
    {
        g.saveState();
        juce::Path clip;
        clip.addEllipse (knobBounds);
        g.reduceClipRegion (clip);
        g.addTransform (juce::AffineTransform::rotation (angle - (start + end) * 0.5f, centre.x, centre.y));
        g.setOpacity (slider.isEnabled() ? 1.0f : 0.5f);
        g.drawImage (knobImage, knobBounds);
        g.restoreState();
    }
    else
    {
        g.setColour (panel.brighter (0.15f));
        g.fillEllipse (knobBounds);
        g.setColour (accent);
        g.drawLine (centre.x, centre.y, centre.x + radius * 0.7f * std::sin (angle),
                    centre.y - radius * 0.7f * std::cos (angle), 2.0f);
    }
}

void MultiSqueezeLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                                 float position, float, float,
                                                 juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style != juce::Slider::LinearHorizontal)
        return LookAndFeel_V4::drawLinearSlider (g, x, y, width, height, position, 0, 0, style, slider);
    auto track = juce::Rectangle<float> ((float) x, (float) y + height * 0.42f, (float) width, 5.0f);
    g.setColour (juce::Colour (0xff090a0c));
    g.fillRoundedRectangle (track, 2.5f);
    g.setColour (accent.withMultipliedAlpha (slider.isEnabled() ? 0.9f : 0.35f));
    g.fillRoundedRectangle (track.withRight (position), 2.5f);
    g.fillRoundedRectangle ({ position - 3.0f, (float) y + height * 0.22f, 6.0f, height * 0.56f }, 3.0f);
}

void MultiSqueezeLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                     const juce::Colour&, bool highlighted, bool down)
{
    auto bounds = button.getLocalBounds().toFloat().reduced (1.0f);
    const auto on = button.getToggleState();
    g.setColour (on ? accent.withAlpha (0.88f) : juce::Colour (0xff0d1217));
    g.fillRoundedRectangle (bounds, 5.0f);
    if (highlighted || down)
    {
        g.setColour (juce::Colours::white.withAlpha (down ? 0.12f : 0.06f));
        g.fillRoundedRectangle (bounds, 5.0f);
    }
    g.setColour (on ? accent : juce::Colour (0xff34414b));
    g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
}

void MultiSqueezeLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.getToggleState() ? background : accent);
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (5, 2), juce::Justification::centred, 1);
}

juce::Font MultiSqueezeLookAndFeel::getTextButtonFont (juce::TextButton& button, int height)
{
    if ((bool) button.getProperties().getWithDefault (smallButtonProperty, false))
        return MultiSqueezeFonts::uiMedium (juce::jmin (10.5f, height * 0.52f));

    return MultiSqueezeFonts::uiMedium (juce::jmin (14.0f, height * 0.48f));
}

juce::Font MultiSqueezeLookAndFeel::getComboBoxFont (juce::ComboBox&) { return MultiSqueezeFonts::uiMedium (14.0f); }
juce::Font MultiSqueezeLookAndFeel::getPopupMenuFont() { return MultiSqueezeFonts::uiLight (15.0f); }
