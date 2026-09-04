#include "FactoryPresets.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace mos
{

namespace
{
void addTag(std::vector<std::string>& tags, const std::string& tag)
{
    if (tag.empty())
        return;

    if (std::find(tags.begin(), tags.end(), tag) == tags.end())
        tags.push_back(tag);
}

std::string toAsciiLower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(),
        [] (unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

const char* familyLabelForInstrument(const int instrIndex) noexcept
{
    switch (getFamily(instrIndex))
    {
        case Family::Cordes:      return "strings";
        case Family::Bois:        return "woodwinds";
        case Family::Cuivres:     return "brass";
        case Family::Percussions: return "percussion";
    }

    return "orch";
}

const char* instrumentTagForIndex(const int instrIndex) noexcept
{
    static constexpr const char* tags[] = {
        "violin", "viola", "cello", "contrabass", "harp",
        "flute", "oboe", "clarinet", "bassoon", "piccolo", "english-horn", "bass-clarinet",
        "horn", "trumpet", "trombone", "tuba",
        "timpani", "celesta", "snare", "xylophone"
    };

    if (instrIndex < 0 || instrIndex >= static_cast<int>(std::size(tags)))
        return "orch";

    return tags[static_cast<std::size_t>(instrIndex)];
}

std::string humanizeSlug(std::string slug)
{
    std::replace(slug.begin(), slug.end(), '_', ' ');
    bool upperNext = true;
    for (auto& ch : slug)
    {
        if (ch == ' ')
        {
            upperNext = true;
            continue;
        }

        if (upperNext)
        {
            ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            upperNext = false;
        }
    }
    return slug;
}

int defaultOutputBusForRole(const std::string& mixRole)
{
    if (mixRole == "accent-layer" || mixRole == "short-articulation")
        return 1;
    if (mixRole == "trailer-layer" || mixRole == "cinematic-section")
        return 2;
    if (mixRole == "atmospheric-layer" || mixRole == "stereo-layer")
        return 3;
    if (mixRole == "low-mid-support")
        return 4;
    return 0;
}

std::string outputProfileFor(const std::string& mixRole, const int outputBus)
{
    if (mixRole == "short-articulation" || mixRole == "accent-layer")
        return outputBus > 0 ? "main-plus-aux1-articulation" : "main-articulation";
    if (mixRole == "trailer-layer" || mixRole == "cinematic-section")
        return outputBus > 0 ? "main-plus-aux2-score" : "main-score";
    if (mixRole == "atmospheric-layer" || mixRole == "stereo-layer")
        return outputBus > 0 ? "main-plus-aux3-space" : "main-space";
    if (mixRole == "low-mid-support")
        return outputBus > 0 ? "main-plus-aux4-support" : "main-support";
    return "main-core";
}

std::string descriptionFor(const std::string& instrumentLabel, const std::string& mixRole)
{
    if (mixRole == "solo-core")
        return instrumentLabel + " primary solo orchestral-synth preset for balanced writing, harmonic clarity and controlled doubling.";
    if (mixRole == "engine-dry")
        return instrumentLabel + " dry solo engine-evaluation preset for direct V2 algorithm listening without spatial FX or doubled width.";
    if (mixRole == "trailer-layer")
        return instrumentLabel + " secondary solo trailer layer for reinforced doubles, trailer lifts and bold overlays.";
    if (mixRole == "cinematic-section")
        return instrumentLabel + " secondary solo score layer with controlled bloom and restrained cinematic spread.";
    if (mixRole == "atmospheric-layer")
        return instrumentLabel + " secondary solo atmospheric layer for restrained washes, drones and orchestral-synth glue.";
    if (mixRole == "short-articulation")
        return instrumentLabel + " primary solo short preset for precise attacks, readable pulses and rhythmic writing.";
    if (mixRole == "accent-layer")
        return instrumentLabel + " primary solo accent preset for marcato-style attacks, defined phrasing and orchestral emphasis.";
    if (mixRole == "low-mid-support")
        return instrumentLabel + " primary solo darker preset for restrained weight, body and controlled arrangement depth.";
    if (mixRole == "stereo-layer")
        return instrumentLabel + " secondary solo spread layer for panoramic doubles and controlled section widening.";
    if (mixRole == "support-layer")
        return instrumentLabel + " primary solo softer preset for restrained dynamics, blend and supportive orchestral lines.";
    return instrumentLabel + " orchestral-synth preset.";
}

bool isSecondaryMixRole(const std::string& mixRole) noexcept
{
    return mixRole == "trailer-layer"
        || mixRole == "cinematic-section"
        || mixRole == "atmospheric-layer"
        || mixRole == "stereo-layer";
}

PresetMetadata makeMetadata(const int instrIndex, const std::string& presetName)
{
    PresetMetadata metadata;
    metadata.familyLabel = familyLabelForInstrument(instrIndex);
    addTag(metadata.tags, "solo");
    addTag(metadata.tags, "single-voice");

    const auto presetNameLower = toAsciiLower(presetName);
    if (presetNameLower.find("engine dry") != std::string::npos)
    {
        metadata.mixRole = "engine-dry";
        metadata.nominalPeakDb = -13.0f;
        addTag(metadata.tags, "engine-evaluation");
        addTag(metadata.tags, "dry-core");
        addTag(metadata.tags, "v2-audit");
        addTag(metadata.tags, "no-fx");
    }
    else if (presetNameLower.find("staccato") != std::string::npos)
    {
        metadata.mixRole = "short-articulation";
        metadata.nominalPeakDb = -10.0f;
        addTag(metadata.tags, "staccato");
        addTag(metadata.tags, "short");
        addTag(metadata.tags, "transient");
    }
    else if (presetNameLower.find("marcato") != std::string::npos)
    {
        metadata.mixRole = "accent-layer";
        metadata.nominalPeakDb = -9.5f;
        addTag(metadata.tags, "marcato");
        addTag(metadata.tags, "accent");
        addTag(metadata.tags, "defined-attack");
    }
    else if (presetNameLower.find("trailer") != std::string::npos)
    {
        metadata.mixRole = "trailer-layer";
        metadata.nominalPeakDb = -9.0f;
        addTag(metadata.tags, "trailer");
        addTag(metadata.tags, "wide");
        addTag(metadata.tags, "bold");
    }
    else if (presetNameLower.find("cinematic") != std::string::npos)
    {
        metadata.mixRole = "cinematic-section";
        metadata.nominalPeakDb = -10.5f;
        addTag(metadata.tags, "cinematic");
        addTag(metadata.tags, "score");
        addTag(metadata.tags, "space");
    }
    else if (presetNameLower.find("ambient") != std::string::npos)
    {
        metadata.mixRole = "atmospheric-layer";
        metadata.nominalPeakDb = -13.5f;
        addTag(metadata.tags, "ambient");
        addTag(metadata.tags, "wash");
        addTag(metadata.tags, "air");
    }
    else if (presetNameLower.find("soft") != std::string::npos)
    {
        metadata.mixRole = "support-layer";
        metadata.nominalPeakDb = -14.0f;
        addTag(metadata.tags, "soft");
        addTag(metadata.tags, "mellow");
        addTag(metadata.tags, "pp");
    }
    else if (presetNameLower.find("dark") != std::string::npos)
    {
        metadata.mixRole = "low-mid-support";
        metadata.nominalPeakDb = -12.5f;
        addTag(metadata.tags, "dark");
        addTag(metadata.tags, "warm");
        addTag(metadata.tags, "reduced-brightness");
    }
    else if (presetNameLower.find("wide") != std::string::npos)
    {
        metadata.mixRole = "stereo-layer";
        metadata.nominalPeakDb = -10.5f;
        addTag(metadata.tags, "wide");
        addTag(metadata.tags, "spread");
        addTag(metadata.tags, "stereo");
    }
    else
    {
        metadata.mixRole = "solo-core";
        metadata.nominalPeakDb = -12.0f;
        addTag(metadata.tags, "concert");
        addTag(metadata.tags, "balanced");
        addTag(metadata.tags, "sustain");
    }

    addTag(metadata.tags, metadata.familyLabel);
    addTag(metadata.tags, instrumentTagForIndex(instrIndex));
    metadata.outputProfile = outputProfileFor(metadata.mixRole, defaultOutputBusForRole(metadata.mixRole));
    metadata.description = descriptionFor(humanizeSlug(instrumentTagForIndex(instrIndex)), metadata.mixRole);
    addTag(metadata.tags, metadata.outputProfile);
    addTag(metadata.tags, isSecondaryMixRole(metadata.mixRole) ? "secondary-line" : "core-line");
    addTag(metadata.tags, isSecondaryMixRole(metadata.mixRole) ? "texture-role" : "primary-role");

    if (getFamily(instrIndex) == Family::Percussions)
    {
        addTag(metadata.tags, "modal");
        addTag(metadata.tags, instrIndex == 18 ? "unpitched" : "pitched");
    }
    else
    {
        addTag(metadata.tags, "pitched");
    }

    if (getFamily(instrIndex) == Family::Cuivres)
        metadata.nominalPeakDb += 1.0f;
    else if (getFamily(instrIndex) == Family::Bois)
        metadata.nominalPeakDb -= 0.5f;
    else if (getFamily(instrIndex) == Family::Percussions)
        metadata.nominalPeakDb += 0.5f;

    metadata.nominalPeakDb = std::clamp(metadata.nominalPeakDb, -24.0f, -1.0f);
    return metadata;
}

InstrumentPreset makePreset(std::string name, const InstrSettings& settings,
                            const GlobalFxSettings& fx = {})
{
    return { std::move(name), settings, fx };
}

GlobalFxSettings makeCinematicFx()
{
    GlobalFxSettings fx;
    fx.satDrive = 1.2f;           fx.satMix = 0.04f;
    fx.transientAttack  = 0.18f;  fx.transientSustain = 0.05f;  fx.transientMix = 0.08f;
    fx.eqLowFreq = 220.0f;       fx.eqLowGain  =  2.0f;
    fx.eqMidFreq = 1200.0f;      fx.eqMidGain  = -1.5f;        fx.eqMidQ = 0.8f;
    fx.eqHighFreq = 5600.0f;     fx.eqHighGain =  0.3f;
    fx.compThreshold = -12.0f;    fx.compRatio = 1.8f;
    fx.compAttack = 20.0f;        fx.compRelease = 300.0f;      fx.compMix = 1.0f;
    fx.chorusRate = 0.42f;        fx.chorusDepth = 0.22f;        fx.chorusMix = 0.08f;
    fx.delayTime = 350.0f;        fx.delayFeedback = 0.18f;      fx.delayMix = 0.08f;
    fx.reverbSize = 0.52f;        fx.reverbDamping = 0.55f;
    fx.reverbWidth = 0.92f;       fx.reverbMix = 0.22f;          fx.reverbPredelay = 18.0f;
    fx.limiterThreshold = -1.0f;  fx.limiterRelease = 300.0f;
    return fx;
}

GlobalFxSettings makeAmbientFx()
{
    GlobalFxSettings fx;
    fx.satDrive = 1.2f;           fx.satMix = 0.03f;
    fx.transientAttack  = 0.30f;  fx.transientSustain = 0.50f;  fx.transientMix = 0.10f;
    fx.eqLowFreq = 180.0f;       fx.eqLowGain  =  1.5f;
    fx.eqMidFreq = 1400.0f;      fx.eqMidGain  = -2.0f;        fx.eqMidQ = 0.6f;
    fx.eqHighFreq = 4300.0f;     fx.eqHighGain = -2.0f;
    fx.compThreshold = -12.0f;    fx.compRatio = 2.0f;
    fx.compAttack = 50.0f;        fx.compRelease = 300.0f;      fx.compMix = 0.8f;
    fx.chorusRate = 0.24f;        fx.chorusDepth = 0.28f;        fx.chorusMix = 0.12f;
    fx.delayTime = 500.0f;        fx.delayFeedback = 0.28f;      fx.delayMix = 0.14f;
    fx.reverbSize = 0.68f;        fx.reverbDamping = 0.60f;
    fx.reverbWidth = 0.94f;       fx.reverbMix = 0.30f;          fx.reverbPredelay = 45.0f;
    fx.limiterThreshold = -1.0f;  fx.limiterRelease = 150.0f;
    return fx;
}

GlobalFxSettings makeStaccatoFx()
{
    GlobalFxSettings fx;
    fx.satDrive = 1.4f;           fx.satMix = 0.06f;
    fx.transientAttack  = 0.45f;  fx.transientSustain = 0.10f;  fx.transientMix = 0.14f;
    fx.eqLowFreq = 200.0f;       fx.eqLowGain  = -0.5f;
    fx.eqMidFreq = 2200.0f;      fx.eqMidGain  =  0.4f;        fx.eqMidQ = 1.0f;
    fx.eqHighFreq = 6400.0f;     fx.eqHighGain =  0.5f;
    fx.compThreshold = -15.0f;    fx.compRatio = 2.5f;
    fx.compAttack = 5.0f;         fx.compRelease = 80.0f;       fx.compMix = 1.0f;
    fx.chorusRate = 0.50f;        fx.chorusDepth = 0.10f;        fx.chorusMix = 0.0f;
    fx.delayTime = 150.0f;        fx.delayFeedback = 0.10f;      fx.delayMix = 0.05f;
    fx.reverbSize = 0.32f;        fx.reverbDamping = 0.68f;
    fx.reverbWidth = 0.56f;       fx.reverbMix = 0.16f;          fx.reverbPredelay = 5.0f;
    fx.limiterThreshold = -1.0f;  fx.limiterRelease = 120.0f;
    return fx;
}

GlobalFxSettings makeEngineDryFx()
{
    GlobalFxSettings fx;
    fx.satDrive = 1.0f;           fx.satMix = 0.0f;
    fx.transientAttack = 0.0f;    fx.transientSustain = 0.0f;    fx.transientMix = 0.0f;
    fx.eqLowGain = 0.0f;          fx.eqMidGain = 0.0f;           fx.eqHighGain = 0.0f;
    fx.compThreshold = 0.0f;      fx.compRatio = 1.0f;           fx.compMix = 0.0f;
    fx.chorusDepth = 0.0f;        fx.chorusMix = 0.0f;
    fx.delayFeedback = 0.0f;      fx.delayMix = 0.0f;
    fx.reverbSize = 0.0f;         fx.reverbDamping = 0.50f;
    fx.reverbWidth = 0.50f;       fx.reverbMix = 0.0f;           fx.reverbPredelay = 0.0f;
    fx.limiterThreshold = -0.5f;  fx.limiterRelease = 120.0f;
    return fx;
}

GlobalFxSettings makeWideFx(GlobalFxSettings fx)
{
    fx.chorusRate = std::clamp(fx.chorusRate + 0.08f, 0.1f, 5.0f);
    fx.chorusDepth = std::clamp(fx.chorusDepth + 0.10f, 0.0f, 1.0f);
    fx.chorusMix = std::clamp(fx.chorusMix + 0.06f, 0.0f, 1.0f);
    fx.delayTime = std::clamp(fx.delayTime + 65.0f, 1.0f, 2000.0f);
    fx.delayFeedback = std::clamp(fx.delayFeedback + 0.05f, 0.0f, 0.95f);
    fx.delayMix = std::clamp(fx.delayMix + 0.04f, 0.0f, 1.0f);
    fx.reverbSize = std::clamp(fx.reverbSize + 0.08f, 0.0f, 1.0f);
    fx.reverbWidth = std::clamp(fx.reverbWidth + 0.10f, 0.0f, 1.0f);
    fx.reverbMix = std::clamp(fx.reverbMix + 0.04f, 0.0f, 1.0f);
    fx.reverbPredelay = std::clamp(fx.reverbPredelay + 10.0f, 0.0f, 100.0f);
    return fx;
}

GlobalFxSettings makeMarcatoFx(GlobalFxSettings fx)
{
    fx.satDrive = std::clamp(fx.satDrive + 0.4f, 1.0f, 16.0f);
    fx.satMix = std::clamp(fx.satMix + 0.03f, 0.0f, 1.0f);
    fx.transientAttack = std::clamp(fx.transientAttack + 0.12f, -1.0f, 1.0f);
    fx.transientSustain = std::clamp(fx.transientSustain - 0.06f, -1.0f, 1.0f);
    fx.transientMix = std::clamp(fx.transientMix + 0.06f, 0.0f, 1.0f);
    fx.eqMidGain = std::clamp(fx.eqMidGain + 1.0f, -12.0f, 12.0f);
    fx.eqHighGain = std::clamp(fx.eqHighGain + 0.3f, -12.0f, 12.0f);
    fx.compThreshold = std::clamp(fx.compThreshold - 2.0f, -60.0f, 0.0f);
    fx.compAttack = std::clamp(fx.compAttack * 0.75f, 0.1f, 100.0f);
    fx.compRelease = std::clamp(fx.compRelease * 0.85f, 5.0f, 500.0f);
    fx.reverbMix = std::clamp(fx.reverbMix * 0.85f, 0.0f, 1.0f);
    return fx;
}

InstrSettings makeTrailerSettings(InstrSettings settings,
                                 const float brightnessBoost,
                                 const float attackScale,
                                 const float decayScale,
                                 const float releaseScale,
                                 const float vibratoBoost,
                                 const float warmthBoost,
                                 const float detuneBoost,
                                 const float stereoBoost,
                                 const float characterBoost,
                                 const float cutoffScale,
                                 const float levelBoost)
{
    settings.brightness = std::clamp(settings.brightness + brightnessBoost, 0.0f, 1.0f);
    settings.attackSeconds = std::clamp(settings.attackSeconds * attackScale, 0.0f, 2.0f);
    settings.decaySeconds *= decayScale;
    settings.releaseSeconds *= releaseScale;
    settings.vibrato = std::clamp(settings.vibrato + vibratoBoost, 0.0f, 1.0f);
    settings.warmth = std::clamp(settings.warmth + warmthBoost, 0.0f, 1.0f);
    settings.detune = std::clamp(settings.detune + detuneBoost, 0.0f, 1.0f);
    settings.stereoWidth = std::clamp(settings.stereoWidth + stereoBoost, 0.0f, 1.0f);
    settings.character = std::clamp(settings.character + characterBoost, 0.0f, 1.0f);
    settings.cutoffHz = std::clamp(settings.cutoffHz * cutoffScale, 120.0f, 16000.0f);
    settings.level = std::clamp(settings.level + levelBoost, 0.0f, 1.0f);
    return settings;
}

InstrSettings makeStaccatoSettings(InstrSettings settings)
{
    settings.attackSeconds  = std::clamp(settings.attackSeconds * 0.20f, 0.001f, 0.04f);
    settings.decaySeconds  *= 0.50f;
    settings.sustainLevel   = std::clamp(settings.sustainLevel * 0.40f, 0.0f, 1.0f);
    settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.35f, 0.02f, 0.5f);
    settings.vibrato       *= 0.20f;
    settings.brightness     = std::clamp(settings.brightness + 0.08f, 0.0f, 1.0f);
    settings.cutoffHz       = std::clamp(settings.cutoffHz * 1.20f, 120.0f, 16000.0f);
    return settings;
}

InstrSettings makeEngineDrySettings(const int instrIndex, InstrSettings settings)
{
    const auto family = getFamily(instrIndex);
    settings.level = std::clamp(settings.level - 0.02f, 0.0f, 1.0f);
    settings.detune = 0.0f;
    settings.stereoWidth = std::min(settings.stereoWidth, 0.04f);
    settings.pan = 0.0f;
    settings.tuneSemitones = 0.0f;
    settings.vibrato = std::min(settings.vibrato, 0.16f);

    if (family == Family::Cordes)
    {
        const bool isHarp = instrIndex == 4;
        settings.attackSeconds = std::clamp(settings.attackSeconds * (isHarp ? 0.36f : 0.78f),
                                            0.001f,
                                            isHarp ? 0.018f : 0.095f);
        settings.decaySeconds = std::clamp(settings.decaySeconds * (isHarp ? 1.36f : 1.08f), 0.08f, 8.0f);
        settings.sustainLevel = std::clamp(isHarp ? settings.sustainLevel * 0.62f
                                                   : settings.sustainLevel + 0.08f,
                                            0.0f,
                                            isHarp ? 0.42f : 0.86f);
        settings.releaseSeconds = std::clamp(settings.releaseSeconds * (isHarp ? 1.18f : 0.95f), 0.04f, 2.0f);
        settings.brightness = std::clamp(settings.brightness + (isHarp ? 0.08f : 0.06f), 0.0f, isHarp ? 0.66f : 0.72f);
        settings.character = std::clamp(settings.character + (isHarp ? 0.12f : 0.10f), 0.0f, 0.82f);
        settings.warmth = std::clamp(settings.warmth + (instrIndex == 3 ? 0.08f : 0.04f), 0.0f, 0.72f);
        settings.vibrato = isHarp ? 0.0f : std::min(settings.vibrato, 0.10f);
        settings.cutoffHz = std::clamp(settings.cutoffHz * (isHarp ? 1.08f : 1.02f), 120.0f, isHarp ? 9200.0f : 8600.0f);
    }
    else if (family == Family::Bois)
    {
        const bool isPiccolo = instrIndex == 9;
        const bool isDoubleReed = instrIndex == 6 || instrIndex == 10 || instrIndex == 8;
        const bool isLowWoodwind = instrIndex == 8 || instrIndex == 11;
        settings.attackSeconds = std::clamp(settings.attackSeconds * (isPiccolo ? 0.62f : 0.82f),
                                            0.004f,
                                            isPiccolo ? 0.040f : 0.070f);
        settings.decaySeconds = std::clamp(settings.decaySeconds * 1.10f, 0.12f, 6.0f);
        settings.sustainLevel = std::clamp(settings.sustainLevel + (isLowWoodwind ? 0.10f : 0.08f), 0.0f, 0.82f);
        settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.96f, 0.06f, 1.20f);
        settings.brightness = std::clamp(settings.brightness + (isPiccolo ? 0.12f : isDoubleReed ? -0.02f : 0.04f),
                                         0.0f,
                                         isPiccolo ? 0.72f : isDoubleReed ? 0.46f : 0.62f);
        settings.character = std::clamp(settings.character + (isDoubleReed ? 0.10f : 0.08f), 0.0f, 0.78f);
        settings.warmth = std::clamp(settings.warmth + (isLowWoodwind ? 0.10f : isDoubleReed ? 0.06f : 0.02f), 0.0f, 0.58f);
        settings.vibrato = std::min(settings.vibrato, isPiccolo ? 0.10f : 0.12f);
        settings.cutoffHz = std::clamp(settings.cutoffHz,
                                       120.0f,
                                       isPiccolo ? 9800.0f : isDoubleReed ? 5200.0f : isLowWoodwind ? 4200.0f : 7600.0f);
    }
    else if (family == Family::Cuivres)
    {
        const bool isTrumpet = instrIndex == 13;
        const bool isTuba = instrIndex == 15;
        settings.attackSeconds = std::clamp(settings.attackSeconds * (isTrumpet ? 0.58f : isTuba ? 0.88f : 0.76f),
                                            0.006f,
                                            isTuba ? 0.110f : 0.080f);
        settings.decaySeconds = std::clamp(settings.decaySeconds * 1.12f, 0.14f, 7.0f);
        settings.sustainLevel = std::clamp(settings.sustainLevel + (isTuba ? 0.12f : 0.09f), 0.0f, 0.84f);
        settings.releaseSeconds = std::clamp(settings.releaseSeconds * 1.04f, 0.08f, 1.60f);
        settings.brightness = std::clamp(settings.brightness + (isTrumpet ? 0.08f : isTuba ? -0.03f : 0.03f),
                                         0.0f,
                                         isTuba ? 0.48f : 0.70f);
        settings.character = std::clamp(settings.character + 0.14f, 0.0f, 0.84f);
        settings.warmth = std::clamp(settings.warmth + (isTuba ? 0.14f : 0.08f), 0.0f, 0.78f);
        settings.vibrato = std::min(settings.vibrato, 0.08f);
        settings.cutoffHz = std::clamp(settings.cutoffHz * (isTuba ? 0.88f : 1.02f), 120.0f, isTuba ? 5600.0f : 9000.0f);
    }
    else
    {
        const bool isSnare = instrIndex == 18;
        const bool isXylophone = instrIndex == 19;
        settings.attackSeconds = std::clamp(settings.attackSeconds * 0.46f, 0.001f, 0.018f);
        settings.decaySeconds = std::clamp(settings.decaySeconds * (isSnare || isXylophone ? 0.82f : 1.12f), 0.06f, 4.0f);
        settings.sustainLevel = std::clamp(settings.sustainLevel * (isSnare || isXylophone ? 0.58f : 0.76f), 0.0f, 0.38f);
        settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.72f, 0.02f, 0.90f);
        settings.brightness = std::clamp(settings.brightness + (isXylophone ? 0.10f : isSnare ? 0.04f : 0.02f), 0.0f, 0.78f);
        settings.character = std::clamp(settings.character + (isSnare ? 0.14f : 0.10f), 0.0f, 0.82f);
        settings.warmth = std::clamp(settings.warmth + (instrIndex == 16 ? 0.08f : 0.02f), 0.0f, 0.52f);
        settings.vibrato = 0.0f;
        settings.cutoffHz = std::clamp(settings.cutoffHz * (isXylophone ? 1.10f : 0.98f), 120.0f, isXylophone ? 10500.0f : 7800.0f);
    }
    return settings;
}

InstrSettings makeSoftSettings(InstrSettings settings)
{
    settings.level = std::clamp(settings.level - 0.08f, 0.0f, 1.0f);
    settings.brightness = std::clamp(settings.brightness - 0.18f, 0.0f, 1.0f);
    settings.attackSeconds = std::clamp(settings.attackSeconds * 1.45f, 0.001f, 2.0f);
    settings.decaySeconds *= 1.10f;
    settings.sustainLevel = std::clamp(settings.sustainLevel + 0.08f, 0.0f, 1.0f);
    settings.releaseSeconds = std::clamp(settings.releaseSeconds * 1.20f, 0.02f, 8.0f);
    settings.vibrato = std::clamp(settings.vibrato + 0.04f, 0.0f, 1.0f);
    settings.warmth = std::clamp(settings.warmth + 0.10f, 0.0f, 1.0f);
    settings.character = std::clamp(settings.character - 0.05f, 0.0f, 1.0f);
    settings.cutoffHz = std::clamp(settings.cutoffHz * 0.82f, 120.0f, 16000.0f);
    return settings;
}

InstrSettings makeDarkSettings(InstrSettings settings)
{
    settings.level = std::clamp(settings.level - 0.04f, 0.0f, 1.0f);
    settings.brightness = std::clamp(settings.brightness - 0.24f, 0.0f, 1.0f);
    settings.warmth = std::clamp(settings.warmth + 0.16f, 0.0f, 1.0f);
    settings.detune = std::clamp(settings.detune + 0.04f, 0.0f, 1.0f);
    settings.character = std::clamp(settings.character + 0.08f, 0.0f, 1.0f);
    settings.cutoffHz = std::clamp(settings.cutoffHz * 0.68f, 120.0f, 16000.0f);
    return settings;
}

InstrSettings makeWideSettings(InstrSettings settings)
{
    settings.level = std::clamp(settings.level - 0.03f, 0.0f, 1.0f);
    settings.brightness = std::clamp(settings.brightness + 0.05f, 0.0f, 1.0f);
    settings.releaseSeconds = std::clamp(settings.releaseSeconds * 1.18f, 0.02f, 8.0f);
    settings.vibrato = std::clamp(settings.vibrato + 0.05f, 0.0f, 1.0f);
    settings.detune = std::clamp(settings.detune + 0.10f, 0.0f, 1.0f);
    settings.stereoWidth = std::clamp(settings.stereoWidth + 0.24f, 0.0f, 1.0f);
    settings.cutoffHz = std::clamp(settings.cutoffHz * 1.06f, 120.0f, 16000.0f);
    return settings;
}

InstrSettings makeMarcatoSettings(InstrSettings settings)
{
    settings.level = std::clamp(settings.level + 0.03f, 0.0f, 1.0f);
    settings.brightness = std::clamp(settings.brightness + 0.14f, 0.0f, 1.0f);
    settings.attackSeconds = std::clamp(settings.attackSeconds * 0.35f, 0.001f, 0.20f);
    settings.decaySeconds *= 0.82f;
    settings.sustainLevel = std::clamp(settings.sustainLevel * 0.74f, 0.0f, 1.0f);
    settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.80f, 0.02f, 8.0f);
    settings.character = std::clamp(settings.character + 0.18f, 0.0f, 1.0f);
    settings.cutoffHz = std::clamp(settings.cutoffHz * 1.12f, 120.0f, 16000.0f);
    return settings;
}

void applyFamilyRoleProfile(int instrIndex,
                            const std::string& presetName,
                            InstrSettings& settings,
                            GlobalFxSettings& fx)
{
    const auto family = getFamily(instrIndex);
    const auto lowerName = toAsciiLower(presetName);
    const bool isHarp = instrIndex == 4;
    const bool isTimpani = instrIndex == 16;
    const bool isCelesta = instrIndex == 17;
    const bool isSoft = lowerName.find("soft") != std::string::npos;
    const bool isDark = lowerName.find("dark") != std::string::npos;
    const bool isWide = lowerName.find("wide") != std::string::npos;
    const bool isMarcato = lowerName.find("marcato") != std::string::npos;
    const bool isStaccato = lowerName.find("staccato") != std::string::npos;
    const bool isTrailer = lowerName.find("trailer") != std::string::npos;
    const bool isCinematic = lowerName.find("cinematic") != std::string::npos;
    const bool isAmbient = lowerName.find("ambient") != std::string::npos;
    const bool isSecondary = isWide || isTrailer || isCinematic || isAmbient;

    if (isSecondary)
    {
        float levelTrim = isAmbient ? 0.10f
                         : isTrailer ? 0.07f
                         : isCinematic ? 0.05f
                         : 0.04f;

        if (family == Family::Cuivres || family == Family::Percussions)
            levelTrim += 0.02f;

        settings.level = std::clamp(settings.level - levelTrim, 0.0f, 1.0f);
    }

    if (isHarp)
    {
        settings.vibrato = 0.0f;
        settings.detune = std::clamp(settings.detune, 0.0f, 0.01f);
        settings.stereoWidth = std::clamp(settings.stereoWidth, 0.0f, isWide ? 0.20f : 0.14f);
        if (isSoft)
        {
            settings.level = std::clamp(settings.level - 0.04f, 0.0f, 1.0f);
            settings.brightness = std::clamp(settings.brightness - 0.04f, 0.0f, 1.0f);
        }
        if (isDark)
        {
            settings.warmth = std::clamp(settings.warmth + 0.04f, 0.0f, 1.0f);
            settings.cutoffHz = std::clamp(settings.cutoffHz * 0.82f, 120.0f, 16000.0f);
        }
        if (isStaccato)
        {
            settings.decaySeconds = std::clamp(settings.decaySeconds * 0.64f, 0.02f, 8.0f);
            settings.sustainLevel = std::clamp(settings.sustainLevel * 0.72f, 0.0f, 1.0f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.60f, 0.02f, 0.80f);
        }
        if (isMarcato)
        {
            settings.brightness = std::clamp(settings.brightness + 0.08f, 0.0f, 1.0f);
            settings.attackSeconds = std::clamp(settings.attackSeconds * 0.55f, 0.001f, 0.06f);
            settings.decaySeconds = std::clamp(settings.decaySeconds * 0.78f, 0.02f, 8.0f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.74f, 0.02f, 1.10f);
        }
        if (isSecondary)
        {
            fx.chorusMix = 0.0f;
            fx.delayMix = std::min(fx.delayMix, 0.08f);
            fx.reverbMix = std::min(fx.reverbMix, isAmbient ? 0.30f : 0.24f);
        }
        return;
    }

    if (family == Family::Cordes)
    {
        const bool lowStringAnchor = instrIndex == 3;
        settings.detune = std::clamp(settings.detune, 0.0f, lowStringAnchor ? (isWide ? 0.02f : 0.01f)
                                                                               : (isWide ? 0.12f : 0.07f));
        settings.stereoWidth = std::clamp(settings.stereoWidth, 0.0f, lowStringAnchor ? (isWide ? 0.14f : 0.08f)
                                                                                         : (isWide ? 0.30f : 0.18f));
        if (isSoft)
        {
            settings.attackSeconds = std::clamp(settings.attackSeconds * 1.12f, 0.001f, 2.0f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 1.12f, 0.02f, 8.0f);
            settings.brightness = std::clamp(settings.brightness + 0.04f, 0.0f, 1.0f);
            settings.warmth = std::clamp(settings.warmth - 0.02f, 0.0f, 1.0f);
            settings.character = std::clamp(settings.character - 0.04f, 0.0f, 1.0f);
            settings.detune *= lowStringAnchor ? 0.90f : 0.78f;
            settings.stereoWidth *= lowStringAnchor ? 0.94f : 0.82f;
            settings.cutoffHz = std::clamp(settings.cutoffHz * 1.04f, 120.0f, 16000.0f);
        }
        if (isDark)
        {
            settings.brightness = std::clamp(settings.brightness - 0.06f, 0.0f, 1.0f);
            settings.warmth = std::clamp(settings.warmth + 0.08f, 0.0f, 1.0f);
            settings.character = std::clamp(settings.character + 0.05f, 0.0f, 1.0f);
            settings.stereoWidth *= lowStringAnchor ? 0.92f : 0.78f;
            settings.cutoffHz = std::clamp(settings.cutoffHz * 0.86f, 120.0f, 16000.0f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 1.06f, 0.02f, 8.0f);
        }
        if (isWide)
        {
            settings.brightness = std::clamp(settings.brightness + 0.03f, 0.0f, 1.0f);
            settings.warmth = std::clamp(settings.warmth - 0.05f, 0.0f, 1.0f);
            settings.character = std::clamp(settings.character - 0.02f, 0.0f, 1.0f);
            settings.cutoffHz = std::clamp(settings.cutoffHz * 1.05f, 120.0f, 16000.0f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 1.06f, 0.02f, 8.0f);
        }
        if (isStaccato)
        {
            settings.attackSeconds = std::clamp(settings.attackSeconds * 0.82f, 0.001f, 0.05f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.74f, 0.02f, 0.55f);
        }
        if (isMarcato)
        {
            settings.character = std::clamp(settings.character + 0.06f, 0.0f, 1.0f);
            settings.attackSeconds = std::clamp(settings.attackSeconds * 0.62f, 0.001f, 0.08f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.82f, 0.02f, 0.90f);
        }
        if (isSecondary)
        {
            fx.delayMix = std::min(fx.delayMix, 0.12f);
            fx.reverbMix = std::min(fx.reverbMix, isAmbient ? 0.34f : 0.30f);
        }
    }
    else if (family == Family::Bois)
    {
        const bool isPiccolo = instrIndex == 9;
        const bool isDoubleReed = instrIndex == 6 || instrIndex == 10;
        const bool isLowWoodwind = instrIndex == 8 || instrIndex == 11;
        settings.detune = std::clamp(settings.detune, 0.0f, 0.02f);
        settings.stereoWidth = std::clamp(settings.stereoWidth, 0.0f, isPiccolo ? 0.10f : 0.14f);
        settings.warmth = std::clamp(settings.warmth + (isDoubleReed ? 0.06f : 0.0f),
                                     0.0f,
                                     isLowWoodwind ? 0.48f : 0.38f);
        if (isStaccato)
        {
            settings.attackSeconds = std::clamp(settings.attackSeconds * 0.75f, 0.001f, 0.05f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.80f, 0.02f, 0.45f);
        }
        if (isSoft)
        {
            settings.character = std::clamp(settings.character - 0.03f, 0.0f, 1.0f);
            settings.attackSeconds = std::clamp(settings.attackSeconds * 1.10f, 0.001f, 0.24f);
        }
        if (isDark)
        {
            settings.brightness = std::clamp(settings.brightness - 0.05f, 0.0f, 1.0f);
            settings.cutoffHz = std::clamp(settings.cutoffHz * 0.84f, 120.0f, 16000.0f);
        }
        if (isMarcato)
        {
            settings.attackSeconds = std::clamp(settings.attackSeconds * 0.70f, 0.001f, 0.07f);
            settings.character = std::clamp(settings.character + 0.05f, 0.0f, 1.0f);
        }
        if (isSecondary)
        {
            fx.chorusMix = 0.0f;
            fx.delayMix = std::min(fx.delayMix, 0.10f);
            fx.reverbMix = std::min(fx.reverbMix, isAmbient ? 0.32f : 0.28f);
        }

        const float brightnessCap = isPiccolo ? 0.70f
                                  : isDoubleReed ? 0.46f
                                  : isLowWoodwind ? 0.42f
                                  : 0.62f;
        const float cutoffCap = isPiccolo ? 9800.0f
                              : isDoubleReed ? 5200.0f
                              : isLowWoodwind ? 4200.0f
                              : 7600.0f;
        settings.brightness = std::clamp(settings.brightness, 0.0f, brightnessCap);
        settings.cutoffHz = std::clamp(settings.cutoffHz, 120.0f, cutoffCap);
        settings.vibrato = std::min(settings.vibrato, isPiccolo ? 0.18f : (isDoubleReed ? 0.24f : 0.20f));
        if (isDoubleReed)
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 1.08f, 0.02f, 5.0f);
    }
    else if (family == Family::Cuivres)
    {
        const bool lowBrassAnchor = instrIndex == 15;
        settings.detune = std::clamp(settings.detune, 0.0f, lowBrassAnchor ? 0.01f : 0.04f);
        settings.stereoWidth = std::clamp(settings.stereoWidth, 0.0f, lowBrassAnchor ? 0.10f : 0.16f);
        settings.warmth = std::clamp(settings.warmth + 0.03f, 0.0f, 1.0f);
        if (isSoft)
            settings.attackSeconds = std::clamp(settings.attackSeconds * 1.16f, 0.001f, 0.30f);
        if (isDark)
        {
            settings.cutoffHz = std::clamp(settings.cutoffHz * 0.82f, 120.0f, 16000.0f);
            settings.brightness = std::clamp(settings.brightness - 0.04f, 0.0f, 1.0f);
        }
        if (isStaccato)
        {
            settings.attackSeconds = std::clamp(settings.attackSeconds * 0.72f, 0.001f, 0.06f);
            settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.72f, 0.02f, 0.55f);
            settings.sustainLevel = std::clamp(settings.sustainLevel * 0.78f, 0.0f, 1.0f);
        }
        if (isMarcato)
        {
            settings.brightness = std::clamp(settings.brightness + 0.06f, 0.0f, 1.0f);
            settings.attackSeconds = std::clamp(settings.attackSeconds * 0.46f, 0.001f, 0.07f);
            settings.level = std::clamp(settings.level + 0.02f, 0.0f, 1.0f);
        }
        if (isSecondary)
        {
            fx.chorusMix = 0.0f;
            fx.delayMix = std::min(fx.delayMix, 0.11f);
            fx.reverbMix = std::min(fx.reverbMix, isAmbient ? 0.30f : 0.26f);
        }
    }
    else if (family == Family::Percussions)
    {
        settings.vibrato = 0.0f;
        settings.detune = 0.0f;
        settings.stereoWidth = std::clamp(settings.stereoWidth, 0.0f, isCelesta ? 0.12f : 0.10f);
        fx.chorusMix = std::min(fx.chorusMix, isCelesta ? 0.05f : 0.0f);
        fx.delayMix = std::min(fx.delayMix, isCelesta ? 0.08f : 0.04f);
        fx.reverbMix = std::min(fx.reverbMix, isCelesta ? 0.26f : 0.20f);

        if (isTimpani)
        {
            settings.warmth = std::clamp(settings.warmth, 0.0f, 0.18f);
            settings.character = std::clamp(settings.character, 0.0f, 0.70f);
            if (isSoft)
            {
                settings.level = std::clamp(settings.level - 0.05f, 0.0f, 1.0f);
                settings.brightness = std::clamp(settings.brightness - 0.06f, 0.0f, 1.0f);
            }
            if (isDark)
            {
                settings.brightness = std::clamp(settings.brightness - 0.08f, 0.0f, 1.0f);
                settings.cutoffHz = std::clamp(settings.cutoffHz * 0.78f, 120.0f, 16000.0f);
            }
            if (isStaccato || isMarcato)
            {
                settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.72f, 0.02f, 0.60f);
                settings.decaySeconds = std::clamp(settings.decaySeconds * (isMarcato ? 0.74f : 0.62f), 0.02f, 8.0f);
            }
            if (isMarcato)
                settings.brightness = std::clamp(settings.brightness + 0.08f, 0.0f, 1.0f);
        }
        else
        {
            settings.warmth = std::clamp(settings.warmth, 0.0f, 0.14f);
            settings.character = std::clamp(settings.character, 0.0f, 0.68f);
            if (isSoft)
            {
                settings.level = std::clamp(settings.level - 0.03f, 0.0f, 1.0f);
                settings.brightness = std::clamp(settings.brightness - 0.04f, 0.0f, 1.0f);
            }
            if (isDark)
            {
                settings.brightness = std::clamp(settings.brightness - 0.06f, 0.0f, 1.0f);
                settings.cutoffHz = std::clamp(settings.cutoffHz * 0.82f, 120.0f, 16000.0f);
            }
            if (isStaccato)
            {
                settings.decaySeconds = std::clamp(settings.decaySeconds * 0.58f, 0.02f, 8.0f);
                settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.54f, 0.02f, 0.80f);
            }
            if (isMarcato)
            {
                settings.attackSeconds = std::clamp(settings.attackSeconds * 0.60f, 0.001f, 0.04f);
                settings.brightness = std::clamp(settings.brightness + 0.06f, 0.0f, 1.0f);
                settings.releaseSeconds = std::clamp(settings.releaseSeconds * 0.72f, 0.02f, 1.00f);
            }
        }
    }
}

void appendProductionVariants(std::vector<InstrumentPreset>& bank, int instrIndex)
{
    if (bank.empty())
        return;

    const auto instrumentName = std::string(getInstrName(instrIndex));
    const auto concertPreset = bank.front();
    const auto findFxByName = [&bank, &concertPreset] (const char* needle)
    {
        const auto it = std::find_if(bank.begin(), bank.end(), [needle] (const InstrumentPreset& preset)
        {
            return toAsciiLower(preset.name).find(needle) != std::string::npos;
        });
        return it != bank.end() ? it->fx : concertPreset.fx;
    };
    const auto cinematicFx = findFxByName("cinematic");
    const auto staccatoFx = findFxByName("staccato");

    bank.reserve(bank.size() + 4);
    bank.push_back({ instrumentName + " soft",
                     makeSoftSettings(concertPreset.settings),
                     concertPreset.fx,
                     concertPreset.outputBus });
    bank.push_back({ instrumentName + " dark",
                     makeDarkSettings(concertPreset.settings),
                     concertPreset.fx,
                     concertPreset.outputBus });
    bank.push_back({ instrumentName + " marcato",
                     makeMarcatoSettings(concertPreset.settings),
                     makeMarcatoFx(staccatoFx),
                     concertPreset.outputBus });
    bank.push_back({ instrumentName + " wide",
                     makeWideSettings(concertPreset.settings),
                     makeWideFx(cinematicFx),
                     concertPreset.outputBus });
}

std::vector<InstrumentPreset> makeSeedBank(const int instrIndex,
                                           const GlobalFxSettings& cinFx,
                                           const GlobalFxSettings& ambFx,
                                           const GlobalFxSettings& stcFx)
{
    const auto instrumentName = std::string(getInstrName(instrIndex));
    const auto base = getDefaultSettings(instrIndex);
    const auto family = getFamily(instrIndex);

    float brightnessBoost = 0.08f;
    float attackScale = 0.78f;
    float decayScale = 1.14f;
    float releaseScale = 1.24f;
    float vibratoBoost = 0.04f;
    float warmthBoost = 0.10f;
    float detuneBoost = 0.03f;
    float stereoBoost = 0.14f;
    float characterBoost = 0.12f;
    float cutoffScale = 1.12f;
    float levelBoost = 0.03f;

    if (family == Family::Cordes)
    {
        attackScale = instrIndex == 4 ? 0.75f : 0.84f;
        decayScale = instrIndex == 4 ? 1.10f : 1.18f;
        releaseScale = instrIndex == 4 ? 1.22f : 1.26f;
        stereoBoost = instrIndex == 3 ? 0.10f : 0.16f;
        detuneBoost = instrIndex == 3 ? 0.01f : 0.05f;
    }
    else if (family == Family::Bois)
    {
        brightnessBoost = instrIndex == 9 ? 0.04f : 0.00f;
        attackScale = 0.76f;
        decayScale = 1.10f;
        releaseScale = 1.18f;
        vibratoBoost = 0.00f;
        warmthBoost = instrIndex == 11 ? 0.10f : 0.08f;
        detuneBoost = 0.00f;
        stereoBoost = 0.06f;
        characterBoost = 0.06f;
        cutoffScale = instrIndex == 9 ? 1.02f : 0.96f;
    }
    else if (family == Family::Cuivres)
    {
        brightnessBoost = instrIndex == 13 ? 0.06f : 0.03f;
        attackScale = instrIndex == 15 ? 0.82f : 0.72f;
        decayScale = 1.18f;
        releaseScale = instrIndex == 15 ? 1.22f : 1.28f;
        vibratoBoost = 0.02f;
        warmthBoost = instrIndex == 15 ? 0.14f : 0.10f;
        detuneBoost = instrIndex == 15 ? 0.00f : 0.03f;
        stereoBoost = instrIndex == 15 ? 0.04f : 0.12f;
        characterBoost = 0.14f;
        cutoffScale = instrIndex == 15 ? 0.96f : 1.02f;
        levelBoost = 0.04f;
    }
    else
    {
        brightnessBoost = instrIndex == 18 ? 0.00f : 0.04f;
        attackScale = 0.70f;
        decayScale = instrIndex == 18 ? 0.92f : 1.14f;
        releaseScale = instrIndex == 18 ? 0.90f : 1.20f;
        vibratoBoost = 0.0f;
        warmthBoost = 0.04f;
        detuneBoost = 0.0f;
        stereoBoost = 0.04f;
        characterBoost = 0.10f;
        cutoffScale = instrIndex == 19 ? 1.02f : 0.96f;
        levelBoost = 0.02f;
    }

    return {
        makePreset(instrumentName + " concert", base),
        makePreset(instrumentName + " engine dry", makeEngineDrySettings(instrIndex, base), makeEngineDryFx()),
        makePreset(instrumentName + " trailer",
                   makeTrailerSettings(base, brightnessBoost, attackScale, decayScale, releaseScale,
                                       vibratoBoost, warmthBoost, detuneBoost, stereoBoost,
                                       characterBoost, cutoffScale, levelBoost)),
        makePreset(instrumentName + " cinematic", base, cinFx),
        makePreset(instrumentName + " ambient", base, ambFx),
        makePreset(instrumentName + " staccato", makeStaccatoSettings(base), stcFx)
    };
}
} // namespace

// =========================================================================
// Per-instrument factory banks (6 seed presets x 20 instruments)
// =========================================================================

const std::array<std::vector<InstrumentPreset>, kNumInstruments>& getFactoryPresetBanks()
{
    // clang-format off
    static const auto cinFx  = makeCinematicFx();
    static const auto ambFx  = makeAmbientFx();
    static const auto stcFx  = makeStaccatoFx();

    static const std::array<std::vector<InstrumentPreset>, kNumInstruments> banks = []()
    {
        std::array<std::vector<InstrumentPreset>, kNumInstruments> initializedBanks {};

        for (int instrIndex = 0; instrIndex < kNumInstruments; ++instrIndex)
            initializedBanks[static_cast<std::size_t>(instrIndex)] = makeSeedBank(instrIndex, cinFx, ambFx, stcFx);

        for (int instrIndex = 0; instrIndex < kNumInstruments; ++instrIndex)
            appendProductionVariants(initializedBanks[static_cast<std::size_t>(instrIndex)], instrIndex);

        for (int instrIndex = 0; instrIndex < kNumInstruments; ++instrIndex)
            for (auto& preset : initializedBanks[static_cast<std::size_t>(instrIndex)])
            {
                applyFamilyRoleProfile(instrIndex, preset.name, preset.settings, preset.fx);
                preset.metadata = makeMetadata(instrIndex, preset.name);
                preset.outputBus = std::clamp(defaultOutputBusForRole(preset.metadata.mixRole), 0, 4);
                preset.metadata.outputProfile = outputProfileFor(preset.metadata.mixRole, preset.outputBus);
                preset.metadata.description = descriptionFor(humanizeSlug(instrumentTagForIndex(instrIndex)),
                                                             preset.metadata.mixRole);
                addTag(preset.metadata.tags, preset.metadata.outputProfile);
            }

        return initializedBanks;
    }();
    // clang-format on
    return banks;
}

std::size_t getTotalFactoryPresetCount()
{
    const auto& banks = getFactoryPresetBanks();
    std::size_t total = 0;
    for (const auto& bank : banks)
        total += bank.size();
    return total;
}

} // namespace mos
