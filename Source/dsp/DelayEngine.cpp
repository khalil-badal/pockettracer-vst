#include "DelayEngine.h"

namespace
{
    constexpr double kTwoPi = juce::MathConstants<double>::twoPi;

    // ---- modulation tuning (depths in ms, freqs in Hz) ----
    constexpr double kWowFreq        = 0.6;   // tape wow
    constexpr double kWowDepthMs     = 7.0;
    constexpr double kFlutterFreq    = 7.0;   // tape flutter
    constexpr double kFlutterDepthMs = 1.2;
    constexpr double kDriftFreq      = 0.15;  // analog slow drift
    constexpr double kDriftDepthMs   = 3.5;
    constexpr double kWidthFreq      = 0.3;   // digital stereo width
    constexpr double kWidthDepthMs   = 0.6;
    constexpr double kPanFreq        = 0.25;  // multi-tap pan wobble
    constexpr double kWarbleFreq     = 5.0;   // lo-fi warble

    // ---- per-type voicing constants ----
    constexpr double kAnalogCutoffHz = 3500.0; // in-loop darkening
    constexpr double kDigitalDampHz  = 8000.0; // subtle per-repeat softening (lead-friendly)
    constexpr float  kAnalogDrive    = 1.2f;
    constexpr float  kAnalogGritAmt  = 0.006f;
    constexpr double kGritLPHz       = 4000.0;

    constexpr double kTapeCutoffHz   = 6500.0; // gentler HF rolloff
    constexpr float  kTapeDrive      = 1.3f;
    constexpr float  kTapeComp       = 0.95f;

    constexpr int    kLofiBaseHold   = 4;      // SR reduction: fs/4
    constexpr int    kLofiWarbleAmt  = 2;
    constexpr float  kLofiLevels     = 128.0f; // ~7-bit
    constexpr double kLofiHPHz       = 300.0;
    constexpr double kLofiLPHz       = 3200.0;

    // multi-tap relative tap lengths (of base = 1/4), pans and gains
    constexpr double kTapQuarter   = 1.0;   constexpr float kPanQuarter   = 0.0f;  constexpr float kGainQuarter   = 1.0f;
    constexpr double kTapEighthDot = 0.75;  constexpr float kPanEighthDot = -0.7f; constexpr float kGainEighthDot = 0.8f;
    constexpr double kTapSixteenth = 0.25;  constexpr float kPanSixteenth = 0.7f;  constexpr float kGainSixteenth = 0.7f;
}

// =============================================================================
void DelayEngine::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate      = spec.sampleRate;
    maxDelaySamples = (int) std::ceil (kMaxDelaySeconds * sampleRate) + 4;

    const int numCh = (int) juce::jmax (1u, spec.numChannels);
    delayBuffer.setSize (numCh, maxDelaySamples, false, true, true);
    delayBuffer.clear();

    smoothTimeMs  .reset (sampleRate, 0.05);
    smoothFeedback.reset (sampleRate, 0.02);
    smoothMod     .reset (sampleRate, 0.02);

    for (int ch = 0; ch < 2; ++ch)
    {
        analogLP[ch].setCutoff (kAnalogCutoffHz, sampleRate);
        digitalLP[ch].setCutoff (kDigitalDampHz, sampleRate);
        analogGritLP[ch].setCutoff (kGritLPHz, sampleRate);
        tapeLP[ch].setCutoff (kTapeCutoffHz, sampleRate);
        tapeSat[ch].prepare (sampleRate);
        lofiLP[ch].setCutoff (kLofiLPHz, sampleRate);
        lofiHP[ch].setCutoff (kLofiHPHz, sampleRate);
    }

    reset();
}

void DelayEngine::reset()
{
    delayBuffer.clear();
    writePos = 0;
    writeCounter = 0;
    for (auto& d : dcBlock) d.reset();
    freezeGain = freezeTarget ? 1.0f : 0.0f;
    wowPhase = flutterPhase = widthPhase = driftPhase = panPhase = warblePhase = 0.0;
    revInit = false;
    resetColorStates();
}

void DelayEngine::resetColorStates()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        analogLP[ch].reset();  analogGritLP[ch].reset();  digitalLP[ch].reset();
        tapeLP[ch].reset();    tapeSat[ch].reset();
        lofiLP[ch].reset();    lofiHP[ch].reset();
        lofiHold[ch] = 0.0f;   lofiHoldCnt[ch] = 0;
    }
}

void DelayEngine::setType (int typeIndex)
{
    auto t = (Type) juce::jlimit (0, (int) Type::MultiTap, typeIndex);
    if (t != currentType)
    {
        currentType = t;
        resetColorStates();   // avoid stale filter state when switching character
        revInit = false;
    }
}

void DelayEngine::setTimeMs (float timeMs)
{
    smoothTimeMs.setTargetValue (juce::jlimit (1.0f, (float) (kMaxDelaySeconds * 1000.0), timeMs));
}

void DelayEngine::setFeedback (float fb01)
{
    // Linear knob feel with max loop gain 0.85, so 25-35% gives a few clean
    // decaying repeats (lead-friendly) and knob max is long but never infinite.
    const float fb = juce::jlimit (0.0f, 1.0f, fb01);
    smoothFeedback.setTargetValue (fb * 0.85f);
}
void DelayEngine::setModulation (float mod01){ smoothMod.setTargetValue (juce::jlimit (0.0f, 1.0f, mod01)); }
void DelayEngine::setPingPong (bool b)       { pingPong = b; }

void DelayEngine::setFreeze (bool shouldFreeze)
{
    if (shouldFreeze != freezeTarget)
    {
        freezeTarget = shouldFreeze;
        freezeRamp = 1.0f / (float) juce::jmax (1.0, 0.030 * sampleRate); // ~30ms
    }
}

// ---------------------------------------------------------------- reads
float DelayEngine::readHermite (int channel, double delaySamples) const
{
    double readPos = (double) writePos - delaySamples;
    while (readPos < 0.0) readPos += (double) maxDelaySamples;

    const int i1 = (int) readPos;
    const float frac = (float) (readPos - (double) i1);
    const int i0 = (i1 - 1 + maxDelaySamples) % maxDelaySamples;
    const int i2 = (i1 + 1) % maxDelaySamples;
    const int i3 = (i1 + 2) % maxDelaySamples;

    const auto* d = delayBuffer.getReadPointer (channel);
    const float x0 = d[i0], x1 = d[i1], x2 = d[i2], x3 = d[i3];

    // 4-point, 3rd-order Hermite
    const float c0 = x1;
    const float c1 = 0.5f * (x2 - x0);
    const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
    const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

float DelayEngine::linearReadAbs (int channel, double absTime) const
{
    long long i0abs = (long long) std::floor (absTime);
    float frac = (float) (absTime - (double) i0abs);
    int idx0 = (int) (((i0abs % maxDelaySamples) + maxDelaySamples) % maxDelaySamples);
    int idx1 = (idx0 + 1) % maxDelaySamples;
    const auto* d = delayBuffer.getReadPointer (channel);
    return d[idx0] + frac * (d[idx1] - d[idx0]);
}

double DelayEngine::applyFeedbackSafety (int channel, double sample)
{
    const double dc   = dcBlock[channel].process (sample);
    const double soft = std::tanh (dc);            // bounds loop energy
    constexpr double ceiling = 0.98;
    return juce::jlimit (-ceiling, ceiling, soft);  // hard safety guarantee
}

void DelayEngine::panGains (float pan, float& l, float& r)
{
    const float a = (juce::jlimit (-1.0f, 1.0f, pan) * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
    l = std::cos (a);
    r = std::sin (a);
}

// ---------------------------------------------------------------- modulation
double DelayEngine::readModOffset (Type t, int ch, float mod) const
{
    switch (t)
    {
        case Type::Digital:
        {
            double d = mod * kWidthDepthMs * 0.001 * sampleRate * std::sin (kTwoPi * widthPhase);
            return ch == 0 ? d : -d;
        }
        case Type::Analog:
            return mod * kDriftDepthMs * 0.001 * sampleRate * std::sin (kTwoPi * driftPhase);
        case Type::Tape:
            return mod * 0.001 * sampleRate * (kWowDepthMs     * std::sin (kTwoPi * wowPhase)
                                             + kFlutterDepthMs * std::sin (kTwoPi * flutterPhase));
        default:
            return 0.0;
    }
}

// ---------------------------------------------------------------- coloration
float DelayEngine::colorize (Type t, int ch, float delayed, float mod)
{
    switch (t)
    {
        case Type::Analog:
        {
            float c = analogLP[ch].tick (delayed);          // cumulative darkening
            c = std::tanh (c * kAnalogDrive);               // soft compression / BBD companding
            float noise = analogGritLP[ch].tick (rng.nextFloat() * 2.0f - 1.0f);
            float env = juce::jmin (1.0f, std::abs (c) * 4.0f);
            c += noise * kAnalogGritAmt * env;              // signal-gated grit
            return c;
        }
        case Type::Tape:
        {
            float c = tapeSat[ch].process (delayed, [] (float v) { return std::tanh (v * kTapeDrive); });
            c = tapeLP[ch].tick (c);                        // HF rolloff
            return c * kTapeComp;
        }
        case Type::LoFi:
        {
            int holdLen = kLofiBaseHold
                        + (int) std::lround (mod * kLofiWarbleAmt * std::sin (kTwoPi * warblePhase));
            holdLen = juce::jmax (1, holdLen);
            if (--lofiHoldCnt[ch] <= 0) { lofiHold[ch] = delayed; lofiHoldCnt[ch] = holdLen; }
            float c = lofiHold[ch];                         // sample-rate reduction
            c = std::round (c * kLofiLevels) / kLofiLevels; // bitcrush (compounds per repeat)
            c = lofiHP[ch].tick (c);
            c = lofiLP[ch].tick (c);                        // harsher band-limiting
            return c;
        }
        case Type::Digital:
        default:
            // Near-pristine, but a gentle 8 kHz high-cut per pass keeps stacked
            // repeats from competing with the dry signal (NDSP-style behavior).
            return digitalLP[ch].tick (delayed);
    }
}

// =============================================================================
void DelayEngine::process (juce::AudioBuffer<float>& buffer)
{
    switch (currentType)
    {
        case Type::Reverse:  processReverse (buffer);  break;
        case Type::MultiTap: processMultiTap (buffer); break;
        default:             processStandard (buffer); break; // Digital/Analog/Tape/LoFi
    }
}

// ---- Digital / Analog / Tape / Lo-Fi : single tap, coloration in feedback ----
void DelayEngine::processStandard (juce::AudioBuffer<float>& buffer)
{
    const int numCh      = juce::jmin (buffer.getNumChannels(), delayBuffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    const bool stereo    = numCh >= 2;
    const Type t         = currentType;

    for (int i = 0; i < numSamples; ++i)
    {
        writePos = (int) (writeCounter % maxDelaySamples);

        const double baseDelay = juce::jlimit (2.0, (double) (maxDelaySamples - 3),
                                               (double) smoothTimeMs.getNextValue() * 0.001 * sampleRate);
        const float fb  = smoothFeedback.getNextValue();
        const float mod = smoothMod.getNextValue();

        // advance LFO phases
        wowPhase     += kWowFreq     / sampleRate; if (wowPhase     >= 1.0) wowPhase     -= 1.0;
        flutterPhase += kFlutterFreq / sampleRate; if (flutterPhase >= 1.0) flutterPhase -= 1.0;
        driftPhase   += kDriftFreq   / sampleRate; if (driftPhase   >= 1.0) driftPhase   -= 1.0;
        widthPhase   += kWidthFreq   / sampleRate; if (widthPhase   >= 1.0) widthPhase   -= 1.0;
        warblePhase  += kWarbleFreq  / sampleRate; if (warblePhase  >= 1.0) warblePhase  -= 1.0;

        // freeze crossfade
        const float fTarget = freezeTarget ? 1.0f : 0.0f;
        if (freezeGain < fTarget) freezeGain = juce::jmin (fTarget, freezeGain + freezeRamp);
        else                      freezeGain = juce::jmax (fTarget, freezeGain - freezeRamp);

        float delayed[2] = { 0, 0 }, colored[2] = { 0, 0 };
        for (int ch = 0; ch < numCh; ++ch)
        {
            double dly = juce::jlimit (2.0, (double) (maxDelaySamples - 3),
                                       baseDelay + readModOffset (t, ch, mod));
            delayed[ch] = readHermite (ch, dly);
            colored[ch] = colorize (t, ch, delayed[ch], mod);
        }

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float in  = buffer.getSample (ch, i);
            const float fbSource = (pingPong && stereo) ? colored[1 - ch] : colored[ch];

            const double safe        = applyFeedbackSafety (ch, (double) fbSource * (double) fb);
            const double liveWrite   = (double) in + safe;
            const double frozenWrite = (double) delayed[ch];     // pure loop (fb = 1, no decay)
            const double writeVal    = liveWrite * (1.0 - (double) freezeGain)
                                     + frozenWrite * (double) freezeGain;

            delayBuffer.setSample (ch, writePos, (float) writeVal);
            buffer.setSample (ch, i, colored[ch]);               // wet = repeats only
        }

        ++writeCounter;
    }
}

// ---- Reverse : two raised-cosine grains read backwards ----
void DelayEngine::processReverse (juce::AudioBuffer<float>& buffer)
{
    const int numCh      = juce::jmin (buffer.getNumChannels(), delayBuffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();

    for (int i = 0; i < numSamples; ++i)
    {
        writePos = (int) (writeCounter % maxDelaySamples);

        const double L = juce::jlimit (8.0, (double) (maxDelaySamples - 3),
                                       (double) smoothTimeMs.getNextValue() * 0.001 * sampleRate);
        const float fb  = smoothFeedback.getNextValue();
        smoothMod.getNextValue();

        const float fTarget = freezeTarget ? 1.0f : 0.0f;
        if (freezeGain < fTarget) freezeGain = juce::jmin (fTarget, freezeGain + freezeRamp);
        else                      freezeGain = juce::jmax (fTarget, freezeGain - freezeRamp);

        if (! revInit)
        {
            revPhaseA = 0.0;       revAnchorA = writeCounter;
            revPhaseB = L * 0.5;   revAnchorB = writeCounter;
            revInit = true;
        }

        // re-latch grains when they reach the end of their window
        if (revPhaseA >= L) { revPhaseA -= L; revAnchorA = writeCounter; }
        if (revPhaseB >= L) { revPhaseB -= L; revAnchorB = writeCounter; }

        const float winA = std::sin (juce::MathConstants<float>::pi * (float) (revPhaseA / L));
        const float winB = std::sin (juce::MathConstants<float>::pi * (float) (revPhaseB / L));
        const float gA = winA * winA, gB = winB * winB; // sin^2 -> constant power sum

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float in = buffer.getSample (ch, i);
            const float sA = linearReadAbs (ch, (double) revAnchorA - 1.0 - revPhaseA);
            const float sB = linearReadAbs (ch, (double) revAnchorB - 1.0 - revPhaseB);
            const float revOut = sA * gA + sB * gB;

            const double safe        = applyFeedbackSafety (ch, (double) revOut * (double) fb);
            const double liveWrite   = (double) in + safe;
            const double frozenWrite = (double) revOut;          // loop the reversed pattern
            const double writeVal    = liveWrite * (1.0 - (double) freezeGain)
                                     + frozenWrite * (double) freezeGain;

            delayBuffer.setSample (ch, writePos, (float) writeVal);
            buffer.setSample (ch, i, revOut);
        }

        revPhaseA += 1.0;
        revPhaseB += 1.0;
        ++writeCounter;
    }
}

// ---- Multi-Tap : 1/4 (C) + 1/8 dotted (L) + 1/16 (R), mono feedback line ----
void DelayEngine::processMultiTap (juce::AudioBuffer<float>& buffer)
{
    const int numCh      = juce::jmin (buffer.getNumChannels(), delayBuffer.getNumChannels());
    const int numSamples = buffer.getNumSamples();
    const bool stereo    = numCh >= 2;

    for (int i = 0; i < numSamples; ++i)
    {
        writePos = (int) (writeCounter % maxDelaySamples);

        const double baseDelay = juce::jlimit (4.0, (double) (maxDelaySamples - 3),
                                               (double) smoothTimeMs.getNextValue() * 0.001 * sampleRate);
        const float fb  = smoothFeedback.getNextValue();
        const float mod = smoothMod.getNextValue();
        panPhase += kPanFreq / sampleRate; if (panPhase >= 1.0) panPhase -= 1.0;

        const float fTarget = freezeTarget ? 1.0f : 0.0f;
        if (freezeGain < fTarget) freezeGain = juce::jmin (fTarget, freezeGain + freezeRamp);
        else                      freezeGain = juce::jmax (fTarget, freezeGain - freezeRamp);

        const float inMono = stereo ? 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i))
                                    : buffer.getSample (0, i);

        // read three taps from the (mono) line on channel 0
        const float t1 = readHermite (0, baseDelay * kTapQuarter)   * kGainQuarter;
        const float t2 = readHermite (0, baseDelay * kTapEighthDot) * kGainEighthDot;
        const float t3 = readHermite (0, baseDelay * kTapSixteenth) * kGainSixteenth;

        const float sum = t1 + t2 + t3; // mono feedback

        // pan wobble scaled by modulation
        const float wob = mod * 0.25f * (float) std::sin (kTwoPi * panPhase);
        float l1, r1, l2, r2, l3, r3;
        panGains (kPanQuarter,            l1, r1);
        panGains (kPanEighthDot + wob,    l2, r2);
        panGains (kPanSixteenth - wob,    l3, r3);

        const float outL = t1 * l1 + t2 * l2 + t3 * l3;
        const float outR = t1 * r1 + t2 * r2 + t3 * r3;

        const double safe        = applyFeedbackSafety (0, (double) sum * (double) fb);
        const double liveWrite   = (double) inMono + safe;
        const double frozenWrite = (double) sum;
        const double writeVal    = liveWrite * (1.0 - (double) freezeGain)
                                 + frozenWrite * (double) freezeGain;

        // write the mono feedback line to all channels so the buffer stays coherent
        for (int ch = 0; ch < numCh; ++ch)
            delayBuffer.setSample (ch, writePos, (float) writeVal);

        if (stereo)
        {
            buffer.setSample (0, i, outL);
            buffer.setSample (1, i, outR);
        }
        else
        {
            buffer.setSample (0, i, 0.5f * (outL + outR));
        }

        ++writeCounter;
    }
}
