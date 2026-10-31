#include "PluginProcessor.h"
#include "PluginEditor.h"

using Decibels = juce::Decibels;

ChannelStripProcessor::ChannelStripProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    inGain      = prm ("inGain");      drive       = prm ("drive");
    hpfFreq     = prm ("hpf");         gateThresh  = prm ("gateThresh");
    gateRange   = prm ("gateRange");   compThresh  = prm ("compThresh");
    compRatio   = prm ("compRatio");   compAttack  = prm ("compAttack");
    compRelease = prm ("compRelease"); compMakeup  = prm ("compMakeup");
    lfFreq      = prm ("lfFreq");      lfGain      = prm ("lfGain");
    lmfFreq     = prm ("lmfFreq");     lmfGain     = prm ("lmfGain");
    lmfQ        = prm ("lmfQ");        hmfFreq     = prm ("hmfFreq");
    hmfGain     = prm ("hmfGain");     hmfQ        = prm ("hmfQ");
    hfFreq      = prm ("hfFreq");      hfGain      = prm ("hfGain");
    outGain     = prm ("outGain");
}

juce::AudioProcessorValueTreeState::ParameterLayout ChannelStripProcessor::createLayout()
{
    using R = juce::NormalisableRange<float>;
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;

    auto add = [&p] (const char* id, const char* name, R range, float def)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name, range, def));
    };

    // Input / saturation
    add ("inGain", "Input Gain", R (-24.0f, 24.0f, 0.1f), 0.0f);
    add ("drive",  "Drive",      R (0.0f, 1.0f, 0.01f), 0.0f);

    // High-pass (20 Hz = off)
    add ("hpf", "HPF", R (20.0f, 500.0f, 1.0f, 0.4f), 20.0f);

    // Gate (-80 dB = off)
    add ("gateThresh", "Gate Threshold", R (-80.0f, 0.0f, 0.1f), -80.0f);
    add ("gateRange",  "Gate Range",     R (-60.0f, 0.0f, 0.1f), -40.0f);

    // Compressor (threshold 0 dB = effectively off)
    add ("compThresh",  "Comp Threshold", R (-40.0f, 0.0f, 0.1f), 0.0f);
    add ("compRatio",   "Comp Ratio",     R (1.0f, 20.0f, 0.1f, 0.5f), 2.0f);
    add ("compAttack",  "Comp Attack",    R (0.1f, 100.0f, 0.1f, 0.4f), 10.0f);
    add ("compRelease", "Comp Release",   R (10.0f, 1000.0f, 1.0f, 0.4f), 150.0f);
    add ("compMakeup",  "Comp Makeup",    R (0.0f, 24.0f, 0.1f), 0.0f);

    // 4-band EQ
    add ("lfFreq",  "LF Freq",  R (30.0f, 300.0f, 1.0f, 0.5f), 100.0f);
    add ("lfGain",  "LF Gain",  R (-15.0f, 15.0f, 0.1f), 0.0f);
    add ("lmfFreq", "LMF Freq", R (200.0f, 2000.0f, 1.0f, 0.5f), 600.0f);
    add ("lmfGain", "LMF Gain", R (-15.0f, 15.0f, 0.1f), 0.0f);
    add ("lmfQ",    "LMF Q",    R (0.3f, 5.0f, 0.01f, 0.5f), 1.0f);
    add ("hmfFreq", "HMF Freq", R (1000.0f, 8000.0f, 1.0f, 0.5f), 3000.0f);
    add ("hmfGain", "HMF Gain", R (-15.0f, 15.0f, 0.1f), 0.0f);
    add ("hmfQ",    "HMF Q",    R (0.3f, 5.0f, 0.01f, 0.5f), 1.0f);
    add ("hfFreq",  "HF Freq",  R (3000.0f, 16000.0f, 1.0f, 0.5f), 8000.0f);
    add ("hfGain",  "HF Gain",  R (-15.0f, 15.0f, 0.1f), 0.0f);

    // Output
    add ("outGain", "Output Gain", R (-24.0f, 12.0f, 0.1f), 0.0f);

    return { p.begin(), p.end() };
}

bool ChannelStripProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void ChannelStripProcessor::prepareToPlay (double sampleRate, int)
{
    sampleRateHz = sampleRate;
    gateEnv = 0.0f;
    gateGainDb = 0.0f;
    compGrDb = 0.0f;

    for (auto* arr : { &hpf, &lowShelf, &lmf, &hmf, &highShelf })
        for (auto& f : *arr)
            f.reset();

    lastFilterParams.fill (-1.0f); // force coefficient rebuild
    updateFilters();
}

void ChannelStripProcessor::updateFilters()
{
    const std::array<float, 12> now {
        hpfFreq->load(), lfFreq->load(), lfGain->load(), lmfFreq->load(), lmfGain->load(),
        lmfQ->load(), hmfFreq->load(), hmfGain->load(), hmfQ->load(), hfFreq->load(),
        hfGain->load(), (float) sampleRateHz };

    if (now == lastFilterParams)
        return;
    lastFilterParams = now;

    const auto sr = sampleRateHz;
    const float maxF = (float) (sr * 0.45);
    constexpr float shelfQ = 0.7071f;
    auto g = [] (float db) { return Decibels::decibelsToGain (db); };

    hpfActive = now[0] > 20.5f;
    auto cHpf = juce::dsp::IIR::Coefficients<float>::makeHighPass (sr, juce::jmin (now[0], maxF));
    auto cLf  = juce::dsp::IIR::Coefficients<float>::makeLowShelf (sr, juce::jmin (now[1], maxF), shelfQ, g (now[2]));
    auto cLmf = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, juce::jmin (now[3], maxF), now[5], g (now[4]));
    auto cHmf = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sr, juce::jmin (now[6], maxF), now[8], g (now[7]));
    auto cHf  = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sr, juce::jmin (now[9], maxF), shelfQ, g (now[10]));

    for (int ch = 0; ch < 2; ++ch)
    {
        hpf[(size_t) ch].coefficients       = cHpf;
        lowShelf[(size_t) ch].coefficients  = cLf;
        lmf[(size_t) ch].coefficients       = cLmf;
        hmf[(size_t) ch].coefficients       = cHmf;
        highShelf[(size_t) ch].coefficients = cHf;
    }
}

void ChannelStripProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    updateFilters();

    // Snapshot parameters for this block
    const float inLin     = Decibels::decibelsToGain (inGain->load());
    const float driveAmt  = drive->load();
    const float satK      = 1.0f + driveAmt * 9.0f;
    const float satComp   = 1.0f / std::sqrt (satK);
    const bool  gateOn    = gateThresh->load() > -79.9f;
    const float gateThr   = gateThresh->load();
    const float gateRng   = gateRange->load();
    const float cThr      = compThresh->load();
    const float cRatio    = compRatio->load();
    const float makeupDb  = compMakeup->load();
    const float outLin    = Decibels::decibelsToGain (outGain->load());
    const float knee      = 6.0f;
    const float sr        = (float) sampleRateHz;

    auto coefFor = [sr] (float ms) { return std::exp (-1.0f / (juce::jmax (ms, 0.01f) * 0.001f * sr)); };
    const float atkCoef      = coefFor (compAttack->load());
    const float relCoef      = coefFor (compRelease->load());
    const float gateOpenCoef = coefFor (1.0f);
    const float gateCloseCoef = coefFor (80.0f);
    const float gateEnvRel   = coefFor (50.0f);

    float* chans[2] = { nullptr, nullptr };
    for (int ch = 0; ch < numCh; ++ch)
        chans[ch] = buffer.getWritePointer (ch);

    float inPeak = 0.0f, outPeak = 0.0f, minGr = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        // 1) Input gain + saturation + HPF
        float peakAfterHpf = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            float x = chans[ch][i];
            inPeak = juce::jmax (inPeak, std::abs (x));

            x *= inLin;
            if (driveAmt > 0.001f)
                x = std::tanh (satK * x) * satComp;
            if (hpfActive)
                x = hpf[(size_t) ch].processSample (x);

            chans[ch][i] = x;
            peakAfterHpf = juce::jmax (peakAfterHpf, std::abs (x));
        }

        // 2) Gate / downward expander (linked across channels)
        gateEnv = juce::jmax (peakAfterHpf, gateEnv * gateEnvRel);
        const float gateEnvDb = Decibels::gainToDecibels (gateEnv, -120.0f);
        const float gateTarget = (gateOn && gateEnvDb < gateThr) ? gateRng : 0.0f;
        const float gCoef = gateTarget < gateGainDb ? gateCloseCoef : gateOpenCoef;
        gateGainDb = gateTarget + gCoef * (gateGainDb - gateTarget);
        const float gateGain = Decibels::decibelsToGain (gateGainDb);

        float peakAfterGate = 0.0f;
        for (int ch = 0; ch < numCh; ++ch)
        {
            chans[ch][i] *= gateGain;
            peakAfterGate = juce::jmax (peakAfterGate, std::abs (chans[ch][i]));
        }

        // 3) Compressor: feed-forward, soft knee, linked detector
        const float levelDb = Decibels::gainToDecibels (peakAfterGate, -120.0f);
        const float over = levelDb - cThr;
        float grTarget = 0.0f;
        const float slope = (1.0f / cRatio) - 1.0f;
        if (2.0f * over > knee)
            grTarget = slope * over;
        else if (2.0f * std::abs (over) <= knee)
            grTarget = slope * (over + knee * 0.5f) * (over + knee * 0.5f) / (2.0f * knee);

        const float cCoef = grTarget < compGrDb ? atkCoef : relCoef;
        compGrDb = grTarget + cCoef * (compGrDb - grTarget);
        minGr = juce::jmin (minGr, compGrDb);
        const float compGain = Decibels::decibelsToGain (compGrDb + makeupDb);

        // 4) EQ and 5) output gain
        for (int ch = 0; ch < numCh; ++ch)
        {
            float x = chans[ch][i] * compGain;
            x = lowShelf[(size_t) ch].processSample (x);
            x = lmf[(size_t) ch].processSample (x);
            x = hmf[(size_t) ch].processSample (x);
            x = highShelf[(size_t) ch].processSample (x);
            x *= outLin;

            chans[ch][i] = x;
            outPeak = juce::jmax (outPeak, std::abs (x));
        }
    }

    inputPeakDb.store (Decibels::gainToDecibels (inPeak, -100.0f));
    outputPeakDb.store (Decibels::gainToDecibels (outPeak, -100.0f));
    gainReductionDb.store (minGr);
}

juce::AudioProcessorEditor* ChannelStripProcessor::createEditor()
{
    return new ChannelStripEditor (*this);
}

void ChannelStripProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void ChannelStripProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ChannelStripProcessor();
}
