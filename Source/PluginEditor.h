#pragma once

#include "PluginProcessor.h"
#include <array>
#include <functional>
#include <memory>
#include <vector>

namespace hui
{
// ------------------------------------------------------------------ palette
namespace col
{
    const juce::Colour panel      { 0xff0a0b0d };
    const juce::Colour drawer     { 0xff0f1115 };
    const juce::Colour rack       { 0xff0d0f13 };
    const juce::Colour line       { 0xff1f2937 };
    const juce::Colour lineDim    { 0xff16191f };
    const juce::Colour blue       { 0xff60a5fa };
    const juce::Colour blueBright { 0xff93c5fd };
    const juce::Colour blueDeep   { 0xff3b82f6 };
    const juce::Colour blueDark   { 0xff1e3a5f };
    const juce::Colour lcdBg      { 0xff06101f };
    const juce::Colour textHi     { 0xffe5e7eb };
    const juce::Colour textMid    { 0xff9ca3af };
    const juce::Colour textLo     { 0xff6b7280 };
    const juce::Colour textDim    { 0xff4b5563 };
    const juce::Colour iceBlue    { 0xff7dd3fc };
    const juce::Colour amber      { 0xfffacc15 };
    const juce::Colour red        { 0xffef4444 };
}

juce::String formatValue (const juce::String& paramId, float v);

class LookAndFeel : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override {}
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float sliderPos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius (juce::Slider&) override { return 11; }
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// ------------------------------------------------------------------ knob
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, float radius, bool big);

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::Slider slider;
    std::function<juce::String()> valueOverride;      // optional live readout (ducking)
    std::function<float()> ringReductionDb;            // optional glow ring (ducking)
    std::function<void()> onTouched;

private:
    juce::String paramId, label;
    float radius;
    bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ------------------------------------------------------------------ fader
class Fader : public juce::Component
{
public:
    Fader (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;

private:
    juce::String paramId;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ------------------------------------------------------------------ LED meter
class LedMeter : public juce::Component
{
public:
    static constexpr int kSegments = 22;
    void setLevels (float leftLin, float rightLin, double nowSec);
    void paint (juce::Graphics&) override;

private:
    struct Channel { float db = -100.0f; float peakDb = -100.0f; double peakTime = 0.0; };
    std::array<Channel, 2> ch;
    double lastTime = 0.0;
};

// ------------------------------------------------------------------ choice row
class ChoiceRow : public juce::Component
{
public:
    ChoiceRow (juce::AudioProcessorValueTreeState&, const juce::String& paramId);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<bool()> isActive;
    bool violet = false;   // IN SPACE styling

private:
    juce::RangedAudioParameter* param = nullptr;
    juce::StringArray choices;
    int current = 0;
    std::unique_ptr<juce::ParameterAttachment> attachment;
};

// ------------------------------------------------------------------ LCD
class Lcd : public juce::Component
{
public:
    explicit Lcd (HaldenReverbAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void update (double nowSec);                   // pulls audio and advances the analyser
    // keeps the overlay up for as long as the knob keeps moving (no restart of the fade-in)
    // the picture is taken when the overlay appears and re-taken 1 s after the knob stops,
    // so it never jitters while a knob is moving
    void showEnvelope (double nowSec) { if (nowSec - envTouch > 2.8) { envStart = nowSec; snapEnvelope(); } envTouch = nowSec; envSnapped = false; }
    void showEq (double nowSec) { if (nowSec - eqTouch > 2.8) { eqStart = nowSec; snapEq(); } eqTouch = nowSec; eqSnapped = false; }

private:
    HaldenReverbAudioProcessor& proc;
    static constexpr int kFftOrder = 12;
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kPoints = 160;
    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window { kFftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> (kFftSize, 0.0f);
    int ringPos = 0;
    std::vector<float> fftData = std::vector<float> (kFftSize * 2, 0.0f);
    std::array<float, kPoints> curveDb {}, peakDb {};
    std::array<double, kPoints> peakTime {};
    double envTouch = -100.0, eqTouch = -100.0, envStart = -100.0, eqStart = -100.0, now = 0.0, lastUpdate = 0.0;
    float overlayAlpha (double start, double touch) const;
    void snapEnvelope();
    void snapEq();
    float snapDecay = 3.0f, snapPre = 20.0f, snapShape = 0.4f, snapSize = 0.85f, snapLo = 500.0f, snapHi = 6000.0f;
    bool envSnapped = true, eqSnapped = true;
    void paintHaalAi (juce::Graphics&, juce::Rectangle<float> area);

    float xForFreq (float f) const;
    float yForDb (float db) const;
    juce::Rectangle<float> plotArea() const;
};

// ------------------------------------------------------------------ IN SPACE entry button
// one click: on + open / close the window; double click: IN SPACE off
class SpaceEntryButton : public juce::TextButton, private juce::Timer
{
public:
    SpaceEntryButton() : juce::TextButton ("IN SPACE") {}
    std::function<void()> onSingle, onDouble;
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! isEnabled() || ! getLocalBounds().contains (e.getPosition())) { juce::TextButton::mouseUp (e); return; }
        if (e.getNumberOfClicks() >= 2) { stopTimer(); if (onDouble) onDouble(); }
        else startTimer (juce::MouseEvent::getDoubleClickTimeout() + 20);
        juce::TextButton::mouseUp (e);
    }
private:
    void timerCallback() override { stopTimer(); if (onSingle) onSingle(); }
    void clicked() override {}
};

// ------------------------------------------------------------------ IN SPACE glass knob
class GlassKnob : public juce::Component
{
public:
    GlassKnob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& label, float radius, bool big);
    void paint (juce::Graphics&) override;
    void resized() override;
    juce::Slider slider;
    std::function<float()> liveValue;   // optional 0..1 shown instead of the knob (LAUNCH ramp)
    juce::String lowHint, highHint;

private:
    juce::String paramId, label;
    float radius;
    bool big;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// ------------------------------------------------------------------ IN SPACE window
class SpaceWindow : public juce::Component
{
public:
    explicit SpaceWindow (HaldenReverbAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void refresh();                         // called from the editor timer
    std::function<void()> onClose;

private:
    HaldenReverbAudioProcessor& proc;
    GlassKnob orbit, size, decay;
    ChoiceRow launch, routing;
    juce::TextButton go, stardust, close;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> stardustAttachment;
    float lastProgress = -1.0f, lastOrbit = -1.0f;
};

// ------------------------------------------------------------------ FX module
class FxModule : public juce::Component
{
public:
    FxModule (juce::AudioProcessorValueTreeState&, const juce::String& onParamId, const juce::String& title);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    void addKnob (const juce::String& paramId, const juce::String& label);
    void addChoices (const juce::String& paramId);
    void addToggle (const juce::String& paramId, const juce::String& text);
    void setHint (const juce::String& h) { hint = h; }
    bool isOn() const;

private:
    juce::AudioProcessorValueTreeState& state;
    juce::String onId, title, hint;
    std::unique_ptr<juce::ParameterAttachment> onAttachment;
    std::unique_ptr<Knob> knob;
    std::unique_ptr<ChoiceRow> choices;
    std::unique_ptr<juce::TextButton> toggle;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> toggleAttachment;
};
} // namespace hui

// ------------------------------------------------------------------ editor
class HaldenReverbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                         private juce::Timer
{
public:
    explicit HaldenReverbAudioProcessorEditor (HaldenReverbAudioProcessor&);
    ~HaldenReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int kPanelW = 1000, kRackW = 290, kClosedH = 610, kOpenH = 830;
    float getEffectiveScale() const { return effScale; }

private:
    class Content : public juce::Component
    {
    public:
        explicit Content (HaldenReverbAudioProcessorEditor& e) : owner (e) {}
        void paint (juce::Graphics&) override;
        HaldenReverbAudioProcessorEditor& owner;
    };

    void timerCallback() override;
    void layout();
    void applyWindowSize();
    void setUiProp (const juce::Identifier& id, const juce::var& v);

    HaldenReverbAudioProcessor& proc;
    hui::LookAndFeel lnf;
    Content content { *this };

    std::vector<std::unique_ptr<juce::TextButton>> algoButtons;
    std::unique_ptr<juce::ParameterAttachment> algoAttachment;
    int currentAlgo = 0;

    hui::Fader inFader, outFader;
    hui::LedMeter inMeter, outMeter;
    hui::Lcd lcd;

    std::vector<std::unique_ptr<hui::Knob>> mainKnobs, advKnobs;
    juce::TextButton drawerTab, rackTab, eqButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> eqAttachment;
    std::array<juce::TextButton, 4> scaleButtons;
    std::vector<std::unique_ptr<hui::FxModule>> fxModules;
    hui::Knob* duckKnob = nullptr;

    bool drawerOpen = true, rackOpen = false;
    float uiScale = 1.0f, effScale = 1.0f;
    bool applyingSize = false;
    juce::TextButton learnButton, resetButton;
    hui::SpaceEntryButton spaceButton;
    void setSpaceOn (bool on);
    std::unique_ptr<hui::ChoiceRow> routingRow;
    std::unique_ptr<hui::SpaceWindow> spaceWindow;
    hui::Knob* mixKnob = nullptr;
    void setSpaceOpen (bool open);
    float maxScaleForScreen (int w, int h) const;

    friend class Content;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaldenReverbAudioProcessorEditor)
};
