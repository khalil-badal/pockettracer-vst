#include "PocketTracerLookAndFeel.h"
#include <BinaryData.h>

// Original Pocket Tracer palette — unchanged
const juce::Colour PocketTracerLookAndFeel::bgOuter      { 0xff080a12 };
const juce::Colour PocketTracerLookAndFeel::bgPanel      { 0xff0d1019 };
const juce::Colour PocketTracerLookAndFeel::accent       { 0xff2f6bff };
const juce::Colour PocketTracerLookAndFeel::accentBright { 0xff5b8cff };
const juce::Colour PocketTracerLookAndFeel::text         { 0xffeef1f8 };
const juce::Colour PocketTracerLookAndFeel::textDim      { 0xff8893ad };

// The embedded OpenDyslexic faces, loaded once and shared for the plugin's
// lifetime. Function-local statics keep this thread-safe and avoid a parallel
// registration system — every font accessor funnels through here.
static juce::Typeface::Ptr openDyslexic (bool bold)
{
    static juce::Typeface::Ptr regular = juce::Typeface::createSystemTypefaceFor (
        BinaryData::OpenDyslexicRegular_otf, (size_t) BinaryData::OpenDyslexicRegular_otfSize);
    static juce::Typeface::Ptr boldFace = juce::Typeface::createSystemTypefaceFor (
        BinaryData::OpenDyslexicBold_otf,    (size_t) BinaryData::OpenDyslexicBold_otfSize);
    return bold ? boldFace : regular;
}

// A named style counts as "bold" unless it is plain Regular or Light. This keeps
// the light/heavy emphasis distinction the old Inter/Barlow calls expressed,
// mapped onto OpenDyslexic's two weights.
static bool styleIsBold (const juce::String& style)
{
    return ! (style.equalsIgnoreCase ("Regular") || style.equalsIgnoreCase ("Light"));
}

PocketTracerLookAndFeel::PocketTracerLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId,   bgOuter);
    setColour (juce::Label::textColourId,                   text);
    setColour (juce::Slider::textBoxTextColourId,           text);
    setColour (juce::Slider::textBoxOutlineColourId,        juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId,     juce::Colours::transparentBlack);
    setColour (juce::TooltipWindow::backgroundColourId,     bgPanel);
    setColour (juce::TooltipWindow::textColourId,           text);
}

// ------------------------------------------------------------- font accessors --
juce::Font PocketTracerLookAndFeel::font (float height, bool bold)
{
    return juce::Font (juce::FontOptions().withTypeface (openDyslexic (bold)).withHeight (height));
}

juce::Font PocketTracerLookAndFeel::interFont (float height, const char* style)
{
    return font (height, styleIsBold (style));
}

juce::Font PocketTracerLookAndFeel::barlowFont (float height, const char* style)
{
    return font (height, styleIsBold (style));
}

juce::Font PocketTracerLookAndFeel::uiFont (float height, bool bold) const
{
    return font (height, bold);
}

juce::Font PocketTracerLookAndFeel::brandFont (float height) const
{
    return font (height, true);
}

// --------------------------------------------------------------- LNF overrides --
juce::Font PocketTracerLookAndFeel::getLabelFont (juce::Label&)
{
    return font (12.5f, false);
}

juce::Font PocketTracerLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return font (12.0f, true);
}

// -------------------------------------------- background (ported from Rootbound) --
// Cyan accent replaced with Pocket Tracer blue (0xFF2F6BFF).
void PocketTracerLookAndFeel::drawPluginBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (bgOuter);

    const float cx     = (float)width  * 0.5f;
    const float cy     = (float)height * 0.48f;
    const float radius = (float)juce::jmax (width, height) * 0.65f;

    juce::ColourGradient bloom (
        accent.withAlpha (0.055f), cx, cy,
        bgOuter.withAlpha (0.0f),  cx, cy + radius,
        true);
    g.setGradientFill (bloom);
    g.fillRect (0, 0, width, height);
}

// ------------------------------------------- rotary knob (ported from Rootbound) --
// Cyan accent replaced with Pocket Tracer blue (0xFF2F6BFF).
void PocketTracerLookAndFeel::drawRotarySlider (juce::Graphics& g,
                                                 int x, int y, int width, int height,
                                                 float sliderPos,
                                                 float rotaryStartAngle,
                                                 float rotaryEndAngle,
                                                 juce::Slider&)
{
    const float cx  = (float)x + (float)width  * 0.5f;
    const float cy  = (float)y + (float)height * 0.5f;
    const float r   = juce::jmin ((float)width, (float)height) * 0.42f;
    const float arc = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    // Shadow / depth ring
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - r - 3.0f, cy - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);

    // Outer rim
    g.setColour (juce::Colour (0xFF1E2C36));
    g.fillEllipse (cx - r - 1.5f, cy - r - 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f);

    // Knob body — subtle dark radial gradient
    {
        juce::ColourGradient grad (
            juce::Colour (0xFF253040), cx - r * 0.3f, cy - r * 0.3f,
            juce::Colour (0xFF141C22), cx + r * 0.5f, cy + r * 0.5f,
            false);
        g.setGradientFill (grad);
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    }

    // Subtle highlight arc (top-left shine)
    {
        juce::Path shine;
        shine.addArc (cx - r * 0.85f, cy - r * 0.85f, r * 1.7f, r * 1.7f,
                      rotaryStartAngle - 0.5f, rotaryStartAngle + 1.8f, true);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.strokePath (shine, juce::PathStrokeType (r * 0.18f));
    }

    // Track (unfilled arc, dark)
    {
        juce::Path trackPath;
        trackPath.addArc (cx - r * 0.88f, cy - r * 0.88f, r * 1.76f, r * 1.76f,
                          rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xFF0A1018));
        g.strokePath (trackPath, juce::PathStrokeType (r * 0.10f,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Filled value arc — blue
    {
        juce::Path arcPath;
        arcPath.addArc (cx - r * 0.88f, cy - r * 0.88f, r * 1.76f, r * 1.76f,
                        rotaryStartAngle, arc, true);
        // Outer glow
        g.setColour (accent.withAlpha (0.2f));
        g.strokePath (arcPath, juce::PathStrokeType (r * 0.18f,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Core
        g.setColour (accent.withAlpha (0.9f));
        g.strokePath (arcPath, juce::PathStrokeType (r * 0.09f,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // Indicator line + dot
    {
        const float ix = cx + std::sin (arc) * r * 0.60f;
        const float iy = cy - std::cos (arc) * r * 0.60f;
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawLine (cx, cy, ix, iy, 1.2f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (ix - 2.5f, iy - 2.5f, 5.0f, 5.0f);
    }

    // Inner shadow circle
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.fillEllipse (cx - r * 0.45f, cy - r * 0.45f, r * 0.9f, r * 0.9f);
}
