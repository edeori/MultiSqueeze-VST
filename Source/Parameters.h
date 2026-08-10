#pragma once

#include <juce_core/juce_core.h>

namespace MultiSqueezeConstants
{
    // Plugin formats require a stable parameter list. Sixteen bands gives a
    // generous practical ceiling while keeping every control automatable.
    inline constexpr int maxBands = 16;
    inline constexpr int maxSplits = maxBands - 1;
    inline constexpr int numSidechainBuses = 4;
    inline constexpr float minFrequencyHz = 20.0f;
    inline constexpr float maxFrequencyHz = 20000.0f;

    // Fixed lookahead time when the (whole-plugin) Lookahead toggle is enabled.
    // Shared between CompressorBand (delay line sizing) and the processor
    // (host latency reporting) so the two stay in lockstep.
    inline constexpr float lookaheadMs = 5.0f;
}

namespace ParamIDs
{
    inline constexpr auto bypass = "bypass";
    inline constexpr auto output = "output";
    inline constexpr auto lookahead = "lookahead";

    inline juce::String splitActive (int i) { return "splitActive" + juce::String (i); }
    inline juce::String splitFrequency (int i) { return "splitFrequency" + juce::String (i); }
    inline juce::String threshold (int i) { return "threshold" + juce::String (i); }
    inline juce::String ratio (int i) { return "ratio" + juce::String (i); }
    inline juce::String knee (int i) { return "knee" + juce::String (i); }
    inline juce::String attack (int i) { return "attack" + juce::String (i); }
    inline juce::String release (int i) { return "release" + juce::String (i); }
    inline juce::String gain (int i) { return "gain" + juce::String (i); }
    inline juce::String autoGain (int i) { return "autoGain" + juce::String (i); }
    inline juce::String sidechain (int i) { return "sidechain" + juce::String (i); }
    inline juce::String compressorBypass (int i) { return "compressorBypass" + juce::String (i); }
    inline juce::String solo (int i) { return "solo" + juce::String (i); }
    inline juce::String mute (int i) { return "mute" + juce::String (i); }
}
