#pragma once
#include <JuceHeader.h>

class PocketTracerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PocketTracerLookAndFeel();

    // Draw the plugin background. Call this first in PluginEditor::paint().
    void drawPluginBackground (juce::Graphics& g, int width, int height);

    // LookAndFeel overrides
    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle,
                           juce::Slider&) override;

    juce::Font getLabelFont      (juce::Label&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // Brand palette — original Pocket Tracer colors, unchanged
    static const juce::Colour bgOuter;
    static const juce::Colour bgPanel;
    static const juce::Colour accent;
    static const juce::Colour accentBright;
    static const juce::Colour text;
    static const juce::Colour textDim;

    // -------------------------------------------------------------------------
    // Type system — the whole UI is set in OpenDyslexic (embedded via BinaryData).
    // The accessor names below are kept for call-site compatibility; every one of
    // them now returns an OpenDyslexic face. The "style" argument only selects
    // weight: any bold-ish style maps to OpenDyslexic Bold, otherwise Regular.
    // -------------------------------------------------------------------------
    static juce::Font font (float height, bool bold);            // canonical accessor
    static juce::Font interFont  (float height, const char* style = "Regular");
    static juce::Font barlowFont (float height, const char* style = "ExtraBold");

    juce::Font uiFont    (float height, bool bold = false) const;
    juce::Font brandFont (float height) const;
};
