#include "PluginEditor.h"

namespace
{
    struct KnobSpec { const char* id; const char* label; const char* suffix; };

    const KnobSpec specs[] = {
        // 0-4: input / filter / gate
        { "inGain",      "Input",     " dB" }, { "drive",       "Drive",   "" },
        { "hpf",         "HPF",       " Hz" }, { "gateThresh",  "Gate Thr", " dB" },
        { "gateRange",   "Gate Rng",  " dB" },
        // 5-9: compressor
        { "compThresh",  "Threshold", " dB" }, { "compRatio",   "Ratio",   ":1" },
        { "compAttack",  "Attack",    " ms" }, { "compRelease", "Release", " ms" },
        { "compMakeup",  "Makeup",    " dB" },
        // 10-19: EQ
        { "lfFreq",      "LF Freq",   " Hz" }, { "lfGain",      "LF Gain", " dB" },
        { "lmfFreq",     "LMF Freq",  " Hz" }, { "lmfGain",     "LMF Gain", " dB" },
        { "lmfQ",        "LMF Q",     "" },    { "hmfFreq",     "HMF Freq", " Hz" },
        { "hmfGain",     "HMF Gain",  " dB" }, { "hmfQ",        "HMF Q",   "" },
        { "hfFreq",      "HF Freq",   " Hz" }, { "hfGain",      "HF Gain", " dB" },
        // 20: output
        { "outGain",     "Output",    " dB" }
    };

    float dbToFraction (float db, float lo, float hi)
    {
        return juce::jlimit (0.0f, 1.0f, (db - lo) / (hi - lo));
    }
}

void MeterComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (4.0f);
    g.setColour (juce::Colour (0xff15181c));
    g.fillRoundedRectangle (area, 4.0f);

    const float barW = area.getWidth() / 3.0f;
    auto drawBar = [&] (int index, float fraction, juce::Colour c, bool fromTop, const juce::String& name)
    {
        auto col = juce::Rectangle<float> (area.getX() + barW * (float) index + 6.0f, area.getY() + 6.0f,
                                           barW - 12.0f, area.getHeight() - 24.0f);
        g.setColour (juce::Colour (0xff23272d));
        g.fillRect (col);
        g.setColour (c);
        const float h = col.getHeight() * fraction;
        if (fromTop) g.fillRect (col.withHeight (h));
        else         g.fillRect (col.withTop (col.getBottom() - h));
        g.setColour (juce::Colours::lightgrey);
        g.setFont (11.0f);
        g.drawText (name, (int) col.getX() - 4, (int) col.getBottom() + 2, (int) col.getWidth() + 8, 14,
                    juce::Justification::centred);
    };

    auto levelColour = [] (float db) { return db > -1.0f ? juce::Colours::red
                                            : db > -12.0f ? juce::Colours::orange
                                                          : juce::Colour (0xff4cc38a); };

    drawBar (0, dbToFraction (inDb,  -60.0f, 6.0f),  levelColour (inDb),  false, "IN");
    drawBar (1, dbToFraction (outDb, -60.0f, 6.0f),  levelColour (outDb), false, "OUT");
    drawBar (2, dbToFraction (-grDb,  0.0f, 24.0f),  juce::Colour (0xfff0b44c), true, "GR");
}

ChannelStripEditor::ChannelStripEditor (ChannelStripProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    for (const auto& s : specs)
    {
        auto k = std::make_unique<Knob>();
        k->slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        k->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        k->slider.setTextValueSuffix (s.suffix);
        k->slider.setNumDecimalPlacesToDisplay (1);
        k->label.setText (s.label, juce::dontSendNotification);
        k->label.setJustificationType (juce::Justification::centred);
        k->label.setFont (juce::FontOptions (12.0f));
        k->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            processor.apvts, s.id, k->slider);
        addAndMakeVisible (k->slider);
        addAndMakeVisible (k->label);
        knobs.push_back (std::move (k));
    }

    sections = {
        { "INPUT  /  HPF  /  GATE", { 0, 1, 2, 3, 4 } },
        { "COMPRESSOR",             { 5, 6, 7, 8, 9 } },
        { "EQUALIZER",              { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 } },
        { "OUTPUT",                 { 20 } }
    };

    addAndMakeVisible (meter);
    setSize (900, 620);
    startTimerHz (30);
}

ChannelStripEditor::~ChannelStripEditor()
{
    stopTimer();
}

void ChannelStripEditor::timerCallback()
{
    // Instant attack, steady fall-back for readable ballistics
    dispIn  = juce::jmax (processor.inputPeakDb.load(),  dispIn  - 2.0f);
    dispOut = juce::jmax (processor.outputPeakDb.load(), dispOut - 2.0f);
    dispGr  = juce::jmin (processor.gainReductionDb.load(), dispGr + 1.0f);
    meter.setLevels (dispIn, dispOut, dispGr);
}

void ChannelStripEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1c2025));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("MY CHANNEL STRIP", 16, 8, 400, 28, juce::Justification::centredLeft);

    // Section titles
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (36);
    area.removeFromRight (130); // meter column
    const int rowH = area.getHeight() / (int) sections.size();

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    for (size_t r = 0; r < sections.size(); ++r)
    {
        auto row = area.removeFromTop (rowH);
        g.setColour (juce::Colour (0xff8a93a0));
        g.drawText (sections[r].title, row.removeFromTop (16), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff2d333b));
        g.drawHorizontalLine (row.getY() - 16, (float) row.getX(), (float) row.getRight());
    }
}

void ChannelStripEditor::resized()
{
    auto area = getLocalBounds().reduced (12);
    area.removeFromTop (36);
    meter.setBounds (area.removeFromRight (120));
    area.removeFromRight (10);

    const int rowH = area.getHeight() / (int) sections.size();

    for (auto& sec : sections)
    {
        auto row = area.removeFromTop (rowH);
        row.removeFromTop (16); // title strip
        const int n = (int) sec.knobIndices.size();
        const int knobW = juce::jmin (84, row.getWidth() / juce::jmax (1, n));

        for (int i = 0; i < n; ++i)
        {
            auto cell = row.removeFromLeft (knobW);
            auto& k = *knobs[(size_t) sec.knobIndices[(size_t) i]];
            k.label.setBounds (cell.removeFromTop (16));
            k.slider.setBounds (cell);
        }
    }
}
