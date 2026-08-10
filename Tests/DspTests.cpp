#include "../Source/CompressorBand.h"
#include "../Source/Parameters.h"
#include <juce_core/juce_core.h>
#include <cmath>
#include <iostream>
#include <string>

namespace
{
    int failures = 0;

    bool closeEnough (float a, float b, float tolerance = 1.0e-5f)
    {
        return std::abs (a - b) <= tolerance;
    }

    void check (bool condition, const std::string& name)
    {
        if (! condition)
        {
            std::cerr << "FAILED: " << name << '\n';
            ++failures;
        }
        else
        {
            std::cout << "  ok: " << name << '\n';
        }
    }

    // Closed-form standard quadratic soft-knee reduction, mirroring
    // CompressorBand::process exactly, for computing expected values instead
    // of guessing tolerances.
    float standardReductionDb (float overDb, float ratio, float kneeDb)
    {
        const auto slope = 1.0f - 1.0f / ratio;
        if (overDb <= -kneeDb * 0.5f)
            return 0.0f;
        if (overDb >= kneeDb * 0.5f)
            return overDb * slope;
        const auto x = overDb + kneeDb * 0.5f;
        return slope * (x * x) / (2.0f * juce::jmax (0.001f, kneeDb));
    }

    // Closed-form Auto Gain, mirroring CompressorBand::process exactly: a
    // fixed static amount (half the reduction a signal at 0 dBFS would get),
    // scaled by how much of that maximum is actually being reduced right now.
    float staticAutoMakeupDb (float thresholdDb, float ratio, float ceiling = 30.0f)
    {
        const auto slope = 1.0f - 1.0f / ratio;
        const auto maxReductionAt0dBFS = juce::jmax (0.001f, -thresholdDb * slope);
        return juce::jlimit (0.0f, ceiling, maxReductionAt0dBFS * 0.5f);
    }

    float scaledAutoMakeupDb (float thresholdDb, float ratio, float kneeDb, float overDb, float ceiling = 30.0f)
    {
        const auto slope = 1.0f - 1.0f / ratio;
        const auto maxReductionAt0dBFS = juce::jmax (0.001f, -thresholdDb * slope);
        const auto staticMakeup = juce::jlimit (0.0f, ceiling, maxReductionAt0dBFS * 0.5f);
        const auto reduction = standardReductionDb (overDb, ratio, kneeDb);
        return staticMakeup * juce::jlimit (0.0f, 1.0f, reduction / maxReductionAt0dBFS);
    }

    // Feeds `totalSamples` of a constant amplitude through the compressor in
    // realistic-sized blocks, letting internal state (envelope, lookahead
    // line) evolve exactly as it would in real-time use.
    void feedConstant (CompressorBand& compressor, const CompressorBand::Settings& settings,
                       float amplitude, int totalSamples, int channels = 1)
    {
        constexpr int blockSize = 1024;
        juce::AudioBuffer<float> block (channels, blockSize);
        int remaining = totalSamples;
        while (remaining > 0)
        {
            const auto n = std::min (remaining, blockSize);
            block.setSize (channels, n, false, false, true);
            for (int ch = 0; ch < channels; ++ch)
                for (int s = 0; s < n; ++s)
                    block.setSample (ch, s, amplitude);
            compressor.process (block, nullptr, settings);
            remaining -= n;
        }
    }

    // Processes one final small block of a constant amplitude and returns it,
    // for inspecting the compressor's settled output/metering precisely.
    juce::AudioBuffer<float> probeConstant (CompressorBand& compressor, const CompressorBand::Settings& settings,
                                            float amplitude, int probeSamples, int channels = 1)
    {
        juce::AudioBuffer<float> block (channels, probeSamples);
        for (int ch = 0; ch < channels; ++ch)
            for (int s = 0; s < probeSamples; ++s)
                block.setSample (ch, s, amplitude);
        compressor.process (block, nullptr, settings);
        return block;
    }

    constexpr double sampleRate = 48000.0;

    void testBypassPassesThrough()
    {
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        juce::AudioBuffer<float> bypassed (2, 512);
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 512; ++s)
                bypassed.setSample (ch, s, 0.25f);

        CompressorBand::Settings settings;
        settings.bypassed = true;
        compressor.process (bypassed, nullptr, settings);
        check (closeEnough (bypassed.getSample (0, 511), 0.25f), "bypass: signal passes through unchanged");
    }

    void testBelowKneeNoReduction()
    {
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        CompressorBand::Settings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.kneeDb = 6.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        // -30 dBFS is 10 dB under Threshold, well outside the +-3 dB knee.
        const auto amplitude = juce::Decibels::decibelsToGain (-30.0f);
        juce::AudioBuffer<float> buf (1, 48000);
        for (int s = 0; s < 48000; ++s)
            buf.setSample (0, s, amplitude);
        compressor.process (buf, nullptr, settings);

        check (closeEnough (compressor.getGainReductionDb(), 0.0f, 0.01f),
              "standard curve: no reduction below the knee");
        check (closeEnough (buf.getSample (0, 47999), amplitude, 0.0005f),
              "standard curve: signal below the knee passes through unchanged");
    }

    void testAboveKneeMatchesStandardRatioFormula()
    {
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        CompressorBand::Settings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.kneeDb = 6.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        juce::AudioBuffer<float> buf (2, 48000);
        for (int ch = 0; ch < 2; ++ch)
            for (int s = 0; s < 48000; ++s)
                buf.setSample (ch, s, 1.0f);
        compressor.process (buf, nullptr, settings);

        const auto over = 0.0f - settings.thresholdDb;
        const auto expected = juce::Decibels::decibelsToGain (
            -standardReductionDb (over, settings.ratio, settings.kneeDb));
        check (closeEnough (buf.getSample (0, 47999), expected, 0.002f),
              "standard curve: matches plain Threshold/Ratio slope above the knee");
        check (closeEnough (buf.getSample (0, 47999), buf.getSample (1, 47999)),
              "standard curve: both stereo channels receive identical gain");
    }

    void testWithinKneeMatchesQuadraticFormula()
    {
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        CompressorBand::Settings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.kneeDb = 12.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        // Sitting exactly at Threshold, inside the +-6 dB knee.
        const auto amplitude = juce::Decibels::decibelsToGain (settings.thresholdDb);
        juce::AudioBuffer<float> buf (1, 48000);
        for (int s = 0; s < 48000; ++s)
            buf.setSample (0, s, amplitude);
        compressor.process (buf, nullptr, settings);

        const auto expectedReduction = standardReductionDb (0.0f, settings.ratio, settings.kneeDb);
        check (closeEnough (compressor.getGainReductionDb(), expectedReduction, 0.05f),
              "standard curve: matches the quadratic knee formula exactly at Threshold");
        check (expectedReduction > 0.0f && expectedReduction < (1.0f - 1.0f / settings.ratio) * (settings.kneeDb * 0.5f),
              "standard curve: reduction at Threshold is a fraction of the full-ratio slope, not the full amount");
    }

    void testWiderKneeGivesMoreReductionAtThreshold()
    {
        CompressorBand::Settings narrow, wide;
        narrow.thresholdDb = wide.thresholdDb = -20.0f;
        narrow.ratio = wide.ratio = 4.0f;
        narrow.attackMs = wide.attackMs = 1.0f;
        narrow.releaseMs = wide.releaseMs = 100.0f;
        narrow.kneeDb = 2.0f;
        wide.kneeDb = 18.0f;

        CompressorBand narrowComp, wideComp;
        narrowComp.prepare (sampleRate);
        wideComp.prepare (sampleRate);

        const auto amplitude = juce::Decibels::decibelsToGain (-20.0f);
        juce::AudioBuffer<float> narrowBuf (1, 48000), wideBuf (1, 48000);
        for (int s = 0; s < 48000; ++s)
        {
            narrowBuf.setSample (0, s, amplitude);
            wideBuf.setSample (0, s, amplitude);
        }
        narrowComp.process (narrowBuf, nullptr, narrow);
        wideComp.process (wideBuf, nullptr, wide);

        check (wideComp.getGainReductionDb() > narrowComp.getGainReductionDb() + 0.5f,
              "standard curve: a wider knee compresses more right at Threshold (softer, earlier onset)");
    }

    void testZeroKneeIsHardKnee()
    {
        CompressorBand below, above;
        below.prepare (sampleRate);
        above.prepare (sampleRate);
        CompressorBand::Settings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.kneeDb = 0.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        juce::AudioBuffer<float> belowBuf (1, 48000), aboveBuf (1, 48000);
        for (int s = 0; s < 48000; ++s)
        {
            belowBuf.setSample (0, s, juce::Decibels::decibelsToGain (-21.0f)); // 1 dB under
            aboveBuf.setSample (0, s, juce::Decibels::decibelsToGain (-19.0f)); // 1 dB over
        }
        below.process (belowBuf, nullptr, settings);
        above.process (aboveBuf, nullptr, settings);

        check (closeEnough (below.getGainReductionDb(), 0.0f, 0.01f),
              "zero knee: no reduction just under Threshold");
        check (closeEnough (above.getGainReductionDb(), 1.0f * (1.0f - 1.0f / settings.ratio), 0.05f),
              "zero knee: full-ratio slope kicks in immediately just over Threshold");
    }

    void testSidechainDetectorDrivesReduction()
    {
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        CompressorBand::Settings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        juce::AudioBuffer<float> keyed (1, 4800), sidechain (1, 4800);
        for (int s = 0; s < 4800; ++s)
        {
            keyed.setSample (0, s, 0.5f);
            sidechain.setSample (0, s, 1.0f);
        }
        compressor.process (keyed, &sidechain, settings);
        check (keyed.getSample (0, 4799) < 0.2f, "sidechain: external detector drives gain reduction on the main signal");
    }

    void testLimiterHoldsBoundedAtExtremeLow()
    {
        // Standard behaviour on purpose: an extreme Threshold with a modest
        // Ratio still means heavy, continuous reduction -- there is no hidden
        // auto-limiter kicking in. Verify the math stays sane (finite,
        // predictable) rather than any special-cased "becomes a limiter".
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        CompressorBand::Settings settings;
        settings.thresholdDb = -60.0f;
        settings.ratio = 4.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        juce::AudioBuffer<float> buf (1, 48000);
        for (int s = 0; s < 48000; ++s)
            buf.setSample (0, s, 1.0f);
        compressor.process (buf, nullptr, settings);

        const auto over = 0.0f - settings.thresholdDb;
        const auto expected = juce::Decibels::decibelsToGain (
            -standardReductionDb (over, settings.ratio, settings.kneeDb));
        check (closeEnough (buf.getSample (0, 47999), expected, 0.002f),
              "standard curve: plain Ratio slope still applies even at an extreme Threshold");
    }

    CompressorBand::Settings autoGainSettings (float thresholdDb = -20.0f, float ratio = 4.0f)
    {
        CompressorBand::Settings settings;
        settings.thresholdDb = thresholdDb;
        settings.ratio = ratio;
        settings.attackMs = 5.0f;
        settings.releaseMs = 100.0f;
        settings.autoGainEnabled = true;
        return settings;
    }

    // A 1 s settle is orders of magnitude longer than any Attack time these
    // tests use (1-10 ms), so the envelope -- and therefore the reduction and
    // the make-up derived from it -- is fully at steady state afterwards.
    constexpr int settleSamples = 48000;

    void testAutoGainMatchesStaticFormulaAt0dBFS()
    {
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        auto settings = autoGainSettings (-24.0f, 6.0f);
        settings.kneeDb = 0.0f;

        // At 0 dBFS the signal is as far over Threshold as it can get, so the
        // actual reduction equals the theoretical 0 dBFS maximum and the
        // scaling factor is exactly 1 -- make-up should hit the full static
        // amount precisely.
        feedConstant (compressor, settings, 1.0f, settleSamples);
        probeConstant (compressor, settings, 1.0f, 8);
        check (closeEnough (compressor.getAutoMakeupDb(),
                            staticAutoMakeupDb (settings.thresholdDb, settings.ratio), 1.0e-3f),
              "auto gain: matches the full static formula when the signal is at 0 dBFS");
    }

    void testAutoGainIsZeroWhenNothingIsCompressed()
    {
        // The bug this guards against: a static Threshold/Ratio-only formula
        // would add make-up even when the material never reaches Threshold at
        // all, i.e. while zero compression is actually happening.
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        const auto settings = autoGainSettings (-12.0f, 8.0f);
        const auto quietAmplitude = juce::Decibels::decibelsToGain (-30.0f); // 18 dB under Threshold

        feedConstant (compressor, settings, quietAmplitude, settleSamples);
        probeConstant (compressor, settings, quietAmplitude, 8);
        check (closeEnough (compressor.getGainReductionDb(), 0.0f, 0.01f),
              "auto gain test setup: sanity check that this signal really isn't being reduced");
        check (closeEnough (compressor.getAutoMakeupDb(), 0.0f, 0.01f),
              "auto gain: exactly 0 dB make-up when nothing is actually being compressed");
    }

    void testAutoGainScalesWithActualReduction()
    {
        CompressorBand quiet, moderate, loud;
        quiet.prepare (sampleRate);
        moderate.prepare (sampleRate);
        loud.prepare (sampleRate);
        auto settings = autoGainSettings (-18.0f, 5.0f);
        settings.kneeDb = 0.0f;

        feedConstant (quiet, settings, juce::Decibels::decibelsToGain (-30.0f), settleSamples);    // below Threshold
        feedConstant (moderate, settings, juce::Decibels::decibelsToGain (-9.0f), settleSamples);  // 9 dB over
        feedConstant (loud, settings, 1.0f, settleSamples);                                        // 0 dBFS, max over
        probeConstant (quiet, settings, juce::Decibels::decibelsToGain (-30.0f), 8);
        probeConstant (moderate, settings, juce::Decibels::decibelsToGain (-9.0f), 8);
        probeConstant (loud, settings, 1.0f, 8);

        const auto quietMakeup = quiet.getAutoMakeupDb();
        const auto moderateMakeup = moderate.getAutoMakeupDb();
        const auto loudMakeup = loud.getAutoMakeupDb();

        check (closeEnough (quietMakeup, 0.0f, 0.01f),
              "auto gain: still 0 dB below Threshold");
        check (moderateMakeup > quietMakeup && moderateMakeup < loudMakeup,
              "auto gain: scales up smoothly as the signal goes further over Threshold");
        check (closeEnough (loudMakeup, staticAutoMakeupDb (settings.thresholdDb, settings.ratio), 1.0e-3f),
              "auto gain: reaches the full static amount only once the signal hits 0 dBFS");
        check (closeEnough (moderateMakeup,
                            scaledAutoMakeupDb (settings.thresholdDb, settings.ratio, settings.kneeDb, 9.0f), 1.0e-3f),
              "auto gain: matches the closed-form scaled formula exactly at a moderate level");
    }

    void testAutoGainUpdatesInstantlyWithSettings()
    {
        // The envelope only tracks signal amplitude, not Threshold/Ratio, and
        // the amplitude here never changes -- so once it has settled once,
        // changing Threshold/Ratio needs no further settling: the very next
        // sample already reflects the new formula.
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        auto settings = autoGainSettings (-12.0f, 3.0f);
        settings.kneeDb = 0.0f;
        feedConstant (compressor, settings, 1.0f, settleSamples);
        probeConstant (compressor, settings, 1.0f, 8);
        const auto first = compressor.getAutoMakeupDb();

        settings.thresholdDb = -40.0f;
        settings.ratio = 8.0f;
        probeConstant (compressor, settings, 1.0f, 8);
        const auto second = compressor.getAutoMakeupDb();

        check (closeEnough (first, staticAutoMakeupDb (-12.0f, 3.0f), 1.0e-3f),
              "auto gain: matches formula for the first Threshold/Ratio setting");
        check (closeEnough (second, staticAutoMakeupDb (-40.0f, 8.0f), 1.0e-3f),
              "auto gain: updates instantly (no lag/memory) when Threshold/Ratio change");
        check (second > first,
              "auto gain: a more aggressive setting produces more make-up, immediately");
    }

    void testAutoGainCapsAtCeilingForExtremeSetting()
    {
        // -60 dB / 20:1 (the UI's actual extremes) only asks for 28.5 dB --
        // comfortably under the 30 dB ceiling, which exists purely as a safety
        // net above that reachable maximum. Drive the formula past it directly
        // to prove the clamp itself works, since normal use never reaches it.
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        const auto settings = autoGainSettings (-200.0f, 20.0f);

        feedConstant (compressor, settings, 1.0f, settleSamples);
        probeConstant (compressor, settings, 1.0f, 8);
        check (closeEnough (compressor.getAutoMakeupDb(), 30.0f, 0.01f),
              "auto gain: caps at the 30 dB safety ceiling if ever driven past it");
    }

    void testAutoGainDisabledAppliesNoCompensation()
    {
        CompressorBand compressor;
        compressor.prepare (sampleRate);
        auto settings = autoGainSettings (-20.0f);
        settings.autoGainEnabled = false;

        feedConstant (compressor, settings, 1.0f, settleSamples);
        probeConstant (compressor, settings, 1.0f, 8);
        check (closeEnough (compressor.getAutoMakeupDb(), 0.0f, 1.0e-4f),
              "auto gain: reports 0 dB make-up while disabled, even under heavy reduction");
    }

    void testLookaheadEngagesReductionBeforeTransientReachesOutput()
    {
        const auto lookaheadSamples = juce::jmax (1, (int) std::round (
            0.001 * MultiSqueezeConstants::lookaheadMs * sampleRate));

        CompressorBand::Settings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.attackMs = 10.0f;
        settings.releaseMs = 100.0f;

        constexpr int silenceLen = 1000;

        // Without lookahead: the transient appears in the output immediately,
        // so the very first loud output sample should show ~no reduction yet.
        CompressorBand noLookahead;
        noLookahead.prepare (sampleRate);
        settings.lookaheadEnabled = false;
        feedConstant (noLookahead, settings, 0.0f, silenceLen);
        probeConstant (noLookahead, settings, 1.0f, 1);
        const auto reductionWithoutLookahead = noLookahead.getGainReductionDb();

        // With lookahead: by the time the (delayed) transient exits the delay
        // line, the detector has already been looking at it for lookaheadSamples
        // samples, so reduction should already be well underway.
        CompressorBand withLookahead;
        withLookahead.prepare (sampleRate);
        settings.lookaheadEnabled = true;
        feedConstant (withLookahead, settings, 0.0f, silenceLen);
        const auto probe = probeConstant (withLookahead, settings, 1.0f, lookaheadSamples + 1);
        const auto reductionWithLookahead = withLookahead.getGainReductionDb();

        check (reductionWithoutLookahead < 1.0f,
              "lookahead off: reduction has not engaged yet on the very first transient sample");
        check (reductionWithLookahead > 5.0f,
              "lookahead on: reduction is already well underway by the time the transient exits the delay line");
        check (reductionWithLookahead > reductionWithoutLookahead + 5.0f,
              "lookahead: measurably reacts earlier than without it, on an identical transient");
        check (probe.getSample (0, lookaheadSamples) > 0.05f,
              "lookahead: the delayed transient does exit the output at exactly the expected sample");
    }

    void testLookaheadDoesNotAlterSteadyStateGain()
    {
        CompressorBand::Settings settings;
        settings.thresholdDb = -20.0f;
        settings.ratio = 4.0f;
        settings.attackMs = 1.0f;
        settings.releaseMs = 100.0f;

        CompressorBand withoutLookahead, withLookahead;
        withoutLookahead.prepare (sampleRate);
        withLookahead.prepare (sampleRate);

        settings.lookaheadEnabled = false;
        feedConstant (withoutLookahead, settings, 1.0f, (int) sampleRate);
        const auto probeA = probeConstant (withoutLookahead, settings, 1.0f, 8);

        settings.lookaheadEnabled = true;
        feedConstant (withLookahead, settings, 1.0f, (int) sampleRate);
        const auto probeB = probeConstant (withLookahead, settings, 1.0f, 8);

        check (closeEnough (probeA.getSample (0, 7), probeB.getSample (0, 7), 0.0005f),
              "lookahead: steady-state gain reduction is identical with or without lookahead, only timing differs");
    }
}

int main()
{
    testBypassPassesThrough();
    testBelowKneeNoReduction();
    testAboveKneeMatchesStandardRatioFormula();
    testWithinKneeMatchesQuadraticFormula();
    testWiderKneeGivesMoreReductionAtThreshold();
    testZeroKneeIsHardKnee();
    testSidechainDetectorDrivesReduction();
    testLimiterHoldsBoundedAtExtremeLow();
    testAutoGainMatchesStaticFormulaAt0dBFS();
    testAutoGainIsZeroWhenNothingIsCompressed();
    testAutoGainScalesWithActualReduction();
    testAutoGainUpdatesInstantlyWithSettings();
    testAutoGainCapsAtCeilingForExtremeSetting();
    testAutoGainDisabledAppliesNoCompensation();
    testLookaheadEngagesReductionBeforeTransientReachesOutput();
    testLookaheadDoesNotAlterSteadyStateGain();

    if (failures == 0)
    {
        std::cout << "All MultiSqueeze DSP tests passed.\n";
        return 0;
    }
    std::cerr << failures << " MultiSqueeze DSP test(s) failed.\n";
    return 1;
}
