#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <array>
#include "SpectrumFifo.h"

class MultiSqueezeAudioProcessor;

class SpectrumAnalyzer final : public juce::Component, private juce::Timer
{
public:
    explicit SpectrumAnalyzer (MultiSqueezeAudioProcessor&);
    ~SpectrumAnalyzer() override;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    static juce::String formatFrequency (float);

private:
    void timerCallback() override;
    juce::Rectangle<float> overviewBounds() const;
    juce::Rectangle<float> graphBounds() const;
    float frequencyToX (float) const;
    float xToFrequency (float) const;
    float spectrumDbToY (float) const;
    float gainToY (float) const;
    float yToGain (float) const;
    int splitNear (float) const;
    int bandAt (float) const;
    void updatePath (const std::array<float, SpectrumFifo::fftSize>&, juce::Path&);

    MultiSqueezeAudioProcessor& processor;
    juce::dsp::FFT fft { SpectrumFifo::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) SpectrumFifo::fftSize,
                                                  juce::dsp::WindowingFunction<float>::hann };
    std::array<float, SpectrumFifo::fftSize> inputSamples {}, outputSamples {};
    std::array<float, SpectrumFifo::fftSize * 2> fftData {};
    std::array<float, SpectrumFifo::fftSize / 2> inputMagnitudes {}, outputMagnitudes {};
    juce::Path inputPath, outputPath;
    int draggedSplit = -1;
    int draggedGainBand = -1;
    bool gestureStarted = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
