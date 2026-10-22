#pragma once
#include <JuceHeader.h>
#include <cmath>

// =============================================================================
// Core delay engine for Pocket Tracer.
//
// One shared circular delay buffer + feedback path, dispatched to one of six
// character "types". Per-type coloration lives INSIDE the feedback loop, so
// Analog/Tape/Lo-Fi degrade cumulatively per repeat (signed off Stage 1).
//
// The engine REPLACES the input buffer with the wet (repeats-only) signal;
// the processor applies the Tone tilt and the equal-power dry/wet mix.
// =============================================================================

class DelayEngine
{
public:
    enum class Type { Digital = 0, Analog, Tape, Reverse, LoFi, MultiTap };

    void prepare (const juce::dsp::ProcessSpec& spec);
    void reset();

    void setType        (int typeIndex);
    void setTimeMs      (float timeMs);     // resolved delay time (free or synced)
    void setFeedback    (float fb01);       // 0.0 .. 1.0 knob; tapered internally, loop gain caps at 0.92
    void setModulation  (float mod01);      // 0.0 .. 1.0
    void setPingPong    (bool shouldPingPong);
    void setFreeze      (bool shouldFreeze);

    void process (juce::AudioBuffer<float>& buffer);

    static constexpr double kMaxDelaySeconds = 6.0; // covers 1 bar @ ~40 BPM

private:
    // ------------------------------------------------------------------ DSP bits
    struct DCBlocker
    {
        double x1 = 0.0, y1 = 0.0;
        double process (double x) { double y = x - x1 + 0.9995 * y1; x1 = x; y1 = y; return y; }
        void reset() { x1 = y1 = 0.0; }
    };

    struct OnePoleLP
    {
        float a = 1.0f, z = 0.0f;
        void setCutoff (double fc, double fs)
        { a = (float) (1.0 - std::exp (-2.0 * juce::MathConstants<double>::pi * fc / fs)); }
        float tick (float x) { z += a * (x - z); return z; }
        void reset() { z = 0.0f; }
    };

    struct OnePoleHP
    {
        OnePoleLP lp;
        void setCutoff (double fc, double fs) { lp.setCutoff (fc, fs); }
        float tick (float x) { return x - lp.tick (x); }
        void reset() { lp.reset(); }
    };

    // RBJ biquad low-pass (transposed direct form II) — used for OS anti-aliasing.
    struct LP2
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void setLowpass (double fc, double Q, double fs)
        {
            double w0 = 2.0 * juce::MathConstants<double>::pi * fc / fs;
            double c = std::cos (w0), s = std::sin (w0);
            double alpha = s / (2.0 * Q);
            double a0 =  1.0 + alpha;
            b0 = ((1.0 - c) * 0.5) / a0;
            b1 =  (1.0 - c)        / a0;
            b2 = ((1.0 - c) * 0.5) / a0;
            a1 = (-2.0 * c)        / a0;
            a2 =  (1.0 - alpha)    / a0;
        }
        float tick (float x)
        {
            double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void reset() { z1 = z2 = 0.0; }
    };

    // Lightweight 2x oversampler for a scalar nonlinearity: linear-interp
    // upsample -> apply f() at 2x -> Butterworth LP -> decimate.
    struct Oversampler2x
    {
        float prevIn = 0.0f;
        LP2   down;
        void prepare (double fs) { down.setLowpass (0.45 * fs, 0.7071, 2.0 * fs); reset(); }
        void reset() { prevIn = 0.0f; down.reset(); }
        template <typename Fn>
        float process (float x, Fn&& f)
        {
            float s0 = 0.5f * (prevIn + x); // interpolated midpoint
            float s1 = x;
            prevIn = x;
            down.tick (f (s0));             // run filter at both subsamples
            return down.tick (f (s1));      // decimate -> keep second
        }
    };

    // --------------------------------------------------------------- reads
    float readHermite   (int channel, double delaySamples) const;
    float linearReadAbs (int channel, double absTime) const;

    double applyFeedbackSafety (int channel, double sample);

    static void panGains (float pan, float& l, float& r); // pan in [-1,+1], equal power

    void resetColorStates();

    // per-type process paths
    void processStandard (juce::AudioBuffer<float>& buffer); // Digital/Analog/Tape/LoFi
    void processReverse  (juce::AudioBuffer<float>& buffer);
    void processMultiTap (juce::AudioBuffer<float>& buffer);

    float colorize (Type t, int ch, float delayed, float mod); // in-loop character
    double readModOffset (Type t, int ch, float mod) const;    // pitch/width read offset

    // ------------------------------------------------------------------ state
    double sampleRate      = 44100.0;
    int    maxDelaySamples = 1;

    juce::AudioBuffer<float> delayBuffer;
    int        writePos     = 0;
    long long  writeCounter = 0;

    DCBlocker dcBlock[2];

    juce::SmoothedValue<float> smoothTimeMs, smoothFeedback, smoothMod;

    Type currentType = Type::Digital;
    bool pingPong    = false;

    // freeze (equal-power-ish crossfade in/out of the looped buffer)
    bool  freezeTarget = false;
    float freezeGain   = 0.0f;
    float freezeRamp   = 1.0f;

    // modulation LFO phases (0..1)
    double wowPhase = 0, flutterPhase = 0, widthPhase = 0, driftPhase = 0,
           panPhase = 0, warblePhase = 0;

    // coloration state
    OnePoleLP analogLP[2], analogGritLP[2];
    OnePoleLP digitalLP[2];              // per-repeat softening for Digital type
    OnePoleLP tapeLP[2];
    Oversampler2x tapeSat[2];
    OnePoleLP lofiLP[2];
    OnePoleHP lofiHP[2];
    float     lofiHold[2]   = { 0, 0 };
    int       lofiHoldCnt[2] = { 0, 0 };

    // reverse grain state
    double    revPhaseA = 0.0, revPhaseB = 0.0;
    long long revAnchorA = 0, revAnchorB = 0;
    bool      revInit = false;

    juce::Random rng;

    JUCE_LEAK_DETECTOR (DelayEngine)
};
