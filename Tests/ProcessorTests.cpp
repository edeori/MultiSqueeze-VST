#include "../Source/PluginProcessor.h"
#include <iostream>
#include <cmath>
int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    int failures = 0;
    auto check = [&] (bool ok, const char* name) { if (!ok) { ++failures; std::cerr << name << '\n'; } };
    for (bool hostBypass : { false, true })
    {
        MultiSqueezeAudioProcessor processor;
        auto set = [&] (const juce::String& id, float value) {
            auto* p = processor.apvts.getParameter(id);
            p->setValueNotifyingHost(p->convertTo0to1(value));
        };
        set(ParamIDs::lookahead, 1);
        set(ParamIDs::bypass, hostBypass ? 0 : 1);
        processor.prepareToPlay(48000, 512);
        const int latency = processor.getLatencySamples();
        check(latency > 0 && latency < 512, "lookahead latency");
        juce::AudioBuffer<float> audio(2, 512); audio.clear();
        audio.setSample(0, 0, 1); audio.setSample(1, 0, 1);
        juce::MidiBuffer midi;
        if (hostBypass) processor.processBlockBypassed(audio, midi);
        else processor.processBlock(audio, midi);
        for (int i=0; i<512; ++i)
            check(std::abs(audio.getSample(0,i) - (i == latency ? 1.0f : 0.0f)) < 1e-6f,
                  "bypass impulse must match announced latency");
        juce::MemoryBlock state; processor.getStateInformation(state);
        MultiSqueezeAudioProcessor restored;
        restored.setStateInformation(state.getData(), (int)state.getSize());
        check(restored.apvts.getRawParameterValue(ParamIDs::lookahead)->load() == 1, "state recall");
    }
    MultiSqueezeAudioProcessor processor;
    while (processor.addBand()) {}
    check(processor.getNumBands() == 16, "16 band limit");
    processor.prepareToPlay(22050, 512);
    juce::AudioBuffer<float> audio(2,512); audio.clear(); audio.setSample(0,0,0.5f);
    juce::MidiBuffer midi; processor.processBlock(audio,midi);
    for(int i=0;i<512;++i) check(std::isfinite(audio.getSample(0,i)), "low-rate 16 band output finite");
    std::cout << "Processor test failures: " << failures << '\n';
    return failures ? 1 : 0;
}
