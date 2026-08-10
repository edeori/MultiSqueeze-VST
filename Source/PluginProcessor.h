#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

#include "BandSplitter.h"
#include "CompressorBand.h"
#include "Parameters.h"
#include "SpectrumFifo.h"

class MultiSqueezeAudioProcessor final : public juce::AudioProcessor
{
public:
    MultiSqueezeAudioProcessor();
    ~MultiSqueezeAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    juce::AudioProcessorParameter* getBypassParameter() const override;

    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMETERS", createParameterLayout() };

    int getNumBands() const noexcept;
    int getSelectedBand() const noexcept { return selectedBand.load(); }
    void setSelectedBand (int) noexcept;
    bool addBand (float preferredFrequencyHz = 0.0f);
    bool removeSelectedBand();
    float getBandLowFrequency (int band) const noexcept;
    float getBandHighFrequency (int band) const noexcept;
    void setBandUpperFrequency (int band, float frequencyHz);
    float getGainReductionDb (int band) const noexcept;
    float getAutoMakeupDb (int band) const noexcept;
    SpectrumFifo& getInputSpectrumFifo() noexcept { return inputSpectrum; }
    SpectrumFifo& getOutputSpectrumFifo() noexcept { return outputSpectrum; }

private:
    struct ActiveSplit { float frequency = 0.0f; int parameterSlot = -1; };
    static BusesProperties createBuses();
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    int getActiveSplits (std::array<ActiveSplit, MultiSqueezeConstants::maxSplits>&) const noexcept;

    BandSplitter splitter;
    std::array<juce::AudioBuffer<float>, MultiSqueezeConstants::maxBands> bandBuffers;
    std::array<CompressorBand, MultiSqueezeConstants::maxBands> compressors;
    juce::AudioBuffer<float> inputScratch;
    SpectrumFifo inputSpectrum, outputSpectrum;

    std::array<std::atomic<float>*, MultiSqueezeConstants::maxSplits> splitActiveParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxSplits> splitFrequencyParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> thresholdParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> ratioParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> kneeParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> attackParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> releaseParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> gainParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> autoGainParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> sidechainParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> compressorBypassParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> soloParams {};
    std::array<std::atomic<float>*, MultiSqueezeConstants::maxBands> muteParams {};
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* outputParam = nullptr;
    std::atomic<float>* lookaheadParam = nullptr;
    std::atomic<int> selectedBand { 0 };
    int lookaheadLatencySamples = 0;
    bool lastLookaheadEnabled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MultiSqueezeAudioProcessor)
};
