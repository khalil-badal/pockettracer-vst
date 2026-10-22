#include "PluginEditor.h"
#include "dsp/TempoSync.h"

using LNF = PocketTracerLookAndFeel;

// =============================================================================
// PTKnob
// =============================================================================
PTKnob::PTKnob (const juce::String& name, bool hasReadout) : showReadout (hasReadout)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    addAndMakeVisible (slider);

    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (LNF::font (12.5f, true));
    nameLabel.setColour (juce::Label::textColourId, LNF::text);
    addAndMakeVisible (nameLabel);

    if (showReadout)
    {
        readoutLabel.setJustificationType (juce::Justification::centred);
        readoutLabel.setFont (LNF::font (11.5f, false));
        readoutLabel.setColour (juce::Label::textColourId, LNF::text.withAlpha (0.70f));
        addAndMakeVisible (readoutLabel);
    }
}

void PTKnob::resized()
{
    // Reserve the name row (top) and readout row (bottom) on every knob, even when
    // no readout is shown, so all knobs render an identical diameter and the
    // spare bottom strip becomes natural breathing room below the label-less knobs.
    auto r = getLocalBounds();
    nameLabel.setBounds (r.removeFromTop (18));
    auto readArea = r.removeFromBottom (16);
    if (showReadout) readoutLabel.setBounds (readArea);
    slider.setBounds (r);
}

// =============================================================================
// PTTypeButton  (LED dot on left + centred label, no icon)
// =============================================================================
PTTypeButton::PTTypeButton (const juce::String& text) : juce::Button (text)
{
    setClickingTogglesState (false);
}

void PTTypeButton::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    const bool on = getToggleState();

    // Background fill + border
    g.setColour (on ? LNF::accent.withAlpha (0.16f) : juce::Colour (0xff10131d));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (on ? LNF::accent
                    : (highlighted ? LNF::textDim.withAlpha (0.6f) : juce::Colour (0xff20263a)));
    g.drawRoundedRectangle (b, 5.0f, on ? 1.6f : 1.0f);

    // LED dot on the left
    const float ledD = 5.0f;
    auto led = juce::Rectangle<float> (ledD, ledD)
                   .withCentre ({ b.getX() + 10.0f, b.getCentreY() });
    if (on) { g.setColour (LNF::accentBright.withAlpha (0.45f)); g.fillEllipse (led.expanded (3.0f)); }
    g.setColour (on ? LNF::accentBright : juce::Colour (0xff2a3046));
    g.fillEllipse (led);

    // Label centred in the area to the right of the LED dot. drawFittedText lets
    // the longest label (MULTI-TAP) shrink a touch to fit rather than clip.
    g.setColour (on ? LNF::accentBright : LNF::textDim);
    g.setFont (LNF::font (11.0f, true));
    auto textArea = b.withTrimmedLeft (18.0f).withTrimmedRight (4.0f).toNearestInt();
    g.drawFittedText (getButtonText(), textArea, juce::Justification::centred, 1, 0.85f);
}

// =============================================================================
// PTToggle  (name label + LED dot + plain indicator circle, no icon)
// =============================================================================
PTToggle::PTToggle (const juce::String& text) : juce::Button (text)
{
    setClickingTogglesState (true);
}

void PTToggle::paintButton (juce::Graphics& g, bool highlighted, bool)
{
    auto b   = getLocalBounds().toFloat();
    const bool on = getToggleState();

    // Name label at top
    g.setColour (on ? LNF::text : LNF::textDim);
    g.setFont (LNF::font (12.0f, true));
    g.drawText (getButtonText(), b.removeFromTop (16.0f), juce::Justification::centred, false);

    // LED indicator, centred in the space below the label (no circle body)
    auto led = juce::Rectangle<float> (8.0f, 8.0f).withCentre (b.getCentre());
    if (on) { g.setColour (LNF::accentBright.withAlpha (0.55f)); g.fillEllipse (led.expanded (5.0f)); }
    g.setColour (on ? LNF::accentBright
                    : (highlighted ? LNF::textDim : juce::Colour (0xff2a3046)));
    g.fillEllipse (led);
}

// =============================================================================
// PluginContent
// =============================================================================
PluginContent::PluginContent (PocketTracerAudioProcessor& p) : processor (p)
{
    setLookAndFeel (&lnf);
    auto& apvts = processor.apvts;

    for (auto* k : { &timeKnob, &feedbackKnob, &mixKnob, &toneKnob, &modKnob, &inputKnob, &outputKnob })
        addAndMakeVisible (k);

    feedbackAtt = std::make_unique<APVTS::SliderAttachment> (apvts, "feedback",   feedbackKnob.slider);
    mixAtt      = std::make_unique<APVTS::SliderAttachment> (apvts, "mix",        mixKnob.slider);
    toneAtt     = std::make_unique<APVTS::SliderAttachment> (apvts, "tone",       toneKnob.slider);
    modAtt      = std::make_unique<APVTS::SliderAttachment> (apvts, "modulation", modKnob.slider);
    inAtt       = std::make_unique<APVTS::SliderAttachment> (apvts, "inputGain",  inputKnob.slider);
    outAtt      = std::make_unique<APVTS::SliderAttachment> (apvts, "outputGain", outputKnob.slider);

    auto dbReadout = [] (PTKnob& k) {
        k.readoutLabel.setText (juce::String (k.slider.getValue(), 1) + " dB",
                                juce::dontSendNotification);
    };
    inputKnob.slider.onValueChange  = [this, dbReadout] { dbReadout (inputKnob); };
    outputKnob.slider.onValueChange = [this, dbReadout] { dbReadout (outputKnob); };
    dbReadout (inputKnob);
    dbReadout (outputKnob);

    timeKnob.slider.onValueChange = [this] { refreshTimeReadout(); };

    // DELAY TYPE buttons
    for (const char* name : { "DIGITAL", "ANALOG", "TAPE", "REVERSE", "LO-FI", "MULTI-TAP" })
    {
        auto* b = new PTTypeButton (name);
        addAndMakeVisible (b);
        typeButtons.add (b);
    }
    if (auto* typeParam = apvts.getParameter ("delayType"))
    {
        typeWatcher = std::make_unique<juce::ParameterAttachment> (*typeParam,
            [this] (float v) {
                const int idx = (int)v;
                for (int i = 0; i < typeButtons.size(); ++i)
                    typeButtons[i]->setToggleState (i == idx, juce::dontSendNotification);
            }, nullptr);
        for (int i = 0; i < typeButtons.size(); ++i)
            typeButtons[i]->onClick = [this, i] { typeWatcher->setValueAsCompleteGesture ((float)i); };
        typeWatcher->sendInitialUpdate();
    }

    for (auto* t : { &syncToggle, &pingPongToggle, &freezeToggle })
        addAndMakeVisible (t);
    syncAtt   = std::make_unique<APVTS::ButtonAttachment> (apvts, "sync",     syncToggle);
    pingAtt   = std::make_unique<APVTS::ButtonAttachment> (apvts, "pingpong", pingPongToggle);
    freezeAtt = std::make_unique<APVTS::ButtonAttachment> (apvts, "freeze",   freezeToggle);

    if (auto* syncParam = apvts.getParameter ("sync"))
    {
        syncWatcher = std::make_unique<juce::ParameterAttachment> (*syncParam,
            [this] (float v) { updateTimeControl (v >= 0.5f); }, nullptr);
        syncWatcher->sendInitialUpdate();
    }

    startTimerHz (15);
    setSize (kBaseWidth, kBaseHeight);
}

PluginContent::~PluginContent()
{
    setLookAndFeel (nullptr);
}

void PluginContent::updateTimeControl (bool synced)
{
    isSynced = synced;
    timeAtt.reset();
    divisionAtt.reset();

    if (synced)
        divisionAtt = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "division", timeKnob.slider);
    else
        timeAtt = std::make_unique<APVTS::SliderAttachment> (processor.apvts, "time", timeKnob.slider);

    refreshTimeReadout();
}

void PluginContent::refreshTimeReadout()
{
    if (isSynced)
    {
        auto names = TempoSync::getDivisionNames();
        const int idx = juce::jlimit (0, names.size() - 1, (int)std::round (timeKnob.slider.getValue()));
        timeKnob.readoutLabel.setText (names[idx], juce::dontSendNotification);
    }
    else
    {
        const double ms = timeKnob.slider.getValue();
        timeKnob.readoutLabel.setText (ms >= 1000.0 ? juce::String (ms / 1000.0, 2) + " s"
                                                    : juce::String ((int)std::round (ms)) + " ms",
                                       juce::dontSendNotification);
    }
}

// =============================================================================
// Restrained faceplate framing — matched to the Rootbound aesthetic.
// Just structure: an engraved inner border, corner L-brackets with a node dot,
// and a few short diagonal accent strokes near the top corners. Nothing else.
// Painted in the background layer, behind every control, in the existing colorway.
// =============================================================================
static void paintTechDecor (juce::Graphics& g, juce::Rectangle<int> win, juce::Rectangle<float> panel)
{
    juce::ignoreUnused (win);

    const juce::Colour bracket = LNF::accent.withAlpha (0.45f);
    const juce::Colour node    = LNF::accent.withAlpha (0.65f);
    const juce::Colour diagonal = LNF::accent.withAlpha (0.28f);
    const juce::Colour engrave  = LNF::text.withAlpha (0.05f);

    // Engraved inner border for a hint of depth
    g.setColour (engrave);
    g.drawRoundedRectangle (panel.reduced (3.5f), 13.0f, 1.0f);

    // Corner L-brackets, each with a small node dot at the vertex
    const float in = 12.0f, len = 15.0f;
    auto corner = [&] (float cx, float cy, int dx, int dy)
    {
        g.setColour (bracket);
        g.drawLine (cx, cy, cx + dx * len, cy, 1.3f);
        g.drawLine (cx, cy, cx, cy + dy * len, 1.3f);
        g.setColour (node);
        g.fillEllipse (cx - 1.6f, cy - 1.6f, 3.2f, 3.2f);
    };
    corner (panel.getX() + in,     panel.getY() + in,      +1, +1);
    corner (panel.getRight() - in, panel.getY() + in,      -1, +1);
    corner (panel.getX() + in,     panel.getBottom() - in, +1, -1);
    corner (panel.getRight() - in, panel.getBottom() - in, -1, -1);

    // A few short diagonal accent strokes tucked beside the top corners
    g.setColour (diagonal);
    for (int i = 0; i < 3; ++i)
    {
        const float o = i * 7.0f;
        g.drawLine (panel.getX() + 34.0f + o, panel.getY() + 40.0f,
                    panel.getX() + 46.0f + o, panel.getY() + 28.0f, 1.2f);   // top-left  ╱
        g.drawLine (panel.getRight() - 34.0f - o, panel.getY() + 40.0f,
                    panel.getRight() - 46.0f - o, panel.getY() + 28.0f, 1.2f); // top-right ╲
    }
}

void PluginContent::paint (juce::Graphics& g)
{
    lnf.drawPluginBackground (g, getWidth(), getHeight());

    // Central panel
    g.setColour (LNF::accent.withAlpha (0.20f));
    g.drawRoundedRectangle (panelBounds.expanded (1.5f), 18.0f, 2.0f);
    g.setColour (LNF::bgPanel);
    g.fillRoundedRectangle (panelBounds, 16.0f);
    g.setColour (juce::Colour (0xff1a2030));
    g.drawRoundedRectangle (panelBounds, 16.0f, 1.0f);

    // Decorative faceplate + technical linework (behind all controls and text)
    paintTechDecor (g, getLocalBounds(), panelBounds);

    // KHYLE AUDIO — lower-right of the window, outside the panel enclosure
    g.setFont (lnf.brandFont (11.5f));
    g.setColour (LNF::textDim.withAlpha (0.60f));
    g.drawText ("KHYLE AUDIO",
                juce::Rectangle<int> (getWidth() - 162, getHeight() - 20, 150, 16),
                juce::Justification::centredRight, false);

    // DELAY TYPE section label with flanking lines
    {
        auto row = panelBounds.withY (panelBounds.getY() + 210.0f).withHeight (18.0f);
        g.setFont (LNF::font (12.0f, true));
        g.setColour (LNF::textDim);
        const juce::String t = "DELAY TYPE";
        const float tw = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t) + 16.0f;
        g.drawText (t, row, juce::Justification::centred, false);
        const float ly = row.getCentreY();
        g.setColour (LNF::accent.withAlpha (0.4f));
        g.drawLine (panelBounds.getX() + 24.0f, ly, row.getCentreX() - tw * 0.5f, ly, 1.0f);
        g.drawLine (row.getCentreX() + tw * 0.5f, ly, panelBounds.getRight() - 24.0f, ly, 1.0f);
    }

    // Bottom branding — anchored just below the toggle row for a tight title block
    {
        auto logo = panelBounds.withY (panelBounds.getY() + 342.0f).withHeight (28.0f);

        g.setColour (LNF::text);
        g.setFont (lnf.brandFont (20.0f).withExtraKerningFactor (0.16f));
        g.drawText ("POCKET TRACER", logo, juce::Justification::centred, false);

        g.setColour (LNF::text.withAlpha (0.55f));
        g.setFont (LNF::font (12.5f, false).withExtraKerningFactor (0.55f));
        g.drawText ("DELAY", logo.translated (0, 26.0f).withHeight (16.0f),
                    juce::Justification::centred, false);

        g.setColour (LNF::accent.withAlpha (0.6f));
        const float oy = logo.getCentreY();
        g.drawLine (logo.getX() + 50, oy, logo.getX() + 82, oy, 1.0f);
        g.drawLine (logo.getRight() - 82, oy, logo.getRight() - 50, oy, 1.0f);
    }

    // I/O dB scales
    auto drawIOExtras = [&] (PTKnob& k)
    {
        auto kb = k.getBounds().toFloat();
        g.setColour (LNF::textDim);
        g.setFont (LNF::font (10.5f, false));
        auto scale = kb.withY (kb.getBottom() - 28.0f).withHeight (13.0f);
        g.drawText ("-24", scale.removeFromLeft  (kb.getWidth() / 3.0f), juce::Justification::centred, false);
        g.drawText ("+24", scale.removeFromRight (kb.getWidth() / 3.0f), juce::Justification::centred, false);
        g.drawText ("0",   scale,                                          juce::Justification::centred, false);
    };
    drawIOExtras (inputKnob);
    drawIOExtras (outputKnob);
}

void PluginContent::resized()
{
    auto r = getLocalBounds();
    const int ioW = 82;
    auto ioLeft  = r.removeFromLeft (ioW);
    auto ioRight = r.removeFromRight (ioW);
    panelBounds  = r.reduced (6, 12).toFloat();

    const auto pb = panelBounds.toNearestInt();

    // I/O knobs — upper-left / upper-right, aligned with the top knob row
    inputKnob.setBounds  (juce::Rectangle<int> (0, 0, 76, 104).withCentre ({ ioLeft.getCentreX(),  pb.getY() + 60 }));
    outputKnob.setBounds (juce::Rectangle<int> (0, 0, 76, 104).withCentre ({ ioRight.getCentreX(), pb.getY() + 60 }));

    // Main knobs — two rows of three (5 knobs; centre-bottom slot left open)
    const int kw = 84, kh = 98;
    const int leftX  = pb.getX() + (int)(pb.getWidth() * 0.18f);
    const int midX   = pb.getCentreX();
    const int rightX = pb.getX() + (int)(pb.getWidth() * 0.82f);
    const int row1Y  = pb.getY() + 8;
    const int row2Y  = pb.getY() + 106;

    auto place = [&] (PTKnob& k, int cx, int cy) {
        k.setBounds (juce::Rectangle<int> (0, 0, kw, kh).withCentre ({ cx, cy + kh / 2 }));
    };
    place (timeKnob,     leftX,  row1Y);
    place (feedbackKnob, midX,   row1Y);
    place (mixKnob,      rightX, row1Y);
    place (toneKnob,     leftX,  row2Y);
    place (modKnob,      rightX, row2Y);

    // DELAY TYPE row — compact, no icon needed
    auto typeRow = juce::Rectangle<int> (pb.getX() + 8, pb.getY() + 230, pb.getWidth() - 16, 30);
    const int n  = typeButtons.size();
    const int bw = typeRow.getWidth() / n;
    for (int i = 0; i < n; ++i)
        typeButtons[i]->setBounds (typeRow.removeFromLeft (i == n - 1 ? typeRow.getWidth() : bw).reduced (2, 0));

    // Toggles row — no icon, taller circle proportion
    auto togRow = juce::Rectangle<int> (pb.getX(), pb.getY() + 272, pb.getWidth(), 56);
    const int tw  = 100;
    const int gap = (togRow.getWidth() - 3 * tw) / 4;
    int tx = togRow.getX() + gap;
    for (auto* t : { &syncToggle, &pingPongToggle, &freezeToggle })
    {
        t->setBounds (tx, togRow.getY(), tw, togRow.getHeight());
        tx += tw + gap;
    }
}

// =============================================================================
// Editor — hosts PluginContent at a fixed base layout and scales it via a
// transform to realise the Small/Medium/Large window sizes. Right-click
// anywhere on the plugin to change size; Small is the default.
// =============================================================================
PocketTracerAudioProcessorEditor::PocketTracerAudioProcessorEditor (PocketTracerAudioProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);
    content.addMouseListener (this, true); // let right-clicks on any child bubble up to us
    setResizable (false, false);
    applyUiSize (UiSize::Small);
}

PocketTracerAudioProcessorEditor::~PocketTracerAudioProcessorEditor() = default;

float PocketTracerAudioProcessorEditor::scaleFor (UiSize size)
{
    switch (size)
    {
        case UiSize::Medium: return 1.25f;
        case UiSize::Large:  return 1.5f;
        case UiSize::Small:
        default:             return 1.0f;
    }
}

const char* PocketTracerAudioProcessorEditor::nameFor (UiSize size)
{
    switch (size)
    {
        case UiSize::Medium: return "Medium";
        case UiSize::Large:  return "Large";
        case UiSize::Small:
        default:             return "Small";
    }
}

void PocketTracerAudioProcessorEditor::applyUiSize (UiSize size)
{
    uiSize = size;
    const float scale = scaleFor (size);
    content.setTransform (juce::AffineTransform::scale (scale));
    setSize ((int) std::round (PluginContent::kBaseWidth  * scale),
             (int) std::round (PluginContent::kBaseHeight * scale));
}

void PocketTracerAudioProcessorEditor::resized()
{
    content.setBounds (0, 0, PluginContent::kBaseWidth, PluginContent::kBaseHeight);
}

void PocketTracerAudioProcessorEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isPopupMenu())
        return;

    juce::PopupMenu menu;
    menu.addItem (1, juce::String ("Small (Default)"),  true, uiSize == UiSize::Small);
    menu.addItem (2, juce::String ("Medium"),            true, uiSize == UiSize::Medium);
    menu.addItem (3, juce::String ("Large"),             true, uiSize == UiSize::Large);

    menu.showMenuAsync (juce::PopupMenu::Options(), [this] (int result)
    {
        if (result == 1) applyUiSize (UiSize::Small);
        else if (result == 2) applyUiSize (UiSize::Medium);
        else if (result == 3) applyUiSize (UiSize::Large);
    });
}
