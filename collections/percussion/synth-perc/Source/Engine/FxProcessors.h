#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "SinTable.h"

namespace mpc {
namespace fx {

// =============================================================================
// Utility
// =============================================================================
inline float clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

inline float hermite(float frac, float y0, float y1, float y2, float y3)
{
    const float c0 = y1;
    const float c1 = 0.5f * (y2 - y0);
    const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// =============================================================================
// Simple fractional delay line (Hermite interpolation)
// =============================================================================
class DelayLine
{
public:
    void allocate(int maxSamples)
    {
        buf.assign(static_cast<std::size_t>(maxSamples + 4), 0.0f);
        mask = static_cast<int>(buf.size());
        wp   = 0;
    }

    void clear()
    {
        std::fill(buf.begin(), buf.end(), 0.0f);
        wp = 0;
    }

    void push(float x)
    {
        buf[static_cast<std::size_t>(wp)] = x;
        if (++wp >= mask) wp = 0;
    }

    float read(float delaySamples) const
    {
        const float d = std::max(0.0f, delaySamples);
        const int   di  = static_cast<int>(d);
        const float frac = d - static_cast<float>(di);
        auto idx = [&](int offset) -> std::size_t {
            int i = wp - 1 - offset;
            while (i < 0) i += mask;
            return static_cast<std::size_t>(i % mask);
        };
        const float y0 = buf[idx(di + 1)];
        const float y1 = buf[idx(di)];
        const float y2 = buf[idx(std::max(0, di - 1))];
        const float y3 = buf[idx(std::max(0, di - 2))];
        return hermite(frac, y0, y1, y2, y3);
    }

    float readLinear(float delaySamples) const
    {
        const float d = std::max(0.0f, delaySamples);
        const int   di  = static_cast<int>(d);
        const float frac = d - static_cast<float>(di);
        auto idx = [&](int offset) -> std::size_t {
            int i = wp - 1 - offset;
            while (i < 0) i += mask;
            return static_cast<std::size_t>(i % mask);
        };
        return buf[idx(di)] + frac * (buf[idx(di + 1)] - buf[idx(di)]);
    }

private:
    std::vector<float> buf;
    int mask = 0;
    int wp   = 0;
};

// =============================================================================
// One-pole lowpass filter
// =============================================================================
struct OnePole
{
    float state = 0.0f;
    float process(float x, float coeff)
    {
        state += coeff * (x - state);
        state += 1e-25f;       // flush denormals
        state -= 1e-25f;
        return state;
    }
    void  clear() { state = 0.0f; }
};

// =============================================================================
// All-pass filter (single delay)
// =============================================================================
class AllPass
{
public:
    void allocate(int maxLen) { delay.allocate(maxLen + 4); }
    void clear() { delay.clear(); }

    float process(float x, float delaySamples, float coeff)
    {
        const float delayed = delay.readLinear(delaySamples);
        const float y = -coeff * x + delayed;
        delay.push(x + coeff * y);
        return y;
    }

private:
    DelayLine delay;
};

// =============================================================================
// Transient Shaper
// =============================================================================
class TransientShaper
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(1.0, sampleRate);
        reset();
    }

    void reset()
    {
        for (auto& e : fastEnv) e = 0.0f;
        for (auto& e : slowEnv) e = 0.0f;
    }

    struct Params
    {
        float attack  =  0.0f;   // -1 .. +1
        float sustain =  0.0f;   // -1 .. +1
        float mix     =  0.0f;   // 0 .. 1
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float mix = clamp01(p.mix);
        const float attack = std::max(-1.0f, std::min(1.0f, p.attack));
        const float sustain = std::max(-1.0f, std::min(1.0f, p.sustain));

        if (mix <= 0.0001f || (std::abs(attack) <= 0.0001f && std::abs(sustain) <= 0.0001f))
            return;

        const float srf = static_cast<float>(sr);
        const float fastCoeff = std::exp(-1.0f / (0.0018f * srf));
        const float slowCoeff = std::exp(-1.0f / (0.055f * srf));

        const int numCh = (right != nullptr) ? 2 : 1;
        float* ch[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const float dry = ch[c][i];
                const float absSample = std::abs(dry);

                auto& fast = fastEnv[static_cast<std::size_t>(c)];
                auto& slow = slowEnv[static_cast<std::size_t>(c)];

                fast = fastCoeff * fast + (1.0f - fastCoeff) * absSample;
                slow = slowCoeff * slow + (1.0f - slowCoeff) * absSample;

                const float transient = fast - slow;
                const float transientPos = std::max(0.0f, transient);
                const float transientNeg = std::max(0.0f, -transient);
                const float gain = std::max(0.2f, std::min(4.0f,
                    1.0f + attack * transientPos * 7.0f + sustain * transientNeg * 5.0f));

                const float wet = dry * gain;
                ch[c][i] = dry + (wet - dry) * mix;
            }
        }
    }

private:
    double sr = 44100.0;
    std::array<float, 2> fastEnv = { 0.0f, 0.0f };
    std::array<float, 2> slowEnv = { 0.0f, 0.0f };
};

// =============================================================================
// Saturator  (tanh waveshaper)
// =============================================================================
class Saturator
{
public:
    struct Params
    {
        float drive = 1.0f;    // 1 .. 16
        float mix   = 0.0f;    // 0 .. 1
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float mix = clamp01(p.mix);
        if (mix <= 0.0001f) return;

        const float drive = std::max(1.0f, std::min(16.0f, p.drive));
        const float normalizer = 1.0f / std::max(0.0001f, std::tanh(drive));

        const int numCh = (right != nullptr) ? 2 : 1;
        float* ch[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const float dry = ch[c][i];
                const float wet = std::tanh(dry * drive) * normalizer;
                ch[c][i] = dry + (wet - dry) * mix;
            }
        }
    }
};

// =============================================================================
// Dattorro Plate Reverb
// =============================================================================
class DattorroPlateReverb
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = std::max(1.0, sampleRate);
        const float scale = static_cast<float>(sr / 29761.0);

        preDelay.allocate(static_cast<int>(sr * 0.1) + 16);

        for (int i = 0; i < 4; ++i)
            inDiff[i].allocate(static_cast<int>(kInDiffLen[i] * scale) + 16);

        for (int i = 0; i < 2; ++i)
        {
            tankModApf[i].allocate(static_cast<int>(kTankApfLen[i] * scale * 1.15f) + 16);
            tankDelay[i].allocate(static_cast<int>(kTankDelayLen[i] * scale) + 16);
        }

        scaleFactor = scale;
        reset();
    }

    void reset()
    {
        preDelay.clear();
        for (auto& d : inDiff) d.clear();
        for (auto& a : tankModApf) a.clear();
        for (auto& d : tankDelay) d.clear();
        for (auto& f : tankDamp) f.clear();
        for (auto& f : inBandwidth) f.clear();
        for (auto& s : tankState) s = 0.0f;
        modPhase = 0.0f;
        smoothingInitialized = false;
    }

    struct Params
    {
        float decay      = 0.55f;  // 0..1
        float damping    = 0.50f;  // 0..1
        float width      = 0.80f;  // 0..1
        float mix        = 0.25f;  // 0..1
        float preDelayMs = 0.0f;   // 0..100 ms
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float targetDecay     = 0.25f + clamp01(p.decay) * 0.73f;
        const float targetDampCoeff = 1.0f - clamp01(p.damping) * 0.7f;
        const float targetBw        = 0.9995f - clamp01(p.damping) * 0.3f;
        const float targetMix       = clamp01(p.mix);
        if (targetMix <= 0.0001f && smoothedMix <= 0.0001f) return;

        const float targetPreDelaySamples = clamp01(p.preDelayMs / 100.0f)
                                            * static_cast<float>(sr) * 0.1f;
        const float modRate  = 0.8f / static_cast<float>(sr);
        const float modDepth = 8.0f * scaleFactor;
        const float targetWidth = clamp01(p.width);
        const float smoothCoeff = 1.0f - std::exp(-1.0f / (0.010f * static_cast<float>(sr)));

        if (!smoothingInitialized)
        {
            smoothedPreDelaySamples = targetPreDelaySamples;
            smoothedDecay = targetDecay;
            smoothedDampCoeff = targetDampCoeff;
            smoothedBw = targetBw;
            smoothedWidth = targetWidth;
            smoothedMix = targetMix;
            smoothingInitialized = true;
        }

        const float id0 = kInDiffLen[0] * scaleFactor;
        const float id1 = kInDiffLen[1] * scaleFactor;
        const float id2 = kInDiffLen[2] * scaleFactor;
        const float id3 = kInDiffLen[3] * scaleFactor;
        const float ta0 = kTankApfLen[0] * scaleFactor;
        const float ta1 = kTankApfLen[1] * scaleFactor;
        const float td0 = kTankDelayLen[0] * scaleFactor;
        const float td1 = kTankDelayLen[1] * scaleFactor;

        for (int i = 0; i < numSamples; ++i)
        {
            smoothedPreDelaySamples += smoothCoeff * (targetPreDelaySamples - smoothedPreDelaySamples);
            smoothedDecay += smoothCoeff * (targetDecay - smoothedDecay);
            smoothedDampCoeff += smoothCoeff * (targetDampCoeff - smoothedDampCoeff);
            smoothedBw += smoothCoeff * (targetBw - smoothedBw);
            smoothedWidth += smoothCoeff * (targetWidth - smoothedWidth);
            smoothedMix += smoothCoeff * (targetMix - smoothedMix);
            const float decayDiff = smoothedDecay * 0.6f + 0.1f;

            const float dryL = left[i];
            const float dryR = right != nullptr ? right[i] : dryL;

            float input = (dryL + dryR) * 0.5f;
            preDelay.push(input);
            input = preDelay.readLinear(smoothedPreDelaySamples);
            input = inBandwidth[0].process(input, smoothedBw);

            input = inDiff[0].process(input, id0, 0.75f);
            input = inDiff[1].process(input, id1, 0.75f);
            input = inDiff[2].process(input, id2, 0.625f);
            input = inDiff[3].process(input, id3, 0.625f);

            const float lfo = mpc::fastSin(modPhase);
            modPhase += modRate;
            if (modPhase >= 1.0f) modPhase -= 1.0f;

            float t0 = input + tankState[1] * smoothedDecay;
            t0 = tankModApf[0].process(t0, ta0 + lfo * modDepth, decayDiff);
            tankDelay[0].push(t0);
            t0 = tankDelay[0].read(td0);
            t0 = tankDamp[0].process(t0, smoothedDampCoeff) * smoothedDecay;
            tankState[0] = std::isfinite(t0) ? t0 : 0.0f;

            float t1 = input + tankState[0] * smoothedDecay;
            t1 = tankModApf[1].process(t1, ta1 - lfo * modDepth, decayDiff);
            tankDelay[1].push(t1);
            t1 = tankDelay[1].read(td1);
            t1 = tankDamp[1].process(t1, smoothedDampCoeff) * smoothedDecay;
            tankState[1] = std::isfinite(t1) ? t1 : 0.0f;

            // flush tank denormals and recover from non-finite feedback states
            for (auto& state : tankState)
            {
                if (!std::isfinite(state)) state = 0.0f;
                state += 1e-25f;
                state -= 1e-25f;
            }

            const float wetL = tankState[0];
            const float wetR = tankState[1];
            const float wetMono = (wetL + wetR) * 0.5f;
            const float outWetL = wetMono + (wetL - wetMono) * smoothedWidth;
            const float outWetR = wetMono + (wetR - wetMono) * smoothedWidth;

            const float dry = 1.0f - smoothedMix * 0.5f;
            left[i] = dryL * dry + outWetL * smoothedMix;
            if (right != nullptr)
                right[i] = dryR * dry + outWetR * smoothedMix;
        }
    }

private:
    double sr = 44100.0;
    float scaleFactor = 1.0f;
    float smoothedPreDelaySamples = 0.0f;
    float smoothedDecay = 0.55f;
    float smoothedDampCoeff = 0.65f;
    float smoothedBw = 0.85f;
    float smoothedWidth = 0.8f;
    float smoothedMix = 0.0f;
    bool smoothingInitialized = false;

    static constexpr float kInDiffLen[4]    = { 142.0f, 107.0f, 379.0f, 277.0f };
    static constexpr float kTankApfLen[2]   = { 672.0f, 908.0f };
    static constexpr float kTankDelayLen[2] = { 4453.0f, 3720.0f };

    DelayLine preDelay;
    AllPass   inDiff[4];
    AllPass   tankModApf[2];
    DelayLine tankDelay[2];
    OnePole   tankDamp[2];
    OnePole   inBandwidth[1];
    float     tankState[2] = { 0.0f, 0.0f };
    float     modPhase = 0.0f;
};

// =============================================================================
// 3-Band Parametric EQ  (Low Shelf / Mid Peak / High Shelf)
// =============================================================================
class ParametricEQ3Band
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(1.0, sampleRate);
        coeffsValid = false;
        smoothedParams = Params {};
        for (auto& st : state) st = {};
    }

    void reset()
    {
        coeffsValid = false;
        smoothedParams = Params {};
        for (auto& st : state) st = {};
    }

    struct Params
    {
        float lowFreq    = 200.0f;
        float lowGainDb  = 0.0f;
        float midFreq    = 1000.0f;
        float midGainDb  = 0.0f;
        float midQ       = 1.0f;
        float highFreq   = 5000.0f;
        float highGainDb = 0.0f;
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const Params target = sanitizeParams(p);
        if (std::abs(target.lowGainDb)  < 0.05f &&
            std::abs(target.midGainDb)  < 0.05f &&
            std::abs(target.highGainDb) < 0.05f)
            return;

        if (!coeffsValid)
            smoothedParams = target;
        else
            smoothParamsToward(smoothedParams, target, 0.35f);

        if (!coeffsValid || paramsChanged(lastCoeffParams, smoothedParams))
        {
            coeffs[0] = sanitizeCoeffs(calcLowShelf(smoothedParams.lowFreq, smoothedParams.lowGainDb));
            coeffs[1] = sanitizeCoeffs(calcPeaking(smoothedParams.midFreq, smoothedParams.midGainDb, smoothedParams.midQ));
            coeffs[2] = sanitizeCoeffs(calcHighShelf(smoothedParams.highFreq, smoothedParams.highGainDb));
            lastCoeffParams = smoothedParams;
            coeffsValid = true;
        }

        for (int i = 0; i < numSamples; ++i)
        {
            float L = left[i];
            float R = right != nullptr ? right[i] : 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                L = biquadDF2T(state[b * 2],     coeffs[b], L);
                R = biquadDF2T(state[b * 2 + 1], coeffs[b], R);
            }
            if (!std::isfinite(L))
            {
                L = 0.0f;
                for (int b = 0; b < 3; ++b)
                    state[b * 2] = {};
            }
            if (right != nullptr && !std::isfinite(R))
            {
                R = 0.0f;
                for (int b = 0; b < 3; ++b)
                    state[b * 2 + 1] = {};
            }
            left[i] = L;
            if (right != nullptr) right[i] = R;
        }
    }

private:
    struct BiquadCoeffs { float b0=1, b1=0, b2=0, a1=0, a2=0; };
    struct BiquadState  { float z1=0, z2=0; };

    static float biquadDF2T(BiquadState& st, const BiquadCoeffs& c, float x)
    {
        const float y = c.b0 * x + st.z1;
        st.z1 = c.b1 * x - c.a1 * y + st.z2;
        st.z2 = c.b2 * x - c.a2 * y;
        if (!std::isfinite(y) || !std::isfinite(st.z1) || !std::isfinite(st.z2))
        {
            st = {};
            return 0.0f;
        }
        return y;
    }

    Params sanitizeParams(Params p) const noexcept
    {
        const float maxFreq = std::max(20.0f, static_cast<float>(sr) * 0.45f);
        p.lowFreq = std::clamp(p.lowFreq, 20.0f, maxFreq);
        p.midFreq = std::clamp(p.midFreq, 20.0f, maxFreq);
        p.highFreq = std::clamp(p.highFreq, 20.0f, maxFreq);
        p.lowGainDb = std::clamp(p.lowGainDb, -18.0f, 18.0f);
        p.midGainDb = std::clamp(p.midGainDb, -18.0f, 18.0f);
        p.highGainDb = std::clamp(p.highGainDb, -18.0f, 18.0f);
        p.midQ = std::clamp(p.midQ, 0.10f, 8.0f);
        return p;
    }

    static bool paramsChanged(const Params& a, const Params& b) noexcept
    {
        return std::abs(a.lowFreq - b.lowFreq) > 0.5f
            || std::abs(a.lowGainDb - b.lowGainDb) > 0.01f
            || std::abs(a.midFreq - b.midFreq) > 0.5f
            || std::abs(a.midGainDb - b.midGainDb) > 0.01f
            || std::abs(a.midQ - b.midQ) > 0.001f
            || std::abs(a.highFreq - b.highFreq) > 0.5f
            || std::abs(a.highGainDb - b.highGainDb) > 0.01f;
    }

    static void smoothParamsToward(Params& current, const Params& target, float amount) noexcept
    {
        current.lowFreq += (target.lowFreq - current.lowFreq) * amount;
        current.lowGainDb += (target.lowGainDb - current.lowGainDb) * amount;
        current.midFreq += (target.midFreq - current.midFreq) * amount;
        current.midGainDb += (target.midGainDb - current.midGainDb) * amount;
        current.midQ += (target.midQ - current.midQ) * amount;
        current.highFreq += (target.highFreq - current.highFreq) * amount;
        current.highGainDb += (target.highGainDb - current.highGainDb) * amount;
    }

    static BiquadCoeffs sanitizeCoeffs(BiquadCoeffs c) noexcept
    {
        if (!std::isfinite(c.b0) || !std::isfinite(c.b1) || !std::isfinite(c.b2)
            || !std::isfinite(c.a1) || !std::isfinite(c.a2))
            return {};
        return c;
    }

    BiquadCoeffs calcLowShelf(float freq, float gainDb) const
    {
        freq = std::clamp(freq, 20.0f, static_cast<float>(sr) * 0.45f);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * 3.14159265f * freq / static_cast<float>(sr);
        const float cosw = std::cos(w0), sinw = std::sin(w0);
        const float alpha = sinw / (2.0f * 0.707f);
        const float sqA = std::sqrt(A);
        const float a0 = (A + 1.0f) + (A - 1.0f) * cosw + 2.0f * sqA * alpha;
        BiquadCoeffs c;
        c.b0 = A * ((A + 1.0f) - (A - 1.0f) * cosw + 2.0f * sqA * alpha) / a0;
        c.b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
        c.b2 = A * ((A + 1.0f) - (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        c.a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
        c.a2 = ((A + 1.0f) + (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        return c;
    }

    BiquadCoeffs calcPeaking(float freq, float gainDb, float Q) const
    {
        freq = std::clamp(freq, 20.0f, static_cast<float>(sr) * 0.45f);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * 3.14159265f * freq / static_cast<float>(sr);
        const float cosw = std::cos(w0), sinw = std::sin(w0);
        const float alpha = sinw / (2.0f * std::max(0.01f, Q));
        const float a0 = 1.0f + alpha / A;
        BiquadCoeffs c;
        c.b0 = (1.0f + alpha * A) / a0;
        c.b1 = (-2.0f * cosw) / a0;
        c.b2 = (1.0f - alpha * A) / a0;
        c.a1 = c.b1;
        c.a2 = (1.0f - alpha / A) / a0;
        return c;
    }

    BiquadCoeffs calcHighShelf(float freq, float gainDb) const
    {
        freq = std::clamp(freq, 20.0f, static_cast<float>(sr) * 0.45f);
        const float A = std::pow(10.0f, gainDb / 40.0f);
        const float w0 = 2.0f * 3.14159265f * freq / static_cast<float>(sr);
        const float cosw = std::cos(w0), sinw = std::sin(w0);
        const float alpha = sinw / (2.0f * 0.707f);
        const float sqA = std::sqrt(A);
        const float a0 = (A + 1.0f) - (A - 1.0f) * cosw + 2.0f * sqA * alpha;
        BiquadCoeffs c;
        c.b0 = A * ((A + 1.0f) + (A - 1.0f) * cosw + 2.0f * sqA * alpha) / a0;
        c.b1 = -2.0f * A * ((A - 1.0f) + (A + 1.0f) * cosw) / a0;
        c.b2 = A * ((A + 1.0f) + (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        c.a1 = 2.0f * ((A - 1.0f) - (A + 1.0f) * cosw) / a0;
        c.a2 = ((A + 1.0f) - (A - 1.0f) * cosw - 2.0f * sqA * alpha) / a0;
        return c;
    }

    double sr = 44100.0;
    BiquadCoeffs coeffs[3];
    BiquadState  state[6];
    Params lastCoeffParams {};
    Params smoothedParams {};
    bool coeffsValid = false;
};

// =============================================================================
// Stereo Chorus
// =============================================================================
class StereoChorus
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = std::max(1.0, sampleRate);
        const int maxDelaySamples = static_cast<int>(sr * 0.05) + 16;
        for (auto& d : delay) d.allocate(maxDelaySamples);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) d.clear();
        lfoPhase[0] = 0.0f;
        lfoPhase[1] = 0.25f;
    }

    struct Params
    {
        float rateHz = 1.0f;   // 0.1 - 5 Hz
        float depth  = 0.5f;   // 0 - 1
        float mix    = 0.0f;   // 0 - 1
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float mix = clamp01(p.mix);
        if (mix <= 0.0001f) return;

        const float rate  = std::max(0.01f, p.rateHz);
        const float depth = clamp01(p.depth);
        const float phaseInc = rate / static_cast<float>(sr);
        const float baseDelay = 0.007f * static_cast<float>(sr);
        const float modAmt    = depth * 0.003f * static_cast<float>(sr);

        const int numCh = (right != nullptr) ? 2 : 1;
        float* ch[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const float lfo = mpc::fastSin(lfoPhase[c]);
                const float delaySamples = baseDelay + lfo * modAmt;
                delay[c].push(ch[c][i]);
                const float wet = delay[c].read(delaySamples);
                ch[c][i] = ch[c][i] * (1.0f - mix) + wet * mix;
            }

            lfoPhase[0] += phaseInc;
            if (lfoPhase[0] >= 1.0f) lfoPhase[0] -= 1.0f;
            lfoPhase[1] += phaseInc;
            if (lfoPhase[1] >= 1.0f) lfoPhase[1] -= 1.0f;
        }
    }

private:
    double sr = 44100.0;
    DelayLine delay[2];
    float lfoPhase[2] = { 0.0f, 0.25f };
};

// =============================================================================
// Stereo Delay with optional BPM sync
// =============================================================================
class StereoDelay
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = std::max(1.0, sampleRate);
        const int maxDelaySamples = static_cast<int>(sr * 2.0) + 16;
        for (auto& d : delay) d.allocate(maxDelaySamples);
        reset();
    }

    void reset()
    {
        for (auto& d : delay) d.clear();
    }

    struct Params
    {
        float timeMs     = 300.0f;   // 1 - 2000 ms
        float feedback   = 0.30f;    // 0 - 0.95
        float mix        = 0.0f;     // 0 - 1
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float mix = clamp01(p.mix);
        if (mix <= 0.0001f) return;

        float delaySamples = std::max(1.0f, p.timeMs) * 0.001f * static_cast<float>(sr);

        const float maxDelay = static_cast<float>(sr) * 2.0f - 2.0f;
        delaySamples = std::min(delaySamples, maxDelay);

        const float fb = std::min(0.95f, std::max(0.0f, p.feedback));
        const int numCh = (right != nullptr) ? 2 : 1;
        float* ch[2] = { left, right };

        for (int i = 0; i < numSamples; ++i)
        {
            for (int c = 0; c < numCh; ++c)
            {
                const float delayed = delay[c].readLinear(delaySamples);
                delay[c].push(ch[c][i] + delayed * fb);
                ch[c][i] = ch[c][i] * (1.0f - mix) + delayed * mix;
            }
        }
    }

private:
    double sr = 44100.0;
    DelayLine delay[2];
};

// =============================================================================
// Output Limiter  (feed-forward, look-ahead-free brick-wall)
// =============================================================================
class OutputLimiter
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(1.0, sampleRate);
        reset();
    }

    void reset()
    {
        envL = 0.0f;
        envR = 0.0f;
    }

    struct Params
    {
        float thresholdDb = -0.3f;  // -12 .. 0 dB
        float releaseMs   = 50.0f;  // 1 .. 200 ms
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float thresh = std::pow(10.0f, std::min(0.0f, p.thresholdDb) / 20.0f);
        if (thresh >= 0.9999f) return;

        const float relCoeff = std::exp(-1.0f / (std::max(1.0f, p.releaseMs) * 0.001f
                                                  * static_cast<float>(sr)));

        for (int i = 0; i < numSamples; ++i)
        {
            const float absL = std::abs(left[i]);
            if (absL > envL) envL = absL;
            else             envL = relCoeff * envL + (1.0f - relCoeff) * absL;

            if (right != nullptr)
            {
                const float absR = std::abs(right[i]);
                if (absR > envR) envR = absR;
                else             envR = relCoeff * envR + (1.0f - relCoeff) * absR;
            }

            // Linked gain: use the louder channel to preserve stereo image
            const float env = (right != nullptr) ? std::max(envL, envR) : envL;
            if (env > thresh)
            {
                const float gain = thresh / env;
                left[i] *= gain;
                if (right != nullptr) right[i] *= gain;
            }
        }
    }

private:
    double sr = 44100.0;
    float envL = 0.0f;
    float envR = 0.0f;
};

} // namespace fx
} // namespace mpc
