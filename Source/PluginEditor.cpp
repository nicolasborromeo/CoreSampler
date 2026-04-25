#include "PluginEditor.h"

static const juce::Colour kBg        { 0xff1a1a2e };
static const juce::Colour kPanel     { 0xff0d0d1a };
static const juce::Colour kBorder    { 0xff3a3a5a };
static const juce::Colour kLabel     { 0xff9999bb };
static const juce::Colour kDivider   { 0xff2a2a45 };

// ============================================================
// Constructor
// ============================================================
CoreSamplerEditor::CoreSamplerEditor (CoreSamplerProcessor& p)
    : AudioProcessorEditor (&p),
      processorRef  (p),
      filterTypeAtt (p.apvts, "filterType", filterTypeBox),
      filterFreqAtt (p.apvts, "filterFreq", filterFreqKnob),
      filterResAtt  (p.apvts, "filterRes",  filterResKnob),
      attackAtt     (p.apvts, "attack",     attackKnob),
      decayAtt      (p.apvts, "decay",      decayKnob),
      sustainAtt    (p.apvts, "sustain",    sustainKnob),
      releaseAtt    (p.apvts, "release",    releaseKnob),
      gainAtt       (p.apvts, "gain",       gainKnob),
      transposeAtt  (p.apvts, "transpose",  transposeSlider)
{
    setSize (720, 380);

    // --- Load button ---
    loadButton.setLookAndFeel (&laf);
    addAndMakeVisible (loadButton);
    loadButton.onClick = [this]
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Choose a sample...",
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.wav;*.aif;*.aiff;*.mp3;*.flac");

        fileChooser->launchAsync (
            juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [this] (const juce::FileChooser& fc)
            {
                auto chosen = fc.getResult();
                if (chosen.existsAsFile())
                {
                    processorRef.loadSample (chosen);
                    sampleLabel.setText (chosen.getFileName(), juce::dontSendNotification);
                }
            });
    };

    sampleLabel.setText ("No sample loaded", juce::dontSendNotification);
    sampleLabel.setJustificationType (juce::Justification::centredLeft);
    sampleLabel.setColour (juce::Label::textColourId, kLabel);
    addAndMakeVisible (sampleLabel);

    // --- Filter type combo ---
    filterTypeBox.addItem ("LPF", 1);
    filterTypeBox.addItem ("HPF", 2);
    filterTypeBox.setLookAndFeel (&laf);
    addAndMakeVisible (filterTypeBox);

    // filterLabel — now shown as the column title above the dropdown
    filterLabel.setText ("FILTER", juce::dontSendNotification);
    filterLabel.setJustificationType (juce::Justification::centred);
    filterLabel.setFont (juce::FontOptions (9.5f));
    filterLabel.setColour (juce::Label::textColourId, kLabel);
    addAndMakeVisible (filterLabel);

    // --- Slope buttons (radio-style, blue when active) ---
    auto setupSlope = [this] (juce::TextButton& btn, int choiceIndex)
    {
        btn.setClickingTogglesState (true);
        btn.setRadioGroupId (1);
        btn.setColour (juce::TextButton::buttonColourId,  juce::Colour (0xff1e1e35));
        btn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff1a3a7a)); // blue active
        btn.setColour (juce::TextButton::textColourOffId, kLabel);
        btn.setColour (juce::TextButton::textColourOnId,  juce::Colours::white);
        addAndMakeVisible (btn);
        btn.onClick = [this, choiceIndex]
        {
            if (auto* p = processorRef.apvts.getParameter ("filterSlope"))
                p->setValueNotifyingHost (p->convertTo0to1 ((float) choiceIndex));
        };
    };
    slopeButton12.setButtonText ("12");
    slopeButton24.setButtonText ("24");
    setupSlope (slopeButton12, 0);
    setupSlope (slopeButton24, 1);
    slopeButton12.setLookAndFeel (&laf);
    slopeButton24.setLookAndFeel (&laf);

    // Sync slope button state to saved parameter on open
    updateSlopeButtons();

    // --- Filter knobs ---
    setupKnob (filterFreqKnob, filterFreqLabel, "FREQ", -1); // keep raw frequency display
    setupKnob (filterResKnob,  filterResLabel,  "RES",   2);

    // Tag ADSR sliders BEFORE setupKnob — setupKnob calls setTextBoxStyle which
    // triggers createSliderTextBox, which copies the componentID to the label.
    // If we set the ID after setupKnob the label gets stamped with "" instead.
    // "ms"  → value is in seconds, drawLabel converts to integer ms + " ms"
    // "pct" → value is 0–1, drawLabel converts to integer 0–100 + "%"
    attackKnob .setComponentID ("ms");
    decayKnob  .setComponentID ("ms");
    releaseKnob.setComponentID ("ms");
    sustainKnob.setComponentID ("pct");

    // --- Envelope knobs ---
    setupKnob (attackKnob,  attackLabel,  "ATTACK");
    setupKnob (decayKnob,   decayLabel,   "DECAY");
    setupKnob (sustainKnob, sustainLabel, "SUSTAIN");
    setupKnob (releaseKnob, releaseLabel, "RELEASE");
    setupKnob (gainKnob,    gainLabel,    "GAIN");

    // "ms" sliders store values in seconds (e.g. 0.001 = 1 ms). drawLabel
    // parses the text back to a double and multiplies by 1000. With only 2
    // decimal places "0.001" rounds to "0.00", which gives 0 ms instead of 1 ms.
    // 3 decimal places preserves the precision we need (1 ms resolution).
    attackKnob .setNumDecimalPlacesToDisplay (3);
    decayKnob  .setNumDecimalPlacesToDisplay (3);
    releaseKnob.setNumDecimalPlacesToDisplay (3);

    // --- Transpose ---
    transposeSlider.setLookAndFeel (&laf);  // must come before setTextBoxStyle
    transposeSlider.setSliderStyle (juce::Slider::IncDecButtons);
    transposeSlider.setTextBoxStyle (juce::Slider::TextBoxAbove, false, 56, 27); // 27px label (22 visible + 5 gap trim)
    transposeSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsNotDraggable);
    transposeSlider.setNumDecimalPlacesToDisplay (0);
    addAndMakeVisible (transposeSlider);
    transposeLabel.setText ("TRANSPOSE", juce::dontSendNotification);
    transposeLabel.setJustificationType (juce::Justification::centred);
    transposeLabel.setFont (juce::FontOptions (9.5f));
    transposeLabel.setColour (juce::Label::textColourId, kLabel);
    addAndMakeVisible (transposeLabel);

    processorRef.thumbnail.addChangeListener (this);
}

CoreSamplerEditor::~CoreSamplerEditor()
{
    processorRef.thumbnail.removeChangeListener (this);
    // Clear look-and-feel before laf is destroyed
    loadButton.setLookAndFeel (nullptr);
    filterTypeBox.setLookAndFeel (nullptr);
    slopeButton12.setLookAndFeel (nullptr);
    slopeButton24.setLookAndFeel (nullptr);
    transposeSlider.setLookAndFeel (nullptr);
    for (auto* knob : { &filterFreqKnob, &filterResKnob,
                        &attackKnob, &decayKnob, &sustainKnob, &releaseKnob,
                        &gainKnob })
        knob->setLookAndFeel (nullptr);
}

void CoreSamplerEditor::setupKnob (juce::Slider& knob, juce::Label& label,
                                     const juce::String& text, int decimals)
{
    // setLookAndFeel MUST come first — setTextBoxStyle calls createSliderTextBox,
    // which stamps the componentID onto the label. If laf isn't set yet, the
    // default LookAndFeel's createSliderTextBox runs instead and the ID is lost.
    knob.setLookAndFeel (&laf);
    knob.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    knob.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 56, 14);
    if (decimals >= 0)
        knob.setNumDecimalPlacesToDisplay (decimals);
    addAndMakeVisible (knob);
    label.setText (text, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::FontOptions (9.5f));
    label.setColour (juce::Label::textColourId, kLabel);
    addAndMakeVisible (label);
}

void CoreSamplerEditor::updateSlopeButtons()
{
    int idx = (int) processorRef.apvts.getRawParameterValue ("filterSlope")->load();
    slopeButton12.setToggleState (idx == 0, juce::dontSendNotification);
    slopeButton24.setToggleState (idx == 1, juce::dontSendNotification);
}

// ============================================================
// Waveform drag — start and end point
// ============================================================
void CoreSamplerEditor::setStartPoint (int mouseX)
{
    if (waveformBounds.getWidth() <= 0) return;
    float fraction = juce::jlimit (0.0f, 0.9f,
        (float) (mouseX - waveformBounds.getX()) / (float) waveformBounds.getWidth());
    if (auto* param = processorRef.apvts.getParameter ("startPoint"))
        param->setValueNotifyingHost (param->convertTo0to1 (fraction));
    repaint (waveformBounds);
}

void CoreSamplerEditor::setEndPoint (int mouseX)
{
    if (waveformBounds.getWidth() <= 0) return;
    float startFrac = processorRef.apvts.getRawParameterValue ("startPoint")->load();
    float fraction  = juce::jlimit (startFrac + 0.05f, 1.0f,
        (float) (mouseX - waveformBounds.getX()) / (float) waveformBounds.getWidth());
    if (auto* param = processorRef.apvts.getParameter ("endPoint"))
        param->setValueNotifyingHost (param->convertTo0to1 (fraction));
    repaint (waveformBounds);
}

void CoreSamplerEditor::mouseDown (const juce::MouseEvent& e)
{
    if (! waveformBounds.contains (e.getPosition())) return;

    float startFrac = processorRef.apvts.getRawParameterValue ("startPoint")->load();
    float endFrac   = processorRef.apvts.getRawParameterValue ("endPoint")->load();
    int   startX    = waveformBounds.getX() + (int) (startFrac * waveformBounds.getWidth());
    int   endX      = waveformBounds.getX() + (int) (endFrac   * waveformBounds.getWidth());

    // Grab whichever marker is closer
    bool nearStart = std::abs (e.x - startX) <= std::abs (e.x - endX);

    if (nearStart)
    {
        isDraggingStart = true;
        if (auto* p = processorRef.apvts.getParameter ("startPoint")) p->beginChangeGesture();
        setStartPoint (e.x);
    }
    else
    {
        isDraggingEnd = true;
        if (auto* p = processorRef.apvts.getParameter ("endPoint")) p->beginChangeGesture();
        setEndPoint (e.x);
    }
}

void CoreSamplerEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (isDraggingStart) setStartPoint (e.x);
    if (isDraggingEnd)   setEndPoint   (e.x);
}

void CoreSamplerEditor::mouseUp (const juce::MouseEvent&)
{
    if (isDraggingStart)
    {
        if (auto* p = processorRef.apvts.getParameter ("startPoint")) p->endChangeGesture();
        processorRef.rebuildSamplerSound();
        isDraggingStart = false;
    }
    if (isDraggingEnd)
    {
        if (auto* p = processorRef.apvts.getParameter ("endPoint")) p->endChangeGesture();
        processorRef.rebuildSamplerSound();
        isDraggingEnd = false;
    }
}

void CoreSamplerEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    repaint (waveformBounds);
}

// ============================================================
// paint
// ============================================================
void CoreSamplerEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    auto waveF = waveformBounds.toFloat();

    // ── Drop shadow (stacked semi-transparent layers) ─────────────────
    for (int i = 3; i >= 1; --i)
    {
        g.setColour (juce::Colour (0x18000000));
        g.fillRoundedRectangle (waveF.translated (0.0f, (float) i), 4.0f);
    }

    // ── Waveform panel — vertical gradient ───────────────────────────
    {
        juce::ColourGradient grad (juce::Colour (0xff13132a), waveF.getX(), waveF.getY(),
                                   juce::Colour (0xff0a0a18), waveF.getX(), waveF.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (waveF, 4.0f);
    }

    // Border
    g.setColour (kBorder);
    g.drawRoundedRectangle (waveF, 4.0f, 1.0f);

    // Inner top-edge rim highlight
    g.setColour (juce::Colour (0x25ffffff));
    g.drawHorizontalLine (waveformBounds.getY() + 1,
                          waveF.getX() + 4.0f, waveF.getRight() - 4.0f);

    if (processorRef.thumbnail.getNumChannels() == 0)
    {
        g.setColour (juce::Colour (0xff555577));
        g.setFont (juce::FontOptions (13.0f));
        g.drawText ("Load a sample  --  drag the orange / white markers to set start and end points",
                    waveformBounds, juce::Justification::centred);
    }
    else
    {
        float startFrac = processorRef.apvts.getRawParameterValue ("startPoint")->load();
        float endFrac   = processorRef.apvts.getRawParameterValue ("endPoint")->load();
        int   startX    = waveformBounds.getX() + (int) (startFrac * waveformBounds.getWidth());
        int   endX      = waveformBounds.getX() + (int) (endFrac   * waveformBounds.getWidth());
        float top       = (float) waveformBounds.getY();
        float bottom    = (float) waveformBounds.getBottom();

        // Active region tint between markers
        g.setColour (juce::Colour (0x1500aacc));
        g.fillRect (startX, waveformBounds.getY(), endX - startX, waveformBounds.getHeight());

        // Waveform + neon gradient overlay inside clipped region
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (waveformBounds.reduced (2));

            g.setColour (juce::Colour (0xff00aacc));
            processorRef.thumbnail.drawChannels (g, waveformBounds.reduced (2), 0.0,
                                                 processorRef.thumbnail.getTotalLength(), 1.0f);

            // Neon wash: teal top → transparent bottom
            juce::ColourGradient waveGrad (
                juce::Colour (0x6000ccdd), waveF.getX(), waveF.getY(),
                juce::Colour (0x00001133), waveF.getX(), waveF.getBottom(), false);
            g.setGradientFill (waveGrad);
            g.fillRect (waveformBounds.reduced (2).toFloat());
        }

        // Start marker — orange, triangle points down from top
        g.setColour (juce::Colours::orange);
        g.drawVerticalLine (startX, top, bottom);
        juce::Path startHandle;
        startHandle.addTriangle ((float) startX - 5, top,
                                 (float) startX + 5, top,
                                 (float) startX,     top + 9);
        g.fillPath (startHandle);

        // End marker — white, triangle points up from bottom
        g.setColour (juce::Colours::white);
        g.drawVerticalLine (endX, top, bottom);
        juce::Path endHandle;
        endHandle.addTriangle ((float) endX - 5, bottom,
                               (float) endX + 5, bottom,
                               (float) endX,     bottom - 9);
        g.fillPath (endHandle);
    }

    // ── Bottom section panel backgrounds ─────────────────────────────
    const int bottomRowY = waveformBounds.getBottom() + 8;
    const int bottomH    = getHeight() - bottomRowY;

    auto drawPanel = [&] (juce::Rectangle<float> panel)
    {
        g.setColour (juce::Colour (0xff0f0f20));
        g.fillRoundedRectangle (panel, 4.0f);
        g.setColour (juce::Colour (0xff2e2e52));
        g.drawRoundedRectangle (panel, 4.0f, 1.0f);
        // Top rim highlight for 3-D depth
        g.setColour (juce::Colour (0x20ffffff));
        g.drawHorizontalLine ((int) panel.getY() + 1,
                              panel.getX() + 4.0f, panel.getRight() - 4.0f);
    };

    // Layout: area starts at x=6 (6px outer margin), colW=78
    //   Filter region:   x=6  … x=240  (3 cols × 78)
    //   Gap:             x=240 … x=246 (6 px — holds the divider)
    //   Envelope region: x=246 … x=714 (6 cols × 78)
    // Panels use 2 px padding on each side of their region → symmetric outer margins of 4 px.
    //   Filter panel:   x=4,   w=238  →  x=4  … x=242
    //   Envelope panel: x=244, w=472  →  x=244 … x=716
    //   Divider: x=243 (centre of the 6-px gap)
    drawPanel (juce::Rectangle<float> (4.0f,   (float) bottomRowY,
                                       238.0f,  (float) bottomH - 2.0f));
    drawPanel (juce::Rectangle<float> (244.0f,  (float) bottomRowY,
                                       472.0f,  (float) bottomH - 2.0f));

    // ── Section dividers ─────────────────────────────────────────────
    g.setColour (kDivider);
    g.drawHorizontalLine (waveformBounds.getBottom() + 4, 0.0f, (float) getWidth());
    g.drawVerticalLine   (243, (float) (waveformBounds.getBottom() + 6), (float) getHeight());
}

// ============================================================
// resized
// ============================================================
void CoreSamplerEditor::resized()
{
    // 6 px outer horizontal margin — consistent across every row
    auto area = getLocalBounds().reduced (6, 0);

    // Load row (30px)
    auto loadRow = area.removeFromTop (30);
    loadButton.setBounds (loadRow.removeFromLeft (114).reduced (0, 5));
    sampleLabel.setBounds (loadRow.reduced (4, 5));

    // Waveform — takes most of the vertical real-estate
    waveformBounds = area.removeFromTop (175).reduced (0, 4);

    area.removeFromTop (4); // gap to divider

    // Bottom row — filter region (3 cols) + gap + envelope region (6 cols)
    // Total: 3*78 + 6 + 6*78 = 234 + 6 + 468 = 708 px (exactly fills the area)
    auto bottomRow = area.reduced (0, 2); // tight vertical padding so waveform dominates
    constexpr int colW    = 78;
    constexpr int gapW    = 6;          // visual gap that holds the divider line

    // Shared knob-unit dimensions — every column uses the same centering
    // so all titles sit on exactly the same horizontal baseline.
    constexpr int kLabelH = 13;
    constexpr int kGap    = 6;
    constexpr int kKnobH  = 100;  // rotary area(86) + textbox(14)
    constexpr int kUnitH  = kLabelH + kGap + kKnobH;   // 119 px

    auto placeKnob = [&] (juce::Rectangle<int> col, juce::Slider& knob, juce::Label& label)
    {
        auto unit = col.withSizeKeepingCentre (col.getWidth(),
                                               juce::jmin (kUnitH, col.getHeight()));
        label.setBounds (unit.removeFromTop (kLabelH));
        unit.removeFromTop (kGap);
        knob.setBounds (unit);
    };

    // Col 1 — FILTER: label aligned with all other labels (kUnitH centering),
    // then dropdown + chips centered in the space below the label.
    {
        constexpr int kDropH    = 22;
        constexpr int kChipH    = 22;
        constexpr int kInnerGap = 5;
        constexpr int kCtrlH    = kDropH + kInnerGap + kChipH; // 49 px

        auto col  = bottomRow.removeFromLeft (colW);
        auto unit = col.withSizeKeepingCentre (col.getWidth(),
                                               juce::jmin (kUnitH, col.getHeight()));
        filterLabel.setBounds (unit.removeFromTop (kLabelH));
        unit.removeFromTop (kGap);
        // Centre the 49-px control block in the remaining 100-px space
        auto ctrl = unit.withSizeKeepingCentre (unit.getWidth(), kCtrlH);
        filterTypeBox.setBounds (ctrl.removeFromTop (kDropH).reduced (4, 0));
        ctrl.removeFromTop (kInnerGap);
        auto slopeRow = ctrl.removeFromTop (kChipH);
        const int bW = 26, gp = 4;
        const int ox = slopeRow.getX() + (slopeRow.getWidth() - (bW + gp + bW)) / 2;
        slopeButton12.setBounds (ox,           slopeRow.getY(), bW, slopeRow.getHeight());
        slopeButton24.setBounds (ox + bW + gp, slopeRow.getY(), bW, slopeRow.getHeight());
    }

    // Col 2-3: filter knobs
    placeKnob (bottomRow.removeFromLeft (colW), filterFreqKnob, filterFreqLabel);
    placeKnob (bottomRow.removeFromLeft (colW), filterResKnob,  filterResLabel);

    // --- gap between filter section and envelope section (holds divider) ---
    bottomRow.removeFromLeft (gapW);

    // Col 4-8: envelope knobs
    placeKnob (bottomRow.removeFromLeft (colW), attackKnob,  attackLabel);
    placeKnob (bottomRow.removeFromLeft (colW), decayKnob,   decayLabel);
    placeKnob (bottomRow.removeFromLeft (colW), sustainKnob, sustainLabel);
    placeKnob (bottomRow.removeFromLeft (colW), releaseKnob, releaseLabel);
    placeKnob (bottomRow.removeFromLeft (colW), gainKnob,    gainLabel);

    // Col 9: Transpose — label aligned with all other labels (kUnitH centering),
    // controls centred in the space below, matching the filter column treatment.
    {
        constexpr int kCtrlH = 49;  // textbox(27) + buttons(22)

        auto unit = bottomRow.withSizeKeepingCentre (bottomRow.getWidth(),
                                                     juce::jmin (kUnitH, bottomRow.getHeight()));
        transposeLabel.setBounds (unit.removeFromTop (kLabelH));
        unit.removeFromTop (kGap);
        // Centre the 49-px control block in the remaining 100-px space
        transposeSlider.setBounds (unit.withSizeKeepingCentre (56, kCtrlH));
    }
}
