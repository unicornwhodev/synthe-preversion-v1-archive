#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Engine/SinTable.h"
#include "../../Shared/PresetManifest.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <limits>

namespace
{
juce::String utf8Text(const char* text)
{
    return juce::String(juce::CharPointer_UTF8(text));
}

constexpr const char* kOutputGain          = "output_gain";
constexpr const char* kSelectedInstr = "selected_instr";
constexpr const char* kQualityMode         = "quality_mode";
constexpr const char* kStopNotesOnKeyReleaseState = "stop_notes_on_key_release";
constexpr const char* kDelaySync           = "delay_sync";
constexpr const char* kDelayDivision       = "delay_division";
constexpr const char* kLfoRate       = "lfo_rate";
constexpr const char* kLfoDepth            = "lfo_depth";
constexpr const char* kLfoWave             = "lfo_wave";
constexpr const char* kVelocityCurve = "velocity_curve";
constexpr const char* kPortamentoSeconds   = "portamento_seconds";
constexpr const char* kLegatoAmount        = "legato_amount";
constexpr const char* kRoundRobinAmount    = "round_robin_amount";
constexpr const char* kModMatrixSourceSuffix = "source";
constexpr const char* kModMatrixDestSuffix   = "dest";
constexpr const char* kModMatrixAmountSuffix = "amount";
// FIX: expanded mod matrix from 8 to 16 slots (P16)
static const char* kModMatrixSourceParamIds[] = {
    "mod_0_source", "mod_1_source", "mod_2_source", "mod_3_source",
    "mod_4_source", "mod_5_source", "mod_6_source", "mod_7_source",
    "mod_8_source", "mod_9_source", "mod_10_source", "mod_11_source",
    "mod_12_source", "mod_13_source", "mod_14_source", "mod_15_source"
};
static const char* kModMatrixDestParamIds[] = {
    "mod_0_dest", "mod_1_dest", "mod_2_dest", "mod_3_dest",
    "mod_4_dest", "mod_5_dest", "mod_6_dest", "mod_7_dest",
    "mod_8_dest", "mod_9_dest", "mod_10_dest", "mod_11_dest",
    "mod_12_dest", "mod_13_dest", "mod_14_dest", "mod_15_dest"
};
static const char* kModMatrixAmountParamIds[] = {
    "mod_0_amount", "mod_1_amount", "mod_2_amount", "mod_3_amount",
    "mod_4_amount", "mod_5_amount", "mod_6_amount", "mod_7_amount",
    "mod_8_amount", "mod_9_amount", "mod_10_amount", "mod_11_amount",
    "mod_12_amount", "mod_13_amount", "mod_14_amount", "mod_15_amount"
};

constexpr const char* kMacroWarmth     = "macro_warmth";
constexpr const char* kMacroBrillance  = "macro_brillance";
constexpr const char* kMacroSpace      = "macro_space";
constexpr const char* kMacroExpression = "macro_expression";

constexpr const char* kCompThreshold = "comp_threshold";
constexpr const char* kCompRatio     = "comp_ratio";
constexpr const char* kCompAttack    = "comp_attack";
constexpr const char* kCompRelease   = "comp_release";
constexpr const char* kCompMix       = "comp_mix";

constexpr const char* kSatDrive = "sat_drive";
constexpr const char* kSatMix   = "sat_mix";

constexpr const char* kTransientAttack  = "transient_attack";
constexpr const char* kTransientSustain = "transient_sustain";
constexpr const char* kTransientMix     = "transient_mix";

constexpr const char* kReverbSize     = "reverb_size";
constexpr const char* kReverbDamping  = "reverb_damping";
constexpr const char* kReverbWidth    = "reverb_width";
constexpr const char* kReverbMix      = "reverb_mix";
constexpr const char* kReverbPredelay = "reverb_predelay";
constexpr const char* kReverbType     = "reverb_type";

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

constexpr int kPresetSchemaVersion   = 5;
constexpr int kOrchSynthIndex = 4;

constexpr float kDefaultReverbSize    = 0.65f;
constexpr float kDefaultReverbDamping = 0.40f;
constexpr float kDefaultReverbWidth   = 0.90f;
constexpr float kDefaultReverbMix     = 0.28f;

constexpr const char* kInstrOutputSuffix = "output";

struct PresetFieldDef
{
    const char* xmlName;
    const char* paramSuffix;
};

constexpr std::array<PresetFieldDef, 14> kPresetFieldDefs = {{
    { "level",        "level" },
    { "tune",         "tune" },
    { "brightness",   "brightness" },
    { "attack",       "attack" },
    { "decay",        "decay" },
    { "sustain",      "sustain" },
    { "release",      "release" },
    { "vibrato",      "vibrato" },
    { "warmth",       "warmth" },
    { "detune",       "detune" },
    { "stereo_width", "stereo_width" },
    { "character",    "character" },
    { "cutoff",       "cutoff" },
    { "pan",          "pan" },
}};

juce::StringArray makeOutputChoices()
{
    juce::StringArray outputs;
    outputs.add("Master");
    for (int i = 0; i < OrchSynthAudioProcessor::kNumAuxOutputs; ++i)
        outputs.add("Out " + juce::String(i + 1));
    return outputs;
}

juce::StringArray makeModMatrixSourceChoices()
{
    return juce::StringArray{ "Off", "Mod Wheel", "CC11", "Breath", "Aftertouch", "Velocity", "LFO", "Pitch Bend", "Envelope" };
}

juce::StringArray makeModMatrixDestinationChoices()
{
    return juce::StringArray{ "Off", "Gain", "Timbre", "Vibrato", "Release", "Aftertouch", "Cutoff", "Pan", "Pitch", "Attack", "Decay", "EQ Mid Freq", "EQ Mid Gain" };
}

juce::StringArray makeQualityChoices()
{
    return juce::StringArray{ "Live", "Studio" };
}

juce::StringArray makeDelaySyncChoices()
{
    return juce::StringArray{ "Off", "Host" };
}

float safeDefaultForMissingParam(const juce::String& paramId) noexcept
{
    if (paramId == kOutputGain) return -3.0f;
    if (paramId == kSelectedInstr || paramId == kQualityMode || paramId == kDelaySync || paramId == kLfoDepth
        || paramId == kLfoWave || paramId == kVelocityCurve || paramId == kPortamentoSeconds || paramId == "fx_lock")
        return 0.0f;
    if (paramId == kDelayDivision) return 1.0f;
    if (paramId == kLfoRate) return 1.1f;
    if (paramId == kLegatoAmount || (paramId.startsWith("fx_") && paramId.endsWith("_en"))) return 1.0f;
    if (paramId == kRoundRobinAmount || paramId == kMacroWarmth || paramId == kMacroBrillance
        || paramId == kMacroSpace || paramId == kMacroExpression) return 0.5f;

    if (paramId == kSatDrive) return 1.5f;
    if (paramId == kSatMix || paramId == kTransientAttack || paramId == kTransientSustain || paramId == kTransientMix
        || paramId == kChorusMix || paramId == kDelayMix) return 0.0f;
    if (paramId == kCompThreshold) return -19.0f;
    if (paramId == kCompRatio) return 3.0f;
    if (paramId == kCompAttack) return 10.0f;
    if (paramId == kCompRelease) return 120.0f;
    if (paramId == kCompMix) return 1.0f;
    if (paramId == kEqLowFreq) return 180.0f;
    if (paramId == kEqMidFreq) return 1200.0f;
    if (paramId == kEqHighFreq) return 6500.0f;
    if (paramId == kEqLowGain || paramId == kEqMidGain || paramId == kEqHighGain) return 0.0f;
    if (paramId == kEqMidQ) return 0.8f;
    if (paramId == kChorusRate) return 0.8f;
    if (paramId == kChorusDepth) return 0.25f;
    if (paramId == kDelayTime) return 360.0f;
    if (paramId == kDelayFeedback) return 0.24f;
    if (paramId == kReverbSize) return kDefaultReverbSize;
    if (paramId == kReverbDamping) return kDefaultReverbDamping;
    if (paramId == kReverbWidth) return kDefaultReverbWidth;
    if (paramId == kReverbMix) return kDefaultReverbMix;
    if (paramId == kReverbPredelay) return 20.0f;
    if (paramId == kReverbType) return 0.0f;
    if (paramId == kLimiterThreshold) return -1.0f;
    if (paramId == kLimiterRelease) return 200.0f;

    for (int slotIndex = 0; slotIndex < OrchSynthAudioProcessor::kNumModMatrixSlots; ++slotIndex)
    {
        if (paramId == OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix)
            || paramId == OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixDestSuffix)
            || paramId == OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix))
            return 0.0f;
    }

    if (paramId.endsWith("_level")) return 0.75f;
    if (paramId.endsWith("_tune") || paramId.endsWith("_vibrato") || paramId.endsWith("_detune")
        || paramId.endsWith("_character") || paramId.endsWith("_pan") || paramId.endsWith("_output"))
        return 0.0f;
    if (paramId.endsWith("_brightness")) return 0.5f;
    if (paramId.endsWith("_attack")) return 0.05f;
    if (paramId.endsWith("_decay")) return 1.0f;
    if (paramId.endsWith("_sustain")) return 0.7f;
    if (paramId.endsWith("_release")) return 0.4f;
    if (paramId.endsWith("_warmth")) return 0.25f;
    if (paramId.endsWith("_stereo_width")) return 0.2f;
    if (paramId.endsWith("_cutoff")) return 5000.0f;

    return 1.0f;
}

juce::StringArray makeDelayDivisionChoices()
{
    return juce::StringArray{ "1/4", "1/8", "1/8D", "1/8T", "1/16", "1/16D" };
}

juce::StringArray makeReverbTypeChoices()
{
    return juce::StringArray{ "Plate", "Hall" };
}

void setRangedParameterValue(juce::RangedAudioParameter* parameter, float actualValue, bool notifyHost)
{
    if (parameter == nullptr)
        return;

    const auto normalised = parameter->convertTo0to1(actualValue);
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

float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

float sampleLfoWaveform(const float phase, const int wave) noexcept
{
    switch (wave)
    {
        case 1: return 1.0f - 4.0f * std::abs(phase - 0.5f);
        case 2: return phase * 2.0f - 1.0f;
        case 3: return phase < 0.5f ? 1.0f : -1.0f;
        default: return mos::fastSin(phase);
    }
}

juce::File findWritableDirectory(const juce::File& preferred, const juce::String& fallbackRelative)
{
    auto tryDirectory = [] (const juce::File& base) -> juce::File
    {
        auto dir = base;
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

    const auto tempFallback = juce::File::getSpecialLocation(juce::File::tempDirectory)
        .getChildFile(fallbackRelative);
    if (auto dir = tryDirectory(tempFallback); dir != juce::File{})
        return dir;

    auto cwdFallback = juce::File::getCurrentWorkingDirectory()
        .getChildFile(".musique_user_data")
        .getChildFile(fallbackRelative);
    cwdFallback.createDirectory();
    return cwdFallback;
}

juce::String instrumentSlugForIndex(const int instrIndex)
{
    auto slug = utf8Text(mos::getInstrName(instrIndex)).toLowerCase();
    slug = slug.replaceCharacters(" .,/\\-()[]{}", "____________");
    juce::String normalized;
    bool lastWasUnderscore = false;
    for (const auto ch : slug)
    {
        const bool keep = juce::CharacterFunctions::isLetterOrDigit(ch) || ch == '_';
        if (!keep)
            continue;
        if (ch == '_')
        {
            if (!lastWasUnderscore && normalized.isNotEmpty())
                normalized << ch;
            lastWasUnderscore = true;
            continue;
        }
        normalized << ch;
        lastWasUnderscore = false;
    }
    return normalized.trimCharactersAtEnd("_");
}

juce::String joinTags(const std::vector<std::string>& tags)
{
    juce::StringArray values;
    for (const auto& tag : tags)
    {
        if (!tag.empty())
            values.add(utf8Text(tag.c_str()));
    }
    return values.joinIntoString(",");
}

std::vector<std::string> splitTags(const juce::String& tags)
{
    std::vector<std::string> values;
    for (auto token : juce::StringArray::fromTokens(tags, ",;", {}))
    {
        token = token.trim();
        if (token.isNotEmpty())
            values.emplace_back(token.toStdString());
    }
    return values;
}

OrchSynthAudioProcessor::PersistedPresetMetadata makeUserMetadata(const int instrIndex)
{
    OrchSynthAudioProcessor::PersistedPresetMetadata metadata;
    metadata.mixRole = "custom";
    metadata.family = utf8Text(mos::getFamilyName(static_cast<int>(mos::getFamily(instrIndex))));
    metadata.tags = "orch,user,custom," + metadata.family + "," + instrumentSlugForIndex(instrIndex);
    metadata.description = "Custom orchestral preset";
    metadata.outputProfile = "user-custom";
    metadata.nominalPeakDb = -12.0f;
    return metadata;
}

OrchSynthAudioProcessor::PersistedPresetMetadata makeFactoryMetadata(const mos::PresetMetadata& metadata)
{
    OrchSynthAudioProcessor::PersistedPresetMetadata persisted;
    persisted.mixRole = utf8Text(metadata.mixRole.c_str());
    persisted.family = utf8Text(metadata.familyLabel.c_str());
    persisted.tags = joinTags(metadata.tags);
    persisted.description = utf8Text(metadata.description.c_str());
    persisted.outputProfile = utf8Text(metadata.outputProfile.c_str());
    persisted.nominalPeakDb = metadata.nominalPeakDb;
    return persisted;
}

OrchSynthAudioProcessor::PresetPersistenceState makeDefaultPresetState(const int instrIndex)
{
    OrchSynthAudioProcessor::PresetPersistenceState state;
    state.instrIndex = juce::jlimit(0, mos::kNumInstruments - 1, instrIndex);
    state.settings = mos::getDefaultSettings(state.instrIndex);
    state.fx = mos::GlobalFxSettings{};
    state.outputBus = 0;
    state.qualityMode = 0;
    state.delaySync = 0;
    state.delayDivision = 1;
    state.lfoRate = 1.1f;
    state.lfoDepth = 0.0f;
    state.lfoWave = 0;
    state.velocityCurve = 0;
    state.portamentoSeconds = 0.0f;
    state.legatoAmount = 1.0f;
    state.roundRobinAmount = 0.5f;
    state.macroWarmth = 0.5f;
    state.macroBrillance = 0.5f;
    state.macroSpace = 0.5f;
    state.macroExpression = 0.5f;
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

int readInstrumentIndexAttribute(const juce::XmlElement& xml, const int fallback)
{
    if (xml.hasAttribute("instr"))
        return xml.getIntAttribute("instr", fallback);
    if (xml.hasAttribute("instrument_index"))
        return xml.getIntAttribute("instrument_index", fallback);
    if (xml.hasAttribute("instrumentIndex"))
        return xml.getIntAttribute("instrumentIndex", fallback);
    return fallback;
}

int readPresetIndexAttribute(const juce::XmlElement& xml, const int fallback)
{
    if (xml.hasAttribute("preset_index"))
        return xml.getIntAttribute("preset_index", fallback);
    if (xml.hasAttribute("preset"))
        return xml.getIntAttribute("preset", fallback);
    return fallback;
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

bool isFiniteXmlNumber(const double value)
{
    return std::isfinite(value) && value >= -std::numeric_limits<float>::max()
        && value <= std::numeric_limits<float>::max();
}

bool tryParseXmlDoubleAttribute(const juce::XmlElement& xml, const char* attrName, double& value)
{
    if (!xml.hasAttribute(attrName))
        return false;

    const auto raw = xml.getStringAttribute(attrName).trim();
    if (raw.isEmpty())
        return false;

    char* end = nullptr;
    const auto parsed = std::strtod(raw.toRawUTF8(), &end);
    if (end == raw.toRawUTF8() || end == nullptr || *end != '\0' || !isFiniteXmlNumber(parsed))
        return false;

    value = parsed;
    return true;
}

float readValidatedXmlFloat(const juce::XmlElement& xml,
                            const char* attrName,
                            float fallback,
                            float minValue,
                            float maxValue,
                            int& warningCount)
{
    double parsed = 0.0;
    if (!tryParseXmlDoubleAttribute(xml, attrName, parsed))
    {
        if (xml.hasAttribute(attrName))
            ++warningCount;
        return fallback;
    }

    const auto clamped = juce::jlimit(minValue, maxValue, static_cast<float>(parsed));
    if (clamped != static_cast<float>(parsed))
        ++warningCount;
    return clamped;
}

float readFiniteXmlFloat(const juce::XmlElement& xml,
                         const char* attrName,
                         float fallback,
                         float minValue,
                         float maxValue,
                         int* warningCount = nullptr)
{
    int localWarnings = 0;
    const auto value = readValidatedXmlFloat(xml, attrName, fallback, minValue, maxValue, localWarnings);
    if (warningCount != nullptr)
        *warningCount += localWarnings;
    return value;
}

float readFiniteStateFloat(const juce::ValueTree& state,
                           const juce::String& paramId,
                           float fallback,
                           float minValue,
                           float maxValue)
{
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        const auto child = state.getChild(childIndex);
        if (child.getProperty("id").toString() != paramId)
            continue;

        const auto rawValue = child.getProperty("value");
        if (rawValue.isDouble() || rawValue.isInt() || rawValue.isInt64() || rawValue.isBool())
        {
            const auto numeric = static_cast<float>(static_cast<double>(rawValue));
            if (std::isfinite(numeric))
                return juce::jlimit(minValue, maxValue, numeric);
        }

        const auto stringValue = rawValue.toString().trim();
        if (stringValue.isNotEmpty())
        {
            char* end = nullptr;
            const auto parsed = std::strtod(stringValue.toRawUTF8(), &end);
            if (end != stringValue.toRawUTF8() && end != nullptr && *end == '\0' && std::isfinite(parsed))
                return juce::jlimit(minValue, maxValue, static_cast<float>(parsed));
        }

        return fallback;
    }

    return fallback;
}

void setStateFloatProperty(juce::ValueTree& state, const juce::String& paramId, float value)
{
    for (int childIndex = 0; childIndex < state.getNumChildren(); ++childIndex)
    {
        auto child = state.getChild(childIndex);
        if (child.getProperty("id").toString() == paramId)
        {
            child.setProperty("value", value, nullptr);
            return;
        }
    }
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

bool tryReadValidatedChildInstrumentIndex(const juce::ValueTree& child, int& instrumentIndex)
{
    float parsedValue = 0.0f;
    if (!tryParseStateFloatValue(child.getProperty("inst"), parsedValue))
        return false;

    const auto roundedValue = static_cast<int>(std::round(parsedValue));
    if (roundedValue < 0 || roundedValue >= mos::kNumInstruments)
        return false;

    instrumentIndex = roundedValue;
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

bool readStateBoolProperty(const juce::ValueTree& state, const juce::Identifier& property, bool fallback)
{
    if (!state.hasProperty(property))
        return fallback;

    const auto value = state.getProperty(property);
    if (value.isBool())
        return static_cast<bool>(value);
    if (value.isInt() || value.isInt64())
        return static_cast<int>(value) != 0;
    if (value.isDouble())
        return std::abs(static_cast<double>(value)) > 0.5;

    const auto text = value.toString().trim().toLowerCase();
    if (text == "1" || text == "true" || text == "yes" || text == "on")
        return true;
    if (text == "0" || text == "false" || text == "no" || text == "off")
        return false;

    return fallback;
}

void writePresetSettingsAttributes(juce::XmlElement& root,
                                   const OrchSynthAudioProcessor::PresetPersistenceState& state)
{
    root.setAttribute("level", static_cast<double>(state.settings.level));
    root.setAttribute("tune", static_cast<double>(state.settings.tuneSemitones));
    root.setAttribute("brightness", static_cast<double>(state.settings.brightness));
    root.setAttribute("attack", static_cast<double>(state.settings.attackSeconds));
    root.setAttribute("decay", static_cast<double>(state.settings.decaySeconds));
    root.setAttribute("sustain", static_cast<double>(state.settings.sustainLevel));
    root.setAttribute("release", static_cast<double>(state.settings.releaseSeconds));
    root.setAttribute("vibrato", static_cast<double>(state.settings.vibrato));
    root.setAttribute("warmth", static_cast<double>(state.settings.warmth));
    root.setAttribute("detune", static_cast<double>(state.settings.detune));
    root.setAttribute("stereo_width", static_cast<double>(state.settings.stereoWidth));
    root.setAttribute("character", static_cast<double>(state.settings.character));
    root.setAttribute("cutoff", static_cast<double>(state.settings.cutoffHz));
    root.setAttribute("pan", static_cast<double>(state.settings.pan));
}

void writeFxAttributes(juce::XmlElement& root,
                       const OrchSynthAudioProcessor::PresetPersistenceState& state)
{
    root.setAttribute("sat_drive", static_cast<double>(state.fx.satDrive));
    root.setAttribute("sat_mix", static_cast<double>(state.fx.satMix));
    root.setAttribute("transient_attack", static_cast<double>(state.fx.transientAttack));
    root.setAttribute("transient_sustain", static_cast<double>(state.fx.transientSustain));
    root.setAttribute("transient_mix", static_cast<double>(state.fx.transientMix));
    root.setAttribute("eq_low_freq", static_cast<double>(state.fx.eqLowFreq));
    root.setAttribute("eq_low_gain", static_cast<double>(state.fx.eqLowGain));
    root.setAttribute("eq_mid_freq", static_cast<double>(state.fx.eqMidFreq));
    root.setAttribute("eq_mid_gain", static_cast<double>(state.fx.eqMidGain));
    root.setAttribute("eq_mid_q", static_cast<double>(state.fx.eqMidQ));
    root.setAttribute("eq_high_freq", static_cast<double>(state.fx.eqHighFreq));
    root.setAttribute("eq_high_gain", static_cast<double>(state.fx.eqHighGain));
    root.setAttribute("comp_threshold", static_cast<double>(state.fx.compThreshold));
    root.setAttribute("comp_ratio", static_cast<double>(state.fx.compRatio));
    root.setAttribute("comp_attack", static_cast<double>(state.fx.compAttack));
    root.setAttribute("comp_release", static_cast<double>(state.fx.compRelease));
    root.setAttribute("comp_mix", static_cast<double>(state.fx.compMix));
    root.setAttribute("chorus_rate", static_cast<double>(state.fx.chorusRate));
    root.setAttribute("chorus_depth", static_cast<double>(state.fx.chorusDepth));
    root.setAttribute("chorus_mix", static_cast<double>(state.fx.chorusMix));
    root.setAttribute("delay_time", static_cast<double>(state.fx.delayTime));
    root.setAttribute("delay_feedback", static_cast<double>(state.fx.delayFeedback));
    root.setAttribute("delay_mix", static_cast<double>(state.fx.delayMix));
    root.setAttribute("reverb_size", static_cast<double>(state.fx.reverbSize));
    root.setAttribute("reverb_damping", static_cast<double>(state.fx.reverbDamping));
    root.setAttribute("reverb_width", static_cast<double>(state.fx.reverbWidth));
    root.setAttribute("reverb_mix", static_cast<double>(state.fx.reverbMix));
    root.setAttribute("reverb_predelay", static_cast<double>(state.fx.reverbPredelay));
    root.setAttribute("reverb_type", juce::jlimit(0, 1, state.fx.reverbType));
    root.setAttribute("limiter_threshold", static_cast<double>(state.fx.limiterThreshold));
    root.setAttribute("limiter_release", static_cast<double>(state.fx.limiterRelease));
    root.setAttribute("fx_tab0_en", state.fxEnables[0]);
    root.setAttribute("fx_tab1_en", state.fxEnables[1]);
    root.setAttribute("fx_tab2_en", state.fxEnables[2]);
    root.setAttribute("fx_tab3_en", state.fxEnables[3]);
    root.setAttribute("fx_eq_en", state.fxEnables[4]);
    root.setAttribute("fx_chorus_en", state.fxEnables[5]);
    root.setAttribute("fx_delay_en", state.fxEnables[6]);
    root.setAttribute("fx_limiter_en", state.fxEnables[7]);
    root.setAttribute("fx_lock", state.fxLock);
}

void writeGlobalPresetAttributes(juce::XmlElement& root,
                                 const OrchSynthAudioProcessor::PresetPersistenceState& state)
{
    root.setAttribute("quality_mode", juce::jlimit(0, 1, state.qualityMode));
    root.setAttribute("delay_sync", juce::jlimit(0, 1, state.delaySync));
    root.setAttribute("delay_division", juce::jlimit(0, 5, state.delayDivision));
    root.setAttribute("velocity_curve", juce::jlimit(0, 6, state.velocityCurve));
    root.setAttribute("lfo_rate", static_cast<double>(juce::jlimit(0.05f, 12.0f, state.lfoRate)));
    root.setAttribute("lfo_depth", static_cast<double>(juce::jlimit(0.0f, 1.0f, state.lfoDepth)));
    root.setAttribute("lfo_wave", juce::jlimit(0, 3, state.lfoWave));
    root.setAttribute("portamento_seconds", static_cast<double>(juce::jlimit(0.0f, 2.0f, state.portamentoSeconds)));
    root.setAttribute("legato_amount", static_cast<double>(clamp01(state.legatoAmount)));
    root.setAttribute("round_robin_amount", static_cast<double>(clamp01(state.roundRobinAmount)));
    root.setAttribute("macro_warmth", static_cast<double>(clamp01(state.macroWarmth)));
    root.setAttribute("macro_brillance", static_cast<double>(clamp01(state.macroBrillance)));
    root.setAttribute("macro_space", static_cast<double>(clamp01(state.macroSpace)));
    root.setAttribute("macro_expression", static_cast<double>(clamp01(state.macroExpression)));

    for (int slotIndex = 0; slotIndex < OrchSynthAudioProcessor::kNumModMatrixSlots; ++slotIndex)
    {
        root.setAttribute(OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix),
                          juce::jlimit(0, 8, state.modSources[static_cast<std::size_t>(slotIndex)]));
        root.setAttribute(OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixDestSuffix),
                          juce::jlimit(0, 12, state.modDestinations[static_cast<std::size_t>(slotIndex)]));
        root.setAttribute(OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix),
                          static_cast<double>(juce::jlimit(-1.0f, 1.0f, state.modAmounts[static_cast<std::size_t>(slotIndex)])));
    }
}

void writeMetadataAttributes(juce::XmlElement& root,
                             const OrchSynthAudioProcessor::PersistedPresetMetadata& metadata)
{
    root.setAttribute("mix_role", metadata.mixRole);
    root.setAttribute("preset_family", metadata.family);
    root.setAttribute("tags", metadata.tags);
    root.setAttribute("description", metadata.description);
    root.setAttribute("output_profile", metadata.outputProfile);
    root.setAttribute("nominal_peak_db", static_cast<double>(metadata.nominalPeakDb));
}

bool parsePresetXml(const juce::XmlElement& xml,
                    const char* expectedTag,
                    const int expectedInstrIndex,
                    OrchSynthAudioProcessor::PresetPersistenceState& out,
                    const bool requirePresetIndex,
                    const OrchSynthAudioProcessor::PresetPersistenceState* fallback = nullptr)
{
    if (!xml.hasTagName(expectedTag))
        return false;

    out = fallback != nullptr ? *fallback : makeDefaultPresetState(expectedInstrIndex);
    out.name = readStringAttribute(xml, { "name" }, out.name);
    out.instrIndex = readInstrumentIndexAttribute(xml, expectedInstrIndex);
    if (out.instrIndex != expectedInstrIndex)
        return false;

    const auto expectedFamily = utf8Text(mos::getFamilyName(static_cast<int>(mos::getFamily(expectedInstrIndex))));
    const auto storedFamily = readStringAttribute(xml, { "family" }, expectedFamily);
    if (!storedFamily.equalsIgnoreCase(expectedFamily))
        return false;

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

    int warningCount = 0;
    auto settings = out.settings;
    settings.level = readFiniteXmlFloat(xml, "level", settings.level, 0.0f, 1.0f, &warningCount);
    settings.tuneSemitones = readFiniteXmlFloat(xml, "tune", settings.tuneSemitones, -24.0f, 24.0f, &warningCount);
    settings.brightness = readFiniteXmlFloat(xml, "brightness", settings.brightness, 0.0f, 1.0f, &warningCount);
    settings.attackSeconds = readFiniteXmlFloat(xml, "attack", settings.attackSeconds, 0.0f, 2.0f, &warningCount);
    settings.decaySeconds = readFiniteXmlFloat(xml, "decay", settings.decaySeconds, 0.1f, 10.0f, &warningCount);
    settings.sustainLevel = readFiniteXmlFloat(xml, "sustain", settings.sustainLevel, 0.0f, 1.0f, &warningCount);
    settings.releaseSeconds = readFiniteXmlFloat(xml, "release", settings.releaseSeconds, 0.01f, 5.0f, &warningCount);
    settings.vibrato = readFiniteXmlFloat(xml, "vibrato", settings.vibrato, 0.0f, 1.0f, &warningCount);
    settings.warmth = readFiniteXmlFloat(xml, "warmth", settings.warmth, 0.0f, 1.0f, &warningCount);
    settings.detune = readFiniteXmlFloat(xml, "detune", settings.detune, 0.0f, 1.0f, &warningCount);
    settings.stereoWidth = readFiniteXmlFloat(xml, "stereo_width", settings.stereoWidth, 0.0f, 1.0f, &warningCount);
    settings.character = readFiniteXmlFloat(xml, "character", settings.character, 0.0f, 1.0f, &warningCount);
    settings.cutoffHz = readFiniteXmlFloat(xml, "cutoff", settings.cutoffHz, 120.0f, 16000.0f, &warningCount);
    settings.pan = readFiniteXmlFloat(xml, "pan", settings.pan, -1.0f, 1.0f, &warningCount);
    out.settings = settings;

    out.outputBus = juce::jlimit(0, OrchSynthAudioProcessor::kNumAuxOutputs,
                                 xml.getIntAttribute("output", xml.getIntAttribute("output_bus", out.outputBus)));
    out.qualityMode = juce::jlimit(0, 1, xml.getIntAttribute("quality_mode", out.qualityMode));
    out.delaySync = juce::jlimit(0, 1, xml.getIntAttribute("delay_sync", out.delaySync));
    out.delayDivision = juce::jlimit(0, 5, xml.getIntAttribute("delay_division", out.delayDivision));
    out.velocityCurve = juce::jlimit(0, 6, xml.getIntAttribute("velocity_curve", out.velocityCurve));
    out.lfoRate = readFiniteXmlFloat(xml, "lfo_rate", out.lfoRate, 0.05f, 12.0f);
    out.lfoDepth = readFiniteXmlFloat(xml, "lfo_depth", out.lfoDepth, 0.0f, 1.0f);
    out.lfoWave = juce::jlimit(0, 3, xml.getIntAttribute("lfo_wave", out.lfoWave));
    out.portamentoSeconds = readFiniteXmlFloat(xml, "portamento_seconds", out.portamentoSeconds, 0.0f, 2.0f);
    out.legatoAmount = readFiniteXmlFloat(xml, "legato_amount", out.legatoAmount, 0.0f, 1.0f);
    out.roundRobinAmount = readFiniteXmlFloat(xml, "round_robin_amount", out.roundRobinAmount, 0.0f, 1.0f);
    out.macroWarmth = readFiniteXmlFloat(xml, "macro_warmth", out.macroWarmth, 0.0f, 1.0f);
    out.macroBrillance = readFiniteXmlFloat(xml, "macro_brillance", out.macroBrillance, 0.0f, 1.0f);
    out.macroSpace = readFiniteXmlFloat(xml, "macro_space", out.macroSpace, 0.0f, 1.0f);
    out.macroExpression = readFiniteXmlFloat(xml, "macro_expression", out.macroExpression, 0.0f, 1.0f);

    out.fx.satDrive = readValidatedXmlFloat(xml, "sat_drive", out.fx.satDrive, 1.0f, 16.0f, warningCount);
    out.fx.satMix = readValidatedXmlFloat(xml, "sat_mix", out.fx.satMix, 0.0f, 1.0f, warningCount);
    out.fx.transientAttack = readValidatedXmlFloat(xml, "transient_attack", out.fx.transientAttack, -1.0f, 1.0f, warningCount);
    out.fx.transientSustain = readValidatedXmlFloat(xml, "transient_sustain", out.fx.transientSustain, -1.0f, 1.0f, warningCount);
    out.fx.transientMix = readValidatedXmlFloat(xml, "transient_mix", out.fx.transientMix, 0.0f, 1.0f, warningCount);
    out.fx.eqLowFreq = readValidatedXmlFloat(xml, "eq_low_freq", out.fx.eqLowFreq, 40.0f, 600.0f, warningCount);
    out.fx.eqLowGain = readValidatedXmlFloat(xml, "eq_low_gain", out.fx.eqLowGain, -12.0f, 12.0f, warningCount);
    out.fx.eqMidFreq = readValidatedXmlFloat(xml, "eq_mid_freq", out.fx.eqMidFreq, 200.0f, 8000.0f, warningCount);
    out.fx.eqMidGain = readValidatedXmlFloat(xml, "eq_mid_gain", out.fx.eqMidGain, -12.0f, 12.0f, warningCount);
    out.fx.eqMidQ = readValidatedXmlFloat(xml, "eq_mid_q", out.fx.eqMidQ, 0.1f, 10.0f, warningCount);
    out.fx.eqHighFreq = readValidatedXmlFloat(xml, "eq_high_freq", out.fx.eqHighFreq, 1000.0f, 16000.0f, warningCount);
    out.fx.eqHighGain = readValidatedXmlFloat(xml, "eq_high_gain", out.fx.eqHighGain, -12.0f, 12.0f, warningCount);
    out.fx.compThreshold = readValidatedXmlFloat(xml, "comp_threshold", out.fx.compThreshold, -60.0f, 0.0f, warningCount);
    out.fx.compRatio = readValidatedXmlFloat(xml, "comp_ratio", out.fx.compRatio, 1.0f, 20.0f, warningCount);
    out.fx.compAttack = readValidatedXmlFloat(xml, "comp_attack", out.fx.compAttack, 0.1f, 100.0f, warningCount);
    out.fx.compRelease = readValidatedXmlFloat(xml, "comp_release", out.fx.compRelease, 5.0f, 500.0f, warningCount);
    out.fx.compMix = readValidatedXmlFloat(xml, "comp_mix", out.fx.compMix, 0.0f, 1.0f, warningCount);
    out.fx.chorusRate = readValidatedXmlFloat(xml, "chorus_rate", out.fx.chorusRate, 0.1f, 5.0f, warningCount);
    out.fx.chorusDepth = readValidatedXmlFloat(xml, "chorus_depth", out.fx.chorusDepth, 0.0f, 1.0f, warningCount);
    out.fx.chorusMix = readValidatedXmlFloat(xml, "chorus_mix", out.fx.chorusMix, 0.0f, 1.0f, warningCount);
    out.fx.delayTime = readValidatedXmlFloat(xml, "delay_time", out.fx.delayTime, 1.0f, 2000.0f, warningCount);
    out.fx.delayFeedback = readValidatedXmlFloat(xml, "delay_feedback", out.fx.delayFeedback, 0.0f, 0.95f, warningCount);
    out.fx.delayMix = readValidatedXmlFloat(xml, "delay_mix", out.fx.delayMix, 0.0f, 1.0f, warningCount);
    out.fx.reverbSize = readValidatedXmlFloat(xml, "reverb_size", out.fx.reverbSize, 0.0f, 1.0f, warningCount);
    out.fx.reverbDamping = readValidatedXmlFloat(xml, "reverb_damping", out.fx.reverbDamping, 0.0f, 1.0f, warningCount);
    out.fx.reverbWidth = readValidatedXmlFloat(xml, "reverb_width", out.fx.reverbWidth, 0.0f, 1.0f, warningCount);
    out.fx.reverbMix = readValidatedXmlFloat(xml, "reverb_mix", out.fx.reverbMix, 0.0f, 1.0f, warningCount);
    out.fx.reverbPredelay = readValidatedXmlFloat(xml, "reverb_predelay", out.fx.reverbPredelay, 0.0f, 100.0f, warningCount);
    out.fx.reverbType = juce::jlimit(0, 1, xml.getIntAttribute("reverb_type", out.fx.reverbType));
    out.fx.limiterThreshold = readValidatedXmlFloat(xml, "limiter_threshold", out.fx.limiterThreshold, -12.0f, 0.0f, warningCount);
    out.fx.limiterRelease = readValidatedXmlFloat(xml, "limiter_release", out.fx.limiterRelease, 1.0f, 500.0f, warningCount);
    out.fxEnables[0] = xml.getBoolAttribute("fx_tab0_en", out.fxEnables[0]);
    out.fxEnables[1] = xml.getBoolAttribute("fx_tab1_en", out.fxEnables[1]);
    out.fxEnables[2] = xml.getBoolAttribute("fx_tab2_en", out.fxEnables[2]);
    out.fxEnables[3] = xml.getBoolAttribute("fx_tab3_en", out.fxEnables[3]);
    out.fxEnables[4] = xml.getBoolAttribute("fx_eq_en", out.fxEnables[4]);
    out.fxEnables[5] = xml.getBoolAttribute("fx_chorus_en", out.fxEnables[5]);
    out.fxEnables[6] = xml.getBoolAttribute("fx_delay_en", out.fxEnables[6]);
    out.fxEnables[7] = xml.getBoolAttribute("fx_limiter_en", out.fxEnables[7]);
    out.fxLock = xml.getBoolAttribute("fx_lock", out.fxLock);

    for (int slotIndex = 0; slotIndex < OrchSynthAudioProcessor::kNumModMatrixSlots; ++slotIndex)
    {
        const auto sourceId = OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix);
        const auto destId = OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixDestSuffix);
        const auto amountId = OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix);
        out.modSources[static_cast<std::size_t>(slotIndex)] = juce::jlimit(0, 8, xml.getIntAttribute(sourceId, out.modSources[static_cast<std::size_t>(slotIndex)]));
        out.modDestinations[static_cast<std::size_t>(slotIndex)] = juce::jlimit(0, 12, xml.getIntAttribute(destId, out.modDestinations[static_cast<std::size_t>(slotIndex)]));
        out.modAmounts[static_cast<std::size_t>(slotIndex)] = readValidatedXmlFloat(xml,
            amountId.toRawUTF8(), out.modAmounts[static_cast<std::size_t>(slotIndex)], -1.0f, 1.0f, warningCount);
    }

    out.metadata.mixRole = readStringAttribute(xml, { "mix_role" }, out.metadata.mixRole);
    out.metadata.family = readStringAttribute(xml, { "preset_family" }, out.metadata.family);
    out.metadata.tags = readStringAttribute(xml, { "tags" }, out.metadata.tags);
    out.metadata.description = readStringAttribute(xml, { "description" }, out.metadata.description);
    out.metadata.outputProfile = readStringAttribute(xml, { "output_profile" }, out.metadata.outputProfile);
    out.metadata.nominalPeakDb = readFiniteXmlFloat(xml, "nominal_peak_db", out.metadata.nominalPeakDb, -24.0f, -1.0f);
    return true;
}

bool shouldRewritePresetXml(const juce::XmlElement& xml, const bool requirePresetIndex)
{
    if (readPresetFormatVersion(xml) < kPresetSchemaVersion)
        return true;

    static constexpr const char* kRequiredAttributes[] = {
        "name", "version", "format_version", "synth_index", "instr", "instrument_index", "family",
        "level", "tune", "brightness", "attack", "decay", "sustain", "release", "vibrato", "warmth",
        "detune", "stereo_width", "character", "cutoff", "pan", "output", "quality_mode",
        "delay_sync", "delay_division", "velocity_curve", "lfo_rate", "lfo_depth", "lfo_wave",
        "portamento_seconds", "legato_amount", "round_robin_amount",
        "macro_warmth", "macro_brillance", "macro_space", "macro_expression",
        "sat_drive", "sat_mix", "transient_attack", "transient_sustain", "transient_mix",
        "eq_low_freq", "eq_low_gain", "eq_mid_freq", "eq_mid_gain", "eq_mid_q", "eq_high_freq", "eq_high_gain",
        "comp_threshold", "comp_ratio", "comp_attack", "comp_release", "comp_mix",
        "chorus_rate", "chorus_depth", "chorus_mix", "delay_time", "delay_feedback", "delay_mix",
        "reverb_size", "reverb_damping", "reverb_width", "reverb_mix", "reverb_predelay", "reverb_type",
        "limiter_threshold", "limiter_release", "fx_tab0_en", "fx_tab1_en", "fx_tab2_en", "fx_tab3_en",
        "fx_eq_en", "fx_chorus_en", "fx_delay_en", "fx_limiter_en", "fx_lock",
        "mix_role", "preset_family", "tags", "description", "output_profile", "nominal_peak_db"
    };
    for (const auto* attribute : kRequiredAttributes)
    {
        if (!xml.hasAttribute(attribute))
            return true;
    }

    if (requirePresetIndex && (!xml.hasAttribute("preset") || !xml.hasAttribute("preset_index")))
        return true;

    for (int slotIndex = 0; slotIndex < OrchSynthAudioProcessor::kNumModMatrixSlots; ++slotIndex)
    {
        if (!xml.hasAttribute(OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix))
            || !xml.hasAttribute(OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixDestSuffix))
            || !xml.hasAttribute(OrchSynthAudioProcessor::makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix)))
        {
            return true;
        }
    }

    return false;
}

std::unique_ptr<juce::XmlElement> createPresetXml(const juce::String& tagName,
                                                  const OrchSynthAudioProcessor::PresetPersistenceState& state)
{
    auto root = std::make_unique<juce::XmlElement>(tagName);
    root->setAttribute("version", kPresetSchemaVersion);
    root->setAttribute("format_version", kPresetSchemaVersion);
    root->setAttribute("name", state.name);
    root->setAttribute("synth_index", kOrchSynthIndex);
    root->setAttribute("instr", state.instrIndex);
    root->setAttribute("instrument_index", state.instrIndex);
    root->setAttribute("family", utf8Text(mos::getFamilyName(static_cast<int>(mos::getFamily(state.instrIndex)))));
    if (state.presetIndex >= 0)
    {
        root->setAttribute("preset", state.presetIndex);
        root->setAttribute("preset_index", state.presetIndex);
    }
    root->setAttribute("output", juce::jlimit(0, OrchSynthAudioProcessor::kNumAuxOutputs, state.outputBus));
    writePresetSettingsAttributes(*root, state);
    writeGlobalPresetAttributes(*root, state);
    writeFxAttributes(*root, state);
    writeMetadataAttributes(*root, state.metadata);
    return root;
}

bool validateUserPresetXml(const juce::XmlElement& xml, const juce::File& file,
                           const int selectedInstr, juce::String& error)
{
    juce::ignoreUnused(file);
    auto fallback = makeDefaultPresetState(selectedInstr);
    fallback.name = "preset";
    if (!parsePresetXml(xml, "OrchPreset", selectedInstr, fallback, false, &fallback))
    {
        error = "invalid orch preset payload";
        return false;
    }
    return true;
}

void writeFxCacheStateProperties(juce::ValueTree& state, const mos::GlobalFxSettings& fx)
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
    state.setProperty("reverb_type", fx.reverbType, nullptr);
    state.setProperty("limiter_threshold", fx.limiterThreshold, nullptr);
    state.setProperty("limiter_release", fx.limiterRelease, nullptr);
}

mos::GlobalFxSettings readFxCacheStateProperties(const juce::ValueTree& state, mos::GlobalFxSettings fallback)
{
    auto fx = fallback;
    fx.satDrive         = static_cast<float>(state.getProperty("sat_drive", fx.satDrive));
    fx.satMix           = static_cast<float>(state.getProperty("sat_mix", fx.satMix));
    fx.transientAttack  = static_cast<float>(state.getProperty("transient_attack", fx.transientAttack));
    fx.transientSustain = static_cast<float>(state.getProperty("transient_sustain", fx.transientSustain));
    fx.transientMix     = static_cast<float>(state.getProperty("transient_mix", fx.transientMix));
    fx.eqLowFreq        = static_cast<float>(state.getProperty("eq_low_freq", fx.eqLowFreq));
    fx.eqLowGain        = static_cast<float>(state.getProperty("eq_low_gain", fx.eqLowGain));
    fx.eqMidFreq        = static_cast<float>(state.getProperty("eq_mid_freq", fx.eqMidFreq));
    fx.eqMidGain        = static_cast<float>(state.getProperty("eq_mid_gain", fx.eqMidGain));
    fx.eqMidQ           = static_cast<float>(state.getProperty("eq_mid_q", fx.eqMidQ));
    fx.eqHighFreq       = static_cast<float>(state.getProperty("eq_high_freq", fx.eqHighFreq));
    fx.eqHighGain       = static_cast<float>(state.getProperty("eq_high_gain", fx.eqHighGain));
    fx.compThreshold    = static_cast<float>(state.getProperty("comp_threshold", fx.compThreshold));
    fx.compRatio        = static_cast<float>(state.getProperty("comp_ratio", fx.compRatio));
    fx.compAttack       = static_cast<float>(state.getProperty("comp_attack", fx.compAttack));
    fx.compRelease      = static_cast<float>(state.getProperty("comp_release", fx.compRelease));
    fx.compMix          = static_cast<float>(state.getProperty("comp_mix", fx.compMix));
    fx.chorusRate       = static_cast<float>(state.getProperty("chorus_rate", fx.chorusRate));
    fx.chorusDepth      = static_cast<float>(state.getProperty("chorus_depth", fx.chorusDepth));
    fx.chorusMix        = static_cast<float>(state.getProperty("chorus_mix", fx.chorusMix));
    fx.delayTime        = static_cast<float>(state.getProperty("delay_time", fx.delayTime));
    fx.delayFeedback    = static_cast<float>(state.getProperty("delay_feedback", fx.delayFeedback));
    fx.delayMix         = static_cast<float>(state.getProperty("delay_mix", fx.delayMix));
    fx.reverbSize       = static_cast<float>(state.getProperty("reverb_size", fx.reverbSize));
    fx.reverbDamping    = static_cast<float>(state.getProperty("reverb_damping", fx.reverbDamping));
    fx.reverbWidth      = static_cast<float>(state.getProperty("reverb_width", fx.reverbWidth));
    fx.reverbMix        = static_cast<float>(state.getProperty("reverb_mix", fx.reverbMix));
    fx.reverbPredelay   = static_cast<float>(state.getProperty("reverb_predelay", fx.reverbPredelay));
    fx.reverbType       = static_cast<int>(state.getProperty("reverb_type", fx.reverbType));
    fx.limiterThreshold = static_cast<float>(state.getProperty("limiter_threshold", fx.limiterThreshold));
    fx.limiterRelease   = static_cast<float>(state.getProperty("limiter_release", fx.limiterRelease));
    return fx;
}

// ── MIDI CC page tables ──────────────────────────────────────────────────────
struct CCSlot {
    const char* paramId;      // global param, or nullptr for per-instrument
    const char* instrSuffix;  // per-instrument suffix (when paramId == nullptr)
};

static constexpr int kKnobsPerPage = 8;

static const char* kCCPageNames[] = {
    "MACROS",       // 0
    "ENVELOPE",     // 1
    "TONE",         // 2
    "REVERB/DELAY", // 3
    "DYNAMICS",     // 4
    "EQ",           // 5
    "MOD/LIMITER"   // 6
};

static const CCSlot kCCPages[][kKnobsPerPage] = {
    // Page 0 — Macros & Master
    { { "macro_warmth",     nullptr }, { "macro_brillance", nullptr },
      { "macro_space",      nullptr }, { "macro_expression", nullptr },
      { "lfo_rate",         nullptr }, { "lfo_depth",        nullptr },
      { "reverb_mix",       nullptr }, { "output_gain",      nullptr } },

    // Page 1 — Envelope (per-instrument)
    { { nullptr, "attack"     }, { nullptr, "decay"      },
      { nullptr, "sustain"    }, { nullptr, "release"    },
      { nullptr, "level"      }, { nullptr, "tune"       },
      { nullptr, "brightness" }, { nullptr, "vibrato"    } },

    // Page 2 — Tone (per-instrument + globals)
    { { nullptr, "warmth"       }, { nullptr, "detune"       },
      { nullptr, "stereo_width" }, { nullptr, "character"    },
      { nullptr, "cutoff"       }, { nullptr, "pan"          },
      { "output_gain", nullptr  }, { nullptr, nullptr        } },

    // Page 3 — Reverb & Delay
    { { "reverb_size",    nullptr }, { "reverb_damping",  nullptr },
      { "reverb_width",   nullptr }, { "reverb_mix",      nullptr },
      { "reverb_predelay", nullptr }, { "delay_time",      nullptr },
      { "delay_feedback", nullptr }, { "delay_mix",       nullptr } },

    // Page 4 — Dynamics
    { { "comp_threshold",    nullptr }, { "comp_ratio",         nullptr },
      { "comp_attack",       nullptr }, { "comp_release",       nullptr },
      { "comp_mix",          nullptr }, { "transient_attack",   nullptr },
      { "transient_sustain", nullptr }, { "transient_mix",      nullptr } },

    // Page 5 — EQ
    { { "eq_low_freq",  nullptr }, { "eq_low_gain",  nullptr },
      { "eq_mid_freq",  nullptr }, { "eq_mid_gain",  nullptr },
      { "eq_mid_q",     nullptr }, { "eq_high_freq", nullptr },
      { "eq_high_gain", nullptr }, { "sat_drive",    nullptr } },

    // Page 6 — Mod / Limiter
    { { "chorus_rate",       nullptr }, { "chorus_depth",       nullptr },
      { "chorus_mix",        nullptr }, { "limiter_threshold",  nullptr },
      { "limiter_release",   nullptr }, { "sat_mix",            nullptr },
      { "output_gain",       nullptr }, { nullptr,              nullptr } }
};

} // namespace

// =============================================================================
auto OrchSynthAudioProcessor::createBusLayout() -> BusesProperties
{
    BusesProperties buses;
    buses = buses.withOutput("Master", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < kNumAuxOutputs; ++i)
        buses = buses.withOutput("Instr " + juce::String(i + 1) + " Out",
                                 juce::AudioChannelSet::stereo(), false);
    return buses;
}

// =============================================================================
OrchSynthAudioProcessor::OrchSynthAudioProcessor()
    : AudioProcessor(createBusLayout()),
      parameters(*this, nullptr, juce::Identifier("MOS_PARAMS"), createParameterLayout()),
      factoryPresetBanks(mos::getFactoryPresetBanks())
{
    currentPresetIndices.fill(0);
    currentUserPresetFiles.fill(juce::File{});

    // Pre-allocate voice pool (avoids RT allocation on the audio thread)
    for (auto& slot : voices)
    {
        for (int i = 0; i < mos::kNumInstruments; ++i)
        {
            slot.voiceBank[static_cast<std::size_t>(i)] = mos::createVoiceForInstrument(i);
            slot.alternateVoiceBank[static_cast<std::size_t>(i)] = mos::createVoiceForInstrument(i);
        }
    }

    for (int instrIndex = 0; instrIndex < mos::kNumInstruments; ++instrIndex)
    {
        auto& persistedBank = factoryPresetStates[static_cast<std::size_t>(instrIndex)];
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(instrIndex)];
        persistedBank.reserve(bank.size());
        for (int presetIndex = 0; presetIndex < static_cast<int>(bank.size()); ++presetIndex)
            persistedBank.push_back(makeFactoryPresetState(instrIndex,
                                                           presetIndex,
                                                           bank[static_cast<std::size_t>(presetIndex)]));
    }

    loadFactoryOverrides();

    for (int b = 0; b < mos::kNumInstruments; ++b)
    {
        const auto& bootState = factoryPresetStates[static_cast<std::size_t>(b)].front();
        cachedFxPerInstr[static_cast<std::size_t>(b)] = sanitizeFxSettings(bootState.fx);
        applyInstrPresetSettings(b, bootState.settings);
        setParamValue(makeInstrParamId(b, kInstrOutputSuffix),
                      static_cast<float>(juce::jlimit(0, kNumAuxOutputs, bootState.outputBus)));
    }

    applyPresetPersistenceState(factoryPresetStates[0].front(), false);

    parameters.addParameterListener(kSelectedInstr, this);
    cachedSelectedInstrIndex = getSelectedInstrIndex();
    pendingSelectedInstrIndex.store(cachedSelectedInstrIndex);
    currentFxOwnerInstr.store(cachedSelectedInstrIndex);
    applyFxSettingsToParams(cachedFxPerInstr[static_cast<std::size_t>(cachedSelectedInstrIndex)], false);
    sanitizeAllParameters();
}

OrchSynthAudioProcessor::~OrchSynthAudioProcessor()
{
    cancelPendingUpdate();
    parameters.removeParameterListener(kSelectedInstr, this);
}

// =============================================================================
void OrchSynthAudioProcessor::applyInstrPresetSettings(int instrIndex, const mos::InstrSettings& s)
{
    const auto sanitized = sanitizeInstrSettings(instrIndex, s);
    setParamValue(makeInstrParamId(instrIndex, "level"),        sanitized.level);
    setParamValue(makeInstrParamId(instrIndex, "tune"),         sanitized.tuneSemitones);
    setParamValue(makeInstrParamId(instrIndex, "brightness"),   sanitized.brightness);
    setParamValue(makeInstrParamId(instrIndex, "attack"),       sanitized.attackSeconds);
    setParamValue(makeInstrParamId(instrIndex, "decay"),        sanitized.decaySeconds);
    setParamValue(makeInstrParamId(instrIndex, "sustain"),      sanitized.sustainLevel);
    setParamValue(makeInstrParamId(instrIndex, "release"),      sanitized.releaseSeconds);
    setParamValue(makeInstrParamId(instrIndex, "vibrato"),      sanitized.vibrato);
    setParamValue(makeInstrParamId(instrIndex, "warmth"),       sanitized.warmth);
    setParamValue(makeInstrParamId(instrIndex, "detune"),       sanitized.detune);
    setParamValue(makeInstrParamId(instrIndex, "stereo_width"), sanitized.stereoWidth);
    setParamValue(makeInstrParamId(instrIndex, "character"),    sanitized.character);
    setParamValue(makeInstrParamId(instrIndex, "cutoff"),       sanitized.cutoffHz);
    setParamValue(makeInstrParamId(instrIndex, "pan"),          sanitized.pan);
}

// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
OrchSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& banks = mos::getFactoryPresetBanks();
    const auto outputChoices = makeOutputChoices();

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kOutputGain, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.01f), -3.0f));

    juce::StringArray instrChoices;
    for (int i = 0; i < mos::kNumInstruments; ++i)
        instrChoices.add(utf8Text(mos::getInstrName(i)));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kSelectedInstr, "Selected Instrument", instrChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kQualityMode, "Quality Mode", makeQualityChoices(), 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoRate, "LFO Rate",
        juce::NormalisableRange<float>(0.05f, 12.0f, 0.0001f), 1.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoDepth, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kLfoWave, "LFO Wave",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square" }, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kVelocityCurve, "Velocity Curve",
        juce::StringArray{ "Linear", "Soft", "Softer", "Hard", "Harder", "Fixed", "Touch" }, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kPortamentoSeconds, "Portamento Seconds",
        juce::NormalisableRange<float>(0.0f, 2.0f, 0.001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLegatoAmount, "Legato Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kRoundRobinAmount, "Round Robin Amount",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));

    // Macros
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroWarmth, "Macro Chaleur",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroBrillance, "Macro Brillance",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroSpace, "Macro Espace",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroExpression, "Macro Expression",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));

    for (int slotIndex = 0; slotIndex < kNumModMatrixSlots; ++slotIndex)
    {
        const auto slotLabel = "Mod Slot " + juce::String(slotIndex + 1) + " ";
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix),
            slotLabel + "Source",
            makeModMatrixSourceChoices(),
            0));
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeModMatrixParamId(slotIndex, kModMatrixDestSuffix),
            slotLabel + "Destination",
            makeModMatrixDestinationChoices(),
            0));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix),
            slotLabel + "Amount",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f),
            0.0f));
    }

    // FX: Compressor
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompThreshold, "Comp Threshold",
        juce::NormalisableRange<float>(-60.0f, 0.0f, 0.01f), -19.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompRatio, "Comp Ratio",
        juce::NormalisableRange<float>(1.0f, 20.0f, 0.01f), 3.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompAttack, "Comp Attack",
        juce::NormalisableRange<float>(0.1f, 100.0f, 0.01f), 10.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompRelease, "Comp Release",
        juce::NormalisableRange<float>(5.0f, 500.0f, 0.01f), 120.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kCompMix, "Comp Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 1.0f));

    // FX: Saturator
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kSatDrive, "Sat Drive",
        juce::NormalisableRange<float>(1.0f, 16.0f, 0.01f), 1.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kSatMix, "Sat Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.10f));

    // FX: Transient
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientAttack, "Transient Attack",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.05f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientSustain, "Transient Sustain",
        juce::NormalisableRange<float>(-1.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kTransientMix, "Transient Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.3f));

    // FX: Reverb
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbSize, "Reverb Size",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbSize));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbDamping, "Reverb Damping",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbDamping));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbWidth, "Reverb Width",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbWidth));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbMix, "Reverb Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), kDefaultReverbMix));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kReverbPredelay, "Reverb Pre-Delay",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kReverbType, "Reverb Type", makeReverbTypeChoices(), 0));

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
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kDelaySync, "Delay Sync", makeDelaySyncChoices(), 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kDelayDivision, "Delay Division", makeDelayDivisionChoices(), 1));

    // --- FX: Limiter ---
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterThreshold, "Limiter Threshold",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.01f), -1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterRelease,   "Limiter Release",
        juce::NormalisableRange<float>(1.0f, 500.0f, 0.1f), 200.0f));

    // FX enable toggles (true = active)
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab0_en",     "FX Reverb Enable",     true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab1_en",     "FX Saturator Enable",  true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab2_en",     "FX Transient Enable",  true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_tab3_en",     "FX Compressor Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_eq_en",       "FX EQ Enable",         true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_chorus_en",   "FX Chorus Enable",     true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_delay_en",    "FX Delay Enable",      true));
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_limiter_en",  "FX Limiter Enable",    true));

    // FX Lock — when on, switching instruments does NOT overwrite FX chain
    layout.add(std::make_unique<juce::AudioParameterBool>("fx_lock", "FX Lock", false));

    // Per-instrument parameters (20 instruments x 14 + output)
    for (int b = 0; b < mos::kNumInstruments; ++b)
    {
        const auto& def = banks[static_cast<std::size_t>(b)][0].settings;
        const auto prefix = utf8Text(mos::getInstrName(b)) + " ";

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "level"), prefix + "Level",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.level));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "tune"), prefix + "Tune",
            juce::NormalisableRange<float>(-24.0f, 24.0f, 0.01f), def.tuneSemitones));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "brightness"), prefix + "Brightness",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.brightness));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "attack"), prefix + "Attack",
            juce::NormalisableRange<float>(0.0f, 2.0f, 0.0001f), def.attackSeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "decay"), prefix + "Decay",
            juce::NormalisableRange<float>(0.1f, 10.0f, 0.001f), def.decaySeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "sustain"), prefix + "Sustain",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.sustainLevel));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "release"), prefix + "Release",
            juce::NormalisableRange<float>(0.01f, 5.0f, 0.0001f), def.releaseSeconds));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "vibrato"), prefix + "Vibrato",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.vibrato));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "warmth"), prefix + "Warmth",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.warmth));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "detune"), prefix + "Detune",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.detune));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "stereo_width"), prefix + "Stereo Width",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.stereoWidth));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "character"), prefix + "Character",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.character));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "cutoff"), prefix + "Cutoff",
            juce::NormalisableRange<float>(120.0f, 16000.0f, 0.0f, 0.28f), def.cutoffHz));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "pan"), prefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), def.pan));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeInstrParamId(b, kInstrOutputSuffix), prefix + "Output",
            outputChoices, 0));
    }

    return layout;
}

juce::String OrchSynthAudioProcessor::makeInstrParamId(int instrIndex, const juce::String& suffix)
{
    return "instr_" + juce::String(instrIndex) + "_" + suffix;
}

juce::String OrchSynthAudioProcessor::makeModMatrixParamId(int slotIndex, const juce::String& suffix)
{
    return "mod_" + juce::String(slotIndex) + "_" + suffix;
}

// =============================================================================
void OrchSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = std::max(1.0, sampleRate);
    const int scratchSamples = juce::jmax(32768, samplesPerBlock);

    for (auto& slot : voices)
    {
        slot.active = nullptr;
        slot.dying = nullptr;
        slot.dyingBus = 0;
        slot.dyingMidiChannel = 0;
        slot.sourceMidiNote = -1;
        slot.renderMidiNote = -1;
        slot.midiChannel = 0;
        slot.deferredNoteOff = false;
        slot.heldAfterKeyRelease = false;
        slot.activeTailHintSeconds = 0.0f;
        slot.dyingTailHintSeconds = 0.0f;
        slot.activationAge = 0;
    }

    const juce::dsp::ProcessSpec spec {
        preparedSampleRate,
        static_cast<juce::uint32>(juce::jmax(1, scratchSamples)),
        static_cast<juce::uint32>(juce::jmax(1, getMainBusNumOutputChannels()))
    };

    compressor.reset();
    compressor.prepare(spec);
    const auto threshold = getParamValue(kCompThreshold);
    const auto ratio = getParamValue(kCompRatio);
    const auto attack = getParamValue(kCompAttack);
    const auto release = getParamValue(kCompRelease);
    compressor.setThreshold(threshold);
    compressor.setRatio(ratio);
    compressor.setAttack(attack);
    compressor.setRelease(release);
    compCache.threshold = threshold;
    compCache.ratio = ratio;
    compCache.attack = attack;
    compCache.release = release;
    fxDryBuffer.setSize(static_cast<int>(spec.numChannels),
                        static_cast<int>(spec.maximumBlockSize), false, true, true);
    lfoPhase = 0.0f;
    globalLfoDepthCurrent = 0.0f;
    realtimePerformanceBlock = captureRealtimePerformanceBlock();
    satDriveCurrent = 1.5f;
    satMixCurrent = 0.0f;
    saturatorPrevInput = { 0.0f, 0.0f };
    resetMidiPerformanceState();
    for (auto& bend : pitchBendPerChannel)
        bend.reset();
    liveVoiceCount.store(0);
    displayedVoiceCount.store(0);
    lastKnownHostTempoBpm.store(120.0f, std::memory_order_relaxed);
    for (auto& meter : mainMeterLevels)
        meter.store(0.0f, std::memory_order_relaxed);
    for (auto& meter : auxMeterLevels)
        meter.store(0.0f, std::memory_order_relaxed);
    clipLatched.store(false, std::memory_order_relaxed);

    // New FX processors
    transientShaper.prepare(preparedSampleRate);
    saturator = {};
    reverb.prepare(preparedSampleRate, scratchSamples);
    hallReverb.prepare(preparedSampleRate, scratchSamples);
    eq.prepare(preparedSampleRate);
    chorus.prepare(preparedSampleRate, scratchSamples);
    stereoDelay.prepare(preparedSampleRate, scratchSamples);
    limiter.prepare(preparedSampleRate);
    satOversampling.initProcessing(static_cast<size_t>(scratchSamples));
    satOversampling.reset();

    outputGainSmoother.reset(preparedSampleRate, 0.02);
    outputGainSmoother.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(getParamValue(kOutputGain)));
}

void OrchSynthAudioProcessor::releaseResources()
{
    for (auto& slot : voices)
    {
        slot.active = nullptr;
        slot.dying = nullptr;
        slot.dyingBus = 0;
        slot.dyingMidiChannel = 0;
        slot.sourceMidiNote = -1;
        slot.renderMidiNote = -1;
        slot.midiChannel = 0;
        slot.deferredNoteOff = false;
        slot.heldAfterKeyRelease = false;
        slot.activeTailHintSeconds = 0.0f;
        slot.dyingTailHintSeconds = 0.0f;
        slot.activationAge = 0;
    }
    for (auto& bend : pitchBendPerChannel)
        bend.reset();
    resetMidiPerformanceState();
    liveVoiceCount.store(0);
    displayedVoiceCount.store(0);
    fxDryBuffer.setSize(0, 0);
    for (auto& meter : mainMeterLevels)
        meter.store(0.0f, std::memory_order_relaxed);
    for (auto& meter : auxMeterLevels)
        meter.store(0.0f, std::memory_order_relaxed);
    clipLatched.store(false, std::memory_order_relaxed);
}

bool OrchSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.outputBuses.isEmpty())
        return false;

    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() &&
        mainOutput != juce::AudioChannelSet::stereo())
        return false;

    for (int busIndex = 1; busIndex < layouts.outputBuses.size(); ++busIndex)
    {
        const auto auxSet = layouts.getChannelSet(false, busIndex);
        if (auxSet.isDisabled()) continue;
        if (auxSet != juce::AudioChannelSet::mono() &&
            auxSet != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

// =============================================================================
void OrchSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    if (pendingPanicAllVoices.exchange(false, std::memory_order_acq_rel))
        panicAllVoices();

    const auto outputBusCount = getBusCount(false);
    for (int busIndex = 0; busIndex < outputBusCount; ++busIndex)
        getBusBuffer(buffer, false, busIndex).clear();

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    const int selectedInstr = getSelectedInstrIndex();
    velocityCurve = intToVelocityCurve(static_cast<int>(std::round(getParamValue(kVelocityCurve))));
    lastKnownHostTempoBpm.store(static_cast<float>(readHostTempoBpm()), std::memory_order_relaxed);
    realtimePerformanceBlock = captureRealtimePerformanceBlock();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        const int ch = juce::jlimit(0, 15, msg.getChannel() - 1);

        if (msg.isNoteOn())
        {
            const float vel = applyVelocityCurve(msg.getFloatVelocity(), velocityCurve);
            channelPerformance[static_cast<std::size_t>(ch)].lastNoteVelocity = vel;  // FIX: P8
            triggerNoteOn(selectedInstr, msg.getNoteNumber(), vel, ch);
        }
        else if (msg.isNoteOff())
            triggerNoteOff(selectedInstr, msg.getNoteNumber(), ch);
        else if (msg.isController() && msg.getControllerNumber() == 64)
        {
            const int ccVal = msg.getControllerValue();
            const float previousDamper = damperPosition[static_cast<std::size_t>(ch)];
            damperPosition[static_cast<std::size_t>(ch)] = ccVal / 127.0f;

            if (ccVal >= 96)
            {
                sustainPedalDown[static_cast<std::size_t>(ch)] = true;
            }
            else if (ccVal <= 80)
            {
                const bool wasSustaining = sustainPedalDown[static_cast<std::size_t>(ch)];
                sustainPedalDown[static_cast<std::size_t>(ch)] = false;
                if (wasSustaining)
                    releaseSustainedVoices(ch, previousDamper);
            }
        }
        else if (msg.isController() && msg.getControllerNumber() == 2)
            channelPerformance[static_cast<std::size_t>(ch)].breath = msg.getControllerValue() / 127.0f;
        else if (msg.isController() && msg.getControllerNumber() == 1)
        {
            channelPerformance[static_cast<std::size_t>(ch)].modWheel = msg.getControllerValue() / 127.0f;
            channelPerformance[static_cast<std::size_t>(ch)].modWheelSeen = true;
        }
        else if (msg.isController() && msg.getControllerNumber() == 11)
            channelPerformance[static_cast<std::size_t>(ch)].expression = msg.getControllerValue() / 127.0f;
        else if (msg.isController() && msg.getControllerNumber() == 121)
        {
            pitchBendPerChannel[static_cast<std::size_t>(ch)].reset();
            channelPerformance[static_cast<std::size_t>(ch)] = {};
            sustainPedalDown[static_cast<std::size_t>(ch)] = false;
            damperPosition[static_cast<std::size_t>(ch)] = 0.0f;
            releaseSustainedVoices(ch, 0.0f);
        }
        else if (msg.isController()
                 && (msg.getControllerNumber() == 120
                     || msg.getControllerNumber() == 123))
        {
            panicAllVoices();
        }
        else if (msg.isChannelPressure())
            channelPerformance[static_cast<std::size_t>(ch)].aftertouch = msg.getChannelPressureValue() / 127.0f;
        else if (msg.isAftertouch())
            channelPerformance[static_cast<std::size_t>(ch)].aftertouch = msg.getAfterTouchValue() / 127.0f;
        else if (msg.isPitchWheel())
            pitchBendPerChannel[static_cast<std::size_t>(ch)].setPitchWheel(msg.getPitchWheelValue());
        else if (msg.isController())
            handleMidiCC(msg.getControllerNumber(), msg.getControllerValue(), selectedInstr);
    }

    midiMessages.clear();

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    int liveVoices = 0;
    int displayedVoices = 0;
    for (auto& slot : voices)
    {
        // Render dying voice (stolen voice fading out)
        if (slot.dying != nullptr)
        {
            if (!slot.dying->isActive())
            {
                slot.dying = nullptr;
                slot.dyingTailHintSeconds = 0.0f;
            }
            else
            {
                ++liveVoices;
                const int dyingChannel = juce::jlimit(0, 15, slot.dyingMidiChannel);
                slot.dying->setPitchBendFactor(
                    pitchBendPerChannel[static_cast<std::size_t>(dyingChannel)].pitchBendFactor);
                applyRealtimePerformance(*slot.dying, dyingChannel, realtimePerformanceBlock);
                int dBus = juce::jlimit(0, outputBusCount - 1, slot.dyingBus);
                if (dBus > 0 && getChannelCountOfBus(false, dBus) <= 0)
                    dBus = 0;
                if (dBus == 0)
                    slot.dying->render(mainBuffer, 0, mainBuffer.getNumSamples());
                else
                {
                    auto dBuf = getBusBuffer(buffer, false, dBus);
                    if (dBuf.getNumChannels() > 0 && dBuf.getNumSamples() > 0)
                        slot.dying->render(dBuf, 0, dBuf.getNumSamples());
                    else
                        slot.dying->render(mainBuffer, 0, mainBuffer.getNumSamples());
                }
                if (!slot.dying->isActive())
                {
                    slot.dying = nullptr;
                    slot.dyingBus = 0;
                    slot.dyingMidiChannel = 0;
                    slot.dyingTailHintSeconds = 0.0f;
                }
            }
        }

        if (slot.active != nullptr && !slot.active->isActive())
        {
            slot.active = nullptr;
            slot.sourceMidiNote = -1;
            slot.renderMidiNote = -1;
            slot.deferredNoteOff = false;
            slot.heldAfterKeyRelease = false;
            slot.activeTailHintSeconds = 0.0f;
        }

        // Render active voice
        if (slot.active != nullptr && slot.active->isActive())
        {
            ++liveVoices;
            if (!slot.active->isReleasing())
                ++displayedVoices;
            const int voiceChannel = juce::jlimit(0, 15, slot.midiChannel);
            slot.active->setPitchBendFactor(
                pitchBendPerChannel[static_cast<std::size_t>(voiceChannel)].pitchBendFactor);
            applyRealtimePerformance(*slot.active, voiceChannel, realtimePerformanceBlock);
            int targetBus = juce::jlimit(0, outputBusCount - 1,
                static_cast<int>(std::round(getParamValue(
                    makeInstrParamId(slot.instrIndex, kInstrOutputSuffix)))));
            if (targetBus > 0 && getChannelCountOfBus(false, targetBus) <= 0)
                targetBus = 0;

            if (targetBus == 0)
            {
                slot.active->render(mainBuffer, 0, mainBuffer.getNumSamples());
            }
            else
            {
                auto targetBuffer = getBusBuffer(buffer, false, targetBus);
                if (targetBuffer.getNumChannels() > 0 && targetBuffer.getNumSamples() > 0)
                    slot.active->render(targetBuffer, 0, targetBuffer.getNumSamples());
                else
                    slot.active->render(mainBuffer, 0, mainBuffer.getNumSamples());
            }
        }
    }

    liveVoiceCount.store(liveVoices);
    displayedVoiceCount.store(displayedVoices);

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
    }

    updateOutputMeters(buffer, mainBuffer);

    if (pendingFxRecallInstrIndex.load() >= 0 && liveVoices == 0)
        triggerAsyncUpdate();
}

// =============================================================================
// MIDI CC paged mapping
// =============================================================================
const char* OrchSynthAudioProcessor::getCCPageName(int page) noexcept
{
    if (page >= 0 && page < kNumCCPages)
        return kCCPageNames[page];
    return "???";
}

void OrchSynthAudioProcessor::handleMidiCC(int ccNumber, int ccValue, int instrIndex)
{
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
        paramId = makeInstrParamId(instrIndex, slot.instrSuffix);
    else
        return;

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId)))
    {
        const float normalised = static_cast<float>(ccValue) / 127.0f;
        queueParamUpdate(parameter, normalised);
    }
}

// =============================================================================
juce::AudioProcessorEditor* OrchSynthAudioProcessor::createEditor()
{
    return new OrchSynthAudioProcessorEditor(*this);
}

double OrchSynthAudioProcessor::getTailLengthSeconds() const
{
    double tailSeconds = 0.25;

    for (const auto& slot : voices)
    {
        if (slot.active != nullptr && slot.active->isActive())
            tailSeconds = juce::jmax(tailSeconds, static_cast<double>(slot.activeTailHintSeconds));
        if (slot.dying != nullptr && slot.dying->isActive())
            tailSeconds = juce::jmax(tailSeconds, static_cast<double>(slot.dyingTailHintSeconds));
    }

    const int fxOwnerInstr = juce::jlimit(0, mos::kNumInstruments - 1, currentFxOwnerInstr.load());

    const bool reverbEnabled = mos::isFxAvailable(fxOwnerInstr, mos::GlobalFxSlot::Reverb)
        && getParamValue("fx_tab0_en") >= 0.5f;
    if (reverbEnabled)
    {
        const auto reverbMix = clamp01(getParamValue(kReverbMix));
        const auto reverbSize = clamp01(getParamValue(kReverbSize));
        tailSeconds = juce::jmax(tailSeconds,
                                 0.3 + static_cast<double>(reverbSize) * 6.5
                                     + static_cast<double>(reverbMix) * 4.5);
    }

    const bool delayEnabled = mos::isFxAvailable(fxOwnerInstr, mos::GlobalFxSlot::Delay)
        && getParamValue("fx_delay_en") >= 0.5f;
    if (delayEnabled)
    {
        double delayTimeSec = juce::jlimit(0.01, 2.0, static_cast<double>(getParamValue(kDelayTime)) * 0.001);
        if (getParamValue(kDelaySync) >= 0.5f)
        {
            static constexpr double kDivisionBeats[] = { 1.0, 0.5, 0.75, 1.0 / 3.0, 0.25, 0.375 };
            const auto bpm = juce::jlimit(20.0, 240.0, readHostTempoBpm());
            const auto division = juce::jlimit(0, 5, static_cast<int>(std::round(getParamValue(kDelayDivision))));
            delayTimeSec = (60.0 / bpm) * kDivisionBeats[static_cast<std::size_t>(division)];
        }

        const auto feedback = juce::jlimit(0.0f, 0.95f, getParamValue(kDelayFeedback));
        const auto repeatFactor = juce::jlimit(1.0, 10.0, 1.0 + static_cast<double>(feedback) * 10.0);
        tailSeconds = juce::jmax(tailSeconds, delayTimeSec * repeatFactor + 0.25);
    }

    return juce::jlimit(0.1, 30.0, tailSeconds);
}

// =============================================================================
int OrchSynthAudioProcessor::getNumPrograms()
{
    const int sel = getSelectedInstrIndex();
    return static_cast<int>(factoryPresetBanks[static_cast<std::size_t>(sel)].size());
}

int OrchSynthAudioProcessor::getCurrentProgram()
{
    return juce::jmax(0, currentPresetIndices[static_cast<std::size_t>(getSelectedInstrIndex())]);
}

void OrchSynthAudioProcessor::setCurrentProgram(int index) { applyFactoryPreset(index); }

const juce::String OrchSynthAudioProcessor::getProgramName(int index)
{
    const int sel = getSelectedInstrIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(sel)];
    if (index < 0 || index >= static_cast<int>(bank.size())) return {};
    return juce::String(juce::CharPointer_UTF8(bank[static_cast<std::size_t>(index)].name.c_str()));
}

void OrchSynthAudioProcessor::changeProgramName(int, const juce::String&) {}

void OrchSynthAudioProcessor::randomizePreset(float amount)
{
    auto& rng = juce::Random::getSystemRandom();
    const int instrIndex = getSelectedInstrIndex();

    auto randomizeNormalizedParam = [this, &rng, amount](const juce::String& paramId, const float scale = 1.0f)
    {
        if (auto* param = parameters.getParameter(paramId))
        {
            const float currentValue = param->getValue();
            const float delta = (rng.nextFloat() * 2.0f - 1.0f) * amount * scale;
            param->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, currentValue + delta));
        }
    };

    for (const auto* suffix : { "level", "brightness", "attack", "decay", "sustain", "release",
                                "vibrato", "warmth", "detune", "stereo_width", "character", "cutoff" })
        randomizeNormalizedParam(makeInstrParamId(instrIndex, suffix));

    randomizeNormalizedParam(makeInstrParamId(instrIndex, "pan"), 0.55f);
    randomizeNormalizedParam(kLfoRate, 0.75f);
    randomizeNormalizedParam(kLfoDepth, 0.75f);
    randomizeNormalizedParam(kMacroWarmth, 0.90f);
    randomizeNormalizedParam(kMacroBrillance, 0.90f);
    randomizeNormalizedParam(kMacroSpace, 0.90f);
    randomizeNormalizedParam(kMacroExpression, 0.90f);
}

void OrchSynthAudioProcessor::requestPanicAllVoices() noexcept
{
    pendingPanicAllVoices.store(true, std::memory_order_release);
}

// =============================================================================
void OrchSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    const int fxOwnerInstr = juce::jlimit(0, mos::kNumInstruments - 1, currentFxOwnerInstr.load());
    cachedFxPerInstr[static_cast<std::size_t>(fxOwnerInstr)] = snapshotFx();

    auto state = parameters.copyState();
    for (int childIndex = state.getNumChildren(); --childIndex >= 0;)
    {
        if (state.getChild(childIndex).hasType("instr_fx_cache"))
            state.removeChild(childIndex, nullptr);
    }
    for (int b = 0; b < mos::kNumInstruments; ++b)
    {
        state.setProperty("pi_" + juce::String(b),
                          currentPresetIndices[static_cast<std::size_t>(b)], nullptr);
        if (currentUserPresetFiles[static_cast<std::size_t>(b)].existsAsFile())
            state.setProperty("upf_" + juce::String(b),
                              currentUserPresetFiles[static_cast<std::size_t>(b)].getFullPathName(), nullptr);

        juce::ValueTree fxCache("instr_fx_cache");
        fxCache.setProperty("inst", b, nullptr);
        writeFxCacheStateProperties(fxCache, cachedFxPerInstr[static_cast<std::size_t>(b)]);
        state.appendChild(fxCache, nullptr);
    }
    state.setProperty(kStopNotesOnKeyReleaseState, shouldStopNotesOnKeyRelease(), nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void OrchSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState == nullptr || !xmlState->hasTagName(parameters.state.getType()))
        return;

    auto restoredState = juce::ValueTree::fromXml(*xmlState);
    setStateFloatProperty(restoredState, kSelectedInstr, readFiniteStateFloat(restoredState, kSelectedInstr, 0.0f, 0.0f, static_cast<float>(mos::kNumInstruments - 1)));
    setStateFloatProperty(restoredState, kOutputGain, readFiniteStateFloat(restoredState, kOutputGain, -3.0f, -24.0f, 12.0f));
    setStateFloatProperty(restoredState, kQualityMode, readFiniteStateFloat(restoredState, kQualityMode, 0.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kDelaySync, readFiniteStateFloat(restoredState, kDelaySync, 0.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kDelayDivision, readFiniteStateFloat(restoredState, kDelayDivision, 1.0f, 0.0f, 5.0f));
    setStateFloatProperty(restoredState, kLfoRate, readFiniteStateFloat(restoredState, kLfoRate, 1.1f, 0.05f, 12.0f));
    setStateFloatProperty(restoredState, kLfoDepth, readFiniteStateFloat(restoredState, kLfoDepth, 0.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kLfoWave, readFiniteStateFloat(restoredState, kLfoWave, 0.0f, 0.0f, 3.0f));
    setStateFloatProperty(restoredState, kVelocityCurve, readFiniteStateFloat(restoredState, kVelocityCurve, 0.0f, 0.0f, 6.0f));
    setStateFloatProperty(restoredState, kPortamentoSeconds, readFiniteStateFloat(restoredState, kPortamentoSeconds, 0.0f, 0.0f, 2.0f));
    setStateFloatProperty(restoredState, kLegatoAmount, readFiniteStateFloat(restoredState, kLegatoAmount, 1.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kRoundRobinAmount, readFiniteStateFloat(restoredState, kRoundRobinAmount, 0.5f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kReverbType, readFiniteStateFloat(restoredState, kReverbType, 0.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kSatDrive, readFiniteStateFloat(restoredState, kSatDrive, 1.8f, 1.0f, 16.0f));
    setStateFloatProperty(restoredState, "fx_lock", readFiniteStateFloat(restoredState, "fx_lock", 0.0f, 0.0f, 1.0f));

    for (int slotIndex = 0; slotIndex < kNumModMatrixSlots; ++slotIndex)
    {
        setStateFloatProperty(restoredState,
                              makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix),
                              readFiniteStateFloat(restoredState,
                                                   makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix),
                                                   0.0f, 0.0f, 8.0f));
        setStateFloatProperty(restoredState,
                              makeModMatrixParamId(slotIndex, kModMatrixDestSuffix),
                              readFiniteStateFloat(restoredState,
                                                   makeModMatrixParamId(slotIndex, kModMatrixDestSuffix),
                                                   0.0f, 0.0f, 12.0f));
        setStateFloatProperty(restoredState,
                              makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix),
                              readFiniteStateFloat(restoredState,
                                                   makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix),
                                                   0.0f, -1.0f, 1.0f));
    }

    for (int instrIndex = 0; instrIndex < mos::kNumInstruments; ++instrIndex)
    {
        const auto defaults = mos::getDefaultSettings(instrIndex);
        setStateFloatProperty(restoredState,
                              makeInstrParamId(instrIndex, "level"),
                              readFiniteStateFloat(restoredState,
                                                   makeInstrParamId(instrIndex, "level"),
                                                   defaults.level, 0.0f, 1.0f));
        setStateFloatProperty(restoredState,
                              makeInstrParamId(instrIndex, kInstrOutputSuffix),
                              readFiniteStateFloat(restoredState,
                                                   makeInstrParamId(instrIndex, kInstrOutputSuffix),
                                                   0.0f, 0.0f, static_cast<float>(kNumAuxOutputs)));
    }

    sanitizeStateParameterValues(parameters, restoredState);
    cancelPendingUpdate();
    isRestoringState = true;
    parameters.replaceState(restoredState);
    isRestoringState = false;
    sanitizeAllParameters();
    stopNotesOnKeyRelease.store(readStateBoolProperty(restoredState, kStopNotesOnKeyReleaseState, true),
                                std::memory_order_relaxed);

    for (int b = 0; b < mos::kNumInstruments; ++b)
    {
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
        cachedFxPerInstr[static_cast<std::size_t>(b)] = sanitizeFxSettings(bank.empty()
            ? mos::GlobalFxSettings{}
            : bank[0].fx);
    }

    for (int b = 0; b < mos::kNumInstruments; ++b)
    {
        const int maxIdx = static_cast<int>(factoryPresetBanks[static_cast<std::size_t>(b)].size()) - 1;
        currentPresetIndices[static_cast<std::size_t>(b)] =
            juce::jlimit(0, juce::jmax(0, maxIdx),
                         static_cast<int>(restoredState.getProperty("pi_" + juce::String(b), 0)));

        currentUserPresetFiles[static_cast<std::size_t>(b)] = juce::File{};
        auto upf = restoredState.getProperty("upf_" + juce::String(b), "").toString();
        if (upf.isNotEmpty())
        {
            juce::File f(upf);
            if (f.existsAsFile())
                currentUserPresetFiles[static_cast<std::size_t>(b)] = f;
        }
    }

    const int restoredSelectedInstr = getSelectedInstrIndex();
    if (restoredSelectedInstr >= 0 && restoredSelectedInstr < mos::kNumInstruments)
        cachedFxPerInstr[static_cast<std::size_t>(restoredSelectedInstr)] = snapshotFx();

    std::array<bool, mos::kNumInstruments> restoredFxCacheSeen {};
    for (int childIndex = 0; childIndex < restoredState.getNumChildren(); ++childIndex)
    {
        const auto child = restoredState.getChild(childIndex);
        if (!child.hasType("instr_fx_cache"))
            continue;

        int inst = -1;
        if (!tryReadValidatedChildInstrumentIndex(child, inst))
            continue;

        if (restoredFxCacheSeen[static_cast<std::size_t>(inst)])
            continue;

        restoredFxCacheSeen[static_cast<std::size_t>(inst)] = true;
        cachedFxPerInstr[static_cast<std::size_t>(inst)] = sanitizeFxSettings(readFxCacheStateProperties(
            child, cachedFxPerInstr[static_cast<std::size_t>(inst)]));
    }

    const int selectedInstr = getSelectedInstrIndex();
    cachedSelectedInstrIndex = selectedInstr;
    pendingSelectedInstrIndex.store(selectedInstr);
    pendingFxRecallInstrIndex.store(-1);
    currentFxOwnerInstr.store(selectedInstr);
    liveVoiceCount.store(0);
    displayedVoiceCount.store(0);
    cancelPendingUpdate();
    applyFxSettingsToParams(cachedFxPerInstr[static_cast<std::size_t>(selectedInstr)], false);
    sanitizeAllParameters();
    const auto threshold = getParamValue(kCompThreshold);
    const auto ratio = getParamValue(kCompRatio);
    const auto attack = getParamValue(kCompAttack);
    const auto release = getParamValue(kCompRelease);
    compressor.setThreshold(threshold);
    compressor.setRatio(ratio);
    compressor.setAttack(attack);
    compressor.setRelease(release);
    compCache.threshold = threshold;
    compCache.ratio = ratio;
    compCache.attack = attack;
    compCache.release = release;
}

// =============================================================================
juce::StringArray OrchSynthAudioProcessor::getFactoryPresetNames() const
{
    juce::StringArray names;
    for (const auto& p : factoryPresetBanks[static_cast<std::size_t>(getSelectedInstrIndex())])
        names.add(juce::String(juce::CharPointer_UTF8(p.name.c_str())));
    return names;
}

int OrchSynthAudioProcessor::getCurrentFactoryPresetIndex() const noexcept
{
    return currentPresetIndices[static_cast<std::size_t>(getSelectedInstrIndex())];
}

void OrchSynthAudioProcessor::applyFactoryPreset(int presetIndex)
{
    const int b = getSelectedInstrIndex();
    const auto& states = factoryPresetStates[static_cast<std::size_t>(b)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(states.size()))
        return;

    applyPresetPersistenceState(states[static_cast<std::size_t>(presetIndex)], false);
    currentPresetIndices[static_cast<std::size_t>(b)] = presetIndex;
    currentUserPresetFiles[static_cast<std::size_t>(b)] = juce::File{};
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
}

bool OrchSynthAudioProcessor::saveFactoryPreset(int presetIndex)
{
    const int b = getSelectedInstrIndex();
    auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return false;

    auto& preset = bank[static_cast<std::size_t>(presetIndex)];
    auto state = captureCurrentPresetState(b);
    state.name = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    state.presetIndex = presetIndex;
    state.metadata = makeFactoryMetadata(preset.metadata);

    preset.settings = state.settings;
    preset.fx = state.fx;
    preset.outputBus = state.outputBus;
    factoryPresetStates[static_cast<std::size_t>(b)][static_cast<std::size_t>(presetIndex)] = state;

    auto dir = getFactoryOverridesDirectory().getChildFile("instr_" + juce::String(b));
    dir.createDirectory();
    auto file = dir.getChildFile(juce::String(presetIndex) + ".xml");

    auto root = createPresetXml("OrchFactoryPreset", state);
    return root->writeTo(file);
}

void OrchSynthAudioProcessor::loadFactoryOverrides()
{
    auto baseDir = getFactoryOverridesDirectory();
    for (int b = 0; b < mos::kNumInstruments; ++b)
    {
        auto dir = baseDir.getChildFile("instr_" + juce::String(b));
        if (!dir.isDirectory()) continue;
        auto& bank = factoryPresetBanks[static_cast<std::size_t>(b)];
        for (int i = 0; i < static_cast<int>(bank.size()); ++i)
        {
            auto file = dir.getChildFile(juce::String(i) + ".xml");
            if (!file.existsAsFile()) continue;
            auto xml = juce::XmlDocument::parse(file);
            if (xml == nullptr)
                continue;

            auto parsed = makeFactoryPresetState(b, i, bank[static_cast<std::size_t>(i)]);
            if (!parsePresetXml(*xml, "OrchFactoryPreset", b, parsed, true, &parsed)
                || parsed.presetIndex != i)
            {
                continue;
            }

            auto& preset = bank[static_cast<std::size_t>(i)];
            preset.settings = parsed.settings;
            preset.fx = parsed.fx;
            preset.outputBus = parsed.outputBus;
            preset.metadata.mixRole = parsed.metadata.mixRole.toStdString();
            preset.metadata.familyLabel = parsed.metadata.family.toStdString();
            preset.metadata.tags = splitTags(parsed.metadata.tags);
            preset.metadata.description = parsed.metadata.description.toStdString();
            preset.metadata.outputProfile = parsed.metadata.outputProfile.toStdString();
            preset.metadata.nominalPeakDb = parsed.metadata.nominalPeakDb;
            factoryPresetStates[static_cast<std::size_t>(b)][static_cast<std::size_t>(i)] = parsed;

            if (shouldRewritePresetXml(*xml, true))
                createPresetXml("OrchFactoryPreset", parsed)->writeTo(file);
        }
    }
}

// =============================================================================
juce::File OrchSynthAudioProcessor::getFactoryOverridesDirectory()
{
    const auto preferred = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                               .getChildFile("MusiqueOrchSynth")
                               .getChildFile("FactoryOverrides");
    return findWritableDirectory(preferred, "MusiqueOrchSynth/FactoryOverrides");
}

juce::File OrchSynthAudioProcessor::getUserPresetsDirectory(int instrIndex)
{
    const auto preferred = musique::preset::nativeUserPresetsDirectoryForSynth(kOrchSynthIndex, instrIndex);
    return findWritableDirectory(preferred,
                                 "MusiqueOrchSynth/Presets/instr_"
                                     + juce::String(juce::jlimit(0, mos::kNumInstruments - 1, instrIndex)));
}

juce::Array<juce::File> OrchSynthAudioProcessor::scanUserPresets() const
{
    juce::Array<juce::File> results;
    auto dir = getUserPresetsDirectory(getSelectedInstrIndex());
    if (dir.isDirectory())
        dir.findChildFiles(results, juce::File::findFiles, false, "*.xml");
    results.sort();
    return results;
}

bool OrchSynthAudioProcessor::writePresetWithManifestRollback(const juce::File& presetFile,
                                                              const juce::XmlElement& presetXml,
                                                              const juce::String& presetName,
                                                              int instrIndex,
                                                              const juce::String& sourceModel) const
{
    presetFile.getParentDirectory().createDirectory();
    const auto manifestFile = musique::preset::manifestFileForPresetFile(presetFile);
    const auto stamp = juce::String(juce::Time::getCurrentTime().toMilliseconds());
    const auto tempPreset = presetFile.getSiblingFile(presetFile.getFileName() + ".tmp_" + stamp);
    const auto tempManifest = manifestFile.getSiblingFile(manifestFile.getFileName() + ".tmp_" + stamp);
    const auto presetBackup = presetFile.getSiblingFile(presetFile.getFileName() + ".bak_" + stamp);
    const auto manifestBackup = manifestFile.getSiblingFile(manifestFile.getFileName() + ".bak_" + stamp);

    auto cleanupTemps = [&]()
    {
        if (tempPreset.existsAsFile())
            tempPreset.deleteFile();
        if (tempManifest.existsAsFile())
            tempManifest.deleteFile();
    };

    auto restoreBackup = [] (const juce::File& backup, const juce::File& target)
    {
        if (target.existsAsFile())
            target.deleteFile();
        if (!backup.existsAsFile())
            return true;
        if (backup.moveFileTo(target))
            return true;
        const auto copied = backup.copyFileTo(target);
        if (copied)
            backup.deleteFile();
        return copied;
    };

    auto replaceTarget = [] (const juce::File& source, const juce::File& target)
    {
        if (target.existsAsFile() && !target.deleteFile())
            return false;
        if (source.moveFileTo(target))
            return true;
        const auto copied = source.copyFileTo(target);
        if (copied)
            source.deleteFile();
        return copied;
    };

    const bool hadPreset = presetFile.existsAsFile();
    const bool hadManifest = manifestFile.existsAsFile();
    if (hadPreset && !presetFile.copyFileTo(presetBackup))
        return false;
    if (hadManifest && !manifestFile.copyFileTo(manifestBackup))
    {
        if (presetBackup.existsAsFile())
            presetBackup.deleteFile();
        return false;
    }

    if (!presetXml.writeTo(tempPreset))
    {
        cleanupTemps();
        restoreBackup(presetBackup, presetFile);
        restoreBackup(manifestBackup, manifestFile);
        return false;
    }

    const auto identity = musique::preset::getSynthIdentity(kOrchSynthIndex);
    if (!identity.isValid())
    {
        cleanupTemps();
        restoreBackup(presetBackup, presetFile);
        restoreBackup(manifestBackup, manifestFile);
        return false;
    }

    musique::preset::PresetManifest manifest;
    manifest.synthId = identity.synthId;
    manifest.synthType = identity.synthType;
    manifest.instrumentIndex = juce::jlimit(0, mos::kNumInstruments - 1, instrIndex);
    manifest.instrumentName = utf8Text(mos::getInstrName(manifest.instrumentIndex));
    manifest.presetName = presetName;
    manifest.xmlRootTag = identity.xmlRootTag;
    manifest.sourceModel = sourceModel;
    manifest.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    manifest.sourcePath = presetFile.getFullPathName();
    manifest.validationVersion = 1;
    if (!musique::preset::saveManifestToFile(tempManifest, manifest))
    {
        cleanupTemps();
        restoreBackup(presetBackup, presetFile);
        restoreBackup(manifestBackup, manifestFile);
        return false;
    }

    if (!replaceTarget(tempPreset, presetFile))
    {
        cleanupTemps();
        restoreBackup(presetBackup, presetFile);
        restoreBackup(manifestBackup, manifestFile);
        return false;
    }

    if (!replaceTarget(tempManifest, manifestFile))
    {
        restoreBackup(presetBackup, presetFile);
        restoreBackup(manifestBackup, manifestFile);
        cleanupTemps();
        return false;
    }

    if (presetBackup.existsAsFile())
        presetBackup.deleteFile();
    if (manifestBackup.existsAsFile())
        manifestBackup.deleteFile();
    cleanupTemps();
    return true;
}

bool OrchSynthAudioProcessor::writePresetManifest(const juce::File& presetFile,
                                                  const juce::String& presetName,
                                                  int instrIndex,
                                                  const juce::String& sourceModel) const
{
    const auto identity = musique::preset::getSynthIdentity(4);
    if (!identity.isValid())
        return false;

    musique::preset::PresetManifest manifest;
    manifest.synthId = identity.synthId;
    manifest.synthType = identity.synthType;
    manifest.instrumentIndex = juce::jlimit(0, mos::kNumInstruments - 1, instrIndex);
    manifest.instrumentName = utf8Text(mos::getInstrName(manifest.instrumentIndex));
    manifest.presetName = presetName;
    manifest.xmlRootTag = identity.xmlRootTag;
    manifest.sourceModel = sourceModel;
    manifest.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    manifest.sourcePath = presetFile.getFullPathName();
    manifest.validationVersion = 1;

    return musique::preset::saveManifestToFile(
        musique::preset::manifestFileForPresetFile(presetFile), manifest);
}

void OrchSynthAudioProcessor::backfillUserPresetLibrary(int instrIndex) const
{
    if (instrIndex < 0 || instrIndex >= mos::kNumInstruments)
        return;

    const auto dir = getUserPresetsDirectory(instrIndex);
    if (!dir.isDirectory())
        return;

    juce::Array<juce::File> presetFiles;
    dir.findChildFiles(presetFiles, juce::File::findFiles, false, "*.xml");
    for (const auto& presetFile : presetFiles)
    {
        auto xml = juce::XmlDocument::parse(presetFile);
        if (xml == nullptr || !xml->hasTagName("OrchPreset"))
        {
            juce::Logger::writeToLog("[OrchPreset] ignoring invalid preset during manifest backfill: "
                                     + presetFile.getFileName());
            continue;
        }

        const int resolvedInstr = readInstrumentIndexAttribute(*xml, instrIndex);
        auto parsed = makeDefaultPresetState(resolvedInstr);
        parsed.name = presetFile.getFileNameWithoutExtension();
        if (!parsePresetXml(*xml, "OrchPreset", resolvedInstr, parsed, false, &parsed))
        {
            juce::Logger::writeToLog("[OrchPreset] ignoring unsupported preset payload during backfill: "
                                     + presetFile.getFileName());
            continue;
        }

        musique::preset::PresetManifest manifest;
        const auto manifestFile = musique::preset::manifestFileForPresetFile(presetFile);
        const bool manifestValid = musique::preset::loadManifestFromFile(manifestFile, manifest)
            && manifest.synthType == "orch"
            && manifest.instrumentIndex == resolvedInstr
            && manifest.xmlRootTag == "OrchPreset";
        const bool needsRewrite = shouldRewritePresetXml(*xml, false);
        const auto presetName = parsed.name.isNotEmpty() ? parsed.name : presetFile.getFileNameWithoutExtension();

        if (needsRewrite)
        {
            auto normalizedXml = createPresetXml("OrchPreset", parsed);
            if (!writePresetWithManifestRollback(presetFile,
                                                *normalizedXml,
                                                presetName,
                                                resolvedInstr,
                                                utf8Text(mos::getInstrName(resolvedInstr))))
            {
                juce::Logger::writeToLog("[OrchPreset] failed to rewrite canonical preset during backfill: "
                                         + presetFile.getFileName());
            }
            continue;
        }

        if (!manifestValid
            && !writePresetManifest(presetFile, presetName, resolvedInstr, utf8Text(mos::getInstrName(resolvedInstr))))
        {
            juce::Logger::writeToLog("[OrchPreset] failed to backfill manifest for "
                                     + presetFile.getFileName());
        }
    }
}

bool OrchSynthAudioProcessor::saveUserPreset(const juce::String& name)
{
    if (name.isEmpty()) return false;
    const int b = getSelectedInstrIndex();
    auto file = getUserPresetsDirectory(b).getChildFile(
        juce::File::createLegalFileName(name) + ".xml");
    auto state = captureCurrentPresetState(b);
    state.name = name;
    auto root = createPresetXml("OrchPreset", state);

    if (writePresetWithManifestRollback(file, *root, name, b, utf8Text(mos::getInstrName(b))))
    {
        currentUserPresetFiles[static_cast<std::size_t>(b)] = file;
        currentPresetIndices[static_cast<std::size_t>(b)] = -1;
        return true;
    }
    return false;
}

bool OrchSynthAudioProcessor::updateUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int b = getSelectedInstrIndex();
    auto state = captureCurrentPresetState(b);
    state.name = file.getFileNameWithoutExtension();
    auto root = createPresetXml("OrchPreset", state);

    if (writePresetWithManifestRollback(file, *root, file.getFileNameWithoutExtension(), b,
                                        utf8Text(mos::getInstrName(b))))
    {
        currentUserPresetFiles[static_cast<std::size_t>(b)] = file;
        currentPresetIndices[static_cast<std::size_t>(b)] = -1;
        return true;
    }
    return false;
}

bool OrchSynthAudioProcessor::deleteUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int b = getSelectedInstrIndex();
    if (currentUserPresetFiles[static_cast<std::size_t>(b)] == file)
        currentUserPresetFiles[static_cast<std::size_t>(b)] = juce::File{};
    musique::preset::manifestFileForPresetFile(file).deleteFile();
    return file.deleteFile();
}

bool OrchSynthAudioProcessor::loadUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) return false;

    const int b = getSelectedInstrIndex();
    auto parsed = makeDefaultPresetState(b);
    parsed.name = file.getFileNameWithoutExtension();
    if (!parsePresetXml(*xml, "OrchPreset", b, parsed, false, &parsed))
        return false;

    applyPresetPersistenceState(parsed, false);

    currentUserPresetFiles[static_cast<std::size_t>(b)] = file;
    currentPresetIndices[static_cast<std::size_t>(b)] = -1;
    const auto presetName = parsed.name.isNotEmpty() ? parsed.name : file.getFileNameWithoutExtension();
    if (shouldRewritePresetXml(*xml, false))
    {
        auto normalizedXml = createPresetXml("OrchPreset", parsed);
        writePresetWithManifestRollback(file, *normalizedXml, presetName, b, utf8Text(mos::getInstrName(b)));
    }
    else
    {
        writePresetManifest(file, presetName, b, utf8Text(mos::getInstrName(b)));
    }
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
    return true;
}

bool OrchSynthAudioProcessor::isCurrentPresetUser() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedInstrIndex())].existsAsFile();
}

juce::File OrchSynthAudioProcessor::getCurrentUserPresetFile() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedInstrIndex())];
}

// =============================================================================
int OrchSynthAudioProcessor::getSelectedInstrIndex() const
{
    return juce::jlimit(0, mos::kNumInstruments - 1,
                        static_cast<int>(std::round(getParamValue(kSelectedInstr))));
}

OrchSynthAudioProcessor::QualityMode OrchSynthAudioProcessor::getQualityMode() const noexcept
{
    return getParamValue(kQualityMode) >= 0.5f ? QualityMode::Studio : QualityMode::Live;
}

bool OrchSynthAudioProcessor::shouldStopNotesOnKeyRelease() const noexcept
{
    return stopNotesOnKeyRelease.load(std::memory_order_relaxed);
}

void OrchSynthAudioProcessor::setStopNotesOnKeyRelease(bool shouldStop) noexcept
{
    const bool previous = stopNotesOnKeyRelease.exchange(shouldStop, std::memory_order_relaxed);

    if (!previous && shouldStop)
    {
        for (auto& slot : voices)
        {
            if (slot.active != nullptr
                && slot.active->isActive()
                && !slot.active->isReleasing()
                && slot.heldAfterKeyRelease)
            {
                const auto releaseSeconds = releaseSecondsForNoteOff(slot.instrIndex, slot.midiChannel);
                slot.activeTailHintSeconds = juce::jmax(0.05f, releaseSeconds * 1.5f);
                slot.active->setReleaseTimeSeconds(releaseSeconds);
                slot.active->noteOff();
                slot.heldAfterKeyRelease = false;
            }
        }
    }

    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withParameterInfoChanged(true));
}

bool OrchSynthAudioProcessor::isDelaySyncEnabled() const noexcept
{
    return getParamValue(kDelaySync) >= 0.5f;
}

int OrchSynthAudioProcessor::getDelayDivisionIndex() const noexcept
{
    return juce::jlimit(0, 5, static_cast<int>(std::round(getParamValue(kDelayDivision))));
}

float OrchSynthAudioProcessor::getLastKnownHostTempoBpm() const noexcept
{
    return lastKnownHostTempoBpm.load(std::memory_order_relaxed);
}

float OrchSynthAudioProcessor::getMainMeterLevel(int channel) const noexcept
{
    const auto index = static_cast<std::size_t>(juce::jlimit(0, 1, channel));
    return mainMeterLevels[index].load(std::memory_order_relaxed);
}

float OrchSynthAudioProcessor::getAuxMeterLevel(int auxIndex) const noexcept
{
    const auto index = static_cast<std::size_t>(juce::jlimit(0, kNumAuxOutputs - 1, auxIndex));
    return auxMeterLevels[index].load(std::memory_order_relaxed);
}

bool OrchSynthAudioProcessor::isClipLatched() const noexcept
{
    return clipLatched.load(std::memory_order_relaxed);
}

void OrchSynthAudioProcessor::clearClipLatch() noexcept
{
    clipLatched.store(false, std::memory_order_relaxed);
}

int OrchSynthAudioProcessor::getActiveVoiceCount() const noexcept
{
    return displayedVoiceCount.load(std::memory_order_relaxed);
}

bool OrchSynthAudioProcessor::isFxAvailableForCurrentInstr(mos::GlobalFxSlot slot) const
{
    return mos::isFxAvailable(juce::jlimit(0, mos::kNumInstruments - 1, currentFxOwnerInstr.load()), slot);
}

const mos::InstrumentPreset* OrchSynthAudioProcessor::getFactoryPresetDefinition(int presetIndex) const noexcept
{
    const int selectedInstr = getSelectedInstrIndex();
    if (selectedInstr < 0 || selectedInstr >= mos::kNumInstruments)
        return nullptr;

    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(selectedInstr)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return nullptr;

    return &bank[static_cast<std::size_t>(presetIndex)];
}

double OrchSynthAudioProcessor::readHostTempoBpm() const
{
    if (const auto* currentPlayHead = getPlayHead())
    {
        if (auto position = currentPlayHead->getPosition())
        {
            if (const auto bpm = position->getBpm())
                return juce::jlimit(20.0, 320.0, *bpm);
        }
    }

    const auto cached = lastKnownHostTempoBpm.load(std::memory_order_relaxed);
    return juce::jlimit(20.0, 320.0, cached > 0.0f ? static_cast<double>(cached) : 120.0);
}

float OrchSynthAudioProcessor::getParamValue(const juce::String& paramId) const
{
    if (const auto* raw = parameters.getRawParameterValue(paramId))
        return raw->load();
    return safeDefaultForMissingParam(paramId);
}

float OrchSynthAudioProcessor::sanitizeParameterValue(const juce::String& paramId,
                                                      float value,
                                                      float fallback,
                                                      int* warningCount) const
{
    if (!std::isfinite(value))
    {
        if (warningCount != nullptr)
            ++(*warningCount);
        value = fallback;
    }

    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId)))
    {
        const auto normalised = juce::jlimit(0.0f, 1.0f, parameter->convertTo0to1(value));
        const auto sanitized = parameter->convertFrom0to1(normalised);
        if (warningCount != nullptr && std::abs(sanitized - value) > 1.0e-4f)
            ++(*warningCount);
        return sanitized;
    }

    return fallback;
}

void OrchSynthAudioProcessor::setParamValueInternal(const juce::String& paramId, float value, bool notifyHost)
{
    if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId)))
        setRangedParameterValue(parameter, value, notifyHost);
}

void OrchSynthAudioProcessor::setParamValue(const juce::String& paramId, float value)
{
    setParamValueInternal(paramId, value, true);
}

void OrchSynthAudioProcessor::queueParamUpdate(juce::RangedAudioParameter* parameter, float normalisedValue)
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

mos::InstrSettings OrchSynthAudioProcessor::sanitizeInstrSettings(int instrIndex,
                                                                  const mos::InstrSettings& settings) const
{
    auto sanitized = settings;
    const auto defaults = mos::getDefaultSettings(instrIndex);
    sanitized.level = sanitizeParameterValue(makeInstrParamId(instrIndex, "level"), sanitized.level, defaults.level);
    sanitized.tuneSemitones = sanitizeParameterValue(makeInstrParamId(instrIndex, "tune"), sanitized.tuneSemitones, defaults.tuneSemitones);
    sanitized.brightness = sanitizeParameterValue(makeInstrParamId(instrIndex, "brightness"), sanitized.brightness, defaults.brightness);
    sanitized.attackSeconds = sanitizeParameterValue(makeInstrParamId(instrIndex, "attack"), sanitized.attackSeconds, defaults.attackSeconds);
    sanitized.decaySeconds = sanitizeParameterValue(makeInstrParamId(instrIndex, "decay"), sanitized.decaySeconds, defaults.decaySeconds);
    sanitized.sustainLevel = sanitizeParameterValue(makeInstrParamId(instrIndex, "sustain"), sanitized.sustainLevel, defaults.sustainLevel);
    sanitized.releaseSeconds = sanitizeParameterValue(makeInstrParamId(instrIndex, "release"), sanitized.releaseSeconds, defaults.releaseSeconds);
    sanitized.vibrato = sanitizeParameterValue(makeInstrParamId(instrIndex, "vibrato"), sanitized.vibrato, defaults.vibrato);
    sanitized.warmth = sanitizeParameterValue(makeInstrParamId(instrIndex, "warmth"), sanitized.warmth, defaults.warmth);
    sanitized.detune = sanitizeParameterValue(makeInstrParamId(instrIndex, "detune"), sanitized.detune, defaults.detune);
    sanitized.stereoWidth = sanitizeParameterValue(makeInstrParamId(instrIndex, "stereo_width"), sanitized.stereoWidth, defaults.stereoWidth);
    sanitized.character = sanitizeParameterValue(makeInstrParamId(instrIndex, "character"), sanitized.character, defaults.character);
    sanitized.cutoffHz = sanitizeParameterValue(makeInstrParamId(instrIndex, "cutoff"), sanitized.cutoffHz, defaults.cutoffHz);
    sanitized.pan = sanitizeParameterValue(makeInstrParamId(instrIndex, "pan"), sanitized.pan, defaults.pan);

    if (instrIndex == 13)
    {
        sanitized.vibrato = 0.0f;
        sanitized.detune = 0.0f;
        sanitized.stereoWidth = juce::jmin(sanitized.stereoWidth, 0.10f);
        sanitized.sustainLevel = juce::jmin(sanitized.sustainLevel, 0.92f);
    }
    else if (instrIndex == 14)
    {
        sanitized.vibrato = 0.0f;
        sanitized.detune = 0.0f;
        sanitized.stereoWidth = juce::jmin(sanitized.stereoWidth, 0.12f);
        sanitized.sustainLevel = juce::jmin(sanitized.sustainLevel, 0.90f);
    }
    else if (instrIndex == 3 || instrIndex == 15)
    {
        sanitized.detune = juce::jmin(sanitized.detune, 0.04f);
        sanitized.stereoWidth = juce::jmin(sanitized.stereoWidth, 0.14f);
    }

    return sanitized;
}

mos::GlobalFxSettings OrchSynthAudioProcessor::sanitizeFxSettings(const mos::GlobalFxSettings& fx) const
{
    auto sanitized = fx;
    const mos::GlobalFxSettings defaults {};
    const auto finiteOr = [](float value, float fallback)
    {
        return std::isfinite(value) ? value : fallback;
    };

    sanitized.satDrive = juce::jlimit(1.0f, 16.0f, finiteOr(sanitized.satDrive, defaults.satDrive));
    sanitized.satMix = clamp01(finiteOr(sanitized.satMix, defaults.satMix));
    sanitized.transientAttack = juce::jlimit(-1.0f, 1.0f, finiteOr(sanitized.transientAttack, defaults.transientAttack));
    sanitized.transientSustain = juce::jlimit(-1.0f, 1.0f, finiteOr(sanitized.transientSustain, defaults.transientSustain));
    sanitized.transientMix = clamp01(finiteOr(sanitized.transientMix, defaults.transientMix));
    sanitized.eqLowFreq = juce::jlimit(40.0f, 600.0f, finiteOr(sanitized.eqLowFreq, defaults.eqLowFreq));
    sanitized.eqLowGain = juce::jlimit(-12.0f, 12.0f, finiteOr(sanitized.eqLowGain, defaults.eqLowGain));
    sanitized.eqMidFreq = juce::jlimit(200.0f, 8000.0f, finiteOr(sanitized.eqMidFreq, defaults.eqMidFreq));
    sanitized.eqMidGain = juce::jlimit(-12.0f, 12.0f, finiteOr(sanitized.eqMidGain, defaults.eqMidGain));
    sanitized.eqMidQ = juce::jlimit(0.1f, 10.0f, finiteOr(sanitized.eqMidQ, defaults.eqMidQ));
    sanitized.eqHighFreq = juce::jlimit(1000.0f, 16000.0f, finiteOr(sanitized.eqHighFreq, defaults.eqHighFreq));
    sanitized.eqHighGain = juce::jlimit(-12.0f, 12.0f, finiteOr(sanitized.eqHighGain, defaults.eqHighGain));
    sanitized.compThreshold = juce::jlimit(-60.0f, 0.0f, finiteOr(sanitized.compThreshold, defaults.compThreshold));
    sanitized.compRatio = juce::jlimit(1.0f, 20.0f, finiteOr(sanitized.compRatio, defaults.compRatio));
    sanitized.compAttack = juce::jlimit(0.1f, 100.0f, finiteOr(sanitized.compAttack, defaults.compAttack));
    sanitized.compRelease = juce::jlimit(5.0f, 500.0f, finiteOr(sanitized.compRelease, defaults.compRelease));
    sanitized.compMix = clamp01(finiteOr(sanitized.compMix, defaults.compMix));
    sanitized.chorusRate = juce::jlimit(0.1f, 5.0f, finiteOr(sanitized.chorusRate, defaults.chorusRate));
    sanitized.chorusDepth = clamp01(finiteOr(sanitized.chorusDepth, defaults.chorusDepth));
    sanitized.chorusMix = clamp01(finiteOr(sanitized.chorusMix, defaults.chorusMix));
    sanitized.delayTime = juce::jlimit(1.0f, 2000.0f, finiteOr(sanitized.delayTime, defaults.delayTime));
    sanitized.delayFeedback = juce::jlimit(0.0f, 0.95f, finiteOr(sanitized.delayFeedback, defaults.delayFeedback));
    sanitized.delayMix = clamp01(finiteOr(sanitized.delayMix, defaults.delayMix));
    sanitized.reverbSize = clamp01(finiteOr(sanitized.reverbSize, defaults.reverbSize));
    sanitized.reverbDamping = clamp01(finiteOr(sanitized.reverbDamping, defaults.reverbDamping));
    sanitized.reverbWidth = clamp01(finiteOr(sanitized.reverbWidth, defaults.reverbWidth));
    sanitized.reverbMix = clamp01(finiteOr(sanitized.reverbMix, defaults.reverbMix));
    sanitized.reverbPredelay = juce::jlimit(0.0f, 100.0f, finiteOr(sanitized.reverbPredelay, defaults.reverbPredelay));
    sanitized.reverbType = juce::jlimit(0, 1, sanitized.reverbType);
    sanitized.limiterThreshold = juce::jlimit(-12.0f, 0.0f, finiteOr(sanitized.limiterThreshold, defaults.limiterThreshold));
    sanitized.limiterRelease = juce::jlimit(1.0f, 500.0f, finiteOr(sanitized.limiterRelease, defaults.limiterRelease));
    return sanitized;
}

void OrchSynthAudioProcessor::sanitizeAllParameters()
{
    setParamValueInternal(kSelectedInstr, sanitizeParameterValue(kSelectedInstr, getParamValue(kSelectedInstr), 0.0f), false);
    setParamValueInternal(kOutputGain, sanitizeParameterValue(kOutputGain, getParamValue(kOutputGain), -3.0f), false);
    setParamValueInternal(kQualityMode, sanitizeParameterValue(kQualityMode, getParamValue(kQualityMode), 0.0f), false);
    setParamValueInternal(kDelaySync, sanitizeParameterValue(kDelaySync, getParamValue(kDelaySync), 0.0f), false);
    setParamValueInternal(kDelayDivision, sanitizeParameterValue(kDelayDivision, getParamValue(kDelayDivision), 1.0f), false);
    setParamValueInternal(kLfoRate, sanitizeParameterValue(kLfoRate, getParamValue(kLfoRate), 1.1f), false);
    setParamValueInternal(kLfoDepth, sanitizeParameterValue(kLfoDepth, getParamValue(kLfoDepth), 0.0f), false);
    setParamValueInternal(kLfoWave, sanitizeParameterValue(kLfoWave, getParamValue(kLfoWave), 0.0f), false);
    setParamValueInternal(kVelocityCurve, sanitizeParameterValue(kVelocityCurve, getParamValue(kVelocityCurve), 0.0f), false);
    setParamValueInternal(kPortamentoSeconds, sanitizeParameterValue(kPortamentoSeconds, getParamValue(kPortamentoSeconds), 0.0f), false);
    setParamValueInternal(kLegatoAmount, sanitizeParameterValue(kLegatoAmount, getParamValue(kLegatoAmount), 1.0f), false);
    setParamValueInternal(kRoundRobinAmount, sanitizeParameterValue(kRoundRobinAmount, getParamValue(kRoundRobinAmount), 0.5f), false);
    setParamValueInternal(kMacroWarmth, sanitizeParameterValue(kMacroWarmth, getParamValue(kMacroWarmth), 0.5f), false);
    setParamValueInternal(kMacroBrillance, sanitizeParameterValue(kMacroBrillance, getParamValue(kMacroBrillance), 0.5f), false);
    setParamValueInternal(kMacroSpace, sanitizeParameterValue(kMacroSpace, getParamValue(kMacroSpace), 0.5f), false);
    setParamValueInternal(kMacroExpression, sanitizeParameterValue(kMacroExpression, getParamValue(kMacroExpression), 0.5f), false);
    for (int slotIndex = 0; slotIndex < kNumModMatrixSlots; ++slotIndex)
    {
        setParamValueInternal(makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix),
                              sanitizeParameterValue(makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix),
                                                     getParamValue(makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix)),
                                                     0.0f),
                              false);
        setParamValueInternal(makeModMatrixParamId(slotIndex, kModMatrixDestSuffix),
                              sanitizeParameterValue(makeModMatrixParamId(slotIndex, kModMatrixDestSuffix),
                                                     getParamValue(makeModMatrixParamId(slotIndex, kModMatrixDestSuffix)),
                                                     0.0f),
                              false);
        setParamValueInternal(makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix),
                              sanitizeParameterValue(makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix),
                                                     getParamValue(makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix)),
                                                     0.0f),
                              false);
    }

    applyFxSettingsToParams(snapshotFx(), false);

    setParamValueInternal("fx_tab0_en", sanitizeParameterValue("fx_tab0_en", getParamValue("fx_tab0_en"), 1.0f), false);
    setParamValueInternal("fx_tab1_en", sanitizeParameterValue("fx_tab1_en", getParamValue("fx_tab1_en"), 1.0f), false);
    setParamValueInternal("fx_tab2_en", sanitizeParameterValue("fx_tab2_en", getParamValue("fx_tab2_en"), 1.0f), false);
    setParamValueInternal("fx_tab3_en", sanitizeParameterValue("fx_tab3_en", getParamValue("fx_tab3_en"), 1.0f), false);
    setParamValueInternal("fx_eq_en", sanitizeParameterValue("fx_eq_en", getParamValue("fx_eq_en"), 1.0f), false);
    setParamValueInternal("fx_chorus_en", sanitizeParameterValue("fx_chorus_en", getParamValue("fx_chorus_en"), 1.0f), false);
    setParamValueInternal("fx_delay_en", sanitizeParameterValue("fx_delay_en", getParamValue("fx_delay_en"), 1.0f), false);
    setParamValueInternal("fx_limiter_en", sanitizeParameterValue("fx_limiter_en", getParamValue("fx_limiter_en"), 1.0f), false);
    setParamValueInternal("fx_lock", sanitizeParameterValue("fx_lock", getParamValue("fx_lock"), 0.0f), false);

    for (int instrIndex = 0; instrIndex < mos::kNumInstruments; ++instrIndex)
    {
        mos::InstrSettings rawSettings;
        rawSettings.level = getParamValue(makeInstrParamId(instrIndex, "level"));
        rawSettings.tuneSemitones = getParamValue(makeInstrParamId(instrIndex, "tune"));
        rawSettings.brightness = getParamValue(makeInstrParamId(instrIndex, "brightness"));
        rawSettings.attackSeconds = getParamValue(makeInstrParamId(instrIndex, "attack"));
        rawSettings.decaySeconds = getParamValue(makeInstrParamId(instrIndex, "decay"));
        rawSettings.sustainLevel = getParamValue(makeInstrParamId(instrIndex, "sustain"));
        rawSettings.releaseSeconds = getParamValue(makeInstrParamId(instrIndex, "release"));
        rawSettings.vibrato = getParamValue(makeInstrParamId(instrIndex, "vibrato"));
        rawSettings.warmth = getParamValue(makeInstrParamId(instrIndex, "warmth"));
        rawSettings.detune = getParamValue(makeInstrParamId(instrIndex, "detune"));
        rawSettings.stereoWidth = getParamValue(makeInstrParamId(instrIndex, "stereo_width"));
        rawSettings.character = getParamValue(makeInstrParamId(instrIndex, "character"));
        rawSettings.cutoffHz = getParamValue(makeInstrParamId(instrIndex, "cutoff"));
        rawSettings.pan = getParamValue(makeInstrParamId(instrIndex, "pan"));
        const auto sanitized = sanitizeInstrSettings(instrIndex, rawSettings);
        setParamValueInternal(makeInstrParamId(instrIndex, "level"), sanitized.level, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "tune"), sanitized.tuneSemitones, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "brightness"), sanitized.brightness, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "attack"), sanitized.attackSeconds, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "decay"), sanitized.decaySeconds, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "sustain"), sanitized.sustainLevel, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "release"), sanitized.releaseSeconds, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "vibrato"), sanitized.vibrato, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "warmth"), sanitized.warmth, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "detune"), sanitized.detune, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "stereo_width"), sanitized.stereoWidth, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "character"), sanitized.character, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "cutoff"), sanitized.cutoffHz, false);
        setParamValueInternal(makeInstrParamId(instrIndex, "pan"), sanitized.pan, false);
        setParamValueInternal(makeInstrParamId(instrIndex, kInstrOutputSuffix),
                              sanitizeParameterValue(makeInstrParamId(instrIndex, kInstrOutputSuffix),
                                                     getParamValue(makeInstrParamId(instrIndex, kInstrOutputSuffix)),
                                                     0.0f),
                              false);
    }
}

mos::InstrSettings OrchSynthAudioProcessor::captureBaseInstrSettings(int instrIndex) const
{
    mos::InstrSettings s;
    s.level          = getParamValue(makeInstrParamId(instrIndex, "level"));
    s.tuneSemitones  = getParamValue(makeInstrParamId(instrIndex, "tune"));
    s.brightness     = getParamValue(makeInstrParamId(instrIndex, "brightness"));
    s.attackSeconds  = getParamValue(makeInstrParamId(instrIndex, "attack"));
    s.decaySeconds   = getParamValue(makeInstrParamId(instrIndex, "decay"));
    s.sustainLevel   = getParamValue(makeInstrParamId(instrIndex, "sustain"));
    s.releaseSeconds = getParamValue(makeInstrParamId(instrIndex, "release"));
    s.vibrato        = getParamValue(makeInstrParamId(instrIndex, "vibrato"));
    s.warmth         = getParamValue(makeInstrParamId(instrIndex, "warmth"));
    s.detune         = getParamValue(makeInstrParamId(instrIndex, "detune"));
    s.stereoWidth    = getParamValue(makeInstrParamId(instrIndex, "stereo_width"));
    s.character      = getParamValue(makeInstrParamId(instrIndex, "character"));
    s.cutoffHz       = getParamValue(makeInstrParamId(instrIndex, "cutoff"));
    s.pan            = getParamValue(makeInstrParamId(instrIndex, "pan"));

    return sanitizeInstrSettings(instrIndex, s);
}

mos::InstrSettings OrchSynthAudioProcessor::snapshotInstrSettings(int instrIndex) const
{
    auto s = captureBaseInstrSettings(instrIndex);
    applyPerformanceMacros(instrIndex, s);
    return s;
}

OrchSynthAudioProcessor::PresetPersistenceState
OrchSynthAudioProcessor::captureCurrentPresetState(int instrIndex) const
{
    auto state = makeDefaultPresetState(instrIndex);
    state.instrIndex = juce::jlimit(0, mos::kNumInstruments - 1, instrIndex);
    state.settings = captureBaseInstrSettings(state.instrIndex);
    state.outputBus = juce::jlimit(
        0, kNumAuxOutputs,
        static_cast<int>(std::round(getParamValue(makeInstrParamId(state.instrIndex, kInstrOutputSuffix)))));

    const int currentOwner = juce::jlimit(0, mos::kNumInstruments - 1, currentFxOwnerInstr.load());
    state.fx = sanitizeFxSettings(currentOwner == state.instrIndex
        ? snapshotFx()
        : cachedFxPerInstr[static_cast<std::size_t>(state.instrIndex)]);

    state.qualityMode = juce::jlimit(0, 1, static_cast<int>(std::round(getParamValue(kQualityMode))));
    state.delaySync = juce::jlimit(0, 1, static_cast<int>(std::round(getParamValue(kDelaySync))));
    state.delayDivision = juce::jlimit(0, 5, static_cast<int>(std::round(getParamValue(kDelayDivision))));
    state.lfoRate = juce::jlimit(0.05f, 12.0f, getParamValue(kLfoRate));
    state.lfoDepth = juce::jlimit(0.0f, 1.0f, getParamValue(kLfoDepth));
    state.lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(getParamValue(kLfoWave))));
    state.velocityCurve = juce::jlimit(0, 6, static_cast<int>(std::round(getParamValue(kVelocityCurve))));
    state.portamentoSeconds = juce::jlimit(0.0f, 2.0f, getParamValue(kPortamentoSeconds));
    state.legatoAmount = clamp01(getParamValue(kLegatoAmount));
    state.roundRobinAmount = clamp01(getParamValue(kRoundRobinAmount));
    state.macroWarmth = clamp01(getParamValue(kMacroWarmth));
    state.macroBrillance = clamp01(getParamValue(kMacroBrillance));
    state.macroSpace = clamp01(getParamValue(kMacroSpace));
    state.macroExpression = clamp01(getParamValue(kMacroExpression));
    state.fxEnables[0] = getParamValue("fx_tab0_en") >= 0.5f;
    state.fxEnables[1] = getParamValue("fx_tab1_en") >= 0.5f;
    state.fxEnables[2] = getParamValue("fx_tab2_en") >= 0.5f;
    state.fxEnables[3] = getParamValue("fx_tab3_en") >= 0.5f;
    state.fxEnables[4] = getParamValue("fx_eq_en") >= 0.5f;
    state.fxEnables[5] = getParamValue("fx_chorus_en") >= 0.5f;
    state.fxEnables[6] = getParamValue("fx_delay_en") >= 0.5f;
    state.fxEnables[7] = getParamValue("fx_limiter_en") >= 0.5f;
    state.fxLock = getParamValue("fx_lock") >= 0.5f;

    for (int slotIndex = 0; slotIndex < kNumModMatrixSlots; ++slotIndex)
    {
        state.modSources[static_cast<std::size_t>(slotIndex)] = juce::jlimit(
            0, 8,
            static_cast<int>(std::round(getParamValue(makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix)))));
        state.modDestinations[static_cast<std::size_t>(slotIndex)] = juce::jlimit(
            0, 12,
            static_cast<int>(std::round(getParamValue(makeModMatrixParamId(slotIndex, kModMatrixDestSuffix)))));
        state.modAmounts[static_cast<std::size_t>(slotIndex)] = juce::jlimit(
            -1.0f, 1.0f, getParamValue(makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix)));
    }

    state.metadata = makeUserMetadata(state.instrIndex);
    return state;
}

OrchSynthAudioProcessor::PresetPersistenceState
OrchSynthAudioProcessor::makeFactoryPresetState(int instrIndex,
                                                int presetIndex,
                                                const mos::InstrumentPreset& preset) const
{
    auto state = makeDefaultPresetState(instrIndex);
    state.name = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    state.instrIndex = juce::jlimit(0, mos::kNumInstruments - 1, instrIndex);
    state.presetIndex = presetIndex;
    state.settings = sanitizeInstrSettings(state.instrIndex, preset.settings);
    state.fx = sanitizeFxSettings(preset.fx);
    state.outputBus = juce::jlimit(0, kNumAuxOutputs, preset.outputBus);
    state.metadata = makeFactoryMetadata(preset.metadata);
    return state;
}

void OrchSynthAudioProcessor::applyPresetPersistenceState(const PresetPersistenceState& state,
                                                          bool notifyHost)
{
    const auto instrIndex = juce::jlimit(0, mos::kNumInstruments - 1, state.instrIndex);
    applyInstrPresetSettings(instrIndex, state.settings);
    setParamValueInternal(makeInstrParamId(instrIndex, kInstrOutputSuffix),
                          static_cast<float>(juce::jlimit(0, kNumAuxOutputs, state.outputBus)),
                          notifyHost);
    setParamValueInternal(kQualityMode, static_cast<float>(juce::jlimit(0, 1, state.qualityMode)), notifyHost);
    setParamValueInternal(kDelaySync, static_cast<float>(juce::jlimit(0, 1, state.delaySync)), notifyHost);
    setParamValueInternal(kDelayDivision, static_cast<float>(juce::jlimit(0, 5, state.delayDivision)), notifyHost);
    setParamValueInternal(kLfoRate, juce::jlimit(0.05f, 12.0f, state.lfoRate), notifyHost);
    setParamValueInternal(kLfoDepth, juce::jlimit(0.0f, 1.0f, state.lfoDepth), notifyHost);
    setParamValueInternal(kLfoWave, static_cast<float>(juce::jlimit(0, 3, state.lfoWave)), notifyHost);
    setParamValueInternal(kVelocityCurve, static_cast<float>(juce::jlimit(0, 6, state.velocityCurve)), notifyHost);
    setParamValueInternal(kPortamentoSeconds, juce::jlimit(0.0f, 2.0f, state.portamentoSeconds), notifyHost);
    setParamValueInternal(kLegatoAmount, clamp01(state.legatoAmount), notifyHost);
    setParamValueInternal(kRoundRobinAmount, clamp01(state.roundRobinAmount), notifyHost);
    setParamValueInternal(kMacroWarmth, clamp01(state.macroWarmth), notifyHost);
    setParamValueInternal(kMacroBrillance, clamp01(state.macroBrillance), notifyHost);
    setParamValueInternal(kMacroSpace, clamp01(state.macroSpace), notifyHost);
    setParamValueInternal(kMacroExpression, clamp01(state.macroExpression), notifyHost);
    setParamValueInternal("fx_tab0_en", state.fxEnables[0] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab1_en", state.fxEnables[1] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab2_en", state.fxEnables[2] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_tab3_en", state.fxEnables[3] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_eq_en", state.fxEnables[4] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_chorus_en", state.fxEnables[5] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_delay_en", state.fxEnables[6] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_limiter_en", state.fxEnables[7] ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal("fx_lock", state.fxLock ? 1.0f : 0.0f, notifyHost);

    for (int slotIndex = 0; slotIndex < kNumModMatrixSlots; ++slotIndex)
    {
        setParamValueInternal(makeModMatrixParamId(slotIndex, kModMatrixSourceSuffix),
                              static_cast<float>(juce::jlimit(0, 8, state.modSources[static_cast<std::size_t>(slotIndex)])),
                              notifyHost);
        setParamValueInternal(makeModMatrixParamId(slotIndex, kModMatrixDestSuffix),
                              static_cast<float>(juce::jlimit(0, 12, state.modDestinations[static_cast<std::size_t>(slotIndex)])),
                              notifyHost);
        setParamValueInternal(makeModMatrixParamId(slotIndex, kModMatrixAmountSuffix),
                              juce::jlimit(-1.0f, 1.0f, state.modAmounts[static_cast<std::size_t>(slotIndex)]),
                              notifyHost);
    }

    const auto sanitizedFx = sanitizeFxSettings(state.fx);
    cachedFxPerInstr[static_cast<std::size_t>(instrIndex)] = sanitizedFx;
    applyFxSettingsToParams(sanitizedFx, notifyHost);
    sanitizeAllParameters();
    pendingFxRecallInstrIndex.store(-1);
    currentFxOwnerInstr.store(instrIndex);
    cachedSelectedInstrIndex = instrIndex;
    pendingSelectedInstrIndex.store(instrIndex);
    velocityCurve = intToVelocityCurve(static_cast<int>(std::round(getParamValue(kVelocityCurve))));
}

void OrchSynthAudioProcessor::applyPerformanceMacros(int instrIndex, mos::InstrSettings& s) const
{
    const auto warmth     = (getParamValue(kMacroWarmth)     - 0.5f) * 2.0f;
    const auto brillance  = (getParamValue(kMacroBrillance)  - 0.5f) * 2.0f;
    const auto space      = (getParamValue(kMacroSpace)      - 0.5f) * 2.0f;
    const auto expression = (getParamValue(kMacroExpression)  - 0.5f) * 2.0f;

    const auto family = mos::getFamily(instrIndex);
    const bool isHarp = instrIndex == 4;
    const bool isLowAnchor = instrIndex == 3 || instrIndex == 15;
    const bool isTimpani = instrIndex == 16;
    const bool isCelesta = instrIndex == 17;

    s.warmth = clamp01(s.warmth + warmth * 0.10f);
    s.cutoffHz = juce::jlimit(120.0f, 16000.0f, s.cutoffHz * std::pow(2.0f, -warmth * 0.24f));
    s.brightness = clamp01(s.brightness + brillance * 0.12f);
    s.cutoffHz = juce::jlimit(120.0f, 16000.0f, s.cutoffHz * std::pow(2.0f, brillance * 0.38f));
    s.character = clamp01(s.character + expression * 0.05f);

    if (family == mos::Family::Cordes && !isHarp)
    {
        s.vibrato = clamp01(s.vibrato + warmth * 0.08f + expression * 0.14f);
        s.stereoWidth = clamp01(s.stereoWidth + space * 0.16f);
        s.detune = clamp01(s.detune + space * 0.08f);
        s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + space * 0.22f));
        s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - expression * 0.18f));
        if (isLowAnchor)
        {
            s.stereoWidth = juce::jmin(s.stereoWidth, 0.14f);
            s.detune = juce::jmin(s.detune, 0.04f);
        }
    }
    else if (isHarp)
    {
        s.vibrato = 0.0f;
        s.brightness = clamp01(s.brightness + brillance * 0.08f);
        s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - expression * 0.12f));
        s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + space * 0.10f));
        s.stereoWidth = clamp01(s.stereoWidth + space * 0.10f);
        s.detune = clamp01(s.detune + space * 0.03f);
    }
    else if (family == mos::Family::Bois)
    {
        s.warmth = clamp01(s.warmth + warmth * 0.04f);
        s.brightness = clamp01(s.brightness + brillance * 0.10f);
        s.vibrato = clamp01(s.vibrato + expression * 0.05f);
        s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - expression * 0.12f));
        s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + space * 0.08f));
        s.stereoWidth = clamp01(s.stereoWidth + space * 0.06f);
        s.detune = clamp01(s.detune + space * 0.02f);
    }
    else if (family == mos::Family::Cuivres)
    {
        s.warmth = clamp01(s.warmth + warmth * 0.09f);
        s.level = clamp01(s.level + expression * 0.04f);
        s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - expression * 0.15f));
        s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + space * 0.10f));
        s.brightness = clamp01(s.brightness + brillance * 0.08f);
        s.stereoWidth = clamp01(s.stereoWidth + space * 0.07f);
        s.detune = clamp01(s.detune + space * 0.03f);
    }
    else if (family == mos::Family::Percussions)
    {
        s.vibrato = 0.0f;
        s.detune = 0.0f;
        s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - expression * 0.10f));
        s.brightness = clamp01(s.brightness + brillance * (isCelesta ? 0.12f : 0.08f));
        s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + space * (isCelesta ? 0.10f : 0.05f)));
        s.stereoWidth = clamp01(s.stereoWidth + space * (isCelesta ? 0.03f : 0.02f));
        s.character = clamp01(s.character + expression * (isCelesta ? 0.07f : 0.05f));
        if (isTimpani)
            s.warmth = clamp01(s.warmth + warmth * 0.04f);
    }

    s = sanitizeInstrSettings(instrIndex, s);
}

int OrchSynthAudioProcessor::findFreeVoice() const
{
    for (int i = 0; i < kMaxVoices; ++i)
    {
        const auto& slot = voices[static_cast<std::size_t>(i)];
        const bool activeAvailable = slot.active == nullptr || !slot.active->isActive();
        const bool dyingAvailable = slot.dying == nullptr || !slot.dying->isActive();
        if (activeAvailable && dyingAvailable)
            return i;
    }

    int oldest = 0;
    uint64_t oldestAge = UINT64_MAX;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[static_cast<std::size_t>(i)].active != nullptr
            && voices[static_cast<std::size_t>(i)].active->isReleasing()
            && voices[static_cast<std::size_t>(i)].activationAge < oldestAge)
        {
            oldest = i;
            oldestAge = voices[static_cast<std::size_t>(i)].activationAge;
        }
    }
    if (oldestAge < UINT64_MAX) return oldest;

    oldestAge = UINT64_MAX;
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

void OrchSynthAudioProcessor::clearVoice(VoiceSlot& slot)
{
    slot.active = nullptr;
    slot.dying = nullptr;
    slot.dyingBus = 0;
    slot.dyingMidiChannel = 0;
    slot.sourceMidiNote = -1;
    slot.renderMidiNote = -1;
    slot.instrIndex = 0;
    slot.midiChannel = 0;
    slot.deferredNoteOff = false;
    slot.heldAfterKeyRelease = false;
    slot.activeTailHintSeconds = 0.0f;
    slot.dyingTailHintSeconds = 0.0f;
    slot.activationAge = 0;
}

void OrchSynthAudioProcessor::panicAllVoices()
{
    for (auto& bend : pitchBendPerChannel)
        bend.reset();
    resetMidiPerformanceState();
    for (auto& slot : voices)
        clearVoice(slot);
    liveVoiceCount.store(0);
    displayedVoiceCount.store(0);
    if (pendingFxRecallInstrIndex.load() >= 0)
        triggerAsyncUpdate();
}

void OrchSynthAudioProcessor::triggerNoteOn(int instrIndex, int midiNote, float velocity, int midiChannel)
{
    if (instrIndex < 0 || instrIndex >= mos::kNumInstruments) return;
    if (preparedSampleRate <= 0.0) return;

    const auto noteRange = mos::getInstrMidiNoteRange(instrIndex);
    const int renderMidiNote = juce::jlimit(noteRange.low, noteRange.high, midiNote);
    const int channel = juce::jlimit(0, 15, midiChannel);
    auto settings = snapshotInstrSettings(instrIndex);
    const auto portamentoSeconds = juce::jlimit(0.0f, 2.0f, getParamValue(kPortamentoSeconds));
    const auto legatoAmount = clamp01(getParamValue(kLegatoAmount));
    float previousNoteFrequency = 0.0f;
    int previousSlotIndex = -1;
    uint64_t newestActivationAge = 0;
    if (portamentoSeconds > 0.0f)
    {
        for (int index = 0; index < kMaxVoices; ++index)
        {
            const auto& candidate = voices[static_cast<std::size_t>(index)];
            if (candidate.active == nullptr
                || !candidate.active->isActive()
                || candidate.instrIndex != instrIndex
                || candidate.midiChannel != channel
                || candidate.activationAge < newestActivationAge)
            {
                continue;
            }

            newestActivationAge = candidate.activationAge;
            previousSlotIndex = index;
        }
        if (previousSlotIndex >= 0)
        {
            const auto previousNote = voices[static_cast<std::size_t>(previousSlotIndex)].renderMidiNote;
            previousNoteFrequency = 440.0f * std::pow(2.0f,
                (static_cast<float>(previousNote) - 69.0f + settings.tuneSemitones) / 12.0f);
        }
    }

    const int slot = findFreeVoice();
    auto& v = voices[static_cast<std::size_t>(slot)];
    auto* primaryVoice = v.voiceBank[static_cast<std::size_t>(instrIndex)].get();
    if (primaryVoice == nullptr)
        return;
    auto* alternateVoice = v.alternateVoiceBank[static_cast<std::size_t>(instrIndex)].get();
    auto* targetVoice = primaryVoice;
    if (v.active == targetVoice || v.dying == targetVoice)
        targetVoice = alternateVoice;
    if (v.active == targetVoice || v.dying == targetVoice)
        targetVoice = nullptr;
    if (targetVoice == nullptr)
    {
        if (v.dying != nullptr)
        {
            v.dying = nullptr;
            v.dyingTailHintSeconds = 0.0f;
            targetVoice = (v.active == primaryVoice) ? alternateVoice : primaryVoice;
        }
        if (targetVoice == nullptr || v.active == targetVoice)
            return;
    }

    // Move stolen voice to dying slot for crossfade. Same-instrument retriggers use
    // the slot's alternate preallocated voice so the previous object can fade out.
    if (v.active != nullptr && v.active->isActive() && !v.active->isReleasing())
    {
        v.active->forceQuickRelease();
        v.dying = v.active;
        v.dyingMidiChannel = v.midiChannel;
        v.dyingBus = juce::jlimit(0, kNumAuxOutputs,
            static_cast<int>(std::round(getParamValue(
                makeInstrParamId(v.instrIndex, kInstrOutputSuffix)))));
        v.dyingTailHintSeconds = juce::jmax(v.activeTailHintSeconds,
                                            static_cast<float>(64.0 / preparedSampleRate) + 0.02f);
    }

    if (previousSlotIndex >= 0 && previousSlotIndex != slot)
    {
        auto& previousSlot = voices[static_cast<std::size_t>(previousSlotIndex)];
        if (previousSlot.active != nullptr && previousSlot.active->isActive() && !previousSlot.active->isReleasing())
            previousSlot.active->forceQuickRelease();
    }

    v.active = targetVoice;
    v.sourceMidiNote = midiNote;
    v.renderMidiNote = renderMidiNote;
    v.instrIndex = instrIndex;
    v.midiChannel = channel;
    v.deferredNoteOff = false;
    v.heldAfterKeyRelease = false;
    v.activeTailHintSeconds = juce::jmax(0.25f, settings.releaseSeconds * 1.5f);
    v.activationAge = ++voiceAgeCounter;

    const auto performanceAtNoteOn = evaluateRealtimePerformanceState(channel, 1.0f, realtimePerformanceBlock);
    settings.attackSeconds = juce::jlimit(0.0001f, 2.0f, settings.attackSeconds * performanceAtNoteOn.attack);
    settings.decaySeconds = juce::jlimit(0.05f, 10.0f, settings.decaySeconds * performanceAtNoteOn.decay);

    const auto roundRobinAmount = juce::jlimit(0.0f, 1.0f, getParamValue(kRoundRobinAmount));
    const auto noteSeed = static_cast<uint32_t>(v.activationAge)
        ^ (static_cast<uint32_t>(renderMidiNote & 0xff) << 8)
        ^ (static_cast<uint32_t>(instrIndex & 0xff) << 16)
        ^ static_cast<uint32_t>(v.midiChannel & 0xff);
    v.active->noteOn(settings, renderMidiNote, velocity, preparedSampleRate,
                     portamentoSeconds, roundRobinAmount, noteSeed, previousNoteFrequency, legatoAmount);
}

void OrchSynthAudioProcessor::triggerNoteOff(int instrIndex, int midiNote, int midiChannel)
{
    juce::ignoreUnused(instrIndex);
    const int ch = juce::jlimit(0, 15, midiChannel);
    const bool stopOnRelease = shouldStopNotesOnKeyRelease();

    for (auto& slot : voices)
    {
        if (slot.active != nullptr
            && slot.active->isActive() && !slot.active->isReleasing()
            && slot.sourceMidiNote == midiNote
            && slot.midiChannel == ch)
        {
            if (sustainPedalDown[static_cast<std::size_t>(ch)])
            {
                slot.deferredNoteOff = true;
                slot.heldAfterKeyRelease = false;
            }
            else if (!stopOnRelease)
            {
                slot.deferredNoteOff = false;
                slot.heldAfterKeyRelease = true;
            }
            else
            {
                const auto pedalPosition = damperPosition[static_cast<std::size_t>(ch)];
                const auto releaseSeconds = pedalPosition > 0.0f
                    ? releaseSecondsForPedal(slot.instrIndex, pedalPosition, ch)
                    : releaseSecondsForNoteOff(slot.instrIndex, ch);
                slot.activeTailHintSeconds = juce::jmax(0.05f, releaseSeconds * 1.5f);
                slot.active->setReleaseTimeSeconds(
                    releaseSeconds);
                slot.active->noteOff();
                slot.deferredNoteOff = false;
                slot.heldAfterKeyRelease = false;
            }
        }
    }
}

void OrchSynthAudioProcessor::resetMidiPerformanceState()
{
    sustainPedalDown.fill(false);
    damperPosition.fill(0.0f);
    channelPerformance.fill(ChannelPerformanceState{});
}

OrchSynthAudioProcessor::ModMatrixSource OrchSynthAudioProcessor::modulationSourceFromParam(float value) noexcept
{
    switch (juce::jlimit(0, 8, static_cast<int>(std::round(value))))
    {
        case 1: return ModMatrixSource::ModWheel;
        case 2: return ModMatrixSource::CC11;
        case 3: return ModMatrixSource::Breath;
        case 4: return ModMatrixSource::Aftertouch;
        case 5: return ModMatrixSource::Velocity;
        case 6: return ModMatrixSource::LFO;
        case 7: return ModMatrixSource::PitchBend;
        case 8: return ModMatrixSource::Envelope;
        default: return ModMatrixSource::Off;
    }
}

OrchSynthAudioProcessor::ModMatrixDestination OrchSynthAudioProcessor::modulationDestinationFromParam(float value) noexcept
{
    switch (juce::jlimit(0, 12, static_cast<int>(std::round(value))))
    {
        case 1: return ModMatrixDestination::Gain;
        case 2: return ModMatrixDestination::Timbre;
        case 3: return ModMatrixDestination::Vibrato;
        case 4: return ModMatrixDestination::Release;
        case 5: return ModMatrixDestination::Aftertouch;
        case 6: return ModMatrixDestination::Cutoff;
        case 7: return ModMatrixDestination::Pan;
        case 8: return ModMatrixDestination::Pitch;
        case 9: return ModMatrixDestination::Attack;
        case 10: return ModMatrixDestination::Decay;
        case 11: return ModMatrixDestination::EqMidFreq;
        case 12: return ModMatrixDestination::EqMidGain;
        default: return ModMatrixDestination::Off;
    }
}

OrchSynthAudioProcessor::RealtimePerformanceBlock
OrchSynthAudioProcessor::captureRealtimePerformanceBlock() const
{
    RealtimePerformanceBlock block;
    block.lfoDepth = clamp01(getParamValue(kLfoDepth));
    block.lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(getParamValue(kLfoWave))));
    block.lfoValue = sampleLfoWaveform(lfoPhase, block.lfoWave);

    for (int slotIndex = 0; slotIndex < kNumModMatrixSlots; ++slotIndex)
    {
        const auto* sourceParam = parameters.getRawParameterValue(kModMatrixSourceParamIds[static_cast<std::size_t>(slotIndex)]);
        const auto* destParam = parameters.getRawParameterValue(kModMatrixDestParamIds[static_cast<std::size_t>(slotIndex)]);
        const auto* amountParam = parameters.getRawParameterValue(kModMatrixAmountParamIds[static_cast<std::size_t>(slotIndex)]);
        if (sourceParam == nullptr || destParam == nullptr || amountParam == nullptr)
            continue;

        const auto source = modulationSourceFromParam(sourceParam->load());
        const auto destination = modulationDestinationFromParam(destParam->load());
        block.slots[static_cast<std::size_t>(slotIndex)] = {
            static_cast<int>(source),
            static_cast<int>(destination),
            juce::jlimit(-1.0f, 1.0f, amountParam->load())
        };
        if (source == ModMatrixSource::ModWheel && destination == ModMatrixDestination::Gain)
            block.hasExplicitModWheelGain = true;
    }

    return block;
}

OrchSynthAudioProcessor::RealtimePerformanceState
OrchSynthAudioProcessor::evaluateRealtimePerformanceState(int midiChannel, float envelopeLevel) const
{
    return evaluateRealtimePerformanceState(midiChannel, envelopeLevel, captureRealtimePerformanceBlock());
}

OrchSynthAudioProcessor::RealtimePerformanceState
OrchSynthAudioProcessor::evaluateRealtimePerformanceState(int midiChannel,
                                                          float envelopeLevel,
                                                          const RealtimePerformanceBlock& block) const
{
    const int ch = juce::jlimit(0, 15, midiChannel);
    const auto& performance = channelPerformance[static_cast<std::size_t>(ch)];
    const float expression = juce::jlimit(0.0f, 1.0f, performance.expression);
    const float modWheel = juce::jlimit(0.0f, 1.0f, performance.modWheel);
    const float breath = juce::jlimit(0.0f, 1.0f, performance.breath);
    const float aftertouch = juce::jlimit(0.0f, 1.0f, performance.aftertouch);
    const float noteVelocity = juce::jlimit(0.0f, 1.0f, performance.lastNoteVelocity);
    const float pitchBendValue = juce::jlimit(-1.0f, 1.0f,
        static_cast<float>((pitchBendPerChannel[static_cast<std::size_t>(ch)].pitchBendFactor - 1.0) * 2.0));
    const float envelopeValue = juce::jlimit(0.0f, 1.0f, envelopeLevel);

    const float modWheelExpression = performance.modWheelSeen ? modWheel : 1.0f;
    float gain = expression * (block.hasExplicitModWheelGain ? 1.0f : modWheelExpression) * (1.0f + aftertouch * 0.15f);
    float timbre = 1.0f + breath * 0.35f + aftertouch * 0.20f;
    float vibrato = 1.0f + aftertouch * 0.60f;
    float release = 1.0f;
    float aftertouchBias = 0.0f;
    float pan = 0.0f;
    float pitchSemitones = 0.0f;
    float attack = 1.0f;
    float decay = 1.0f;
    float eqMidFreqOctaves = 0.0f;
    float eqMidGainDb = 0.0f;

    for (const auto& slot : block.slots)
    {
        const auto source = static_cast<ModMatrixSource>(slot.source);
        const auto destination = static_cast<ModMatrixDestination>(slot.destination);
        if (source == ModMatrixSource::Off || destination == ModMatrixDestination::Off)
            continue;

        float sourceValue = 0.0f;
        switch (source)
        {
            case ModMatrixSource::ModWheel:   sourceValue = modWheel; break;
            case ModMatrixSource::CC11:       sourceValue = expression; break;
            case ModMatrixSource::Breath:     sourceValue = breath; break;
            case ModMatrixSource::Aftertouch: sourceValue = aftertouch; break;
            case ModMatrixSource::Velocity:   sourceValue = noteVelocity; break;
            case ModMatrixSource::LFO:        sourceValue = block.lfoValue; break;
            case ModMatrixSource::PitchBend:  sourceValue = pitchBendValue; break;
            case ModMatrixSource::Envelope:   sourceValue = envelopeValue; break;
            case ModMatrixSource::Off:        sourceValue = 0.0f; break;
        }

        const float contribution = slot.amount * sourceValue;

        switch (destination)
        {
            case ModMatrixDestination::Gain:       gain += contribution; break;
            case ModMatrixDestination::Timbre:     timbre += contribution; break;
            case ModMatrixDestination::Vibrato:    vibrato += contribution; break;
            case ModMatrixDestination::Release:    release += contribution; break;
            case ModMatrixDestination::Aftertouch: aftertouchBias += contribution; break;
            case ModMatrixDestination::Cutoff:     timbre += contribution * 0.75f; break;
            case ModMatrixDestination::Pan:        pan += contribution; break;
            case ModMatrixDestination::Pitch:      pitchSemitones += contribution * 2.0f; break;
            case ModMatrixDestination::Attack:     attack += contribution; break;
            case ModMatrixDestination::Decay:      decay += contribution; break;
            case ModMatrixDestination::EqMidFreq:  eqMidFreqOctaves += contribution; break;
            case ModMatrixDestination::EqMidGain:  eqMidGainDb += contribution * 12.0f; break;
            case ModMatrixDestination::Off:        break;
        }
    }

    if (std::abs(aftertouchBias) > 1.0e-5f)
    {
        const float effectiveAftertouch = juce::jlimit(0.0f, 1.0f, aftertouch + aftertouchBias);
        const float delta = effectiveAftertouch - aftertouch;
        gain += delta * 0.15f;
        timbre += delta * 0.20f;
        vibrato += delta * 0.60f;
    }

    RealtimePerformanceState state;
    state.gain = juce::jlimit(0.0f, 2.0f, gain);
    state.timbre = juce::jlimit(0.25f, 2.5f, timbre);
    state.vibrato = juce::jlimit(0.0f, 2.5f, vibrato);
    state.release = juce::jlimit(0.25f, 2.5f, release);
    state.pitch = std::exp2(juce::jlimit(-12.0f, 12.0f, pitchSemitones) / 12.0f);
    state.pan = juce::jlimit(-1.0f, 1.0f, pan);
    state.attack = juce::jlimit(0.25f, 4.0f, attack);
    state.decay = juce::jlimit(0.25f, 4.0f, decay);
    state.eqMidFreq = std::exp2(juce::jlimit(-2.0f, 2.0f, eqMidFreqOctaves));
    state.eqMidGainDb = juce::jlimit(-12.0f, 12.0f, eqMidGainDb);
    return state;
}

void OrchSynthAudioProcessor::applyRealtimePerformance(mos::OrchVoice& voice, int midiChannel) const
{
    applyRealtimePerformance(voice, midiChannel, realtimePerformanceBlock);
}

void OrchSynthAudioProcessor::applyRealtimePerformance(mos::OrchVoice& voice,
                                                       int midiChannel,
                                                       const RealtimePerformanceBlock& block) const
{
    auto state = evaluateRealtimePerformanceState(midiChannel, voice.getEnvelopeLevelEstimate(), block);

    if (block.lfoDepth > 0.0001f)
    {
        const float lfo = block.lfoValue;
        switch (voice.getInstrumentIndex())
        {
            case 0:
            case 1:
            case 2:
            case 3:
                state.timbre += lfo * block.lfoDepth * 0.18f;
                state.vibrato += lfo * block.lfoDepth * 0.55f;
                break;
            case 4:
                state.timbre += lfo * block.lfoDepth * 0.10f;
                break;
            case 5:
            case 6:
            case 7:
            case 8:
                state.timbre += lfo * block.lfoDepth * 0.22f;
                state.vibrato += lfo * block.lfoDepth * 0.22f;
                break;
            case 9:
            case 10:
            case 11:
            case 12:
                state.timbre += lfo * block.lfoDepth * 0.28f;
                state.vibrato += lfo * block.lfoDepth * 0.10f;
                break;
            case 13:
            case 14:
            default:
                break;
        }
    }

    voice.setPerformanceControls(state.gain, state.timbre, state.vibrato, state.pitch, state.pan);
}

float OrchSynthAudioProcessor::releaseSecondsForNoteOff(int instrIndex, int midiChannel) const
{
    const auto baseRelease = sanitizeParameterValue(makeInstrParamId(instrIndex, "release"),
                                                    getParamValue(makeInstrParamId(instrIndex, "release")),
                                                    0.4f);
    const auto performance = evaluateRealtimePerformanceState(midiChannel, 1.0f, realtimePerformanceBlock);
    return juce::jlimit(0.01f, 8.0f, baseRelease * performance.release);
}

float OrchSynthAudioProcessor::releaseSecondsForPedal(int instrIndex,
                                                      float pedalPosition,
                                                      int midiChannel) const
{
    const auto baseRelease = releaseSecondsForNoteOff(instrIndex, midiChannel);
    const auto extendedRelease = juce::jlimit(0.01f, 8.0f, baseRelease * (1.0f + pedalPosition * 2.5f));
    return juce::jmap(juce::jlimit(0.0f, 1.0f, pedalPosition), baseRelease, extendedRelease);
}

void OrchSynthAudioProcessor::sumAuxBusesIntoMain(juce::AudioBuffer<float>& buffer,
                                                  juce::AudioBuffer<float>& mainBuffer,
                                                  int outputBusCount)
{
    if (mainBuffer.getNumChannels() <= 0 || mainBuffer.getNumSamples() <= 0)
        return;

    // Auxiliary buses remain dry/direct stems, but they are summed back into the
    // master dry mix before the shared master-FX chain so the main bus stays the
    // full production output instead of silently dropping stem-routed instruments.
    for (int busIndex = 1; busIndex < outputBusCount; ++busIndex)
    {
        auto auxBuffer = getBusBuffer(buffer, false, busIndex);
        if (auxBuffer.getNumChannels() <= 0 || auxBuffer.getNumSamples() <= 0)
            continue;

        const int mainChannels = mainBuffer.getNumChannels();
        const int auxChannels = auxBuffer.getNumChannels();
        const int samples = juce::jmin(mainBuffer.getNumSamples(), auxBuffer.getNumSamples());

        if (mainChannels == 1 && auxChannels > 1)
        {
            auto* mainData = mainBuffer.getWritePointer(0);
            const auto* left = auxBuffer.getReadPointer(0);
            const auto* right = auxBuffer.getReadPointer(1);
            for (int sample = 0; sample < samples; ++sample)
                mainData[sample] += 0.5f * (left[sample] + right[sample]);
            continue;
        }

        const int channelsToCopy = juce::jmin(mainChannels, auxChannels);
        for (int channel = 0; channel < channelsToCopy; ++channel)
            mainBuffer.addFrom(channel, 0, auxBuffer, channel, 0, samples);
    }
}

void OrchSynthAudioProcessor::releaseSustainedVoices(int channel, float pedalPosition)
{
    const int ch = juce::jlimit(0, 15, channel);

    for (auto& slot : voices)
    {
        if (slot.active != nullptr && slot.active->isActive()
            && !slot.active->isReleasing()
            && slot.deferredNoteOff
            && slot.midiChannel == ch)
        {
            const auto releaseSeconds = releaseSecondsForPedal(slot.instrIndex, pedalPosition, ch);
            slot.activeTailHintSeconds = juce::jmax(0.05f, releaseSeconds * 1.5f);
            slot.active->setReleaseTimeSeconds(releaseSeconds);
            slot.active->noteOff();
            slot.deferredNoteOff = false;
            slot.heldAfterKeyRelease = false;
        }
    }
}

// =============================================================================
void OrchSynthAudioProcessor::updateGlobalEffectParameters()
{
    const auto threshold = getParamValue(kCompThreshold);
    const auto ratio     = getParamValue(kCompRatio);
    const auto attack    = getParamValue(kCompAttack);
    const auto release   = getParamValue(kCompRelease);

    if (threshold != compCache.threshold) { compressor.setThreshold(threshold); compCache.threshold = threshold; }
    if (ratio     != compCache.ratio)     { compressor.setRatio(ratio);          compCache.ratio     = ratio; }
    if (attack    != compCache.attack)    { compressor.setAttack(attack);         compCache.attack    = attack; }
    if (release   != compCache.release)   { compressor.setRelease(release);       compCache.release   = release; }
}

void OrchSynthAudioProcessor::processGlobalTransient(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Transient)) return;
    if (getParamValue("fx_tab2_en") < 0.5f) return;

    mos::fx::TransientShaper::Params p;
    p.attack  = juce::jlimit(-1.0f, 1.0f, getParamValue(kTransientAttack));
    p.sustain = juce::jlimit(-1.0f, 1.0f, getParamValue(kTransientSustain));
    p.mix     = clamp01(getParamValue(kTransientMix));

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    transientShaper.process(left, right, mainBuffer.getNumSamples(), p);
}

void OrchSynthAudioProcessor::processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Saturator) && satMixCurrent <= 0.0001f) return;

    const bool effectActive = isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Saturator)
        && getParamValue("fx_tab1_en") >= 0.5f;
    const auto targetMix = effectActive ? clamp01(getParamValue(kSatMix)) : 0.0f;
    const auto targetDrive = juce::jlimit(1.0f, 16.0f, getParamValue(kSatDrive));
    if (targetMix <= 0.0001f && satMixCurrent <= 0.0001f)
        return;

    const int numSamples = mainBuffer.getNumSamples();
    if (numSamples <= 0)
        return;
    const float mixStep = numSamples > 0 ? (targetMix - satMixCurrent) / static_cast<float>(numSamples) : 0.0f;
    const float driveStep = numSamples > 0 ? (targetDrive - satDriveCurrent) / static_cast<float>(numSamples) : 0.0f;

    if (getQualityMode() == QualityMode::Studio)
    {
        const bool needsDryMix = targetMix < 0.9999f || satMixCurrent < 0.9999f;
        if (needsDryMix
            && (fxDryBuffer.getNumChannels() < mainBuffer.getNumChannels()
                || fxDryBuffer.getNumSamples() < numSamples))
        {
            jassertfalse;
            return;
        }

        if (needsDryMix)
            for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
                fxDryBuffer.copyFrom(ch, 0, mainBuffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> block(mainBuffer);
        auto osBlock = satOversampling.processSamplesUp(block);
        const float norm = 1.0f / std::max(0.0001f, std::tanh(targetDrive));
        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* data = osBlock.getChannelPointer(ch);
            for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
                data[i] = std::tanh(data[i] * targetDrive) * norm;
        }
        satOversampling.processSamplesDown(block);

        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        {
            auto* wet = mainBuffer.getWritePointer(ch);
            if (needsDryMix)
            {
                const auto* dry = fxDryBuffer.getReadPointer(ch);
                float mix = satMixCurrent;
                for (int i = 0; i < numSamples; ++i)
                {
                    wet[i] = dry[i] + (wet[i] - dry[i]) * mix;
                    mix += mixStep;
                }
                saturatorPrevInput[static_cast<std::size_t>(juce::jlimit(0, 1, ch))] = dry[numSamples - 1];
            }
            else
            {
                saturatorPrevInput[static_cast<std::size_t>(juce::jlimit(0, 1, ch))] = wet[numSamples - 1];
            }
        }

        satDriveCurrent = targetDrive;
        satMixCurrent = targetMix;
        return;
    }

    for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
    {
        auto* data = mainBuffer.getWritePointer(ch);
        auto& prevInput = saturatorPrevInput[static_cast<std::size_t>(juce::jlimit(0, 1, ch))];
        float drive = satDriveCurrent;
        float mix = satMixCurrent;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto dry = data[i];
            const auto norm = 1.0f / std::max(0.0001f, std::tanh(drive));
            float wet = std::tanh(dry * drive) * norm;

            data[i] = dry + (wet - dry) * mix;
            prevInput = dry;
            drive += driveStep;
            mix += mixStep;
        }
    }

    satDriveCurrent = targetDrive;
    satMixCurrent = targetMix;
}

void OrchSynthAudioProcessor::processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Compressor)) return;
    if (getParamValue("fx_tab3_en") < 0.5f) return;
    const auto mix = clamp01(getParamValue(kCompMix));

    if (mix <= 0.0001f)
        return;

    const bool needsDryMix = mix < 0.9999f;
    if (needsDryMix
        && (fxDryBuffer.getNumChannels() < mainBuffer.getNumChannels()
            || fxDryBuffer.getNumSamples() < mainBuffer.getNumSamples()))
    {
        jassertfalse;
        return;
    }

    updateGlobalEffectParameters();

    if (needsDryMix)
        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
            fxDryBuffer.copyFrom(ch, 0, mainBuffer, ch, 0, mainBuffer.getNumSamples());

    juce::dsp::AudioBlock<float> block(mainBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    compressor.process(context);

    if (needsDryMix)
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

void OrchSynthAudioProcessor::applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer)
{
    const auto numSamples = mainBuffer.getNumSamples();
    if (numSamples <= 0)
        return;

    const float rateHz = juce::jlimit(0.05f, 12.0f, getParamValue(kLfoRate));
    const float targetDepth = clamp01(getParamValue(kLfoDepth));
    const int wave = juce::jlimit(0, 3, static_cast<int>(std::round(getParamValue(kLfoWave))));
    const float phaseInc = rateHz / static_cast<float>(juce::jmax(1.0, preparedSampleRate));
    const int numChannels = mainBuffer.getNumChannels();
    float phase = lfoPhase;
    const float startDepth = globalLfoDepthCurrent;
    const float depthStep = (targetDepth - startDepth) / static_cast<float>(juce::jmax(1, numSamples));
    float depth = startDepth;

    if (numChannels <= 0)
    {
        lfoPhase += phaseInc * static_cast<float>(numSamples);
        lfoPhase -= std::floor(lfoPhase);
        globalLfoDepthCurrent = targetDepth;
        return;
    }

    auto* left = mainBuffer.getWritePointer(0);
    auto* right = numChannels > 1 ? mainBuffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        const float lfo = sampleLfoWaveform(phase, wave);
        if (depth > 0.0001f)
        {
            const float tremolo = 1.0f + lfo * depth * 0.08f;
            if (right != nullptr)
            {
                const float pan = lfo * depth * 0.10f;
                left[i] *= tremolo * (1.0f - juce::jmax(0.0f, pan));
                right[i] *= tremolo * (1.0f + juce::jmin(0.0f, pan));
            }
            else
            {
                left[i] *= tremolo;
            }
        }

        depth += depthStep;
        phase += phaseInc;
        if (phase >= 1.0f)
            phase -= std::floor(phase);
    }

    lfoPhase = phase;
    globalLfoDepthCurrent = targetDepth;
}

// =============================================================================
void OrchSynthAudioProcessor::processGlobalEQ(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Eq)) return;
    if (getParamValue("fx_eq_en") < 0.5f) return;

    mos::fx::ParametricEQ3Band::Params p;
    p.lowFreq    = getParamValue(kEqLowFreq);
    p.lowGainDb  = getParamValue(kEqLowGain);
    const auto eqMod = evaluateRealtimePerformanceState(0, 1.0f, realtimePerformanceBlock);
    p.midFreq    = juce::jlimit(200.0f, 8000.0f, getParamValue(kEqMidFreq) * eqMod.eqMidFreq);
    p.midGainDb  = juce::jlimit(-12.0f, 12.0f, getParamValue(kEqMidGain) + eqMod.eqMidGainDb);
    p.midQ       = getParamValue(kEqMidQ);
    p.highFreq   = getParamValue(kEqHighFreq);
    p.highGainDb = getParamValue(kEqHighGain);

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    eq.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
void OrchSynthAudioProcessor::processGlobalChorus(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Chorus)) return;
    if (getParamValue("fx_chorus_en") < 0.5f) return;

    mos::fx::StereoChorus::Params p;
    p.rateHz = getParamValue(kChorusRate);
    p.depth  = getParamValue(kChorusDepth);
    p.mix    = getParamValue(kChorusMix);

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    chorus.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
void OrchSynthAudioProcessor::processGlobalDelay(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Delay)) return;
    if (getParamValue("fx_delay_en") < 0.5f) return;

    mos::fx::StereoDelay::Params p;
    p.timeMs   = getParamValue(kDelayTime);
    if (isDelaySyncEnabled())
    {
        static constexpr std::array<float, 6> kDelayBeats { 1.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f };
        const auto bpm = juce::jlimit(20.0f, 320.0f, getLastKnownHostTempoBpm());
        const auto beatSeconds = 60.0f / juce::jmax(1.0f, bpm);
        const auto beats = kDelayBeats[static_cast<std::size_t>(getDelayDivisionIndex())];
        p.timeMs = beatSeconds * beats * 1000.0f;
    }
    p.feedback = getParamValue(kDelayFeedback);
    p.mix      = getParamValue(kDelayMix);

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    stereoDelay.process(left, right, mainBuffer.getNumSamples(), p);
}

void OrchSynthAudioProcessor::updateOutputMeters(juce::AudioBuffer<float>& fullBuffer,
                                                 const juce::AudioBuffer<float>& mainBuffer)
{
    for (int channel = 0; channel < 2; ++channel)
    {
        float peak = 0.0f;
        if (channel < mainBuffer.getNumChannels())
            peak = mainBuffer.getMagnitude(channel, 0, mainBuffer.getNumSamples());
        mainMeterLevels[static_cast<std::size_t>(channel)].store(peak, std::memory_order_relaxed);
        if (peak >= 0.999f)
            clipLatched.store(true, std::memory_order_relaxed);
    }

    for (int auxIndex = 0; auxIndex < kNumAuxOutputs; ++auxIndex)
    {
        float peak = 0.0f;
        const int busIndex = auxIndex + 1;
        if (busIndex < getBusCount(false) && getChannelCountOfBus(false, busIndex) > 0)
        {
            auto auxBuffer = getBusBuffer(fullBuffer, false, busIndex);
            for (int channel = 0; channel < auxBuffer.getNumChannels(); ++channel)
                peak = juce::jmax(peak, auxBuffer.getMagnitude(channel, 0, auxBuffer.getNumSamples()));
        }
        auxMeterLevels[static_cast<std::size_t>(auxIndex)].store(peak, std::memory_order_relaxed);
    }
}

// =============================================================================
void OrchSynthAudioProcessor::processGlobalReverb(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Reverb)) return;
    if (getParamValue("fx_tab0_en") < 0.5f) return;

    const auto reverbType = juce::jlimit(0, 1, static_cast<int>(std::round(getParamValue(kReverbType))));
    const float decay = clamp01(getParamValue(kReverbSize));
    const float damping = clamp01(getParamValue(kReverbDamping));
    const float width = clamp01(getParamValue(kReverbWidth));
    const float mix = clamp01(getParamValue(kReverbMix));
    const float preDelayMs = getParamValue(kReverbPredelay);

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;

    if (reverbType == 1)
    {
        mos::fx::DiffuseHallReverb::Params p;
        p.decay = decay;
        p.damping = damping;
        p.width = width;
        p.mix = mix;
        p.preDelayMs = preDelayMs;
        hallReverb.process(left, right, mainBuffer.getNumSamples(), p);
        return;
    }

    mos::fx::DattorroPlateReverb::Params p;
    p.decay      = decay;
    p.damping    = damping;
    p.width      = width;
    p.mix        = mix;
    p.preDelayMs = preDelayMs;
    reverb.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
void OrchSynthAudioProcessor::processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer)
{
    if (!isFxAvailableForCurrentInstr(mos::GlobalFxSlot::Limiter)) return;
    if (getParamValue("fx_limiter_en") < 0.5f) return;

    mos::fx::OutputLimiter::Params p;
    p.thresholdDb = getParamValue(kLimiterThreshold);
    p.releaseMs   = getParamValue(kLimiterRelease);

    const int numCh = mainBuffer.getNumChannels();
    float* left  = mainBuffer.getWritePointer(0);
    float* right = numCh > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    limiter.process(left, right, mainBuffer.getNumSamples(), p);
}

// =============================================================================
mos::GlobalFxSettings OrchSynthAudioProcessor::snapshotFx() const
{
    mos::GlobalFxSettings fx;
    fx.satDrive         = getParamValue(kSatDrive);
    fx.satMix           = getParamValue(kSatMix);
    fx.transientAttack  = getParamValue(kTransientAttack);
    fx.transientSustain = getParamValue(kTransientSustain);
    fx.transientMix     = getParamValue(kTransientMix);
    fx.eqLowFreq        = getParamValue(kEqLowFreq);
    fx.eqLowGain        = getParamValue(kEqLowGain);
    fx.eqMidFreq         = getParamValue(kEqMidFreq);
    fx.eqMidGain         = getParamValue(kEqMidGain);
    fx.eqMidQ            = getParamValue(kEqMidQ);
    fx.eqHighFreq        = getParamValue(kEqHighFreq);
    fx.eqHighGain        = getParamValue(kEqHighGain);
    fx.compThreshold     = getParamValue(kCompThreshold);
    fx.compRatio         = getParamValue(kCompRatio);
    fx.compAttack        = getParamValue(kCompAttack);
    fx.compRelease       = getParamValue(kCompRelease);
    fx.compMix           = getParamValue(kCompMix);
    fx.chorusRate        = getParamValue(kChorusRate);
    fx.chorusDepth       = getParamValue(kChorusDepth);
    fx.chorusMix         = getParamValue(kChorusMix);
    fx.delayTime         = getParamValue(kDelayTime);
    fx.delayFeedback     = getParamValue(kDelayFeedback);
    fx.delayMix          = getParamValue(kDelayMix);
    fx.reverbSize        = getParamValue(kReverbSize);
    fx.reverbDamping     = getParamValue(kReverbDamping);
    fx.reverbWidth       = getParamValue(kReverbWidth);
    fx.reverbMix         = getParamValue(kReverbMix);
    fx.reverbPredelay    = getParamValue(kReverbPredelay);
    fx.reverbType        = juce::jlimit(0, 1, static_cast<int>(std::round(getParamValue(kReverbType))));
    fx.limiterThreshold  = getParamValue(kLimiterThreshold);
    fx.limiterRelease    = getParamValue(kLimiterRelease);
    return sanitizeFxSettings(fx);
}

void OrchSynthAudioProcessor::applyFxSettingsToParams(const mos::GlobalFxSettings& fx, bool notifyHost)
{
    const auto sanitized = sanitizeFxSettings(fx);
    setParamValueInternal(kSatDrive,         sanitized.satDrive, notifyHost);
    setParamValueInternal(kSatMix,           sanitized.satMix, notifyHost);
    setParamValueInternal(kTransientAttack,  sanitized.transientAttack, notifyHost);
    setParamValueInternal(kTransientSustain, sanitized.transientSustain, notifyHost);
    setParamValueInternal(kTransientMix,     sanitized.transientMix, notifyHost);
    setParamValueInternal(kEqLowFreq,        sanitized.eqLowFreq, notifyHost);
    setParamValueInternal(kEqLowGain,        sanitized.eqLowGain, notifyHost);
    setParamValueInternal(kEqMidFreq,        sanitized.eqMidFreq, notifyHost);
    setParamValueInternal(kEqMidGain,        sanitized.eqMidGain, notifyHost);
    setParamValueInternal(kEqMidQ,           sanitized.eqMidQ, notifyHost);
    setParamValueInternal(kEqHighFreq,       sanitized.eqHighFreq, notifyHost);
    setParamValueInternal(kEqHighGain,       sanitized.eqHighGain, notifyHost);
    setParamValueInternal(kCompThreshold,    sanitized.compThreshold, notifyHost);
    setParamValueInternal(kCompRatio,        sanitized.compRatio, notifyHost);
    setParamValueInternal(kCompAttack,       sanitized.compAttack, notifyHost);
    setParamValueInternal(kCompRelease,      sanitized.compRelease, notifyHost);
    setParamValueInternal(kCompMix,          sanitized.compMix, notifyHost);
    setParamValueInternal(kChorusRate,       sanitized.chorusRate, notifyHost);
    setParamValueInternal(kChorusDepth,      sanitized.chorusDepth, notifyHost);
    setParamValueInternal(kChorusMix,        sanitized.chorusMix, notifyHost);
    setParamValueInternal(kDelayTime,        sanitized.delayTime, notifyHost);
    setParamValueInternal(kDelayFeedback,    sanitized.delayFeedback, notifyHost);
    setParamValueInternal(kDelayMix,         sanitized.delayMix, notifyHost);
    setParamValueInternal(kReverbSize,       sanitized.reverbSize, notifyHost);
    setParamValueInternal(kReverbDamping,    sanitized.reverbDamping, notifyHost);
    setParamValueInternal(kReverbWidth,      sanitized.reverbWidth, notifyHost);
    setParamValueInternal(kReverbMix,        sanitized.reverbMix, notifyHost);
    setParamValueInternal(kReverbPredelay,   sanitized.reverbPredelay, notifyHost);
    setParamValueInternal(kReverbType,       static_cast<float>(sanitized.reverbType), notifyHost);
    setParamValueInternal(kLimiterThreshold, sanitized.limiterThreshold, notifyHost);
    setParamValueInternal(kLimiterRelease,   sanitized.limiterRelease, notifyHost);
}

void OrchSynthAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID != kSelectedInstr)
        return;

    pendingSelectedInstrIndex.store(juce::jlimit(
        0, mos::kNumInstruments - 1, static_cast<int>(std::round(newValue))));

    if (isRestoringState)
        return;

    triggerAsyncUpdate();
}

void OrchSynthAudioProcessor::handleAsyncUpdate()
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

    const int newSelectedInstr = pendingSelectedInstrIndex.load();
    const int currentOwner = juce::jlimit(0, mos::kNumInstruments - 1, currentFxOwnerInstr.load());
    const bool selectionChanged = newSelectedInstr != cachedSelectedInstrIndex;
    const int pendingRecall = pendingFxRecallInstrIndex.load();

    if (!selectionChanged && pendingRecall < 0)
        return;

    cachedFxPerInstr[static_cast<std::size_t>(currentOwner)] = snapshotFx();
    cachedSelectedInstrIndex = newSelectedInstr;

    if (getParamValue("fx_lock") >= 0.5f)
    {
        currentFxOwnerInstr.store(newSelectedInstr);
        pendingFxRecallInstrIndex.store(-1);
        cachedFxPerInstr[static_cast<std::size_t>(newSelectedInstr)] = snapshotFx();
        return;
    }

    if (liveVoiceCount.load() > 0)
    {
        pendingFxRecallInstrIndex.store(newSelectedInstr);
        return;
    }

    currentFxOwnerInstr.store(newSelectedInstr);
    pendingFxRecallInstrIndex.store(-1);
    applyFxSettingsToParams(cachedFxPerInstr[static_cast<std::size_t>(newSelectedInstr)], false);
}

// =============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OrchSynthAudioProcessor();
}
