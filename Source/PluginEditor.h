#pragma once
#include <JuceHeader.h>
#include <BinaryData.h>
#include "PluginProcessor.h"

// ============================================================
// Custom Look & Feel — blue arc rotary knobs
// ============================================================
class LittleSamplerLookAndFeel : public juce::LookAndFeel_V4
{
public:

    // ── Rotary knob — PNG sprite sheet (128 frames, vertical strip) ──
    void drawRotarySlider (juce::Graphics& g,
                           int x, int y, int width, int height,
                           float sliderPos,
                           float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override
    {
        // Lazy-load once; ImageCache keeps a reference so this is cheap.
        if (! knobImage.isValid())
            knobImage = juce::ImageCache::getFromMemory (
                BinaryData::knobStrip_png, BinaryData::knobStrip_pngSize);

        if (knobImage.isValid())
        {
            constexpr int kFrames = 128;
            const int frameW = knobImage.getWidth();
            const int frameH = knobImage.getHeight() / kFrames;
            const int frame  = juce::roundToInt (sliderPos * float (kFrames - 1));

            // Scale frame to fit knob bounds, preserving aspect ratio, centred
            const float scaleX = (float) width  / (float) frameW;
            const float scaleY = (float) height / (float) frameH;
            const float scale  = juce::jmin (scaleX, scaleY);
            const float dw     = (float) frameW * scale;
            const float dh     = (float) frameH * scale;
            const float dx     = (float) x + ((float) width  - dw) * 0.5f;
            const float dy     = (float) y + ((float) height - dh) * 0.5f;

            // Dark-grey backdrop so the knob body is always visible, even at position 0
            const float cx = dx + dw * 0.5f;
            const float cy = dy + dh * 0.5f;
            const float r  = juce::jmin (dw, dh) * 0.42f;
            g.setColour (juce::Colour (0xff1e1e30));
            g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
            g.setColour (juce::Colour (0xff2e2e48));
            g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.0f);

            g.drawImage (knobImage,
                         (int) dx, (int) dy, (int) dw, (int) dh,
                         0, frame * frameH, frameW, frameH);
            return;
        }

        // ── Fallback: drawn knob (used if PNG fails to load) ──────────
        const float radius   = juce::jmin (static_cast<float> (juce::jmin (width / 2, height / 2)) - 2.0f, 14.0f);
        const float centreX  = static_cast<float> (x) + width  * 0.5f;
        const float centreY  = static_cast<float> (y) + height * 0.5f;
        const float angle    = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float arcWidth = juce::jmax (2.0f, radius * 0.18f);

        g.setColour (juce::Colour (0xff111122));
        g.fillEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f);

        juce::Path track;
        track.addCentredArc (centreX, centreY, radius - arcWidth * 0.5f,
                             radius - arcWidth * 0.5f, 0.0f,
                             rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (0xff2a2a50));
        g.strokePath (track, juce::PathStrokeType (arcWidth,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, radius - arcWidth * 0.5f,
                                radius - arcWidth * 0.5f, 0.0f,
                                rotaryStartAngle, angle, true);
        g.setColour (juce::Colour (0x403377ff));
        g.strokePath (valueArc, juce::PathStrokeType (arcWidth * 3.0f,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xff44aaff));
        g.strokePath (valueArc, juce::PathStrokeType (arcWidth,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        g.setColour (juce::Colour (0xff3a3a60));
        g.drawEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.0f);
        const float dotR  = arcWidth * 0.55f;
        const float dotX  = centreX + std::sin (angle) * (radius - arcWidth * 0.5f);
        const float dotY  = centreY - std::cos (angle) * (radius - arcWidth * 0.5f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (dotX - dotR, dotY - dotR, dotR * 2.0f, dotR * 2.0f);
    }

    // ── Slider textbox factory — stamps the slider's componentID onto its Label ──
    // JUCE calls this when the textbox Label is first created, guaranteeing the ID
    // is available every time drawLabel paints — no parent-cast fragility.
    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* label = LookAndFeel_V4::createSliderTextBox (slider);

        auto id = slider.getComponentID();    // "ms", "pct", or ""

        // For untagged rotary sliders stamp "knob" so drawLabel still renders
        // the dark textbox style (FREQ, RES, GAIN etc.)
        if (id.isEmpty())
        {
            const auto style = slider.getSliderStyle();
            if (style == juce::Slider::RotaryVerticalDrag ||
                style == juce::Slider::Rotary ||
                style == juce::Slider::RotaryHorizontalVerticalDrag)
                id = "knob";
        }

        label->setComponentID (id);
        return label;
    }

    // ── Knob textbox — dark inset, no border ─────────────────────────
    void drawLabel (juce::Graphics& g, juce::Label& label) override
    {
        // Rotary knob textbox: identified by componentID stamped in createSliderTextBox.
        // Reading from the label itself avoids the brittle parent-component cast.
        const auto id = label.getComponentID();
        const bool isKnobBox = (id == "ms" || id == "pct" || id == "knob");

        // Check if we're dealing with an IncDecButtons textbox (for transpose).
        // Those labels have a Slider parent with IncDecButtons style.
        bool isIncDec = false;
        if (auto* slider = dynamic_cast<juce::Slider*> (label.getParentComponent()))
            isIncDec = (slider->getSliderStyle() == juce::Slider::IncDecButtons);

        if (isKnobBox)
        {
            // Reformat raw value text based on the unit tag.
            juce::String text = label.getText();
            if (id == "ms")
            {
                double v = text.getDoubleValue();
                text = juce::String (juce::roundToInt (v * 1000.0)) + " ms";
            }
            else if (id == "pct")
            {
                double v = text.getDoubleValue();
                text = juce::String (juce::roundToInt (v * 100.0)) + "%";
            }

            g.fillAll (juce::Colour (0xff111122));
            g.setColour (juce::Colour (0xff7788cc));
            g.setFont (juce::FontOptions (9.5f));
            g.drawFittedText (text, label.getLocalBounds(),
                              juce::Justification::centred, 1, 0.9f);
            return;
        }

        if (isIncDec)
        {
            // IncDecButtons textbox (transpose)
            // Trim 5 px from the bottom to create a visual gap matching the
            // 5 px gap between the filter dropdown and slope chips.
            auto fill = label.getLocalBounds().toFloat().withTrimmedBottom (5.0f);
            g.setColour (juce::Colour (0xff0d0d1a));
            g.fillRoundedRectangle (fill, 2.0f);
            g.setColour (juce::Colour (0xff2a2a55));
            g.drawRoundedRectangle (fill.reduced (0.5f), 2.0f, 1.0f);
            g.setColour (juce::Colour (0xff7788cc));
            g.setFont (juce::FontOptions (11.0f));
            // Draw text centred within the visible fill, not the full label bounds
            g.drawFittedText (label.getText(), fill.toNearestInt(),
                              juce::Justification::centred, 1);
            return;
        }
        LookAndFeel_V4::drawLabel (g, label);
    }

    // ── ComboBox — dark pill, blue-tint border, chevron arrow ────────
    void drawComboBox (juce::Graphics& g, int width, int height,
                       bool /*isButtonDown*/,
                       int /*buttonX*/, int /*buttonY*/,
                       int /*buttonW*/, int /*buttonH*/,
                       juce::ComboBox& box) override
    {
        const float corner = static_cast<float> (height) * 0.4f;
        const juce::Rectangle<float> bounds (0.0f, 0.0f,
                                             static_cast<float> (width),
                                             static_cast<float> (height));
        g.setColour (juce::Colour (0xff0d0d1a));
        g.fillRoundedRectangle (bounds, corner);

        g.setColour (box.isPopupActive() ? juce::Colour (0xff3355cc)
                                         : juce::Colour (0xff2a2a55));
        g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

        // Chevron
        const float az  = static_cast<float> (height);
        const float ax  = static_cast<float> (width) - az * 0.65f;
        const float ay  = static_cast<float> (height) * 0.5f;
        const float as_ = static_cast<float> (height) * 0.18f;
        juce::Path arrow;
        arrow.startNewSubPath (ax - as_, ay - as_ * 0.5f);
        arrow.lineTo           (ax,       ay + as_ * 0.5f);
        arrow.lineTo           (ax + as_, ay - as_ * 0.5f);
        g.setColour (juce::Colour (0xff6677bb));
        g.strokePath (arrow, juce::PathStrokeType (1.2f,
                      juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (6, 0, box.getWidth() - static_cast<int> (box.getHeight() * 0.9f), box.getHeight());
        label.setFont (getComboBoxFont (box));
        label.setColour (juce::Label::textColourId, juce::Colour (0xff9999bb));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        return juce::Font (juce::FontOptions (static_cast<float> (box.getHeight()) * 0.45f));
    }

    // ── Button background — slope chips + transpose inc/dec ──────────
    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& /*backgroundColour*/,
                               bool isMouseOverButton,
                               bool isButtonDown) override
    {
        const auto  bounds = button.getLocalBounds().toFloat().reduced (0.8f);
        const float corner = bounds.getHeight() * 0.4f;

        // For the transpose +/- buttons: light up the + when value > 0,
        // the - when value < 0, so the active direction stays visually "on".
        const bool active = [&]() -> bool {
            if (auto* sl = dynamic_cast<juce::Slider*> (button.getParentComponent()))
                if (sl->getSliderStyle() == juce::Slider::IncDecButtons)
                    return button.getButtonText() == "+" ? sl->getValue() > 0.0
                                                        : sl->getValue() < 0.0;
            return button.getToggleState();
        }();

        if (active)
        {
            // Gradient body — teal-tinted top, darker bottom (3D depth)
            juce::ColourGradient body (juce::Colour (0xff0e3a44), bounds.getX(), bounds.getY(),
                                      juce::Colour (0xff061e26), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (bounds, corner);

            // Layered teal glow
            g.setColour (juce::Colour (0x1500aacc));
            g.drawRoundedRectangle (bounds, corner, 7.0f);
            g.setColour (juce::Colour (0x3000bbdd));
            g.drawRoundedRectangle (bounds, corner, 4.5f);
            g.setColour (juce::Colour (0x5800ccee));
            g.drawRoundedRectangle (bounds, corner, 2.5f);
            g.setColour (juce::Colour (0xff00ccdd));
            g.drawRoundedRectangle (bounds, corner, 1.0f);

            // Top rim highlight
            g.setColour (juce::Colour (0x5500ddee));
            g.drawHorizontalLine ((int) (bounds.getY() + 1.0f),
                                  bounds.getX() + corner * 0.8f,
                                  bounds.getRight() - corner * 0.8f);
        }
        else
        {
            // Subtle gradient for inactive 3D feel
            juce::ColourGradient body (
                juce::Colour (isButtonDown       ? 0xff1f1f3a
                              : isMouseOverButton ? 0xff1b1b32
                                                 : 0xff171728),
                bounds.getX(), bounds.getY(),
                juce::Colour (isButtonDown       ? 0xff111126
                              : isMouseOverButton ? 0xff0e0e22
                                                 : 0xff0c0c1e),
                bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill (body);
            g.fillRoundedRectangle (bounds, corner);

            g.setColour (juce::Colour (0xff2a2a50));
            g.drawRoundedRectangle (bounds, corner, 1.0f);

            // Faint top rim
            g.setColour (juce::Colour (0x18ffffff));
            g.drawHorizontalLine ((int) (bounds.getY() + 1.0f),
                                  bounds.getX() + corner * 0.8f,
                                  bounds.getRight() - corner * 0.8f);
        }
    }

    // ── Button text — small, consistent weight ───────────────────────
    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*isMouseOverButton*/, bool /*isButtonDown*/) override
    {
        const bool lit = [&]() -> bool {
            if (auto* sl = dynamic_cast<juce::Slider*> (button.getParentComponent()))
                if (sl->getSliderStyle() == juce::Slider::IncDecButtons)
                    return button.getButtonText() == "+" ? sl->getValue() > 0.0
                                                        : sl->getValue() < 0.0;
            return button.getToggleState();
        }();

        // Wider buttons (e.g. Load Sample) get a slightly larger font
        const float fontSize = button.getHeight() >= 18 ? 11.0f : 10.0f;
        g.setFont (juce::FontOptions (fontSize));
        g.setColour (lit ? juce::Colour (0xff00eeff) : juce::Colour (0xff9999bb));
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (4, 0),
                          juce::Justification::centred, 1, 0.9f);
    }

    // ── Popup menu — dark themed, same font scale as labels ──────────
    juce::Font getPopupMenuFont() override
    {
        return juce::Font (juce::FontOptions (11.0f));
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        g.setColour (juce::Colour (0xff0d0d1a));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, 4.0f);
        g.setColour (juce::Colour (0xff2a2a55));
        g.drawRoundedRectangle (0.5f, 0.5f,
                                (float) width  - 1.0f,
                                (float) height - 1.0f, 4.0f, 1.0f);
    }

    // ── Knob sprite cache ─────────────────────────────────────────────
    juce::Image knobImage;

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool /*hasSubMenu*/,
                            const juce::String& text, const juce::String& /*shortcutKeyText*/,
                            const juce::Drawable* /*icon*/,
                            const juce::Colour* /*textColour*/) override
    {
        if (isSeparator)
        {
            g.setColour (juce::Colour (0xff2a2a45));
            g.fillRect (area.reduced (6, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }

        if (isHighlighted && isActive)
        {
            g.setColour (juce::Colour (0xff1a3a7a));
            g.fillRoundedRectangle (area.reduced (2, 1).toFloat(), 3.0f);
        }

        g.setFont (getPopupMenuFont());
        g.setColour (isActive ? (isTicked ? juce::Colour (0xff00ccdd)
                                          : juce::Colour (0xff9999bb))
                              : juce::Colour (0xff555566));
        g.drawFittedText (text, area.reduced (8, 0),
                          juce::Justification::centredLeft, 1);

        if (isTicked)
        {
            const float cx = (float) area.getRight() - 10.0f;
            const float cy = (float) area.getCentreY();
            g.setColour (juce::Colour (0xff00ccdd));
            g.fillEllipse (cx - 3.0f, cy - 3.0f, 6.0f, 6.0f);
        }
    }
};

// ============================================================
// Editor
// ============================================================
class LittleSamplerEditor : public juce::AudioProcessorEditor,
                             public juce::ChangeListener
{
public:
    LittleSamplerEditor (LittleSamplerProcessor&);
    ~LittleSamplerEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp   (const juce::MouseEvent&) override;

    void changeListenerCallback (juce::ChangeBroadcaster*) override;

private:
    LittleSamplerProcessor& processorRef;

    // --- Load row ---
    juce::TextButton loadButton { "Load Sample" };
    juce::Label      sampleLabel;
    std::unique_ptr<juce::FileChooser> fileChooser;

    // --- Filter section ---
    juce::ComboBox   filterTypeBox;
    juce::TextButton slopeButton12 { "12dB" };
    juce::TextButton slopeButton24 { "24dB" };
    juce::Label      filterLabel;
    juce::Slider     filterFreqKnob, filterResKnob;
    juce::Label      filterFreqLabel, filterResLabel;

    // --- Envelope section ---
    juce::Slider attackKnob, decayKnob, sustainKnob, releaseKnob;
    juce::Label  attackLabel, decayLabel, sustainLabel, releaseLabel;
    juce::Slider gainKnob;
    juce::Label  gainLabel;
    juce::Slider transposeSlider;
    juce::Label  transposeLabel;

    // --- APVTS attachments ---
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    ComboAttachment  filterTypeAtt;
    SliderAttachment filterFreqAtt, filterResAtt;
    SliderAttachment attackAtt, decayAtt, sustainAtt, releaseAtt;
    SliderAttachment gainAtt, transposeAtt;

    // --- Waveform state ---
    juce::Rectangle<int> waveformBounds;
    bool isDraggingStart = false;
    bool isDraggingEnd   = false;

    void setStartPoint (int mouseX);
    void setEndPoint   (int mouseX);
    void updateSlopeButtons();
    void setupKnob (juce::Slider& knob, juce::Label& label, const juce::String& text, int decimals = 2);

    // Declare laf last so it is destroyed before the knobs
    LittleSamplerLookAndFeel laf;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LittleSamplerEditor)
};
