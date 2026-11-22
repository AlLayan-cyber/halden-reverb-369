#pragma once

#include <JuceHeader.h>
#include "DSP.h"
#include "HaalAi.h"
#include <array>
#include <atomic>

class HaldenReverbAudioProcessor : public juce::AudioProcessor
{
public:
    HaldenReverbAudioProcessor();
    ~HaldenReverbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::AudioProcessorValueTreeState apvts;

    static const juce::StringArray& algorithmNames();

    // ---- Telemetry for the UI (written on the audio thread, read on the message thread)
    std::array<std::atomic<float>, 2> inPeak  {};   // linear peak since last UI read
    std::array<std::atomic<float>, 2> outPeak {};
    std::atomic<float> duckReductionDb { 0.0f };   // positive dB of tail attenuation

    // ---- HaalAi telemetry / control
    static constexpr int kHaalAlgo = 7;
    static constexpr int kStatic = haldendsp::VoiceAnalyzer::kMaxStatic;
    static constexpr int kDyn = haldendsp::VoiceAnalyzer::kDyn;
    std::atomic<bool> haalRequestLearn { false };
    std::atomic<float> haalProgress { 0.0f };       // 0..1 while learning, 1 when a curve exists
    std::atomic<bool> haalLearning { false };
    std::atomic<int> haalNumStatic { 0 };
    std::array<std::atomic<float>, kStatic> haalStaticF {}, haalStaticG {}, haalStaticQ {};
    std::atomic<float> haalShelf { 0.0f };
    std::array<std::atomic<float>, kDyn> haalDynF {}, haalDynG {};   // Hz, dB (<= 0)
    float haalMinPreDelayMs() const { return 32.0f; }

    // Wet signal tap for the LCD spectrum analyser (single producer / single consumer)
    static constexpr int kScopeFifoSize = 16384;
    juce::AbstractFifo scopeFifo { kScopeFifoSize };
    std::array<float, kScopeFifoSize> scopeBuffer {};
    std::atomic<double> scopeSampleRate { 48000.0 };

private:
    void pushToScope (const float* data, int num) noexcept;

    double currentSampleRate = 48000.0;
    float srScale = 1.0f;

    haldendsp::DelayLine preDelayL, preDelayR, earlyDelay;
    haldendsp::AllpassFilter inAp1, inAp2, inAp3, inAp4;
    haldendsp::AllpassFilter modApL, modApR, apL2, apR2;
    haldendsp::DelayLine delayL1, delayR1, delayL2, delayR2;
    haldendsp::BiquadFilter eqHpfL, eqHpfR, eqLpfL, eqLpfR;
    haldendsp::BiquadFilter riserHpL1, riserHpR1, riserHpL2, riserHpR2;
    haldendsp::BiquadFilter monoSideHp1, monoSideHp2;
    haldendsp::BiquadFilter shimHpL, shimHpR, shimLpL, shimLpR;
    haldendsp::PitchShifter shiftL, shiftR, glideL, glideR;
    haldendsp::OnsetDetector onset;
    haldendsp::XfadeLength xSize, xPre;

    haldendsp::VoiceAnalyzer haal;
    haldendsp::BiquadFilter haalHp1, haalHp2, haalShelfF, haalLight;
    std::array<haldendsp::BiquadFilter, kStatic> haalStaticFilt;
    std::array<float, kStatic> haalStaticCur {};
    std::array<haldendsp::BiquadFilter, kDyn> haalDynFilt;
    std::array<float, kDyn> haalDynLogf {}, haalDynGain {};
    float haalShelfCur = 0.0f;
    int haalCounter = 0;
    bool haalWasActive = false;

    // curve restored from a saved project (message thread -> audio thread)
    std::array<haldendsp::CurveBand, kStatic> pendingCurve {};
    int pendingNum = 0;
    float pendingShelf = 0.0f;
    std::atomic<bool> pendingCurveReady { false };
    void publishHaalCurve();
    double samplesSinceOnset = 1.0e9;
    float glideSemis = 0.0f, riserSmooth = 0.0f, glideW = 0.0f;
    float actFast = 0.0f, actPeak = 0.0f, shimEnv = 0.0f, lastWetL = 0.0f, lastWetR = 0.0f;

    // Smoothed controls (per sample)
    haldendsp::Smooth smPreDelay, smSize, smGMid, smGLow, smErWeight, smLateWeight, smModDepth,
                      smMix, smWidth, smInGain, smOutGain, smShim, smFreeze, smRiserWidth, smRiserGain;
    float pumpGain = 1.0f, gateGain = 1.0f;

    std::array<float, 1024> scopeScratch {};
    int scopeScratchCount = 0;

    float tankL = 0.0f, tankR = 0.0f;
    float xoverLpL = 0.0f, xoverLpR = 0.0f;
    float dampLpL = 0.0f, dampLpR = 0.0f;
    float inHpL = 0.0f, inHpR = 0.0f;
    float dryEnv = 0.0f, duckEnv = 0.0f;
    float spinPhase = 0.0f, wanderPhase = 1.1f;
    float randomTarget = 0.0f, randomValue = 0.0f;
    int randomCounter = 0;
    juce::Random rng;

    double freeRunPpq = 0.0;
    int gateOpenSamples = 0;
    bool firstBlock = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HaldenReverbAudioProcessor)
};
