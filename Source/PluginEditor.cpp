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
    if (id == "preDelayMs" || id == "gateRelease") return juce::String (v, v < 10.0f && id == "preDelayMs" ? 1 : 0) + " ms";
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

    if (style == "scale" || style == "text")
        return;

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
    slider.onDragStart = [this] { if (onTouched) onTouched(); };
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
        g.setColour (sel ? (active ? juce::Colour (0xff12305e) : juce::Colour (0xff1a1d23)) : juce::Colour (0xff0b0c0f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (sel ? (active ? col::blueDeep : col::textDim) : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
        g.setColour (sel ? (active ? col::blueBright : col::textMid) : col::textDim);
        auto text = choices[i].replace ("-", juce::String (juce::CharPointer_UTF8 ("\xe2\x88\x92")));
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
        const float eqTouchA = overlayAlpha (eqStart, eqTouch);
        const float eqA = eqOn ? 0.9f : eqTouchA * 0.6f;
        if (eqA > 0.01f)
        {
            const float lo = st.getRawParameterValue ("eqLowCutHz")->load();
            const float hi = st.getRawParameterValue ("eqHighCutHz")->load();
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

    // decay envelope overlay: fades in once, stays while the knob moves, fades after release
    const float envA = overlayAlpha (envStart, envTouch);
    if (envA > 0.01f)
    {
        g.setColour (col::lcdBg.withAlpha (0.72f * envA));
        g.fillRect (a.expanded (2.0f));

        const float decay = st.getRawParameterValue ("decay")->load();
        const float pre = st.getRawParameterValue ("preDelayMs")->load();
        const float shape = st.getRawParameterValue ("shape")->load();
        const float size = st.getRawParameterValue ("size")->load();
        const float maxSec = std::max (2.0f, decay * 1.25f + pre * 0.001f);
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

        g.setFont (font (13.0f));
        g.drawText ("DECAY " + formatValue ("decay", decay) + "   PRE " + formatValue ("preDelayMs", pre),
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
    if (on ("gateOn"))   fx.add ("GATE");
    if (! fx.isEmpty())
    {
        g.setColour (col::blueDeep);
        g.setFont (font (12.0f));
        g.drawText ("FX  " + fx.joinIntoString (juce::String (juce::CharPointer_UTF8 (" \xc2\xb7 "))),
                    juce::Rectangle<float> (b.getRight() - 420.0f, 10.0f, 404.0f, 20.0f), juce::Justification::centredRight);
    }
}

float Lcd::overlayAlpha (double start, double touch) const
{
    const double sinceStart = now - start, sinceTouch = now - touch;
    if (sinceTouch < 0.0 || sinceStart < 0.0) return 0.0f;
    const float fadeIn = static_cast<float> (juce::jlimit (0.0, 1.0, sinceStart / 0.15));
    const float fadeOut = sinceTouch < 1.4 ? 1.0f : std::max (0.0f, 1.0f - static_cast<float> ((sinceTouch - 1.4) / 0.4));
    return std::min (fadeIn, fadeOut);
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
    const auto curveDb = [&] (float f)
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
        const float y = refY - curveDb (f) * pxPerDb;
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
        const float y = refY - (curveDb (f) - gr) * pxPerDb;
        const float active = juce::jlimit (0.0f, 1.0f, gr / 2.0f);
        const auto ring = col::iceBlue.interpolatedWith (col::amber, active);

        if (active > 0.05f)
        {
            g.setColour (ring.withAlpha (0.25f * active));
            g.fillEllipse (x - 15.0f, y - 15.0f, 30.0f, 30.0f);
        }
        g.setColour (col::lcdBg);
        g.fillEllipse (x - 11.0f, y - 11.0f, 22.0f, 22.0f);
        g.setColour (ring);
        g.drawEllipse (x - 11.0f, y - 11.0f, 22.0f, 22.0f, 1.6f);
        g.setFont (font (10.5f, true));
        g.drawText ("D" + juce::String (b + 1), juce::Rectangle<float> (x - 11.0f, y - 11.0f, 22.0f, 22.0f), juce::Justification::centred);

        // activity meter: grows with the cut (full = 2 dB)
        const juce::Rectangle<float> meter (x - 3.0f, y + 15.0f, 6.0f, 22.0f);
        g.setColour (juce::Colour (0xff0f2340));
        g.fillRoundedRectangle (meter, 2.0f);
        g.setColour (ring);
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
        content.addAndMakeVisible (*k);
        mainKnobs.push_back (std::move (k));
    }

    for (const auto& d : kAdv)
    {
        auto k = std::make_unique<hui::Knob> (p.apvts, d.id, d.label, 22.0f, false);
        const juce::String id (d.id);
        if (id == "shape")
            k->onTouched = [this] { lcd.showEnvelope (juce::Time::getMillisecondCounterHiRes() * 0.001); };
        if (id == "eqLowCutHz" || id == "eqHighCutHz")
            k->onTouched = [this] { lcd.showEq (juce::Time::getMillisecondCounterHiRes() * 0.001); };
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
        auto& gate = add ("gateOn", "GATE");          gate.addChoices ("gateLen");   gate.addKnob ("gateRelease", "RELEASE");
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

void HaldenReverbAudioProcessorEditor::timerCallback()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    inMeter.setLevels (proc.inPeak[0].exchange (0.0f), proc.inPeak[1].exchange (0.0f), now);
    outMeter.setLevels (proc.outPeak[0].exchange (0.0f), proc.outPeak[1].exchange (0.0f), now);
    lcd.update (now);
    if (duckKnob != nullptr && drawerOpen)
        duckKnob->repaint();
}
