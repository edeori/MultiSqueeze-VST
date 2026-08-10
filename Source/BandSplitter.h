#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "Parameters.h"

class BandSplitter
{
public:
    void prepare (const juce::dsp::ProcessSpec&);
    void reset();
    void process (const juce::dsp::AudioBlock<const float>& input,
                  const std::array<float, MultiSqueezeConstants::maxSplits>& frequencies,
                  int numSplits,
                  std::array<juce::AudioBuffer<float>, MultiSqueezeConstants::maxBands>& output);

private:
    std::array<juce::dsp::LinkwitzRileyFilter<float>, MultiSqueezeConstants::maxSplits> filters;
};
