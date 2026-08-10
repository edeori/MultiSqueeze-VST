#include "CompressorBand.h"
#include "Parameters.h"
#include <cmath>

void CompressorBand::prepare (double newSampleRate) noexcept
{
    sampleRate = juce::jmax (1.0, newSampleRate);
    lookaheadSamples = juce::jmax (1, (int) std::round (0.001 * MultiSqueezeConstants::lookaheadMs * sampleRate));
    lookaheadBuffer.setSize (maxLookaheadChannels, lookaheadSamples, false, true, true);
    reset();
}

void CompressorBand::reset() noexcept
{
    envelope = 0.0f;
    gainReductionDb = 0.0f;
    autoMakeupDb = 0.0f;
    lookaheadBuffer.clear();
    lookaheadWritePos = 0;
}

void CompressorBand::process (juce::AudioBuffer<float>& signal,
                              const juce::AudioBuffer<float>* sidechain,
                              const Settings& settings) noexcept
{
    const auto channels = signal.getNumChannels();
    const auto samples = signal.getNumSamples();
    if (channels == 0 || samples == 0)
        return;

    const auto& detector = sidechain != nullptr && sidechain->getNumChannels() > 0 ? *sidechain : signal;
    const auto attack = std::exp (-1.0f / (0.001f * juce::jmax (0.01f, settings.attackMs) * (float) sampleRate));
    const auto release = std::exp (-1.0f / (0.001f * juce::jmax (1.0f, settings.releaseMs) * (float) sampleRate));
    const auto ratio = juce::jmax (1.0f, settings.ratio);
    const auto kneeDb = juce::jmax (0.0f, settings.kneeDb);
    const auto slope = 1.0f - 1.0f / ratio;

    // Auto Gain: a fixed, static make-up amount -- half the reduction a
    // signal sitting continuously at 0 dBFS would receive -- but scaled down
    // by how much of that maximum is actually being reduced *right now*,
    // using the very same envelope that drives the reduction itself below.
    // That means exactly 0 dB make-up whenever nothing is being compressed
    // (quiet material that never reaches Threshold), rising smoothly towards
    // the full static amount only as the signal approaches 0 dBFS. There is
    // no separate follower with its own timing, so it can't drift out of
    // sync with the compressor and turn into a second, slower compressor.
    constexpr float autoGainCeilingDb = 30.0f;
    const auto maxReductionAt0dBFS = juce::jmax (0.001f, -settings.thresholdDb * slope);
    const auto staticMakeup = settings.autoGainEnabled
                                  ? juce::jlimit (0.0f, autoGainCeilingDb, maxReductionAt0dBFS * 0.5f)
                                  : 0.0f;

    float lastReduction = 0.0f;
    float lastAutoMakeup = 0.0f;

    for (int sample = 0; sample < samples; ++sample)
    {
        float peak = 0.0f;
        for (int channel = 0; channel < detector.getNumChannels(); ++channel)
            peak = juce::jmax (peak, std::abs (detector.getSample (channel, sample)));

        const auto coefficient = peak > envelope ? attack : release;
        envelope = coefficient * envelope + (1.0f - coefficient) * peak;
        const auto levelDb = juce::Decibels::gainToDecibels (envelope, -120.0f);
        const auto overDb = levelDb - settings.thresholdDb;

        // Standard quadratic soft-knee compressor curve (as used in most
        // hardware and software compressors): flat below the knee, a smooth
        // parabolic blend through it, and the plain Threshold/Ratio slope
        // above it. No escalation, no hidden extra behaviour -- Ratio means
        // exactly what it says at any Threshold.
        if (settings.bypassed)
            lastReduction = 0.0f;
        else if (overDb <= -kneeDb * 0.5f)
            lastReduction = 0.0f;
        else if (overDb >= kneeDb * 0.5f)
            lastReduction = overDb * slope;
        else
        {
            const auto x = overDb + kneeDb * 0.5f;
            lastReduction = slope * (x * x) / (2.0f * juce::jmax (0.001f, kneeDb));
        }

        lastAutoMakeup = staticMakeup * juce::jlimit (0.0f, 1.0f, lastReduction / maxReductionAt0dBFS);
        const auto totalGain = juce::Decibels::decibelsToGain (settings.makeupDb + lastAutoMakeup - lastReduction);

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto rawSample = signal.getSample (channel, sample);
            auto delayedSample = rawSample;
            if (settings.lookaheadEnabled && channel < maxLookaheadChannels)
            {
                delayedSample = lookaheadBuffer.getSample (channel, lookaheadWritePos);
                lookaheadBuffer.setSample (channel, lookaheadWritePos, rawSample);
            }
            signal.setSample (channel, sample, delayedSample * totalGain);
        }
        if (settings.lookaheadEnabled)
            lookaheadWritePos = (lookaheadWritePos + 1) % lookaheadSamples;
    }

    gainReductionDb = lastReduction;
    autoMakeupDb = lastAutoMakeup;
}
