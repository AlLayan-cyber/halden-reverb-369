#include "PluginEditor.h"
#include <cmath>

using namespace hui;

namespace
{
const float kStartAngle = juce::MathConstants<float>::pi * 1.25f;
const float kEndAngle   = juce::MathConstants<float>::pi * 2.75f;

juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}

juce::String hz (float v)
{
    return v < 1000.0f ? juce::String (juce::roundToInt (v)) + " Hz"
                       : juce::String (v / 1000.0f, v < 10000.0f ? 1 : 0) + " kHz";
}

juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + " %"; }

juce::String db (float v)
{
    if (std::abs (v) < 0.05f) return "0.0 dB";
    return (v > 0 ? "+" : juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92"))) + juce::String (std::abs (v), 1) + " dB";
}

juce::Colour ringColour (float grDb)
{
    if (grDb <= 6.0f)  return col::iceBlue;
    if (grDb <= 9.0f)  return col::iceBlue.interpolatedWith (col::amber, (grDb - 6.0f) / 3.0f);
    if (grDb <= 12.0f) return col::amber.interpolatedWith (col::red, (grDb - 9.0f) / 3.0f);
    return col::red;
}

void setStyle (juce::Component& c, const char* style) { c.getProperties().set ("style", style); }
} // namespace

juce::String hui::formatValue (const juce::String& id, float v)
{
    if (id == "preDelayMs") return v < 10.0f ? juce::String (std::round (v * 10.0f) / 10.0f, 1) + " ms" : juce::String (juce::roundToInt (v)) + " ms";
    if (id == "decay")      return juce::String (v, v < 10.0f ? 2 : 1) + " s";
    if (id == "freezeFade") return juce::String (v, 2) + " s";
    if (id == "highCutHz" || id == "crossoverHz" || id == "eqLowCutHz" || id == "eqHighCutHz" || id == "monoHz") return hz (v);
    if (id == "bassMult")   return juce::String (juce::CharPointer_UTF8 ("\xc3\x97")) + juce::String (v, 2);
    if (id == "inGain" || id == "outGain") return db (v);
    return pct (v);
}

// =========================================================================== LookAndFeel
hui::LookAndFeel::LookAndFeel()
{
    setColour (juce::TextButton::buttonColourId, juce::Colour (0xff111318));
    setColour (juce::TextButton::textColourOffId, col::textLo);
    setColour (juce::TextButton::textColourOnId, col::blueBright);
}

void hui::LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float,
                                         juce::Slider::SliderStyle, juce::Slider& s)
{
    const float cx = static_cast<float> (x) + static_cast<float> (w) * 0.5f;
    const float top = static_cast<float> (y), bottom = static_cast<float> (y + h);

    g.setColour (juce::Colour (0xff050608));
    g.fillRoundedRectangle (cx - 3.0f, top - 4.0f, 6.0f, bottom - top + 8.0f, 3.0f);
    g.setColour (col::line);
    g.drawRoundedRectangle (cx - 3.0f, top - 4.0f, 6.0f, bottom - top + 8.0f, 3.0f, 1.0f);

    for (float v : { 12.0f, 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f })
    {
        const float ty = bottom + static_cast<float> (s.valueToProportionOfLength (v)) * (top - bottom);
        const bool zero = juce::exactlyEqual (v, 0.0f);
        g.setColour (zero ? col::blue : juce::Colour (0xff374151));
        g.drawLine (cx - 14.0f, ty, cx - 8.0f, ty, zero ? 2.0f : 1.0f);
        g.drawLine (cx + 8.0f, ty, cx + 14.0f, ty, zero ? 2.0f : 1.0f);
    }

    const juce::Rectangle<float> cap (cx - 13.0f, sliderPos - 10.0f, 26.0f, 20.0f);
    g.setColour (juce::Colours::black);
    g.fillRoundedRectangle (cap.translated (0.0f, 2.0f), 3.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2f37), cap.getX(), cap.getY(),
                                             juce::Colour (0xff15181d), cap.getX(), cap.getBottom(), false));
    g.fillRoundedRectangle (cap, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (cap, 3.0f, 1.5f);
    g.setColour (col::blue);
    g.drawLine (cap.getX() + 3.0f, sliderPos, cap.getRight() - 3.0f, sliderPos, 2.0f);
}

void hui::LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto style = b.getProperties()["style"].toString();
    auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = b.getToggleState();

    if (style == "scale" || style == "text" || style == "reset" || style == "spaceText")
        return;

    if (style == "spaceBtn" || style == "spaceGo" || style == "spaceToggle")
    {
        const float rr = r.getHeight() * (style == "spaceGo" ? 0.5f : 0.3f);
        const bool lit = style == "spaceGo" || on;
        juce::Colour fill = style == "spaceGo" ? juce::Colour (0xffa78bfa) : (lit ? juce::Colour (0xff3b2a7a) : juce::Colour (0x66120f1d));
        if (over) fill = fill.brighter (0.12f);
        if (down) fill = fill.darker (0.15f);
        if (lit) { g.setColour (juce::Colour (0x338b5cf6)); g.drawRoundedRectangle (r.expanded (2.0f), rr + 2.0f, 4.0f); }
        g.setColour (fill);
        g.fillRoundedRectangle (r, rr);
        const float progress = static_cast<float> (static_cast<double> (b.getProperties().getWithDefault ("progress", 0.0)));
        if (style == "spaceGo" && progress > 0.0f)
        {
            g.setColour (juce::Colour (0xff6d28d9));
            g.fillRoundedRectangle (r.withWidth (std::max (r.getHeight(), r.getWidth() * progress)), rr);
        }
        g.setColour (lit ? juce::Colour (0xffc4b5fd) : juce::Colour (0xff2e2652));
        g.drawRoundedRectangle (r.reduced (0.5f), rr, 1.0f);
        return;
    }

    if (style == "spaceEntry")
    {
        if (on)   // soft violet glow, easy on the eyes
        {
            g.setColour (juce::Colour (0xff8b5cf6).withAlpha (0.10f));
            g.fillRoundedRectangle (r.expanded (3.0f), 6.0f);
            g.setColour (juce::Colour (0xff8b5cf6).withAlpha (0.16f));
            g.fillRoundedRectangle (r.expanded (1.5f), 5.0f);
        }
        g.setColour (on ? juce::Colour (0xff261b4d) : juce::Colour (0xff111318));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? juce::Colour (0xff9f86e8) : (over ? juce::Colour (0xff4a3f78) : col::line));
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        return;
    }

    if (style == "tab")
    {
        g.setColour (juce::Colour (0xff111827).brighter (over ? 0.15f : 0.0f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::blueDark);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
        return;
    }

    juce::Colour fill = on ? juce::Colour (0xff0c2347) : juce::Colour (0xff111318);
    if (over) fill = fill.brighter (0.12f);
    if (down) fill = fill.darker (0.2f);
    g.setColour (fill);
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (on ? col::blueDeep : col::line);
    g.drawRoundedRectangle (r, 4.0f, 1.0f);
}

void hui::LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool over, bool)
{
    const auto style = b.getProperties()["style"].toString();
    const bool on = b.getToggleState();
    juce::Colour c = on ? col::blueBright : col::textLo;

    if (style == "scale") c = on ? col::textMid : col::textDim.brighter (over ? 0.3f : 0.0f);
    if (style == "tab")   c = col::blue;
    if (style == "reset") c = over ? col::textMid : col::textDim;
    if (style == "spaceBtn" || style == "spaceToggle") c = on ? juce::Colour (0xfff3efff) : juce::Colour (0xff9a90c8);
    if (style == "spaceGo") c = juce::Colour (0xff1a1033);
    if (style == "spaceText") c = over ? juce::Colour (0xfff3efff) : juce::Colour (0xffb7acdf);
    if (style == "spaceEntry") c = on ? juce::Colour (0xffe4dcff) : (over ? juce::Colour (0xff9a90c8) : col::textLo);

    if (style == "spaceBtn" || style == "spaceGo" || style == "spaceToggle" || style == "spaceText")
    {
        g.setColour (c);
        g.setFont (font (style == "spaceText" ? 22.0f : std::min (15.0f, static_cast<float> (b.getHeight()) * 0.45f)));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
        return;
    }
    if (style == "reset" || style == "spaceEntry")
    {
        g.setColour (c);
        g.setFont (font (style == "reset" ? 11.0f : 11.0f));
        g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
        return;
    }

    g.setColour (c);
    const float size = style == "algo" ? 13.0f : style == "scale" ? 11.0f : style == "tab" ? 13.0f : 11.0f;
    g.setFont (font (size));

    if (style == "tab" && b.getWidth() < b.getHeight())
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi,
                                                         static_cast<float> (b.getWidth()) * 0.5f,
                                                         static_cast<float> (b.getHeight()) * 0.5f));
        auto rr = juce::Rectangle<int> (0, 0, b.getHeight(), b.getWidth())
                      .withCentre (b.getLocalBounds().getCentre());
        g.drawText (b.getButtonText(), rr, juce::Justification::centred);
        return;
    }
    g.drawText (b.getButtonText(), b.getLocalBounds(), juce::Justification::centred);
}

// =========================================================================== Knob
Knob::Knob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l, float r, bool isBig)
    : paramId (id), label (l), radius (r), big (isBig)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    slider.setMouseDragSensitivity (big ? 260 : 200);
    slider.setPopupDisplayEnabled (false, false, nullptr);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); if (onTouched) onTouched(); };
    slider.onDragStart = [this] { if (onHold) onHold (true); if (onTouched) onTouched(); };
    slider.onDragEnd = [this] { if (onHold) onHold (false); };
}

void Knob::resized()
{
    const float labelH = big ? 18.0f : 14.0f;
    const float cy = labelH + 8.0f + radius;
    const int d = juce::roundToInt (radius * 2.0f + 8.0f);
    slider.setBounds (juce::Rectangle<int> (d, d).withCentre ({ getWidth() / 2, juce::roundToInt (cy) }));
}

void Knob::paint (juce::Graphics& g)
{
    const float w = static_cast<float> (getWidth());
    const float labelH = big ? 18.0f : 14.0f;
    const float cx = w * 0.5f;
    const float cy = labelH + 8.0f + radius;
    const float r = radius;
    const float pos = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
    const float angle = kStartAngle + pos * (kEndAngle - kStartAngle);

    // label
    g.setColour (col::textMid);
    g.setFont (font (big ? 13.0f : 11.0f));
    g.drawText (label, juce::Rectangle<float> (0.0f, 0.0f, w, labelH), juce::Justification::centred);

    // scale arc + value arc, or the ducking glow ring
    const float arcR = r + 6.0f;
    const float grDb = ringReductionDb ? ringReductionDb() : 0.0f;
    {
        juce::Path bg;
        bg.addCentredArc (cx, cy, arcR, arcR, 0.0f, kStartAngle, kEndAngle, true);
        g.setColour (juce::Colour (0xff141922));
        g.strokePath (bg, juce::PathStrokeType (big ? 3.0f : 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    if (grDb > 0.1f)
    {
        const auto c = ringColour (grDb);
        const float end = kStartAngle + juce::jlimit (0.02f, 1.0f, grDb / 24.0f) * (kEndAngle - kStartAngle);
        juce::Path ring;
        ring.addCentredArc (cx, cy, arcR, arcR, 0.0f, kStartAngle, end, true);
        g.setColour (c.withAlpha (0.18f));
        g.strokePath (ring, juce::PathStrokeType (12.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.withAlpha (0.35f));
        g.strokePath (ring, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c);
        g.strokePath (ring, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    else if (pos > 0.002f)
    {
        juce::Path val;
        val.addCentredArc (cx, cy, arcR, arcR, 0.0f, kStartAngle, angle, true);
        g.setColour (col::blueDeep.withAlpha (0.75f));
        g.strokePath (val, juce::PathStrokeType (big ? 3.0f : 2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // body
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillEllipse (cx - r, cy - r + 3.0f, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colour (0xff16181d));
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 2.5f);
    const float capR = r * 0.78f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a2f38), cx - capR * 0.5f, cy - capR * 0.7f,
                                             juce::Colour (0xff16191e), cx + capR * 0.5f, cy + capR * 0.8f, false));
    g.fillEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (cx - capR, cy - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // pointer
    const float s = std::sin (angle), c = -std::cos (angle);
    g.setColour (col::blue);
    g.drawLine (cx + s * r * 0.30f, cy + c * r * 0.30f, cx + s * r * 0.74f, cy + c * r * 0.74f, big ? 4.0f : 3.0f);

    // value
    juce::String text = valueOverride ? valueOverride() : juce::String();
    if (text.isEmpty())
        text = formatValue (paramId, static_cast<float> (slider.getValue()));
    g.setColour (grDb > 0.1f ? ringColour (grDb) : col::blueBright);
    g.setFont (font (big ? 15.0f : 12.0f));
    g.drawText (text, juce::Rectangle<float> (0.0f, cy + r + 8.0f, w, big ? 20.0f : 16.0f), juce::Justification::centred);
}

// =========================================================================== Fader
Fader::Fader (juce::AudioProcessorValueTreeState& s, const juce::String& id) : paramId (id)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    slider.setDoubleClickReturnValue (true, 0.0);
    slider.onValueChange = [this] { repaint(); };
}

void Fader::resized() { slider.setBounds (0, 0, getWidth(), getHeight() - 20); }

void Fader::paint (juce::Graphics& g)
{
    g.setColour (col::blueBright);
    g.setFont (font (12.0f));
    g.drawText (formatValue (paramId, static_cast<float> (slider.getValue())).upToFirstOccurrenceOf (" dB", false, false),
                getLocalBounds().removeFromBottom (16), juce::Justification::centred);
}

// =========================================================================== LED meter
namespace
{
constexpr std::array<float, LedMeter::kSegments> kSegDb {
    -60.f, -54.f, -48.f, -42.f, -36.f, -33.f, -30.f, -27.f, -24.f, -21.f, -18.f,
    -15.f, -12.f, -10.f, -8.f, -6.f, -5.f, -4.f, -3.f, -2.f, -1.f, 0.f };
}

void LedMeter::setLevels (float l, float r, double nowSec)
{
    const float dt = lastTime > 0.0 ? static_cast<float> (nowSec - lastTime) : 0.0f;
    lastTime = nowSec;
    const float levels[2] = { l, r };
    for (int c = 0; c < 2; ++c)
    {
        auto& m = ch[static_cast<size_t> (c)];
        const float d = juce::Decibels::gainToDecibels (levels[c], -100.0f);
        m.db = std::max (d, m.db - 26.0f * dt);
        if (d >= m.peakDb) { m.peakDb = d; m.peakTime = nowSec; }
        else if (nowSec - m.peakTime > 1.5) m.peakDb = std::max (m.db, m.peakDb - 20.0f * dt);
    }
    repaint();
}

void LedMeter::paint (juce::Graphics& g)
{
    const float step = static_cast<float> (getHeight()) / static_cast<float> (kSegments);
    const float size = 8.0f;
    for (int c = 0; c < 2; ++c)
    {
        const auto& m = ch[static_cast<size_t> (c)];
        int peakSeg = -1;
        for (int s = 0; s < kSegments; ++s)
            if (m.peakDb >= kSegDb[static_cast<size_t> (s)]) peakSeg = s;

        const float x = c == 0 ? 0.0f : static_cast<float> (getWidth()) - size;
        for (int s = 0; s < kSegments; ++s)
        {
            const int fromTop = kSegments - 1 - s;
            const bool lit = m.db >= kSegDb[static_cast<size_t> (s)] || s == peakSeg;
            juce::Colour on  = fromTop < 3 ? col::red : fromTop < 7 ? juce::Colour (0xfff59e0b) : juce::Colour (0xff22c55e);
            juce::Colour off = fromTop < 3 ? juce::Colour (0xff2a0f0f) : fromTop < 7 ? juce::Colour (0xff2a1f08) : juce::Colour (0xff0b2013);
            const float y = static_cast<float> (fromTop) * step;
            g.setColour (lit ? on : off);
            g.fillRoundedRectangle (x, y, size, size, 1.0f);
        }
    }
}

// =========================================================================== ChoiceRow
ChoiceRow::ChoiceRow (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    param = s.getParameter (id);
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param))
        choices = c->choices;
    attachment = std::make_unique<juce::ParameterAttachment> (*param, [this] (float v)
    {
        current = juce::roundToInt (v);
        repaint();
    });
    attachment->sendInitialUpdate();
}

void ChoiceRow::paint (juce::Graphics& g)
{
    const bool active = isActive ? isActive() : true;
    const int n = choices.size();
    if (n == 0) return;
    const float w = (static_cast<float> (getWidth()) - static_cast<float> (n - 1) * 4.0f) / static_cast<float> (n);
    g.setFont (font (11.0f));
    for (int i = 0; i < n; ++i)
    {
        juce::Rectangle<float> r (static_cast<float> (i) * (w + 4.0f), 0.0f, w, static_cast<float> (getHeight()));
        const bool sel = i == current;
        auto text = choices[i].toUpperCase().replace ("-", juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")));
        if (violet)
        {
            const float rr = r.getHeight() * 0.3f;
            g.setColour (sel ? juce::Colour (0xff3b2a7a) : juce::Colour (0x66120f1d));
            g.fillRoundedRectangle (r, rr);
            if (sel) { g.setColour (juce::Colour (0x338b5cf6)); g.drawRoundedRectangle (r.expanded (1.5f), rr + 1.5f, 3.0f); }
            g.setColour (sel ? juce::Colour (0xffa78bfa) : juce::Colour (0xff2e2652));
            g.drawRoundedRectangle (r.reduced (0.5f), rr, 1.0f);
            g.setColour (sel ? juce::Colour (0xfff3efff) : juce::Colour (0xff9a90c8));
            g.setFont (font (std::min (14.0f, r.getHeight() * 0.45f)));
            g.drawText (text, r, juce::Justification::centred);
            continue;
        }
        g.setColour (sel ? (active ? juce::Colour (0xff12305e) : juce::Colour (0xff1a1d23)) : juce::Colour (0xff0b0c0f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (sel ? (active ? col::blueDeep : col::textDim) : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (sel ? (active ? col::blueBright : col::textMid) : col::textDim);
        g.drawText (text, r, juce::Justification::centred);
    }
}

void ChoiceRow::mouseDown (const juce::MouseEvent& e)
{
    const int n = choices.size();
    if (n == 0) return;
    const int idx = juce::jlimit (0, n - 1, static_cast<int> (static_cast<float> (e.x) / static_cast<float> (getWidth()) * static_cast<float> (n)));
    attachment->setValueAsCompleteGesture (static_cast<float> (idx));
}

// =========================================================================== LCD
Lcd::Lcd (HaldenReverbAudioProcessor& p) : proc (p)
{
    curveDb.fill (-120.0f);
    peakDb.fill (-120.0f);
    peakTime.fill (0.0);
}

juce::Rectangle<float> Lcd::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (34.0f).withTrimmedRight (18.0f).withTrimmedTop (42.0f).withTrimmedBottom (26.0f);
}

float Lcd::xForFreq (float f) const
{
    const auto a = plotArea();
    return a.getX() + a.getWidth() * std::log10 (f / 20.0f) / 3.0f;
}

float Lcd::yForDb (float d) const
{
    const auto a = plotArea();
    return a.getY() + a.getHeight() * juce::jlimit (0.0f, 1.0f, (-6.0f - d) / 72.0f);
}

void Lcd::mouseDown (const juce::MouseEvent&)
{
    peakDb = curveDb;
    repaint();
}

void Lcd::update (double nowSec)
{
    const float dt = lastUpdate > 0.0 ? static_cast<float> (nowSec - lastUpdate) : 0.033f;
    lastUpdate = nowSec;
    now = nowSec;
    animateOverlays (juce::jlimit (0.001f, 0.1f, dt));

    auto& fifo = proc.scopeFifo;
    const int ready = fifo.getNumReady();
    if (ready > 0)
    {
        const auto rd = fifo.read (ready);
        const auto copy = [this] (const float* src, int num)
        {
            for (int i = 0; i < num; ++i)
            {
                ring[static_cast<size_t> (ringPos)] = src[i];
                ringPos = (ringPos + 1) % kFftSize;
            }
        };
        copy (proc.scopeBuffer.data() + rd.startIndex1, rd.blockSize1);
        copy (proc.scopeBuffer.data() + rd.startIndex2, rd.blockSize2);
    }
    else
    {
        // nothing arriving (transport stopped / bypassed): let the display fall
        for (int i = 0; i < 1600; ++i)
        {
            ring[static_cast<size_t> (ringPos)] = 0.0f;
            ringPos = (ringPos + 1) % kFftSize;
        }
    }

    std::fill (fftData.begin(), fftData.end(), 0.0f);
    for (int i = 0; i < kFftSize; ++i)
        fftData[static_cast<size_t> (i)] = ring[static_cast<size_t> ((ringPos + i) % kFftSize)];
    window.multiplyWithWindowingTable (fftData.data(), kFftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const float sr = static_cast<float> (proc.scopeSampleRate.load());
    const float binHz = sr / static_cast<float> (kFftSize);
    const float norm = static_cast<float> (kFftSize) * 0.25f;
    const float sixth = std::pow (2.0f, 1.0f / 12.0f);

    for (int k = 0; k < kPoints; ++k)
    {
        const float f = 20.0f * std::pow (1000.0f, static_cast<float> (k) / static_cast<float> (kPoints - 1));
        const int b0 = juce::jlimit (1, kFftSize / 2 - 1, static_cast<int> (std::floor (f / sixth / binHz)));
        const int b1 = juce::jlimit (b0, kFftSize / 2 - 1, static_cast<int> (std::ceil (f * sixth / binHz)));
        float power = 0.0f;
        for (int b = b0; b <= b1; ++b)
        {
            const float m = fftData[static_cast<size_t> (b)] / norm;
            power += m * m;
        }
        power /= static_cast<float> (b1 - b0 + 1);
        const float d = 10.0f * std::log10 (power + 1.0e-14f);

        auto& c = curveDb[static_cast<size_t> (k)];
        c = d > c ? c + (d - c) * 0.6f : c + (d - c) * 0.25f;

        auto& p = peakDb[static_cast<size_t> (k)];
        if (c >= p) { p = c; peakTime[static_cast<size_t> (k)] = nowSec; }
        else if (nowSec - peakTime[static_cast<size_t> (k)] > 4.0) p = std::max (c, p - 12.0f * dt);
    }
    repaint();
}

void Lcd::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    g.setColour (col::lcdBg);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (col::blueDark);
    g.drawRoundedRectangle (b.reduced (0.5f), 4.0f, 1.0f);

    const auto a = plotArea();
    const auto& st = proc.apvts;

    // grid
    g.setFont (font (11.0f));
    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xForFreq (f);
        g.setColour (juce::Colour (0xff0f2340));
        g.drawVerticalLine (juce::roundToInt (x), a.getY(), a.getBottom());
        if (juce::exactlyEqual (f, 100.0f) || juce::exactlyEqual (f, 1000.0f) || juce::exactlyEqual (f, 10000.0f))
        {
            g.setColour (juce::Colour (0xff2f4f7a));
            g.drawText (f >= 1000.0f ? juce::String (juce::roundToInt (f / 1000.0f)) + "k" : juce::String (juce::roundToInt (f)),
                        juce::Rectangle<float> (x - 20.0f, a.getBottom() + 4.0f, 40.0f, 14.0f), juce::Justification::centred);
        }
    }
    for (float d : { -18.0f, -30.0f, -42.0f, -54.0f, -66.0f })
    {
        const float y = yForDb (d);
        g.setColour (juce::Colour (0xff0f2340));
        g.drawHorizontalLine (juce::roundToInt (y), a.getX(), a.getRight());
        g.setColour (juce::Colour (0xff2f4f7a));
        g.drawText (juce::String (juce::roundToInt (d)), juce::Rectangle<float> (2.0f, y - 7.0f, 28.0f, 14.0f), juce::Justification::centredRight);
    }

    // spectrum + peak hold
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (a.toNearestInt());
        juce::Path line, peak;
        for (int k = 0; k < kPoints; ++k)
        {
            const float f = 20.0f * std::pow (1000.0f, static_cast<float> (k) / static_cast<float> (kPoints - 1));
            const float x = xForFreq (f);
            const float y = yForDb (curveDb[static_cast<size_t> (k)]);
            const float py = yForDb (peakDb[static_cast<size_t> (k)]);
            if (k == 0) { line.startNewSubPath (x, y); peak.startNewSubPath (x, py); }
            else        { line.lineTo (x, y); peak.lineTo (x, py); }
        }
        juce::Path fill (line);
        fill.lineTo (a.getRight(), a.getBottom());
        fill.lineTo (a.getX(), a.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x5c2563eb), 0.0f, a.getY(),
                                                 juce::Colour (0x182563eb), 0.0f, a.getBottom(), false));
        g.fillPath (fill);
        g.setColour (col::blue);
        g.strokePath (line, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        const bool haalView = juce::roundToInt (st.getRawParameterValue ("algorithm")->load()) == HaldenReverbAudioProcessor::kHaalAlgo;
        juce::Path dashed;
        const float dashes[] = { 4.0f, 3.0f };
        const float dots[] = { 1.5f, 3.0f };
        juce::PathStrokeType (haalView ? 1.0f : 1.2f).createDashedStroke (dashed, peak, haalView ? dots : dashes, 2);
        g.setColour (juce::Colour (0xffe0f2fe).withAlpha (haalView ? 0.4f : 0.85f));
        g.fillPath (dashed);

        // Post-EQ curve: always while EQ is on, briefly while its cutoffs are being touched
        const bool eqOn = st.getRawParameterValue ("eqEnabled")->load() >= 0.5f;
        const float eqA = eqOn ? 0.9f : eqOv.alpha * 0.6f;
        if (eqA > 0.01f)
        {
            const float lo = dLo, hi = dHi;
            juce::Path eq;
            for (int i = 0; i <= 120; ++i)
            {
                const float f = 20.0f * std::pow (1000.0f, static_cast<float> (i) / 120.0f);
                const float mag = (1.0f / std::sqrt (1.0f + std::pow (lo / f, 4.0f))) * (1.0f / std::sqrt (1.0f + std::pow (f / hi, 4.0f)));
                const float y = yForDb (-12.0f + juce::Decibels::gainToDecibels (mag, -100.0f));
                if (i == 0) eq.startNewSubPath (xForFreq (f), y); else eq.lineTo (xForFreq (f), y);
            }
            g.setColour (col::amber.withAlpha (eqA));
            g.strokePath (eq, juce::PathStrokeType (1.4f));
        }
    }

    // HaalAi: learned voice curve (dashed) and the three tracking bands
    const int algoNow = juce::jlimit (0, 7, static_cast<int> (st.getRawParameterValue ("algorithm")->load()));
    if (algoNow == HaldenReverbAudioProcessor::kHaalAlgo)
        paintHaalAi (g, a);

    // decay envelope overlay: follows the knob on a fixed time axis, fades out on release
    const float envA = envOv.alpha;
    if (envA > 0.01f)
    {
        // cover the moving spectrum completely so nothing jitters under the curve
        g.setColour (col::lcdBg.withAlpha (0.97f * envA));
        g.fillRect (a.withBottom (a.getBottom() + 20.0f).expanded (2.0f));   // also hides the Hz labels

        const float decay = dDecay, pre = dPre, shape = dShape, size = dSize;
        const float maxSec = dSpan;
        const float preX = a.getX() + (pre * 0.001f / maxSec) * a.getWidth();
        const float bloomSec = 0.02f + shape * 0.12f + size * 0.05f;

        juce::Path env;
        env.startNewSubPath (a.getX(), a.getBottom());
        env.lineTo (preX, a.getBottom());
        for (int i = 0; i <= 140; ++i)
        {
            const float ts = static_cast<float> (i) / 140.0f * (maxSec - pre * 0.001f);
            const float att = ts < bloomSec ? std::pow (ts / bloomSec, 0.6f + shape) : 1.0f;
            const float lvl = att * std::pow (10.0f, -3.0f * ts / std::max (0.15f, decay));
            const float dbv = juce::Decibels::gainToDecibels (lvl, -100.0f);
            const float x = preX + (ts / maxSec) * a.getWidth();
            env.lineTo (x, a.getY() + a.getHeight() * juce::jlimit (0.0f, 1.0f, -dbv / 66.0f));
        }
        g.setColour (col::blueBright.withAlpha (envA));
        g.strokePath (env, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // time axis (replaces the frequency labels while the overlay is up)
        g.setFont (font (11.0f));
        for (int i = 0; i <= 4; ++i)
        {
            const float tsec = maxSec * static_cast<float> (i) / 4.0f;
            const float x = a.getX() + a.getWidth() * static_cast<float> (i) / 4.0f;
            g.setColour (juce::Colour (0xff0f2340).withAlpha (envA));
            g.drawVerticalLine (juce::roundToInt (x), a.getY(), a.getBottom());
            g.setColour (juce::Colour (0xff4f77ad).withAlpha (envA));
            const juce::String lab = tsec < 1.0f ? juce::String (juce::roundToInt (tsec * 1000.0f)) + " ms" : juce::String (tsec, tsec < 10.0f && std::abs (tsec - std::round (tsec)) > 0.01f ? 1 : 0) + " s";
            g.drawText (lab, juce::Rectangle<float> (x - 30.0f, a.getBottom() + 4.0f, 60.0f, 14.0f),
                        i == 0 ? juce::Justification::centredLeft : i == 4 ? juce::Justification::centredRight : juce::Justification::centred);
        }

        g.setFont (font (13.0f));
        g.drawText ("DECAY " + formatValue ("decay", st.getRawParameterValue ("decay")->load()) + "   PRE " + formatValue ("preDelayMs", st.getRawParameterValue ("preDelayMs")->load()),
                    juce::Rectangle<float> (a.getRight() - 300.0f, a.getY() + 4.0f, 296.0f, 16.0f), juce::Justification::centredRight);
    }

    // header text
    const int algo = algoNow;
    g.setColour (col::blue);
    g.setFont (font (15.0f));
    g.drawText (HaldenReverbAudioProcessor::algorithmNames()[algo].toUpperCase(), juce::Rectangle<float> (16.0f, 10.0f, 300.0f, 20.0f), juce::Justification::centredLeft);

    juce::StringArray fx;
    const auto on = [&st] (const char* id) { return st.getRawParameterValue (id)->load() >= 0.5f; };
    if (on ("pumpOn")) fx.add ("PUMP");
    if (on ("shimOn"))
    {
        static const char* p[] = { "\xe2\x88\x92" "12", "\xe2\x88\x92" "7", "+7", "+12" };
        fx.add ("SHIMMER " + juce::String (juce::CharPointer_UTF8 (p[juce::jlimit (0, 3, static_cast<int> (st.getRawParameterValue ("shimPitch")->load()))])));
    }
    if (on ("freezeOn")) fx.add ("FREEZE");
    if (on ("riserOn"))  fx.add ("RISER");
    if (on ("monoOn"))   fx.add ("MONO BASS");
    const float orbitNow = proc.spaceOrbitNow.load();
    if (orbitNow > 0.0f)
        fx.insert (0, "IN SPACE " + juce::String (juce::roundToInt (orbitNow * 100.0f)) + " %");
    if (! fx.isEmpty())
    {
        g.setColour (col::blueDeep);
        g.setFont (font (12.0f));
        g.drawText ("FX  " + fx.joinIntoString (juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))),
                    juce::Rectangle<float> (b.getRight() - 420.0f, 10.0f, 404.0f, 20.0f), juce::Justification::centredRight);
    }
}

float Lcd::niceSpan (float seconds)
{
    for (float s : { 0.5f, 1.0f, 2.0f, 3.0f, 5.0f, 8.0f, 12.0f, 16.0f, 24.0f, 32.0f, 48.0f })
        if (s >= seconds) return s;
    return 64.0f;
}

void Lcd::animateOverlays (float dt)
{
    const auto& st = proc.apvts;
    const float decay = st.getRawParameterValue ("decay")->load(), pre = st.getRawParameterValue ("preDelayMs")->load();
    const float need = decay * 1.3f + pre * 0.001f;

    for (auto* ov : { &envOv, &eqOv })
    {
        const bool active = ov->held || now - ov->last < 0.25;   // wheel / automation: short grace
        const float wasAlpha = ov->alpha;
        ov->alpha = active ? std::min (1.0f, ov->alpha + dt / 0.12f) : std::max (0.0f, ov->alpha - dt / 0.5f);
        if (ov == &envOv && wasAlpha <= 0.0f && ov->alpha > 0.0f)
        {
            // overlay appears: start from the current values, lock the time axis
            dDecay = decay; dPre = pre; dShape = st.getRawParameterValue ("shape")->load(); dSize = st.getRawParameterValue ("size")->load();
            spanTarget = dSpan = niceSpan (need);
        }
        if (ov == &eqOv && wasAlpha <= 0.0f && ov->alpha > 0.0f)
        {
            dLo = st.getRawParameterValue ("eqLowCutHz")->load(); dHi = st.getRawParameterValue ("eqHighCutHz")->load();
        }
    }

    // values glide after the knobs (no jumps), the axis only grows if the tail no longer fits
    const float k = 1.0f - std::exp (-dt / 0.06f);
    dDecay += k * (decay - dDecay);
    dPre += k * (pre - dPre);
    dShape += k * (st.getRawParameterValue ("shape")->load() - dShape);
    dSize += k * (st.getRawParameterValue ("size")->load() - dSize);
    dLo += k * (st.getRawParameterValue ("eqLowCutHz")->load() - dLo);
    dHi += k * (st.getRawParameterValue ("eqHighCutHz")->load() - dHi);
    if (need > spanTarget) spanTarget = niceSpan (need);
    dSpan += (1.0f - std::exp (-dt / 0.15f)) * (spanTarget - dSpan);
}

void Lcd::paintHaalAi (juce::Graphics& g, juce::Rectangle<float> a)
{
    const float sr = static_cast<float> (proc.scopeSampleRate.load());
    const bool learning = proc.haalLearning.load();
    const float progress = proc.haalProgress.load();
    const int nStatic = learning ? 0 : proc.haalNumStatic.load();

    std::array<haldendsp::BiquadFilter, HaldenReverbAudioProcessor::kStatic> bands;
    for (int b = 0; b < nStatic; ++b)
        bands[(size_t) b].setPeak (proc.haalStaticF[(size_t) b].load(), sr, proc.haalStaticQ[(size_t) b].load(), proc.haalStaticG[(size_t) b].load());
    haldendsp::BiquadFilter shelf, hp, light;
    shelf.setHighShelf (8000.0f, sr, learning ? 0.0f : proc.haalShelf.load());
    hp.setHighPass (120.0f, sr);
    light.setPeak (280.0f, sr, 0.7f, -2.0f);

    const float refY = a.getY() + a.getHeight() * 0.42f;
    const float pxPerDb = 4.0f;
    const auto voiceCurveDb = [&] (float f)
    {
        float d = 2.0f * hp.magnitudeDb (f, sr) + shelf.magnitudeDb (f, sr) + light.magnitudeDb (f, sr);
        for (int b = 0; b < nStatic; ++b) d += bands[(size_t) b].magnitudeDb (f, sr);
        return juce::jlimit (-14.0f, 6.0f, d);
    };

    // 0 dB reference and the learned curve (dashed)
    g.setColour (juce::Colour (0xff1d3b63));
    g.drawHorizontalLine (juce::roundToInt (refY), a.getX(), a.getRight());
    juce::Path curve;
    for (int i = 0; i <= 160; ++i)
    {
        const float f = 20.0f * std::pow (1000.0f, static_cast<float> (i) / 160.0f);
        const float y = refY - voiceCurveDb (f) * pxPerDb;
        if (i == 0) curve.startNewSubPath (xForFreq (f), y); else curve.lineTo (xForFreq (f), y);
    }
    juce::Path dashed;
    const float dashes[] = { 6.0f, 4.0f };
    juce::PathStrokeType (1.6f).createDashedStroke (dashed, curve, dashes, 2);
    g.setColour (juce::Colour (0xffe8d9a8).withAlpha (learning ? 0.35f : 0.9f));
    g.fillPath (dashed);

    // D1-D3: circles riding on the curve, gain-reduction bar under each
    for (int b = 0; b < HaldenReverbAudioProcessor::kDyn; ++b)
    {
        const float f = proc.haalDynF[(size_t) b].load();
        const float gr = -proc.haalDynG[(size_t) b].load();            // 0..2 dB
        const float x = xForFreq (juce::jlimit (25.0f, 19000.0f, f));
        const float y = refY - (voiceCurveDb (f) - gr) * pxPerDb;
        const float active = juce::jlimit (0.0f, 1.0f, gr / 2.0f);
        const auto ringCol = col::iceBlue.interpolatedWith (col::amber, active);

        if (active > 0.05f)
        {
            g.setColour (ringCol.withAlpha (0.25f * active));
            g.fillEllipse (x - 15.0f, y - 15.0f, 30.0f, 30.0f);
        }
        g.setColour (col::lcdBg);
        g.fillEllipse (x - 11.0f, y - 11.0f, 22.0f, 22.0f);
        g.setColour (ringCol);
        g.drawEllipse (x - 11.0f, y - 11.0f, 22.0f, 22.0f, 1.6f);
        g.setFont (font (10.5f, true));
        g.drawText ("D" + juce::String (b + 1), juce::Rectangle<float> (x - 11.0f, y - 11.0f, 22.0f, 22.0f), juce::Justification::centred);

        // activity meter: grows with the cut (full = 2 dB)
        const juce::Rectangle<float> meter (x - 3.0f, y + 15.0f, 6.0f, 22.0f);
        g.setColour (juce::Colour (0xff0f2340));
        g.fillRoundedRectangle (meter, 2.0f);
        g.setColour (ringCol);
        g.fillRoundedRectangle (meter.withTop (meter.getBottom() - meter.getHeight() * active), 2.0f);
        g.setFont (font (10.0f));
        g.drawText (gr > 0.05f ? juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (gr, 1) : juce::String ("0.0"),
                    juce::Rectangle<float> (x - 20.0f, meter.getBottom() + 1.0f, 40.0f, 12.0f), juce::Justification::centred);
    }

    // status line
    g.setFont (font (12.0f));
    if (learning)
    {
        const juce::Rectangle<float> bar (a.getX() + 4.0f, a.getY() + 9.0f, 160.0f, 4.0f);
        g.setColour (juce::Colour (0xff0f2340));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (col::amber);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * progress), 2.0f);
        g.drawText ("LEARNING VOICE  " + juce::String (juce::roundToInt (progress * 100.0f)) + " %   sing for 4 s",
                    juce::Rectangle<float> (bar.getRight() + 10.0f, a.getY() + 4.0f, 300.0f, 14.0f), juce::Justification::centredLeft);
    }
    else if (progress >= 1.0f)
    {
        g.setColour (juce::Colour (0xffe8d9a8));
        g.drawText ("VOICE CURVE  - - -     D1 D2 D3  max 2 dB", juce::Rectangle<float> (a.getX() + 4.0f, a.getY() + 4.0f, 330.0f, 14.0f), juce::Justification::centredLeft);
    }
}

// =========================================================================== IN SPACE: glass knob
namespace
{
const juce::Colour vLight { 0xffc4b5fd }, vMid { 0xffa78bfa }, vDeep { 0xff8b5cf6 }, vText { 0xfff3efff }, vDim { 0xff9a90c8 };
}

GlassKnob::GlassKnob (juce::AudioProcessorValueTreeState& s, const juce::String& id, const juce::String& l, float r, bool isBig)
    : paramId (id), label (l), radius (r), big (isBig)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (kStartAngle, kEndAngle, true);
    slider.setMouseDragSensitivity (big ? 320 : 220);
    addAndMakeVisible (slider);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (s, id, slider);
    if (auto* p = s.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    slider.onValueChange = [this] { repaint(); };
}

void GlassKnob::resized()
{
    const int d = juce::roundToInt (radius * 2.0f + 30.0f);
    slider.setBounds (juce::Rectangle<int> (d, d).withCentre (getLocalBounds().getCentre()));
}

void GlassKnob::paint (juce::Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float r = radius, R = r + (big ? 18.0f : 11.0f);
    float v = static_cast<float> (slider.valueToProportionOfLength (slider.getValue()));
    if (liveValue) v = liveValue();
    const float a = kStartAngle + v * (kEndAngle - kStartAngle);
    const auto pt = [&] (float ang, float rad) { return juce::Point<float> (c.x + rad * std::sin (ang), c.y - rad * std::cos (ang)); };

    // label above, value below
    g.setColour (juce::Colour (0xffd6d0f0));
    g.setFont (font (big ? 17.0f : 14.0f));
    juce::String spaced;
    for (auto ch : label) { spaced << juce::String::charToString (ch) << (big ? "  " : " "); }
    g.drawText (spaced.trimEnd(), juce::Rectangle<float> (0.0f, c.y - R - (big ? 40.0f : 34.0f), static_cast<float> (getWidth()), 22.0f), juce::Justification::centred);

    // remaining scale: fine dots
    g.setColour (juce::Colour (0xff6d5bb8));
    const float span = kEndAngle - a;
    const int dots = static_cast<int> (span * R / 7.0f);
    for (int i = 1; i <= dots; ++i)
    {
        const auto p = pt (a + span * static_cast<float> (i) / static_cast<float> (dots), R);
        g.fillEllipse (p.x - 1.1f, p.y - 1.1f, 2.2f, 2.2f);
    }

    // value arc with a soft glow
    if (v > 0.002f)
    {
        juce::Path arc;
        arc.addCentredArc (c.x, c.y, R, R, 0.0f, kStartAngle, a, true);
        const float w = big ? 5.0f : 3.5f;
        for (auto [width, alpha] : { std::pair<float, float> { w * 5.0f, 0.06f }, { w * 3.2f, 0.10f }, { w * 2.0f, 0.18f } })
        {
            g.setColour (vDeep.withAlpha (alpha));
            g.strokePath (arc, juce::PathStrokeType (width, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setGradientFill (juce::ColourGradient (vDeep, pt (kStartAngle, R), vLight, pt (a, R), false));
        g.strokePath (arc, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // shadow, glass body, rim
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r * 1.02f, c.y - r + r * 0.12f, r * 2.04f, r * 2.04f);
    {
        juce::ColourGradient body (juce::Colour (0xff3a3158), c.x - r * 0.35f, c.y - r * 0.45f, juce::Colour (0xff07060d), c.x + r * 0.6f, c.y + r * 0.9f, true);
        body.addColour (0.55, juce::Colour (0xff17132a));
        g.setGradientFill (body);
        g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
    }
    g.setColour (vDeep.withAlpha (0.06f + 0.22f * v));   // inner light grows with the value
    g.fillEllipse (c.x - r * 0.8f, c.y - r * 0.8f, r * 1.6f, r * 1.6f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.45f), c.x, c.y - r, juce::Colour (0xff2a2347).withAlpha (0.6f), c.x, c.y + r, false));
    g.drawEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f, big ? 1.6f : 1.2f);
    {
        juce::Graphics::ScopedSaveState ss (g);
        juce::Path clip; clip.addEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.32f), c.x, c.y - r, juce::Colours::white.withAlpha (0.0f), c.x, c.y - r * 0.05f, false));
        g.fillEllipse (c.x - r * 0.78f, c.y - r * 0.98f, r * 1.56f, r * 0.95f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));       // reflection at the lower right
        juce::Path refl; refl.addCentredArc (c.x, c.y, r * 0.82f, r * 0.82f, 0.0f, 2.0f, 2.9f, true);
        g.strokePath (refl, juce::PathStrokeType (r * 0.12f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // pointer from the centre
    const auto p0 = pt (a, r * 0.06f), p1 = pt (a, r * 0.62f);
    g.setColour (vDeep.withAlpha (0.35f));
    g.drawLine ({ p0, p1 }, big ? 9.0f : 6.0f);
    g.setColour (vLight);
    g.drawLine ({ p0, p1 }, big ? 4.0f : 3.0f);

    // the "satellite" at the end of the arc
    const auto h = pt (a, R);
    const float hb = big ? 8.0f : 5.0f;
    g.setColour (vMid.withAlpha (0.25f));
    g.fillEllipse (h.x - hb * 2.0f, h.y - hb * 2.0f, hb * 4.0f, hb * 4.0f);
    g.setGradientFill (juce::ColourGradient (juce::Colours::white, h.x - hb * 0.4f, h.y - hb * 0.4f, vLight, h.x + hb, h.y + hb, true));
    g.fillEllipse (h.x - hb, h.y - hb, hb * 2.0f, hb * 2.0f);

    // hints at the ends and the value
    if (lowHint.isNotEmpty())
    {
        g.setColour (vDim);
        g.setFont (font (15.0f));
        const auto lo = pt (kStartAngle, R + 26.0f), hi = pt (kEndAngle, R + 26.0f);
        g.drawText (lowHint, juce::Rectangle<float> (lo.x - 50.0f, lo.y - 4.0f, 70.0f, 20.0f), juce::Justification::centred);
        g.drawText (highHint, juce::Rectangle<float> (hi.x - 20.0f, hi.y - 4.0f, 70.0f, 20.0f), juce::Justification::centred);
    }
    g.setColour (vText);
    g.setFont (font (big ? 30.0f : 17.0f));
    const juce::String text = paramId == "orbit" ? juce::String (juce::roundToInt (v * 100.0f)) + " %"
                            : paramId == "spaceDecay" ? juce::String (slider.getValue(), 1) + " s"
                            : juce::String (juce::roundToInt (slider.getValue() * 100.0)) + " %";
    g.drawText (text, juce::Rectangle<float> (0.0f, c.y + R + (big ? 12.0f : 10.0f), static_cast<float> (getWidth()), big ? 36.0f : 22.0f), juce::Justification::centred);
}

// =========================================================================== IN SPACE: window
SpaceWindow::SpaceWindow (HaldenReverbAudioProcessor& p)
    : proc (p),
      orbit (p.apvts, "orbit", "ORBIT", 82.0f, true),
      size (p.apvts, "spaceSize", "SIZE", 36.0f, false),
      decay (p.apvts, "spaceDecay", "DECAY", 36.0f, false),
      launch (p.apvts, "launchBars"),
      routing (p.apvts, "routing")
{
    orbit.lowHint = "ground";
    orbit.highHint = "orbit";
    orbit.liveValue = [this] { return proc.spaceLaunchProgress.load() > 0.0f ? proc.spaceOrbitNow.load()
                                                                            : static_cast<float> (orbit.slider.valueToProportionOfLength (orbit.slider.getValue())); };
    launch.violet = routing.violet = true;

    go.setButtonText ("GO");
    go.getProperties().set ("style", "spaceGo");
    go.setTooltip ("ORBIT rises to 100 % over the chosen bars, then drops. Press again to stop.");
    go.onClick = [this] { proc.spaceGoRequest.store (true); };

    stardust.setButtonText ("STARDUST +12");
    stardust.setClickingTogglesState (true);
    stardust.getProperties().set ("style", "spaceToggle");
    stardust.setTooltip ("An octave-up shimmer that blooms near the top of ORBIT");
    stardustAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "stardust", stardust);

    close.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x95")));
    close.getProperties().set ("style", "spaceText");
    close.onClick = [this] { if (onClose) onClose(); };

    for (juce::Component* c : { (juce::Component*) &orbit, (juce::Component*) &size, (juce::Component*) &decay, (juce::Component*) &launch,
                                (juce::Component*) &routing, (juce::Component*) &go, (juce::Component*) &stardust, (juce::Component*) &close })
        addAndMakeVisible (c);
}

void SpaceWindow::resized()
{
    const int w = getWidth(), h = getHeight();
    close.setBounds (w - 52, 12, 40, 34);
    orbit.setBounds (w / 2 - 170, 40, 340, 330);
    size.setBounds (110, 120, 160, 190);
    decay.setBounds (w - 270, 120, 160, 190);
    routing.setBounds (30, h - 64, 170, 38);
    launch.setBounds (w / 2 - 190, h - 64, 260, 38);
    go.setBounds (w / 2 + 86, h - 66, 96, 42);
    stardust.setBounds (w - 210, h - 64, 180, 38);
}

void SpaceWindow::refresh()
{
    const float pr = proc.spaceLaunchProgress.load();
    const float ob = proc.spaceOrbitNow.load();
    if (std::abs (pr - lastProgress) > 0.002f || std::abs (ob - lastOrbit) > 0.002f)
    {
        go.getProperties().set ("progress", pr);
        go.setButtonText (pr > 0.0f ? "STOP" : "GO");
        go.repaint();
        orbit.repaint();
        lastProgress = pr;
        lastOrbit = ob;
    }
}

void SpaceWindow::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float rr = 20.0f;

    g.setColour (juce::Colours::black.withAlpha (0.55f));   // drop shadow onto the panel
    g.fillRoundedRectangle (b.translated (0.0f, 6.0f), rr);

    juce::Path shape;
    shape.addRoundedRectangle (b, rr);
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (shape);
        juce::ColourGradient bg (juce::Colour (0xff1d1636), b.getX(), b.getY(), juce::Colour (0xff120d24), b.getRight(), b.getBottom(), false);
        bg.addColour (0.5, juce::Colour (0xff0b0915));
        g.setGradientFill (bg);
        g.fillRect (b);

        // diagonal light streaks, like light catching a glass panel
        const auto streak = [&] (float x0, float width, float alpha)
        {
            juce::Path p;
            p.startNewSubPath (x0, b.getBottom());
            p.lineTo (x0 + width, b.getBottom());
            p.lineTo (x0 + width + b.getHeight() * 0.62f, b.getY());
            p.lineTo (x0 + b.getHeight() * 0.62f, b.getY());
            p.closeSubPath();
            juce::ColourGradient gr (vDeep.withAlpha (0.0f), x0, 0.0f, vDeep.withAlpha (0.0f), x0 + width, 0.0f, false);
            gr.addColour (0.5, vMid.withAlpha (alpha));
            g.setGradientFill (gr);
            g.fillPath (p);
        };
        streak (b.getX() - 120.0f, 180.0f, 0.16f);
        streak (b.getX() + 10.0f, 40.0f, 0.10f);
        streak (b.getRight() - 330.0f, 120.0f, 0.13f);
        streak (b.getRight() - 160.0f, 30.0f, 0.09f);

        // faint stars
        juce::Random rnd (369);
        for (int i = 0; i < 70; ++i)
        {
            const float x = rnd.nextFloat() * b.getWidth(), y = rnd.nextFloat() * b.getHeight();
            g.setColour (juce::Colour (0xffd8ccff).withAlpha (0.10f + 0.35f * rnd.nextFloat()));
            const float d = 0.8f + rnd.nextFloat() * 1.2f;
            g.fillEllipse (x, y, d, d);
        }
        // top sheen
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.06f), 0.0f, b.getY(), juce::Colours::white.withAlpha (0.0f), 0.0f, b.getY() + 90.0f, false));
        g.fillRect (b.withHeight (90.0f));
    }
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.30f), b.getX(), b.getY(), vDeep.withAlpha (0.35f), b.getRight(), b.getBottom(), false));
    g.strokePath (shape, juce::PathStrokeType (1.4f));

    g.setColour (juce::Colour (0xffe9e3ff));
    g.setFont (font (22.0f));
    g.drawText ("I  N     S  P  A  C  E", juce::Rectangle<float> (34.0f, 20.0f, 360.0f, 30.0f), juce::Justification::centredLeft);
    g.setColour (vDim);
    g.setFont (font (12.0f));
    g.drawText ("LAUNCH", juce::Rectangle<float> (b.getCentreX() - 190.0f, b.getBottom() - 86.0f, 260.0f, 16.0f), juce::Justification::centred);
}

// =========================================================================== FX module
FxModule::FxModule (juce::AudioProcessorValueTreeState& s, const juce::String& onParamId, const juce::String& t)
    : state (s), onId (onParamId), title (t)
{
    onAttachment = std::make_unique<juce::ParameterAttachment> (*s.getParameter (onParamId), [this] (float) { repaint(); });
}

bool FxModule::isOn() const { return state.getRawParameterValue (onId)->load() >= 0.5f; }

void FxModule::addKnob (const juce::String& id, const juce::String& label)
{
    knob = std::make_unique<Knob> (state, id, label, 15.0f, false);
    addAndMakeVisible (*knob);
}

void FxModule::addChoices (const juce::String& id)
{
    choices = std::make_unique<ChoiceRow> (state, id);
    choices->isActive = [this] { return isOn(); };
    addAndMakeVisible (*choices);
}

void FxModule::addToggle (const juce::String& id, const juce::String& text)
{
    toggle = std::make_unique<juce::TextButton> (text);
    toggle->setClickingTogglesState (true);
    setStyle (*toggle, "small");
    addAndMakeVisible (*toggle);
    toggleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, id, *toggle);
}

void FxModule::resized()
{
    if (knob)    knob->setBounds (getWidth() - 78, 2, 76, getHeight() - 4);
    if (choices) choices->setBounds (12, 38, 162, 20);
    if (toggle)  toggle->setBounds (12, 62, 64, 18);
}

void FxModule::mouseDown (const juce::MouseEvent& e)
{
    if (e.y < 34 && e.x < 180)
        onAttachment->setValueAsCompleteGesture (isOn() ? 0.0f : 1.0f);
}

void FxModule::paint (juce::Graphics& g)
{
    const bool on = isOn();
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (on ? juce::Colour (0xff0c1a30) : juce::Colour (0xff111318));
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (on ? juce::Colour (0xff2563eb) : col::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    if (on)
    {
        g.setColour (col::blue.withAlpha (0.35f));
        g.fillEllipse (10.0f, 10.0f, 16.0f, 16.0f);
    }
    g.setColour (on ? col::blue : juce::Colour (0xff1f2937));
    g.fillEllipse (13.0f, 13.0f, 10.0f, 10.0f);

    g.setColour (on ? juce::Colour (0xffdbeafe) : col::textLo);
    g.setFont (font (14.0f));
    g.drawText (title, juce::Rectangle<int> (32, 8, 150, 20), juce::Justification::centredLeft);

    if (hint.isNotEmpty())
    {
        g.setColour (on ? col::blueBright : col::textDim);
        g.setFont (font (11.0f));
        g.drawFittedText (hint, juce::Rectangle<int> (14, 38, 166, 40), juce::Justification::topLeft, 3, 1.0f);
    }
}

// =========================================================================== Editor
namespace
{
const juce::Identifier idDrawer ("uiDrawerOpen"), idRack ("uiRackOpen"), idScale ("uiScale");

struct KnobDef { const char* id; const char* label; int cx; };
const KnobDef kMain[] = { { "preDelayMs", "PRE-DELAY", 160 }, { "size", "SIZE", 330 }, { "decay", "DECAY", 500 },
                          { "highCutHz", "DAMPING", 670 }, { "mix", "MIX", 840 } };
const KnobDef kAdv[]  = { { "shape", "SHAPE", 80 }, { "spread", "SPREAD", 150 }, { "diffusion", "DIFFUSION", 220 },
                          { "bassMult", "BASS", 365 }, { "crossoverHz", "X-OVER", 435 },
                          { "spin", "SPIN", 535 }, { "wander", "WANDER", 605 },
                          { "width", "WIDTH", 705 }, { "ducking", "DUCKING", 775 },
                          { "eqLowCutHz", "LO CUT", 860 }, { "eqHighCutHz", "HI CUT", 925 } };
const char* kAlgoShort[] = { "R.HALL", "C.HALL", "HALL", "CHAMBER", "ROOM", "PLATE", "AMBIENCE", "HaalAi" };
const float kScales[] = { 0.6f, 0.75f, 1.0f, 1.25f };
} // namespace

HaldenReverbAudioProcessorEditor::HaldenReverbAudioProcessorEditor (HaldenReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      inFader (p.apvts, "inGain"), outFader (p.apvts, "outGain"), lcd (p)
{
    setLookAndFeel (&lnf);
    addAndMakeVisible (content);

    drawerOpen = static_cast<bool> (p.apvts.state.getProperty (idDrawer, false));
    rackOpen   = static_cast<bool> (p.apvts.state.getProperty (idRack, false));
    uiScale    = static_cast<float> (static_cast<double> (p.apvts.state.getProperty (idScale, 1.0)));

    // algorithm buttons
    for (int i = 0; i < 8; ++i)
    {
        auto b = std::make_unique<juce::TextButton> (kAlgoShort[i]);
        setStyle (*b, "algo");
        b->onClick = [this, i] { algoAttachment->setValueAsCompleteGesture (static_cast<float> (i)); };
        content.addAndMakeVisible (*b);
        algoButtons.push_back (std::move (b));
    }
    algoAttachment = std::make_unique<juce::ParameterAttachment> (*p.apvts.getParameter ("algorithm"), [this] (float v)
    {
        currentAlgo = juce::roundToInt (v);
        for (size_t i = 0; i < algoButtons.size(); ++i)
            algoButtons[i]->setToggleState (static_cast<int> (i) == currentAlgo, juce::dontSendNotification);
        learnButton.setVisible (currentAlgo == HaldenReverbAudioProcessor::kHaalAlgo);
        lcd.repaint();
    });

    learnButton.setButtonText ("RE-LEARN");
    setStyle (learnButton, "small");
    learnButton.setTooltip ("Listen to the voice again for 4 s and rebuild the curve");
    learnButton.onClick = [this] { proc.haalRequestLearn.store (true); };
    content.addChildComponent (learnButton);
    algoAttachment->sendInitialUpdate();

    content.addAndMakeVisible (inFader);
    content.addAndMakeVisible (outFader);
    content.addAndMakeVisible (inMeter);
    content.addAndMakeVisible (outMeter);
    content.addAndMakeVisible (lcd);
    learnButton.toFront (false);

    for (const auto& d : kMain)
    {
        auto k = std::make_unique<hui::Knob> (p.apvts, d.id, d.label, 40.0f, true);
        const juce::String id (d.id);
        if (id == "preDelayMs" || id == "size" || id == "decay")
            k->onTouched = [this] { lcd.showEnvelope (juce::Time::getMillisecondCounterHiRes() * 0.001); };
            k->onHold = [this] (bool held) { lcd.holdEnvelope (held, juce::Time::getMillisecondCounterHiRes() * 0.001); };
        content.addAndMakeVisible (*k);
        mainKnobs.push_back (std::move (k));
    }

    for (const auto& d : kAdv)
    {
        auto k = std::make_unique<hui::Knob> (p.apvts, d.id, d.label, 22.0f, false);
        const juce::String id (d.id);
        if (id == "shape")
            k->onTouched = [this] { lcd.showEnvelope (juce::Time::getMillisecondCounterHiRes() * 0.001); };
            k->onHold = [this] (bool held) { lcd.holdEnvelope (held, juce::Time::getMillisecondCounterHiRes() * 0.001); };
        if (id == "eqLowCutHz" || id == "eqHighCutHz")
            k->onTouched = [this] { lcd.showEq (juce::Time::getMillisecondCounterHiRes() * 0.001); };
            k->onHold = [this] (bool held) { lcd.holdEq (held, juce::Time::getMillisecondCounterHiRes() * 0.001); };
        if (id == "ducking")
        {
            duckKnob = k.get();
            k->ringReductionDb = [this] { return proc.apvts.getRawParameterValue ("ducking")->load() > 0.001f ? proc.duckReductionDb.load() : 0.0f; };
            k->valueOverride = [this]() -> juce::String
            {
                const float gr = proc.duckReductionDb.load();
                if (proc.apvts.getRawParameterValue ("ducking")->load() <= 0.001f) return "OFF";
                if (duckKnob != nullptr && duckKnob->slider.isMouseButtonDown()) return {};
                if (gr < 0.1f) return {};
                return juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")) + juce::String (gr, 1) + " dB";
            };
        }
        content.addChildComponent (*k);
        advKnobs.push_back (std::move (k));
    }

    eqButton.setButtonText ("EQ");
    eqButton.setClickingTogglesState (true);
    setStyle (eqButton, "small");
    content.addChildComponent (eqButton);
    eqAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.apvts, "eqEnabled", eqButton);

    setStyle (drawerTab, "tab");
    drawerTab.onClick = [this] { drawerOpen = ! drawerOpen; setUiProp (idDrawer, drawerOpen); applyWindowSize(); };
    content.addAndMakeVisible (drawerTab);

    setStyle (rackTab, "tab");
    rackTab.onClick = [this] { rackOpen = ! rackOpen; setUiProp (idRack, rackOpen); applyWindowSize(); };
    content.addAndMakeVisible (rackTab);

    // MIX shows SEND when the plug-in sits on a bus
    mixKnob = mainKnobs.back().get();
    mixKnob->valueOverride = [this]() -> juce::String
    {
        return proc.apvts.getRawParameterValue ("routing")->load() >= 0.5f ? juce::String ("SEND 100 %") : juce::String();
    };
    routingRow = std::make_unique<hui::ChoiceRow> (p.apvts, "routing");
    content.addAndMakeVisible (*routingRow);

    // IN SPACE entry under HaalAi, and a quiet "reset" above it
    setStyle (spaceButton, "spaceEntry");
    spaceButton.setTooltip ("Click: IN SPACE on, open or close its window.  Double-click: IN SPACE off");
    spaceButton.onSingle = [this]
    {
        if (proc.apvts.getRawParameterValue ("spaceOn")->load() < 0.5f) { setSpaceOn (true); setSpaceOpen (true); }
        else setSpaceOpen (! spaceWindow->isVisible());
    };
    spaceButton.onDouble = [this] { setSpaceOpen (false); setSpaceOn (false); };
    content.addAndMakeVisible (spaceButton);

    resetButton.setButtonText ("reset");
    setStyle (resetButton, "reset");
    resetButton.setTooltip ("All controls back to how the plug-in opens");
    resetButton.onClick = [this] { proc.resetAllToDefaults(); };
    content.addAndMakeVisible (resetButton);

    spaceWindow = std::make_unique<hui::SpaceWindow> (p);
    spaceWindow->onClose = [this] { setSpaceOpen (false); };
    content.addChildComponent (*spaceWindow);

    for (int i = 0; i < 4; ++i)
    {
        auto& b = scaleButtons[static_cast<size_t> (i)];
        b.setButtonText (juce::String (juce::roundToInt (kScales[i] * 100.0f)) + (i == 3 ? " %" : ""));
        setStyle (b, "scale");
        b.onClick = [this, i] { uiScale = kScales[i]; setUiProp (idScale, uiScale); applyWindowSize(); };
        content.addAndMakeVisible (b);
    }

    // FX rack
    {
        auto add = [this] (const char* onId, const char* title) -> hui::FxModule&
        {
            fxModules.push_back (std::make_unique<hui::FxModule> (proc.apvts, onId, title));
            content.addChildComponent (*fxModules.back());
            return *fxModules.back();
        };
        auto& pump = add ("pumpOn", "PUMP");          pump.addChoices ("pumpRate");  pump.addKnob ("pumpDepth", "DEPTH");
        auto& shim = add ("shimOn", "SHIMMER");       shim.addChoices ("shimPitch"); shim.addToggle ("shimGlide", "GLIDE"); shim.addKnob ("shimMix", "AMOUNT");
        auto& frz  = add ("freezeOn", "FREEZE");      frz.setHint ("infinite hold of the tail");  frz.addKnob ("freezeFade", "FADE");
        auto& ris  = add ("riserOn", "RISER");        ris.setHint ("build-up macro: size, decay, width, air"); ris.addKnob ("riserAmt", "AMOUNT");
        auto& mono = add ("monoOn", "MONO BASS");     mono.setHint ("lows of the tail in mono"); mono.addKnob ("monoHz", "BELOW");
    }

    applyWindowSize();
    startTimerHz (30);
}

HaldenReverbAudioProcessorEditor::~HaldenReverbAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void HaldenReverbAudioProcessorEditor::setUiProp (const juce::Identifier& id, const juce::var& v)
{
    proc.apvts.state.setProperty (id, v, nullptr);
}

float HaldenReverbAudioProcessorEditor::maxScaleForScreen (int w, int h) const
{
    const auto& displays = juce::Desktop::getInstance().getDisplays();
    const juce::Displays::Display* d = isShowing() ? displays.getDisplayForRect (getScreenBounds()) : displays.getPrimaryDisplay();
    if (d == nullptr) return 2.0f;
    // leave room for the host's title bar, the plug-in header and the taskbar / dock
    const auto area = d->userArea;
    return std::min (static_cast<float> (area.getWidth()) * 0.96f / static_cast<float> (w),
                     static_cast<float> (area.getHeight()) * 0.86f / static_cast<float> (h));
}

void HaldenReverbAudioProcessorEditor::applyWindowSize()
{
    const int w = kPanelW + (rackOpen ? kRackW : 0);
    const int h = drawerOpen ? kOpenH : kClosedH;
    // never open bigger than the screen: shrink automatically on small laptop displays
    effScale = juce::jlimit (0.45f, 1.6f, std::min (uiScale, maxScaleForScreen (w, h)));

    applyingSize = true;
    setResizable (true, true);
    if (auto* c = getConstrainer())
    {
        c->setFixedAspectRatio (static_cast<double> (w) / static_cast<double> (h));
        c->setSizeLimits (juce::roundToInt (w * 0.45f), juce::roundToInt (h * 0.45f), juce::roundToInt (w * 1.6f), juce::roundToInt (h * 1.6f));
    }
    content.setBounds (0, 0, w, h);
    content.setTransform (juce::AffineTransform::scale (effScale));
    layout();
    setSize (juce::roundToInt (static_cast<float> (w) * effScale), juce::roundToInt (static_cast<float> (h) * effScale));
    applyingSize = false;
    content.repaint();
}

void HaldenReverbAudioProcessorEditor::resized()
{
    if (applyingSize) return;
    // dragged by the corner: follow the new width (the aspect ratio is fixed)
    const int w = content.getWidth();
    if (w <= 0) return;
    effScale = uiScale = static_cast<float> (getWidth()) / static_cast<float> (w);
    setUiProp ("uiScale", uiScale);
    content.setTransform (juce::AffineTransform::scale (effScale));
    for (size_t i = 0; i < scaleButtons.size(); ++i)
        scaleButtons[i].setToggleState (std::abs (kScales[i] - effScale) < 0.02f, juce::dontSendNotification);
}

void HaldenReverbAudioProcessorEditor::layout()
{
    const int h = content.getHeight();

    int total = 0;
    for (size_t i = 0; i < algoButtons.size(); ++i) total += juce::String (kAlgoShort[i]).length() * 8 + 18 + 6;
    int x = 964 - total + 6;
    for (size_t i = 0; i < algoButtons.size(); ++i)
    {
        const int w = juce::String (kAlgoShort[i]).length() * 8 + 18;
        algoButtons[i]->setBounds (x, 30, w, 34);
        x += w + 6;
    }
    learnButton.setBounds (150 + 700 - 96, 100 + 250 - 54, 78, 20);
    {
        const auto hb = algoButtons.back()->getBounds();
        spaceButton.setBounds (hb.getX(), hb.getBottom() + 5, hb.getWidth(), 22);
        resetButton.setBounds (hb.getX(), 8, hb.getWidth(), 18);
    }
    routingRow->setBounds (kMain[4].cx - 46, 540, 92, 17);
    spaceWindow->setBounds (40, 96, 920, 470);

    inFader.setBounds (40, 98, 40, 272);
    inMeter.setBounds (96, 104, 26, 242);
    lcd.setBounds (150, 100, 700, 250);
    outMeter.setBounds (878, 104, 26, 242);
    outFader.setBounds (922, 98, 40, 272);

    for (size_t i = 0; i < mainKnobs.size(); ++i)
        mainKnobs[i]->setBounds (kMain[i].cx - 60, 398, 120, 140);

    drawerTab.setButtonText (juce::String ("ADVANCED ") + juce::String (juce::CharPointer_UTF8 (drawerOpen ? "\xe2\x96\xb4" : "\xe2\x96\xbe")));
    drawerTab.setBounds (420, 545, 160, 30);

    for (size_t i = 0; i < advKnobs.size(); ++i)
    {
        advKnobs[i]->setVisible (drawerOpen);
        advKnobs[i]->setBounds (kAdv[i].cx - 38, 626, 76, 110);
    }
    eqButton.setVisible (drawerOpen);
    eqButton.setBounds (868, 750, 52, 22);

    rackTab.setButtonText (juce::String ("FX ") + juce::String (juce::CharPointer_UTF8 (rackOpen ? "\xe2\x97\x82" : "\xe2\x96\xb8")));
    rackTab.setBounds (972, juce::jmin (300, h / 2 - 65), 24, 130);

    for (size_t i = 0; i < fxModules.size(); ++i)
    {
        fxModules[i]->setVisible (rackOpen);
        fxModules[i]->setBounds (kPanelW + 22, 50 + static_cast<int> (i) * 92, 256, 86);
    }

    int sx = 800;
    for (size_t i = 0; i < scaleButtons.size(); ++i)
    {
        scaleButtons[i].setToggleState (std::abs (kScales[i] - effScale) < 0.02f, juce::dontSendNotification);
        scaleButtons[i].setBounds (sx, h - 30, i == 3 ? 46 : 36, 20);
        sx += i == 3 ? 46 : 38;
    }
}

void HaldenReverbAudioProcessorEditor::Content::paint (juce::Graphics& g)
{
    const auto& e = owner;
    const float h = static_cast<float> (getHeight());

    g.fillAll (juce::Colour (0xff050607));
    g.setColour (col::panel);
    g.fillRoundedRectangle (0.0f, 0.0f, static_cast<float> (kPanelW), h, 10.0f);

    // header
    g.setColour (col::textHi);
    g.setFont (font (26.0f));
    g.drawText ("HALDEN 369", 40, 28, 280, 30, juce::Justification::centredLeft);
    g.setColour (col::textDim);
    g.setFont (font (13.0f));
    g.drawText ("DIGITAL REVERBERATOR", 40, 60, 280, 18, juce::Justification::centredLeft);

    g.setColour (col::textLo);
    g.setFont (font (12.0f));
    g.drawText ("IN", 96, 352, 26, 16, juce::Justification::centred);
    g.drawText ("OUT", 870, 352, 42, 16, juce::Justification::centred);

    // drawer divider
    g.setColour (col::line);
    g.drawHorizontalLine (560, 40.0f, 410.0f);
    g.drawHorizontalLine (560, 590.0f, 960.0f);

    if (e.drawerOpen)
    {
        const juce::Rectangle<float> d (30.0f, 587.0f, 940.0f, 200.0f);
        g.setColour (col::drawer);
        g.fillRoundedRectangle (d, 6.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (d, 6.0f, 1.0f);
        for (int xLine : { 315, 485, 655, 825 })
            g.drawVerticalLine (xLine, 600.0f, 775.0f);
        g.setColour (col::blueDeep);
        g.setFont (font (12.0f));
        const std::pair<const char*, int> groups[] = { { "SPACE", 150 }, { "TONE", 400 }, { "MOTION", 570 }, { "OUTPUT", 740 }, { "POST EQ", 892 } };
        for (const auto& gr : groups)
            g.drawText (gr.first, gr.second - 60, 598, 120, 16, juce::Justification::centred);
    }

    // footer: Moon logo + MOON AUDIO ENGINEERING (subtle)
    const float fy = h - 40.0f;
    g.setColour (col::lineDim);
    g.drawHorizontalLine (juce::roundToInt (fy), 30.0f, 970.0f);
    {
        juce::Path logo;
        logo.setUsingNonZeroWinding (false);
        const float lx = 46.0f, ly = fy + 20.0f, lr = 9.0f;
        logo.addEllipse (lx - lr, ly - lr, lr * 2.0f, lr * 2.0f);
        for (float off : { -3.9f, 0.0f, 3.9f })
            logo.addRectangle (lx + off - 1.1f, ly - lr * 0.9f, 2.2f, lr * 1.8f);
        g.setColour (juce::Colour (0xff7c838f));
        g.fillPath (logo);
    }
    g.setColour (col::textLo);
    g.setFont (font (11.0f));
    g.drawFittedText ("M O O N   A U D I O   E N G I N E E R I N G", 64, juce::roundToInt (fy) + 12, 320, 16, juce::Justification::centredLeft, 1);
    g.setColour (juce::Colour (0xff374151));
    g.drawText ("v" + juce::String (JucePlugin_VersionString), 450, juce::roundToInt (fy) + 12, 100, 16, juce::Justification::centred);

    // FX rack panel
    if (e.rackOpen)
    {
        const juce::Rectangle<float> r (static_cast<float> (kPanelW + 10), 0.0f, static_cast<float> (kRackW - 10), h);
        g.setColour (col::rack);
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
        g.setColour (col::textHi);
        g.setFont (font (15.0f));
        g.drawText ("F X   R A C K", r.withHeight (44.0f).toNearestInt(), juce::Justification::centred);
    }
}

void HaldenReverbAudioProcessorEditor::setSpaceOn (bool on)
{
    if (auto* q = proc.apvts.getParameter ("spaceOn"))
    {
        q->beginChangeGesture();
        q->setValueNotifyingHost (on ? 1.0f : 0.0f);
        q->endChangeGesture();
    }
    spaceButton.setToggleState (on, juce::dontSendNotification);
}

void HaldenReverbAudioProcessorEditor::setSpaceOpen (bool open)
{
    spaceWindow->setVisible (open);
    if (open) spaceWindow->toFront (false);
    drawerTab.setVisible (! open);
}

void HaldenReverbAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const bool spaceLit = proc.apvts.getRawParameterValue ("spaceOn")->load() >= 0.5f;
    if (! spaceLit && spaceWindow->isVisible()) setSpaceOpen (false);   // switched off by automation / reset
    if (spaceButton.getToggleState() != spaceLit) spaceButton.setToggleState (spaceLit, juce::dontSendNotification);
    if (spaceWindow->isVisible()) spaceWindow->refresh();
    const bool send = proc.apvts.getRawParameterValue ("routing")->load() >= 0.5f;
    if (mixKnob->slider.isEnabled() == send) { mixKnob->slider.setEnabled (! send); mixKnob->setAlpha (send ? 0.55f : 1.0f); }
    inMeter.setLevels (proc.inPeak[0].exchange (0.0f), proc.inPeak[1].exchange (0.0f), now);
    outMeter.setLevels (proc.outPeak[0].exchange (0.0f), proc.outPeak[1].exchange (0.0f), now);
    lcd.update (now);
    if (duckKnob != nullptr && drawerOpen)
        duckKnob->repaint();
}
