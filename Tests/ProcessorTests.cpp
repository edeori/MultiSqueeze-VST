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
    {
        MultiSqueezeAudioProcessor routed;
        auto set = [&] (const juce::String& id, float value)
        {
            auto* parameter = routed.apvts.getParameter (id);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        };
        // DC lives in the first band. Its muted, heavily trimmed signal must
        // still drive the high band's detector at the original crossover level.
        set (ParamIDs::mute (0), 1);
        set (ParamIDs::gain (0), -24);
        set (ParamIDs::detectorBand (2), 1);
        routed.prepareToPlay (48000, 256);
        auto processDC = [&]
        {
            juce::AudioBuffer<float> block (2, 256);
            juce::MidiBuffer events;
            for (int n = 0; n < 160; ++n)
            {
                for (int ch = 0; ch < 2; ++ch)
                    juce::FloatVectorOperations::fill (block.getWritePointer (ch), 1.0f, 256);
                routed.processBlock (block, events);
            }
        };
        processDC();
        check (routed.getGainReductionDb (2) > 12, "muted pre-compression band drives high detector");
        set (ParamIDs::detectorBand (2), 4);
        processDC();
        check (routed.getGainReductionDb (2) < 0.01f, "inactive detector falls back to self");

        routed.setSelectedBand (2);
        std::unique_ptr<juce::AudioProcessorEditor> editor (routed.createEditor());
        juce::ComboBox* menu = nullptr;
        for (auto* component : editor->getChildren())
            if (auto* combo = dynamic_cast<juce::ComboBox*> (component))
                menu = combo;
        check (menu != nullptr, "detector menu exists");
        if (menu != nullptr)
        {
            check (menu->getNumItems() == 7 && menu->getItemText (5) == "Band 1"
                   && menu->getItemText (6) == "Band 2" && menu->getText() == "Internal",
                   "only other active bands appear in detector menu");
            menu->setSelectedItemIndex (6, juce::sendNotificationSync);
            juce::MemoryBlock state;
            routed.getStateInformation (state);
            MultiSqueezeAudioProcessor recalled;
            recalled.setStateInformation (state.getData(), (int) state.getSize());
            check (recalled.apvts.getRawParameterValue (ParamIDs::detectorBand (2))->load() == 2,
                   "selected detector band persists");
            menu->setSelectedItemIndex (4, juce::sendNotificationSync);
            check (routed.apvts.getRawParameterValue (ParamIDs::detectorBand (2))->load() == 0
                   && routed.apvts.getRawParameterValue (ParamIDs::sidechain (2))->load() == 4,
                   "external bus remains selectable after using a band");
            auto click = [&] (const juce::String& label)
            {
                for (auto* component : editor->getChildren())
                    if (auto* button = dynamic_cast<juce::TextButton*> (component))
                        if (button->getButtonText() == label)
                            button->onClick();
            };
            click ("+ ADD BAND");
            check (menu->getNumItems() == 8, "added band updates open editor");
            click ("- REMOVE");
            check (menu->getNumItems() == 7, "removed band disappears from open editor");
        }
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
