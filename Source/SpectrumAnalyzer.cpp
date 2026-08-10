#include "SpectrumAnalyzer.h"
#include "PluginProcessor.h"
#include "MultiSqueezeLookAndFeel.h"
#include "MultiSqueezeFonts.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
    constexpr float graphMinDb = -72.0f;
    constexpr float graphMaxDb = 12.0f;
    constexpr float releaseSmoothing = 0.76f;
}

SpectrumAnalyzer::SpectrumAnalyzer (MultiSqueezeAudioProcessor& owner) : processor (owner)
{
    setMouseCursor (juce::MouseCursor::CrosshairCursor);
    startTimerHz (30);
}

SpectrumAnalyzer::~SpectrumAnalyzer() { stopTimer(); }

juce::Rectangle<float> SpectrumAnalyzer::graphBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    bounds.removeFromTop (48.0f);
    bounds.removeFromBottom (22.0f);
    bounds.removeFromLeft (28.0f);
    bounds.removeFromRight (38.0f);
    return bounds;
}

juce::Rectangle<float> SpectrumAnalyzer::overviewBounds() const
{
    auto bounds = getLocalBounds().toFloat();
    bounds.removeFromLeft (28.0f);
    bounds.removeFromRight (38.0f);
    bounds.setHeight (34.0f);
    return bounds;
}

float SpectrumAnalyzer::frequencyToX (float frequency) const
{
    const auto bounds = graphBounds();
    const auto proportion = std::log (juce::jlimit (MultiSqueezeConstants::minFrequencyHz,
                                                    MultiSqueezeConstants::maxFrequencyHz, frequency)
                                      / MultiSqueezeConstants::minFrequencyHz)
                            / std::log (MultiSqueezeConstants::maxFrequencyHz
                                        / MultiSqueezeConstants::minFrequencyHz);
    return bounds.getX() + proportion * bounds.getWidth();
}

float SpectrumAnalyzer::xToFrequency (float x) const
{
    const auto bounds = graphBounds();
    const auto proportion = juce::jlimit (0.0f, 1.0f, (x - bounds.getX()) / bounds.getWidth());
    return MultiSqueezeConstants::minFrequencyHz
           * std::pow (MultiSqueezeConstants::maxFrequencyHz / MultiSqueezeConstants::minFrequencyHz,
                       proportion);
}

float SpectrumAnalyzer::spectrumDbToY (float db) const
{
    return juce::jmap (juce::jlimit (graphMinDb, graphMaxDb, db), graphMinDb, graphMaxDb,
                       graphBounds().getBottom(), graphBounds().getY());
}

float SpectrumAnalyzer::gainToY (float gain) const
{
    return juce::jmap (juce::jlimit (-24.0f, 24.0f, gain), -24.0f, 24.0f,
                       graphBounds().getBottom(), graphBounds().getY());
}

float SpectrumAnalyzer::yToGain (float y) const
{
    return juce::jlimit (-24.0f, 24.0f,
                         juce::jmap (y, graphBounds().getBottom(), graphBounds().getY(), -24.0f, 24.0f));
}

int SpectrumAnalyzer::bandAt (float x) const
{
    for (int band = 0; band < processor.getNumBands() - 1; ++band)
        if (x < frequencyToX (processor.getBandHighFrequency (band)))
            return band;
    return processor.getNumBands() - 1;
}

int SpectrumAnalyzer::splitNear (float x) const
{
    int nearest = -1;
    auto distance = 9.0f;
    for (int band = 0; band < processor.getNumBands() - 1; ++band)
    {
        const auto candidate = std::abs (frequencyToX (processor.getBandHighFrequency (band)) - x);
        if (candidate < distance) { nearest = band; distance = candidate; }
    }
    return nearest;
}

juce::String SpectrumAnalyzer::formatFrequency (float frequency)
{
    return frequency >= 1000.0f ? juce::String (frequency / 1000.0f, 2) + " kHz"
                                : juce::String (juce::roundToInt (frequency)) + " Hz";
}

void SpectrumAnalyzer::mouseDown (const juce::MouseEvent& event)
{
    draggedSplit = splitNear (event.position.x);
    draggedGainBand = -1;
    gestureStarted = false;
    if (draggedSplit >= 0)
        return;

    const auto band = bandAt (event.position.x);
    processor.setSelectedBand (band);
    if (std::abs (gainToY (processor.apvts.getRawParameterValue (ParamIDs::gain (band))->load())
                  - event.position.y) < 9.0f)
        draggedGainBand = band;
    repaint();
}

void SpectrumAnalyzer::mouseDrag (const juce::MouseEvent& event)
{
    if (draggedSplit >= 0)
    {
        processor.setBandUpperFrequency (draggedSplit, xToFrequency (event.position.x));
        repaint();
        return;
    }

    if (draggedGainBand >= 0)
    {
        if (auto* parameter = processor.apvts.getParameter (ParamIDs::gain (draggedGainBand)))
        {
            if (! gestureStarted) { parameter->beginChangeGesture(); gestureStarted = true; }
            parameter->setValueNotifyingHost (processor.apvts.getParameterRange (parameter->getParameterID())
                                                   .convertTo0to1 (yToGain (event.position.y)));
        }
        repaint();
    }
}

void SpectrumAnalyzer::mouseUp (const juce::MouseEvent&)
{
    if (draggedGainBand >= 0 && gestureStarted)
        if (auto* parameter = processor.apvts.getParameter (ParamIDs::gain (draggedGainBand)))
            parameter->endChangeGesture();
    draggedSplit = draggedGainBand = -1;
    gestureStarted = false;
}

void SpectrumAnalyzer::mouseDoubleClick (const juce::MouseEvent& event)
{
    if (const auto split = splitNear (event.position.x); split >= 0)
    {
        processor.setSelectedBand (split);
        processor.removeSelectedBand();
    }
    else if (graphBounds().contains (event.position))
    {
        processor.addBand (xToFrequency (event.position.x));
        processor.setSelectedBand (bandAt (event.position.x));
    }
    repaint();
}

void SpectrumAnalyzer::timerCallback()
{
    bool changed = false;
    if (processor.getInputSpectrumFifo().pullLatestBlock (inputSamples))
    {
        updatePath (inputSamples, inputPath);
        changed = true;
    }
    if (processor.getOutputSpectrumFifo().pullLatestBlock (outputSamples))
    {
        updatePath (outputSamples, outputPath);
        changed = true;
    }
    if (changed)
        repaint();
    else
        repaint(); // also follows automation and gain-reduction metering
}

void SpectrumAnalyzer::updatePath (const std::array<float, SpectrumFifo::fftSize>& samples, juce::Path& path)
{
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    std::copy (samples.begin(), samples.end(), fftData.begin());
    window.multiplyWithWindowingTable (fftData.data(), (size_t) SpectrumFifo::fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    auto& magnitudes = &path == &inputPath ? inputMagnitudes : outputMagnitudes;
    for (int bin = 0; bin < SpectrumFifo::fftSize / 2; ++bin)
    {
        // performFrequencyOnlyForwardTransform is an unnormalised DFT: a full-
        // scale sine landing on a bin reads back as fftSize/2, not fftSize --
        // dividing by fftSize alone (the old code) under-read every level by
        // half (-6 dB). The *2 here restores the true input amplitude.
        const auto value = fftData[(size_t) bin] * 2.0f / (float) SpectrumFifo::fftSize;
        magnitudes[(size_t) bin] = value > magnitudes[(size_t) bin]
                                     ? value : magnitudes[(size_t) bin] * releaseSmoothing;
    }

    const auto bounds = graphBounds();
    const auto sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : 44100.0;
    path.clear();
    for (int pixel = 0; pixel <= (int) bounds.getWidth(); ++pixel)
    {
        const auto x = bounds.getX() + (float) pixel;
        const auto freqLo = xToFrequency (x - 0.5f);
        const auto freqHi = xToFrequency (x + 0.5f);
        const auto maxBin = SpectrumFifo::fftSize / 2 - 1;
        const auto binPosLo = freqLo * (float) SpectrumFifo::fftSize / (float) sampleRate;
        const auto binPosHi = freqHi * (float) SpectrumFifo::fftSize / (float) sampleRate;

        float value;
        if (binPosHi - binPosLo >= 1.0f)
        {
            // High end of a log-frequency axis: many FFT bins fall inside a
            // single pixel here, so average all of them instead of sampling
            // just the nearest one -- picking one made the curve jump between
            // neighbouring bins and look jagged.
            auto binLo = juce::jlimit (0, maxBin, juce::roundToInt (binPosLo));
            auto binHi = juce::jlimit (0, maxBin, juce::roundToInt (binPosHi));
            if (binHi < binLo)
                std::swap (binLo, binHi);
            float sum = 0.0f;
            for (int bin = binLo; bin <= binHi; ++bin)
                sum += magnitudes[(size_t) bin];
            value = sum / (float) (binHi - binLo + 1);
        }
        else
        {
            // Low end: the FFT's fixed ~11-12 Hz bin spacing is *wider* than
            // several consecutive pixels down here, so the same raw bin would
            // otherwise repeat across many pixels as a jagged staircase.
            // Linearly interpolate between the two surrounding bins instead,
            // which reconstructs a smooth curve at any frequency resolution.
            const auto binPos = juce::jlimit (0.0f, (float) maxBin, (binPosLo + binPosHi) * 0.5f);
            const auto binLo = (int) binPos;
            const auto binHi = juce::jmin (maxBin, binLo + 1);
            const auto frac = binPos - (float) binLo;
            value = magnitudes[(size_t) binLo] + (magnitudes[(size_t) binHi] - magnitudes[(size_t) binLo]) * frac;
        }

        const auto db = juce::Decibels::gainToDecibels (value, graphMinDb);
        const auto y = juce::jmap (juce::jlimit (graphMinDb, graphMaxDb, db), graphMinDb, graphMaxDb,
                                   bounds.getBottom(), bounds.getY());
        if (pixel == 0) path.startNewSubPath (x, y); else path.lineTo (x, y);
    }
}

void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    const auto bounds = graphBounds();
    const auto overview = overviewBounds();
    g.setColour (juce::Colour (0xff070b0f).withAlpha (0.94f));
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 7.0f);

    g.setColour (juce::Colour (0xff0a0e12));
    g.fillRoundedRectangle (overview, 4.0f);
    g.setColour (juce::Colour (0xff34414b));
    g.drawRoundedRectangle (overview.reduced (0.5f), 4.0f, 1.0f);

    const auto selected = processor.getSelectedBand();
    static constexpr std::array<juce::uint32, 6> bandColours {
        0xff36d7ff, 0xff43d6c5, 0xffc8b75d, 0xff8e829f, 0xff6888a4, 0xff78a888
    };

    for (int band = 0; band < processor.getNumBands(); ++band)
    {
        const auto left = frequencyToX (processor.getBandLowFrequency (band));
        const auto right = frequencyToX (processor.getBandHighFrequency (band));
        const auto colour = juce::Colour (bandColours[(size_t) band % bandColours.size()]);
        g.setColour (colour.withAlpha (band == selected ? 0.10f : 0.035f));
        g.fillRect (bounds.withX (left).withRight (right));

        auto railBand = overview.withX (left).withRight (right);
        g.setColour (band == selected ? MultiSqueezeLookAndFeel::accent.withAlpha (0.17f)
                                     : juce::Colour (0xff141a20).withAlpha (0.82f));
        g.fillRect (railBand.reduced (1.0f, 1.0f));
        g.setColour (band == selected ? MultiSqueezeLookAndFeel::accent
                                     : MultiSqueezeLookAndFeel::textPrimary);
        g.setFont (MultiSqueezeFonts::numeric (11.0f));
        g.drawText (juce::String (band + 1).paddedLeft ('0', 2), railBand,
                    juce::Justification::centred);
    }

    g.setColour (juce::Colour (0xff34414b));
    g.drawRect (bounds, 1.0f);

    static constexpr std::array<float, 10> frequencies { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (auto frequency : frequencies)
    {
        const auto x = frequencyToX (frequency);
        g.setColour (juce::Colour (0xff303640).withAlpha (0.65f));
        g.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());
        g.setColour (MultiSqueezeLookAndFeel::textDim);
        g.setFont (MultiSqueezeFonts::numeric (10.0f));
        g.drawText (formatFrequency (frequency), juce::Rectangle<float> (x - 28.0f, bounds.getBottom() + 3.0f, 56.0f, 15.0f),
                    juce::Justification::centred);
    }

    for (int db = -60; db <= 12; db += 12)
    {
        const auto y = spectrumDbToY ((float) db);
        g.setColour (juce::Colour (0xff2a3038).withAlpha (0.55f));
        g.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());
        g.setColour (MultiSqueezeLookAndFeel::textDim);
        g.setFont (MultiSqueezeFonts::numeric (10.0f));
        const auto label = db > 0 ? "+" + juce::String (db) : juce::String (db);
        g.drawText (label, juce::Rectangle<float> (0.0f, y - 7.0f, bounds.getX() - 5.0f, 14.0f),
                    juce::Justification::centredRight);
    }

    g.setColour (MultiSqueezeLookAndFeel::textDim);
    g.setFont (MultiSqueezeFonts::uiMedium (9.0f));
    g.drawText ("LEVEL", juce::Rectangle<float> (0.0f, bounds.getY() - 18.0f, bounds.getX() - 5.0f, 14.0f),
                juce::Justification::centredRight);

    for (const auto gainDb : { -24, -12, 0, 12, 24 })
    {
        const auto y = gainToY ((float) gainDb);
        g.setColour (gainDb == 0 ? MultiSqueezeLookAndFeel::accent.withAlpha (0.8f)
                                 : juce::Colour (0xff3a444d).withAlpha (0.75f));
        g.drawLine (bounds.getRight(), y, bounds.getRight() + 4.0f, y, 1.0f);
        g.setColour (gainDb == 0 ? MultiSqueezeLookAndFeel::textPrimary
                                 : MultiSqueezeLookAndFeel::textDim);
        g.setFont (MultiSqueezeFonts::numeric (9.5f));
        const auto label = gainDb > 0 ? "+" + juce::String (gainDb) : juce::String (gainDb);
        g.drawText (label,
                    juce::Rectangle<float> (bounds.getRight() + 5.0f, y - 7.0f,
                                            (float) getWidth() - bounds.getRight() - 6.0f, 14.0f),
                    juce::Justification::centredLeft);
    }

    g.setColour (MultiSqueezeLookAndFeel::textDim);
    g.setFont (MultiSqueezeFonts::uiMedium (8.5f));
    g.drawText ("GAIN", juce::Rectangle<float> (bounds.getRight() + 4.0f, bounds.getY() - 18.0f,
                                                 (float) getWidth() - bounds.getRight() - 5.0f, 14.0f),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (0xff77838f).withAlpha (0.75f));
    g.strokePath (inputPath, juce::PathStrokeType (1.0f));
    g.setColour (MultiSqueezeLookAndFeel::accent);
    g.strokePath (outputPath, juce::PathStrokeType (1.6f));

    for (int band = 0; band < processor.getNumBands(); ++band)
    {
        const auto left = frequencyToX (processor.getBandLowFrequency (band));
        const auto right = frequencyToX (processor.getBandHighFrequency (band));
        const auto gain = processor.apvts.getRawParameterValue (ParamIDs::gain (band))->load();
        const auto y = gainToY (gain);
        g.setColour ((band == selected ? juce::Colour (0xffdff47a) : juce::Colour (0xff667078)).withAlpha (0.85f));
        g.drawHorizontalLine (juce::roundToInt (y), left + 4.0f, right - 4.0f);
        if (band == selected)
            g.fillEllipse (juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (left + right) * 0.5f, y }));

    }

    const auto threshold = processor.apvts.getRawParameterValue (ParamIDs::threshold (selected))->load();
    const auto thresholdY = spectrumDbToY (threshold);
    const auto thresholdLeft = frequencyToX (processor.getBandLowFrequency (selected));
    const auto thresholdRight = frequencyToX (processor.getBandHighFrequency (selected));
    const auto thresholdCentre = (thresholdLeft + thresholdRight) * 0.5f;
    const auto thresholdHalfWidth = juce::jmax (8.0f, juce::jmin (42.0f, (thresholdRight - thresholdLeft) * 0.5f - 6.0f));
    const auto thresholdColour = threshold <= -36.0f ? juce::Colour (0xffffb547)
                                                     : MultiSqueezeLookAndFeel::textPrimary;

    g.setColour (thresholdColour.withAlpha (0.95f));
    g.drawLine (thresholdCentre - thresholdHalfWidth, thresholdY,
                thresholdCentre + thresholdHalfWidth, thresholdY, 1.6f);
    g.fillPath ([thresholdCentre, thresholdY]
    {
        juce::Path marker;
        marker.addTriangle (thresholdCentre - 4.0f, thresholdY - 5.0f,
                            thresholdCentre + 4.0f, thresholdY - 5.0f,
                            thresholdCentre, thresholdY);
        return marker;
    }());

    const auto thresholdText = "THR  " + juce::String (threshold, 1) + " dB";
    const auto textWidth = 76.0f;
    auto thresholdLabel = juce::Rectangle<float> (textWidth, 16.0f)
                              .withCentre ({ thresholdCentre, thresholdY - 12.0f })
                              .constrainedWithin (bounds.reduced (2.0f));
    g.setColour (juce::Colour (0xff07090b).withAlpha (0.88f));
    g.fillRoundedRectangle (thresholdLabel, 3.0f);
    g.setColour (thresholdColour.withAlpha (0.8f));
    g.drawRoundedRectangle (thresholdLabel, 3.0f, 0.8f);
    g.setColour (thresholdColour);
    g.setFont (MultiSqueezeFonts::numeric (10.0f));
    g.drawText (thresholdText, thresholdLabel, juce::Justification::centred);

    for (int band = 0; band < processor.getNumBands() - 1; ++band)
    {
        const auto x = frequencyToX (processor.getBandHighFrequency (band));
        g.setColour (MultiSqueezeLookAndFeel::accent.withAlpha (0.85f));
        g.drawVerticalLine (juce::roundToInt (x), bounds.getY(), bounds.getBottom());
        g.fillEllipse ({ x - 4.0f, bounds.getCentreY() - 4.0f, 8.0f, 8.0f });
    }

    g.setColour (juce::Colour (0xff303640));
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 7.0f, 1.0f);
}
