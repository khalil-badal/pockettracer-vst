#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "gui/PocketTracerLookAndFeel.h"

// ----- a labelled knob (name above, optional value readout below) -----
class PTKnob : public juce::Component
{
public:
    PTKnob (const juce::String& name, bool hasReadout);
    void resized() override;

    juce::Slider slider;
    juce::Label  nameLabel, readoutLabel;
    bool         showReadout = false;
};

// ----- a segmented DELAY TYPE button (label + LED, no icon) -----
class PTTypeButton : public juce::Button
{
public:
    explicit PTTypeButton (const juce::String& text);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// ----- a round footswitch toggle (name + LED + circle, no icon) -----
class PTToggle : public juce::Button
{
public:
    explicit PTToggle (const juce::String& text);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

// =============================================================================
// PluginContent — the entire faceplate, laid out at a fixed base size
// (kBaseWidth x kBaseHeight). The owning editor scales this component via a
// transform to realise the Small/Medium/Large window sizes, so none of the
// layout math below ever needs to know about the current UI size.
// =============================================================================
class PluginContent : public juce::Component,
                       private juce::Timer
{
public:
    static constexpr int kBaseWidth  = 720;
    static constexpr int kBaseHeight = 424;

    explicit PluginContent (PocketTracerAudioProcessor&);
    ~PluginContent() override;

    void paint   (juce::Graphics&) override;
    void resized () override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override { repaint(); }
    void updateTimeControl (bool synced);
    void refreshTimeReadout();

    PocketTracerAudioProcessor& processor;
    PocketTracerLookAndFeel     lnf;

    juce::Rectangle<float> panelBounds;

    PTKnob timeKnob     { "TIME",       true  };
    PTKnob feedbackKnob { "FEEDBACK",   false };
    PTKnob mixKnob      { "MIX",        false };
    PTKnob toneKnob     { "TONE",       false };
    PTKnob modKnob      { "MODULATION", false };
    PTKnob inputKnob    { "INPUT",      true  };
    PTKnob outputKnob   { "OUTPUT",     true  };

    std::unique_ptr<APVTS::SliderAttachment> feedbackAtt, mixAtt, toneAtt, modAtt, inAtt, outAtt;
    std::unique_ptr<APVTS::SliderAttachment> timeAtt, divisionAtt;

    juce::OwnedArray<PTTypeButton> typeButtons;
    std::unique_ptr<juce::ParameterAttachment> typeWatcher;

    PTToggle syncToggle     { "SYNC"      };
    PTToggle pingPongToggle { "PING-PONG" };
    PTToggle freezeToggle   { "FREEZE"    };
    std::unique_ptr<APVTS::ButtonAttachment> syncAtt, pingAtt, freezeAtt;
    std::unique_ptr<juce::ParameterAttachment> syncWatcher;

    bool isSynced = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginContent)
};

// =============================================================================
// Editor — hosts PluginContent and offers a right-click sizing menu
// (Small / Medium / Large). Small is the default window size.
// =============================================================================
class PocketTracerAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit PocketTracerAudioProcessorEditor (PocketTracerAudioProcessor&);
    ~PocketTracerAudioProcessorEditor() override;

    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    enum class UiSize { Small, Medium, Large };

    void applyUiSize (UiSize size);
    static float scaleFor (UiSize size);
    static const char* nameFor (UiSize size);

    PluginContent content;
    UiSize        uiSize = UiSize::Small;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PocketTracerAudioProcessorEditor)
};
