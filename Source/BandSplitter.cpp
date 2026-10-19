#include "BandSplitter.h"

void BandSplitter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    for (auto& filter : filters)
        filter.prepare (spec);
    for (auto& row : phaseCompensation)
        for (auto& filter : row)
        {
            filter.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
            filter.prepare (spec);
        }
}

void BandSplitter::reset()
{
    for (auto& filter : filters)
        filter.reset();
    for (auto& row : phaseCompensation)
        for (auto& filter : row)
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
    {
        const auto cutoff = juce::jlimit (1.0f, (float) (sampleRate * 0.49), frequencies[(size_t) split]);
        filters[(size_t) split].setCutoffFrequency (cutoff);
        for (int band = 0; band < split; ++band)
            phaseCompensation[(size_t) band][(size_t) split].setCutoffFrequency (cutoff);
    }

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
                // Every lower band must receive the phase shift of subsequent
                // crossovers too, so the summed response remains flat.
                for (int later = split + 1; later < numSplits; ++later)
                    low = phaseCompensation[(size_t) split][(size_t) later].processSample (channel, low);
                output[(size_t) split].setSample (channel, sample, low);
                remainder = high;
            }
            output[(size_t) numSplits].setSample (channel, sample, remainder);
        }
    }
}
