#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

class ChannelStripProcessor : public juce::AudioProcessor
{
public:
    ChannelStripProcessor();
    ~ChannelStripProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "My Channel Strip"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // Metering, read by the editor (dB)
    std::atomic<float> inputPeakDb { -100.0f };
    std::atomic<float> outputPeakDb { -100.0f };
    std::atomic<float> gainReductionDb { 0.0f };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void updateFilters();

    std::atomic<float>* prm (const char* id) { return apvts.getRawParameterValue (id); }

    // Parameter handles
    std::atomic<float> *inGain, *drive, *hpfFreq, *gateThresh, *gateRange,
                       *compThresh, *compRatio, *compAttack, *compRelease, *compMakeup,
                       *lfFreq, *lfGain, *lmfFreq, *lmfGain, *lmfQ,
                       *hmfFreq, *hmfGain, *hmfQ, *hfFreq, *hfGain, *outGain;

    using Filter = juce::dsp::IIR::Filter<float>;
    std::array<Filter, 2> hpf, lowShelf, lmf, hmf, highShelf;
    std::array<float, 12> lastFilterParams {};
    bool hpfActive = false;

    double sampleRateHz = 44100.0;

    // Dynamics state
    float gateEnv = 0.0f;
    float gateGainDb = 0.0f;
    float compGrDb = 0.0f;
};
