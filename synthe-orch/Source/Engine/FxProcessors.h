#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <vector>

#include "SinTable.h"

namespace mos {
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
                const float gain = std::max(0.5f, std::min(2.0f,
                    1.0f + attack * transientPos * 3.0f + sustain * transientNeg * 2.5f));

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
        const float decay     = 0.25f + clamp01(p.decay) * 0.73f;
        const float dampCoeff = 1.0f - clamp01(p.damping) * 0.7f;
        const float bw        = 0.9995f - clamp01(p.damping) * 0.3f;
        const float mix       = clamp01(p.mix);
        if (mix <= 0.0001f) return;

        const float preDelaySamples = clamp01(p.preDelayMs / 100.0f)
                                       * static_cast<float>(sr) * 0.1f;
        const float modRate  = 0.8f / static_cast<float>(sr);
        const float modDepth = 8.0f * scaleFactor;
        const float width    = clamp01(p.width);

        const float id0 = kInDiffLen[0] * scaleFactor;
        const float id1 = kInDiffLen[1] * scaleFactor;
        const float id2 = kInDiffLen[2] * scaleFactor;
        const float id3 = kInDiffLen[3] * scaleFactor;
        const float ta0 = kTankApfLen[0] * scaleFactor;
        const float ta1 = kTankApfLen[1] * scaleFactor;
        const float td0 = kTankDelayLen[0] * scaleFactor;
        const float td1 = kTankDelayLen[1] * scaleFactor;
        const float decayDiff = decay * 0.6f + 0.1f;

        for (int i = 0; i < numSamples; ++i)
        {
            const float dryL = left[i];
            const float dryR = right != nullptr ? right[i] : dryL;

            float input = (dryL + dryR) * 0.5f;
            preDelay.push(input);
            input = preDelay.readLinear(preDelaySamples);
            input = inBandwidth[0].process(input, bw);

            input = inDiff[0].process(input, id0, 0.75f);
            input = inDiff[1].process(input, id1, 0.75f);
            input = inDiff[2].process(input, id2, 0.625f);
            input = inDiff[3].process(input, id3, 0.625f);

            const float lfo = mos::fastSin(modPhase);
            modPhase += modRate;
            if (modPhase >= 1.0f) modPhase -= 1.0f;

            float t0 = input + tankState[1] * decay;
            t0 = tankModApf[0].process(t0, ta0 + lfo * modDepth, decayDiff);
            tankDelay[0].push(t0);
            t0 = tankDelay[0].readLinear(td0);
            t0 = tankDamp[0].process(t0, dampCoeff) * decay;
            tankState[0] = t0;

            float t1 = input + tankState[0] * decay;
            t1 = tankModApf[1].process(t1, ta1 - lfo * modDepth, decayDiff);
            tankDelay[1].push(t1);
            t1 = tankDelay[1].readLinear(td1);
            t1 = tankDamp[1].process(t1, dampCoeff) * decay;
            tankState[1] = t1;

            // flush tank denormals
            for (auto& s : tankState) { s += 1e-25f; s -= 1e-25f; }

            const float wetL = tankState[0];
            const float wetR = tankState[1];
            const float wetMono = (wetL + wetR) * 0.5f;
            const float outWetL = wetMono + (wetL - wetMono) * width;
            const float outWetR = wetMono + (wetR - wetMono) * width;

            const float dry = 1.0f - mix * 0.5f;
            left[i] = dryL * dry + outWetL * mix;
            if (right != nullptr)
                right[i] = dryR * dry + outWetR * mix;
        }
    }

private:
    double sr = 44100.0;
    float scaleFactor = 1.0f;

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
// Diffuse Hall Reverb
// =============================================================================
class DiffuseHallReverb
{
public:
    void prepare(double sampleRate, int /*maxBlockSize*/)
    {
        sr = std::max(1.0, sampleRate);
        scaleFactor = static_cast<float>(sr / 48000.0);

        preDelay.allocate(static_cast<int>(sr * 0.12) + 16);

        for (int channel = 0; channel < 2; ++channel)
        {
            for (int idx = 0; idx < 2; ++idx)
                earlyDiff[channel][idx].allocate(static_cast<int>(kEarlyDiffLen[idx] * scaleFactor * 1.4f) + 16);

            for (int idx = 0; idx < 4; ++idx)
                combDelay[channel][idx].allocate(static_cast<int>(kCombLen[idx] * scaleFactor * 1.8f) + 16);

            for (int idx = 0; idx < 2; ++idx)
                lateDiff[channel][idx].allocate(static_cast<int>(kLateDiffLen[idx] * scaleFactor * 1.5f) + 16);
        }

        reset();
    }

    void reset()
    {
        preDelay.clear();
        for (auto& channel : earlyDiff)
            for (auto& diff : channel)
                diff.clear();
        for (auto& channel : combDelay)
            for (auto& comb : channel)
                comb.clear();
        for (auto& channel : lateDiff)
            for (auto& diff : channel)
                diff.clear();
        for (auto& channel : combDamp)
            for (auto& damp : channel)
                damp.clear();
        feedbackState = { 0.0f, 0.0f };
        modPhase = 0.0f;
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
        const float mix = clamp01(p.mix);
        if (mix <= 0.0001f)
            return;

        const int numCh = (right != nullptr) ? 2 : 1;
        const float size = clamp01(p.decay);
        const float roomScale = 0.88f + size * 0.52f;
        const float feedback = 0.62f + size * 0.33f;
        const float dampCoeff = 0.58f - clamp01(p.damping) * 0.42f;
        const float width = clamp01(p.width);
        const float preDelaySamples = clamp01(p.preDelayMs / 100.0f) * static_cast<float>(sr) * 0.12f;
        const float stereoSpread = 19.0f * scaleFactor;
        const float modRate = 0.17f / static_cast<float>(sr);
        const float modDepth = 6.0f * scaleFactor;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float dryL = left[sample];
            const float dryR = right != nullptr ? right[sample] : dryL;

            float input = (dryL + dryR) * 0.5f;
            preDelay.push(input);
            input = preDelay.readLinear(preDelaySamples);

            const float mod = mos::fastSin(modPhase) * modDepth;
            modPhase += modRate;
            if (modPhase >= 1.0f)
                modPhase -= 1.0f;

            float wet[2] = { 0.0f, 0.0f };
            for (int channel = 0; channel < numCh; ++channel)
            {
                const int other = numCh > 1 ? 1 - channel : channel;
                float diffuseIn = input + feedbackState[channel] * 0.08f + feedbackState[other] * 0.18f;
                diffuseIn = earlyDiff[channel][0].process(diffuseIn,
                                                          kEarlyDiffLen[0] * scaleFactor * roomScale + (channel == 0 ? mod : -mod),
                                                          0.72f);
                diffuseIn = earlyDiff[channel][1].process(diffuseIn,
                                                          kEarlyDiffLen[1] * scaleFactor * roomScale - (channel == 0 ? mod : -mod),
                                                          0.68f);

                float combSum = 0.0f;
                for (int idx = 0; idx < 4; ++idx)
                {
                    const float delaySamples = kCombLen[idx] * scaleFactor * roomScale
                                             + static_cast<float>(channel) * stereoSpread
                                             + ((idx & 1) == 0 ? mod : -mod);
                    const float delayed = combDelay[channel][idx].readLinear(delaySamples);
                    const float damped = combDamp[channel][idx].process(delayed, dampCoeff);
                    combDelay[channel][idx].push(diffuseIn + damped * feedback);
                    combSum += delayed;
                }

                float hall = combSum * 0.24f;
                hall = lateDiff[channel][0].process(hall, kLateDiffLen[0] * scaleFactor * roomScale, 0.63f);
                hall = lateDiff[channel][1].process(hall, kLateDiffLen[1] * scaleFactor * roomScale, 0.57f);
                feedbackState[channel] = hall;
                wet[channel] = hall;
            }

            if (numCh == 1)
                wet[1] = wet[0];

            const float wetMono = (wet[0] + wet[1]) * 0.5f;
            const float outWetL = wetMono + (wet[0] - wetMono) * width;
            const float outWetR = wetMono + (wet[1] - wetMono) * width;
            const float dry = 1.0f - mix * 0.45f;

            left[sample] = dryL * dry + outWetL * mix;
            if (right != nullptr)
                right[sample] = dryR * dry + outWetR * mix;
        }
    }

private:
    double sr = 44100.0;
    float scaleFactor = 1.0f;

    static constexpr float kEarlyDiffLen[2] = { 173.0f, 241.0f };
    static constexpr float kCombLen[4] = { 1687.0f, 1601.0f, 2053.0f, 2251.0f };
    static constexpr float kLateDiffLen[2] = { 271.0f, 347.0f };

    DelayLine preDelay;
    AllPass earlyDiff[2][2];
    DelayLine combDelay[2][4];
    OnePole combDamp[2][4];
    AllPass lateDiff[2][2];
    std::array<float, 2> feedbackState = { 0.0f, 0.0f };
    float modPhase = 0.0f;
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
        for (auto& s : state) s = {};
    }

    void reset()
    {
        for (auto& s : state) s = {};
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
        if (std::abs(p.lowGainDb)  < 0.05f &&
            std::abs(p.midGainDb)  < 0.05f &&
            std::abs(p.highGainDb) < 0.05f)
            return;

        coeffs[0] = calcLowShelf(p.lowFreq, p.lowGainDb);
        coeffs[1] = calcPeaking(p.midFreq, p.midGainDb, p.midQ);
        coeffs[2] = calcHighShelf(p.highFreq, p.highGainDb);

        for (int i = 0; i < numSamples; ++i)
        {
            float L = left[i];
            float R = right != nullptr ? right[i] : 0.0f;
            for (int b = 0; b < 3; ++b)
            {
                L = biquadDF2T(state[b * 2],     coeffs[b], L);
                R = biquadDF2T(state[b * 2 + 1], coeffs[b], R);
            }
            left[i] = L;
            if (right != nullptr) right[i] = R;
        }
    }

private:
    struct BiquadCoeffs { float b0=1, b1=0, b2=0, a1=0, a2=0; };
    struct BiquadState  { float z1=0, z2=0; };

    static float biquadDF2T(BiquadState& s, const BiquadCoeffs& c, float x)
    {
        const float y = c.b0 * x + s.z1;
        s.z1 = c.b1 * x - c.a1 * y + s.z2;
        s.z2 = c.b2 * x - c.a2 * y;
        return y;
    }

    BiquadCoeffs calcLowShelf(float freq, float gainDb) const
    {
        freq = std::min(freq, static_cast<float>(sr) * 0.48f);
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
        freq = std::min(freq, static_cast<float>(sr) * 0.48f);
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
        freq = std::min(freq, static_cast<float>(sr) * 0.48f);
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
        float rateHz = 1.0f;   // 0.1 – 5 Hz
        float depth  = 0.5f;   // 0 – 1
        float mix    = 0.0f;   // 0 – 1
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
                const float lfo = mos::fastSin(lfoPhase[c]);
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
        float timeMs     = 300.0f;   // 1 – 2000 ms
        float feedback   = 0.30f;    // 0 – 0.95
        float mix        = 0.0f;     // 0 – 1
        bool  syncToBpm  = false;
        float bpm        = 120.0f;
        int   noteDiv    = 0;        // 0=1/4, 1=1/8, 2=1/16, 3=dotted 1/8, 4=triplet 1/8
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float mix = clamp01(p.mix);
        if (mix <= 0.0001f) return;

        float delaySamples;
        if (p.syncToBpm && p.bpm > 20.0f)
        {
            const float beatSec = 60.0f / std::max(20.0f, p.bpm);
            float mult = 1.0f;
            switch (p.noteDiv)
            {
                case 1: mult = 0.5f;      break;  // 1/8
                case 2: mult = 0.25f;     break;  // 1/16
                case 3: mult = 0.75f;     break;  // dotted 1/8
                case 4: mult = 1.0f/3.0f; break;  // triplet 1/8
                default: mult = 1.0f;     break;  // 1/4
            }
            delaySamples = beatSec * mult * static_cast<float>(sr);
        }
        else
        {
            delaySamples = std::max(1.0f, p.timeMs) * 0.001f * static_cast<float>(sr);
        }

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
// Output Limiter  (feed-forward peak limiter with fixed 1 ms lookahead)
// =============================================================================
class OutputLimiter
{
public:
    void prepare(double sampleRate)
    {
        sr = std::max(1.0, sampleRate);
        lookaheadSamples = juce::jlimit(1, kMaxLookaheadSamples - 1,
            static_cast<int>(std::round(sr * 0.001)));
        reset();
    }

    void reset()
    {
        env = 0.0f;
        writePos = 0;
        delayL.fill(0.0f);
        delayR.fill(0.0f);
    }

    struct Params
    {
        float thresholdDb = -1.0f;   // -12 .. 0 dB
        float releaseMs   = 200.0f;  // 1 .. 500 ms
    };

    void process(float* left, float* right, int numSamples, const Params& p)
    {
        const float thresh = std::pow(10.0f, std::min(0.0f, p.thresholdDb) / 20.0f);
        if (thresh >= 0.9999f)
            return;

        lookaheadSamples = juce::jlimit(1, kMaxLookaheadSamples - 1,
            static_cast<int>(std::round(sr * 0.001)));
        const float relCoeff = std::exp(-1.0f / (std::max(1.0f, p.releaseMs) * 0.001f
                                                  * static_cast<float>(sr)));

        for (int i = 0; i < numSamples; ++i)
        {
            const int readPos = (writePos + kMaxLookaheadSamples - lookaheadSamples) & (kMaxLookaheadSamples - 1);
            const float delayedL = delayL[static_cast<std::size_t>(readPos)];
            const float delayedR = delayR[static_cast<std::size_t>(readPos)];
            const float inL = left[i];
            const float inR = right != nullptr ? right[i] : inL;

            delayL[static_cast<std::size_t>(writePos)] = inL;
            delayR[static_cast<std::size_t>(writePos)] = inR;
            writePos = (writePos + 1) & (kMaxLookaheadSamples - 1);

            const float detector = std::max(std::abs(inL), std::abs(inR));
            if (detector > env)
                env = detector;
            else
                env = relCoeff * env + (1.0f - relCoeff) * detector;

            const float gain = env > thresh ? (thresh * 0.985f) / std::max(env, 1.0e-12f) : 1.0f;
            left[i] = delayedL * gain;
            if (right != nullptr)
                right[i] = delayedR * gain;
        }
    }

private:
    static constexpr int kMaxLookaheadSamples = 512;
    static_assert((kMaxLookaheadSamples & (kMaxLookaheadSamples - 1)) == 0,
                  "OutputLimiter lookahead ring must be power-of-two");

    double sr = 44100.0;
    float env = 0.0f;
    int writePos = 0;
    int lookaheadSamples = 44;
    std::array<float, kMaxLookaheadSamples> delayL = {};
    std::array<float, kMaxLookaheadSamples> delayR = {};
};

} // namespace fx
} // namespace mos
