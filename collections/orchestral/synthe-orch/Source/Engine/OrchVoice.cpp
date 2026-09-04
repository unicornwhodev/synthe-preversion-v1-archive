#include "OrchVoice.h"
#include "OrchConstants.h"
#include "SinTable.h"
#include "Models/InstrumentModel.h"
#include "Voices/VoiceFamilyProfiles.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace mos
{
namespace k = mos::constants;

namespace
{
float scaleDecayCoeff(const float coeff, const float timeScale)
{
    if (timeScale <= 0.0f)
        return coeff;

    return std::pow(coeff, 1.0f / timeScale);
}

float coeffToTarget(const float seconds, const double sampleRate, const float target) noexcept
{
    const auto safeSeconds = std::max(0.005f, seconds);
    const auto safeSampleRate = static_cast<float>(std::max(k::kMinSampleRate, sampleRate));
    const auto safeTarget = juce::jlimit(1.0e-6f, 0.999f, target);
    const auto coeff = std::exp(std::log(safeTarget) / (safeSeconds * safeSampleRate));
    return std::isfinite(coeff) ? coeff : 0.999f;
}

float randomSignedUnit(juce::Random& rng)
{
    return rng.nextFloat() * 2.0f - 1.0f;
}

float modelToneControl(const InstrSettings& settings) noexcept
{
    const float cutoffNorm = juce::jlimit(0.0f, 1.0f, (settings.cutoffHz - 800.0f) / 9200.0f);
    return juce::jlimit(0.0f, 1.0f,
        settings.brightness * 0.48f
        + settings.character * 0.24f
        + settings.warmth * 0.16f
        + cutoffNorm * 0.12f);
}

float modelMotionControl(const InstrSettings& settings) noexcept
{
    return juce::jlimit(0.0f, 1.0f,
        settings.vibrato * 0.46f
        + settings.detune * 0.28f
        + settings.stereoWidth * 0.18f
        + settings.character * 0.08f);
}

float modelArticulationControl(const InstrSettings& settings,
                               const InstrCharacteristics& chars,
                               const float velocity) noexcept
{
    const float fastAttack = 1.0f - juce::jlimit(0.0f, 1.0f, settings.attackSeconds / 0.42f);
    const float transientBias = juce::jlimit(0.0f, 1.0f,
        chars.pluckAmount * 0.80f
        + chars.bowNoiseAmount * 0.55f
        + chars.breathNoiseAmount * 0.45f);
    return juce::jlimit(0.0f, 1.0f,
        fastAttack * 0.38f
        + velocity * 0.32f
        + settings.character * 0.18f
        + transientBias * 0.12f);
}
}

// =========================================================================
// PolyBLEP anti-aliasing for saw/square
// =========================================================================
float OrchVoice::polyBlep(float t, float dt)
{
    if (t < dt)
    {
        t /= dt;
        return t + t - t * t - 1.0f;
    }
    if (t > 1.0f - dt)
    {
        t = (t - 1.0f) / dt;
        return t * t + t + t + 1.0f;
    }
    return 0.0f;
}

// =========================================================================
// Linear-interpolating comb read
// =========================================================================
float OrchVoice::readComb(const float* buf, const int bufSize,
                          const int wPos, const float delaySamples) const
{
    const float readPos = static_cast<float>(wPos) - delaySamples;
    const int   idx0 = static_cast<int>(std::floor(readPos));
    const float frac = readPos - static_cast<float>(idx0);

    auto wrap = [bufSize](int i) -> int {
        return ((i % bufSize) + bufSize) % bufSize;
    };

    const float sm1 = buf[wrap(idx0 - 1)];
    const float s0  = buf[wrap(idx0)];
    const float s1  = buf[wrap(idx0 + 1)];
    const float s2  = buf[wrap(idx0 + 2)];

    const float c0 = s0;
    const float c1 = 0.5f * (s1 - sm1);
    const float c2 = sm1 - 2.5f * s0 + 2.0f * s1 - 0.5f * s2;
    const float c3 = 0.5f * (s2 - sm1) + 1.5f * (s0 - s1);
    return ((c3 * frac + c2) * frac + c1) * frac + c0;
}

// =========================================================================
// noteOn
// =========================================================================
void OrchVoice::noteOn(const InstrSettings& s,
                       const int note, const float velocity,
                       const double sampleRate,
                       const float portamentoSeconds,
                       const float roundRobinAmount,
                       const uint32_t noteSeed,
                       const float previousNoteFrequency,
                       const float legatoAmount)
{
    const float previousBaseFreq = baseFreq;
    const bool hadActiveNote = envState != EnvState::Off && midiNote >= 0 && previousBaseFreq > 0.0f;
    const auto& c = getCharacteristics();
    settings = s;
    chars    = c;
    sr       = std::max(k::kMinSampleRate, sampleRate);
    vel      = juce::jlimit(0.0f, 1.0f, velocity);
    midiNote = note;
    rng.setSeed(static_cast<int64>(noteSeed != 0 ? noteSeed
        : static_cast<uint32_t>((getInstrumentIndex() + 1) * 4099u + static_cast<uint32_t>(note + 512))));
    ageSamples = 0;
    realtimeExpressionGain = 1.0f;
    realtimeTimbreCutoffScale = 1.0f;
    realtimeVibratoScale = 1.0f;
    realtimePitchScale = 1.0f;
    realtimePanOffset = 0.0f;
    roundRobinNoiseScale = 1.0f;
    dynamicTimbreCutoffScale = 1.0f;
    dynamicPartialTilt = 0.0f;
    dynamicNoiseScale = 1.0f;
    brassBloomScale = 1.0f;
    formantRegisterScaleApplied = 1.0f;
    legatoOnsetScale = 1.0f;
    legatoAmountActive = 0.0f;
    legatoTransitionActive = false;
    portamentoPitchMult = 1.0f;
    portamentoPitchStep = 0.0f;
    portamentoSamplesRemaining = 0;

    const auto fsr = static_cast<float>(sr);
    const auto family = getFamily(getInstrumentIndex());
    const float velocityCurve = vel * vel;

    const float rrAmount = juce::jlimit(0.0f, 1.0f, roundRobinAmount);
    if (rrAmount > 0.0f)
    {
        juce::Random variationRng(static_cast<int64>(noteSeed != 0 ? noteSeed
            : static_cast<uint32_t>((getInstrumentIndex() + 1) * 131u + static_cast<uint32_t>(note + 128))));
        float detuneRange = 0.0f;
        float attackRange = 0.10f;
        float brightnessRange = 0.03f;
        float noiseRange = 0.0f;

        switch (family)
        {
            case Family::Cordes:
                detuneRange = 0.05f;
                attackRange = 0.20f;
                brightnessRange = 0.08f;
                noiseRange = 0.15f;
                break;
            case Family::Bois:
                detuneRange = 0.03f;
                attackRange = 0.16f;
                brightnessRange = 0.06f;
                break;
            case Family::Cuivres:
                detuneRange = 0.02f;
                attackRange = 0.12f;
                brightnessRange = 0.05f;
                break;
            case Family::Percussions:
                attackRange = 0.10f;
                brightnessRange = 0.03f;
                noiseRange = 0.12f;
                break;
        }

        if (chars.oscMode != OscMode::Modal)
            settings.detune = juce::jlimit(0.0f, 1.0f,
                settings.detune * (1.0f + randomSignedUnit(variationRng) * detuneRange * rrAmount));

        settings.attackSeconds = juce::jmax(0.0001f,
            settings.attackSeconds * (1.0f + randomSignedUnit(variationRng) * attackRange * rrAmount));
        settings.brightness = juce::jlimit(0.0f, 1.0f,
            settings.brightness * (1.0f + randomSignedUnit(variationRng) * brightnessRange * rrAmount));

        if (noiseRange > 0.0f)
        {
            roundRobinNoiseScale = juce::jmax(0.0f,
                1.0f + randomSignedUnit(variationRng) * noiseRange * rrAmount);
        }
    }

    // ----- Base frequency with tuning -----
    baseFreq = 440.0f * std::pow(2.0f,
        (static_cast<float>(note) - 69.0f + settings.tuneSemitones) / 12.0f);

    if (getFamily(getInstrumentIndex()) == Family::Cuivres
        && chars.oscMode == OscMode::Additive
        && chars.detuneAmount > 0.0f)
    {
        juce::Random intonationRng(static_cast<int64>(noteSeed != 0 ? noteSeed
            : static_cast<uint32_t>((getInstrumentIndex() + 17) * 193u + static_cast<uint32_t>(note + 257))));
        const float cents = randomSignedUnit(intonationRng) * chars.detuneAmount
            * (0.35f + settings.detune * 0.65f);
        baseFreq *= std::pow(2.0f, cents / 1200.0f);
    }

    const float portamentoSourceFrequency = previousNoteFrequency > 0.0f
        ? previousNoteFrequency
        : (hadActiveNote ? previousBaseFreq : 0.0f);
    const bool hasPortamentoSource = portamentoSourceFrequency > 0.0f
        && chars.oscMode != OscMode::Modal
        && portamentoSeconds > 0.0f;
    const float legatoControl = juce::jlimit(0.0f, 1.0f, legatoAmount);
    legatoTransitionActive = legatoControl > 0.001f
        && hasPortamentoSource
        && voice::supportsSustainedLegato(family, chars);
    legatoAmountActive = legatoTransitionActive ? legatoControl : 0.0f;

    if (hasPortamentoSource)
    {
        const int glideSamples = std::max(1, static_cast<int>(std::round(portamentoSeconds * fsr)));
        portamentoPitchMult = juce::jlimit(0.25f, 4.0f, portamentoSourceFrequency / std::max(1.0f, baseFreq));
        portamentoPitchStep = (1.0f - portamentoPitchMult) / static_cast<float>(glideSamples);
        portamentoSamplesRemaining = glideSamples;
    }

    // A. Register-dependent decay: high notes decay faster (short strings/columns)
    // MIDI 60 (C4) = 1.0×, MIDI 84 (C6) ≈ 0.90×, MIDI 36 (C2) ≈ 1.10× (±10% max)
    const float regDecayScale   = 1.0f - static_cast<float>(note - 60) / k::kRegisterDecaySlope;
    const float regDecayClamped = std::max(k::kRegisterDecayFloor, regDecayScale);

    // ----- Oscillator setup based on mode -----
    numOscs = 0;
    numActivePartials = 0;

    if (chars.oscMode == OscMode::Additive)
    {
        // Additive synthesis
        const int np = std::min(chars.numPartials, kMaxPartials);
        numActivePartials = np;

        // B. Velocity brightness envelope setup
        const float brightMult = 0.5f + settings.brightness * 1.5f;
        const float cutoffScale = chars.brightnessCutoffScale;
        const float dynamicBrightness = 0.90f + velocityCurve * 0.48f;

        brightCutoffTarget  = baseFreq * cutoffScale * brightMult * dynamicBrightness;
        brightCutoffCurrent = std::min(brightCutoffTarget * (1.0f + vel * 0.62f),
                                       fsr * 0.48f);
        // G. Cuivres: faster brightness decay (lip pressure releases quickly)
        float bDecayTime;
        if (chars.oddHarmonicBias > 0.2f && chars.breathNoiseAmount < 0.01f)
            bDecayTime = 0.07f + vel * 0.11f;  // 70ms pp → 180ms ff
        else
            bDecayTime = 0.12f + vel * 0.18f;  // 120ms pp → 300ms ff
        brightDecayCoeff = std::exp(-1.0f / (bDecayTime * fsr));

        const float oddBias = chars.oddHarmonicBias;

        for (int n = 0; n < np; ++n)
        {
            const int harmonic = n + 1;
            const float fn = static_cast<float>(harmonic) * baseFreq *
                std::sqrt(1.0f + chars.inharmonicity *
                          static_cast<float>(harmonic * harmonic));

            if (fn >= fsr * 0.48f)
            {
                numActivePartials = n;
                break;
            }

            // B. Amplitude stores 1/n * oddMult with velocity tilt — rolloff is applied in render
            const float baseAmp = 1.0f / static_cast<float>(harmonic);

            float oddMult = 1.0f;
            if (harmonic % 2 == 0 && oddBias > 0.0f)
                oddMult = (1.0f - oddBias) * (1.0f - oddBias);

            const float harmonicNorm = static_cast<float>(harmonic - 1)
                / static_cast<float>(std::max(1, np - 1));
            const float partialTilt = (velocityCurve - 0.35f)
                * voice::dynamicPartialDepth(family) * harmonicNorm;

            // A. Register-dependent per-partial decay
            const float dScale = 1.0f /
                (1.0f + (1.0f - settings.brightness * 0.5f)
                 * static_cast<float>(harmonic - 1) * 0.3f);
            const float dTime = std::max(0.02f,
                settings.decaySeconds * chars.decay2Time * dScale * regDecayClamped);

            auto& p = partials[static_cast<std::size_t>(n)];
            p.phase      = (family == Family::Bois || family == Family::Cuivres) ? rng.nextFloat() : 0.0f;
            p.phaseInc   = fn / fsr;
            p.amplitude  = baseAmp * oddMult * std::max(0.30f, 1.0f + partialTilt);
            p.decayCoeff = std::exp(-1.0f / (dTime * fsr));
        }
    }
    else if (chars.oscMode == OscMode::Modal)
    {
        // H. Modal synthesis: parallel inharmonic modes with individual decay (vibraphone etc.)
        numActivePartials = 0;
        // Reset brightness (not used for modal)
        brightCutoffTarget  = 1000.0f;
        brightCutoffCurrent = 1000.0f;
        brightDecayCoeff    = 1.0f;

        for (int n = 0; n < 4; ++n)
        {
            const float fn = chars.modalRatios[n] * baseFreq;
            if (fn <= 0.01f || fn >= fsr * 0.48f)
                break;
            // A. Register-dependent modal decay
            const float dTime = std::max(0.05f,
                settings.decaySeconds * chars.modalDecayMults[n] * regDecayClamped);
            auto& p = partials[static_cast<std::size_t>(n)];
            p.phase      = 0.0f;
            p.phaseInc   = fn / fsr;
            p.amplitude  = chars.modalAmpScales[n];
            p.decayCoeff = std::exp(-1.0f / (dTime * fsr));
            numActivePartials = n + 1;
        }

        // Modal AM vibrato (motor) — driven via chorusPhase / chorusDepth
        chorusPhase    = 0.0f;
        const auto modalVibratoRateHz = chars.vibratoRateHz > 0.01f ? chars.vibratoRateHz : 3.5f;
        chorusPhaseInc = modalVibratoRateHz / fsr;
        chorusDepth    = settings.vibrato * 0.28f;     // AM depth
    }
    else
    {
        // Oscillator modes (saw/sine/square) with unison
        numOscs = std::clamp(chars.numOscillators, 1, kMaxOsc);
        const float detuneHz = chars.detuneAmount * baseFreq * (0.3f + settings.detune * 0.7f);

        // Reset brightness (not used for oscillator modes)
        brightCutoffTarget  = 1000.0f;
        brightCutoffCurrent = 1000.0f;
        brightDecayCoeff    = 1.0f;

        for (int o = 0; o < numOscs; ++o)
        {
            auto& osc = oscs[static_cast<std::size_t>(o)];
            osc.phase = (numOscs > 1) ? rng.nextFloat() : 0.0f;

            float freqOffset = 0.0f;
            if (numOscs == 2)
                freqOffset = (o == 0 ? -detuneHz : detuneHz) * 0.5f;
            else if (numOscs == 3)
                freqOffset = static_cast<float>(o - 1) * detuneHz;
            else if (numOscs == 4)
                freqOffset = (static_cast<float>(o) - 1.5f) * detuneHz * 0.67f;

            osc.phaseInc = (baseFreq + freqOffset) / fsr;

            // Stereo spread for ensemble
            if (numOscs > 1 && settings.stereoWidth > 0.01f)
            {
                float spread = (static_cast<float>(o) / static_cast<float>(numOscs - 1) - 0.5f)
                               * settings.stereoWidth;
                osc.panL = std::sqrt(0.5f * (1.0f - spread));
                osc.panR = std::sqrt(0.5f * (1.0f + spread));
            }
            else
            {
                osc.panL = 0.7071f;
                osc.panR = 0.7071f;
            }
        }
    }

    pitchTransient = 1.0f;
    pitchTransientCoeff = 1.0f;
    if (chars.oscMode == OscMode::Saw && chars.bowNoiseAmount > 0.001f)
    {
        // Bowed string pitch scoop: bow stick-slip overshoots pitch before settling
        const float transientCents = 15.0f + vel * 25.0f;
        const float transientTime  = 0.060f + (1.0f - vel) * 0.040f;
        pitchTransient = std::pow(2.0f, transientCents / 1200.0f);
        pitchTransientCoeff = std::exp(-1.0f / (transientTime * fsr));
    }
    else if (chars.oscMode == OscMode::Additive && chars.breathNoiseAmount < 0.01f)
    {
        float transientCents = 0.0f;
        float transientTime  = 0.040f;
        if (chars.oddHarmonicBias < 0.25f)
        {
            transientCents = 10.0f + vel * 8.0f;   // trumpet lip snap
            transientTime  = 0.028f;
        }
        else if (chars.oddHarmonicBias < 0.50f)
        {
            transientCents = 6.0f + vel * 5.0f;    // horn / trombone onset
            transientTime  = 0.040f;
        }
        else
        {
            transientCents = 3.0f + vel * 3.0f;    // tuba speaks slower and flatter
            transientTime  = 0.055f;
        }

        pitchTransient = std::pow(2.0f, transientCents / 1200.0f);
        pitchTransientCoeff = std::exp(-1.0f / (transientTime * fsr));
    }

    // ----- Vibrato LFO -----
    vibratoPhase = rng.nextFloat();   // random start for natural feel
    vibratoDepthCurrent = 0.0f;
    vibratoDepthTarget = 0.0f;
    vibratoReleaseCoeff = std::exp(-1.0f / (k::kVibratoReleaseSec * fsr));
    vibratoDriftPhase = rng.nextFloat();
    vibratoDriftPhaseInc = k::kVibratoSlowDriftHz / fsr
        * (0.75f + rng.nextFloat() * 0.50f);
    vibratoDriftDepth = k::kVibratoSlowDriftDepth * (0.75f + rng.nextFloat() * 0.50f);
    if (chars.vibratoRateHz > 0.01f && settings.vibrato > 0.01f)
    {
        // Micro-jitter on rate (±2%) prevents phase-locking of concurrent voices
        vibratoPhaseInc = chars.vibratoRateHz / fsr * (1.0f + (rng.nextFloat() - 0.5f) * k::kVibratoJitterPercent);
        const float velocityDepth = k::kVibratoVelocityDepthMin + vel * k::kVibratoVelocityDepthRange;
        vibratoDepth = std::pow(2.0f,
            chars.vibratoDepthCents * settings.vibrato * velocityDepth / 1200.0f) - 1.0f;
        vibratoDepthTarget = vibratoDepth;
        // Do not set vibratoDepthCurrent here — it will be ramped in during the delay period
    }
    else
    {
        vibratoPhaseInc = 0.0f;
        vibratoDepth = 0.0f;
        vibratoDepthCurrent = 0.0f;
        vibratoDepthTarget = 0.0f;
    }

    // D. Vibrato delay: ramp-in after delay for natural onset
    if (chars.vibratoDelaySec > 0.001f && vibratoDepth > 0.0f)
    {
        vibratoDelaySamples = static_cast<int>(chars.vibratoDelaySec * fsr);
        vibratoDelayCounter = 0;
        vibratoDepthCurrent = 0.0f;  // start at 0 and ramp up during delay
    }
    else
    {
        vibratoDelaySamples = 0;
        vibratoDelayCounter = 0;
        vibratoDepthCurrent = vibratoDepth;  // no delay = immediate full depth
    }

    // ----- Amplitude envelope -----
    attackShape = chars.attackShape;
    const float effectiveAttack = settings.attackSeconds *
        (1.0f + attackShape * 2.0f);   // slow bow = longer effective attack

    attackRate = (effectiveAttack > 0.0001f)
                    ? 1.0f / (effectiveAttack * fsr)
                    : 1.0f;

    const float d1Time = std::max(0.01f, settings.decaySeconds * chars.decay1Ratio);
    decay1Coeff  = std::exp(-1.0f / (d1Time * fsr));
    decay1Target = chars.sustainPlatform * settings.sustainLevel;

    const float d2Time = std::max(0.05f, settings.decaySeconds * chars.decay2Time);
    decay2Coeff  = std::exp(-1.0f / (d2Time * fsr));

    releaseCoeff = coeffToTarget(settings.releaseSeconds, sr, std::exp(-1.0f));

    envLevel = 0.0f;
    envState = (effectiveAttack > 0.0001f) ? EnvState::Attack : EnvState::Decay1;
    if (envState == EnvState::Decay1)
        envLevel = 1.0f;

    if (legatoTransitionActive)
    {
        const float glideNorm = juce::jlimit(0.0f, 1.0f, portamentoSeconds / 0.50f);
        const float targetOnsetScale = juce::jmap(glideNorm, 0.72f, 0.42f);
        legatoOnsetScale = 1.0f + (targetOnsetScale - 1.0f) * legatoControl;
        const float targetEnvLevel = juce::jlimit(0.18f, 0.82f, decay1Target * (0.76f + vel * 0.18f));
        envLevel = juce::jmap(legatoControl, envLevel, targetEnvLevel);
        envState = EnvState::Decay1;
    }

    // ----- Pluck transient -----
    pluckLevel = chars.pluckAmount * vel * (0.5f + settings.brightness * 0.5f);
    if (chars.pluckSeconds > 0.0001f)
        pluckDecayCoeff = std::exp(-1.0f / (chars.pluckSeconds * fsr));
    else
        pluckDecayCoeff = 0.0f;
    if (roundRobinNoiseScale != 1.0f && pluckLevel > 0.0f)
        scalePluck(roundRobinNoiseScale);

    // E. Bow noise transient (Section Cordes — ~30ms filtered noise burst)
    bowNoiseLevel = chars.bowNoiseAmount * vel * 0.8f;
    bowNoiseDecay = (chars.bowNoiseAmount > 0.0f)
                    ? std::exp(-1.0f / (0.030f * fsr))
                    : 1.0f;
    bowNoiseState = 0.0f;
    if (roundRobinNoiseScale != 1.0f && bowNoiseLevel > 0.0f)
        scaleBowNoise(roundRobinNoiseScale);

    // F. Breath noise transient (Bois — ~150ms LP-filtered noise burst)
    breathNoiseLevel = chars.breathNoiseAmount * (0.5f + vel * 0.5f);
    breathNoiseDecay = (chars.breathNoiseAmount > 0.0f)
                       ? std::exp(-1.0f / (0.150f * fsr))
                       : 1.0f;
    breathNoiseFilt  = 0.0f;
    breathReleaseTailLevel = 0.0f;
    brassTransientLevel = 0.0f;
    brassTransientDecay = 1.0f;
    brassTransientHpState = 0.0f;
    const bool hasAirReleaseTail = chars.breathNoiseAmount > 0.0f || family == Family::Cuivres;
    const float airReleaseSeconds = family == Family::Cuivres
        ? k::kBrassReleaseTailSec
        : k::kBreathReleaseTailSec;
    breathReleaseTailDecay = hasAirReleaseTail
        ? std::exp(-1.0f / (airReleaseSeconds * fsr))
        : 1.0f;

    dynamicNoiseScale = voice::dynamicNoiseScale(family, vel);
    if (chars.bowNoiseAmount > 0.0f)
        scaleBowNoise(dynamicNoiseScale, 1.12f - velocityCurve * 0.20f);
    if (chars.breathNoiseAmount > 0.0f)
        scaleBreathNoise(dynamicNoiseScale, 1.08f - velocityCurve * 0.16f);
    if (family == Family::Cuivres)
    {
        scaleBrassBloom(0.72f + velocityCurve * 0.58f);
        const float transientSeconds = getInstrumentIndex() == 13 ? 0.018f
            : getInstrumentIndex() == 15 ? 0.055f
            : 0.034f;
        brassTransientLevel = (0.500f + vel * 1.600f)
            * (1.0f + (1.0f - chars.oddHarmonicBias) * 0.35f)
            * (0.70f + dynamicNoiseScale * 0.30f);
        brassTransientDecay = std::exp(-1.0f / (transientSeconds * fsr));
    }

    // ----- SVF filter (with stability guard) -----
    const float safeFreq = juce::jlimit(20.0f, fsr * 0.45f, settings.cutoffHz);
    filterQinv = 1.0f / juce::jmax(0.5f,
        0.5f + (1.0f - settings.brightness) * 1.0f);
    const float rawF = 2.0f * std::sin(juce::MathConstants<float>::pi * safeFreq / fsr);
    const float maxF = -filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f);
    filterF = juce::jmin(rawF, maxF * 0.95f);
    svfLow  = 0.0f;
    svfBand = 0.0f;
    dcX1    = 0.0f;
    dcY1    = 0.0f;

    // ----- Body resonator (primary mode) -----
    if (chars.bodyDelayRatio > 0.01f && settings.warmth > 0.01f)
    {
        const float bodyFreq = baseFreq * chars.bodyDelayRatio;
        bodyDelaySamples = juce::jlimit(2.0f, static_cast<float>(kBodyBufSize - 2),
                                        fsr / std::max(20.0f, bodyFreq));
        bodyFeedback   = settings.warmth * chars.bodyMaxFeedback;
        // FIX: clamp bodyFeedback to prevent comb filter instability (P11)
        bodyFeedback = juce::jlimit(0.0f, 0.5f, bodyFeedback);
        bodyDampState  = 0.0f;
        bodyWritePos   = 0;
        std::memset(bodyBuf, 0, sizeof(bodyBuf));
    }
    else
    {
        bodyFeedback = 0.0f;
    }

    // J. Secondary body modes — parallel BP resonators (Harpe)
    body2State1 = body2State2 = body3State1 = body3State2 = 0.0f;
    body2Gain = body3Gain = 0.0f;
    body2CoeffA = body2CoeffB = body3CoeffA = body3CoeffB = 0.0f;

    if (chars.bodyDelayRatio > 0.01f && settings.warmth > 0.01f)
    {
        constexpr float kTwoPiBody = 6.283185307f;
        const float bodyHz = baseFreq * chars.bodyDelayRatio;

        // Mode 2 at 0.6 × bodyHz (Helmholtz-like lower mode)
        const float f2 = bodyHz * 0.6f;
        if (f2 > 20.0f && f2 < fsr * 0.45f)
        {
            const float w2 = kTwoPiBody * f2 / fsr;
            const float r2 = std::exp(-w2 / std::max(0.5f,
                chars.bodyMaxFeedback * settings.warmth * 4.0f));
            body2CoeffA = 2.0f * r2 * std::cos(w2);
            body2CoeffB = -(r2 * r2);
            body2Gain   = settings.warmth * 0.10f * (1.0f - r2) * 2.0f * std::sin(w2);
        }

        // Mode 3 at 2.2 × bodyHz (higher plate resonance)
        const float f3 = bodyHz * 2.2f;
        if (f3 < fsr * 0.45f && f3 > 20.0f)
        {
            const float w3 = kTwoPiBody * f3 / fsr;
            const float r3 = std::exp(-w3 / std::max(0.5f,
                chars.bodyMaxFeedback * settings.warmth * 3.0f));
            body3CoeffA = 2.0f * r3 * std::cos(w3);
            body3CoeffB = -(r3 * r3);
            body3Gain   = settings.warmth * 0.05f * (1.0f - r3) * 2.0f * std::sin(w3);
        }
    }

    // I. Formant filters (Choeur — parallel BP resonators for vowel character)
    form1State1 = form1State2 = form2State1 = form2State2
               = form3State1 = form3State2 = 0.0f;
    form1Gain = form2Gain = form3Gain = 0.0f;
    form1CoeffA = form1CoeffB = form2CoeffA = form2CoeffB
               = form3CoeffA = form3CoeffB = 0.0f;

    if (chars.hasFormants)
    {
        constexpr float kTwoPiForm = 6.283185307f;
        const auto noteRange = getInstrMidiNoteRange(getInstrumentIndex());
        const float noteNorm = juce::jlimit(0.0f, 1.0f,
            static_cast<float>(note - noteRange.low)
                / static_cast<float>(std::max(1, noteRange.high - noteRange.low)));
        const float lowRegisterScale = family == Family::Cuivres ? 0.78f : 0.72f;
        const float highRegisterScale = family == Family::Cuivres ? 1.32f : 1.40f;
        const float baseRegisterScale = juce::jmap(noteNorm, lowRegisterScale, highRegisterScale);
        const float registerDepth = juce::jlimit(0.0f, 1.0f, chars.formantRegisterScale);
        const float registerScale = juce::jlimit(0.68f, 1.48f,
            (1.0f + (baseRegisterScale - 1.0f) * registerDepth)
            * voice::formantDynamicScale(family, velocityCurve));
        formantRegisterScaleApplied = registerScale;
        auto setupFormant = [&](float fc, float q, float gain,
                                float& ca, float& cb, float& cg)
        {
            fc *= registerScale;
            if (fc < 20.0f || fc > fsr * 0.45f) { ca = cb = cg = 0.0f; return; }
            const float w = kTwoPiForm * fc / fsr;
            const float r = std::exp(-w / (2.0f * std::max(0.1f, q)));
            ca = 2.0f * r * std::cos(w);
            cb = -(r * r);
            cg = gain * (1.0f - r) * std::sin(w);
        };
        setupFormant(chars.formantFreqs[0], chars.formantQs[0], chars.formantGains[0],
                     form1CoeffA, form1CoeffB, form1Gain);
        setupFormant(chars.formantFreqs[1], chars.formantQs[1], chars.formantGains[1],
                     form2CoeffA, form2CoeffB, form2Gain);
        setupFormant(chars.formantFreqs[2], chars.formantQs[2], chars.formantGains[2],
                     form3CoeffA, form3CoeffB, form3Gain);
    }

    // C. Auto-pan by register: subtle, low notes slightly left, high notes slightly right
    {
        const float registerPan = (static_cast<float>(note - 60) / 88.0f) * 0.08f;
        const float totalPan = juce::jlimit(-1.0f, 1.0f, settings.pan + registerPan);
        panL = std::sqrt(0.5f * (1.0f - totalPan));
        panR = std::sqrt(0.5f * (1.0f + totalPan));
    }

    // ----- Ensemble chorus LFO (not used for Modal — it uses its own AM) -----
    if (chars.oscMode != OscMode::Modal)
    {
        chorusPhase = rng.nextFloat();
        if (chars.isEnsemble && settings.stereoWidth > 0.01f)
        {
            chorusPhaseInc = k::kEnsembleChorusRateHz / fsr;
            chorusDepth    = settings.detune * k::kEnsembleChorusDepth;
        }
        else
        {
            chorusPhaseInc = 0.0f;
            chorusDepth    = 0.0f;
        }
    }

    // ----- Max duration guard -----
    maxAgeSamples = static_cast<int>(sr * std::max(1.0f,
        settings.decaySeconds * 6.0f + settings.releaseSeconds * 3.0f));
    if (maxAgeSamples > static_cast<int>(sr * 600.0))
        maxAgeSamples = static_cast<int>(sr * 600.0);

    customizeNoteOn(settings, note, vel);

    if (chars.oscMode != OscMode::Modal)
    {
        dynamicTimbreCutoffScale = juce::jlimit(0.76f, 1.30f,
            1.0f + (velocityCurve - 0.35f) * voice::dynamicCutoffDepth(family));
        scaleFilterCutoff(dynamicTimbreCutoffScale);
    }

    if (legatoTransitionActive)
    {
        scalePitchTransient(1.0f + (0.38f - 1.0f) * legatoControl,
                            1.0f + (0.72f - 1.0f) * legatoControl);
        scaleBowNoise(legatoOnsetScale, 0.70f);
        scaleBreathNoise(legatoOnsetScale, 0.82f);
        vibratoDelayCounter = vibratoDelaySamples;
        const float targetLegatoDepth = vibratoDepthTarget * (0.70f + vel * 0.20f);
        vibratoDepthCurrent = juce::jmap(legatoControl, vibratoDepthCurrent, targetLegatoDepth);
    }

    dynamicPartialTilt = velocityCurve - 0.35f;

    if (instrumentModel != nullptr)
    {
        const float modelTone = modelToneControl(settings);
        const float modelMotion = modelMotionControl(settings);
        const float modelArticulation = modelArticulationControl(settings, chars, vel);
        const auto modelArticulationMode = v2::inferInstrumentArticulation(settings, chars, vel);
        const v2::InstrumentModelNoteContext modelContext {
            getInstrumentIndex(),
            family,
            midiNote,
            vel,
            baseFreq,
            sr,
            settings.level,
            modelTone,
            modelMotion,
            modelArticulation,
            modelArticulationMode,
            legatoTransitionActive,
            legatoControl,
            legatoOnsetScale,
            portamentoSourceFrequency,
            settings,
            chars
        };
        instrumentModel->noteOn(modelContext);
    }
}

void OrchVoice::setInstrumentModel(std::unique_ptr<v2::InstrumentModel> model) noexcept
{
    instrumentModel = std::move(model);
}

void OrchVoice::setLegacyCoreGainOverride(const float gain) noexcept
{
    legacyCoreGainOverride = juce::jlimit(0.0f, 1.0f, gain);
}

void OrchVoice::clearLegacyCoreGainOverride() noexcept
{
    legacyCoreGainOverride = -1.0f;
}

const char* OrchVoice::getInstrumentModelName() const noexcept
{
    return instrumentModel != nullptr ? instrumentModel->name() : "unassigned";
}

bool OrchVoice::isUsingV2InstrumentModel() const noexcept
{
    return instrumentModel != nullptr && instrumentModel->isV2();
}

void OrchVoice::setPerformanceControls(const float expressionGain,
                                       const float timbreCutoffScale,
                                       const float vibratoScale,
                                       const float pitchScale,
                                       const float panOffset) noexcept
{
    realtimeExpressionGain = juce::jlimit(0.0f, 2.0f, expressionGain);
    realtimeTimbreCutoffScale = juce::jlimit(0.25f, 2.5f, timbreCutoffScale);
    realtimeVibratoScale = juce::jlimit(0.0f, 2.5f, vibratoScale);
    realtimePitchScale = juce::jlimit(0.25f, 4.0f, pitchScale);
    realtimePanOffset = juce::jlimit(-1.0f, 1.0f, panOffset);
}

void OrchVoice::setReleaseTimeSeconds(const float seconds) noexcept
{
    releaseCoeff = coeffToTarget(seconds, sr, std::exp(-1.0f));
}

void OrchVoice::setVibratoDelaySeconds(const float seconds)
{
    vibratoDelaySamples = std::max(0, static_cast<int>(seconds * sr));
    vibratoDelayCounter = 0;
}

void OrchVoice::scalePitchTransient(const float amount, const float timeScale)
{
    pitchTransient = 1.0f + (pitchTransient - 1.0f) * std::max(0.0f, amount);
    pitchTransientCoeff = scaleDecayCoeff(pitchTransientCoeff, std::max(0.05f, timeScale));
}

void OrchVoice::scaleBowNoise(const float levelScale, const float decayTimeScale)
{
    bowNoiseLevel *= std::max(0.0f, levelScale);
    bowNoiseDecay = scaleDecayCoeff(bowNoiseDecay, std::max(0.05f, decayTimeScale));
}

void OrchVoice::scaleBreathNoise(const float levelScale, const float decayTimeScale)
{
    breathNoiseLevel *= std::max(0.0f, levelScale);
    breathNoiseDecay = scaleDecayCoeff(breathNoiseDecay, std::max(0.05f, decayTimeScale));
}

void OrchVoice::scalePluck(const float levelScale, const float decayTimeScale)
{
    pluckLevel *= std::max(0.0f, levelScale);
    pluckDecayCoeff = scaleDecayCoeff(pluckDecayCoeff, std::max(0.05f, decayTimeScale));
}

void OrchVoice::scaleBodyResonance(const float primaryScale, const float secondaryScale)
{
    bodyFeedback *= std::max(0.0f, primaryScale);
    body2Gain *= std::max(0.0f, secondaryScale);
    body3Gain *= std::max(0.0f, secondaryScale);
}

void OrchVoice::scaleFormantGains(const float gainScale)
{
    const float scale = std::max(0.0f, gainScale);
    form1Gain *= scale;
    form2Gain *= scale;
    form3Gain *= scale;
}

void OrchVoice::scaleFilterCutoff(const float cutoffScale)
{
    const auto fsr = static_cast<float>(sr);
    const auto scaledCutoff = juce::jlimit(20.0f, fsr * 0.45f,
        settings.cutoffHz * std::max(0.05f, cutoffScale));
    settings.cutoffHz = scaledCutoff;

    const float rawF = 2.0f * std::sin(juce::MathConstants<float>::pi * scaledCutoff / fsr);
    const float maxF = -filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f);
    filterF = juce::jmin(rawF, maxF * 0.95f);

    brightCutoffTarget = std::max(20.0f, brightCutoffTarget * std::max(0.05f, cutoffScale));
    brightCutoffCurrent = std::max(20.0f, brightCutoffCurrent * std::max(0.05f, cutoffScale));
}

void OrchVoice::scaleChorus(const float depthScale, const float rateScale)
{
    chorusDepth *= std::max(0.0f, depthScale);
    chorusPhaseInc *= std::max(0.0f, rateScale);
}

void OrchVoice::scaleBrassBloom(const float bloomScale)
{
    brassBloomScale *= std::max(0.0f, bloomScale);
}

void OrchVoice::setPanOffset(const float delta)
{
    const float currentPan = panR * panR - panL * panL;
    const float nextPan = juce::jlimit(-1.0f, 1.0f, currentPan + delta);
    panL = std::sqrt(0.5f * (1.0f - nextPan));
    panR = std::sqrt(0.5f * (1.0f + nextPan));
}

void OrchVoice::scaleMaxAge(const float ageScale)
{
    maxAgeSamples = std::max(1, static_cast<int>(static_cast<float>(maxAgeSamples) * std::max(0.05f, ageScale)));
}

void OrchVoice::scalePartial(const int partialIndex, const float amplitudeScale, const float decayTimeScale)
{
    if (partialIndex < 0 || partialIndex >= numActivePartials)
        return;

    auto& partial = partials[static_cast<std::size_t>(partialIndex)];
    partial.amplitude *= std::max(0.0f, amplitudeScale);
    partial.decayCoeff = scaleDecayCoeff(partial.decayCoeff, std::max(0.05f, decayTimeScale));
}

void OrchVoice::tiltActivePartials(const float tilt)
{
    if (numActivePartials <= 1)
        return;

    const float denom = static_cast<float>(numActivePartials - 1);
    for (int index = 0; index < numActivePartials; ++index)
    {
        const float position = static_cast<float>(index) / denom;
        const float scale = std::max(0.15f, 1.0f + position * tilt);
        partials[static_cast<std::size_t>(index)].amplitude *= scale;
    }
}

// =========================================================================
// noteOff
// =========================================================================
void OrchVoice::noteOff()
{
    if (envState != EnvState::Off && envState != EnvState::Release)
    {
        const auto family = getFamily(getInstrumentIndex());
        if (chars.breathNoiseAmount > 0.0001f)
            breathReleaseTailLevel = std::max(breathReleaseTailLevel,
                chars.breathNoiseAmount * envLevel * (0.020f + vel * 0.025f));
        else if (family == Family::Cuivres)
            breathReleaseTailLevel = std::max(breathReleaseTailLevel,
                envLevel * (0.0035f + vel * 0.0075f) * (0.70f + chars.oddHarmonicBias * 0.30f));
        vibratoDepthTarget = 0.0f;
        envState = EnvState::Release;
    }
}

void OrchVoice::forceQuickRelease() noexcept
{
    if (envState == EnvState::Off)
        return;
    envState = EnvState::Release;
    vibratoDepthTarget = 0.0f;
    releaseCoeff = coeffToTarget(0.005f, sr, 0.001f);
}

// =========================================================================
// Per-block render
// =========================================================================
void OrchVoice::render(juce::AudioBuffer<float>& buffer,
                       const int startSample, const int numSamples)
{
    if (envState == EnvState::Off)
        return;

    const int numChannels = buffer.getNumChannels();
    if (numChannels <= 0)
        return;

    const float fsr = static_cast<float>(sr);

    auto* left  = buffer.getWritePointer(0);
    auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        if (envState == EnvState::Off)
            break;

        // ---- Vibrato LFO + pitch bend (D: delayed onset for strings/choir) ----
        float pitchMult = pitchBendFactor * realtimePitchScale;
        if (portamentoSamplesRemaining > 0)
        {
            pitchMult *= portamentoPitchMult;
            portamentoPitchMult += portamentoPitchStep;
            --portamentoSamplesRemaining;
            if (portamentoSamplesRemaining <= 0)
            {
                portamentoSamplesRemaining = 0;
                portamentoPitchMult = 1.0f;
                portamentoPitchStep = 0.0f;
            }
        }
        if (std::abs(pitchTransient - 1.0f) > 0.00001f)
        {
            pitchMult *= pitchTransient;
            pitchTransient = 1.0f + (pitchTransient - 1.0f) * pitchTransientCoeff;
        }
        if (vibratoDepthCurrent > 0.0f || vibratoDepthTarget > 0.0f)
        {
            vibratoDepthCurrent = vibratoDepthTarget
                + (vibratoDepthCurrent - vibratoDepthTarget) * vibratoReleaseCoeff;
            if (vibratoDepthCurrent < 1.0e-7f && vibratoDepthTarget <= 0.0f)
                vibratoDepthCurrent = 0.0f;

            ++vibratoDelayCounter;
            if (vibratoDelayCounter >= vibratoDelaySamples)
            {
                const float ramp = std::min(1.0f,
                    static_cast<float>(vibratoDelayCounter - vibratoDelaySamples)
                    / (k::kVibratoRampTimeSec * fsr));
                const float lfo = mos::fastSin(vibratoPhase);
                pitchMult *= 1.0f + lfo * vibratoDepthCurrent * ramp * realtimeVibratoScale;
                const float drift = mos::fastSin(vibratoDriftPhase) * vibratoDriftDepth;
                vibratoPhase += vibratoPhaseInc * (1.0f + drift);
                if (vibratoPhase >= 1.0f) vibratoPhase -= 1.0f;
                vibratoDriftPhase += vibratoDriftPhaseInc;
                if (vibratoDriftPhase >= 1.0f) vibratoDriftPhase -= 1.0f;
            }
        }

        // ---- Ensemble chorus modulation (non-Modal only; Modal uses AM below) ----
        float chorusMod = 0.0f;
        if (chars.oscMode != OscMode::Modal && chorusDepth > 0.0f)
        {
            chorusMod = mos::fastSin(chorusPhase) * chorusDepth;
            chorusPhase += chorusPhaseInc;
            if (chorusPhase >= 1.0f) chorusPhase -= 1.0f;
        }

        // ---- ADSR envelope ----
        switch (envState)
        {
        case EnvState::Attack:
        {
            envLevel += attackRate;
            // Slow bow shape: use S-curve (ease-in)
            float envOutput = envLevel;
            if (attackShape > 0.01f)
            {
                float shaped = envLevel * envLevel * (3.0f - 2.0f * envLevel); // smoothstep
                envOutput = envLevel + attackShape * (shaped - envLevel);
            }
            if (envLevel >= 1.0f)
            {
                envLevel = 1.0f;
                envState = EnvState::Decay1;
            }
            // envLevel stored is always linear for threshold comparison
            break;
        }

        case EnvState::Decay1:
            envLevel = decay1Target + (envLevel - decay1Target) * decay1Coeff;
            if (envLevel <= decay1Target + 0.002f)
            {
                envLevel = decay1Target;
                // Sustained instruments (bowed/blown) hold at sustain level until noteOff;
                // plucked (harp) and percussion (timbales, celesta) continue decaying.
                if (chars.oscMode == OscMode::Saw
                    || chars.breathNoiseAmount > 0.01f
                    || (chars.oscMode == OscMode::Additive && chars.oddHarmonicBias > 0.01f))
                    envState = EnvState::Sustain;
                else
                    envState = EnvState::Decay2;
            }
            break;

        case EnvState::Sustain:
            // Hold at decay1Target level until noteOff triggers Release
            break;

        case EnvState::Decay2:
            envLevel *= decay2Coeff;
            if (envLevel < 0.0001f)
            {
                envLevel = 0.0f;
                envState = EnvState::Off;
                break;
            }
            break;

        case EnvState::Release:
            envLevel *= releaseCoeff;
            if (envLevel < 0.0001f)
            {
                envLevel = 0.0f;
                envState = EnvState::Off;
                break;
            }
            break;

        case EnvState::Off:
            break;
        }

        if (envState == EnvState::Off)
            break;

        // Compute shaped envelope for output
        float envOut = envLevel;
        if (envState == EnvState::Attack && attackShape > 0.01f)
        {
            float shaped = envLevel * envLevel * (3.0f - 2.0f * envLevel);
            envOut = envLevel + attackShape * (shaped - envLevel);
        }

        // ---- Generate oscillator signal via family hook ----
        float signalL = 0.0f;
        float signalR = 0.0f;

        const SampleContext ctx { pitchMult, chorusMod, envOut, i };
        const float modelTone = modelToneControl(settings);
        const float modelMotion = juce::jlimit(0.0f, 1.0f,
            modelMotionControl(settings) * realtimeVibratoScale + std::abs(chorusMod) * 0.18f);
        const float modelArticulation = modelArticulationControl(settings, chars, vel);
        const auto modelArticulationMode = v2::inferInstrumentArticulation(settings, chars, vel);
        const v2::InstrumentModelFrame modelFrame {
            pitchMult,
            chorusMod,
            envOut,
            i,
            midiNote,
            vel,
            baseFreq,
            fsr,
            juce::jlimit(0.0f, 1.5f, realtimeExpressionGain),
            modelTone,
            modelMotion,
            modelArticulation,
            modelArticulationMode,
            legatoTransitionActive,
            legatoAmountActive,
            legatoTransitionActive ? legatoOnsetScale : 1.0f,
            legatoTransitionActive ? baseFreq * portamentoPitchMult : 0.0f
        };
        renderOscillators(signalL, signalR, ctx);

        // ---- Family-specific transients (bow/breath/pluck) ----
        renderTransients(signalL, signalR, ctx);

        if (instrumentModel != nullptr)
        {
            const float modelLegacyCoreGain = legacyCoreGainOverride >= 0.0f
                ? legacyCoreGainOverride
                : instrumentModel->legacyCoreGain();
            const float legacyCoreGain = juce::jlimit(0.0f, 1.0f, modelLegacyCoreGain);
            signalL *= legacyCoreGain;
            signalR *= legacyCoreGain;
            instrumentModel->renderPreFilter(modelFrame, signalL, signalR);
        }

        // ---- SVF low-pass filter (mono path applied to both channels) ----
        {
            const float monoIn = (signalL + signalR) * 0.5f;
            const float dynamicFilterF = juce::jmin(filterF * realtimeTimbreCutoffScale,
                (-filterQinv + std::sqrt(filterQinv * filterQinv + 4.0f)) * 0.95f);
            const float hp = monoIn - svfLow - filterQinv * svfBand;
            svfBand += dynamicFilterF * hp;
            svfLow  += dynamicFilterF * svfBand;
            if (!(svfBand > 1e-15f || svfBand < -1e-15f)) svfBand = 0.0f;
            if (!(svfLow  > 1e-15f || svfLow  < -1e-15f)) svfLow  = 0.0f;
            if (!std::isfinite(svfBand)) svfBand = 0.0f;
            if (!std::isfinite(svfLow))  svfLow  = 0.0f;

            const float diff = (signalL - signalR) * 0.5f;
            signalL = svfLow + diff;
            signalR = svfLow - diff;
        }

        // ---- Warmth / saturation ----
        {
            const float totalWarmth = chars.builtInWarmth + settings.warmth * 1.0f;
            if (totalWarmth > 0.01f)
            {
                const float drv = 1.0f + totalWarmth;
                const float norm = std::max(0.01f, std::tanh(drv));
                signalL = std::tanh(signalL * drv) / norm;
                signalR = std::tanh(signalR * drv) / norm;
            }
        }

        // ---- Character processing ----
        if (settings.character > 0.01f)
        {
            const float charAmount = settings.character * 0.10f;
            signalL += std::tanh(signalL * 2.0f) * charAmount;
            signalR += std::tanh(signalR * 2.0f) * charAmount;
        }

        // ---- Family-specific color (bloom, body, shimmer, etc.) ----
        renderColor(signalL, signalR, ctx);
        if (instrumentModel != nullptr)
            instrumentModel->renderPostColor(modelFrame, signalL, signalR);
        if (!std::isfinite(signalL)) signalL = 0.0f;
        if (!std::isfinite(signalR)) signalR = 0.0f;

        // ---- Apply envelope, velocity, level ----
        signalL *= envOut * vel * settings.level * realtimeExpressionGain;
        signalR *= envOut * vel * settings.level * realtimeExpressionGain;

        // ---- Stereo pan & accumulate ----
        const int idx = startSample + i;
        float outPanL = panL;
        float outPanR = panR;
        if (std::abs(realtimePanOffset) > 1.0e-5f)
        {
            const float currentPan = panR * panR - panL * panL;
            const float modPan = juce::jlimit(-1.0f, 1.0f, currentPan + realtimePanOffset);
            outPanL = std::sqrt(0.5f * (1.0f - modPan));
            outPanR = std::sqrt(0.5f * (1.0f + modPan));
        }
        left[idx] += signalL * outPanL;
        if (right != nullptr)
            right[idx] += signalR * outPanR;

        ++ageSamples;
        if (ageSamples >= maxAgeSamples)
        {
            if (envState != EnvState::Release)
                envState = EnvState::Release;
            break;
        }
    }
}

} // namespace mos
