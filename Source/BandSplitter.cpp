#include "BandSplitter.h"

void BandSplitter::prepare (const juce::dsp::ProcessSpec& spec)
{
    for (auto& filter : filters)
        filter.prepare (spec);
}

void BandSplitter::reset()
{
    for (auto& filter : filters)
        filter.reset();
}

void BandSplitter::process (const juce::dsp::AudioBlock<const float>& input,
                            const std::array<float, MultiSqueezeConstants::maxSplits>& frequencies,
                            int numSplits,
                            std::array<juce::AudioBuffer<float>, MultiSqueezeConstants::maxBands>& output)
{
    const int channels = (int) input.getNumChannels();
    const int samples = (int) input.getNumSamples();
    numSplits = juce::jlimit (0, MultiSqueezeConstants::maxSplits, numSplits);

    for (int band = 0; band <= numSplits; ++band)
        output[(size_t) band].setSize (channels, samples, false, false, true);

    for (int split = 0; split < numSplits; ++split)
        filters[(size_t) split].setCutoffFrequency (frequencies[(size_t) split]);

    for (int channel = 0; channel < channels; ++channel)
    {
        const auto* source = input.getChannelPointer ((size_t) channel);
        for (int sample = 0; sample < samples; ++sample)
        {
            auto remainder = source[sample];
            for (int split = 0; split < numSplits; ++split)
            {
                float low = 0.0f, high = 0.0f;
                filters[(size_t) split].processSample (channel, remainder, low, high);
                output[(size_t) split].setSample (channel, sample, low);
                remainder = high;
            }
            output[(size_t) numSplits].setSample (channel, sample, remainder);
        }
    }
}
