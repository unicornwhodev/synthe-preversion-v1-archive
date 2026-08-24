#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/SinTable.h"
#include "../../Shared/PresetManifest.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>



namespace
{
constexpr int kPresetVersion = 5;
constexpr int kInstrSynthIndex = 1;

constexpr const char* kOutputGain          = "output_gain";
constexpr const char* kSelectedInstrument = "selected_instrument";
constexpr const char* kLfoRate             = "lfo_rate";
constexpr const char* kLfoDepth            = "lfo_depth";
constexpr const char* kLfoWave             = "lfo_wave";

constexpr const char* kMacroWarmth     = "macro_warmth";
constexpr const char* kMacroBrightness = "macro_brightness";
constexpr const char* kMacroExpression = "macro_expression";
constexpr const char* kMacroTexture    = "macro_texture";

constexpr const char* kCompThreshold = "comp_threshold";
constexpr const char* kCompRatio     = "comp_ratio";
constexpr const char* kCompAttack    = "comp_attack";
constexpr const char* kCompRelease   = "comp_release";
constexpr const char* kCompMakeup    = "comp_makeup";
constexpr const char* kCompMix       = "comp_mix";

constexpr const char* kSatDrive = "sat_drive";
constexpr const char* kSatMix   = "sat_mix";

constexpr const char* kTransientAttack  = "transient_attack";
constexpr const char* kTransientSustain = "transient_sustain";
constexpr const char* kTransientMix     = "transient_mix";

constexpr const char* kReverbSize    = "reverb_size";
constexpr const char* kReverbDamping = "reverb_damping";
constexpr const char* kReverbWidth   = "reverb_width";
constexpr const char* kReverbMix     = "reverb_mix";
constexpr const char* kReverbPredelay = "reverb_predelay";
constexpr float kDefaultReverbSize    = 0.55f;
constexpr float kDefaultReverbDamping = 0.45f;
constexpr float kDefaultReverbWidth   = 0.85f;
constexpr float kDefaultReverbMix     = 0.28f;

// EQ
constexpr const char* kEqLowFreq   = "eq_low_freq";
constexpr const char* kEqLowGain   = "eq_low_gain";
constexpr const char* kEqMidFreq   = "eq_mid_freq";
constexpr const char* kEqMidGain   = "eq_mid_gain";
constexpr const char* kEqMidQ      = "eq_mid_q";
constexpr const char* kEqHighFreq  = "eq_high_freq";
constexpr const char* kEqHighGain  = "eq_high_gain";

// Chorus
constexpr const char* kChorusRate  = "chorus_rate";
constexpr const char* kChorusDepth = "chorus_depth";
constexpr const char* kChorusMix   = "chorus_mix";

// Delay
constexpr const char* kDelayTime     = "delay_time";
constexpr const char* kDelayFeedback = "delay_feedback";
constexpr const char* kDelayMix      = "delay_mix";

// Limiter
constexpr const char* kLimiterThreshold = "limiter_threshold";
constexpr const char* kLimiterRelease   = "limiter_release";

constexpr const char* kInstOutputSuffix = "output";
constexpr const char* kBreathPressureSuffix = "breath_pressure";
constexpr const char* kBowSpeedSuffix       = "bow_speed";
constexpr const char* kBowPressureSuffix    = "bow_pressure";
constexpr const char* kStrikePositionSuffix = "strike_position";
constexpr const char* kBrightnessSuffix     = "brightness";

juce::StringArray makeOutputChoices()
{
    juce::StringArray outputs;
    outputs.add("Master");
    for (int i = 0; i < InstrSynthAudioProcessor::kNumAuxOutputs; ++i)
        outputs.add("Out " + juce::String(i + 1));
    return outputs;
}

float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

int normalizedMidiChannel(const int midiChannel)
{
    return juce::jlimit(1, 16, midiChannel > 0 ? midiChannel : 1);
}

bool matchesMidiChannel(const int candidateChannel, const int requestedChannel)
{
    return requestedChannel <= 0 || candidateChannel == requestedChannel;
}
enum class RarePolyPolicy { Mono, Limited, Poly };
enum class RareTailRole { Short, Natural, Long };
enum class RarePitchQaRole { Stable, Flexible };

struct RareInstrumentRuntimeProfile
{
    RarePolyPolicy polyPolicy = RarePolyPolicy::Poly;
    int maxMusicalVoices = InstrSynthAudioProcessor::kMaxVoicesPerInstr;
    RareTailRole tailRole = RareTailRole::Natural;
    RarePitchQaRole pitchQaRole = RarePitchQaRole::Stable;
};

RareInstrumentRuntimeProfile rareRuntimeProfileForInstrument(const int instrumentIndex) noexcept
{
    switch (juce::jlimit(0, mis::kNumInstruments - 1, instrumentIndex))
    {
        case 2:  return { RarePolyPolicy::Poly,    6, RareTailRole::Natural, RarePitchQaRole::Stable }; // Chapman Stick
        case 10: return { RarePolyPolicy::Limited, 4, RareTailRole::Short,   RarePitchQaRole::Flexible }; // Angklung
        case 11: return { RarePolyPolicy::Limited, 2, RareTailRole::Short,   RarePitchQaRole::Flexible }; // Udu
        case 12: return { RarePolyPolicy::Poly,    6, RareTailRole::Natural, RarePitchQaRole::Stable }; // Pyeongyeong
        case 13: return { RarePolyPolicy::Limited, 3, RareTailRole::Long,    RarePitchQaRole::Flexible }; // Cristal Baschet
        case 14: return { RarePolyPolicy::Poly,    6, RareTailRole::Natural, RarePitchQaRole::Stable }; // Mbira
        case 15: return { RarePolyPolicy::Poly,    6, RareTailRole::Long,    RarePitchQaRole::Stable }; // Handpan
        case 20: return { RarePolyPolicy::Limited, 2, RareTailRole::Long,    RarePitchQaRole::Flexible }; // Yaybahar
        case 5:  // Carnyx
        case 6:  // Aulos
        case 7:  // Fujara
        case 8:  // Gemshorn
        case 9:  // Dizi
        case 16: // Theremine
        case 17: // Ondes Martenot
        case 19: // Hydraulophone
            return { RarePolyPolicy::Mono, 1, RareTailRole::Natural, RarePitchQaRole::Stable };
        case 18:
            return { RarePolyPolicy::Limited, 2, RareTailRole::Natural, RarePitchQaRole::Stable };
        default:
            return { RarePolyPolicy::Limited, 3, RareTailRole::Natural, RarePitchQaRole::Stable };
    }
}

void applyRareRuntimeTailGuardrails(const int instrumentIndex, mis::InstrumentSettings& settings) noexcept
{
    const auto profile = rareRuntimeProfileForInstrument(instrumentIndex);
    switch (profile.tailRole)
    {
        case RareTailRole::Short:
            settings.releaseSeconds = juce::jlimit(0.025f, 1.20f, settings.releaseSeconds);
            settings.decaySeconds = juce::jlimit(0.02f, 2.40f, settings.decaySeconds);
            break;
        case RareTailRole::Long:
            settings.releaseSeconds = juce::jlimit(0.08f, 5.00f, settings.releaseSeconds);
            settings.decaySeconds = juce::jlimit(0.08f, 8.00f, settings.decaySeconds);
            break;
        case RareTailRole::Natural:
        default:
            settings.releaseSeconds = juce::jlimit(0.025f, 3.50f, settings.releaseSeconds);
            settings.decaySeconds = juce::jlimit(0.02f, 6.00f, settings.decaySeconds);
            break;
    }
}

std::size_t fxSlotIndex(const mis::GlobalFxSlot slot)
{
    return static_cast<std::size_t>(slot);
}

juce::File findWritableDirectory(const juce::File& preferred, const juce::String& fallbackRelative)
{
    auto tryDirectory = [](const juce::File& candidate) -> juce::File
    {
        auto dir = candidate;
        dir.createDirectory();
        if (!dir.isDirectory())
            return {};

        auto probe = dir.getNonexistentChildFile(".write_probe", ".tmp", false);
        if (probe.replaceWithText("ok"))
        {
            probe.deleteFile();
            return dir;
        }

        probe.deleteFile();
        return {};
    };

    if (auto dir = tryDirectory(preferred); dir != juce::File{})
        return dir;

    if (auto dir = tryDirectory(juce::File::getSpecialLocation(juce::File::tempDirectory)
                                    .getChildFile(fallbackRelative));
        dir != juce::File{})
    {
        return dir;
    }

    auto cwdFallback = juce::File::getCurrentWorkingDirectory()
                           .getChildFile(".musique_user_data")
                           .getChildFile(fallbackRelative);
    cwdFallback.createDirectory();
    return cwdFallback;
}

juce::String familyLabelForInstrument(const int instrIndex)
{
    switch (mis::getFamily(instrIndex))
    {
        case mis::Family::Strings:    return "strings";
        case mis::Family::Winds:      return "winds";
        case mis::Family::Percussion: return "percussion";
        case mis::Family::Conceptual: return "conceptual";
    }

    return "strings";
}

juce::String instrumentSlugForIndex(const int instrIndex)
{
    auto slug = juce::String(mis::getInstrumentName(instrIndex)).toLowerCase();
    slug = slug.replaceCharacters(" .,/\\-()[]{}", "____________");

    juce::String normalized;
    bool lastWasUnderscore = false;
    for (const auto character : slug)
    {
        const bool keep = juce::CharacterFunctions::isLetterOrDigit(character) || character == '_';
        if (!keep)
            continue;

        if (character == '_')
        {
            if (!lastWasUnderscore && normalized.isNotEmpty())
                normalized << character;
            lastWasUnderscore = true;
            continue;
        }

        normalized << character;
        lastWasUnderscore = false;
    }

    return normalized.trimCharactersAtEnd("_");
}

juce::String presetSlug(const juce::String& presetName)
{
    auto slug = presetName.toLowerCase();
    slug = slug.replaceCharacters(" .,/\\-()[]{}", "____________");

    juce::String normalized;
    bool lastWasUnderscore = false;
    for (const auto character : slug)
    {
        const bool keep = juce::CharacterFunctions::isLetterOrDigit(character) || character == '_';
        if (!keep)
            continue;

        if (character == '_')
        {
            if (!lastWasUnderscore && normalized.isNotEmpty())
                normalized << character;
            lastWasUnderscore = true;
            continue;
        }

        normalized << character;
        lastWasUnderscore = false;
    }

    return normalized.trimCharactersAtEnd("_");
}

InstrSynthAudioProcessor::PersistedPresetMetadata makeUserMetadata(const int instrIndex)
{
    InstrSynthAudioProcessor::PersistedPresetMetadata metadata;
    const auto family = familyLabelForInstrument(instrIndex);
    const auto instrument = instrumentSlugForIndex(instrIndex);
    metadata.mixRole = "custom";
    metadata.family = family;
    metadata.tags = "instr,user,custom," + family + "," + instrument;
    metadata.description = "Custom instr preset";
    metadata.outputProfile = "user-custom";
    metadata.nominalPeakDb = -12.0f;
    return metadata;
}

InstrSynthAudioProcessor::PersistedPresetMetadata makeFactoryMetadata(const int instrIndex,
                                                                     const mis::InstrumentPreset& preset)
{
    InstrSynthAudioProcessor::PersistedPresetMetadata metadata;
    const auto family = familyLabelForInstrument(instrIndex);
    const auto instrument = instrumentSlugForIndex(instrIndex);
    const auto presetName = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    metadata.mixRole = "factory";
    metadata.family = family;
    metadata.tags = "instr,factory," + family + "," + instrument + "," + presetSlug(presetName);
    metadata.description = presetName;
    metadata.outputProfile = "factory";
    metadata.nominalPeakDb = preset.nominalPeakDb;
    return metadata;
}

modmatrix::MatrixState makeDefaultModMatrixState()
{
    modmatrix::MatrixState state;
    state.pitchBendRange = 2;
    state.lfo2Rate = 2.0f;
    state.lfo2Wave = 0;
    return state;
}

InstrSynthAudioProcessor::PresetPersistenceState makeDefaultPresetState(const int instrIndex)
{
    InstrSynthAudioProcessor::PresetPersistenceState state;
    state.instrIndex = juce::jlimit(0, mis::kNumInstruments - 1, instrIndex);
    state.settings = mis::getDefaultSettings(state.instrIndex);
    state.fx = mis::GlobalFxSettings{};
    state.outputBus = 0;
    state.performance = mis::PerformanceSettings{};
    state.modMatrix = makeDefaultModMatrixState();
    state.metadata = makeUserMetadata(state.instrIndex);
    return state;
}

int readPresetFormatVersion(const juce::XmlElement& xml)
{
    if (xml.hasAttribute("format_version"))
        return xml.getIntAttribute("format_version", 0);
    if (xml.hasAttribute("version"))
        return xml.getIntAttribute("version", 0);
    return 0;
}

juce::String readStringAttribute(const juce::XmlElement& xml,
                                 const std::initializer_list<const char*> names,
                                 const juce::String& fallback)
{
    for (const auto* name : names)
    {
        if (xml.hasAttribute(name))
            return xml.getStringAttribute(name).trim();
    }

    return fallback;
}

int readInstrumentIndexAttribute(const juce::XmlElement& xml, const int fallback)
{
    if (xml.hasAttribute("instrIndex"))
        return xml.getIntAttribute("instrIndex", fallback);
    if (xml.hasAttribute("instrument_index"))
        return xml.getIntAttribute("instrument_index", fallback);
    if (xml.hasAttribute("inst"))
        return xml.getIntAttribute("inst", fallback);
    return fallback;
}

int readPresetIndexAttribute(const juce::XmlElement& xml, const int fallback)
{
    if (xml.hasAttribute("preset_index"))
        return xml.getIntAttribute("preset_index", fallback);
    if (xml.hasAttribute("index"))
        return xml.getIntAttribute("index", fallback);
    return fallback;
}

bool tryParseStateFloatValue(const juce::var& rawValue, float& value)
{
    if (rawValue.isDouble() || rawValue.isInt() || rawValue.isInt64() || rawValue.isBool())
    {
        value = static_cast<float>(static_cast<double>(rawValue));
        return std::isfinite(value);
    }

    const auto stringValue = rawValue.toString().trim();
    if (stringValue.isEmpty())
        return false;

    char* end = nullptr;
    const auto parsed = std::strtod(stringValue.toRawUTF8(), &end);
    if (end == stringValue.toRawUTF8() || end == nullptr || *end != '\0' || !std::isfinite(parsed))
        return false;

    value = static_cast<float>(parsed);
    return true;
}

void sanitizeStateParameterValues(juce::AudioProcessorValueTreeState& parameters, juce::ValueTree& state)
{
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        auto child = state.getChild(childIndex);
        const auto paramId = child.getProperty("id").toString();
        if (paramId.isEmpty())
            continue;

        auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId));
        if (parameter == nullptr)
            continue;

        const auto fallbackValue = parameter->convertFrom0to1(parameter->getDefaultValue());
        float actualValue = fallbackValue;
        float parsedValue = fallbackValue;
        if (tryParseStateFloatValue(child.getProperty("value"), parsedValue))
            actualValue = parsedValue;

        child.setProperty("value",
                          parameter->convertFrom0to1(parameter->convertTo0to1(actualValue)),
                          nullptr);
    }
}

bool readValidatedXmlFloat(const juce::XmlElement& xml,
                           const char* attributeName,
                           float minValue,
                           float maxValue,
                           float& destination)
{
    if (!xml.hasAttribute(attributeName))
        return false;

    const auto value = xml.getDoubleAttribute(attributeName);
    if (!std::isfinite(value))
        return false;

    destination = juce::jlimit(minValue, maxValue, static_cast<float>(value));
    return true;
}

float readFiniteXmlFloat(const juce::XmlElement& xml,
                         const char* attributeName,
                         float fallback,
                         float minValue,
                         float maxValue)
{
    float value = fallback;
    if (readValidatedXmlFloat(xml, attributeName, minValue, maxValue, value))
        return value;
    return fallback;
}

bool readValidatedXmlParameterValue(juce::AudioProcessorValueTreeState& parameters,
                                    const juce::XmlElement& xml,
                                    const juce::String& attributeName,
                                    const juce::String& parameterId,
                                    float& destination)
{
    if (!xml.hasAttribute(attributeName))
        return false;

    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(parameterId));
    if (parameter == nullptr)
        return false;

    const auto fallbackValue = parameter->convertFrom0to1(parameter->getDefaultValue());
    float parsedValue = fallbackValue;
    if (!tryParseStateFloatValue(xml.getStringAttribute(attributeName), parsedValue))
        return false;

    destination = parameter->convertFrom0to1(parameter->convertTo0to1(parsedValue));
    return true;
}

bool writePresetManifest(const juce::File& presetFile,
                         const juce::String& presetName,
                         int instrIndex,
                         const juce::String& sourceModel)
{
    const auto identity = musique::preset::getSynthIdentity(1);
    if (!identity.isValid())
        return false;

    musique::preset::PresetManifest manifest;
    manifest.synthId = identity.synthId;
    manifest.synthType = identity.synthType;
    manifest.instrumentIndex = juce::jlimit(0, mis::kNumInstruments - 1, instrIndex);
    manifest.instrumentName = mis::getInstrumentName(manifest.instrumentIndex);
    manifest.presetName = presetName;
    manifest.xmlRootTag = identity.xmlRootTag;
    manifest.sourceModel = sourceModel;
    manifest.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    manifest.sourcePath = presetFile.getFullPathName();
    manifest.validationVersion = kPresetVersion;

    return musique::preset::saveManifestToFile(
        musique::preset::manifestFileForPresetFile(presetFile), manifest);
}

bool isParameterApplicableToFamily(const mis::Family family, const juce::String& suffix)
{
    if (suffix == kBreathPressureSuffix)
        return family == mis::Family::Winds;

    if (suffix == kBowSpeedSuffix || suffix == kBowPressureSuffix)
        return family == mis::Family::Strings;

    if (suffix == kStrikePositionSuffix)
        return family == mis::Family::Percussion || family == mis::Family::Strings;

    if (suffix == kBrightnessSuffix)
        return true;

    return true;
}

bool tryReadValidatedStateProperty(juce::AudioProcessorValueTreeState& parameters,
                                   const juce::ValueTree& state,
                                   const char* propertyName,
                                   float& destination)
{
    if (!state.hasProperty(propertyName))
        return false;

    auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(propertyName));
    if (parameter == nullptr)
        return false;

    const auto fallbackValue = parameter->convertFrom0to1(parameter->getDefaultValue());
    float parsedValue = fallbackValue;
    if (!tryParseStateFloatValue(state.getProperty(propertyName), parsedValue))
        return false;

    destination = parameter->convertFrom0to1(parameter->convertTo0to1(parsedValue));
    return true;
}

bool tryReadValidatedStateBool(juce::AudioProcessorValueTreeState& parameters,
                               const juce::ValueTree& state,
                               const char* propertyName,
                               bool& destination)
{
    float actualValue = 0.0f;
    if (!tryReadValidatedStateProperty(parameters, state, propertyName, actualValue))
        return false;

    destination = actualValue >= 0.5f;
    return true;
}

mis::GlobalFxSettings readValidatedFxStateProperties(juce::AudioProcessorValueTreeState& parameters,
                                                     const juce::ValueTree& state,
                                                     mis::GlobalFxSettings fallback)
{
    auto fx = fallback;
    tryReadValidatedStateProperty(parameters, state, kSatDrive, fx.satDrive);
    tryReadValidatedStateProperty(parameters, state, kSatMix, fx.satMix);
    tryReadValidatedStateProperty(parameters, state, kTransientAttack, fx.transientAttack);
    tryReadValidatedStateProperty(parameters, state, kTransientSustain, fx.transientSustain);
    tryReadValidatedStateProperty(parameters, state, kTransientMix, fx.transientMix);
    tryReadValidatedStateProperty(parameters, state, kEqLowFreq, fx.eqLowFreq);
    tryReadValidatedStateProperty(parameters, state, kEqLowGain, fx.eqLowGain);
    tryReadValidatedStateProperty(parameters, state, kEqMidFreq, fx.eqMidFreq);
    tryReadValidatedStateProperty(parameters, state, kEqMidGain, fx.eqMidGain);
    tryReadValidatedStateProperty(parameters, state, kEqMidQ, fx.eqMidQ);
    tryReadValidatedStateProperty(parameters, state, kEqHighFreq, fx.eqHighFreq);
    tryReadValidatedStateProperty(parameters, state, kEqHighGain, fx.eqHighGain);
    tryReadValidatedStateProperty(parameters, state, kCompThreshold, fx.compThreshold);
    tryReadValidatedStateProperty(parameters, state, kCompRatio, fx.compRatio);
    tryReadValidatedStateProperty(parameters, state, kCompAttack, fx.compAttack);
    tryReadValidatedStateProperty(parameters, state, kCompRelease, fx.compRelease);
    tryReadValidatedStateProperty(parameters, state, kCompMakeup, fx.compMakeup);
    tryReadValidatedStateProperty(parameters, state, kCompMix, fx.compMix);
    tryReadValidatedStateProperty(parameters, state, kChorusRate, fx.chorusRate);
    tryReadValidatedStateProperty(parameters, state, kChorusDepth, fx.chorusDepth);
    tryReadValidatedStateProperty(parameters, state, kChorusMix, fx.chorusMix);
    tryReadValidatedStateProperty(parameters, state, kDelayTime, fx.delayTime);
    tryReadValidatedStateProperty(parameters, state, kDelayFeedback, fx.delayFeedback);
    tryReadValidatedStateProperty(parameters, state, kDelayMix, fx.delayMix);
    tryReadValidatedStateProperty(parameters, state, kReverbSize, fx.reverbSize);
    tryReadValidatedStateProperty(parameters, state, kReverbDamping, fx.reverbDamping);
    tryReadValidatedStateProperty(parameters, state, kReverbWidth, fx.reverbWidth);
    tryReadValidatedStateProperty(parameters, state, kReverbMix, fx.reverbMix);
    tryReadValidatedStateProperty(parameters, state, kReverbPredelay, fx.reverbPredelay);
    tryReadValidatedStateProperty(parameters, state, kLimiterThreshold, fx.limiterThreshold);
    tryReadValidatedStateProperty(parameters, state, kLimiterRelease, fx.limiterRelease);
    tryReadValidatedStateBool(parameters, state, "fx_tab0_en", fx.saturatorOn);
    tryReadValidatedStateBool(parameters, state, "fx_tab1_en", fx.transientOn);
    tryReadValidatedStateBool(parameters, state, "fx_eq_en", fx.eqOn);
    tryReadValidatedStateBool(parameters, state, "fx_tab2_en", fx.compressorOn);
    tryReadValidatedStateBool(parameters, state, "fx_chorus_en", fx.chorusOn);
    tryReadValidatedStateBool(parameters, state, "fx_delay_en", fx.delayOn);
    tryReadValidatedStateBool(parameters, state, "fx_tab3_en", fx.reverbOn);
    tryReadValidatedStateBool(parameters, state, "fx_limiter_en", fx.limiterOn);
    return fx;
}

bool tryReadValidatedChildInstrumentIndex(const juce::ValueTree& child, int& instrumentIndex)
{
    float parsedValue = 0.0f;
    if (!tryParseStateFloatValue(child.getProperty("inst"), parsedValue))
        return false;

    const auto roundedValue = static_cast<int>(std::round(parsedValue));
    if (roundedValue < 0 || roundedValue >= mis::kNumInstruments)
        return false;

    instrumentIndex = roundedValue;
    return true;
}
void writeFxStateProperties(juce::ValueTree& state, const mis::GlobalFxSettings& fx)
{
    state.setProperty("sat_drive", fx.satDrive, nullptr);
    state.setProperty("sat_mix", fx.satMix, nullptr);
    state.setProperty("transient_attack", fx.transientAttack, nullptr);
    state.setProperty("transient_sustain", fx.transientSustain, nullptr);
    state.setProperty("transient_mix", fx.transientMix, nullptr);
    state.setProperty("eq_low_freq", fx.eqLowFreq, nullptr);
    state.setProperty("eq_low_gain", fx.eqLowGain, nullptr);
    state.setProperty("eq_mid_freq", fx.eqMidFreq, nullptr);
    state.setProperty("eq_mid_gain", fx.eqMidGain, nullptr);
    state.setProperty("eq_mid_q", fx.eqMidQ, nullptr);
    state.setProperty("eq_high_freq", fx.eqHighFreq, nullptr);
    state.setProperty("eq_high_gain", fx.eqHighGain, nullptr);
    state.setProperty("comp_threshold", fx.compThreshold, nullptr);
    state.setProperty("comp_ratio", fx.compRatio, nullptr);
    state.setProperty("comp_attack", fx.compAttack, nullptr);
    state.setProperty("comp_release", fx.compRelease, nullptr);
    state.setProperty("comp_makeup", fx.compMakeup, nullptr);
    state.setProperty("comp_mix", fx.compMix, nullptr);
    state.setProperty("chorus_rate", fx.chorusRate, nullptr);
    state.setProperty("chorus_depth", fx.chorusDepth, nullptr);
    state.setProperty("chorus_mix", fx.chorusMix, nullptr);
    state.setProperty("delay_time", fx.delayTime, nullptr);
    state.setProperty("delay_feedback", fx.delayFeedback, nullptr);
    state.setProperty("delay_mix", fx.delayMix, nullptr);
    state.setProperty("reverb_size", fx.reverbSize, nullptr);
    state.setProperty("reverb_damping", fx.reverbDamping, nullptr);
    state.setProperty("reverb_width", fx.reverbWidth, nullptr);
    state.setProperty("reverb_mix", fx.reverbMix, nullptr);
    state.setProperty("reverb_predelay", fx.reverbPredelay, nullptr);
    state.setProperty("limiter_threshold", fx.limiterThreshold, nullptr);
    state.setProperty("limiter_release", fx.limiterRelease, nullptr);
    state.setProperty("fx_tab0_en", fx.saturatorOn, nullptr);
    state.setProperty("fx_tab1_en", fx.transientOn, nullptr);
    state.setProperty("fx_eq_en", fx.eqOn, nullptr);
    state.setProperty("fx_tab2_en", fx.compressorOn, nullptr);
    state.setProperty("fx_chorus_en", fx.chorusOn, nullptr);
    state.setProperty("fx_delay_en", fx.delayOn, nullptr);
    state.setProperty("fx_tab3_en", fx.reverbOn, nullptr);
    state.setProperty("fx_limiter_en", fx.limiterOn, nullptr);
}

void writeMetadataAttributes(juce::XmlElement& root,
                             const InstrSynthAudioProcessor::PersistedPresetMetadata& metadata)
{
    root.setAttribute("mix_role", metadata.mixRole);
    root.setAttribute("family", metadata.family);
    root.setAttribute("tags", metadata.tags);
    root.setAttribute("description", metadata.description);
    root.setAttribute("output_profile", metadata.outputProfile);
    root.setAttribute("nominal_peak_db", static_cast<double>(metadata.nominalPeakDb));
}

void readMetadataAttributes(const juce::XmlElement& xml,
                            InstrSynthAudioProcessor::PersistedPresetMetadata& metadata)
{
    metadata.mixRole = readStringAttribute(xml, { "mix_role" }, metadata.mixRole);
    metadata.family = readStringAttribute(xml, { "family" }, metadata.family);
    metadata.tags = readStringAttribute(xml, { "tags" }, metadata.tags);
    metadata.description = readStringAttribute(xml, { "description" }, metadata.description);
    metadata.outputProfile = readStringAttribute(xml, { "output_profile" }, metadata.outputProfile);
    metadata.nominalPeakDb = readFiniteXmlFloat(xml, "nominal_peak_db", metadata.nominalPeakDb, -24.0f, -1.0f);
}

void writePerformanceAttributes(juce::XmlElement& root, const mis::PerformanceSettings& performance)
{
    root.setAttribute("lfo_rate", static_cast<double>(juce::jlimit(0.05f, 12.0f, performance.lfoRate)));
    root.setAttribute("lfo_depth", static_cast<double>(clamp01(performance.lfoDepth)));
    root.setAttribute("lfo_wave", juce::jlimit(0, 3, performance.lfoWave));
    root.setAttribute("macro_warmth", static_cast<double>(clamp01(performance.macroWarmth)));
    root.setAttribute("macro_brightness", static_cast<double>(clamp01(performance.macroBrightness)));
    root.setAttribute("macro_expression", static_cast<double>(clamp01(performance.macroExpression)));
    root.setAttribute("macro_texture", static_cast<double>(clamp01(performance.macroTexture)));
}

void readPerformanceAttributes(juce::AudioProcessorValueTreeState& parameters,
                               const juce::XmlElement& xml,
                               mis::PerformanceSettings& performance)
{
    float value = 0.0f;
    if (readValidatedXmlParameterValue(parameters, xml, "lfo_rate", kLfoRate, value))
        performance.lfoRate = value;
    if (readValidatedXmlParameterValue(parameters, xml, "lfo_depth", kLfoDepth, value))
        performance.lfoDepth = value;
    performance.lfoWave = juce::jlimit(0, 3, xml.getIntAttribute("lfo_wave", performance.lfoWave));
    if (readValidatedXmlParameterValue(parameters, xml, "macro_warmth", kMacroWarmth, value))
        performance.macroWarmth = value;
    if (readValidatedXmlParameterValue(parameters, xml, "macro_brightness", kMacroBrightness, value))
        performance.macroBrightness = value;
    if (readValidatedXmlParameterValue(parameters, xml, "macro_expression", kMacroExpression, value))
        performance.macroExpression = value;
    if (readValidatedXmlParameterValue(parameters, xml, "macro_texture", kMacroTexture, value))
        performance.macroTexture = value;
}

void writeInstrSettingsAttributes(juce::XmlElement& root, const mis::InstrumentSettings& s)
{
    root.setAttribute("level", static_cast<double>(s.level));
    root.setAttribute("tune", static_cast<double>(s.tuneSemitones));
    root.setAttribute("attack", static_cast<double>(s.attackSeconds));
    root.setAttribute("decay", static_cast<double>(s.decaySeconds));
    root.setAttribute("sustain", static_cast<double>(s.sustainLevel));
    root.setAttribute("release", static_cast<double>(s.releaseSeconds));
    root.setAttribute("exciter", static_cast<double>(s.exciter));
    root.setAttribute("body", static_cast<double>(s.body));
    root.setAttribute("sympathetic", static_cast<double>(s.sympathetic));
    root.setAttribute("noise", static_cast<double>(s.noiseAmount));
    root.setAttribute("drive", static_cast<double>(s.drive));
    root.setAttribute("cutoff", static_cast<double>(s.cutoffHz));
    root.setAttribute("filter_q", static_cast<double>(s.filterQ));
    root.setAttribute("pan", static_cast<double>(s.pan));
    root.setAttribute("breath_pressure", static_cast<double>(s.breathPressure));
    root.setAttribute("bow_speed", static_cast<double>(s.bowSpeed));
    root.setAttribute("bow_pressure", static_cast<double>(s.bowPressure));
    root.setAttribute("strike_position", static_cast<double>(s.strikePosition));
    root.setAttribute("brightness", static_cast<double>(s.brightness));
}

void readInstrSettingsAttributes(const juce::XmlElement& xml,
                                 mis::InstrumentSettings& settings)
{
    float value = 0.0f;
    if (readValidatedXmlFloat(xml, "level", 0.0f, 1.0f, value)) settings.level = value;
    if (readValidatedXmlFloat(xml, "tune", -24.0f, 24.0f, value)) settings.tuneSemitones = value;
    if (readValidatedXmlFloat(xml, "attack", 0.0f, 2.0f, value)) settings.attackSeconds = value;
    if (readValidatedXmlFloat(xml, "decay", 0.01f, 5.0f, value)) settings.decaySeconds = value;
    if (readValidatedXmlFloat(xml, "sustain", 0.0f, 1.0f, value)) settings.sustainLevel = value;
    if (readValidatedXmlFloat(xml, "release", 0.01f, 5.0f, value)) settings.releaseSeconds = value;
    if (readValidatedXmlFloat(xml, "exciter", 0.0f, 1.0f, value)) settings.exciter = value;
    if (readValidatedXmlFloat(xml, "body", 0.0f, 1.0f, value)) settings.body = value;
    if (readValidatedXmlFloat(xml, "sympathetic", 0.0f, 1.0f, value)) settings.sympathetic = value;
    if (readValidatedXmlFloat(xml, "noise", 0.0f, 1.0f, value)) settings.noiseAmount = value;
    if (readValidatedXmlFloat(xml, "drive", 1.0f, 12.0f, value)) settings.drive = value;
    if (readValidatedXmlFloat(xml, "cutoff", 120.0f, 18000.0f, value)) settings.cutoffHz = value;
    if (readValidatedXmlFloat(xml, "filter_q", 0.1f, 12.0f, value)) settings.filterQ = value;
    if (readValidatedXmlFloat(xml, "pan", -1.0f, 1.0f, value)) settings.pan = value;
    if (readValidatedXmlFloat(xml, "breath_pressure", 0.0f, 1.0f, value)) settings.breathPressure = value;
    if (readValidatedXmlFloat(xml, "bow_speed", 0.0f, 1.0f, value)) settings.bowSpeed = value;
    if (readValidatedXmlFloat(xml, "bow_pressure", 0.0f, 1.0f, value)) settings.bowPressure = value;
    if (readValidatedXmlFloat(xml, "strike_position", 0.0f, 1.0f, value)) settings.strikePosition = value;
    if (readValidatedXmlFloat(xml, "brightness", 0.0f, 1.0f, value)) settings.brightness = value;
}

void writeFxSettingsAttributes(juce::XmlElement& root, const mis::GlobalFxSettings& fx)
{
    root.setAttribute("sat_drive", static_cast<double>(fx.satDrive));
    root.setAttribute("sat_mix", static_cast<double>(fx.satMix));
    root.setAttribute("transient_attack", static_cast<double>(fx.transientAttack));
    root.setAttribute("transient_sustain", static_cast<double>(fx.transientSustain));
    root.setAttribute("transient_mix", static_cast<double>(fx.transientMix));
    root.setAttribute("eq_low_freq", static_cast<double>(fx.eqLowFreq));
    root.setAttribute("eq_low_gain", static_cast<double>(fx.eqLowGain));
    root.setAttribute("eq_mid_freq", static_cast<double>(fx.eqMidFreq));
    root.setAttribute("eq_mid_gain", static_cast<double>(fx.eqMidGain));
    root.setAttribute("eq_mid_q", static_cast<double>(fx.eqMidQ));
    root.setAttribute("eq_high_freq", static_cast<double>(fx.eqHighFreq));
    root.setAttribute("eq_high_gain", static_cast<double>(fx.eqHighGain));
    root.setAttribute("comp_threshold", static_cast<double>(fx.compThreshold));
    root.setAttribute("comp_ratio", static_cast<double>(fx.compRatio));
    root.setAttribute("comp_attack", static_cast<double>(fx.compAttack));
    root.setAttribute("comp_release", static_cast<double>(fx.compRelease));
    root.setAttribute("comp_makeup", static_cast<double>(fx.compMakeup));
    root.setAttribute("comp_mix", static_cast<double>(fx.compMix));
    root.setAttribute("chorus_rate", static_cast<double>(fx.chorusRate));
    root.setAttribute("chorus_depth", static_cast<double>(fx.chorusDepth));
    root.setAttribute("chorus_mix", static_cast<double>(fx.chorusMix));
    root.setAttribute("delay_time", static_cast<double>(fx.delayTime));
    root.setAttribute("delay_feedback", static_cast<double>(fx.delayFeedback));
    root.setAttribute("delay_mix", static_cast<double>(fx.delayMix));
    root.setAttribute("reverb_size", static_cast<double>(fx.reverbSize));
    root.setAttribute("reverb_damping", static_cast<double>(fx.reverbDamping));
    root.setAttribute("reverb_width", static_cast<double>(fx.reverbWidth));
    root.setAttribute("reverb_mix", static_cast<double>(fx.reverbMix));
    root.setAttribute("reverb_predelay", static_cast<double>(fx.reverbPredelay));
    root.setAttribute("limiter_threshold", static_cast<double>(fx.limiterThreshold));
    root.setAttribute("limiter_release", static_cast<double>(fx.limiterRelease));
    root.setAttribute("fx_tab0_en", fx.saturatorOn);
    root.setAttribute("fx_tab1_en", fx.transientOn);
    root.setAttribute("fx_eq_en", fx.eqOn);
    root.setAttribute("fx_tab2_en", fx.compressorOn);
    root.setAttribute("fx_chorus_en", fx.chorusOn);
    root.setAttribute("fx_delay_en", fx.delayOn);
    root.setAttribute("fx_tab3_en", fx.reverbOn);
    root.setAttribute("fx_limiter_en", fx.limiterOn);
}

void readFxSettingsAttributes(juce::AudioProcessorValueTreeState& parameters,
                              const juce::XmlElement& xml,
                              mis::GlobalFxSettings& fx)
{
    float value = 0.0f;
    if (readValidatedXmlParameterValue(parameters, xml, "sat_drive", kSatDrive, value)) fx.satDrive = value;
    if (readValidatedXmlParameterValue(parameters, xml, "sat_mix", kSatMix, value)) fx.satMix = value;
    if (readValidatedXmlParameterValue(parameters, xml, "transient_attack", kTransientAttack, value)) fx.transientAttack = value;
    if (readValidatedXmlParameterValue(parameters, xml, "transient_sustain", kTransientSustain, value)) fx.transientSustain = value;
    if (readValidatedXmlParameterValue(parameters, xml, "transient_mix", kTransientMix, value)) fx.transientMix = value;
    if (readValidatedXmlParameterValue(parameters, xml, "eq_low_freq", kEqLowFreq, value)) fx.eqLowFreq = value;
    if (readValidatedXmlParameterValue(parameters, xml, "eq_low_gain", kEqLowGain, value)) fx.eqLowGain = value;
    if (readValidatedXmlParameterValue(parameters, xml, "eq_mid_freq", kEqMidFreq, value)) fx.eqMidFreq = value;
    if (readValidatedXmlParameterValue(parameters, xml, "eq_mid_gain", kEqMidGain, value)) fx.eqMidGain = value;
    if (readValidatedXmlParameterValue(parameters, xml, "eq_mid_q", kEqMidQ, value)) fx.eqMidQ = value;
    if (readValidatedXmlParameterValue(parameters, xml, "eq_high_freq", kEqHighFreq, value)) fx.eqHighFreq = value;
    if (readValidatedXmlParameterValue(parameters, xml, "eq_high_gain", kEqHighGain, value)) fx.eqHighGain = value;
    if (readValidatedXmlParameterValue(parameters, xml, "comp_threshold", kCompThreshold, value)) fx.compThreshold = value;
    if (readValidatedXmlParameterValue(parameters, xml, "comp_ratio", kCompRatio, value)) fx.compRatio = value;
    if (readValidatedXmlParameterValue(parameters, xml, "comp_attack", kCompAttack, value)) fx.compAttack = value;
    if (readValidatedXmlParameterValue(parameters, xml, "comp_release", kCompRelease, value)) fx.compRelease = value;
    if (readValidatedXmlParameterValue(parameters, xml, "comp_makeup", kCompMakeup, value)) fx.compMakeup = value;
    if (readValidatedXmlParameterValue(parameters, xml, "comp_mix", kCompMix, value)) fx.compMix = value;
    if (readValidatedXmlParameterValue(parameters, xml, "chorus_rate", kChorusRate, value)) fx.chorusRate = value;
    if (readValidatedXmlParameterValue(parameters, xml, "chorus_depth", kChorusDepth, value)) fx.chorusDepth = value;
    if (readValidatedXmlParameterValue(parameters, xml, "chorus_mix", kChorusMix, value)) fx.chorusMix = value;
    if (readValidatedXmlParameterValue(parameters, xml, "delay_time", kDelayTime, value)) fx.delayTime = value;
    if (readValidatedXmlParameterValue(parameters, xml, "delay_feedback", kDelayFeedback, value)) fx.delayFeedback = value;
    if (readValidatedXmlParameterValue(parameters, xml, "delay_mix", kDelayMix, value)) fx.delayMix = value;
    if (readValidatedXmlParameterValue(parameters, xml, "reverb_size", kReverbSize, value)) fx.reverbSize = value;
    if (readValidatedXmlParameterValue(parameters, xml, "reverb_damping", kReverbDamping, value)) fx.reverbDamping = value;
    if (readValidatedXmlParameterValue(parameters, xml, "reverb_width", kReverbWidth, value)) fx.reverbWidth = value;
    if (readValidatedXmlParameterValue(parameters, xml, "reverb_mix", kReverbMix, value)) fx.reverbMix = value;
    if (readValidatedXmlParameterValue(parameters, xml, "reverb_predelay", kReverbPredelay, value)) fx.reverbPredelay = value;
    if (readValidatedXmlParameterValue(parameters, xml, "limiter_threshold", kLimiterThreshold, value)) fx.limiterThreshold = value;
    if (readValidatedXmlParameterValue(parameters, xml, "limiter_release", kLimiterRelease, value)) fx.limiterRelease = value;
    fx.saturatorOn = xml.getIntAttribute("fx_tab0_en", fx.saturatorOn ? 1 : 0) != 0;
    fx.transientOn = xml.getIntAttribute("fx_tab1_en", fx.transientOn ? 1 : 0) != 0;
    fx.eqOn = xml.getIntAttribute("fx_eq_en", fx.eqOn ? 1 : 0) != 0;
    fx.compressorOn = xml.getIntAttribute("fx_tab2_en", fx.compressorOn ? 1 : 0) != 0;
    fx.chorusOn = xml.getIntAttribute("fx_chorus_en", fx.chorusOn ? 1 : 0) != 0;
    fx.delayOn = xml.getIntAttribute("fx_delay_en", fx.delayOn ? 1 : 0) != 0;
    fx.reverbOn = xml.getIntAttribute("fx_tab3_en", fx.reverbOn ? 1 : 0) != 0;
    fx.limiterOn = xml.getIntAttribute("fx_limiter_en", fx.limiterOn ? 1 : 0) != 0;
}

void writeModMatrixXml(juce::XmlElement& root, const modmatrix::MatrixState& state)
{
    auto* matrix = root.createNewChildElement("ModMatrix");
    matrix->setAttribute("pbRange", juce::jlimit(1, 24, state.pitchBendRange));
    matrix->setAttribute("lfo2Rate", static_cast<double>(juce::jlimit(0.05f, 12.0f, state.lfo2Rate)));
    matrix->setAttribute("lfo2Wave", juce::jlimit(0, 3, state.lfo2Wave));

    for (int slotIndex = 0; slotIndex < modmatrix::kMaxSlots; ++slotIndex)
    {
        const auto& slot = state.slots[static_cast<std::size_t>(slotIndex)];
        auto* slotXml = matrix->createNewChildElement("Slot");
        slotXml->setAttribute("idx", slotIndex);
        slotXml->setAttribute("src", static_cast<int>(slot.source));
        slotXml->setAttribute("dst", static_cast<int>(slot.destination));
        slotXml->setAttribute("amt", static_cast<double>(juce::jlimit(-1.0f, 1.0f, slot.amount)));
    }
}

bool readModMatrixXml(const juce::XmlElement& xml,
                      modmatrix::MatrixState& state,
                      bool* hasCompleteSchema = nullptr)
{
    auto fallback = state;
    if (auto* matrix = xml.getChildByName("ModMatrix"))
    {
        state = fallback;
        state.pitchBendRange = juce::jlimit(1, 24, matrix->getIntAttribute("pbRange", state.pitchBendRange));
        state.lfo2Rate = readFiniteXmlFloat(*matrix, "lfo2Rate", state.lfo2Rate, 0.05f, 12.0f);
        state.lfo2Wave = juce::jlimit(0, 3, matrix->getIntAttribute("lfo2Wave", state.lfo2Wave));

        std::array<bool, modmatrix::kMaxSlots> seen {};
        for (auto* slotXml : matrix->getChildWithTagNameIterator("Slot"))
        {
            const auto index = slotXml->getIntAttribute("idx", -1);
            if (index < 0 || index >= modmatrix::kMaxSlots)
                continue;

            auto& slot = state.slots[static_cast<std::size_t>(index)];
            slot.source = static_cast<modmatrix::Source>(
                juce::jlimit(0, modmatrix::kSourceCount - 1, slotXml->getIntAttribute("src", 0)));
            slot.destination = static_cast<modmatrix::Destination>(
                juce::jlimit(0, modmatrix::kDestCount - 1, slotXml->getIntAttribute("dst", 0)));
            slot.amount = juce::jlimit(-1.0f, 1.0f, static_cast<float>(slotXml->getDoubleAttribute("amt", 0.0)));
            seen[static_cast<std::size_t>(index)] = true;
        }

        if (hasCompleteSchema != nullptr)
        {
            *hasCompleteSchema = matrix->hasAttribute("pbRange")
                && matrix->hasAttribute("lfo2Rate")
                && matrix->hasAttribute("lfo2Wave")
                && std::all_of(seen.begin(), seen.end(), [] (bool value) { return value; });
        }
        return true;
    }

    if (hasCompleteSchema != nullptr)
        *hasCompleteSchema = false;
    return false;
}

std::unique_ptr<juce::XmlElement> createPresetXml(const juce::String& tagName,
                                                  const InstrSynthAudioProcessor::PresetPersistenceState& state)
{
    auto root = std::make_unique<juce::XmlElement>(tagName);
    root->setAttribute("version", kPresetVersion);
    root->setAttribute("format_version", kPresetVersion);
    root->setAttribute("name", state.name);
    root->setAttribute("synth_index", kInstrSynthIndex);
    root->setAttribute("inst", state.instrIndex);
    root->setAttribute("instrIndex", state.instrIndex);
    root->setAttribute("instrument_index", state.instrIndex);
    if (state.presetIndex >= 0)
    {
        root->setAttribute("index", state.presetIndex);
        root->setAttribute("preset_index", state.presetIndex);
    }

    writeInstrSettingsAttributes(*root, state.settings);
    root->setAttribute("output", juce::jlimit(0, InstrSynthAudioProcessor::kNumAuxOutputs, state.outputBus));
    writePerformanceAttributes(*root, state.performance);
    writeFxSettingsAttributes(*root, state.fx);
    writeMetadataAttributes(*root, state.metadata);
    writeModMatrixXml(*root, state.modMatrix);
    return root;
}

bool parsePresetXml(juce::AudioProcessorValueTreeState& parameters,
                    const juce::XmlElement& xml,
                    const char* expectedTag,
                    const int expectedInstrIndex,
                    InstrSynthAudioProcessor::PresetPersistenceState& out,
                    const bool requirePresetIndex,
                    const InstrSynthAudioProcessor::PresetPersistenceState* fallback = nullptr)
{
    if (!xml.hasTagName(expectedTag))
        return false;

    out = fallback != nullptr ? *fallback : makeDefaultPresetState(expectedInstrIndex);
    out.name = readStringAttribute(xml, { "name" }, out.name);
    out.instrIndex = readInstrumentIndexAttribute(xml, expectedInstrIndex);
    if (out.instrIndex != expectedInstrIndex)
        return false;

    out.metadata = fallback != nullptr ? fallback->metadata : makeUserMetadata(expectedInstrIndex);
    if (requirePresetIndex)
    {
        out.presetIndex = readPresetIndexAttribute(xml, out.presetIndex);
        if (out.presetIndex < 0)
            return false;
    }
    else
    {
        out.presetIndex = -1;
    }

    readInstrSettingsAttributes(xml, out.settings);
    out.outputBus = juce::jlimit(0,
                                 InstrSynthAudioProcessor::kNumAuxOutputs,
                                 xml.getIntAttribute("output", out.outputBus));
    readPerformanceAttributes(parameters, xml, out.performance);
    readFxSettingsAttributes(parameters, xml, out.fx);
    readMetadataAttributes(xml, out.metadata);
    readModMatrixXml(xml, out.modMatrix);
    return true;
}

bool shouldRewritePresetXml(const juce::XmlElement& xml, const bool requirePresetIndex)
{
    if (readPresetFormatVersion(xml) < kPresetVersion)
        return true;

    static constexpr const char* kRequiredAttributes[] = {
        "name", "version", "format_version", "synth_index", "inst", "instrIndex", "instrument_index",
        "level", "tune", "attack", "decay", "sustain", "release", "exciter", "body",
        "sympathetic", "noise", "drive", "cutoff", "filter_q", "pan", "breath_pressure",
        "bow_speed", "bow_pressure", "strike_position", "brightness", "output",
        "lfo_rate", "lfo_depth", "lfo_wave", "macro_warmth", "macro_brightness",
        "macro_expression", "macro_texture", "sat_drive", "sat_mix", "transient_attack",
        "transient_sustain", "transient_mix", "eq_low_freq", "eq_low_gain", "eq_mid_freq",
        "eq_mid_gain", "eq_mid_q", "eq_high_freq", "eq_high_gain", "comp_threshold",
        "comp_ratio", "comp_attack", "comp_release", "comp_makeup", "comp_mix",
        "chorus_rate", "chorus_depth", "chorus_mix", "delay_time", "delay_feedback",
        "delay_mix", "reverb_size", "reverb_damping", "reverb_width", "reverb_mix",
        "reverb_predelay", "limiter_threshold", "limiter_release", "fx_tab0_en",
        "fx_tab1_en", "fx_eq_en", "fx_tab2_en", "fx_chorus_en", "fx_delay_en",
        "fx_tab3_en", "fx_limiter_en", "mix_role", "family", "tags",
        "description", "output_profile", "nominal_peak_db"
    };

    for (const auto* attribute : kRequiredAttributes)
    {
        if (!xml.hasAttribute(attribute))
            return true;
    }

    if (requirePresetIndex && (!xml.hasAttribute("index") || !xml.hasAttribute("preset_index")))
        return true;

    bool hasCompleteMatrix = false;
    auto defaultState = makeDefaultModMatrixState();
    if (!readModMatrixXml(xml, defaultState, &hasCompleteMatrix) || !hasCompleteMatrix)
        return true;

    return false;
}

mis::GlobalFxSettings readFxStateProperties(const juce::ValueTree& state,
                                            mis::GlobalFxSettings fallback)
{
    auto fx = fallback;
    fx.satDrive          = static_cast<float>(state.getProperty("sat_drive", fx.satDrive));
    fx.satMix            = static_cast<float>(state.getProperty("sat_mix", fx.satMix));
    fx.transientAttack   = static_cast<float>(state.getProperty("transient_attack", fx.transientAttack));
    fx.transientSustain  = static_cast<float>(state.getProperty("transient_sustain", fx.transientSustain));
    fx.transientMix      = static_cast<float>(state.getProperty("transient_mix", fx.transientMix));
    fx.eqLowFreq         = static_cast<float>(state.getProperty("eq_low_freq", fx.eqLowFreq));
    fx.eqLowGain         = static_cast<float>(state.getProperty("eq_low_gain", fx.eqLowGain));
    fx.eqMidFreq         = static_cast<float>(state.getProperty("eq_mid_freq", fx.eqMidFreq));
    fx.eqMidGain         = static_cast<float>(state.getProperty("eq_mid_gain", fx.eqMidGain));
    fx.eqMidQ            = static_cast<float>(state.getProperty("eq_mid_q", fx.eqMidQ));
    fx.eqHighFreq        = static_cast<float>(state.getProperty("eq_high_freq", fx.eqHighFreq));
    fx.eqHighGain        = static_cast<float>(state.getProperty("eq_high_gain", fx.eqHighGain));
    fx.compThreshold     = static_cast<float>(state.getProperty("comp_threshold", fx.compThreshold));
    fx.compRatio         = static_cast<float>(state.getProperty("comp_ratio", fx.compRatio));
    fx.compAttack        = static_cast<float>(state.getProperty("comp_attack", fx.compAttack));
    fx.compRelease       = static_cast<float>(state.getProperty("comp_release", fx.compRelease));
    fx.compMakeup        = static_cast<float>(state.getProperty("comp_makeup", fx.compMakeup));
    fx.compMix           = static_cast<float>(state.getProperty("comp_mix", fx.compMix));
    fx.chorusRate        = static_cast<float>(state.getProperty("chorus_rate", fx.chorusRate));
    fx.chorusDepth       = static_cast<float>(state.getProperty("chorus_depth", fx.chorusDepth));
    fx.chorusMix         = static_cast<float>(state.getProperty("chorus_mix", fx.chorusMix));
    fx.delayTime         = static_cast<float>(state.getProperty("delay_time", fx.delayTime));
    fx.delayFeedback     = static_cast<float>(state.getProperty("delay_feedback", fx.delayFeedback));
    fx.delayMix          = static_cast<float>(state.getProperty("delay_mix", fx.delayMix));
    fx.reverbSize        = static_cast<float>(state.getProperty("reverb_size", fx.reverbSize));
    fx.reverbDamping     = static_cast<float>(state.getProperty("reverb_damping", fx.reverbDamping));
    fx.reverbWidth       = static_cast<float>(state.getProperty("reverb_width", fx.reverbWidth));
    fx.reverbMix         = static_cast<float>(state.getProperty("reverb_mix", fx.reverbMix));
    fx.reverbPredelay    = static_cast<float>(state.getProperty("reverb_predelay", fx.reverbPredelay));
    fx.limiterThreshold  = static_cast<float>(state.getProperty("limiter_threshold", fx.limiterThreshold));
    fx.limiterRelease    = static_cast<float>(state.getProperty("limiter_release", fx.limiterRelease));
    fx.saturatorOn       = static_cast<bool>(state.getProperty("fx_tab0_en", fx.saturatorOn));
    fx.transientOn       = static_cast<bool>(state.getProperty("fx_tab1_en", fx.transientOn));
    fx.eqOn              = static_cast<bool>(state.getProperty("fx_eq_en", fx.eqOn));
    fx.compressorOn      = static_cast<bool>(state.getProperty("fx_tab2_en", fx.compressorOn));
    fx.chorusOn          = static_cast<bool>(state.getProperty("fx_chorus_en", fx.chorusOn));
    fx.delayOn           = static_cast<bool>(state.getProperty("fx_delay_en", fx.delayOn));
    fx.reverbOn          = static_cast<bool>(state.getProperty("fx_tab3_en", fx.reverbOn));
    fx.limiterOn         = static_cast<bool>(state.getProperty("fx_limiter_en", fx.limiterOn));
    return fx;
}

// ── MIDI CC page tables ──────────────────────────────────────────────────────
struct CCSlot {
    const char* paramId;       // global param, or nullptr for per-instrument
    const char* instrSuffix;   // per-instrument suffix (when paramId == nullptr)
};

static constexpr int kKnobsPerPage = 8;

static const char* kCCPageNames[] = {
    "MACROS",       // 0
    "ENVELOPE",     // 1
    "TONE",         // 2
    "REVERB/DELAY", // 3
    "DYNAMICS",     // 4
    "EQ",           // 5
    "CHORUS/OUT"    // 6
};

static const CCSlot kCCPages[][kKnobsPerPage] = {
    // Page 0 — Macros & Master
    { { "macro_warmth",     nullptr }, { "macro_brightness", nullptr },
      { "macro_expression", nullptr }, { "macro_texture",    nullptr },
      { "lfo_rate",         nullptr }, { "lfo_depth",        nullptr },
      { "reverb_mix",       nullptr }, { "output_gain",      nullptr } },

    // Page 1 — Envelope (per-instrument)
    { { nullptr, "attack"   }, { nullptr, "decay"   },
      { nullptr, "sustain"  }, { nullptr, "release" },
      { nullptr, "exciter"  }, { nullptr, "level"   },
      { nullptr, "tune"     }, { nullptr, "body"    } },

    // Page 2 — Tone (per-instrument + globals)
    { { nullptr, "sympathetic" }, { nullptr, "noise"    },
      { nullptr, "drive"       }, { nullptr, "cutoff"   },
      { nullptr, "filter_q"    }, { nullptr, "pan"      },
      { "sat_drive", nullptr   }, { "sat_mix", nullptr  } },

    // Page 3 — Reverb & Delay
    { { "reverb_size",    nullptr }, { "reverb_damping",  nullptr },
      { "reverb_width",   nullptr }, { "reverb_mix",      nullptr },
      { "reverb_predelay", nullptr }, { "delay_time",      nullptr },
      { "delay_feedback", nullptr }, { "delay_mix",       nullptr } },

    // Page 4 — Dynamics
    { { "comp_threshold",    nullptr }, { "comp_ratio",     nullptr },
      { "comp_attack",       nullptr }, { "comp_release",   nullptr },
      { "comp_makeup",       nullptr }, { "comp_mix",       nullptr },
      { "transient_attack",  nullptr }, { "transient_sustain", nullptr } },

    // Page 5 — EQ
    { { "eq_low_freq",  nullptr }, { "eq_low_gain",  nullptr },
      { "eq_mid_freq",  nullptr }, { "eq_mid_gain",  nullptr },
      { "eq_mid_q",     nullptr }, { "eq_high_freq", nullptr },
      { "eq_high_gain", nullptr }, { "transient_mix", nullptr } },

    // Page 6 — Mod / Limiter
    { { "chorus_rate",       nullptr }, { "chorus_depth",       nullptr },
      { "chorus_mix",        nullptr }, { "limiter_threshold",  nullptr },
      { "limiter_release",   nullptr }, { "sat_drive",          nullptr },
      { "output_gain",       nullptr }, { nullptr,              nullptr } }
};

} // namespace

bool InstrSynthAudioProcessor::writePresetWithManifestRollback(const juce::File& file,
                                                              juce::XmlElement& root,
                                                              const juce::String& presetName,
                                                              int instrumentIndex,
                                                              const juce::String& sourceModel)
{
    const bool hadExistingXml = file.existsAsFile();
    const auto manifestFile = musique::preset::manifestFileForPresetFile(file);
    const bool hadExistingManifest = manifestFile.existsAsFile();

    juce::MemoryBlock xmlBackup;
    juce::MemoryBlock manifestBackup;
    if (hadExistingXml)
        file.loadFileAsData(xmlBackup);
    if (hadExistingManifest)
        manifestFile.loadFileAsData(manifestBackup);

    if (!root.writeTo(file))
        return false;

    if (writePresetManifest(file, presetName, instrumentIndex, sourceModel))
        return true;

    manifestFile.deleteFile();

    if (hadExistingXml)
        file.replaceWithData(xmlBackup.getData(), xmlBackup.getSize());
    else
        file.deleteFile();

    if (hadExistingManifest)
        manifestFile.replaceWithData(manifestBackup.getData(), manifestBackup.getSize());

    return false;
}

bool InstrSynthAudioProcessor::poolSlotOwnedByAnyVoice(int instrumentIndex, int poolSlot) const
{
    for (const auto& slot : voices)
    {
        if (slot.voice != nullptr
            && slot.instrumentIndex == instrumentIndex
            && slot.poolSlot == poolSlot)
        {
            return true;
        }
    }

    for (const auto& slot : dyingVoices)
    {
        if (slot.voice != nullptr
            && slot.instrumentIndex == instrumentIndex
            && slot.poolSlot == poolSlot)
        {
            return true;
        }
    }

    return false;
}

void InstrSynthAudioProcessor::resetVoiceSlotMetadata(VoiceSlot& slot)
{
    slot.voice = nullptr;
    slot.midiNote = -1;
    slot.midiChannel = 0;
    slot.instrumentIndex = -1;
    slot.poolSlot = -1;
    slot.activationAge = 0;
}

// =============================================================================
// Bus layout
// =============================================================================
auto InstrSynthAudioProcessor::createBusLayout() -> BusesProperties
{
    BusesProperties buses;
    buses = buses.withOutput("Master", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < kNumAuxOutputs; ++i)
        buses = buses.withOutput("Instr " + juce::String(i + 1) + " Out",
                                 juce::AudioChannelSet::stereo(), false);
    return buses;
}

// =============================================================================
// Constructor
// =============================================================================
InstrSynthAudioProcessor::InstrSynthAudioProcessor()
    : AudioProcessor(createBusLayout()),
      parameters(*this, nullptr, juce::Identifier("MIS_PARAMS"), createParameterLayout()),
      factoryPresetBanks(mis::getFactoryPresetBanks())
{
    currentPresetIndices.fill(0);
    currentUserPresetFiles.fill(juce::File{});
    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
    {
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(inst)];
        auto& persistedBank = factoryPresetStates[static_cast<std::size_t>(inst)];
        persistedBank.clear();
        persistedBank.reserve(bank.size());
        for (int presetIndex = 0; presetIndex < static_cast<int>(bank.size()); ++presetIndex)
            persistedBank.push_back(makeFactoryPresetState(inst, presetIndex, bank[static_cast<std::size_t>(presetIndex)]));

        if (!bank.empty())
        {
            applyInstPresetSettings(inst, bank[0].settings);
            setParamValue(makeInstParamId(inst, kInstOutputSuffix), static_cast<float>(bank[0].outputBus));
            instrumentFxStates[static_cast<std::size_t>(inst)] = bank[0].fx;
        }
        else
        {
            instrumentFxStates[static_cast<std::size_t>(inst)] = mis::GlobalFxSettings{};
        }
    }

    loadFactoryOverrides();
    cachedSelectedInstrumentIndex = getSelectedInstrumentIndex();
    pendingSelectedInstrumentIndex.store(cachedSelectedInstrumentIndex);
    if (!factoryPresetStates[static_cast<std::size_t>(cachedSelectedInstrumentIndex)].empty())
        applyPresetPersistenceState(factoryPresetStates[static_cast<std::size_t>(cachedSelectedInstrumentIndex)].front());
    else
        applyGlobalFxSettings(cachedSelectedInstrumentIndex,
                              instrumentFxStates[static_cast<std::size_t>(cachedSelectedInstrumentIndex)]);
    parameters.addParameterListener(kSelectedInstrument, this);
}

InstrSynthAudioProcessor::~InstrSynthAudioProcessor()
{
    parameters.removeParameterListener(kSelectedInstrument, this);
    cancelPendingUpdate();
}

// =============================================================================
void InstrSynthAudioProcessor::applyInstPresetSettings(int instrIndex, const mis::InstrumentSettings& s)
{
    setParamValue(makeInstParamId(instrIndex, "level"),       s.level);
    setParamValue(makeInstParamId(instrIndex, "tune"),        s.tuneSemitones);
    setParamValue(makeInstParamId(instrIndex, "attack"),      s.attackSeconds);
    setParamValue(makeInstParamId(instrIndex, "decay"),       s.decaySeconds);
    setParamValue(makeInstParamId(instrIndex, "sustain"),     s.sustainLevel);
    setParamValue(makeInstParamId(instrIndex, "release"),     s.releaseSeconds);
    setParamValue(makeInstParamId(instrIndex, "exciter"),     s.exciter);
    setParamValue(makeInstParamId(instrIndex, "body"),        s.body);
    setParamValue(makeInstParamId(instrIndex, "sympathetic"), s.sympathetic);
    setParamValue(makeInstParamId(instrIndex, "noise"),       s.noiseAmount);
    setParamValue(makeInstParamId(instrIndex, "drive"),       s.drive);
    setParamValue(makeInstParamId(instrIndex, "cutoff"),      s.cutoffHz);
    setParamValue(makeInstParamId(instrIndex, "filter_q"),    s.filterQ);
    setParamValue(makeInstParamId(instrIndex, "pan"),         s.pan);
    setParamValue(makeInstParamId(instrIndex, kBreathPressureSuffix), s.breathPressure);
    setParamValue(makeInstParamId(instrIndex, kBowSpeedSuffix),       s.bowSpeed);
    setParamValue(makeInstParamId(instrIndex, kBowPressureSuffix),    s.bowPressure);
    setParamValue(makeInstParamId(instrIndex, kStrikePositionSuffix), s.strikePosition);
    setParamValue(makeInstParamId(instrIndex, kBrightnessSuffix),     s.brightness);
}

// =============================================================================
// Parameter layout
// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
InstrSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& banks = mis::getFactoryPresetBanks();
    const auto outputChoices = makeOutputChoices();

    // --- Global params ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kOutputGain, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.01f), -3.0f));

    juce::StringArray instrChoices;
    for (int i = 0; i < mis::kNumInstruments; ++i)
        instrChoices.add(mis::getInstrumentName(i));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kSelectedInstrument, "Selected Instrument", instrChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoRate, "LFO Rate",
        juce::NormalisableRange<float>(0.05f, 12.0f, 0.0001f), 1.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoDepth, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kLfoWave, "LFO Wave",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square" }, 0));

    // --- Macros ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroWarmth, "Macro Warmth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroBrightness, "Macro Brightness",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroExpression, "Macro Expression",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroTexture, "Macro Texture",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.3f));

    // --- FX: Compressor ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompThreshold, "Comp Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.01f), -19.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompRatio, "Comp Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f), 4.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompAttack, "Comp Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.01f), 8.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompRelease, "Comp Release",
        juce::NormalisableRange<float>(5.0f, 500.0f, 0.01f), 140.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompMakeup, "Comp Makeup",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompMix, "Comp Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));

    // --- FX: Saturator ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kSatDrive, "Sat Drive",
        juce::NormalisableRange<float>(1.0f, 16.0f, 0.01f), 2.4f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kSatMix, "Sat Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.25f));

    // --- FX: Transient ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientAttack, "Transient Attack",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.15f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientSustain, "Transient Sustain",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientMix, "Transient Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));

    // --- FX: Reverb ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbSize,    "Reverb Size",    juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbSize));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbDamping, "Reverb Damping", juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbDamping));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbWidth,   "Reverb Width",   juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbWidth));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbMix,     "Reverb Mix",     juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbMix));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbPredelay, "Reverb Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    // --- FX: EQ ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqLowFreq,  "EQ Low Freq",
        juce::NormalisableRange<float>(40.0f, 600.0f, 0.1f, 0.4f), 200.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqLowGain,  "EQ Low Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidFreq,  "EQ Mid Freq",
        juce::NormalisableRange<float>(200.0f, 8000.0f, 0.1f, 0.35f), 1000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidGain,  "EQ Mid Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidQ,     "EQ Mid Q",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqHighFreq, "EQ High Freq",
        juce::NormalisableRange<float>(1000.0f, 16000.0f, 0.1f, 0.4f), 5000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqHighGain, "EQ High Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));

    // --- FX: Chorus ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusRate,  "Chorus Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusDepth, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusMix,   "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));

    // --- FX: Delay ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayTime,     "Delay Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 0.1f, 0.35f), 300.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayFeedback, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f), 0.30f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayMix,      "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));

    // --- FX: Limiter ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterThreshold, "Limiter Threshold",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.01f), mis::GlobalFxSettings{}.limiterThreshold));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterRelease,   "Limiter Release",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f), mis::GlobalFxSettings{}.limiterRelease));

    // FX enable toggles (true = active)
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab0_en",     "FX Saturator Enable",  true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab1_en",     "FX Transient Enable",  true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_eq_en",       "FX EQ Enable",         true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab2_en",     "FX Compressor Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_chorus_en",   "FX Chorus Enable",     false));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_delay_en",    "FX Delay Enable",      false));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab3_en",     "FX Reverb Enable",     true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_limiter_en",  "FX Limiter Enable",    true));

    // --- Per-instrument parameters (core + physical controls + output) ---
    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
    {
        const auto& bank = banks[static_cast<std::size_t>(inst)];
        const auto def = bank.empty()
                       ? mis::getDefaultSettings(inst)
                       : bank[0].settings;
        const auto prefix = juce::String(mis::getInstrumentName(inst)) + " ";

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "level"), prefix + "Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.level));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "tune"), prefix + "Tune",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), def.tuneSemitones));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "attack"), prefix + "Attack",
            juce::NormalisableRange<float>(0.0f, 2.0f, 0.0001f), def.attackSeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "decay"), prefix + "Decay",
            juce::NormalisableRange<float>(0.01f, 5.0f, 0.0001f), def.decaySeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "sustain"), prefix + "Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.sustainLevel));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "release"), prefix + "Release",
            juce::NormalisableRange<float>(0.01f, 5.0f, 0.0001f), def.releaseSeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "exciter"), prefix + "Exciter",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.exciter));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "body"), prefix + "Body",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.body));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "sympathetic"), prefix + "Sympathetic",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.sympathetic));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "noise"), prefix + "Noise",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.noiseAmount));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "drive"), prefix + "Drive",
            juce::NormalisableRange<float>(1.0f, 12.0f, 0.01f), def.drive));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "cutoff"), prefix + "Cutoff",
            juce::NormalisableRange<float>(120.0f, 18000.0f, 0.0f, 0.28f), def.cutoffHz));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "filter_q"), prefix + "Filter Q",
            juce::NormalisableRange<float>(0.1f, 12.0f, 0.01f), def.filterQ));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, "pan"), prefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), def.pan));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, kBreathPressureSuffix), prefix + "Breath Pressure",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.breathPressure));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, kBowSpeedSuffix), prefix + "Bow Speed",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.bowSpeed));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, kBowPressureSuffix), prefix + "Bow Pressure",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.bowPressure));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, kStrikePositionSuffix), prefix + "Strike Position",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.strikePosition));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstParamId(inst, kBrightnessSuffix), prefix + "Brightness",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.brightness));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeInstParamId(inst, kInstOutputSuffix), prefix + "Output",
            outputChoices, 0));
    }

    return layout;
}

juce::String InstrSynthAudioProcessor::makeInstParamId(int instrIndex, const juce::String& suffix)
{
    return "inst_" + juce::String(instrIndex) + "_" + suffix;
}

// =============================================================================
// Prepare / Release
// =============================================================================
void InstrSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = std::max(1.0, sampleRate);
    const int scratchSamples = juce::jmax(32768, samplesPerBlock);
    sustainPedalHeld.fill(false);
    clearSustainedNotes();
    latchedFxAvailability.fill(false);
    blockFxAvailability.fill(false);
    latchedFxOwnerInstrumentIndex = -1;
    blockRuntimeFxSettings = snapshotGlobalFxSettings();

    // Pre-allocate voice pool (no RT heap allocation)
    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
    {
        for (int j = 0; j < kMaxVoicesPerInstr; ++j)
        {
            if (!voiceBank[static_cast<std::size_t>(inst)][static_cast<std::size_t>(j)])
                voiceBank[static_cast<std::size_t>(inst)][static_cast<std::size_t>(j)]
                    = mis::createVoiceForInstrument(inst);
        }
        for (auto& inUse : voicePoolInUse[static_cast<std::size_t>(inst)])
            inUse.store(false, std::memory_order_release);
    }

    for (auto& slot : voices)
    {
        slot.voice = nullptr;
        slot.midiNote = -1;
        slot.instrumentIndex = -1;
        slot.poolSlot = -1;
        slot.activationAge = 0;
    }

    const juce::dsp::ProcessSpec spec {
        preparedSampleRate,
        static_cast<juce::uint32>(juce::jmax(1, scratchSamples)),
        static_cast<juce::uint32>(juce::jmax(1, getMainBusNumOutputChannels()))
    };

    compressor.reset();
    compressor.prepare(spec);
    compressor.setThreshold(-19.0f);
    compressor.setRatio(4.0f);
    compressor.setAttack(8.0f);
    compressor.setRelease(140.0f);

    compCache = CompressorCache{};
    fxDryBuffer.setSize(static_cast<int>(spec.numChannels),
                        static_cast<int>(spec.maximumBlockSize), false, true, true);
    transientFastEnv = { 0.0f, 0.0f };
    transientSlowEnv = { 0.0f, 0.0f };
    lfoPhase = 0.0f;

    reverbProcessor.prepare(preparedSampleRate, scratchSamples);
    eqProcessor.prepare(preparedSampleRate);
    chorusProcessor.prepare(preparedSampleRate, scratchSamples);
    delayProcessor.prepare(preparedSampleRate, scratchSamples);
    limiterProcessor.prepare(preparedSampleRate);
    satOversamplingMono.initProcessing(static_cast<std::size_t>(scratchSamples));
    satOversamplingStereo.initProcessing(static_cast<std::size_t>(scratchSamples));
    satOversamplingMono.reset();
    satOversamplingStereo.reset();

    outputGainSmoother.reset(preparedSampleRate, 0.02);
    outputGainSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(getParamValue(kOutputGain)));

    modulationMatrix.resetMidiSources();
    modulationMatrix.lfo2.reset();
}

void InstrSynthAudioProcessor::releaseResources()
{
    sustainPedalHeld.fill(false);
    clearSustainedNotes();
    for (auto& slot : voices)
    {
        slot.voice = nullptr;
        slot.midiNote = -1;
        slot.instrumentIndex = -1;
        slot.poolSlot = -1;
        slot.activationAge = 0;
    }
    for (auto& dv : dyingVoices)
    {
        dv.voice = nullptr;
        dv.instrumentIndex = -1;
        dv.poolSlot = -1;
        dv.outputBus = 0;
        dv.activationAge = 0;
    }
    for (auto& row : voicePoolInUse)
        for (auto& inUse : row)
            inUse.store(false, std::memory_order_release);
    satOversamplingMono.reset();
    satOversamplingStereo.reset();
    fxDryBuffer.setSize(0, 0);
}

bool InstrSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.outputBuses.isEmpty())
        return false;

    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono()
        && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    for (int busIndex = 1; busIndex < layouts.outputBuses.size(); ++busIndex)
    {
        const auto auxSet = layouts.getChannelSet(false, busIndex);
        if (auxSet.isDisabled())
            continue;
        if (auxSet != juce::AudioChannelSet::mono()
            && auxSet != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

// =============================================================================
// Process block
// =============================================================================
void InstrSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalBlockSamples = buffer.getNumSamples();
    const int maxChunkSamples = fxDryBuffer.getNumSamples();
    if (totalBlockSamples > 0 && maxChunkSamples <= 0)
    {
        jassertfalse;
        buffer.clear();
        midiMessages.clear();
        return;
    }

    if (totalBlockSamples > 0 && totalBlockSamples > maxChunkSamples)
    {
        for (int chunkStart = 0; chunkStart < totalBlockSamples; chunkStart += maxChunkSamples)
        {
            const int chunkSamples = juce::jmin(maxChunkSamples, totalBlockSamples - chunkStart);
            juce::AudioBuffer<float> chunkBuffer(buffer.getArrayOfWritePointers(),
                                                buffer.getNumChannels(),
                                                chunkStart,
                                                chunkSamples);
            oversizedChunkMidi.clear();
            for (const auto metadata : midiMessages)
            {
                const int samplePosition = metadata.samplePosition;
                if (samplePosition >= chunkStart && samplePosition < chunkStart + chunkSamples)
                    oversizedChunkMidi.addEvent(metadata.getMessage(), samplePosition - chunkStart);
            }
            processBlock(chunkBuffer, oversizedChunkMidi);
        }
        midiMessages.clear();
        return;
    }

    std::unique_lock<std::mutex> stateLock(stateMutex, std::try_to_lock);
    if (!stateLock.owns_lock())
    {
        buffer.clear();
        midiMessages.clear();
        return;
    }

    const auto outputBusCount = getBusCount(false);
    for (int busIndex = 0; busIndex < outputBusCount; ++busIndex)
        getBusBuffer(buffer, false, busIndex).clear();

    // Merge on-screen keyboard events
    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    const int instrIdx = juce::jlimit(0,
                                      mis::kNumInstruments - 1,
                                      pendingSelectedInstrumentIndex.load(std::memory_order_relaxed));

    // Handle MIDI
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const int midiChannel = normalizedMidiChannel(msg.getChannel());
        modulationMatrix.handleMidiMessage(msg);
        if (msg.isNoteOn())
            triggerNoteOn(instrIdx, midiChannel, msg.getNoteNumber(),
                          applyVelocityCurve(msg.getFloatVelocity(), velocityCurve));
        else if (msg.isNoteOff())
            triggerNoteOff(midiChannel, msg.getNoteNumber());
        else if (msg.isAllNotesOff())
            releaseVoices(midiChannel, false);
        else if (msg.isAllSoundOff())
            panicVoicesOnChannel(midiChannel);
        else if (msg.isController() && msg.getControllerNumber() == 120)
            panicVoicesOnChannel(midiChannel);
        else if (msg.isPitchWheel())
            pitchBend.setPitchWheel(msg.getPitchWheelValue());
        else if (msg.isControllerOfType(64))
        {
            if (msg.getControllerValue() >= 64)
            {
                sustainPedalHeld[static_cast<std::size_t>(midiChannel)] = true;
            }
            else
            {
                sustainPedalHeld[static_cast<std::size_t>(midiChannel)] = false;
                releaseSustainedNotesForChannel(midiChannel);
            }
        }
        else if (msg.isController())
            handleMidiCC(msg.getControllerNumber(), msg.getControllerValue(), instrIdx);
    }

    midiMessages.clear();

    // Compute modulation matrix for this block
    {
        const int lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(getParamValue(kLfoWave))));
        float lfo1Val = 0.0f;
        switch (lfoWave)
        {
            case 1: lfo1Val = 1.0f - 4.0f * std::abs(lfoPhase - 0.5f); break;
            case 2: lfo1Val = lfoPhase * 2.0f - 1.0f; break;
            case 3: lfo1Val = lfoPhase < 0.5f ? 1.0f : -1.0f; break;
            default: lfo1Val = mis::fastSin(lfoPhase); break;
        }
        modmatrix::ModContext modCtx;
        modCtx.lfo1       = lfo1Val;
        modCtx.lfo2       = modulationMatrix.lfo2.tickBlock(static_cast<float>(preparedSampleRate),
                                                            buffer.getNumSamples());
        modCtx.modWheel   = modulationMatrix.modWheelValue.load(std::memory_order_acquire);
        modCtx.aftertouch = modulationMatrix.aftertouchValue.load(std::memory_order_acquire);
        modCtx.pitchBend  = modulationMatrix.pitchBendValue.load(std::memory_order_acquire);
        cachedModResult   = modulationMatrix.process(modCtx);
    }

    // Render active voices into the requested stem bus, with fallback to master.
    auto mainBuffer = getBusBuffer(buffer, false, 0);
    for (auto& slot : voices)
    {
        if (slot.voice && slot.voice->isActive())
        {
            slot.voice->setPitchBendFactor(pitchBend.pitchBendFactor);

            int targetBus = juce::jlimit(0, outputBusCount - 1,
                static_cast<int>(std::round(getParamValue(
                    makeInstParamId(slot.instrumentIndex, kInstOutputSuffix)))));
            if (targetBus > 0 && getChannelCountOfBus(false, targetBus) <= 0)
                targetBus = 0;

            if (targetBus == 0)
            {
                slot.voice->render(mainBuffer, 0, mainBuffer.getNumSamples());
            }
            else
            {
                auto targetBuffer = getBusBuffer(buffer, false, targetBus);
                if (targetBuffer.getNumChannels() > 0 && targetBuffer.getNumSamples() > 0)
                    slot.voice->render(targetBuffer, 0, targetBuffer.getNumSamples());
                else
                    slot.voice->render(mainBuffer, 0, mainBuffer.getNumSamples());
            }
        }
    }

    // Return inactive voices to pool
    for (auto& slot : voices)
    {
        if (slot.voice && !slot.voice->isActive())
            clearVoice(slot);
    }

    // Render dying voices (stolen voices doing a quick fade-out)
    for (auto& dv : dyingVoices)
    {
        if (dv.voice == nullptr)
            continue;
        if (!dv.voice->isActive())
        {
            clearDyingVoice(dv);
            continue;
        }
        dv.voice->setPitchBendFactor(pitchBend.pitchBendFactor);

        int targetBus = juce::jlimit(0, outputBusCount - 1, dv.outputBus);
        if (targetBus > 0 && getChannelCountOfBus(false, targetBus) <= 0)
            targetBus = 0;

        if (targetBus == 0)
        {
            dv.voice->render(mainBuffer, 0, mainBuffer.getNumSamples());
        }
        else
        {
            auto targetBuffer = getBusBuffer(buffer, false, targetBus);
            if (targetBuffer.getNumChannels() > 0 && targetBuffer.getNumSamples() > 0)
                dv.voice->render(targetBuffer, 0, targetBuffer.getNumSamples());
            else
                dv.voice->render(mainBuffer, 0, mainBuffer.getNumSamples());
        }

        if (!dv.voice->isActive())
            clearDyingVoice(dv);
    }

    std::array<bool, 8> liveFxAvailability {};
    bool hasLiveVoicesForFx = false;
    int liveFxOwnerInstrumentIndex = -1;
    bool liveFxOwnersMixed = false;
    const auto registerFxOwner = [&](const int instrumentIndex)
    {
        if (instrumentIndex < 0 || instrumentIndex >= mis::kNumInstruments)
            return;

        hasLiveVoicesForFx = true;
        if (liveFxOwnerInstrumentIndex < 0)
            liveFxOwnerInstrumentIndex = instrumentIndex;
        else if (liveFxOwnerInstrumentIndex != instrumentIndex)
            liveFxOwnersMixed = true;

        for (int slotValue = 0; slotValue < static_cast<int>(liveFxAvailability.size()); ++slotValue)
        {
            const auto fxSlot = static_cast<mis::GlobalFxSlot>(slotValue);
            liveFxAvailability[static_cast<std::size_t>(slotValue)] =
                liveFxAvailability[static_cast<std::size_t>(slotValue)] || mis::isFxAvailable(instrumentIndex, fxSlot);
        }
    };

    for (const auto& slot : voices)
    {
        if (slot.voice != nullptr && slot.voice->isActive())
            registerFxOwner(slot.instrumentIndex);
    }

    for (const auto& dv : dyingVoices)
    {
        if (dv.voice != nullptr && dv.voice->isActive())
            registerFxOwner(dv.instrumentIndex);
    }

    const bool hasLatchedFx = std::any_of(latchedFxAvailability.begin(), latchedFxAvailability.end(),
                                          [] (const bool value) { return value; });
    if (hasLiveVoicesForFx)
    {
        for (std::size_t slot = 0; slot < blockFxAvailability.size(); ++slot)
        {
            blockFxAvailability[slot] = liveFxAvailability[slot] || latchedFxAvailability[slot];
            if (liveFxAvailability[slot])
                latchedFxAvailability[slot] = true;
        }
    }
    else if (hasLatchedFx)
    {
        blockFxAvailability = latchedFxAvailability;
    }
    else
    {
        for (int slotValue = 0; slotValue < static_cast<int>(blockFxAvailability.size()); ++slotValue)
            blockFxAvailability[static_cast<std::size_t>(slotValue)] =
                mis::isFxAvailable(instrIdx, static_cast<mis::GlobalFxSlot>(slotValue));
    }

    blockRuntimeFxSettings = snapshotGlobalFxSettings();
    if (hasLiveVoicesForFx && !liveFxOwnersMixed && liveFxOwnerInstrumentIndex >= 0)
    {
        latchedFxOwnerInstrumentIndex = liveFxOwnerInstrumentIndex;
        if (liveFxOwnerInstrumentIndex != instrIdx)
            blockRuntimeFxSettings = instrumentFxStates[static_cast<std::size_t>(liveFxOwnerInstrumentIndex)];
    }
    else if (!hasLiveVoicesForFx
             && hasLatchedFx
             && latchedFxOwnerInstrumentIndex >= 0
             && latchedFxOwnerInstrumentIndex < mis::kNumInstruments)
    {
        blockRuntimeFxSettings = instrumentFxStates[static_cast<std::size_t>(latchedFxOwnerInstrumentIndex)];
    }

    // Global FX on master only. Aux buses remain dry stems.
    if (mainBuffer.getNumChannels() > 0 && mainBuffer.getNumSamples() > 0)
    {
        processGlobalTransient(mainBuffer);
        processGlobalSaturator(mainBuffer);
        processGlobalEQ(mainBuffer);
        processGlobalCompressor(mainBuffer);
        processGlobalChorus(mainBuffer);
        processGlobalDelay(mainBuffer);
        applyGlobalLfo(mainBuffer);
        processGlobalReverb(mainBuffer);

        const auto targetOutputGain = juce::Decibels::decibelsToGain(getParamValue(kOutputGain));
        const auto startGain = outputGainSmoother.getCurrentValue();
        outputGainSmoother.setTargetValue(targetOutputGain);
        const auto endGain = outputGainSmoother.skip(mainBuffer.getNumSamples());
        for (int channel = 0; channel < mainBuffer.getNumChannels(); ++channel)
            mainBuffer.applyGainRamp(channel, 0, mainBuffer.getNumSamples(), startGain, endGain);
        processGlobalLimiter(mainBuffer);

        if (!hasLiveVoicesForFx)
        {
            const auto fxStillHasTail = [this](const mis::GlobalFxSlot slot)
            {
                switch (slot)
                {
                    case mis::GlobalFxSlot::Chorus:
                        return getParamValue("fx_chorus_en") >= 0.5f
                            && chorusProcessor.hasAudibleTail();
                    case mis::GlobalFxSlot::Delay:
                        return getParamValue("fx_delay_en") >= 0.5f
                            && delayProcessor.hasAudibleTail();
                    case mis::GlobalFxSlot::Reverb:
                        return getParamValue("fx_tab3_en") >= 0.5f
                            && reverbProcessor.hasAudibleTail();
                    default:
                        return false;
                }
            };

            for (int slotValue = 0; slotValue < static_cast<int>(latchedFxAvailability.size()); ++slotValue)
            {
                const auto fxSlot = static_cast<mis::GlobalFxSlot>(slotValue);
                if (!fxStillHasTail(fxSlot))
                    latchedFxAvailability[static_cast<std::size_t>(slotValue)] = false;
            }

            if (!std::any_of(latchedFxAvailability.begin(), latchedFxAvailability.end(),
                             [] (const bool value) { return value; }))
            {
                latchedFxOwnerInstrumentIndex = -1;
            }
        }
    }
}

// =============================================================================
// MIDI CC paged mapping
// =============================================================================
const char* InstrSynthAudioProcessor::getCCPageName(int page) noexcept
{
    if (page >= 0 && page < kNumCCPages)
        return kCCPageNames[page];
    return "???";
}

void InstrSynthAudioProcessor::handleMidiCC(int ccNumber, int ccValue, int instrIndex)
{
    if (instrIndex < 0 || instrIndex >= mis::kNumInstruments)
        return;

    // --- Mod wheel always controls warmth ---
    if (ccNumber == 1)
    {
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(kMacroWarmth)))
                queueParamUpdate(parameter, static_cast<float>(ccValue) / 127.0f);
        return;
    }

    // --- Page navigation: CC 102 = prev, CC 103 = next ---
    if (ccNumber == 102 && ccValue > 0)
    {
        int p = midiCCPage.load(std::memory_order_relaxed);
        midiCCPage.store((p + kNumCCPages - 1) % kNumCCPages, std::memory_order_relaxed);
        return;
    }
    if (ccNumber == 103 && ccValue > 0)
    {
        int p = midiCCPage.load(std::memory_order_relaxed);
        midiCCPage.store((p + 1) % kNumCCPages, std::memory_order_relaxed);
        return;
    }

    // --- Direct page select via pads: CC 44-50 ---
    if (ccNumber >= 44 && ccNumber <= 50 && ccValue > 0)
    {
        midiCCPage.store(ccNumber - 44, std::memory_order_relaxed);
        return;
    }

    // --- Knob mapping: CC 21-28 → paged parameters ---
    if (ccNumber < 21 || ccNumber > 28)
        return;

    const int knobIndex = ccNumber - 21;
    const int page = midiCCPage.load(std::memory_order_relaxed);
    const auto& slot = kCCPages[page][knobIndex];

    juce::String paramId;
    if (slot.paramId != nullptr)
        paramId = slot.paramId;
    else if (slot.instrSuffix != nullptr)
        paramId = makeInstParamId(instrIndex, slot.instrSuffix);
    else
        return;

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId)))
    {
        const float normalised = static_cast<float>(ccValue) / 127.0f;
        queueParamUpdate(parameter, normalised);
    }
}

// =============================================================================
// Editor
// =============================================================================
juce::AudioProcessorEditor* InstrSynthAudioProcessor::createEditor()
{
    return new InstrSynthAudioProcessorEditor(*this);
}

double InstrSynthAudioProcessor::getTailLengthSeconds() const { return 15.0; }

// =============================================================================
// Programs / presets
// =============================================================================
int InstrSynthAudioProcessor::getNumPrograms()
{
    const int inst = getSelectedInstrumentIndex();
    return juce::jmax(1, static_cast<int>(factoryPresetBanks[static_cast<std::size_t>(inst)].size()));
}

int InstrSynthAudioProcessor::getCurrentProgram()
{
    const int inst = getSelectedInstrumentIndex();
    return juce::jmax(0, currentPresetIndices[static_cast<std::size_t>(inst)]);
}

void InstrSynthAudioProcessor::setCurrentProgram(int index) { applyFactoryPreset(index); }

const juce::String InstrSynthAudioProcessor::getProgramName(int index)
{
    const int inst = getSelectedInstrumentIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(inst)];
    if (index < 0 || index >= static_cast<int>(bank.size())) return {};
    return juce::String(juce::CharPointer_UTF8(bank[static_cast<std::size_t>(index)].name.c_str()));
}

void InstrSynthAudioProcessor::changeProgramName(int, const juce::String&) {}

// =============================================================================
// State
// =============================================================================
void InstrSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    storeCurrentInstrumentFxState(getSelectedInstrumentIndex());
    auto state = parameters.copyState();

    for (int childIndex = state.getNumChildren(); --childIndex >= 0;)
    {
        if (state.getChild(childIndex).hasType("inst_fx_state"))
            state.removeChild(childIndex, nullptr);
    }

    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
        state.setProperty("pi_" + juce::String(inst),
                          currentPresetIndices[static_cast<std::size_t>(inst)], nullptr);

    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
    {
        auto& f = currentUserPresetFiles[static_cast<std::size_t>(inst)];
        if (f.existsAsFile())
            state.setProperty("upf_" + juce::String(inst),
                              f.getFullPathName(), nullptr);

        juce::ValueTree fxState("inst_fx_state");
        fxState.setProperty("inst", inst, nullptr);
        writeFxStateProperties(fxState, instrumentFxStates[static_cast<std::size_t>(inst)]);
        state.appendChild(fxState, nullptr);
    }

    if (auto xml = state.createXml())
    {
        modulationMatrix.saveToXml(*xml);
        copyXmlToBinary(*xml, destData);
    }
}

void InstrSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState == nullptr || !xmlState->hasTagName(parameters.state.getType()))
        return;

    const std::lock_guard<std::mutex> stateLock(stateMutex);
    modulationMatrix.loadFromXml(*xmlState);

    auto restoredState = juce::ValueTree::fromXml(*xmlState);
    cancelPendingUpdate();
    isRestoringState.store(true, std::memory_order_release);
    sanitizeStateParameterValues(parameters, restoredState);
    parameters.replaceState(restoredState);
    isRestoringState.store(false, std::memory_order_release);

    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
    {
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(inst)];
        instrumentFxStates[static_cast<std::size_t>(inst)] = bank.empty()
            ? mis::GlobalFxSettings{}
            : bank[0].fx;
        currentUserPresetFiles[static_cast<std::size_t>(inst)] = juce::File{};
    }

    const int selectedInstrument = juce::jlimit(0, mis::kNumInstruments - 1, getSelectedInstrumentIndex());
    instrumentFxStates[static_cast<std::size_t>(selectedInstrument)] = snapshotGlobalFxSettings();

    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
    {
        int pi = static_cast<int>(restoredState.getProperty("pi_" + juce::String(inst), 0));
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(inst)];
        currentPresetIndices[static_cast<std::size_t>(inst)] =
            juce::jlimit(0, juce::jmax(0, static_cast<int>(bank.size()) - 1), pi);

        auto upfKey = "upf_" + juce::String(inst);
        auto path   = restoredState.getProperty(upfKey, "").toString();
        if (path.isNotEmpty())
        {
            juce::File f(path);
            if (f.existsAsFile())
                currentUserPresetFiles[static_cast<std::size_t>(inst)] = f;
        }
    }

    std::array<bool, mis::kNumInstruments> restoredFxStateSeen {};
    for (int childIndex = 0; childIndex < restoredState.getNumChildren(); ++childIndex)
    {
        const auto child = restoredState.getChild(childIndex);
        if (!child.hasType("inst_fx_state"))
            continue;

        int inst = -1;
        if (!tryReadValidatedChildInstrumentIndex(child, inst))
            continue;

        if (restoredFxStateSeen[static_cast<std::size_t>(inst)])
            continue;

        restoredFxStateSeen[static_cast<std::size_t>(inst)] = true;
        instrumentFxStates[static_cast<std::size_t>(inst)] = readValidatedFxStateProperties(
            parameters,
            child,
            instrumentFxStates[static_cast<std::size_t>(inst)]);
    }

    cachedSelectedInstrumentIndex = selectedInstrument;
    pendingSelectedInstrumentIndex.store(selectedInstrument);
    cancelPendingUpdate();
    applyGlobalFxSettings(selectedInstrument,
                          instrumentFxStates[static_cast<std::size_t>(selectedInstrument)],
                          false);
}

// =============================================================================
// Factory preset management
// =============================================================================
juce::StringArray InstrSynthAudioProcessor::getFactoryPresetNames() const
{
    const int inst = getSelectedInstrumentIndex();
    juce::StringArray names;
    for (const auto& p : factoryPresetBanks[static_cast<std::size_t>(inst)])
        names.add(juce::String(juce::CharPointer_UTF8(p.name.c_str())));
    return names;
}

int InstrSynthAudioProcessor::getCurrentFactoryPresetIndex() const noexcept
{
    return currentPresetIndices[static_cast<std::size_t>(getSelectedInstrumentIndex())];
}

void InstrSynthAudioProcessor::applyFactoryPreset(int presetIndex)
{
    const int inst = getSelectedInstrumentIndex();
    const auto& states = factoryPresetStates[static_cast<std::size_t>(inst)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(states.size()))
        return;

    applyPresetPersistenceState(states[static_cast<std::size_t>(presetIndex)]);
    currentPresetIndices[static_cast<std::size_t>(inst)] = presetIndex;
    currentUserPresetFiles[static_cast<std::size_t>(inst)] = juce::File{};
}

bool InstrSynthAudioProcessor::saveFactoryPreset(int presetIndex)
{
    const int inst = getSelectedInstrumentIndex();
    auto& bank = factoryPresetBanks[static_cast<std::size_t>(inst)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return false;

    auto state = captureCurrentPresetState(inst);
    state.presetIndex = presetIndex;
    state.name = juce::String(juce::CharPointer_UTF8(bank[static_cast<std::size_t>(presetIndex)].name.c_str()));
    state.metadata = makeFactoryMetadata(inst, bank[static_cast<std::size_t>(presetIndex)]);
    factoryPresetStates[static_cast<std::size_t>(inst)][static_cast<std::size_t>(presetIndex)] = state;

    bank[static_cast<std::size_t>(presetIndex)].settings = state.settings;
    bank[static_cast<std::size_t>(presetIndex)].fx = state.fx;
    bank[static_cast<std::size_t>(presetIndex)].outputBus = state.outputBus;
    bank[static_cast<std::size_t>(presetIndex)].performance = state.performance;
    bank[static_cast<std::size_t>(presetIndex)].nominalPeakDb = state.metadata.nominalPeakDb;

    auto dir = getFactoryOverridesDirectory().getChildFile("inst_" + juce::String(inst));
    dir.createDirectory();
    auto file = dir.getChildFile(juce::String(presetIndex) + ".xml");
    auto root = createPresetXml("InstrFactoryPreset", state);
    return root != nullptr && root->writeTo(file);
}
void InstrSynthAudioProcessor::loadFactoryOverrides()
{
    auto baseDir = getFactoryOverridesDirectory();
    for (int inst = 0; inst < mis::kNumInstruments; ++inst)
    {
        auto dir = baseDir.getChildFile("inst_" + juce::String(inst));
        if (!dir.isDirectory()) continue;
        auto& bank = factoryPresetBanks[static_cast<std::size_t>(inst)];
        for (int i = 0; i < static_cast<int>(bank.size()); ++i)
        {
            auto file = dir.getChildFile(juce::String(i) + ".xml");
            if (!file.existsAsFile()) continue;
            auto xml = juce::XmlDocument::parse(file);
            if (xml == nullptr)
                continue;

            auto state = makeFactoryPresetState(inst, i, bank[static_cast<std::size_t>(i)]);
            if (!parsePresetXml(parameters, *xml, "InstrFactoryPreset", inst, state, true, &state))
                continue;

            state.presetIndex = i;
            if (state.name.isEmpty())
                state.name = juce::String(juce::CharPointer_UTF8(bank[static_cast<std::size_t>(i)].name.c_str()));
            if (state.metadata.family.isEmpty())
                state.metadata = makeFactoryMetadata(inst, bank[static_cast<std::size_t>(i)]);

            factoryPresetStates[static_cast<std::size_t>(inst)][static_cast<std::size_t>(i)] = state;
            bank[static_cast<std::size_t>(i)].settings = state.settings;
            bank[static_cast<std::size_t>(i)].fx = state.fx;
            bank[static_cast<std::size_t>(i)].outputBus = state.outputBus;
            bank[static_cast<std::size_t>(i)].performance = state.performance;
            bank[static_cast<std::size_t>(i)].nominalPeakDb = state.metadata.nominalPeakDb;

            if (shouldRewritePresetXml(*xml, true))
            {
                auto rewrite = createPresetXml("InstrFactoryPreset", state);
                if (rewrite != nullptr)
                    rewrite->writeTo(file);
            }
        }
    }
}

// =============================================================================
// User presets (same pattern as MDS)
// =============================================================================
juce::File InstrSynthAudioProcessor::getFactoryOverridesDirectory()
{
    return findWritableDirectory(
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("MusiqueInstrSynth")
            .getChildFile("FactoryOverrides"),
        "MusiqueInstrSynth/FactoryOverrides");
}

juce::File InstrSynthAudioProcessor::getUserPresetsDirectory(int instrIndex)
{
    return findWritableDirectory(
        juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("MusiqueInstrSynth")
            .getChildFile("Presets")
            .getChildFile("inst_" + juce::String(instrIndex)),
        "MusiqueInstrSynth/Presets/inst_" + juce::String(instrIndex));
}

juce::Array<juce::File> InstrSynthAudioProcessor::scanUserPresets() const
{
    juce::Array<juce::File> results;
    auto dir = getUserPresetsDirectory(getSelectedInstrumentIndex());
    if (dir.isDirectory())
        dir.findChildFiles(results, juce::File::findFiles, false, "*.xml");
    results.sort();
    return results;
}

bool InstrSynthAudioProcessor::saveUserPreset(const juce::String& name)
{
    if (name.isEmpty()) return false;
    const int inst = getSelectedInstrumentIndex();
    auto file = getUserPresetsDirectory(inst).getChildFile(
        juce::File::createLegalFileName(name) + ".xml");

    auto state = captureCurrentPresetState(inst);
    state.name = name;
    auto root = createPresetXml("InstrPreset", state);
    if (root != nullptr && writePresetWithManifestRollback(file, *root, name, inst, mis::getInstrumentName(inst)))
    {
        currentUserPresetFiles[static_cast<std::size_t>(inst)] = file;
        currentPresetIndices[static_cast<std::size_t>(inst)] = -1;
        return true;
    }
    return false;
}

bool InstrSynthAudioProcessor::updateUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int inst = getSelectedInstrumentIndex();
    auto state = captureCurrentPresetState(inst);
    state.name = file.getFileNameWithoutExtension();
    auto root = createPresetXml("InstrPreset", state);
    if (root != nullptr && writePresetWithManifestRollback(file,
                                        *root,
                                        file.getFileNameWithoutExtension(),
                                        inst,
                                        mis::getInstrumentName(inst)))
    {
        currentUserPresetFiles[static_cast<std::size_t>(inst)] = file;
        currentPresetIndices[static_cast<std::size_t>(inst)] = -1;
        return true;
    }
    return false;
}

bool InstrSynthAudioProcessor::deleteUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int inst = getSelectedInstrumentIndex();
    if (currentUserPresetFiles[static_cast<std::size_t>(inst)] == file)
        currentUserPresetFiles[static_cast<std::size_t>(inst)] = juce::File{};
    musique::preset::manifestFileForPresetFile(file).deleteFile();
    return file.deleteFile();
}

bool InstrSynthAudioProcessor::loadUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr || !xml->hasTagName("InstrPreset")) return false;

    const int inst = getSelectedInstrumentIndex();
    auto state = makeDefaultPresetState(inst);
    if (!parsePresetXml(parameters, *xml, "InstrPreset", inst, state, false, &state))
        return false;

    applyPresetPersistenceState(state);

    currentUserPresetFiles[static_cast<std::size_t>(inst)] = file;
    currentPresetIndices[static_cast<std::size_t>(inst)] = -1;
    writePresetManifest(file,
                        xml->getStringAttribute("name", file.getFileNameWithoutExtension()),
                        inst,
                        mis::getInstrumentName(inst));
    if (shouldRewritePresetXml(*xml, false))
    {
        auto rewrite = createPresetXml("InstrPreset", state);
        if (rewrite != nullptr)
            writePresetWithManifestRollback(file,
                                            *rewrite,
                                            state.name.isNotEmpty() ? state.name : file.getFileNameWithoutExtension(),
                                            inst,
                                            mis::getInstrumentName(inst));
    }
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
    return true;
}

bool InstrSynthAudioProcessor::isCurrentPresetUser() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedInstrumentIndex())].existsAsFile();
}

juce::File InstrSynthAudioProcessor::getCurrentUserPresetFile() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedInstrumentIndex())];
}

// =============================================================================
// Voice management
// =============================================================================
int InstrSynthAudioProcessor::getSelectedInstrumentIndex() const
{
    return juce::jlimit(0, mis::kNumInstruments - 1,
                        static_cast<int>(std::round(getParamValue(kSelectedInstrument))));
}

// =============================================================================
// Modulation Matrix accessors
// =============================================================================
modmatrix::ModSlot InstrSynthAudioProcessor::getModMatrixSlot(int index) const
{
    return modulationMatrix.getSlot(juce::jlimit(0, modmatrix::ModulationMatrix::getNumSlots() - 1, index));
}

void InstrSynthAudioProcessor::setModMatrixSlot(int index,
                                                modmatrix::Source source,
                                                modmatrix::Destination destination,
                                                float amount)
{
    modulationMatrix.setSlot(juce::jlimit(0, modmatrix::ModulationMatrix::getNumSlots() - 1, index),
                             source, destination, amount);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withParameterInfoChanged(true));
}

float InstrSynthAudioProcessor::getModMatrixLfo2Rate() const noexcept
{
    return modulationMatrix.lfo2.getRate();
}

int InstrSynthAudioProcessor::getModMatrixLfo2Wave() const noexcept
{
    return modulationMatrix.lfo2.getWave();
}

void InstrSynthAudioProcessor::setModMatrixLfo2Rate(float rateHz)
{
    modulationMatrix.lfo2.setRate(juce::jlimit(0.05f, 12.0f, rateHz));
}

void InstrSynthAudioProcessor::setModMatrixLfo2Wave(int waveformIndex)
{
    modulationMatrix.lfo2.setWave(juce::jlimit(0, 3, waveformIndex));
}

float InstrSynthAudioProcessor::getParamValue(const juce::String& paramId) const
{
    if (const auto* raw = parameters.getRawParameterValue(paramId))
        return raw->load();
    return 0.0f;
}

void InstrSynthAudioProcessor::setParamValueInternal(const juce::String& paramId, float value, bool notifyHost)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId)))
    {
        const auto normalised = parameter->convertTo0to1(value);
        if (notifyHost)
        {
            parameter->setValueNotifyingHost(normalised);
        }
        else
        {
            parameter->setValue(normalised);
            parameter->sendValueChangedMessageToListeners(normalised);
        }
    }
}

void InstrSynthAudioProcessor::setParamValue(const juce::String& paramId, float value)
{
    setParamValueInternal(paramId, value, true);
}

void InstrSynthAudioProcessor::queueParamUpdate(juce::RangedAudioParameter* parameter, float normalisedValue)
{
    if (parameter == nullptr)
        return;

    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    pendingParamUpdateFifo.prepareToWrite(1, start1, size1, start2, size2);
    if (size1 <= 0)
        return;

    pendingParamUpdates[static_cast<std::size_t>(start1)] = { parameter, normalisedValue };
    pendingParamUpdateFifo.finishedWrite(size1);
    triggerAsyncUpdate();
}

mis::GlobalFxSettings InstrSynthAudioProcessor::snapshotGlobalFxSettings() const
{
    mis::GlobalFxSettings fx;
    fx.satDrive          = getParamValue("sat_drive");
    fx.satMix            = getParamValue("sat_mix");
    fx.transientAttack   = getParamValue("transient_attack");
    fx.transientSustain  = getParamValue("transient_sustain");
    fx.transientMix      = getParamValue("transient_mix");
    fx.eqLowFreq         = getParamValue("eq_low_freq");
    fx.eqLowGain         = getParamValue("eq_low_gain");
    fx.eqMidFreq         = getParamValue("eq_mid_freq");
    fx.eqMidGain         = getParamValue("eq_mid_gain");
    fx.eqMidQ            = getParamValue("eq_mid_q");
    fx.eqHighFreq        = getParamValue("eq_high_freq");
    fx.eqHighGain        = getParamValue("eq_high_gain");
    fx.compThreshold     = getParamValue("comp_threshold");
    fx.compRatio         = getParamValue("comp_ratio");
    fx.compAttack        = getParamValue("comp_attack");
    fx.compRelease       = getParamValue("comp_release");
    fx.compMakeup        = getParamValue("comp_makeup");
    fx.compMix           = getParamValue("comp_mix");
    fx.chorusRate        = getParamValue("chorus_rate");
    fx.chorusDepth       = getParamValue("chorus_depth");
    fx.chorusMix         = getParamValue("chorus_mix");
    fx.delayTime         = getParamValue("delay_time");
    fx.delayFeedback     = getParamValue("delay_feedback");
    fx.delayMix          = getParamValue("delay_mix");
    fx.reverbSize        = getParamValue("reverb_size");
    fx.reverbDamping     = getParamValue("reverb_damping");
    fx.reverbWidth       = getParamValue("reverb_width");
    fx.reverbMix         = getParamValue("reverb_mix");
    fx.reverbPredelay    = getParamValue("reverb_predelay");
    fx.limiterThreshold  = getParamValue("limiter_threshold");
    fx.limiterRelease    = getParamValue("limiter_release");
    fx.saturatorOn       = getParamValue("fx_tab0_en") >= 0.5f;
    fx.transientOn       = getParamValue("fx_tab1_en") >= 0.5f;
    fx.eqOn              = getParamValue("fx_eq_en") >= 0.5f;
    fx.compressorOn      = getParamValue("fx_tab2_en") >= 0.5f;
    fx.chorusOn          = getParamValue("fx_chorus_en") >= 0.5f;
    fx.delayOn           = getParamValue("fx_delay_en") >= 0.5f;
    fx.reverbOn          = getParamValue("fx_tab3_en") >= 0.5f;
    fx.limiterOn         = getParamValue("fx_limiter_en") >= 0.5f;
    return fx;
}

InstrSynthAudioProcessor::PresetPersistenceState
InstrSynthAudioProcessor::captureCurrentPresetState(int instrIndex) const
{
    auto state = makeDefaultPresetState(instrIndex);
    const auto family = mis::getFamily(state.instrIndex);

    state.settings.level = getParamValue(makeInstParamId(state.instrIndex, "level"));
    state.settings.tuneSemitones = getParamValue(makeInstParamId(state.instrIndex, "tune"));
    state.settings.attackSeconds = getParamValue(makeInstParamId(state.instrIndex, "attack"));
    state.settings.decaySeconds = getParamValue(makeInstParamId(state.instrIndex, "decay"));
    state.settings.sustainLevel = getParamValue(makeInstParamId(state.instrIndex, "sustain"));
    state.settings.releaseSeconds = getParamValue(makeInstParamId(state.instrIndex, "release"));
    state.settings.exciter = getParamValue(makeInstParamId(state.instrIndex, "exciter"));
    state.settings.body = getParamValue(makeInstParamId(state.instrIndex, "body"));
    state.settings.sympathetic = getParamValue(makeInstParamId(state.instrIndex, "sympathetic"));
    state.settings.noiseAmount = getParamValue(makeInstParamId(state.instrIndex, "noise"));
    state.settings.drive = getParamValue(makeInstParamId(state.instrIndex, "drive"));
    state.settings.cutoffHz = getParamValue(makeInstParamId(state.instrIndex, "cutoff"));
    state.settings.filterQ = getParamValue(makeInstParamId(state.instrIndex, "filter_q"));
    state.settings.pan = getParamValue(makeInstParamId(state.instrIndex, "pan"));
    if (isParameterApplicableToFamily(family, kBreathPressureSuffix))
        state.settings.breathPressure = getParamValue(makeInstParamId(state.instrIndex, kBreathPressureSuffix));
    if (isParameterApplicableToFamily(family, kBowSpeedSuffix))
        state.settings.bowSpeed = getParamValue(makeInstParamId(state.instrIndex, kBowSpeedSuffix));
    if (isParameterApplicableToFamily(family, kBowPressureSuffix))
        state.settings.bowPressure = getParamValue(makeInstParamId(state.instrIndex, kBowPressureSuffix));
    if (isParameterApplicableToFamily(family, kStrikePositionSuffix))
        state.settings.strikePosition = getParamValue(makeInstParamId(state.instrIndex, kStrikePositionSuffix));
    if (isParameterApplicableToFamily(family, kBrightnessSuffix))
        state.settings.brightness = getParamValue(makeInstParamId(state.instrIndex, kBrightnessSuffix));

    state.outputBus = juce::jlimit(
        0, kNumAuxOutputs,
        static_cast<int>(std::round(getParamValue(makeInstParamId(state.instrIndex, kInstOutputSuffix)))));
    state.performance.lfoRate = getParamValue(kLfoRate);
    state.performance.lfoDepth = getParamValue(kLfoDepth);
    state.performance.lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(getParamValue(kLfoWave))));
    state.performance.macroWarmth = getParamValue(kMacroWarmth);
    state.performance.macroBrightness = getParamValue(kMacroBrightness);
    state.performance.macroExpression = getParamValue(kMacroExpression);
    state.performance.macroTexture = getParamValue(kMacroTexture);
    state.fx = snapshotGlobalFxSettings();
    state.modMatrix = modulationMatrix.captureState();
    state.metadata = makeUserMetadata(state.instrIndex);
    return state;
}

InstrSynthAudioProcessor::PresetPersistenceState
InstrSynthAudioProcessor::makeFactoryPresetState(int instrIndex,
                                                 int presetIndex,
                                                 const mis::InstrumentPreset& preset) const
{
    auto state = makeDefaultPresetState(instrIndex);
    state.name = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    state.instrIndex = juce::jlimit(0, mis::kNumInstruments - 1, instrIndex);
    state.presetIndex = presetIndex;
    state.settings = preset.settings;
    state.fx = preset.fx;
    state.outputBus = juce::jlimit(0, kNumAuxOutputs, preset.outputBus);
    state.performance = preset.performance;
    state.metadata = makeFactoryMetadata(state.instrIndex, preset);
    return state;
}

void InstrSynthAudioProcessor::applyPresetPersistenceState(const PresetPersistenceState& state)
{
    const auto instrIndex = juce::jlimit(0, mis::kNumInstruments - 1, state.instrIndex);
    applyInstPresetSettings(instrIndex, state.settings);
    setParamValue(makeInstParamId(instrIndex, kInstOutputSuffix),
                  static_cast<float>(juce::jlimit(0, kNumAuxOutputs, state.outputBus)));
    setParamValue(kLfoRate, juce::jlimit(0.05f, 12.0f, state.performance.lfoRate));
    setParamValue(kLfoDepth, clamp01(state.performance.lfoDepth));
    setParamValue(kLfoWave, static_cast<float>(juce::jlimit(0, 3, state.performance.lfoWave)));
    setParamValue(kMacroWarmth, clamp01(state.performance.macroWarmth));
    setParamValue(kMacroBrightness, clamp01(state.performance.macroBrightness));
    setParamValue(kMacroExpression, clamp01(state.performance.macroExpression));
    setParamValue(kMacroTexture, clamp01(state.performance.macroTexture));

    instrumentFxStates[static_cast<std::size_t>(instrIndex)] = state.fx;
    applyGlobalFxSettings(instrIndex, state.fx);
    modulationMatrix.applyState(state.modMatrix);
    modulationMatrix.resetMidiSources();
}

void InstrSynthAudioProcessor::applyGlobalFxSettings(const int instrIndex,
                                                     const mis::GlobalFxSettings& settings,
                                                     bool notifyHost)
{
    const auto masked = mis::maskUnavailableFx(instrIndex, settings);
    setParamValueInternal("sat_drive",         masked.satDrive, notifyHost);
    setParamValueInternal("sat_mix",           masked.satMix, notifyHost);
    setParamValueInternal("transient_attack",  masked.transientAttack, notifyHost);
    setParamValueInternal("transient_sustain", masked.transientSustain, notifyHost);
    setParamValueInternal("transient_mix",     masked.transientMix, notifyHost);
    setParamValueInternal("eq_low_freq",       masked.eqLowFreq, notifyHost);
    setParamValueInternal("eq_low_gain",       masked.eqLowGain, notifyHost);
    setParamValueInternal("eq_mid_freq",       masked.eqMidFreq, notifyHost);
    setParamValueInternal("eq_mid_gain",       masked.eqMidGain, notifyHost);
    setParamValueInternal("eq_mid_q",          masked.eqMidQ, notifyHost);
    setParamValueInternal("eq_high_freq",      masked.eqHighFreq, notifyHost);
    setParamValueInternal("eq_high_gain",      masked.eqHighGain, notifyHost);
    setParamValueInternal("comp_threshold",    masked.compThreshold, notifyHost);
    setParamValueInternal("comp_ratio",        masked.compRatio, notifyHost);
    setParamValueInternal("comp_attack",       masked.compAttack, notifyHost);
    setParamValueInternal("comp_release",      masked.compRelease, notifyHost);
    setParamValueInternal("comp_makeup",       masked.compMakeup, notifyHost);
    setParamValueInternal("comp_mix",          masked.compMix, notifyHost);
    setParamValueInternal("chorus_rate",       masked.chorusRate, notifyHost);
    setParamValueInternal("chorus_depth",      masked.chorusDepth, notifyHost);
    setParamValueInternal("chorus_mix",        masked.chorusMix, notifyHost);
    setParamValueInternal("delay_time",        masked.delayTime, notifyHost);
    setParamValueInternal("delay_feedback",    masked.delayFeedback, notifyHost);
    setParamValueInternal("delay_mix",         masked.delayMix, notifyHost);
    setParamValueInternal("reverb_size",       masked.reverbSize, notifyHost);
    setParamValueInternal("reverb_damping",    masked.reverbDamping, notifyHost);
    setParamValueInternal("reverb_width",      masked.reverbWidth, notifyHost);
    setParamValueInternal("reverb_mix",        masked.reverbMix, notifyHost);
    setParamValueInternal("reverb_predelay",   masked.reverbPredelay, notifyHost);
    setParamValueInternal("limiter_threshold", masked.limiterThreshold, notifyHost);
    setParamValueInternal("limiter_release",   masked.limiterRelease, notifyHost);
    setParamValueInternal("fx_tab0_en",        masked.saturatorOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab1_en",        masked.transientOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_eq_en",          masked.eqOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab2_en",        masked.compressorOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_chorus_en",      masked.chorusOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_delay_en",       masked.delayOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab3_en",        masked.reverbOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_limiter_en",     masked.limiterOn ? 1.0f : 0.0f, notifyHost);
}

void InstrSynthAudioProcessor::storeCurrentInstrumentFxState(const int instrIndex)
{
    if (instrIndex < 0 || instrIndex >= mis::kNumInstruments)
        return;

    instrumentFxStates[static_cast<std::size_t>(instrIndex)] = snapshotGlobalFxSettings();
}

void InstrSynthAudioProcessor::restoreInstrumentFxState(const int instrIndex)
{
    if (instrIndex < 0 || instrIndex >= mis::kNumInstruments)
        return;

    applyGlobalFxSettings(instrIndex,
                          instrumentFxStates[static_cast<std::size_t>(instrIndex)],
                          false);
}

bool InstrSynthAudioProcessor::isFxAvailableForCurrentInstrument(const mis::GlobalFxSlot slot) const
{
    return blockFxAvailability[fxSlotIndex(slot)];
}

void InstrSynthAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID != kSelectedInstrument)
        return;

    pendingSelectedInstrumentIndex.store(juce::jlimit(
        0, mis::kNumInstruments - 1, static_cast<int>(std::round(newValue))));

    if (isRestoringState.load(std::memory_order_acquire))
        return;

    triggerAsyncUpdate();
}

void InstrSynthAudioProcessor::handleAsyncUpdate()
{
    int start1 = 0;
    int size1 = 0;
    int start2 = 0;
    int size2 = 0;
    pendingParamUpdateFifo.prepareToRead(pendingParamUpdateFifo.getNumReady(), start1, size1, start2, size2);
    for (int index = 0; index < size1; ++index)
    {
        const auto& update = pendingParamUpdates[static_cast<std::size_t>(start1 + index)];
        if (update.parameter != nullptr)
            update.parameter->setValueNotifyingHost(update.normalisedValue);
    }
    for (int index = 0; index < size2; ++index)
    {
        const auto& update = pendingParamUpdates[static_cast<std::size_t>(start2 + index)];
        if (update.parameter != nullptr)
            update.parameter->setValueNotifyingHost(update.normalisedValue);
    }
    pendingParamUpdateFifo.finishedRead(size1 + size2);

    const int newInstrumentIndex = pendingSelectedInstrumentIndex.load();
    if (newInstrumentIndex == cachedSelectedInstrumentIndex)
        return;

    storeCurrentInstrumentFxState(cachedSelectedInstrumentIndex);
    cachedSelectedInstrumentIndex = newInstrumentIndex;
    restoreInstrumentFxState(newInstrumentIndex);
}

mis::InstrumentSettings InstrSynthAudioProcessor::snapshotInstrumentSettings(int instrIndex) const
{
    auto s = mis::getDefaultSettings(instrIndex);
    const auto family = mis::getFamily(instrIndex);
    s.level          = getParamValue(makeInstParamId(instrIndex, "level"));
    s.tuneSemitones  = getParamValue(makeInstParamId(instrIndex, "tune"));
    s.attackSeconds  = getParamValue(makeInstParamId(instrIndex, "attack"));
    s.decaySeconds   = getParamValue(makeInstParamId(instrIndex, "decay"));
    s.sustainLevel   = getParamValue(makeInstParamId(instrIndex, "sustain"));
    s.releaseSeconds = getParamValue(makeInstParamId(instrIndex, "release"));
    s.exciter        = getParamValue(makeInstParamId(instrIndex, "exciter"));
    s.body           = getParamValue(makeInstParamId(instrIndex, "body"));
    s.sympathetic    = getParamValue(makeInstParamId(instrIndex, "sympathetic"));
    s.noiseAmount    = getParamValue(makeInstParamId(instrIndex, "noise"));
    s.drive          = getParamValue(makeInstParamId(instrIndex, "drive"));
    s.cutoffHz       = getParamValue(makeInstParamId(instrIndex, "cutoff"));
    s.filterQ        = getParamValue(makeInstParamId(instrIndex, "filter_q"));
    s.pan            = getParamValue(makeInstParamId(instrIndex, "pan"));
    if (isParameterApplicableToFamily(family, kBreathPressureSuffix))
        s.breathPressure = getParamValue(makeInstParamId(instrIndex, kBreathPressureSuffix));
    if (isParameterApplicableToFamily(family, kBowSpeedSuffix))
        s.bowSpeed = getParamValue(makeInstParamId(instrIndex, kBowSpeedSuffix));
    if (isParameterApplicableToFamily(family, kBowPressureSuffix))
        s.bowPressure = getParamValue(makeInstParamId(instrIndex, kBowPressureSuffix));
    if (isParameterApplicableToFamily(family, kStrikePositionSuffix))
        s.strikePosition = getParamValue(makeInstParamId(instrIndex, kStrikePositionSuffix));
    if (isParameterApplicableToFamily(family, kBrightnessSuffix))
        s.brightness = getParamValue(makeInstParamId(instrIndex, kBrightnessSuffix));

    applyPerformanceMacros(instrIndex, s);
    applyRareRuntimeTailGuardrails(instrIndex, s);
    return s;
}

void InstrSynthAudioProcessor::applyPerformanceMacros(int instrIndex, mis::InstrumentSettings& s) const
{
    const auto warmth    = (getParamValue(kMacroWarmth)     - 0.5f) * 2.0f;
    const auto bright    = (getParamValue(kMacroBrightness) - 0.5f) * 2.0f;
    const auto express   = (getParamValue(kMacroExpression) - 0.5f) * 2.0f;
    const auto texture   = (getParamValue(kMacroTexture)    - 0.5f) * 2.0f;

    const auto family = mis::getFamily(instrIndex);

    // Warmth: body + low cutoff
    s.body     = clamp01(s.body + warmth * 0.2f);
    s.cutoffHz = juce::jlimit(120.0f, 18000.0f, s.cutoffHz * std::pow(2.0f, -warmth * 0.5f));

    // Brightness macro drives the dedicated spectral control first, then nudges cutoff/exciter.
    s.brightness = clamp01(s.brightness + bright * 0.35f);
    s.cutoffHz = juce::jlimit(120.0f, 18000.0f, s.cutoffHz * std::pow(2.0f, bright * 0.55f));
    s.exciter  = clamp01(s.exciter + bright * 0.08f);

    // Expression: attack/release dynamics
    s.attackSeconds  = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - express * 0.3f));
    s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + express * 0.25f));

    // Texture: noise + sympathetic + drive
    s.noiseAmount  = clamp01(s.noiseAmount + texture * 0.12f);
    s.sympathetic  = clamp01(s.sympathetic + texture * 0.15f);
    s.drive        = juce::jlimit(1.0f, 12.0f, s.drive * (1.0f + texture * 0.3f));

    // Family-specific macro interaction
    if (family == mis::Family::Strings)
        s.sympathetic = clamp01(s.sympathetic + warmth * 0.1f);
    else if (family == mis::Family::Winds)
        s.noiseAmount = clamp01(s.noiseAmount + express * 0.08f);
    else if (family == mis::Family::Percussion)
        s.exciter = clamp01(s.exciter + express * 0.12f);
}

void InstrSynthAudioProcessor::clearVoice(VoiceSlot& slot)
{
    if (slot.instrumentIndex >= 0 && slot.poolSlot >= 0)
        voicePoolInUse[static_cast<std::size_t>(slot.instrumentIndex)]
                      [static_cast<std::size_t>(slot.poolSlot)].store(false, std::memory_order_release);
    slot.voice = nullptr;
    slot.midiNote = -1;
    slot.midiChannel = 0;
    slot.instrumentIndex = -1;
    slot.poolSlot = -1;
    slot.activationAge = 0;
}

void InstrSynthAudioProcessor::clearDyingVoice(DyingVoiceSlot& slot)
{
    if (slot.instrumentIndex >= 0 && slot.poolSlot >= 0)
        voicePoolInUse[static_cast<std::size_t>(slot.instrumentIndex)]
                      [static_cast<std::size_t>(slot.poolSlot)].store(false, std::memory_order_release);
    slot.voice = nullptr;
    slot.midiChannel = 0;
    slot.instrumentIndex = -1;
    slot.poolSlot = -1;
    slot.outputBus = 0;
    slot.activationAge = 0;
}

void InstrSynthAudioProcessor::clearSustainedNotes() noexcept
{
    sustainedNoteCount = 0;
}

void InstrSynthAudioProcessor::removeSustainedNotesForChannel(const int midiChannel) noexcept
{
    const int requestedChannel = normalizedMidiChannel(midiChannel);
    int writeIndex = 0;
    for (int readIndex = 0; readIndex < sustainedNoteCount; ++readIndex)
    {
        const auto note = sustainedNotes[static_cast<std::size_t>(readIndex)];
        if (note.midiChannel != requestedChannel)
            sustainedNotes[static_cast<std::size_t>(writeIndex++)] = note;
    }
    sustainedNoteCount = writeIndex;
}

bool InstrSynthAudioProcessor::addSustainedNote(const int instrumentIndex,
                                                const int midiNote,
                                                const int midiChannel) noexcept
{
    const int normalizedChannel = normalizedMidiChannel(midiChannel);
    for (int i = 0; i < sustainedNoteCount; ++i)
    {
        const auto& note = sustainedNotes[static_cast<std::size_t>(i)];
        if (note.midiChannel == normalizedChannel && note.midiNote == midiNote)
            return true;
    }

    if (sustainedNoteCount >= kMaxSustainedNotes)
        return false;

    sustainedNotes[static_cast<std::size_t>(sustainedNoteCount++)] = { instrumentIndex, midiNote, normalizedChannel };
    return true;
}

void InstrSynthAudioProcessor::releaseSustainedNotesForChannel(const int midiChannel)
{
    const int requestedChannel = normalizedMidiChannel(midiChannel);
    std::array<SustainedNote, kMaxSustainedNotes> notesToRelease {};
    int releaseCount = 0;
    int writeIndex = 0;

    for (int readIndex = 0; readIndex < sustainedNoteCount; ++readIndex)
    {
        const auto note = sustainedNotes[static_cast<std::size_t>(readIndex)];
        if (note.midiChannel == requestedChannel)
        {
            if (releaseCount < kMaxSustainedNotes)
                notesToRelease[static_cast<std::size_t>(releaseCount++)] = note;
        }
        else
        {
            sustainedNotes[static_cast<std::size_t>(writeIndex++)] = note;
        }
    }

    sustainedNoteCount = writeIndex;
    for (int i = 0; i < releaseCount; ++i)
        triggerNoteOff(notesToRelease[static_cast<std::size_t>(i)].midiChannel,
                       notesToRelease[static_cast<std::size_t>(i)].midiNote);
}
void InstrSynthAudioProcessor::panicAllVoices()
{
    pitchBend.reset();
    modulationMatrix.resetMidiSources();
    sustainPedalHeld.fill(false);
    clearSustainedNotes();
    blockRuntimeFxSettings = snapshotGlobalFxSettings();
    latchedFxAvailability.fill(false);
    blockFxAvailability.fill(false);
    latchedFxOwnerInstrumentIndex = -1;
    for (auto& dv : dyingVoices)
        clearDyingVoice(dv);
    for (auto& slot : voices)
        clearVoice(slot);
}

void InstrSynthAudioProcessor::panicVoicesOnChannel(int midiChannel)
{
    const int requestedChannel = normalizedMidiChannel(midiChannel);
    sustainPedalHeld[static_cast<std::size_t>(requestedChannel)] = false;
    removeSustainedNotesForChannel(requestedChannel);

    for (auto& dv : dyingVoices)
    {
        if (dv.voice != nullptr && matchesMidiChannel(dv.midiChannel, requestedChannel))
            clearDyingVoice(dv);
    }

    for (auto& slot : voices)
    {
        if (slot.voice != nullptr && matchesMidiChannel(slot.midiChannel, requestedChannel))
            clearVoice(slot);
    }
}

void InstrSynthAudioProcessor::releaseVoices(int midiChannel, bool immediate)
{
    const int requestedChannel = normalizedMidiChannel(midiChannel);
    sustainPedalHeld[static_cast<std::size_t>(requestedChannel)] = false;
    removeSustainedNotesForChannel(requestedChannel);

    for (auto& slot : voices)
    {
        if (!slot.voice || !slot.voice->isActive())
            continue;
        if (!matchesMidiChannel(slot.midiChannel, requestedChannel))
            continue;

        if (immediate)
        {
            clearVoice(slot);
        }
        else
        {
            if (!slot.voice->isReleasing())
                slot.voice->noteOff();
        }
    }

    if (immediate)
    {
        for (auto& dv : dyingVoices)
        {
            if (dv.voice != nullptr && matchesMidiChannel(dv.midiChannel, requestedChannel))
                clearDyingVoice(dv);
        }
    }
}

int InstrSynthAudioProcessor::findFreeVoice() const
{
    // Prefer empty slots
    for (int i = 0; i < kMaxVoices; ++i)
        if (voices[static_cast<std::size_t>(i)].voice == nullptr)
            return i;

    // Then oldest releasing voice
    int oldest = -1;
    uint64_t oldestAge = UINT64_MAX;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        const auto& s = voices[static_cast<std::size_t>(i)];
        if (s.voice && s.voice->isReleasing() && s.activationAge < oldestAge)
        {
            oldest = i;
            oldestAge = s.activationAge;
        }
    }
    if (oldest >= 0) return oldest;

    // Last resort: oldest active voice
    oldestAge = UINT64_MAX;
    oldest = 0;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[static_cast<std::size_t>(i)].activationAge < oldestAge)
        {
            oldest = i;
            oldestAge = voices[static_cast<std::size_t>(i)].activationAge;
        }
    }
    return oldest;
}

void InstrSynthAudioProcessor::triggerNoteOn(int instrIndex, int midiChannel, int midiNote, float velocity)
{
    if (instrIndex < 0 || instrIndex >= mis::kNumInstruments) return;
    const double noteSampleRate = preparedSampleRate;
    if (noteSampleRate <= 0.0) return;
    const int normalizedChannel = normalizedMidiChannel(midiChannel);

    const auto profile = rareRuntimeProfileForInstrument(instrIndex);
    if (profile.maxMusicalVoices < kMaxVoicesPerInstr)
    {
        int soundingCount = 0;
        for (const auto& slot : voices)
        {
            if (slot.voice && slot.voice->isActive() && !slot.voice->isReleasing()
                && slot.instrumentIndex == instrIndex && slot.midiChannel == normalizedChannel)
                ++soundingCount;
        }

        while (soundingCount >= juce::jmax(1, profile.maxMusicalVoices))
        {
            VoiceSlot* oldest = nullptr;
            for (auto& slot : voices)
            {
                if (slot.voice && slot.voice->isActive() && !slot.voice->isReleasing()
                    && slot.instrumentIndex == instrIndex && slot.midiChannel == normalizedChannel
                    && (oldest == nullptr || slot.activationAge < oldest->activationAge))
                    oldest = &slot;
            }

            if (oldest == nullptr)
                break;

            oldest->voice->forceQuickRelease();
            --soundingCount;
        }
    }
    const int slotIdx = findFreeVoice();
    auto& v = voices[static_cast<std::size_t>(slotIdx)];

    // Move active voice to dying pool before stealing
    if (v.voice != nullptr && v.voice->isActive() && !v.voice->isReleasing() && v.poolSlot >= 0)
    {
        v.voice->forceQuickRelease();
        int targetBus = juce::jlimit(
            0, kNumAuxOutputs,
            static_cast<int>(std::round(getParamValue(
                makeInstParamId(v.instrumentIndex, kInstOutputSuffix)))));

        DyingVoiceSlot* dyingSlot = nullptr;
        for (auto& candidate : dyingVoices)
        {
            if (candidate.voice == nullptr)
            {
                dyingSlot = &candidate;
                break;
            }
        }
        if (dyingSlot == nullptr)
        {
            // All dying slots full — evict oldest
            dyingSlot = &dyingVoices[0];
            for (auto& candidate : dyingVoices)
                if (candidate.activationAge < dyingSlot->activationAge)
                    dyingSlot = &candidate;
            clearDyingVoice(*dyingSlot);
        }

        dyingSlot->voice = v.voice;
        dyingSlot->midiChannel = v.midiChannel;
        dyingSlot->instrumentIndex = v.instrumentIndex;
        dyingSlot->poolSlot = v.poolSlot;
        dyingSlot->outputBus = targetBus;
        dyingSlot->activationAge = ++voiceAgeCounter;
        resetVoiceSlotMetadata(v);
    }
    else
    {
        clearVoice(v);
    }

    // Acquire a free voice from the pool
    const auto inst = static_cast<std::size_t>(instrIndex);
    int pSlot = -1;
    for (int j = 0; j < kMaxVoicesPerInstr; ++j)
    {
        auto& inUse = voicePoolInUse[inst][static_cast<std::size_t>(j)];
        if (!poolSlotOwnedByAnyVoice(instrIndex, j))
            inUse.store(false, std::memory_order_release);

        bool expected = false;
        if (pSlot < 0 && inUse.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
            pSlot = j;
    }

    if (pSlot < 0)
    {
        // All pool voices in use — steal the oldest slot for this instrument
        uint64_t stealAge = UINT64_MAX;
        int stealIdx = -1;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            auto& s = voices[static_cast<std::size_t>(i)];
            if (s.instrumentIndex == instrIndex && s.poolSlot >= 0 && s.activationAge < stealAge)
            {
                stealIdx = i;
                stealAge = s.activationAge;
            }
        }
        if (stealIdx >= 0)
        {
            auto& stolen = voices[static_cast<std::size_t>(stealIdx)];
            pSlot = stolen.poolSlot;
            clearVoice(stolen);
        }
        else
        {
            // Also check dying voices for this instrument
            DyingVoiceSlot* oldestDying = nullptr;
            uint64_t dyingAge = UINT64_MAX;
            for (auto& dv : dyingVoices)
            {
                if (dv.instrumentIndex == instrIndex && dv.poolSlot >= 0
                    && dv.activationAge < dyingAge)
                {
                    oldestDying = &dv;
                    dyingAge = dv.activationAge;
                }
            }
            if (oldestDying != nullptr)
            {
                pSlot = oldestDying->poolSlot;
                clearDyingVoice(*oldestDying);
            }
        }
    }

    if (pSlot < 0 || pSlot >= kMaxVoicesPerInstr)
    {
        resetVoiceSlotMetadata(v);
        return;
    }

    v.voice = voiceBank[inst][static_cast<std::size_t>(pSlot)].get();
    if (v.voice == nullptr)
    {
        voicePoolInUse[inst][static_cast<std::size_t>(pSlot)].store(false, std::memory_order_release);
        resetVoiceSlotMetadata(v);
        return;
    }

    v.poolSlot = pSlot;
    v.instrumentIndex = instrIndex;
    v.midiNote = midiNote;
    v.midiChannel = normalizedChannel;
    v.activationAge = ++voiceAgeCounter;
    voicePoolInUse[inst][static_cast<std::size_t>(pSlot)].store(true, std::memory_order_release);

    auto settings = snapshotInstrumentSettings(instrIndex);
    v.voice->noteOn(settings, midiNote, velocity, noteSampleRate);
}

void InstrSynthAudioProcessor::triggerNoteOff(int midiChannel, int midiNote)
{
    const int normalizedChannel = normalizedMidiChannel(midiChannel);
    VoiceSlot* oldestMatch = nullptr;

    for (auto& slot : voices)
    {
        if (slot.voice && slot.voice->isActive() && !slot.voice->isReleasing()
            && slot.midiNote == midiNote && slot.midiChannel == normalizedChannel
            && (oldestMatch == nullptr || slot.activationAge < oldestMatch->activationAge))
        {
            oldestMatch = &slot;
        }
    }

    if (oldestMatch == nullptr)
        return;

    if (sustainPedalHeld[static_cast<std::size_t>(normalizedChannel)])
    {
        addSustainedNote(oldestMatch->instrumentIndex, midiNote, normalizedChannel);
        return;
    }

    oldestMatch->voice->noteOff();
}

// =============================================================================
// Global FX (same as MDS)
// =============================================================================
void InstrSynthAudioProcessor::updateGlobalEffectParameters()
{
    const auto threshold = blockRuntimeFxSettings.compThreshold;
    const auto ratio     = blockRuntimeFxSettings.compRatio;
    const auto attack    = blockRuntimeFxSettings.compAttack;
    const auto release   = blockRuntimeFxSettings.compRelease;

    if (threshold != compCache.threshold) { compressor.setThreshold(threshold); compCache.threshold = threshold; }
    if (ratio     != compCache.ratio)     { compressor.setRatio(ratio);          compCache.ratio     = ratio; }
    if (attack    != compCache.attack)    { compressor.setAttack(attack);         compCache.attack    = attack; }
    if (release   != compCache.release)   { compressor.setRelease(release);       compCache.release   = release; }
}

void InstrSynthAudioProcessor::processGlobalTransient(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Transient)
        || !blockRuntimeFxSettings.transientOn) return;
    const auto mix     = clamp01(blockRuntimeFxSettings.transientMix);
    const auto attack  = juce::jlimit(-1.0f, 1.0f, blockRuntimeFxSettings.transientAttack);
    const auto sustain = juce::jlimit(-1.0f, 1.0f, blockRuntimeFxSettings.transientSustain);

    if (mix <= 0.0001f || (std::abs(attack) <= 0.0001f && std::abs(sustain) <= 0.0001f))
        return;

    const auto sampleRate = static_cast<float>(std::max(1.0, preparedSampleRate));
    const auto fastCoeff = std::exp(-1.0f / (0.0018f * sampleRate));
    const auto slowCoeff = std::exp(-1.0f / (0.055f  * sampleRate));

    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
    {
        auto* data = mainBuffer.getWritePointer(ch);
        auto& fast = transientFastEnv[static_cast<std::size_t>(juce::jlimit(0, 1, ch))];
        auto& slow = transientSlowEnv[static_cast<std::size_t>(juce::jlimit(0, 1, ch))];

        for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
        {
            const auto dry = data[i];
            const auto absSample = std::abs(dry);
            fast = fastCoeff * fast + (1.0f - fastCoeff) * absSample;
            slow = slowCoeff * slow + (1.0f - slowCoeff) * absSample;

            const auto transient = fast - slow;
            const auto gain = juce::jlimit(0.2f, 4.0f,
                1.0f + attack * std::max(0.0f, transient) * 7.0f
                     + sustain * std::max(0.0f, -transient) * 5.0f);
            data[i] = dry + (dry * gain - dry) * mix;
        }
    }
}

void InstrSynthAudioProcessor::processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Saturator)
        || !blockRuntimeFxSettings.saturatorOn) return;
    const auto mix = clamp01(blockRuntimeFxSettings.satMix);
    if (mix <= 0.0001f) return;

    const auto drive = juce::jlimit(1.0f, 16.0f, blockRuntimeFxSettings.satDrive);
    const auto norm  = 1.0f / std::max(0.0001f, std::tanh(drive));
    const auto numCh = mainBuffer.getNumChannels();
    const auto numSamples = mainBuffer.getNumSamples();
    if (numCh <= 0 || numSamples <= 0)
        return;

    const bool needsDryBlend = mix < 0.9999f;
    if (needsDryBlend)
    {
        if (fxDryBuffer.getNumChannels() < numCh || fxDryBuffer.getNumSamples() < numSamples)
        {
            jassertfalse;
            return;
        }

        for (int ch = 0; ch < numCh; ++ch)
            fxDryBuffer.copyFrom(ch, 0, mainBuffer, ch, 0, numSamples);
    }

    juce::dsp::AudioBlock<float> fullBlock(mainBuffer);
    auto block = fullBlock.getSubsetChannelBlock(0, static_cast<std::size_t>(juce::jmin(2, numCh)));
    auto& oversampler = numCh == 1 ? satOversamplingMono : satOversamplingStereo;
    auto oversampledBlock = oversampler.processSamplesUp(block);
    for (std::size_t ch = 0; ch < oversampledBlock.getNumChannels(); ++ch)
    {
        auto* data = oversampledBlock.getChannelPointer(ch);
        for (std::size_t i = 0; i < oversampledBlock.getNumSamples(); ++i)
            data[i] = std::tanh(data[i] * drive) * norm;
    }
    oversampler.processSamplesDown(block);

    if (needsDryBlend)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wet = mainBuffer.getWritePointer(ch);
            const auto* dry = fxDryBuffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
                wet[i] = dry[i] + (wet[i] - dry[i]) * mix;
        }
    }
}

void InstrSynthAudioProcessor::processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Compressor)
        || !blockRuntimeFxSettings.compressorOn) return;
    const auto mix = clamp01(blockRuntimeFxSettings.compMix);
    const auto makeupGain = juce::Decibels::decibelsToGain(blockRuntimeFxSettings.compMakeup);

    if (mix <= 0.0001f && std::abs(makeupGain - 1.0f) <= 0.0001f)
        return;

    updateGlobalEffectParameters();
    if (fxDryBuffer.getNumChannels() < mainBuffer.getNumChannels()
        || fxDryBuffer.getNumSamples() < mainBuffer.getNumSamples())
    {
        jassertfalse;
        return;
    }

    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        fxDryBuffer.copyFrom(ch, 0, mainBuffer, ch, 0, mainBuffer.getNumSamples());

    juce::dsp::AudioBlock<float> block(mainBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);
    mainBuffer.applyGain(makeupGain);

    if (mix < 0.9999f)
    {
        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        {
            auto* wet = mainBuffer.getWritePointer(ch);
            const auto* dry = fxDryBuffer.getReadPointer(ch);
            for (int i = 0; i < mainBuffer.getNumSamples(); ++i)
                wet[i] = dry[i] + (wet[i] - dry[i]) * mix;
        }
    }
}

void InstrSynthAudioProcessor::applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer)
{
    const auto numCh = mainBuffer.getNumChannels();
    const auto numSamples = mainBuffer.getNumSamples();
    if (numCh <= 0 || numSamples <= 0) return;

    const float rateHz = juce::jlimit(0.05f, 12.0f, getParamValue(kLfoRate));
    const float depth  = clamp01(getParamValue(kLfoDepth));
    if (depth <= 0.0001f) return;

    const int wave = juce::jlimit(0, 3, static_cast<int>(std::round(getParamValue(kLfoWave))));
    const float phaseInc = rateHz / static_cast<float>(juce::jmax(1.0, preparedSampleRate));
    constexpr float kTremDepth = 0.50f;
    constexpr float kPanDepth  = 0.35f;

    auto* left  = mainBuffer.getWritePointer(0);
    auto* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float lfo = 0.0f;
        switch (wave)
        {
            case 1: lfo = 1.0f - 4.0f * std::abs(lfoPhase - 0.5f); break;
            case 2: lfo = lfoPhase * 2.0f - 1.0f; break;
            case 3: lfo = lfoPhase < 0.5f ? 1.0f : -1.0f; break;
            default: lfo = mis::fastSin(lfoPhase); break;
        }

        const float tremAmt = depth * kTremDepth;
        const float trem = 1.0f - tremAmt * 0.5f + lfo * tremAmt * 0.5f;

        if (right != nullptr)
        {
            const float pan = lfo * depth * kPanDepth;
            const float gL = std::sqrt(0.5f * (1.0f - pan)) * trem;
            const float gR = std::sqrt(0.5f * (1.0f + pan)) * trem;
            left[i]  *= gL;
            right[i] *= gR;
        }
        else
        {
            left[i] *= trem;
        }

        lfoPhase += phaseInc;
        if (lfoPhase >= 1.0f) lfoPhase -= 1.0f;
    }
}

// =============================================================================
// Global EQ
// =============================================================================
void InstrSynthAudioProcessor::processGlobalEQ(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Eq)
        || !blockRuntimeFxSettings.eqOn) return;

    mis::fx::ParametricEQ3Band::Params p;
    p.lowFreq    = blockRuntimeFxSettings.eqLowFreq;
    p.lowGainDb  = blockRuntimeFxSettings.eqLowGain;
    p.midFreq    = blockRuntimeFxSettings.eqMidFreq;
    p.midGainDb  = blockRuntimeFxSettings.eqMidGain;
    p.midQ       = blockRuntimeFxSettings.eqMidQ;
    p.highFreq   = blockRuntimeFxSettings.eqHighFreq;
    p.highGainDb = blockRuntimeFxSettings.eqHighGain;

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    eqProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
// Global Chorus
// =============================================================================
void InstrSynthAudioProcessor::processGlobalChorus(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Chorus)
        || !blockRuntimeFxSettings.chorusOn) return;

    mis::fx::StereoChorus::Params p;
    p.rateHz = blockRuntimeFxSettings.chorusRate;
    p.depth  = blockRuntimeFxSettings.chorusDepth;
    p.mix    = blockRuntimeFxSettings.chorusMix;

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    chorusProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
// Global Delay
// =============================================================================
void InstrSynthAudioProcessor::processGlobalDelay(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Delay)
        || !blockRuntimeFxSettings.delayOn) return;

    mis::fx::StereoDelay::Params p;
    p.timeMs   = blockRuntimeFxSettings.delayTime;
    p.feedback = blockRuntimeFxSettings.delayFeedback;
    p.mix      = blockRuntimeFxSettings.delayMix;

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    delayProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
// Global Reverb (Dattorro Plate)
// =============================================================================
void InstrSynthAudioProcessor::processGlobalReverb(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Reverb)
        || !blockRuntimeFxSettings.reverbOn) return;

    mis::fx::DattorroPlateReverb::Params p;
    p.decay      = blockRuntimeFxSettings.reverbSize;
    p.damping    = blockRuntimeFxSettings.reverbDamping;
    p.width      = blockRuntimeFxSettings.reverbWidth;
    p.mix        = blockRuntimeFxSettings.reverbMix;
    p.preDelayMs = blockRuntimeFxSettings.reverbPredelay;

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    reverbProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
// Global Limiter
// =============================================================================
void InstrSynthAudioProcessor::processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstrument(mis::GlobalFxSlot::Limiter)
        || !blockRuntimeFxSettings.limiterOn) return;

    mis::fx::OutputLimiter::Params p;
    p.thresholdDb = blockRuntimeFxSettings.limiterThreshold;
    p.releaseMs   = blockRuntimeFxSettings.limiterRelease;

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    limiterProcessor.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new InstrSynthAudioProcessor();
}
