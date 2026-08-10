#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

class CompressorBand
{
public:
    struct Settings
    {
        float thresholdDb = -18.0f;
        float ratio = 4.0f;
        float kneeDb = 6.0f;
        float attackMs = 20.0f;
        float releaseMs = 120.0f;
        float makeupDb = 0.0f;
        bool autoGainEnabled = false;
        bool lookaheadEnabled = false;
        bool bypassed = false;
    };

    void prepare (double newSampleRate) noexcept;
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& signal, const juce::AudioBuffer<float>* sidechain,
                  const Settings&) noexcept;
    float getGainReductionDb() const noexcept { return gainReductionDb.load(); }
    float getAutoMakeupDb() const noexcept { return autoMakeupDb.load(); }

private:
    static constexpr int maxLookaheadChannels = 2;

    double sampleRate = 44100.0;
    float envelope = 0.0f;
    std::atomic<float> gainReductionDb { 0.0f };
    std::atomic<float> autoMakeupDb { 0.0f };

    // Lookahead: a small fixed delay line on the audio path. The detector always
    // reads the freshest (undelayed) sample so gain reduction can start ramping
    // before a transient reaches the delayed output.
    juce::AudioBuffer<float> lookaheadBuffer;
    int lookaheadSamples = 0;
    int lookaheadWritePos = 0;
};
