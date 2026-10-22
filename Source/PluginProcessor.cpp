#include "PluginProcessor.h"
#include "PluginEditor.h"

PocketTracerAudioProcessor::PocketTracerAudioProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    pInGain     = apvts.getRawParameterValue ("inputGain");
    pOutGain    = apvts.getRawParameterValue ("outputGain");
    pType       = apvts.getRawParameterValue ("delayType");
    pSync       = apvts.getRawParameterValue ("sync");
    pTime       = apvts.getRawParameterValue ("time");
    pDivision   = apvts.getRawParameterValue ("division");
    pFeedback   = apvts.getRawParameterValue ("feedback");
    pMix        = apvts.getRawParameterValue ("mix");
    pTone       = apvts.getRawParameterValue ("tone");
    pModulation = apvts.getRawParameterValue ("modulation");
    pPingPong   = apvts.getRawParameterValue ("pingpong");
    pFreeze     = apvts.getRawParameterValue ("freeze");
}

juce::AudioProcessorValueTreeState::ParameterLayout
PocketTracerAudioProcessor::createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "inputGain", 1 }, "Input",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "outputGain", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("dB")));

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "delayType", 1 }, "Delay Type",
        StringArray { "Digital", "Analog", "Tape", "Reverse", "Lo-Fi", "Multi-Tap" }, 0));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "sync", 1 }, "Sync", false));

    // Free-running time: 1..2000 ms, skewed for finer control at short times.
    {
        NormalisableRange<float> timeRange (1.0f, 2000.0f);
        timeRange.setSkewForCentre (200.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { "time", 1 }, "Time", timeRange, 350.0f,
            AudioParameterFloatAttributes().withLabel ("ms")));
    }

    layout.add (std::make_unique<AudioParameterChoice> (
        ParameterID { "division", 1 }, "Division",
        TempoSync::getDivisionNames(), TempoSync::getDefaultDivisionIndex()));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "feedback", 1 }, "Feedback",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "mix", 1 }, "Mix",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 35.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "tone", 1 }, "Tone",
        NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "modulation", 1 }, "Modulation",
        NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
        AudioParameterFloatAttributes().withLabel ("%")));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "pingpong", 1 }, "Ping-Pong", false));

    layout.add (std::make_unique<AudioParameterBool> (
        ParameterID { "freeze", 1 }, "Freeze", false));

    return layout;
}

void PocketTracerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels      = (juce::uint32) juce::jmax (1, getTotalNumOutputChannels());

    delayEngine.prepare (spec);
    toneFilter.prepare (spec);

    smoothMix.reset (sampleRate, 0.02);
    smoothMix.setCurrentAndTargetValue (pMix ? pMix->load() / 100.0f : 0.35f);

    smoothInGain.reset (sampleRate, 0.02);
    smoothOutGain.reset (sampleRate, 0.02);
    smoothInGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain ? pInGain->load() : 0.0f));
    smoothOutGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain ? pOutGain->load() : 0.0f));
}

bool PocketTracerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PocketTracerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn  = getTotalNumInputChannels();
    const int totalOut = getTotalNumOutputChannels();
    for (int ch = totalIn; ch < totalOut; ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    // --- host tempo (fallback 120 for standalone / no playhead) ---
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (auto bpm = pos->getBpm())
                if (*bpm > 0.0)
                    currentBpm = *bpm;

    // --- resolve parameters into the engine ---
    delayEngine.setType ((int) pType->load());

    const bool synced = pSync->load() >= 0.5f;
    const float timeMs = synced
        ? (float) TempoSync::divisionToMs ((int) pDivision->load(), currentBpm)
        : pTime->load();
    delayEngine.setTimeMs (timeMs);

    delayEngine.setFeedback   (pFeedback->load() / 100.0f);
    delayEngine.setModulation (pModulation->load() / 100.0f);
    delayEngine.setPingPong   (pPingPong->load() >= 0.5f);
    delayEngine.setFreeze     (pFreeze->load()   >= 0.5f);

    // --- input gain (affects the whole signal entering the effect) ---
    smoothInGain.setTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    {
        const int nCh = buffer.getNumChannels();
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            const float g = smoothInGain.getNextValue();
            for (int ch = 0; ch < nCh; ++ch)
                buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
        }
    }

    // keep a dry copy for the equal-power mix
    juce::AudioBuffer<float> dry;
    dry.makeCopyOf (buffer);

    // wet (repeats only)
    delayEngine.process (buffer);

    // Tone tilt on the wet/repeats signal only
    toneFilter.setTilt (pTone->load());
    toneFilter.process (buffer);

    // equal-power dry/wet crossfade
    smoothMix.setTargetValue (pMix->load() / 100.0f);
    const int numCh = buffer.getNumChannels();
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float m   = smoothMix.getNextValue();
        const float theta = m * juce::MathConstants<float>::halfPi;
        const float dryG = std::cos (theta);
        const float wetG = std::sin (theta);

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float wet = buffer.getSample (ch, i);
            const float dryS = dry.getSample (ch, i);
            buffer.setSample (ch, i, dryS * dryG + wet * wetG);
        }
    }

    // --- output gain ---
    smoothOutGain.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float g = smoothOutGain.getNextValue();
        for (int ch = 0; ch < numCh; ++ch)
            buffer.setSample (ch, i, buffer.getSample (ch, i) * g);
    }
}

void PocketTracerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); state.isValid())
        if (auto xml = state.createXml())
            copyXmlToBinary (*xml, destData);
}

void PocketTracerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* PocketTracerAudioProcessor::createEditor()
{
    return new PocketTracerAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PocketTracerAudioProcessor();
}
