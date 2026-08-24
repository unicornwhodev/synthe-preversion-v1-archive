#include "FactoryPresets.h"

#include <algorithm>

namespace mis
{
namespace
{
float clamp01(const float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float scaled(const float value, const float amount, const float lo, const float hi)
{
    return std::clamp(value * amount, lo, hi);
}

int outputBusForInstrument(const int instrumentIndex)
{
    (void) instrumentIndex;
    return 0;
}

const char* presetNameFor(const int instrumentIndex, const bool signature)
{
    switch (instrumentIndex)
    {
        case 0:  return signature ? "Nyckelharpa Signature" : "Nyckelharpa Reference";
        case 1:  return signature ? "Gayageum Signature" : "Gayageum Reference";
        case 2:  return signature ? "Chapman Stick Signature" : "Chapman Stick Reference";
        case 3:  return signature ? "Yayli Tanbur Signature" : "Yayli Tanbur Reference";
        case 4:  return signature ? "Crwth Signature" : "Crwth Reference";
        case 5:  return signature ? "Carnyx Signature" : "Carnyx Reference";
        case 6:  return signature ? "Aulos Signature" : "Aulos Reference";
        case 7:  return signature ? "Fujara Signature" : "Fujara Reference";
        case 8:  return signature ? "Gemshorn Signature" : "Gemshorn Reference";
        case 9:  return signature ? "Dizi Signature" : "Dizi Reference";
        case 10: return signature ? "Angklung Signature" : "Angklung Reference";
        case 11: return signature ? "Udu Signature" : "Udu Reference";
        case 12: return signature ? "Pyeongyeong Signature" : "Pyeongyeong Reference";
        case 13: return signature ? "Cristal Baschet Signature" : "Cristal Baschet Reference";
        case 14: return signature ? "Mbira Signature" : "Mbira Reference";
        case 15: return signature ? "Handpan Signature" : "Handpan Reference";
        case 16: return signature ? "Theremine Signature" : "Theremine Reference";
        case 17: return signature ? "Ondes Martenot Signature" : "Ondes Martenot Reference";
        case 18: return signature ? "Pyrophone Signature" : "Pyrophone Reference";
        case 19: return signature ? "Hydraulophone Signature" : "Hydraulophone Reference";
        case 20: return signature ? "Yaybahar Signature" : "Yaybahar Reference";
        default: return signature ? "Instrument Signature" : "Instrument Reference";
    }
}

InstrumentSettings makeReferenceSettings(const int instrumentIndex)
{
    auto s = getDefaultSettings(instrumentIndex);
    s.tuneSemitones = 0.0f;
    s.level = scaled(s.level, 0.56f, 0.32f, 0.50f);
    s.noiseAmount = clamp01(s.noiseAmount * 0.70f);
    s.drive = std::clamp(s.drive * 0.88f, 1.0f, 1.55f);
    s.pan = 0.0f;

    switch (getFamily(instrumentIndex))
    {
        case Family::Strings:
            s.attackSeconds = std::max(s.attackSeconds, instrumentIndex == 2 ? 0.002f : 0.035f);
            s.releaseSeconds = std::clamp(s.releaseSeconds, 0.18f, 0.80f);
            s.sympathetic = clamp01(s.sympathetic * 0.75f);
            s.bowSpeed = clamp01(s.bowSpeed * 0.95f);
            s.bowPressure = clamp01(s.bowPressure * 0.92f);
            break;

        case Family::Winds:
            s.level = scaled(s.level, 0.52f, 0.30f, 0.45f);
            s.attackSeconds = std::max(s.attackSeconds, 0.018f);
            s.releaseSeconds = std::clamp(s.releaseSeconds, 0.18f, 0.65f);
            s.breathPressure = clamp01(s.breathPressure * 0.90f);
            break;

        case Family::Percussion:
            s.level = scaled(s.level, 0.50f, 0.28f, 0.44f);
            s.sustainLevel = instrumentIndex == 13 ? std::min(s.sustainLevel, 0.32f) : 0.0f;
            s.releaseSeconds = std::clamp(s.releaseSeconds, 0.14f, 1.20f);
            s.strikePosition = clamp01(0.50f + (s.strikePosition - 0.50f) * 0.60f);
            break;

        case Family::Conceptual:
            s.level = scaled(s.level, 0.50f, 0.30f, 0.44f);
            s.releaseSeconds = std::clamp(s.releaseSeconds, 0.18f, 1.00f);
            s.noiseAmount = clamp01(s.noiseAmount * 0.55f);
            break;
    }

    return s;
}

InstrumentSettings makeSignatureSettings(const int instrumentIndex)
{
    auto s = makeReferenceSettings(instrumentIndex);
    s.level = scaled(s.level, 0.94f, 0.26f, 0.48f);
    s.exciter = clamp01(s.exciter + 0.05f);
    s.body = clamp01(s.body + 0.06f);
    s.sympathetic = clamp01(s.sympathetic + 0.05f);
    s.brightness = clamp01(s.brightness + 0.06f);

    switch (getFamily(instrumentIndex))
    {
        case Family::Strings:
            s.attackSeconds = std::min(s.attackSeconds * 1.08f, 0.11f);
            s.releaseSeconds = std::clamp(s.releaseSeconds * 1.18f, 0.25f, 1.00f);
            s.bowSpeed = clamp01(s.bowSpeed + 0.05f);
            s.bowPressure = clamp01(s.bowPressure + 0.04f);
            if (instrumentIndex == 2)
            {
                s.attackSeconds = 0.003f;
                s.decaySeconds = std::clamp(s.decaySeconds * 0.86f, 0.35f, 0.80f);
                s.sustainLevel = std::clamp(s.sustainLevel + 0.06f, 0.20f, 0.52f);
            }
            break;

        case Family::Winds:
            s.breathPressure = clamp01(s.breathPressure + 0.08f);
            s.noiseAmount = clamp01(s.noiseAmount + 0.04f);
            s.cutoffHz = std::clamp(s.cutoffHz * 1.08f, 120.0f, 12000.0f);
            break;

        case Family::Percussion:
            s.exciter = clamp01(s.exciter + 0.07f);
            s.decaySeconds = std::clamp(s.decaySeconds * 1.08f, 0.20f, 4.20f);
            s.releaseSeconds = std::clamp(s.releaseSeconds * 1.12f, 0.18f, 1.50f);
            s.strikePosition = clamp01(s.strikePosition + 0.05f);
            if (instrumentIndex == 13)
                s.sustainLevel = std::clamp(s.sustainLevel + 0.08f, 0.20f, 0.50f);
            break;

        case Family::Conceptual:
            s.releaseSeconds = std::clamp(s.releaseSeconds * 1.22f, 0.25f, 1.30f);
            s.cutoffHz = std::clamp(s.cutoffHz * 1.10f, 120.0f, 14000.0f);
            if (instrumentIndex == 16)
                s.noiseAmount = 0.0f;
            break;
    }

    return s;
}

GlobalFxSettings makeCleanFx()
{
    GlobalFxSettings fx;
    fx.satDrive = 1.15f;
    fx.satMix = 0.0f;
    fx.transientAttack = 0.05f;
    fx.transientSustain = 0.0f;
    fx.transientMix = 0.0f;
    fx.eqLowFreq = 180.0f;
    fx.eqLowGain = 0.0f;
    fx.eqMidFreq = 1100.0f;
    fx.eqMidGain = 0.0f;
    fx.eqMidQ = 0.9f;
    fx.eqHighFreq = 5600.0f;
    fx.eqHighGain = 0.0f;
    fx.compThreshold = -20.0f;
    fx.compRatio = 2.0f;
    fx.compAttack = 16.0f;
    fx.compRelease = 180.0f;
    fx.compMakeup = 0.0f;
    fx.compMix = 0.0f;
    fx.chorusRate = 0.55f;
    fx.chorusDepth = 0.18f;
    fx.chorusMix = 0.0f;
    fx.delayTime = 0.0f;
    fx.delayFeedback = 0.0f;
    fx.delayMix = 0.0f;
    fx.reverbSize = 0.34f;
    fx.reverbDamping = 0.55f;
    fx.reverbWidth = 0.70f;
    fx.reverbMix = 0.018f;
    fx.reverbPredelay = 6.0f;
    fx.limiterThreshold = -3.0f;
    fx.limiterRelease = 80.0f;
    fx.saturatorOn = false;
    fx.transientOn = false;
    fx.eqOn = true;
    fx.compressorOn = false;
    fx.chorusOn = false;
    fx.delayOn = false;
    fx.reverbOn = true;
    fx.limiterOn = true;
    return fx;
}

void zeroDisabledFxValues(GlobalFxSettings& fx)
{
    if (!fx.saturatorOn)
        fx.satMix = 0.0f;
    if (!fx.transientOn)
        fx.transientMix = 0.0f;
    if (!fx.compressorOn)
    {
        fx.compMix = 0.0f;
        fx.compMakeup = 0.0f;
    }
    if (!fx.chorusOn)
        fx.chorusMix = 0.0f;
    if (!fx.reverbOn)
        fx.reverbMix = 0.0f;

    fx.delayOn = false;
    fx.delayTime = 0.0f;
    fx.delayFeedback = 0.0f;
    fx.delayMix = 0.0f;
}

GlobalFxSettings makeReferenceFx(const int instrumentIndex)
{
    auto fx = makeCleanFx();
    const auto family = getFamily(instrumentIndex);

    if (family == Family::Winds)
    {
        fx.eqMidGain = -0.6f;
        fx.eqHighGain = -0.3f;
    }
    else if (family == Family::Percussion)
    {
        fx.eqLowGain = -0.4f;
        fx.eqHighGain = 0.4f;
        fx.reverbMix = 0.014f;
    }
    else if (family == Family::Conceptual)
    {
        fx.eqHighGain = -0.2f;
        fx.reverbMix = 0.020f;
    }

    fx = maskUnavailableFx(instrumentIndex, fx);
    zeroDisabledFxValues(fx);
    return fx;
}

GlobalFxSettings makeSignatureFx(const int instrumentIndex)
{
    auto fx = makeCleanFx();
    const auto family = getFamily(instrumentIndex);
    const auto& availability = getFxAvailability(instrumentIndex);

    fx.reverbSize = 0.42f;
    fx.reverbWidth = 0.78f;
    fx.reverbMix = 0.032f;
    fx.eqLowGain = family == Family::Percussion ? -0.7f : 0.2f;
    fx.eqMidGain = family == Family::Winds ? -0.8f : 0.4f;
    fx.eqHighGain = family == Family::Conceptual ? -0.3f : 0.5f;

    if (availability.saturator && (family == Family::Strings || instrumentIndex == 5 || instrumentIndex == 14 || instrumentIndex == 18))
    {
        fx.saturatorOn = true;
        fx.satDrive = 1.25f;
        fx.satMix = 0.035f;
    }

    if (availability.transient && family != Family::Conceptual)
    {
        fx.transientOn = true;
        fx.transientAttack = family == Family::Percussion ? 0.16f : 0.09f;
        fx.transientSustain = family == Family::Percussion ? -0.04f : 0.0f;
        fx.transientMix = family == Family::Percussion ? 0.12f : 0.08f;
    }
    else if (availability.transient && instrumentIndex == 18)
    {
        fx.transientOn = true;
        fx.transientAttack = 0.10f;
        fx.transientMix = 0.08f;
    }

    if (availability.compressor && (family == Family::Strings || instrumentIndex == 17 || instrumentIndex == 18))
    {
        fx.compressorOn = true;
        fx.compThreshold = -21.0f;
        fx.compRatio = 1.8f;
        fx.compAttack = 18.0f;
        fx.compRelease = 190.0f;
        fx.compMakeup = 0.2f;
        fx.compMix = 0.16f;
    }

    if (availability.chorus && (instrumentIndex == 2 || instrumentIndex == 8 || instrumentIndex == 9 ||
                                instrumentIndex == 13 || instrumentIndex == 16 || instrumentIndex == 17 ||
                                instrumentIndex == 19 || instrumentIndex == 20))
    {
        fx.chorusOn = true;
        fx.chorusRate = instrumentIndex == 16 ? 0.35f : 0.60f;
        fx.chorusDepth = instrumentIndex == 16 ? 0.12f : 0.20f;
        fx.chorusMix = instrumentIndex == 16 ? 0.035f : 0.055f;
    }

    fx = maskUnavailableFx(instrumentIndex, fx);
    zeroDisabledFxValues(fx);
    return fx;
}

PerformanceSettings makePerformance(const bool signature)
{
    PerformanceSettings p;
    p.lfoRate = signature ? 1.6f : 1.2f;
    p.lfoDepth = signature ? 0.015f : 0.0f;
    p.lfoWave = 0;
    p.macroWarmth = signature ? 0.54f : 0.50f;
    p.macroBrightness = signature ? 0.54f : 0.50f;
    p.macroExpression = signature ? 0.58f : 0.50f;
    p.macroTexture = signature ? 0.34f : 0.28f;
    return p;
}

float nominalPeakFor(const int instrumentIndex, const bool signature)
{
    static constexpr float kNominalPeaks[kNumInstruments][2] = {
        { -10.0f, -10.6f }, // Nyckelharpa
        { -21.6f, -21.6f }, // Gayageum
        { -12.8f, -14.2f }, // Chapman Stick
        { -11.7f, -12.1f }, // Yayli Tanbur
        { -10.8f, -11.2f }, // Crwth
        { -14.8f, -15.1f }, // Carnyx
        { -16.5f, -16.3f }, // Aulos
        { -18.5f, -18.1f }, // Fujara
        { -17.3f, -17.2f }, // Gemshorn
        { -15.7f, -15.9f }, // Dizi
        { -25.4f, -25.6f }, // Angklung
        { -38.3f, -33.2f }, // Udu
        { -31.7f, -25.9f }, // Pyeongyeong
        { -8.9f,  -1.2f  }, // Cristal Baschet
        { -13.9f, -14.8f }, // Mbira
        { -35.1f, -21.0f }, // Handpan
        { -15.4f, -16.0f }, // Theremine
        { -15.3f, -16.0f }, // Ondes Martenot
        { -15.7f, -16.3f }, // Pyrophone
        { -15.6f, -16.1f }, // Hydraulophone
        { -14.8f, -15.2f }, // Yaybahar
    };

    return kNominalPeaks[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))]
                        [signature ? 1u : 0u];
}

InstrumentPreset makePreset(const int instrumentIndex, const bool signature)
{
    InstrumentPreset preset;
    preset.name = presetNameFor(instrumentIndex, signature);
    preset.settings = signature ? makeSignatureSettings(instrumentIndex)
                                : makeReferenceSettings(instrumentIndex);
    preset.fx = signature ? makeSignatureFx(instrumentIndex)
                          : makeReferenceFx(instrumentIndex);
    preset.outputBus = outputBusForInstrument(instrumentIndex);
    preset.performance = makePerformance(signature);
    preset.nominalPeakDb = nominalPeakFor(instrumentIndex, signature);
    return preset;
}

std::array<std::vector<InstrumentPreset>, kNumInstruments> buildFactoryBanks()
{
    std::array<std::vector<InstrumentPreset>, kNumInstruments> banks;
    for (int instrumentIndex = 0; instrumentIndex < kNumInstruments; ++instrumentIndex)
    {
        auto& bank = banks[static_cast<std::size_t>(instrumentIndex)];
        bank.reserve(2);
        bank.push_back(makePreset(instrumentIndex, false));
        bank.push_back(makePreset(instrumentIndex, true));
    }
    return banks;
}
} // namespace

const std::array<std::vector<InstrumentPreset>, kNumInstruments>& getFactoryPresetBanks()
{
    static const auto banks = buildFactoryBanks();
    return banks;
}

std::size_t getTotalFactoryPresetCount()
{
    return static_cast<std::size_t>(kNumInstruments) * 2u;
}

} // namespace mis
