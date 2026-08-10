#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    std::unique_ptr<juce::AudioParameterFloat> floatParam (const juce::String& id,
                                                            const juce::String& name,
                                                            juce::NormalisableRange<float> range,
                                                            float value,
                                                            const juce::String& unit = {})
    {
        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                            range, value, unit);
    }
}

juce::AudioProcessor::BusesProperties MultiSqueezeAudioProcessor::createBuses()
{
    auto buses = BusesProperties()
                    .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < MultiSqueezeConstants::numSidechainBuses; ++i)
        buses = buses.withInput ("Sidechain " + juce::String (i + 1), juce::AudioChannelSet::stereo(), false);
    return buses;
}

MultiSqueezeAudioProcessor::MultiSqueezeAudioProcessor()
    : AudioProcessor (createBuses())
{
    bypassParam = apvts.getRawParameterValue (ParamIDs::bypass);
    outputParam = apvts.getRawParameterValue (ParamIDs::output);
    lookaheadParam = apvts.getRawParameterValue (ParamIDs::lookahead);

    for (int i = 0; i < MultiSqueezeConstants::maxSplits; ++i)
    {
        splitActiveParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::splitActive (i));
        splitFrequencyParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::splitFrequency (i));
    }

    for (int i = 0; i < MultiSqueezeConstants::maxBands; ++i)
    {
        thresholdParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::threshold (i));
        ratioParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::ratio (i));
        kneeParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::knee (i));
        attackParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::attack (i));
        releaseParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::release (i));
        gainParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::gain (i));
        autoGainParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::autoGain (i));
        sidechainParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::sidechain (i));
        compressorBypassParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::compressorBypass (i));
        soloParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::solo (i));
        muteParams[(size_t) i] = apvts.getRawParameterValue (ParamIDs::mute (i));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MultiSqueezeAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { ParamIDs::bypass, 1 }, "Bypass", false));
    layout.add (floatParam (ParamIDs::output, "Output", { -24.0f, 24.0f, 0.01f }, 0.0f, "dB"));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { ParamIDs::lookahead, 1 }, "Lookahead", false));

    juce::NormalisableRange<float> frequencyRange { MultiSqueezeConstants::minFrequencyHz,
                                                    MultiSqueezeConstants::maxFrequencyHz, 0.1f, 0.25f };
    for (int i = 0; i < MultiSqueezeConstants::maxSplits; ++i)
    {
        const auto defaultFrequency = 120.0f * std::pow (3.2f, (float) i);
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::splitActive (i), 1 }, "Split " + juce::String (i + 1), i < 2));
        layout.add (floatParam (ParamIDs::splitFrequency (i), "Split frequency " + juce::String (i + 1),
                                frequencyRange, juce::jlimit (25.0f, 19000.0f, defaultFrequency), "Hz"));
    }

    juce::NormalisableRange<float> ratioRange { 1.0f, 20.0f, 0.01f, 0.45f };
    juce::NormalisableRange<float> attackRange { 0.1f, 200.0f, 0.01f, 0.35f };
    juce::NormalisableRange<float> releaseRange { 5.0f, 2000.0f, 0.1f, 0.35f };
    juce::StringArray sidechains { "Internal", "Sidechain 1", "Sidechain 2", "Sidechain 3", "Sidechain 4" };

    for (int i = 0; i < MultiSqueezeConstants::maxBands; ++i)
    {
        const auto suffix = " " + juce::String (i + 1);
        layout.add (floatParam (ParamIDs::threshold (i), "Threshold" + suffix, { -60.0f, 0.0f, 0.01f }, -18.0f, "dB"));
        layout.add (floatParam (ParamIDs::ratio (i), "Ratio" + suffix, ratioRange, 4.0f, ":1"));
        layout.add (floatParam (ParamIDs::knee (i), "Knee" + suffix, { 0.0f, 24.0f, 0.01f }, 6.0f, "dB"));
        layout.add (floatParam (ParamIDs::attack (i), "Attack" + suffix, attackRange, 20.0f, "ms"));
        layout.add (floatParam (ParamIDs::release (i), "Release" + suffix, releaseRange, 120.0f, "ms"));
        layout.add (floatParam (ParamIDs::gain (i), "Band gain" + suffix, { -24.0f, 24.0f, 0.01f }, 0.0f, "dB"));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::autoGain (i), 1 }, "Auto gain" + suffix, true));
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { ParamIDs::sidechain (i), 1 }, "Detector" + suffix, sidechains, 0));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::compressorBypass (i), 1 }, "Compressor bypass" + suffix, false));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::solo (i), 1 }, "Solo" + suffix, false));
        layout.add (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { ParamIDs::mute (i), 1 }, "Mute" + suffix, false));
    }
    return layout;
}

bool MultiSqueezeAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn = layouts.getChannelSet (true, 0);
    const auto mainOut = layouts.getChannelSet (false, 0);
    if ((mainIn != juce::AudioChannelSet::mono() && mainIn != juce::AudioChannelSet::stereo()) || mainIn != mainOut)
        return false;

    for (int bus = 1; bus < layouts.inputBuses.size(); ++bus)
    {
        const auto set = layouts.getChannelSet (true, bus);
        if (! set.isDisabled() && set != juce::AudioChannelSet::mono() && set != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void MultiSqueezeAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const auto channels = juce::jmax (1, getMainBusNumOutputChannels());
    inputScratch.setSize (channels, samplesPerBlock);
    splitter.prepare ({ sampleRate, (juce::uint32) samplesPerBlock, (juce::uint32) channels });
    for (auto& bandBuffer : bandBuffers)
        bandBuffer.setSize (channels, samplesPerBlock);
    for (auto& compressor : compressors)
        compressor.prepare (sampleRate);

    lookaheadLatencySamples = juce::jmax (1, (int) std::round (0.001 * MultiSqueezeConstants::lookaheadMs * sampleRate));
    lastLookaheadEnabled = lookaheadParam->load() >= 0.5f;
    setLatencySamples (lastLookaheadEnabled ? lookaheadLatencySamples : 0);
}

int MultiSqueezeAudioProcessor::getActiveSplits (
    std::array<ActiveSplit, MultiSqueezeConstants::maxSplits>& result) const noexcept
{
    int count = 0;
    for (int i = 0; i < MultiSqueezeConstants::maxSplits; ++i)
        if (splitActiveParams[(size_t) i]->load() >= 0.5f)
            result[(size_t) count++] = { splitFrequencyParams[(size_t) i]->load(), i };
    std::sort (result.begin(), result.begin() + count,
               [] (const ActiveSplit& a, const ActiveSplit& b) { return a.frequency < b.frequency; });
    return count;
}

int MultiSqueezeAudioProcessor::getNumBands() const noexcept
{
    int splits = 0;
    for (auto* active : splitActiveParams)
        splits += active->load() >= 0.5f ? 1 : 0;
    return splits + 1;
}

void MultiSqueezeAudioProcessor::setSelectedBand (int band) noexcept
{
    selectedBand.store (juce::jlimit (0, getNumBands() - 1, band));
}

float MultiSqueezeAudioProcessor::getBandLowFrequency (int band) const noexcept
{
    std::array<ActiveSplit, MultiSqueezeConstants::maxSplits> splits {};
    const auto count = getActiveSplits (splits);
    band = juce::jlimit (0, count, band);
    return band == 0 ? MultiSqueezeConstants::minFrequencyHz : splits[(size_t) band - 1].frequency;
}

float MultiSqueezeAudioProcessor::getBandHighFrequency (int band) const noexcept
{
    std::array<ActiveSplit, MultiSqueezeConstants::maxSplits> splits {};
    const auto count = getActiveSplits (splits);
    band = juce::jlimit (0, count, band);
    return band == count ? MultiSqueezeConstants::maxFrequencyHz : splits[(size_t) band].frequency;
}

void MultiSqueezeAudioProcessor::setBandUpperFrequency (int band, float frequencyHz)
{
    std::array<ActiveSplit, MultiSqueezeConstants::maxSplits> splits {};
    const auto count = getActiveSplits (splits);
    if (band < 0 || band >= count)
        return;

    const auto minimum = band == 0 ? MultiSqueezeConstants::minFrequencyHz : splits[(size_t) band - 1].frequency * 1.01f;
    const auto maximum = band == count - 1 ? MultiSqueezeConstants::maxFrequencyHz : splits[(size_t) band + 1].frequency / 1.01f;
    if (auto* parameter = apvts.getParameter (ParamIDs::splitFrequency (splits[(size_t) band].parameterSlot)))
    {
        const auto value = juce::jlimit (minimum, maximum, frequencyHz);
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (apvts.getParameterRange (parameter->getParameterID()).convertTo0to1 (value));
        parameter->endChangeGesture();
    }
}

bool MultiSqueezeAudioProcessor::addBand (float preferredFrequencyHz)
{
    std::array<ActiveSplit, MultiSqueezeConstants::maxSplits> splits {};
    const auto count = getActiveSplits (splits);
    if (count >= MultiSqueezeConstants::maxSplits)
        return false;

    int freeSlot = 0;
    while (freeSlot < MultiSqueezeConstants::maxSplits && splitActiveParams[(size_t) freeSlot]->load() >= 0.5f)
        ++freeSlot;

    if (preferredFrequencyHz <= 0.0f)
    {
        float widestRatio = 0.0f;
        float lower = MultiSqueezeConstants::minFrequencyHz;
        float upper = MultiSqueezeConstants::maxFrequencyHz;
        for (int interval = 0; interval <= count; ++interval)
        {
            const auto lo = interval == 0 ? MultiSqueezeConstants::minFrequencyHz : splits[(size_t) interval - 1].frequency;
            const auto hi = interval == count ? MultiSqueezeConstants::maxFrequencyHz : splits[(size_t) interval].frequency;
            if (hi / lo > widestRatio) { widestRatio = hi / lo; lower = lo; upper = hi; }
        }
        preferredFrequencyHz = std::sqrt (lower * upper);
    }

    if (auto* frequency = apvts.getParameter (ParamIDs::splitFrequency (freeSlot)))
    {
        frequency->beginChangeGesture();
        frequency->setValueNotifyingHost (apvts.getParameterRange (frequency->getParameterID()).convertTo0to1 (
            juce::jlimit (MultiSqueezeConstants::minFrequencyHz, MultiSqueezeConstants::maxFrequencyHz, preferredFrequencyHz)));
        frequency->endChangeGesture();
    }
    if (auto* active = apvts.getParameter (ParamIDs::splitActive (freeSlot)))
    {
        active->beginChangeGesture();
        active->setValueNotifyingHost (1.0f);
        active->endChangeGesture();
    }
    return true;
}

bool MultiSqueezeAudioProcessor::removeSelectedBand()
{
    std::array<ActiveSplit, MultiSqueezeConstants::maxSplits> splits {};
    const auto count = getActiveSplits (splits);
    if (count == 0)
        return false;
    const auto band = juce::jlimit (0, count, selectedBand.load());
    const auto splitToRemove = band == count ? band - 1 : band;
    if (auto* active = apvts.getParameter (ParamIDs::splitActive (splits[(size_t) splitToRemove].parameterSlot)))
    {
        active->beginChangeGesture();
        active->setValueNotifyingHost (0.0f);
        active->endChangeGesture();
    }
    setSelectedBand (juce::jmin (band, count - 1));
    return true;
}

float MultiSqueezeAudioProcessor::getGainReductionDb (int band) const noexcept
{
    return compressors[(size_t) juce::jlimit (0, MultiSqueezeConstants::maxBands - 1, band)].getGainReductionDb();
}

float MultiSqueezeAudioProcessor::getAutoMakeupDb (int band) const noexcept
{
    return compressors[(size_t) juce::jlimit (0, MultiSqueezeConstants::maxBands - 1, band)].getAutoMakeupDb();
}

void MultiSqueezeAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainInput = getBusBuffer (buffer, true, 0);
    auto mainOutput = getBusBuffer (buffer, false, 0);
    const auto channels = mainOutput.getNumChannels();
    const auto samples = mainOutput.getNumSamples();

    const auto lookaheadEnabled = lookaheadParam->load() >= 0.5f;
    if (lookaheadEnabled != lastLookaheadEnabled)
    {
        lastLookaheadEnabled = lookaheadEnabled;
        setLatencySamples (lookaheadEnabled ? lookaheadLatencySamples : 0);
    }

    inputScratch.setSize (channels, samples, false, false, true);
    inputScratch.clear();
    for (int channel = 0; channel < juce::jmin (channels, mainInput.getNumChannels()); ++channel)
        inputScratch.copyFrom (channel, 0, mainInput, channel, 0, samples);

    for (int sample = 0; sample < samples; ++sample)
        inputSpectrum.push (inputScratch.getSample (0, sample));

    if (bypassParam->load() >= 0.5f)
    {
        for (int channel = 0; channel < channels; ++channel)
            mainOutput.copyFrom (channel, 0, inputScratch, juce::jmin (channel, inputScratch.getNumChannels() - 1), 0, samples);
        for (int sample = 0; sample < samples; ++sample)
            outputSpectrum.push (mainOutput.getSample (0, sample));
        return;
    }

    std::array<ActiveSplit, MultiSqueezeConstants::maxSplits> activeSplits {};
    const auto numSplits = getActiveSplits (activeSplits);
    std::array<float, MultiSqueezeConstants::maxSplits> frequencies {};
    for (int i = 0; i < numSplits; ++i)
        frequencies[(size_t) i] = activeSplits[(size_t) i].frequency;

    splitter.process (juce::dsp::AudioBlock<const float> (inputScratch), frequencies, numSplits, bandBuffers);
    mainOutput.clear();
    bool anySolo = false;
    for (int band = 0; band <= numSplits; ++band)
        anySolo = anySolo || soloParams[(size_t) band]->load() >= 0.5f;

    for (int band = 0; band <= numSplits; ++band)
    {
        const auto scChoice = juce::roundToInt (sidechainParams[(size_t) band]->load());
        juce::AudioBuffer<float> sidechainView;
        const juce::AudioBuffer<float>* detector = nullptr;
        if (scChoice > 0 && scChoice <= MultiSqueezeConstants::numSidechainBuses
            && getBusCount (true) > scChoice && ! getBus (true, scChoice)->isEnabled())
            detector = nullptr;
        else if (scChoice > 0 && scChoice <= MultiSqueezeConstants::numSidechainBuses)
        {
            sidechainView = getBusBuffer (buffer, true, scChoice);
            if (sidechainView.getNumChannels() > 0)
                detector = &sidechainView;
        }

        CompressorBand::Settings settings;
        settings.thresholdDb = thresholdParams[(size_t) band]->load();
        settings.ratio = ratioParams[(size_t) band]->load();
        settings.kneeDb = kneeParams[(size_t) band]->load();
        settings.attackMs = attackParams[(size_t) band]->load();
        settings.releaseMs = releaseParams[(size_t) band]->load();
        settings.makeupDb = gainParams[(size_t) band]->load();
        settings.autoGainEnabled = autoGainParams[(size_t) band]->load() >= 0.5f;
        settings.lookaheadEnabled = lookaheadEnabled;
        settings.bypassed = compressorBypassParams[(size_t) band]->load() >= 0.5f;
        compressors[(size_t) band].process (bandBuffers[(size_t) band], detector, settings);

        const auto shouldMix = muteParams[(size_t) band]->load() < 0.5f
                               && (! anySolo || soloParams[(size_t) band]->load() >= 0.5f);
        if (shouldMix)
            for (int channel = 0; channel < channels; ++channel)
                mainOutput.addFrom (channel, 0, bandBuffers[(size_t) band], channel, 0, samples);
    }

    mainOutput.applyGain (juce::Decibels::decibelsToGain (outputParam->load()));
    for (int sample = 0; sample < samples; ++sample)
        outputSpectrum.push (mainOutput.getSample (0, sample));
}

juce::AudioProcessorEditor* MultiSqueezeAudioProcessor::createEditor()
{
    return new MultiSqueezeAudioProcessorEditor (*this);
}

juce::AudioProcessorParameter* MultiSqueezeAudioProcessor::getBypassParameter() const
{
    return apvts.getParameter (ParamIDs::bypass);
}

void MultiSqueezeAudioProcessor::getStateInformation (juce::MemoryBlock& destination)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destination);
}

void MultiSqueezeAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
    setSelectedBand (selectedBand.load());
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MultiSqueezeAudioProcessor();
}
