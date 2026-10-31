#pragma once
#include "PluginProcessor.h"

class MeterComponent : public juce::Component
{
public:
    void setLevels (float in, float out, float gr)
    {
        inDb = in; outDb = out; grDb = gr;
        repaint();
    }
    void paint (juce::Graphics& g) override;

private:
    float inDb = -100.0f, outDb = -100.0f, grDb = 0.0f;
};

class ChannelStripEditor : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit ChannelStripEditor (ChannelStripProcessor&);
    ~ChannelStripEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct Section
    {
        juce::String title;
        std::vector<int> knobIndices;
    };

    ChannelStripProcessor& processor;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<Section> sections;
    MeterComponent meter;

    float dispIn = -100.0f, dispOut = -100.0f, dispGr = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStripEditor)
};
