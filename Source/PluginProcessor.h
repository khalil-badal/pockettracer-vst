#pragma once
#include <JuceHeader.h>
#include "dsp/DelayEngine.h"
#include "dsp/TiltFilter.h"
#include "dsp/TempoSync.h"

class PocketTracerAudioProcessor : public juce::AudioProcessor
{
public:
    PocketTracerAudioProcessor();
    ~PocketTracerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return DelayEngine::kMaxDelaySeconds; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    DelayEngine delayEngine;
    TiltFilter  toneFilter;

    juce::SmoothedValue<float> smoothMix;
    juce::SmoothedValue<float> smoothInGain, smoothOutGain;

    // cached raw parameter pointers
    std::atomic<float>* pInGain     = nullptr;
    std::atomic<float>* pOutGain    = nullptr;
    std::atomic<float>* pType       = nullptr;
    std::atomic<float>* pSync       = nullptr;
    std::atomic<float>* pTime       = nullptr;
    std::atomic<float>* pDivision   = nullptr;
    std::atomic<float>* pFeedback   = nullptr;
    std::atomic<float>* pMix        = nullptr;
    std::atomic<float>* pTone       = nullptr;
    std::atomic<float>* pModulation = nullptr;
    std::atomic<float>* pPingPong   = nullptr;
    std::atomic<float>* pFreeze     = nullptr;

    double currentBpm = TempoSync::kFallbackBpm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PocketTracerAudioProcessor)
};
