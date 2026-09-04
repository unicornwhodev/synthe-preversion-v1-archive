#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../Shared/PresetManifest.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <optional>

namespace
{
constexpr const char* kOutputGain          = "output_gain";
constexpr const char* kSelectedInstr       = "selected_instr";
constexpr const char* kQualityMode         = "quality_mode";
constexpr const char* kDelaySync           = "delay_sync";
constexpr const char* kDelayDivision       = "delay_division";

constexpr const char* kLfoRate             = "lfo_rate";
constexpr const char* kLfoDepth            = "lfo_depth";
constexpr const char* kLfoWave             = "lfo_wave";

constexpr const char* kMacroImpact    = "macro_impact";
constexpr const char* kMacroResonance = "macro_resonance";
constexpr const char* kMacroSpace     = "macro_space";
constexpr const char* kMacroCouleur   = "macro_couleur";

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

constexpr const char* kEqLowFreq    = "eq_low_freq";
constexpr const char* kEqLowGain    = "eq_low_gain";
constexpr const char* kEqMidFreq    = "eq_mid_freq";
constexpr const char* kEqMidGain    = "eq_mid_gain";
constexpr const char* kEqMidQ       = "eq_mid_q";
constexpr const char* kEqHighFreq   = "eq_high_freq";
constexpr const char* kEqHighGain   = "eq_high_gain";

constexpr const char* kChorusRate   = "chorus_rate";
constexpr const char* kChorusDepth  = "chorus_depth";
constexpr const char* kChorusMix    = "chorus_mix";

constexpr const char* kDelayTime    = "delay_time";
constexpr const char* kDelayFeedback = "delay_feedback";
constexpr const char* kDelayMix     = "delay_mix";

constexpr const char* kLimiterThreshold = "limiter_threshold";
constexpr const char* kLimiterRelease   = "limiter_release";

constexpr const char* kFxSatEnable   = "fx_tab0_en";
constexpr const char* kFxTransientEnable = "fx_tab1_en";
constexpr const char* kFxCompEnable  = "fx_tab2_en";
constexpr const char* kFxReverbEnable = "fx_tab3_en";
constexpr const char* kFxEqEnable    = "fx_eq_en";
constexpr const char* kFxChorusEnable = "fx_chorus_en";
constexpr const char* kFxDelayEnable = "fx_delay_en";
constexpr const char* kFxLimiterEnable = "fx_limiter_en";

constexpr int kPresetFormatVersion = 3;

constexpr float kDefaultReverbSize    = 0.45f;
constexpr float kDefaultReverbDamping = 0.55f;
constexpr float kDefaultReverbWidth   = 0.80f;
constexpr float kDefaultReverbMix     = 0.22f;

constexpr const char* kInstrOutputSuffix = "output";
static constexpr int kKnobsPerPage = 8;

struct CCSlot {
    const char* paramId;
    const char* instrSuffix;
};

static const char* kCCPageNames[] = {
    "MACROS",
    "ENVELOPE",
    "TONE",
    "REVERB",
    "DYNAMICS",
    "EQ",
    "MOD/LIMITER"
};

static constexpr CCSlot kCCPages[][kKnobsPerPage] = {
    { { "macro_impact",     nullptr }, { "macro_resonance", nullptr },
      { "macro_space",      nullptr }, { "macro_couleur",   nullptr },
      { "lfo_rate",         nullptr }, { "lfo_depth",       nullptr },
      { "reverb_mix",       nullptr }, { "output_gain",     nullptr } },

    { { nullptr, "attack" },       { nullptr, "decay"  },
      { nullptr, "sustain" },      { nullptr, "release" },
      { nullptr, "damping" },      { nullptr, "level" },
      { nullptr, "tune" },         { nullptr, "brightness" } },

    { { nullptr, "body" },                  { nullptr, "noise" },
      { nullptr, "stereo_width" },          { nullptr, "color" },
      { nullptr, "cutoff" },                { nullptr, "pan" },
      { "sat_drive", nullptr },             { "sat_mix",   nullptr } },

    { { "reverb_size",     nullptr }, { "reverb_damping",  nullptr },
      { "reverb_width",    nullptr }, { "reverb_mix",      nullptr },
      { "reverb_predelay", nullptr }, { "delay_time",      nullptr },
      { "delay_feedback",  nullptr }, { "delay_mix",       nullptr } },

    { { "comp_threshold", nullptr }, { "comp_ratio",  nullptr },
      { "comp_attack",    nullptr }, { "comp_release", nullptr },
      { "comp_makeup",    nullptr }, { "comp_mix",     nullptr },
      { "transient_attack", nullptr }, { "transient_sustain", nullptr } },

    { { "eq_low_freq",  nullptr }, { "eq_low_gain",  nullptr },
      { "eq_mid_freq",  nullptr }, { "eq_mid_gain",  nullptr },
      { "eq_mid_q",     nullptr }, { "eq_high_freq", nullptr },
      { "eq_high_gain", nullptr }, { "transient_mix", nullptr } },

    { { "chorus_rate",       nullptr }, { "chorus_depth",      nullptr },
      { "chorus_mix",        nullptr }, { "limiter_threshold", nullptr },
      { "limiter_release",   nullptr }, { "output_gain",       nullptr },
      { nullptr,             nullptr }, { nullptr,             nullptr } }
};

juce::StringArray makeOutputChoices()
{
    juce::StringArray outputs;
    outputs.add("Master");
    for (int i = 0; i < PercSynthAudioProcessor::kNumAuxOutputs; ++i)
        outputs.add("Out " + juce::String(i + 1));
    return outputs;
}

juce::StringArray makeQualityChoices()
{
    return { "Live", "Studio" };
}

juce::StringArray makeDelaySyncChoices()
{
    return { "Off", "Host" };
}

juce::StringArray makeDelayDivisionChoices()
{
    return { "1/4", "1/8", "1/8D", "1/8T", "1/16", "1/16D" };
}

float clamp01(float v) { return juce::jlimit(0.0f, 1.0f, v); }

float stableLogCosh(float x) noexcept
{
    const float ax = std::abs(x);
    return ax + std::log1p(std::exp(-2.0f * ax)) - std::log(2.0f);
}

float tanhAdaa(float input, float& previousInput, float drive) noexcept
{
    const float safeDrive = juce::jlimit(1.0f, 16.0f, drive);
    const float delta = input - previousInput;
    float shaped = 0.0f;
    if (std::abs(delta) > 1.0e-5f)
    {
        const float a = stableLogCosh(safeDrive * input) / safeDrive;
        const float b = stableLogCosh(safeDrive * previousInput) / safeDrive;
        shaped = (a - b) / delta;
    }
    else
    {
        shaped = std::tanh(input * safeDrive);
    }

    previousInput = input;
    return shaped / std::max(0.0001f, std::tanh(safeDrive));
}

juce::File findWritableDirectory(const juce::File& preferred, const juce::String& fallbackRelative)
{
    auto tryDirectory = [](const juce::File& base) -> juce::File
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

void setRangedParameterValue(juce::RangedAudioParameter* parameter, float actualValue, bool notifyHost)
{
    if (parameter == nullptr)
        return;

    const auto normalised = parameter->convertTo0to1(actualValue);
    if (notifyHost)
        parameter->setValueNotifyingHost(normalised);
    else
        parameter->setValue(normalised);
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
    if (end == raw.toRawUTF8() || end == nullptr || *end != '\0' || !std::isfinite(parsed))
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
                           float maxValue,
                           int* warningCount = nullptr)
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
            {
                const auto clamped = juce::jlimit(minValue, maxValue, numeric);
                if (warningCount != nullptr && std::abs(clamped - numeric) > 1.0e-4f)
                    ++(*warningCount);
                return clamped;
            }
        }

        if (warningCount != nullptr)
            ++(*warningCount);
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

using PresetXmlData = PercSynthAudioProcessor::PresetPersistenceState;
using PresetMetadataData = PercSynthAudioProcessor::PersistedPresetMetadata;

constexpr int kPercSynthIndex = 5;

juce::String familyLabelForInstrument(const int instrIndex)
{
    switch (mpc::getFamily(instrIndex))
    {
        case mpc::Family::Percussions: return "percussions";
        case mpc::Family::Ambiance:    return "ambiance";
        case mpc::Family::Metalliques: return "metalliques";
    }

    return "percussions";
}

juce::String instrumentSlugForIndex(const int instrIndex)
{
    auto slug = juce::String(mpc::getInstrName(instrIndex)).toLowerCase();
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

juce::String joinTags(const std::vector<std::string>& tags)
{
    juce::StringArray values;
    for (const auto& tag : tags)
    {
        if (!tag.empty())
            values.add(juce::String(juce::CharPointer_UTF8(tag.c_str())));
    }
    return values.joinIntoString(",");
}

std::vector<std::string> splitTags(const juce::String& tags)
{
    std::vector<std::string> result;
    for (auto token : juce::StringArray::fromTokens(tags, ",;", {}))
    {
        token = token.trim();
        if (token.isNotEmpty())
            result.emplace_back(token.toStdString());
    }
    return result;
}

PresetMetadataData makeUserMetadata(const int instrIndex)
{
    const auto family = familyLabelForInstrument(instrIndex);
    const auto slug = instrumentSlugForIndex(instrIndex);

    PresetMetadataData metadata;
    metadata.mixRole = "custom";
    metadata.family = family;
    metadata.tags = "perc,user,custom," + family + "," + slug;
    metadata.description = "Custom perc preset";
    metadata.outputProfile = "user-custom";
    metadata.nominalPeakDb = -12.0f;
    return metadata;
}

PresetMetadataData makeFactoryMetadata(const mpc::PresetMetadata& metadata)
{
    PresetMetadataData persisted;
    persisted.mixRole = juce::String(juce::CharPointer_UTF8(metadata.mixRole.c_str()));
    persisted.family = juce::String(juce::CharPointer_UTF8(metadata.familyLabel.c_str()));
    persisted.tags = joinTags(metadata.tags);
    persisted.description = juce::String(juce::CharPointer_UTF8(metadata.description.c_str()));
    persisted.outputProfile = juce::String(juce::CharPointer_UTF8(metadata.outputProfile.c_str()));
    persisted.nominalPeakDb = metadata.nominalPeakDb;
    return persisted;
}

modmatrix::MatrixState makeDefaultModMatrixState()
{
    modmatrix::MatrixState state;
    state.pitchBendRange = 2;
    state.lfo2Rate = 2.0f;
    state.lfo2Wave = 0;
    state.slots[0] = { modmatrix::Source::ModWheel,   modmatrix::Destination::Level,     0.30f };
    state.slots[1] = { modmatrix::Source::Aftertouch, modmatrix::Destination::Cutoff,    0.25f };
    state.slots[2] = { modmatrix::Source::Velocity,   modmatrix::Destination::DecayTime, 0.20f };
    state.slots[3] = { modmatrix::Source::Envelope,   modmatrix::Destination::Cutoff,    0.18f };
    state.slots[4] = { modmatrix::Source::LFO2,       modmatrix::Destination::Pan,       0.20f };
    state.slots[5] = { modmatrix::Source::Velocity,   modmatrix::Destination::EqMidGain, 0.18f };
    state.slots[6] = { modmatrix::Source::Aftertouch, modmatrix::Destination::EqMidFreq, 0.12f };
    state.slots[7] = { modmatrix::Source::LFO1,       modmatrix::Destination::Cutoff,    0.15f };
    return state;
}

PresetXmlData makeDefaultPresetState(const int instrIndex)
{
    PresetXmlData state;
    state.instrIndex = juce::jlimit(0, mpc::kNumInstruments - 1, instrIndex);
    state.settings = mpc::getDefaultSettings(state.instrIndex);
    state.fx = mpc::GlobalFxSettings{};
    state.outputBus = 0;
    state.qualityMode = 0;
    state.delaySync = 0;
    state.delayDivision = 1;
    state.lfoRate = 1.8f;
    state.lfoDepth = 0.0f;
    state.lfoWave = 0;
    state.macroImpact = 0.5f;
    state.macroResonance = 0.5f;
    state.macroSpace = 0.5f;
    state.macroCouleur = 0.5f;
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

int readInstrumentIndexAttribute(const juce::XmlElement& xml, const int fallback)
{
    if (xml.hasAttribute("instrIndex"))
        return xml.getIntAttribute("instrIndex", fallback);
    if (xml.hasAttribute("instrument_index"))
        return xml.getIntAttribute("instrument_index", fallback);
    if (xml.hasAttribute("instrumentIndex"))
        return xml.getIntAttribute("instrumentIndex", fallback);
    if (xml.hasAttribute("instr"))
        return xml.getIntAttribute("instr", fallback);
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

void writeGlobalSettingsAttributes(juce::XmlElement& root, const PresetXmlData& preset)
{
    root.setAttribute("quality_mode", juce::jlimit(0, 1, preset.qualityMode));
    root.setAttribute("delay_sync", juce::jlimit(0, 1, preset.delaySync));
    root.setAttribute("delay_division", juce::jlimit(0, 5, preset.delayDivision));
    root.setAttribute("lfo_rate", static_cast<double>(juce::jlimit(0.05f, 12.0f, preset.lfoRate)));
    root.setAttribute("lfo_depth", static_cast<double>(juce::jlimit(0.0f, 1.0f, preset.lfoDepth)));
    root.setAttribute("lfo_wave", juce::jlimit(0, 3, preset.lfoWave));
    root.setAttribute("macro_impact", static_cast<double>(clamp01(preset.macroImpact)));
    root.setAttribute("macro_resonance", static_cast<double>(clamp01(preset.macroResonance)));
    root.setAttribute("macro_space", static_cast<double>(clamp01(preset.macroSpace)));
    root.setAttribute("macro_couleur", static_cast<double>(clamp01(preset.macroCouleur)));
}

void readGlobalSettingsAttributes(const juce::XmlElement& xml, PresetXmlData& preset)
{
    preset.qualityMode = juce::jlimit(0, 1, xml.getIntAttribute("quality_mode", preset.qualityMode));
    preset.delaySync = juce::jlimit(0, 1, xml.getIntAttribute("delay_sync", preset.delaySync));
    preset.delayDivision = juce::jlimit(0, 5, xml.getIntAttribute("delay_division", preset.delayDivision));
    preset.lfoRate = readFiniteXmlFloat(xml, "lfo_rate", preset.lfoRate, 0.05f, 12.0f);
    preset.lfoDepth = readFiniteXmlFloat(xml, "lfo_depth", preset.lfoDepth, 0.0f, 1.0f);
    preset.lfoWave = juce::jlimit(0, 3, xml.getIntAttribute("lfo_wave", preset.lfoWave));
    preset.macroImpact = readFiniteXmlFloat(xml, "macro_impact", preset.macroImpact, 0.0f, 1.0f);
    preset.macroResonance = readFiniteXmlFloat(xml, "macro_resonance", preset.macroResonance, 0.0f, 1.0f);
    preset.macroSpace = readFiniteXmlFloat(xml, "macro_space", preset.macroSpace, 0.0f, 1.0f);
    preset.macroCouleur = readFiniteXmlFloat(xml, "macro_couleur", preset.macroCouleur, 0.0f, 1.0f);
}

void writeMetadataAttributes(juce::XmlElement& root, const PresetMetadataData& metadata)
{
    root.setAttribute("mix_role", metadata.mixRole);
    root.setAttribute("family", metadata.family);
    root.setAttribute("tags", metadata.tags);
    root.setAttribute("description", metadata.description);
    root.setAttribute("output_profile", metadata.outputProfile);
    root.setAttribute("nominal_peak_db", static_cast<double>(metadata.nominalPeakDb));
}

void readMetadataAttributes(const juce::XmlElement& xml, PresetMetadataData& metadata)
{
    metadata.mixRole = readStringAttribute(xml, { "mix_role" }, metadata.mixRole);
    metadata.family = readStringAttribute(xml, { "family" }, metadata.family);
    metadata.tags = readStringAttribute(xml, { "tags" }, metadata.tags);
    metadata.description = readStringAttribute(xml, { "description" }, metadata.description);
    metadata.outputProfile = readStringAttribute(xml, { "output_profile" }, metadata.outputProfile);
    metadata.nominalPeakDb = readFiniteXmlFloat(xml, "nominal_peak_db", metadata.nominalPeakDb, -24.0f, -1.0f);
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
            *hasCompleteSchema = matrix->hasAttribute("pbRange")
                && matrix->hasAttribute("lfo2Rate")
                && matrix->hasAttribute("lfo2Wave")
                && std::all_of(seen.begin(), seen.end(), [] (const bool value) { return value; });
        return true;
    }

    if (hasCompleteSchema != nullptr)
        *hasCompleteSchema = false;
    return false;
}

void writeInstrSettingsAttributes(juce::XmlElement& root, const mpc::InstrSettings& s)
{
    root.setAttribute("level",        static_cast<double>(s.level));
    root.setAttribute("tune",         static_cast<double>(s.tuneSemitones));
    root.setAttribute("brightness",   static_cast<double>(s.brightness));
    root.setAttribute("attack",       static_cast<double>(s.attackSeconds));
    root.setAttribute("decay",        static_cast<double>(s.decaySeconds));
    root.setAttribute("sustain",      static_cast<double>(s.sustainLevel));
    root.setAttribute("release",      static_cast<double>(s.releaseSeconds));
    root.setAttribute("damping",      static_cast<double>(s.damping));
    root.setAttribute("body",         static_cast<double>(s.body));
    root.setAttribute("noise",        static_cast<double>(s.noise));
    root.setAttribute("stereo_width", static_cast<double>(s.stereoWidth));
    root.setAttribute("color",        static_cast<double>(s.color));
    root.setAttribute("cutoff",       static_cast<double>(s.cutoffHz));
    root.setAttribute("pan",          static_cast<double>(s.pan));
    root.setAttribute("oneShot",      s.oneShot ? 1 : 0);
    root.setAttribute("oneShotDecayMs",static_cast<double>(s.oneShotDecayMs));
}

void writeFxSettingsAttributes(juce::XmlElement& root, const mpc::GlobalFxSettings& f)
{
    root.setAttribute("fx_sat_drive",          static_cast<double>(f.satDrive));
    root.setAttribute("fx_sat_mix",            static_cast<double>(f.satMix));
    root.setAttribute("fx_transient_attack",   static_cast<double>(f.transientAttack));
    root.setAttribute("fx_transient_sustain",  static_cast<double>(f.transientSustain));
    root.setAttribute("fx_transient_mix",      static_cast<double>(f.transientMix));
    root.setAttribute("fx_comp_threshold",     static_cast<double>(f.compThreshold));
    root.setAttribute("fx_comp_ratio",         static_cast<double>(f.compRatio));
    root.setAttribute("fx_comp_attack",        static_cast<double>(f.compAttack));
    root.setAttribute("fx_comp_release",       static_cast<double>(f.compRelease));
    root.setAttribute("fx_comp_makeup",        static_cast<double>(f.compMakeup));
    root.setAttribute("fx_comp_mix",           static_cast<double>(f.compMix));
    root.setAttribute("fx_eq_low_freq",        static_cast<double>(f.eqLowFreq));
    root.setAttribute("fx_eq_low_gain",        static_cast<double>(f.eqLowGain));
    root.setAttribute("fx_eq_mid_freq",        static_cast<double>(f.eqMidFreq));
    root.setAttribute("fx_eq_mid_gain",        static_cast<double>(f.eqMidGain));
    root.setAttribute("fx_eq_mid_q",           static_cast<double>(f.eqMidQ));
    root.setAttribute("fx_eq_high_freq",       static_cast<double>(f.eqHighFreq));
    root.setAttribute("fx_eq_high_gain",       static_cast<double>(f.eqHighGain));
    root.setAttribute("fx_chorus_rate",        static_cast<double>(f.chorusRate));
    root.setAttribute("fx_chorus_depth",       static_cast<double>(f.chorusDepth));
    root.setAttribute("fx_chorus_mix",         static_cast<double>(f.chorusMix));
    root.setAttribute("fx_delay_time",         static_cast<double>(f.delayTime));
    root.setAttribute("fx_delay_feedback",     static_cast<double>(f.delayFeedback));
    root.setAttribute("fx_delay_mix",          static_cast<double>(f.delayMix));
    root.setAttribute("fx_reverb_size",        static_cast<double>(f.reverbSize));
    root.setAttribute("fx_reverb_damping",     static_cast<double>(f.reverbDamping));
    root.setAttribute("fx_reverb_width",       static_cast<double>(f.reverbWidth));
    root.setAttribute("fx_reverb_mix",         static_cast<double>(f.reverbMix));
    root.setAttribute("fx_reverb_predelay",    static_cast<double>(f.reverbPredelay));
    root.setAttribute("fx_limiter_threshold",  static_cast<double>(f.limiterThreshold));
    root.setAttribute("fx_limiter_release",    static_cast<double>(f.limiterRelease));
    root.setAttribute("fx_tab0_en",            f.saturatorOn);
    root.setAttribute("fx_tab1_en",            f.transientOn);
    root.setAttribute("fx_eq_en",              f.eqOn);
    root.setAttribute("fx_tab2_en",            f.compressorOn);
    root.setAttribute("fx_chorus_en",          f.chorusOn);
    root.setAttribute("fx_delay_en",           f.delayOn);
    root.setAttribute("fx_tab3_en",            f.reverbOn);
    root.setAttribute("fx_limiter_en",         f.limiterOn);
}

void readFxSettingsAttributes(const juce::XmlElement& xml, mpc::GlobalFxSettings& f)
{
    auto rd = [&](const char* name, float& val)
    {
        if (xml.hasAttribute(name))
        {
            auto raw = xml.getDoubleAttribute(name);
            if (std::isfinite(raw)) val = static_cast<float>(raw);
        }
    };
    rd("fx_sat_drive",          f.satDrive);
    rd("fx_sat_mix",            f.satMix);
    rd("fx_transient_attack",   f.transientAttack);
    rd("fx_transient_sustain",  f.transientSustain);
    rd("fx_transient_mix",      f.transientMix);
    rd("fx_comp_threshold",     f.compThreshold);
    rd("fx_comp_ratio",         f.compRatio);
    rd("fx_comp_attack",        f.compAttack);
    rd("fx_comp_release",       f.compRelease);
    rd("fx_comp_makeup",        f.compMakeup);
    rd("fx_comp_mix",           f.compMix);
    rd("fx_eq_low_freq",        f.eqLowFreq);
    rd("fx_eq_low_gain",        f.eqLowGain);
    rd("fx_eq_mid_freq",        f.eqMidFreq);
    rd("fx_eq_mid_gain",        f.eqMidGain);
    rd("fx_eq_mid_q",           f.eqMidQ);
    rd("fx_eq_high_freq",       f.eqHighFreq);
    rd("fx_eq_high_gain",       f.eqHighGain);
    rd("fx_chorus_rate",        f.chorusRate);
    rd("fx_chorus_depth",       f.chorusDepth);
    rd("fx_chorus_mix",         f.chorusMix);
    rd("fx_delay_time",         f.delayTime);
    rd("fx_delay_feedback",     f.delayFeedback);
    rd("fx_delay_mix",          f.delayMix);
    rd("fx_reverb_size",        f.reverbSize);
    rd("fx_reverb_damping",     f.reverbDamping);
    rd("fx_reverb_width",       f.reverbWidth);
    rd("fx_reverb_mix",         f.reverbMix);
    rd("fx_reverb_predelay",    f.reverbPredelay);
    rd("fx_limiter_threshold",  f.limiterThreshold);
    rd("fx_limiter_release",    f.limiterRelease);

    f.saturatorOn  = static_cast<bool>(xml.getBoolAttribute("fx_tab0_en", f.saturatorOn));
    f.transientOn  = static_cast<bool>(xml.getBoolAttribute("fx_tab1_en", f.transientOn));
    f.eqOn         = static_cast<bool>(xml.getBoolAttribute("fx_eq_en", f.eqOn));
    f.compressorOn = static_cast<bool>(xml.getBoolAttribute("fx_tab2_en", f.compressorOn));
    f.chorusOn     = static_cast<bool>(xml.getBoolAttribute("fx_chorus_en", f.chorusOn));
    f.delayOn      = static_cast<bool>(xml.getBoolAttribute("fx_delay_en", f.delayOn));
    f.reverbOn     = static_cast<bool>(xml.getBoolAttribute("fx_tab3_en", f.reverbOn));
    f.limiterOn    = static_cast<bool>(xml.getBoolAttribute("fx_limiter_en", f.limiterOn));
}

void writeFxStateProperties(juce::ValueTree& state, const mpc::GlobalFxSettings& fx)
{
    state.setProperty("sat_drive", fx.satDrive, nullptr);
    state.setProperty("sat_mix", fx.satMix, nullptr);
    state.setProperty("transient_attack", fx.transientAttack, nullptr);
    state.setProperty("transient_sustain", fx.transientSustain, nullptr);
    state.setProperty("transient_mix", fx.transientMix, nullptr);
    state.setProperty("comp_threshold", fx.compThreshold, nullptr);
    state.setProperty("comp_ratio", fx.compRatio, nullptr);
    state.setProperty("comp_attack", fx.compAttack, nullptr);
    state.setProperty("comp_release", fx.compRelease, nullptr);
    state.setProperty("comp_makeup", fx.compMakeup, nullptr);
    state.setProperty("comp_mix", fx.compMix, nullptr);
    state.setProperty("eq_low_freq", fx.eqLowFreq, nullptr);
    state.setProperty("eq_low_gain", fx.eqLowGain, nullptr);
    state.setProperty("eq_mid_freq", fx.eqMidFreq, nullptr);
    state.setProperty("eq_mid_gain", fx.eqMidGain, nullptr);
    state.setProperty("eq_mid_q", fx.eqMidQ, nullptr);
    state.setProperty("eq_high_freq", fx.eqHighFreq, nullptr);
    state.setProperty("eq_high_gain", fx.eqHighGain, nullptr);
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

mpc::GlobalFxSettings readFxStateProperties(const juce::ValueTree& state,
                                            mpc::GlobalFxSettings fallback)
{
    auto fx = fallback;
    fx.satDrive          = static_cast<float>(state.getProperty("sat_drive", fx.satDrive));
    fx.satMix            = static_cast<float>(state.getProperty("sat_mix", fx.satMix));
    fx.transientAttack   = static_cast<float>(state.getProperty("transient_attack", fx.transientAttack));
    fx.transientSustain  = static_cast<float>(state.getProperty("transient_sustain", fx.transientSustain));
    fx.transientMix      = static_cast<float>(state.getProperty("transient_mix", fx.transientMix));
    fx.compThreshold     = static_cast<float>(state.getProperty("comp_threshold", fx.compThreshold));
    fx.compRatio         = static_cast<float>(state.getProperty("comp_ratio", fx.compRatio));
    fx.compAttack        = static_cast<float>(state.getProperty("comp_attack", fx.compAttack));
    fx.compRelease       = static_cast<float>(state.getProperty("comp_release", fx.compRelease));
    fx.compMakeup        = static_cast<float>(state.getProperty("comp_makeup", fx.compMakeup));
    fx.compMix           = static_cast<float>(state.getProperty("comp_mix", fx.compMix));
    fx.eqLowFreq         = static_cast<float>(state.getProperty("eq_low_freq", fx.eqLowFreq));
    fx.eqLowGain         = static_cast<float>(state.getProperty("eq_low_gain", fx.eqLowGain));
    fx.eqMidFreq         = static_cast<float>(state.getProperty("eq_mid_freq", fx.eqMidFreq));
    fx.eqMidGain         = static_cast<float>(state.getProperty("eq_mid_gain", fx.eqMidGain));
    fx.eqMidQ            = static_cast<float>(state.getProperty("eq_mid_q", fx.eqMidQ));
    fx.eqHighFreq        = static_cast<float>(state.getProperty("eq_high_freq", fx.eqHighFreq));
    fx.eqHighGain        = static_cast<float>(state.getProperty("eq_high_gain", fx.eqHighGain));
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

bool readFiniteAttribute(const juce::XmlElement& xml, const char* name, float& value)
{
    if (!xml.hasAttribute(name))
        return false;

    const auto raw = xml.getDoubleAttribute(name);
    if (!std::isfinite(raw))
        return false;

    value = static_cast<float>(raw);
    return std::isfinite(value);
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

bool validateInstrSettings(mpc::InstrSettings& s)
{
    if (!std::isfinite(s.level) || !std::isfinite(s.tuneSemitones) || !std::isfinite(s.brightness)
        || !std::isfinite(s.attackSeconds) || !std::isfinite(s.decaySeconds) || !std::isfinite(s.sustainLevel)
        || !std::isfinite(s.releaseSeconds) || !std::isfinite(s.damping) || !std::isfinite(s.body)
        || !std::isfinite(s.noise) || !std::isfinite(s.stereoWidth) || !std::isfinite(s.color)
        || !std::isfinite(s.cutoffHz) || !std::isfinite(s.pan))
    {
        return false;
    }

    const bool inRange = s.level >= 0.0f && s.level <= 1.0f
        && s.tuneSemitones >= -24.0f && s.tuneSemitones <= 24.0f
        && s.brightness >= 0.0f && s.brightness <= 1.0f
        && s.attackSeconds >= 0.0f && s.attackSeconds <= 2.0f
        && s.decaySeconds >= 0.1f && s.decaySeconds <= 10.0f
        && s.sustainLevel >= 0.0f && s.sustainLevel <= 1.0f
        && s.releaseSeconds >= 0.01f && s.releaseSeconds <= 5.0f
        && s.damping >= 0.0f && s.damping <= 1.0f
        && s.body >= 0.0f && s.body <= 1.0f
        && s.noise >= 0.0f && s.noise <= 1.0f
        && s.stereoWidth >= 0.0f && s.stereoWidth <= 1.0f
        && s.color >= 0.0f && s.color <= 1.0f
        && s.cutoffHz >= 120.0f && s.cutoffHz <= 16000.0f
        && s.pan >= -1.0f && s.pan <= 1.0f;

    return inRange;
}

bool parsePresetXml(const juce::XmlElement& xml,
                    const char* expectedTag,
                    const int expectedInstrIndex,
                    PresetXmlData& out,
                    const bool requirePresetIndex,
                    const PresetXmlData* fallback = nullptr)
{
    if (!xml.hasTagName(expectedTag))
        return false;

    out = fallback != nullptr ? *fallback : makeDefaultPresetState(expectedInstrIndex);
    out.instrIndex = expectedInstrIndex;
    out.name = readStringAttribute(xml, { "name" }, out.name).trim();
    out.instrIndex = readInstrumentIndexAttribute(xml, expectedInstrIndex);
    if (out.instrIndex != expectedInstrIndex)
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
    out.settings.level = readFiniteXmlFloat(xml, "level", out.settings.level, 0.0f, 1.0f, &warningCount);
    out.settings.tuneSemitones = readFiniteXmlFloat(xml, "tune", out.settings.tuneSemitones, -24.0f, 24.0f, &warningCount);
    out.settings.brightness = readFiniteXmlFloat(xml, "brightness", out.settings.brightness, 0.0f, 1.0f, &warningCount);
    out.settings.attackSeconds = readFiniteXmlFloat(xml, "attack", out.settings.attackSeconds, 0.0f, 2.0f, &warningCount);
    out.settings.decaySeconds = readFiniteXmlFloat(xml, "decay", out.settings.decaySeconds, 0.1f, 10.0f, &warningCount);
    out.settings.sustainLevel = readFiniteXmlFloat(xml, "sustain", out.settings.sustainLevel, 0.0f, 1.0f, &warningCount);
    out.settings.releaseSeconds = readFiniteXmlFloat(xml, "release", out.settings.releaseSeconds, 0.01f, 5.0f, &warningCount);
    out.settings.damping = readFiniteXmlFloat(xml, "damping", out.settings.damping, 0.0f, 1.0f, &warningCount);
    out.settings.body = readFiniteXmlFloat(xml, "body", out.settings.body, 0.0f, 1.0f, &warningCount);
    out.settings.noise = readFiniteXmlFloat(xml, "noise", out.settings.noise, 0.0f, 1.0f, &warningCount);
    out.settings.stereoWidth = readFiniteXmlFloat(xml, "stereo_width", out.settings.stereoWidth, 0.0f, 1.0f, &warningCount);
    out.settings.color = readFiniteXmlFloat(xml, "color", out.settings.color, 0.0f, 1.0f, &warningCount);
    out.settings.cutoffHz = readFiniteXmlFloat(xml, "cutoff", out.settings.cutoffHz, 120.0f, 16000.0f, &warningCount);
    out.settings.pan = readFiniteXmlFloat(xml, "pan", out.settings.pan, -1.0f, 1.0f, &warningCount);
    out.settings.oneShot = xml.getIntAttribute("oneShot", out.settings.oneShot ? 1 : 0) != 0;
    out.settings.oneShotDecayMs = readFiniteXmlFloat(xml, "oneShotDecayMs", out.settings.oneShotDecayMs, 10.0f, 500.0f, &warningCount);

    out.outputBus = juce::jlimit(0,
                                 PercSynthAudioProcessor::kNumAuxOutputs,
                                 xml.getIntAttribute("output", xml.getIntAttribute("output_bus", out.outputBus)));
    readFxSettingsAttributes(xml, out.fx);
    readGlobalSettingsAttributes(xml, out);
    readMetadataAttributes(xml, out.metadata);
    readModMatrixXml(xml, out.modMatrix);

    return validateInstrSettings(out.settings);
}

bool shouldRewritePresetXml(const juce::XmlElement& xml, const bool requirePresetIndex)
{
    if (readPresetFormatVersion(xml) < kPresetFormatVersion)
        return true;

    static constexpr const char* kRequiredAttributes[] = {
        "name", "version", "format_version", "synth_index", "instrIndex", "instrument_index",
        "level", "tune", "brightness", "attack", "decay", "sustain", "release", "damping",
        "body", "noise", "stereo_width", "color", "cutoff", "pan", "oneShot", "oneShotDecayMs", "output",
        "quality_mode", "delay_sync", "delay_division", "lfo_rate", "lfo_depth", "lfo_wave",
        "macro_impact", "macro_resonance", "macro_space", "macro_couleur",
        "fx_sat_drive", "fx_sat_mix", "fx_transient_attack", "fx_transient_sustain", "fx_transient_mix",
        "fx_comp_threshold", "fx_comp_ratio", "fx_comp_attack", "fx_comp_release", "fx_comp_makeup",
        "fx_comp_mix", "fx_eq_low_freq", "fx_eq_low_gain", "fx_eq_mid_freq", "fx_eq_mid_gain",
        "fx_eq_mid_q", "fx_eq_high_freq", "fx_eq_high_gain", "fx_chorus_rate", "fx_chorus_depth",
        "fx_chorus_mix", "fx_delay_time", "fx_delay_feedback", "fx_delay_mix", "fx_reverb_size",
        "fx_reverb_damping", "fx_reverb_width", "fx_reverb_mix", "fx_reverb_predelay",
        "fx_limiter_threshold", "fx_limiter_release", "fx_tab0_en", "fx_tab1_en", "fx_eq_en",
        "fx_tab2_en", "fx_chorus_en", "fx_delay_en", "fx_tab3_en", "fx_limiter_en",
        "mix_role", "family", "tags", "description", "output_profile", "nominal_peak_db"
    };

    for (const auto* attribute : kRequiredAttributes)
    {
        if (!xml.hasAttribute(attribute))
            return true;
    }

    if (requirePresetIndex && (!xml.hasAttribute("index") || !xml.hasAttribute("preset_index")))
        return true;

    bool hasCompleteMatrix = false;
    auto matrixState = makeDefaultModMatrixState();
    if (!readModMatrixXml(xml, matrixState, &hasCompleteMatrix) || !hasCompleteMatrix)
        return true;

    return false;
}

std::unique_ptr<juce::XmlElement> createPresetXml(const juce::String& tagName,
                                                  const PresetXmlData& preset)
{
    auto root = std::make_unique<juce::XmlElement>(tagName);
    root->setAttribute("version", kPresetFormatVersion);
    root->setAttribute("format_version", kPresetFormatVersion);
    root->setAttribute("name", preset.name);
    root->setAttribute("synth_index", kPercSynthIndex);
    root->setAttribute("instrIndex", preset.instrIndex);
    root->setAttribute("instrument_index", preset.instrIndex);
    if (preset.presetIndex >= 0)
    {
        root->setAttribute("index", preset.presetIndex);
        root->setAttribute("preset_index", preset.presetIndex);
    }

    writeInstrSettingsAttributes(*root, preset.settings);
    root->setAttribute("output", juce::jlimit(0, PercSynthAudioProcessor::kNumAuxOutputs, preset.outputBus));
    writeGlobalSettingsAttributes(*root, preset);
    writeFxSettingsAttributes(*root, preset.fx);
    writeMetadataAttributes(*root, preset.metadata);
    writeModMatrixXml(*root, preset.modMatrix);
    return root;
}
} // namespace

// =============================================================================
auto PercSynthAudioProcessor::createBusLayout() -> BusesProperties
{
    BusesProperties buses;
    buses = buses.withOutput("Master", juce::AudioChannelSet::stereo(), true);
    for (int i = 0; i < kNumAuxOutputs; ++i)
        buses = buses.withOutput("Instr " + juce::String(i + 1) + " Out",
                                 juce::AudioChannelSet::stereo(), false);
    return buses;
}

// =============================================================================
void PercSynthAudioProcessor::applyInstrPresetSettings(int instrIndex, const mpc::InstrSettings& s, bool notifyHost)
{
    const auto sanitized = sanitizeInstrSettings(instrIndex, s);
    setParamValueInternal(makeInstrParamId(instrIndex, "level"),        sanitized.level, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "tune"),         sanitized.tuneSemitones, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "brightness"),   sanitized.brightness, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "attack"),       sanitized.attackSeconds, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "decay"),        sanitized.decaySeconds, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "sustain"),      sanitized.sustainLevel, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "release"),      sanitized.releaseSeconds, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "damping"),      sanitized.damping, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "body"),         sanitized.body, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "noise"),        sanitized.noise, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "stereo_width"), sanitized.stereoWidth, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "color"),        sanitized.color, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "cutoff"),       sanitized.cutoffHz, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, "pan"),          sanitized.pan, notifyHost);
}

// =============================================================================
PercSynthAudioProcessor::PercSynthAudioProcessor()
    : AudioProcessor(createBusLayout()),
      parameters(*this, nullptr, juce::Identifier("MPC_PARAMS"), createParameterLayout()),
      factoryPresetBanks(mpc::getFactoryPresetBanks())
{
    resolveParameterPointers();
    rebuildMidiCCBindings();
    currentPresetIndices.fill(0);
    currentUserPresetFiles.fill(juce::File{});
    modulationMatrix.applyState(makeDefaultModMatrixState());

    for (int i = 0; i < mpc::kNumInstruments; ++i)
    {
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(i)];
        auto& persistedBank = factoryPresetStates[static_cast<std::size_t>(i)];
        persistedBank.clear();
        persistedBank.reserve(bank.size());
        for (int presetIndex = 0; presetIndex < static_cast<int>(bank.size()); ++presetIndex)
            persistedBank.push_back(makeFactoryPresetState(i, presetIndex, bank[static_cast<std::size_t>(presetIndex)]));
    }

    loadFactoryOverrides();
    backfillAllUserPresetLibraries();
    for (int i = 0; i < mpc::kNumInstruments; ++i)
    {
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(i)];
        instrumentFxStates[static_cast<std::size_t>(i)] = bank.empty()
            ? mpc::GlobalFxSettings{}
            : bank[0].fx;

        if (!factoryPresetBanks[static_cast<std::size_t>(i)].empty())
        {
            applyInstrPresetSettings(i, factoryPresetBanks[static_cast<std::size_t>(i)][0].settings, false);
            setParamValueInternal(makeInstrParamId(i, kInstrOutputSuffix),
                                  static_cast<float>(factoryPresetBanks[static_cast<std::size_t>(i)][0].outputBus), false);
            outputBusCache[static_cast<std::size_t>(i)] =
                juce::jlimit(0, kNumAuxOutputs, factoryPresetBanks[static_cast<std::size_t>(i)][0].outputBus);
        }
    }

    parameters.addParameterListener(kSelectedInstr, this);
    for (int i = 0; i < mpc::kNumInstruments; ++i)
        parameters.addParameterListener(makeInstrParamId(i, kInstrOutputSuffix), this);
    cachedSelectedInstrumentIndex = getSelectedInstrIndex();
    pendingSelectedInstrumentIndex.store(cachedSelectedInstrumentIndex);
    if (!factoryPresetStates[static_cast<std::size_t>(cachedSelectedInstrumentIndex)].empty())
        applyPresetPersistenceState(factoryPresetStates[static_cast<std::size_t>(cachedSelectedInstrumentIndex)][0], false);
    else
        applyFxToParams(cachedSelectedInstrumentIndex,
                        instrumentFxStates[static_cast<std::size_t>(cachedSelectedInstrumentIndex)], false);
    sanitizeAllParameters();
    markOutputBusCacheDirty();
}

PercSynthAudioProcessor::~PercSynthAudioProcessor()
{
    cancelPendingUpdate();
    parameters.removeParameterListener(kSelectedInstr, this);
    for (int i = 0; i < mpc::kNumInstruments; ++i)
        parameters.removeParameterListener(makeInstrParamId(i, kInstrOutputSuffix), this);
}

// =============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
PercSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const auto& banks = mpc::getFactoryPresetBanks();
    const auto outputChoices = makeOutputChoices();
    const auto qualityChoices = makeQualityChoices();
    const auto delaySyncChoices = makeDelaySyncChoices();
    const auto delayDivisionChoices = makeDelayDivisionChoices();

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kOutputGain, "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 12.0f, 0.01f), -5.0f));

    juce::StringArray instrChoices;
    for (int i = 0; i < mpc::kNumInstruments; ++i)
        instrChoices.add(mpc::getInstrName(i));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kSelectedInstr, "Selected Instrument", instrChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kQualityMode, "Quality Mode", qualityChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kDelaySync, "Delay Sync", delaySyncChoices, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kDelayDivision, "Delay Division", delayDivisionChoices, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoRate, "LFO Rate",
        juce::NormalisableRange<float>(0.05f, 12.0f, 0.0001f), 1.8f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLfoDepth, "LFO Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        kLfoWave, "LFO Wave",
        juce::StringArray{ "Sine", "Triangle", "Saw", "Square" }, 0));

    // Macros
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroImpact, "Transient",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroResonance, "Body",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroSpace, "Space",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kMacroCouleur, "Color",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));

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
        kCompMakeup, "Comp Makeup",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.01f), 0.0f));
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

    // FX: EQ
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqLowFreq, "EQ Low Freq",
        juce::NormalisableRange<float>(40.0f, 600.0f, 0.1f, 0.5f), 200.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqLowGain, "EQ Low Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidFreq, "EQ Mid Freq",
        juce::NormalisableRange<float>(200.0f, 8000.0f, 0.1f, 0.4f), 1000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidGain, "EQ Mid Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqMidQ, "EQ Mid Q",
        juce::NormalisableRange<float>(0.1f, 10.0f, 0.01f, 0.5f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqHighFreq, "EQ High Freq",
        juce::NormalisableRange<float>(1000.0f, 16000.0f, 0.1f, 0.4f), 5000.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kEqHighGain, "EQ High Gain",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.01f), 0.0f));

    // FX: Chorus
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusRate, "Chorus Rate",
        juce::NormalisableRange<float>(0.1f, 5.0f, 0.01f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusDepth, "Chorus Depth",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kChorusMix, "Chorus Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));

    // FX: Delay
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayTime, "Delay Time",
        juce::NormalisableRange<float>(1.0f, 2000.0f, 0.1f, 0.35f), 300.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayFeedback, "Delay Feedback",
        juce::NormalisableRange<float>(0.0f, 0.95f, 0.001f), 0.30f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kDelayMix, "Delay Mix",
        juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), 0.0f));

    // FX: Limiter
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterThreshold, "Limiter Threshold",
        juce::NormalisableRange<float>(-12.0f, 0.0f, 0.01f), -0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        kLimiterRelease, "Limiter Release",
        juce::NormalisableRange<float>(1.0f, 200.0f, 0.1f), 50.0f));

    // FX enable toggles (true = active)
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxSatEnable, "FX Saturator Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxTransientEnable, "FX Transient Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxCompEnable, "FX Compressor Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxReverbEnable, "FX Reverb Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxEqEnable, "FX EQ Enable", true));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxChorusEnable, "FX Chorus Enable", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxDelayEnable, "FX Delay Enable", false));
    layout.add(std::make_unique<juce::AudioParameterBool>(kFxLimiterEnable, "FX Limiter Enable", true));

    // Per-instrument parameters (9 instruments x 14 + output)
    for (int b = 0; b < mpc::kNumInstruments; ++b)
    {
        const auto& def = banks[static_cast<std::size_t>(b)].empty()
                        ? mpc::getDefaultSettings(b)
                        : banks[static_cast<std::size_t>(b)][0].settings;
        const auto prefix = juce::String(mpc::getInstrName(b)) + " ";

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
            makeInstrParamId(b, "damping"), prefix + "Damping",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.damping));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "body"), prefix + "Body",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.body));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "noise"), prefix + "Noise",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.noise));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "stereo_width"), prefix + "Stereo Width",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.stereoWidth));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "color"), prefix + "Color",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.0001f), def.color));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "cutoff"), prefix + "Cutoff",
            juce::NormalisableRange<float>(120.0f, 16000.0f, 0.0f, 0.28f), def.cutoffHz));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "pan"), prefix + "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), def.pan));
        layout.add(std::make_unique<juce::AudioParameterBool>(
            makeInstrParamId(b, "oneShot"), prefix + "One-Shot",
            def.oneShot));
        layout.add(std::make_unique<juce::AudioParameterFloat>(
            makeInstrParamId(b, "oneShotDecayMs"), prefix + "One-Shot Decay",
            juce::NormalisableRange<float>(10.0f, 500.0f, 0.1f), def.oneShotDecayMs));

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            makeInstrParamId(b, kInstrOutputSuffix), prefix + "Output",
            outputChoices, 0));
    }

    return layout;
}

juce::String PercSynthAudioProcessor::makeInstrParamId(int instrIndex, const juce::String& suffix)
{
    return "instr_" + juce::String(instrIndex) + "_" + suffix;
}

float PercSynthAudioProcessor::readCachedParamValue(const ParamBinding& binding, float fallback) const noexcept
{
    return binding.raw != nullptr ? binding.raw->load() : fallback;
}

void PercSynthAudioProcessor::resolveParameterPointers()
{
    auto bindParam = [this](const juce::String& paramId)
    {
        ParamBinding binding;
        binding.raw = parameters.getRawParameterValue(paramId);
        binding.ranged = dynamic_cast<juce::RangedAudioParameter*>(parameters.getParameter(paramId));
        return binding;
    };

    globalParamRefs.selectedInstr = bindParam(kSelectedInstr);
    globalParamRefs.outputGain = bindParam(kOutputGain);
    globalParamRefs.qualityMode = bindParam(kQualityMode);
    globalParamRefs.delaySync = bindParam(kDelaySync);
    globalParamRefs.delayDivision = bindParam(kDelayDivision);
    globalParamRefs.lfoRate = bindParam(kLfoRate);
    globalParamRefs.lfoDepth = bindParam(kLfoDepth);
    globalParamRefs.lfoWave = bindParam(kLfoWave);
    globalParamRefs.macroImpact = bindParam(kMacroImpact);
    globalParamRefs.macroResonance = bindParam(kMacroResonance);
    globalParamRefs.macroSpace = bindParam(kMacroSpace);
    globalParamRefs.macroCouleur = bindParam(kMacroCouleur);
    globalParamRefs.compThreshold = bindParam(kCompThreshold);
    globalParamRefs.compRatio = bindParam(kCompRatio);
    globalParamRefs.compAttack = bindParam(kCompAttack);
    globalParamRefs.compRelease = bindParam(kCompRelease);
    globalParamRefs.compMakeup = bindParam(kCompMakeup);
    globalParamRefs.compMix = bindParam(kCompMix);
    globalParamRefs.satDrive = bindParam(kSatDrive);
    globalParamRefs.satMix = bindParam(kSatMix);
    globalParamRefs.transientAttack = bindParam(kTransientAttack);
    globalParamRefs.transientSustain = bindParam(kTransientSustain);
    globalParamRefs.transientMix = bindParam(kTransientMix);
    globalParamRefs.reverbSize = bindParam(kReverbSize);
    globalParamRefs.reverbDamping = bindParam(kReverbDamping);
    globalParamRefs.reverbWidth = bindParam(kReverbWidth);
    globalParamRefs.reverbMix = bindParam(kReverbMix);
    globalParamRefs.reverbPredelay = bindParam(kReverbPredelay);
    globalParamRefs.eqLowFreq = bindParam(kEqLowFreq);
    globalParamRefs.eqLowGain = bindParam(kEqLowGain);
    globalParamRefs.eqMidFreq = bindParam(kEqMidFreq);
    globalParamRefs.eqMidGain = bindParam(kEqMidGain);
    globalParamRefs.eqMidQ = bindParam(kEqMidQ);
    globalParamRefs.eqHighFreq = bindParam(kEqHighFreq);
    globalParamRefs.eqHighGain = bindParam(kEqHighGain);
    globalParamRefs.chorusRate = bindParam(kChorusRate);
    globalParamRefs.chorusDepth = bindParam(kChorusDepth);
    globalParamRefs.chorusMix = bindParam(kChorusMix);
    globalParamRefs.delayTime = bindParam(kDelayTime);
    globalParamRefs.delayFeedback = bindParam(kDelayFeedback);
    globalParamRefs.delayMix = bindParam(kDelayMix);
    globalParamRefs.limiterThreshold = bindParam(kLimiterThreshold);
    globalParamRefs.limiterRelease = bindParam(kLimiterRelease);
    globalParamRefs.fxSatEnable = bindParam(kFxSatEnable);
    globalParamRefs.fxTransientEnable = bindParam(kFxTransientEnable);
    globalParamRefs.fxCompEnable = bindParam(kFxCompEnable);
    globalParamRefs.fxReverbEnable = bindParam(kFxReverbEnable);
    globalParamRefs.fxEqEnable = bindParam(kFxEqEnable);
    globalParamRefs.fxChorusEnable = bindParam(kFxChorusEnable);
    globalParamRefs.fxDelayEnable = bindParam(kFxDelayEnable);
    globalParamRefs.fxLimiterEnable = bindParam(kFxLimiterEnable);

    for (int instrIndex = 0; instrIndex < mpc::kNumInstruments; ++instrIndex)
    {
        auto& refs = instrParamRefs[static_cast<std::size_t>(instrIndex)];
        refs.level = bindParam(makeInstrParamId(instrIndex, "level"));
        refs.tune = bindParam(makeInstrParamId(instrIndex, "tune"));
        refs.brightness = bindParam(makeInstrParamId(instrIndex, "brightness"));
        refs.attack = bindParam(makeInstrParamId(instrIndex, "attack"));
        refs.decay = bindParam(makeInstrParamId(instrIndex, "decay"));
        refs.sustain = bindParam(makeInstrParamId(instrIndex, "sustain"));
        refs.release = bindParam(makeInstrParamId(instrIndex, "release"));
        refs.damping = bindParam(makeInstrParamId(instrIndex, "damping"));
        refs.body = bindParam(makeInstrParamId(instrIndex, "body"));
        refs.noise = bindParam(makeInstrParamId(instrIndex, "noise"));
        refs.stereoWidth = bindParam(makeInstrParamId(instrIndex, "stereo_width"));
        refs.color = bindParam(makeInstrParamId(instrIndex, "color"));
        refs.cutoff = bindParam(makeInstrParamId(instrIndex, "cutoff"));
        refs.pan = bindParam(makeInstrParamId(instrIndex, "pan"));
        refs.output = bindParam(makeInstrParamId(instrIndex, kInstrOutputSuffix));
        refs.oneShot = bindParam(makeInstrParamId(instrIndex, "oneShot"));
        refs.oneShotDecayMs = bindParam(makeInstrParamId(instrIndex, "oneShotDecayMs"));
    }
}

void PercSynthAudioProcessor::rebuildMidiCCBindings() noexcept
{
    for (auto& page : ccGlobalBindings)
        page.fill(nullptr);
    for (auto& instrumentPages : ccInstrumentBindings)
        for (auto& page : instrumentPages)
            page.fill(nullptr);

    auto bindGlobal = [this](const char* paramId) -> juce::RangedAudioParameter*
    {
        if (std::strcmp(paramId, kMacroImpact) == 0) return globalParamRefs.macroImpact.ranged;
        if (std::strcmp(paramId, kMacroResonance) == 0) return globalParamRefs.macroResonance.ranged;
        if (std::strcmp(paramId, kMacroSpace) == 0) return globalParamRefs.macroSpace.ranged;
        if (std::strcmp(paramId, kMacroCouleur) == 0) return globalParamRefs.macroCouleur.ranged;
        if (std::strcmp(paramId, kLfoRate) == 0) return globalParamRefs.lfoRate.ranged;
        if (std::strcmp(paramId, kLfoDepth) == 0) return globalParamRefs.lfoDepth.ranged;
        if (std::strcmp(paramId, kOutputGain) == 0) return globalParamRefs.outputGain.ranged;
        if (std::strcmp(paramId, kCompThreshold) == 0) return globalParamRefs.compThreshold.ranged;
        if (std::strcmp(paramId, kCompRatio) == 0) return globalParamRefs.compRatio.ranged;
        if (std::strcmp(paramId, kCompAttack) == 0) return globalParamRefs.compAttack.ranged;
        if (std::strcmp(paramId, kCompRelease) == 0) return globalParamRefs.compRelease.ranged;
        if (std::strcmp(paramId, kCompMakeup) == 0) return globalParamRefs.compMakeup.ranged;
        if (std::strcmp(paramId, kCompMix) == 0) return globalParamRefs.compMix.ranged;
        if (std::strcmp(paramId, kTransientAttack) == 0) return globalParamRefs.transientAttack.ranged;
        if (std::strcmp(paramId, kTransientSustain) == 0) return globalParamRefs.transientSustain.ranged;
        if (std::strcmp(paramId, kTransientMix) == 0) return globalParamRefs.transientMix.ranged;
        if (std::strcmp(paramId, kSatDrive) == 0) return globalParamRefs.satDrive.ranged;
        if (std::strcmp(paramId, kSatMix) == 0) return globalParamRefs.satMix.ranged;
        if (std::strcmp(paramId, kEqLowFreq) == 0) return globalParamRefs.eqLowFreq.ranged;
        if (std::strcmp(paramId, kEqLowGain) == 0) return globalParamRefs.eqLowGain.ranged;
        if (std::strcmp(paramId, kEqMidFreq) == 0) return globalParamRefs.eqMidFreq.ranged;
        if (std::strcmp(paramId, kEqMidGain) == 0) return globalParamRefs.eqMidGain.ranged;
        if (std::strcmp(paramId, kEqMidQ) == 0) return globalParamRefs.eqMidQ.ranged;
        if (std::strcmp(paramId, kEqHighFreq) == 0) return globalParamRefs.eqHighFreq.ranged;
        if (std::strcmp(paramId, kEqHighGain) == 0) return globalParamRefs.eqHighGain.ranged;
        if (std::strcmp(paramId, kChorusRate) == 0) return globalParamRefs.chorusRate.ranged;
        if (std::strcmp(paramId, kChorusDepth) == 0) return globalParamRefs.chorusDepth.ranged;
        if (std::strcmp(paramId, kChorusMix) == 0) return globalParamRefs.chorusMix.ranged;
        if (std::strcmp(paramId, kDelayTime) == 0) return globalParamRefs.delayTime.ranged;
        if (std::strcmp(paramId, kDelayFeedback) == 0) return globalParamRefs.delayFeedback.ranged;
        if (std::strcmp(paramId, kDelayMix) == 0) return globalParamRefs.delayMix.ranged;
        if (std::strcmp(paramId, kReverbSize) == 0) return globalParamRefs.reverbSize.ranged;
        if (std::strcmp(paramId, kReverbDamping) == 0) return globalParamRefs.reverbDamping.ranged;
        if (std::strcmp(paramId, kReverbWidth) == 0) return globalParamRefs.reverbWidth.ranged;
        if (std::strcmp(paramId, kReverbMix) == 0) return globalParamRefs.reverbMix.ranged;
        if (std::strcmp(paramId, kReverbPredelay) == 0) return globalParamRefs.reverbPredelay.ranged;
        if (std::strcmp(paramId, kLimiterThreshold) == 0) return globalParamRefs.limiterThreshold.ranged;
        if (std::strcmp(paramId, kLimiterRelease) == 0) return globalParamRefs.limiterRelease.ranged;
        return nullptr;
    };

    auto bindInstrument = [this](int instrIndex, const char* suffix) -> juce::RangedAudioParameter*
    {
        const auto& refs = instrParamRefs[static_cast<std::size_t>(juce::jlimit(0, mpc::kNumInstruments - 1, instrIndex))];
        if (std::strcmp(suffix, "attack") == 0) return refs.attack.ranged;
        if (std::strcmp(suffix, "decay") == 0) return refs.decay.ranged;
        if (std::strcmp(suffix, "sustain") == 0) return refs.sustain.ranged;
        if (std::strcmp(suffix, "release") == 0) return refs.release.ranged;
        if (std::strcmp(suffix, "damping") == 0) return refs.damping.ranged;
        if (std::strcmp(suffix, "level") == 0) return refs.level.ranged;
        if (std::strcmp(suffix, "tune") == 0) return refs.tune.ranged;
        if (std::strcmp(suffix, "brightness") == 0) return refs.brightness.ranged;
        if (std::strcmp(suffix, "body") == 0) return refs.body.ranged;
        if (std::strcmp(suffix, "noise") == 0) return refs.noise.ranged;
        if (std::strcmp(suffix, "stereo_width") == 0) return refs.stereoWidth.ranged;
        if (std::strcmp(suffix, "color") == 0) return refs.color.ranged;
        if (std::strcmp(suffix, "cutoff") == 0) return refs.cutoff.ranged;
        if (std::strcmp(suffix, "pan") == 0) return refs.pan.ranged;
        return nullptr;
    };

    for (int page = 0; page < kNumCCPages; ++page)
    {
        for (int knob = 0; knob < kNumMidiCCKnobs; ++knob)
        {
            const auto& slot = kCCPages[page][knob];
            if (slot.paramId != nullptr)
            {
                ccGlobalBindings[static_cast<std::size_t>(page)][static_cast<std::size_t>(knob)] = bindGlobal(slot.paramId);
                continue;
            }

            if (slot.instrSuffix != nullptr)
            {
                for (int instr = 0; instr < mpc::kNumInstruments; ++instr)
                    ccInstrumentBindings[static_cast<std::size_t>(instr)]
                        [static_cast<std::size_t>(page)]
                        [static_cast<std::size_t>(knob)] = bindInstrument(instr, slot.instrSuffix);
            }
        }
    }
}

void PercSynthAudioProcessor::markOutputBusCacheDirty() noexcept
{
    outputBusCacheDirty.store(true, std::memory_order_release);
}

void PercSynthAudioProcessor::refreshOutputBusCacheIfNeeded() noexcept
{
    if (!outputBusCacheDirty.exchange(false, std::memory_order_acq_rel))
        return;

    for (int instrumentIndex = 0; instrumentIndex < mpc::kNumInstruments; ++instrumentIndex)
        outputBusCache[static_cast<std::size_t>(instrumentIndex)] = captureInstrOutputBus(instrumentIndex);
}

float PercSynthAudioProcessor::sanitizeParameterValue(const juce::String& paramId,
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

void PercSynthAudioProcessor::setParamValueInternal(const juce::String& paramId, float value, bool notifyHost)
{
    if (auto* parameter = parameters.getParameter(paramId))
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter);
        const auto fallback = ranged != nullptr
            ? ranged->convertFrom0to1(ranged->getDefaultValue())
            : 0.0f;
        const auto sanitized = sanitizeParameterValue(paramId, value, fallback);
        const auto normalised = parameter->convertTo0to1(sanitized);

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

void PercSynthAudioProcessor::queueParamUpdate(juce::RangedAudioParameter* parameter, float normalisedValue)
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

int PercSynthAudioProcessor::captureInstrOutputBus(int instrIndex) const
{
    return juce::jlimit(0, kNumAuxOutputs,
                        static_cast<int>(std::round(readCachedParamValue(
                            instrParamRefs[static_cast<std::size_t>(instrIndex)].output))));
}

mpc::InstrSettings PercSynthAudioProcessor::sanitizeInstrSettings(int instrIndex, const mpc::InstrSettings& settings) const
{
    auto sanitized = settings;
    const auto defaults = mpc::getDefaultSettings(instrIndex);
    sanitized.level = sanitizeParameterValue(makeInstrParamId(instrIndex, "level"), sanitized.level, defaults.level);
    sanitized.tuneSemitones = sanitizeParameterValue(makeInstrParamId(instrIndex, "tune"), sanitized.tuneSemitones, defaults.tuneSemitones);
    sanitized.brightness = sanitizeParameterValue(makeInstrParamId(instrIndex, "brightness"), sanitized.brightness, defaults.brightness);
    sanitized.attackSeconds = sanitizeParameterValue(makeInstrParamId(instrIndex, "attack"), sanitized.attackSeconds, defaults.attackSeconds);
    sanitized.decaySeconds = sanitizeParameterValue(makeInstrParamId(instrIndex, "decay"), sanitized.decaySeconds, defaults.decaySeconds);
    sanitized.sustainLevel = sanitizeParameterValue(makeInstrParamId(instrIndex, "sustain"), sanitized.sustainLevel, defaults.sustainLevel);
    sanitized.releaseSeconds = sanitizeParameterValue(makeInstrParamId(instrIndex, "release"), sanitized.releaseSeconds, defaults.releaseSeconds);
    sanitized.damping = sanitizeParameterValue(makeInstrParamId(instrIndex, "damping"), sanitized.damping, defaults.damping);
    sanitized.body = sanitizeParameterValue(makeInstrParamId(instrIndex, "body"), sanitized.body, defaults.body);
    sanitized.noise = sanitizeParameterValue(makeInstrParamId(instrIndex, "noise"), sanitized.noise, defaults.noise);
    sanitized.stereoWidth = sanitizeParameterValue(makeInstrParamId(instrIndex, "stereo_width"), sanitized.stereoWidth, defaults.stereoWidth);
    sanitized.color = sanitizeParameterValue(makeInstrParamId(instrIndex, "color"), sanitized.color, defaults.color);
    sanitized.cutoffHz = sanitizeParameterValue(makeInstrParamId(instrIndex, "cutoff"), sanitized.cutoffHz, defaults.cutoffHz);
    sanitized.pan = sanitizeParameterValue(makeInstrParamId(instrIndex, "pan"), sanitized.pan, defaults.pan);
    return sanitized;
}

mpc::GlobalFxSettings PercSynthAudioProcessor::sanitizeFxSettings(const mpc::GlobalFxSettings& fx) const
{
    auto sanitized = fx;
    sanitized.satDrive = juce::jlimit(1.0f, 16.0f, sanitized.satDrive);
    sanitized.satMix = clamp01(sanitized.satMix);
    sanitized.transientAttack = juce::jlimit(-1.0f, 1.0f, sanitized.transientAttack);
    sanitized.transientSustain = juce::jlimit(-1.0f, 1.0f, sanitized.transientSustain);
    sanitized.transientMix = clamp01(sanitized.transientMix);
    sanitized.compThreshold = juce::jlimit(-60.0f, 0.0f, sanitized.compThreshold);
    sanitized.compRatio = juce::jlimit(1.0f, 20.0f, sanitized.compRatio);
    sanitized.compAttack = juce::jlimit(0.1f, 100.0f, sanitized.compAttack);
    sanitized.compRelease = juce::jlimit(5.0f, 500.0f, sanitized.compRelease);
    sanitized.compMakeup = juce::jlimit(0.0f, 24.0f, sanitized.compMakeup);
    sanitized.compMix = clamp01(sanitized.compMix);
    sanitized.eqLowFreq = juce::jlimit(40.0f, 600.0f, sanitized.eqLowFreq);
    sanitized.eqLowGain = juce::jlimit(-12.0f, 12.0f, sanitized.eqLowGain);
    sanitized.eqMidFreq = juce::jlimit(200.0f, 8000.0f, sanitized.eqMidFreq);
    sanitized.eqMidGain = juce::jlimit(-12.0f, 12.0f, sanitized.eqMidGain);
    sanitized.eqMidQ = juce::jlimit(0.1f, 10.0f, sanitized.eqMidQ);
    sanitized.eqHighFreq = juce::jlimit(1000.0f, 16000.0f, sanitized.eqHighFreq);
    sanitized.eqHighGain = juce::jlimit(-12.0f, 12.0f, sanitized.eqHighGain);
    sanitized.chorusRate = juce::jlimit(0.1f, 5.0f, sanitized.chorusRate);
    sanitized.chorusDepth = clamp01(sanitized.chorusDepth);
    sanitized.chorusMix = clamp01(sanitized.chorusMix);
    sanitized.delayTime = juce::jlimit(1.0f, 2000.0f, sanitized.delayTime);
    sanitized.delayFeedback = juce::jlimit(0.0f, 0.95f, sanitized.delayFeedback);
    sanitized.delayMix = clamp01(sanitized.delayMix);
    sanitized.reverbSize = clamp01(sanitized.reverbSize);
    sanitized.reverbDamping = clamp01(sanitized.reverbDamping);
    sanitized.reverbWidth = clamp01(sanitized.reverbWidth);
    sanitized.reverbMix = clamp01(sanitized.reverbMix);
    sanitized.reverbPredelay = juce::jlimit(0.0f, 100.0f, sanitized.reverbPredelay);
    sanitized.limiterThreshold = juce::jlimit(-12.0f, 0.0f, sanitized.limiterThreshold);
    sanitized.limiterRelease = juce::jlimit(1.0f, 200.0f, sanitized.limiterRelease);
    return sanitized;
}

void PercSynthAudioProcessor::sanitizeAllParameters()
{
    setParamValueInternal(kSelectedInstr, sanitizeParameterValue(kSelectedInstr,
        readCachedParamValue(globalParamRefs.selectedInstr), 0.0f), false);
    setParamValueInternal(kOutputGain, sanitizeParameterValue(kOutputGain,
        readCachedParamValue(globalParamRefs.outputGain), -5.0f), false);
    setParamValueInternal(kQualityMode, sanitizeParameterValue(kQualityMode,
        readCachedParamValue(globalParamRefs.qualityMode), 0.0f), false);
    setParamValueInternal(kDelaySync, sanitizeParameterValue(kDelaySync,
        readCachedParamValue(globalParamRefs.delaySync), 0.0f), false);
    setParamValueInternal(kDelayDivision, sanitizeParameterValue(kDelayDivision,
        readCachedParamValue(globalParamRefs.delayDivision), 1.0f), false);
    setParamValueInternal(kLfoRate, sanitizeParameterValue(kLfoRate,
        readCachedParamValue(globalParamRefs.lfoRate), 1.8f), false);
    setParamValueInternal(kLfoDepth, sanitizeParameterValue(kLfoDepth,
        readCachedParamValue(globalParamRefs.lfoDepth), 0.0f), false);
    setParamValueInternal(kLfoWave, sanitizeParameterValue(kLfoWave,
        readCachedParamValue(globalParamRefs.lfoWave), 0.0f), false);
    setParamValueInternal(kMacroImpact, sanitizeParameterValue(kMacroImpact,
        readCachedParamValue(globalParamRefs.macroImpact), 0.5f), false);
    setParamValueInternal(kMacroResonance, sanitizeParameterValue(kMacroResonance,
        readCachedParamValue(globalParamRefs.macroResonance), 0.5f), false);
    setParamValueInternal(kMacroSpace, sanitizeParameterValue(kMacroSpace,
        readCachedParamValue(globalParamRefs.macroSpace), 0.5f), false);
    setParamValueInternal(kMacroCouleur, sanitizeParameterValue(kMacroCouleur,
        readCachedParamValue(globalParamRefs.macroCouleur), 0.5f), false);

    for (int instrIndex = 0; instrIndex < mpc::kNumInstruments; ++instrIndex)
    {
        const auto defaults = mpc::getDefaultSettings(instrIndex);
        const auto& refs = instrParamRefs[static_cast<std::size_t>(instrIndex)];
        setParamValueInternal(makeInstrParamId(instrIndex, "level"), sanitizeParameterValue(makeInstrParamId(instrIndex, "level"), readCachedParamValue(refs.level), defaults.level), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "tune"), sanitizeParameterValue(makeInstrParamId(instrIndex, "tune"), readCachedParamValue(refs.tune), defaults.tuneSemitones), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "brightness"), sanitizeParameterValue(makeInstrParamId(instrIndex, "brightness"), readCachedParamValue(refs.brightness), defaults.brightness), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "attack"), sanitizeParameterValue(makeInstrParamId(instrIndex, "attack"), readCachedParamValue(refs.attack), defaults.attackSeconds), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "decay"), sanitizeParameterValue(makeInstrParamId(instrIndex, "decay"), readCachedParamValue(refs.decay), defaults.decaySeconds), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "sustain"), sanitizeParameterValue(makeInstrParamId(instrIndex, "sustain"), readCachedParamValue(refs.sustain), defaults.sustainLevel), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "release"), sanitizeParameterValue(makeInstrParamId(instrIndex, "release"), readCachedParamValue(refs.release), defaults.releaseSeconds), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "damping"), sanitizeParameterValue(makeInstrParamId(instrIndex, "damping"), readCachedParamValue(refs.damping), defaults.damping), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "body"), sanitizeParameterValue(makeInstrParamId(instrIndex, "body"), readCachedParamValue(refs.body), defaults.body), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "noise"), sanitizeParameterValue(makeInstrParamId(instrIndex, "noise"), readCachedParamValue(refs.noise), defaults.noise), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "stereo_width"), sanitizeParameterValue(makeInstrParamId(instrIndex, "stereo_width"), readCachedParamValue(refs.stereoWidth), defaults.stereoWidth), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "color"), sanitizeParameterValue(makeInstrParamId(instrIndex, "color"), readCachedParamValue(refs.color), defaults.color), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "cutoff"), sanitizeParameterValue(makeInstrParamId(instrIndex, "cutoff"), readCachedParamValue(refs.cutoff), defaults.cutoffHz), false);
        setParamValueInternal(makeInstrParamId(instrIndex, "pan"), sanitizeParameterValue(makeInstrParamId(instrIndex, "pan"), readCachedParamValue(refs.pan), defaults.pan), false);
        setParamValueInternal(makeInstrParamId(instrIndex, kInstrOutputSuffix), sanitizeParameterValue(makeInstrParamId(instrIndex, kInstrOutputSuffix), readCachedParamValue(refs.output), 0.0f), false);
        outputBusCache[static_cast<std::size_t>(instrIndex)] = captureInstrOutputBus(instrIndex);
    }

    applyFxToParams(getSelectedInstrIndex(), sanitizeFxSettings(snapshotFxSettings()), false);
    markOutputBusCacheDirty();
}

// =============================================================================
void PercSynthAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    preparedSampleRate = std::max(1.0, sampleRate);
    const int scratchSamples = juce::jmax(32768, samplesPerBlock);

    for (auto& slot : voices)
    {
        for (int instrIndex = 0; instrIndex < mpc::kNumInstruments; ++instrIndex)
        {
            if (!slot.voiceBank[static_cast<std::size_t>(instrIndex)])
                slot.voiceBank[static_cast<std::size_t>(instrIndex)] = mpc::createVoiceForInstrument(instrIndex);
            if (!slot.dyingVoiceBank[static_cast<std::size_t>(instrIndex)])
                slot.dyingVoiceBank[static_cast<std::size_t>(instrIndex)] = mpc::createVoiceForInstrument(instrIndex);
            if (slot.voiceBank[static_cast<std::size_t>(instrIndex)])
                slot.voiceBank[static_cast<std::size_t>(instrIndex)]->reset();
            if (slot.dyingVoiceBank[static_cast<std::size_t>(instrIndex)])
                slot.dyingVoiceBank[static_cast<std::size_t>(instrIndex)]->reset();
        }
        slot.active = nullptr;
        slot.dying  = nullptr;
        slot.dyingBus = 0;
        slot.midiNote = -1;
        slot.instrIndex = 0;
        slot.velocity = 0.0f;
        slot.dyingVelocity = 0.0f;
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
    compressor.setRatio(3.0f);
    compressor.setAttack(10.0f);
    compressor.setRelease(120.0f);

    compCache = CompressorCache{};
    fxDryBuffer.setSize(static_cast<int>(spec.numChannels),
                        static_cast<int>(spec.maximumBlockSize), false, true, true);
    mainDryBuffer.setSize(static_cast<int>(spec.numChannels),
                          static_cast<int>(spec.maximumBlockSize), false, true, true);
    voiceScratchBuffer.setSize(2, static_cast<int>(spec.maximumBlockSize), false, true, true);
    clearSustainedNotes();
    satDriveCurrent = juce::jlimit(1.0f, 16.0f, readCachedParamValue(globalParamRefs.satDrive, 1.5f));
    satMixCurrent = clamp01(readCachedParamValue(globalParamRefs.satMix, 0.0f));
    saturatorPrevInput = { 0.0f, 0.0f };
    saturatorPrevAdaaInput = { 0.0f, 0.0f };
    mainMeterLevels[0].store(0.0f, std::memory_order_relaxed);
    mainMeterLevels[1].store(0.0f, std::memory_order_relaxed);
    for (auto& meter : auxMeterLevels)
        meter.store(0.0f, std::memory_order_relaxed);
    clipLatched.store(false, std::memory_order_relaxed);
    lfoPhase = 0.0f;
    modulationMatrix.lfo2.reset();

    // FX processors
    fxTransient.prepare(preparedSampleRate);
    fxEQ.prepare(preparedSampleRate);
    fxChorus.prepare(preparedSampleRate, scratchSamples);
    fxDelay.prepare(preparedSampleRate, scratchSamples);
    fxReverb.prepare(preparedSampleRate, scratchSamples);
    fxLimiter.prepare(preparedSampleRate);
    satOversamplingMono.initProcessing(static_cast<size_t>(scratchSamples));
    satOversamplingStereo.initProcessing(static_cast<size_t>(scratchSamples));
    satOversamplingMono.reset();
    satOversamplingStereo.reset();
    markOutputBusCacheDirty();
}

void PercSynthAudioProcessor::releaseResources()
{
    for (auto& slot : voices)
    {
        slot.active = nullptr;
        slot.dying  = nullptr;
        slot.dyingBus = 0;
        slot.midiNote = -1;
        slot.instrIndex = 0;
        slot.velocity = 0.0f;
        slot.dyingVelocity = 0.0f;
        slot.activationAge = 0;
        for (auto& voice : slot.voiceBank)
            if (voice)
                voice->reset();
        for (auto& voice : slot.dyingVoiceBank)
            if (voice)
                voice->reset();
    }
    fxDryBuffer.setSize(0, 0);
    mainDryBuffer.setSize(0, 0);
    voiceScratchBuffer.setSize(0, 0);
    satOversamplingMono.reset();
    satOversamplingStereo.reset();
    sustainHeld = false;
    clearSustainedNotes();
}

bool PercSynthAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

double PercSynthAudioProcessor::readHostTempoBpm() const
{
    if (auto* hostPlayHead = getPlayHead())
    {
        if (auto position = hostPlayHead->getPosition())
        {
            if (auto bpm = position->getBpm())
                return juce::jlimit(20.0, 240.0, *bpm);
        }
    }

    return juce::jlimit(20.0, 240.0,
                        static_cast<double>(lastKnownHostTempoBpm.load(std::memory_order_relaxed)));
}

PercSynthAudioProcessor::GlobalBlockState PercSynthAudioProcessor::buildGlobalBlockState()
{
    GlobalBlockState state;
    state.selectedInstrument = getSelectedInstrIndex();
    state.qualityMode = juce::jlimit(0, 1,
        static_cast<int>(std::round(readCachedParamValue(globalParamRefs.qualityMode, 0.0f))));
    state.delayDivision = juce::jlimit(0, 5,
        static_cast<int>(std::round(readCachedParamValue(globalParamRefs.delayDivision, 1.0f))));
    state.outputGainDb = readCachedParamValue(globalParamRefs.outputGain, -5.0f);
    state.lfoRate = readCachedParamValue(globalParamRefs.lfoRate, 1.8f);
    state.lfoDepth = readCachedParamValue(globalParamRefs.lfoDepth, 0.0f);
    state.lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.lfoWave, 0.0f))));
    state.hostBpm = static_cast<float>(readHostTempoBpm());
    state.delaySyncToHost = readCachedParamValue(globalParamRefs.delaySync, 0.0f) >= 0.5f;
    state.fx = sanitizeFxSettings(snapshotFxSettings());

    switch (state.lfoWave)
    {
        case 1: state.baseModContext.lfo1 = 1.0f - 4.0f * std::abs(lfoPhase - 0.5f); break;
        case 2: state.baseModContext.lfo1 = lfoPhase * 2.0f - 1.0f; break;
        case 3: state.baseModContext.lfo1 = lfoPhase < 0.5f ? 1.0f : -1.0f; break;
        default: state.baseModContext.lfo1 = std::sin(lfoPhase * juce::MathConstants<float>::twoPi); break;
    }

    state.baseModContext.lfo2 = modulationMatrix.lfo2.tick(static_cast<float>(preparedSampleRate));
    state.baseModContext.modWheel = modulationMatrix.modWheelValue;
    state.baseModContext.aftertouch = modulationMatrix.aftertouchValue;
    state.baseModContext.pitchBend = modulationMatrix.pitchBendValue;
    state.sharedModResult = modulationMatrix.process(state.baseModContext);
    state.lfoRate = juce::jlimit(0.05f, 12.0f, state.lfoRate * state.sharedModResult.lfo1RateMul);
    state.lfoDepth = clamp01(state.lfoDepth);
    lastKnownHostTempoBpm.store(state.hostBpm, std::memory_order_relaxed);
    return state;
}

// =============================================================================
void PercSynthAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalBlockSamples = buffer.getNumSamples();
    const int maxChunkSamples = juce::jmin(mainDryBuffer.getNumSamples(), voiceScratchBuffer.getNumSamples());
    if (totalBlockSamples > 0 && maxChunkSamples > 0 && totalBlockSamples > maxChunkSamples)
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

    keyboardState.processNextMidiBuffer(midiMessages, 0, buffer.getNumSamples(), true);

    const int instrIdx = getSelectedInstrIndex();

    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        modulationMatrix.handleMidiMessage(msg);
        if (msg.isNoteOn())
            triggerNoteOn(instrIdx, msg.getNoteNumber(),
                          applyVelocityCurve(msg.getFloatVelocity(), velocityCurve));
        else if (msg.isNoteOff())
            triggerNoteOff(instrIdx, msg.getNoteNumber());
        else if (msg.isAllNotesOff())
            releaseVoices(msg.getChannel(), false);
        else if (msg.isAllSoundOff())
            panicAllVoices();
        else if (msg.isController() && msg.getControllerNumber() == 120)
            panicAllVoices();
        else if (msg.isPitchWheel())
            pitchBend.setPitchWheel(msg.getPitchWheelValue());
        else if (msg.isControllerOfType(64))
        {
            if (msg.getControllerValue() >= 64)
            {
                sustainHeld = true;
            }
            else
            {
                releaseSustainedNotes();
            }
        }
        else if (msg.isController())
            handleMidiCC(msg.getControllerNumber(), msg.getControllerValue(), instrIdx);
    }

    midiMessages.clear();
    auto blockState = buildGlobalBlockState();
    cachedModResult = blockState.sharedModResult;

    struct BusView
    {
        float* left = nullptr;
        float* right = nullptr;
        int numChannels = 0;
    };

    std::array<BusView, kNumAuxOutputs + 1> outputViews{};
    for (int busIndex = 0; busIndex < outputBusCount; ++busIndex)
    {
        auto busBuffer = getBusBuffer(buffer, false, busIndex);
        auto& view = outputViews[static_cast<std::size_t>(busIndex)];
        view.numChannels = busBuffer.getNumChannels();
        if (view.numChannels > 0)
            view.left = busBuffer.getWritePointer(0);
        if (view.numChannels > 1)
            view.right = busBuffer.getWritePointer(1);
    }

    auto mainBuffer = getBusBuffer(buffer, false, 0);
    const auto numSamples = mainBuffer.getNumSamples();
    const auto sr = preparedSampleRate;
    const int workChannels = juce::jmin(mainDryBuffer.getNumChannels(), juce::jmax(2, mainBuffer.getNumChannels()));
    const int workSamples = juce::jmin(mainDryBuffer.getNumSamples(), numSamples);
    jassert(workSamples == numSamples);
    if (workChannels <= 0 || workSamples < numSamples || voiceScratchBuffer.getNumChannels() < 2
        || voiceScratchBuffer.getNumSamples() < numSamples)
    {
        jassertfalse;
        return;
    }
    juce::AudioBuffer<float> mainDryWork(mainDryBuffer.getArrayOfWritePointers(), workChannels, workSamples);
    mainDryWork.clear();

    refreshOutputBusCacheIfNeeded();

    for (auto& slot : voices)
    {
        if (slot.active && slot.active->isActive())
        {
            modmatrix::ModContext voiceContext = blockState.baseModContext;
            voiceContext.envelope = slot.active->getEnvelopeLevel();
            voiceContext.velocity = slot.velocity;
            const auto voiceModResult = modulationMatrix.process(voiceContext);

            mpc::VoiceModulation voiceMod;
            voiceMod.cutoffMul = voiceModResult.cutoffMul;
            voiceMod.resonanceAdd = voiceModResult.resonance;
            voiceMod.panAdd = voiceModResult.pan;
            voiceMod.attackScale = voiceModResult.attackScale;
            voiceMod.decayScale = voiceModResult.decayScale;
            voiceMod.pitchSemi = voiceModResult.pitchSemi;
            voiceMod.levelMul = voiceModResult.levelMul;
            slot.active->setPitchBendFactor(pitchBend.pitchBendFactor);
            slot.active->setVoiceModulation(voiceMod, preparedSampleRate);
        }

        if (slot.dying && slot.dying->isActive())
        {
            modmatrix::ModContext voiceContext = blockState.baseModContext;
            voiceContext.envelope = slot.dying->getEnvelopeLevel();
            voiceContext.velocity = slot.dyingVelocity;
            const auto voiceModResult = modulationMatrix.process(voiceContext);

            mpc::VoiceModulation voiceMod;
            voiceMod.cutoffMul = voiceModResult.cutoffMul;
            voiceMod.resonanceAdd = voiceModResult.resonance;
            voiceMod.panAdd = voiceModResult.pan;
            voiceMod.attackScale = voiceModResult.attackScale;
            voiceMod.decayScale = voiceModResult.decayScale;
            voiceMod.pitchSemi = voiceModResult.pitchSemi;
            voiceMod.levelMul = voiceModResult.levelMul;
            slot.dying->setPitchBendFactor(pitchBend.pitchBendFactor);
            slot.dying->setVoiceModulation(voiceMod, preparedSampleRate);
        }
    }

    auto mixScratchToTargets = [&] (int busIndex)
    {
        auto* scratchL = voiceScratchBuffer.getReadPointer(0);
        auto* scratchR = voiceScratchBuffer.getReadPointer(1);

        if (mainDryWork.getNumChannels() >= 2)
        {
            mainDryWork.addFrom(0, 0, scratchL, numSamples);
            mainDryWork.addFrom(1, 0, scratchR, numSamples);
        }
        else if (mainDryWork.getNumChannels() == 1)
        {
            auto* main = mainDryWork.getWritePointer(0);
            for (int i = 0; i < numSamples; ++i)
                main[i] += (scratchL[i] + scratchR[i]) * 0.5f;
        }

        if (busIndex <= 0)
            return;

        auto& target = outputViews[static_cast<std::size_t>(busIndex)];
        if (target.numChannels >= 2)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                target.left[i] += scratchL[i];
                target.right[i] += scratchR[i];
            }
        }
        else if (target.numChannels == 1)
        {
            for (int i = 0; i < numSamples; ++i)
                target.left[i] += (scratchL[i] + scratchR[i]) * 0.5f;
        }
    };

    auto renderVoiceToScratch = [&] (mpc::PercVoice* voice, int targetBus)
    {
        if (voice == nullptr || !voice->isActive())
            return;

        voiceScratchBuffer.clear(0, 0, numSamples);
        voiceScratchBuffer.clear(1, 0, numSamples);
        auto* scratchL = voiceScratchBuffer.getWritePointer(0);
        auto* scratchR = voiceScratchBuffer.getWritePointer(1);
        voice->renderBlock(scratchL, scratchR, numSamples, sr);

        int safeBus = juce::jlimit(0, outputBusCount - 1, targetBus);
        if (safeBus > 0 && outputViews[static_cast<std::size_t>(safeBus)].numChannels <= 0)
            safeBus = 0;
        mixScratchToTargets(safeBus);
    };

    for (auto& slot : voices)
    {
        if (slot.dying && slot.dying->isActive())
            renderVoiceToScratch(slot.dying, slot.dyingBus);
        else if (slot.dying)
        {
            slot.dying = nullptr;
            slot.dyingBus = 0;
            slot.dyingVelocity = 0.0f;
        }

        if (slot.active && slot.active->isActive())
        {
            const int targetBus = outputBusCache[static_cast<std::size_t>(
                juce::jlimit(0, mpc::kNumInstruments - 1, slot.instrIndex))];
            renderVoiceToScratch(slot.active, targetBus);
        }
    }

    for (auto& slot : voices)
    {
        if (slot.active && !slot.active->isActive())
        {
            slot.active = nullptr;
            slot.midiNote = -1;
            slot.velocity = 0.0f;
        }
        if (slot.dying && !slot.dying->isActive())
        {
            slot.dying = nullptr;
            slot.dyingBus = 0;
            slot.dyingVelocity = 0.0f;
        }
    }

    if (mainBuffer.getNumChannels() > 0 && mainBuffer.getNumSamples() > 0)
    {
        for (int ch = 0; ch < juce::jmin(mainBuffer.getNumChannels(), mainDryWork.getNumChannels()); ++ch)
            mainBuffer.copyFrom(ch, 0, mainDryWork, ch, 0, numSamples);
        processMasterFxChain(mainBuffer, blockState);
    }

    const auto auxSafetyPeak = juce::Decibels::decibelsToGain(-0.5f);
    for (int auxIndex = 0; auxIndex < kNumAuxOutputs; ++auxIndex)
    {
        const int busIndex = auxIndex + 1;
        if (busIndex >= getBusCount(false) || getChannelCountOfBus(false, busIndex) <= 0)
            continue;

        auto auxBuffer = getBusBuffer(buffer, false, busIndex);
        float peak = 0.0f;
        for (int channel = 0; channel < auxBuffer.getNumChannels(); ++channel)
            peak = juce::jmax(peak, auxBuffer.getMagnitude(channel, 0, auxBuffer.getNumSamples()));

        if (peak > auxSafetyPeak && peak > 0.0001f)
            auxBuffer.applyGain(auxSafetyPeak / peak);
    }

    updateOutputMeters(buffer, mainBuffer);
}

// =============================================================================
juce::AudioProcessorEditor* PercSynthAudioProcessor::createEditor()
{
    return new PercSynthAudioProcessorEditor(*this);
}

double PercSynthAudioProcessor::getTailLengthSeconds() const
{
    const auto selectedInstr = getSelectedInstrIndex();
    const auto releaseSec = juce::jlimit(0.01f, 5.0f,
        getParamValue(makeInstrParamId(selectedInstr, "release")));
    const bool reverbEnabled = getParamValue(kFxReverbEnable) >= 0.5f;
    const bool delayEnabled = getParamValue(kFxDelayEnable) >= 0.5f;

    double tailSeconds = static_cast<double>(releaseSec) + 0.2;
    if (reverbEnabled)
    {
        const auto reverbMix = clamp01(getParamValue(kReverbMix));
        const auto reverbSize = clamp01(getParamValue(kReverbSize));
        tailSeconds = juce::jmax(tailSeconds,
                                 0.5 + static_cast<double>(reverbSize) * 10.0
                                     + static_cast<double>(reverbMix) * 4.5);
    }

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
int PercSynthAudioProcessor::getNumPrograms()
{
    const int sel = getSelectedInstrIndex();
    return juce::jmax(1, static_cast<int>(factoryPresetBanks[static_cast<std::size_t>(sel)].size()));
}

int PercSynthAudioProcessor::getCurrentProgram()
{
    return currentPresetIndices[static_cast<std::size_t>(getSelectedInstrIndex())];
}

void PercSynthAudioProcessor::setCurrentProgram(int index) { applyFactoryPreset(index); }

const juce::String PercSynthAudioProcessor::getProgramName(int index)
{
    const int sel = getSelectedInstrIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(sel)];
    if (index < 0 || index >= static_cast<int>(bank.size())) return {};
    return juce::String(juce::CharPointer_UTF8(bank[static_cast<std::size_t>(index)].name.c_str()));
}

void PercSynthAudioProcessor::changeProgramName(int, const juce::String&) {}

// =============================================================================
void PercSynthAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    storeCurrentInstrumentFxState(getSelectedInstrIndex());
    auto state = parameters.copyState();
    for (int childIndex = state.getNumChildren(); --childIndex >= 0;)
    {
        if (state.getChild(childIndex).hasType("inst_fx_state"))
            state.removeChild(childIndex, nullptr);
    }
    for (int i = 0; i < mpc::kNumInstruments; ++i)
    {
        state.setProperty("pi_" + juce::String(i), currentPresetIndices[static_cast<std::size_t>(i)], nullptr);
        const auto& upf = currentUserPresetFiles[static_cast<std::size_t>(i)];
        if (upf.existsAsFile())
            state.setProperty("upf_" + juce::String(i), upf.getFullPathName(), nullptr);

        juce::ValueTree fxState("inst_fx_state");
        fxState.setProperty("inst", i, nullptr);
        writeFxStateProperties(fxState, instrumentFxStates[static_cast<std::size_t>(i)]);
        state.appendChild(fxState, nullptr);
    }
    if (auto xml = state.createXml())
    {
        modulationMatrix.saveToXml(*xml);
        copyXmlToBinary(*xml, destData);
    }
}

void PercSynthAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    const auto xmlState = getXmlFromBinary(data, sizeInBytes);
    if (xmlState == nullptr || !xmlState->hasTagName(parameters.state.getType()))
        return;

    const std::lock_guard<std::mutex> stateLock(stateMutex);
    modulationMatrix.loadFromXml(*xmlState);
    auto restoredState = juce::ValueTree::fromXml(*xmlState);
    setStateFloatProperty(restoredState, kSelectedInstr, readFiniteStateFloat(restoredState, kSelectedInstr, 0.0f, 0.0f, static_cast<float>(mpc::kNumInstruments - 1)));
    setStateFloatProperty(restoredState, kOutputGain, readFiniteStateFloat(restoredState, kOutputGain, -5.0f, -24.0f, 12.0f));
    setStateFloatProperty(restoredState, kQualityMode, readFiniteStateFloat(restoredState, kQualityMode, 0.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kDelaySync, readFiniteStateFloat(restoredState, kDelaySync, 0.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kDelayDivision, readFiniteStateFloat(restoredState, kDelayDivision, 1.0f, 0.0f, 5.0f));
    setStateFloatProperty(restoredState, kLfoRate, readFiniteStateFloat(restoredState, kLfoRate, 1.8f, 0.05f, 12.0f));
    setStateFloatProperty(restoredState, kLfoDepth, readFiniteStateFloat(restoredState, kLfoDepth, 0.0f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kLfoWave, readFiniteStateFloat(restoredState, kLfoWave, 0.0f, 0.0f, 3.0f));
    setStateFloatProperty(restoredState, kMacroImpact, readFiniteStateFloat(restoredState, kMacroImpact, 0.5f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kMacroResonance, readFiniteStateFloat(restoredState, kMacroResonance, 0.5f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kMacroSpace, readFiniteStateFloat(restoredState, kMacroSpace, 0.5f, 0.0f, 1.0f));
    setStateFloatProperty(restoredState, kMacroCouleur, readFiniteStateFloat(restoredState, kMacroCouleur, 0.5f, 0.0f, 1.0f));
    sanitizeStateParameterValues(parameters, restoredState);
    cancelPendingUpdate();
    isRestoringState.store(true, std::memory_order_release);
    parameters.replaceState(restoredState);
    isRestoringState.store(false, std::memory_order_release);
    resolveParameterPointers();
    rebuildMidiCCBindings();
    setParamValueInternal(kSelectedInstr, readFiniteStateFloat(restoredState, kSelectedInstr, 0.0f, 0.0f, static_cast<float>(mpc::kNumInstruments - 1)), false);
    setParamValueInternal(kOutputGain, readFiniteStateFloat(restoredState, kOutputGain, -5.0f, -24.0f, 12.0f), false);
    setParamValueInternal(kQualityMode, readFiniteStateFloat(restoredState, kQualityMode, 0.0f, 0.0f, 1.0f), false);
    setParamValueInternal(kDelaySync, readFiniteStateFloat(restoredState, kDelaySync, 0.0f, 0.0f, 1.0f), false);
    setParamValueInternal(kDelayDivision, readFiniteStateFloat(restoredState, kDelayDivision, 1.0f, 0.0f, 5.0f), false);
    setParamValueInternal(kLfoRate, readFiniteStateFloat(restoredState, kLfoRate, 1.8f, 0.05f, 12.0f), false);
    setParamValueInternal(kLfoDepth, readFiniteStateFloat(restoredState, kLfoDepth, 0.0f, 0.0f, 1.0f), false);
    setParamValueInternal(kLfoWave, readFiniteStateFloat(restoredState, kLfoWave, 0.0f, 0.0f, 3.0f), false);
    setParamValueInternal(kMacroImpact, readFiniteStateFloat(restoredState, kMacroImpact, 0.5f, 0.0f, 1.0f), false);
    setParamValueInternal(kMacroResonance, readFiniteStateFloat(restoredState, kMacroResonance, 0.5f, 0.0f, 1.0f), false);
    setParamValueInternal(kMacroSpace, readFiniteStateFloat(restoredState, kMacroSpace, 0.5f, 0.0f, 1.0f), false);
    setParamValueInternal(kMacroCouleur, readFiniteStateFloat(restoredState, kMacroCouleur, 0.5f, 0.0f, 1.0f), false);
    sanitizeAllParameters();

    for (int i = 0; i < mpc::kNumInstruments; ++i)
    {
        const auto& bank = factoryPresetBanks[static_cast<std::size_t>(i)];
        instrumentFxStates[static_cast<std::size_t>(i)] = bank.empty()
            ? mpc::GlobalFxSettings{}
            : bank[0].fx;
    }

    const int selectedInstr = getSelectedInstrIndex();
    instrumentFxStates[static_cast<std::size_t>(selectedInstr)] = snapshotFxSettings();

    for (int i = 0; i < mpc::kNumInstruments; ++i)
    {
        int idx = static_cast<int>(restoredState.getProperty("pi_" + juce::String(i), 0));
        const int bankSize = static_cast<int>(factoryPresetBanks[static_cast<std::size_t>(i)].size());
        currentPresetIndices[static_cast<std::size_t>(i)] = juce::jlimit(0, juce::jmax(0, bankSize - 1), idx);

        currentUserPresetFiles[static_cast<std::size_t>(i)] = juce::File{};
        auto userPath = restoredState.getProperty("upf_" + juce::String(i), "").toString();
        if (userPath.isNotEmpty())
        {
            juce::File f(userPath);
            if (f.existsAsFile()) currentUserPresetFiles[static_cast<std::size_t>(i)] = f;
        }
        outputBusCache[static_cast<std::size_t>(i)] = captureInstrOutputBus(i);
    }
    markOutputBusCacheDirty();

    for (int childIndex = 0; childIndex < restoredState.getNumChildren(); ++childIndex)
    {
        const auto child = restoredState.getChild(childIndex);
        if (!child.hasType("inst_fx_state"))
            continue;

        const int inst = juce::jlimit(0, mpc::kNumInstruments - 1,
                                      static_cast<int>(child.getProperty("inst", 0)));
        instrumentFxStates[static_cast<std::size_t>(inst)] = sanitizeFxSettings(readFxStateProperties(
            child, instrumentFxStates[static_cast<std::size_t>(inst)]));
        instrumentFxStates[static_cast<std::size_t>(inst)] = mpc::maskUnavailableFx(inst, instrumentFxStates[static_cast<std::size_t>(inst)]);
    }

    cachedSelectedInstrumentIndex = selectedInstr;
    pendingSelectedInstrumentIndex.store(selectedInstr);
    cancelPendingUpdate();
    applyFxToParams(selectedInstr, instrumentFxStates[static_cast<std::size_t>(selectedInstr)], false);
    sanitizeAllParameters();
}

// =============================================================================
juce::StringArray PercSynthAudioProcessor::getFactoryPresetNames() const
{
    const int sel = getSelectedInstrIndex();
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(sel)];
    juce::StringArray names;
    for (const auto& p : bank)
        names.add(juce::String(juce::CharPointer_UTF8(p.name.c_str())));
    return names;
}

int PercSynthAudioProcessor::getCurrentFactoryPresetIndex() const noexcept
{
    return currentPresetIndices[static_cast<std::size_t>(getSelectedInstrIndex())];
}

void PercSynthAudioProcessor::applyFactoryPreset(int presetIndex)
{
    const int sel = getSelectedInstrIndex();
    const auto& states = factoryPresetStates[static_cast<std::size_t>(sel)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(states.size()))
        return;

    applyPresetPersistenceState(states[static_cast<std::size_t>(presetIndex)], false);

    currentPresetIndices[static_cast<std::size_t>(sel)] = presetIndex;
    currentUserPresetFiles[static_cast<std::size_t>(sel)] = juce::File{};
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
}

bool PercSynthAudioProcessor::saveFactoryPreset(int presetIndex)
{
    const int sel = getSelectedInstrIndex();
    auto& bank = factoryPresetBanks[static_cast<std::size_t>(sel)];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return false;

    storeCurrentInstrumentFxState(sel);

    auto& preset = bank[static_cast<std::size_t>(presetIndex)];
    auto state = captureCurrentPresetState(sel);
    state.name = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    state.presetIndex = presetIndex;
    state.metadata = makeFactoryMetadata(preset.metadata);

    preset.settings = state.settings;
    preset.fx = state.fx;
    preset.outputBus = state.outputBus;
    factoryPresetStates[static_cast<std::size_t>(sel)][static_cast<std::size_t>(presetIndex)] = state;

    auto dir = getFactoryOverridesDirectory()
                   .getChildFile("instr_" + juce::String(sel));
    dir.createDirectory();
    auto file = dir.getChildFile(juce::String(presetIndex) + ".xml");

    auto root = createPresetXml("FactoryPreset", state);

    return root->writeTo(file);
}

void PercSynthAudioProcessor::loadFactoryOverrides()
{
    auto baseDir = getFactoryOverridesDirectory();
    if (!baseDir.isDirectory()) return;

    for (int instrIdx = 0; instrIdx < mpc::kNumInstruments; ++instrIdx)
    {
        auto instrDir = baseDir.getChildFile("instr_" + juce::String(instrIdx));
        if (!instrDir.isDirectory()) continue;

        auto& bank = factoryPresetBanks[static_cast<std::size_t>(instrIdx)];
        for (int i = 0; i < static_cast<int>(bank.size()); ++i)
        {
            auto file = instrDir.getChildFile(juce::String(i) + ".xml");
            if (!file.existsAsFile()) continue;

            auto xml = juce::XmlDocument::parse(file);
            if (xml == nullptr)
                continue;

            auto parsed = makeFactoryPresetState(instrIdx, i, bank[static_cast<std::size_t>(i)]);
            if (!parsePresetXml(*xml, "FactoryPreset", instrIdx, parsed, true, &parsed) || parsed.presetIndex != i)
                continue;

            bank[static_cast<std::size_t>(i)].settings = parsed.settings;
            bank[static_cast<std::size_t>(i)].fx = parsed.fx;
            bank[static_cast<std::size_t>(i)].outputBus = parsed.outputBus;
            bank[static_cast<std::size_t>(i)].metadata.mixRole = parsed.metadata.mixRole.toStdString();
            bank[static_cast<std::size_t>(i)].metadata.familyLabel = parsed.metadata.family.toStdString();
            bank[static_cast<std::size_t>(i)].metadata.tags = splitTags(parsed.metadata.tags);
            bank[static_cast<std::size_t>(i)].metadata.description = parsed.metadata.description.toStdString();
            bank[static_cast<std::size_t>(i)].metadata.outputProfile = parsed.metadata.outputProfile.toStdString();
            bank[static_cast<std::size_t>(i)].metadata.nominalPeakDb = parsed.metadata.nominalPeakDb;
            factoryPresetStates[static_cast<std::size_t>(instrIdx)][static_cast<std::size_t>(i)] = parsed;

            if (shouldRewritePresetXml(*xml, true))
                createPresetXml("FactoryPreset", parsed)->writeTo(file);
        }
    }
}

// =============================================================================
juce::File PercSynthAudioProcessor::getFactoryOverridesDirectory()
{
    const auto preferred = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                               .getChildFile("MusiquePercSynth")
                               .getChildFile("FactoryOverrides");
    return findWritableDirectory(preferred, "MusiquePercSynth/FactoryOverrides");
}

juce::File PercSynthAudioProcessor::getUserPresetsDirectory(int instrIndex)
{
    const auto preferred = musique::preset::nativeUserPresetsDirectoryForSynth(5, instrIndex);
    return findWritableDirectory(preferred,
                                 "MusiquePercSynth/Presets/instr_" + juce::String(juce::jmax(0, instrIndex)));
}

bool PercSynthAudioProcessor::writePresetManifest(const juce::File& presetFile,
                                                  const juce::String& presetName,
                                                  int instrIndex,
                                                  const juce::String& sourceModel) const
{
    const auto identity = musique::preset::getSynthIdentity(5);
    if (!identity.isValid())
        return false;

    musique::preset::PresetManifest manifest;
    manifest.synthId = identity.synthId;
    manifest.synthType = identity.synthType;
    manifest.instrumentIndex = juce::jlimit(0, mpc::kNumInstruments - 1, instrIndex);
    manifest.instrumentName = mpc::getInstrName(manifest.instrumentIndex);
    manifest.presetName = presetName;
    manifest.xmlRootTag = identity.xmlRootTag;
    manifest.sourceModel = sourceModel;
    manifest.createdAt = juce::Time::getCurrentTime().toISO8601(true);
    manifest.sourcePath = presetFile.getFullPathName();
    manifest.validationVersion = 1;

    return musique::preset::saveManifestToFile(
        musique::preset::manifestFileForPresetFile(presetFile), manifest);
}

juce::Array<juce::File> PercSynthAudioProcessor::scanUserPresets() const
{
    juce::Array<juce::File> results;
    auto dir = getUserPresetsDirectory(getSelectedInstrIndex());
    if (dir.isDirectory())
        dir.findChildFiles(results, juce::File::findFiles, false, "*.xml");
    results.sort();
    return results;
}

void PercSynthAudioProcessor::backfillUserPresetLibrary(int instrIndex) const
{
    if (instrIndex < 0 || instrIndex >= mpc::kNumInstruments)
        return;

    const auto dir = getUserPresetsDirectory(instrIndex);
    if (!dir.isDirectory())
        return;

    juce::Array<juce::File> files;
    dir.findChildFiles(files, juce::File::findFiles, false, "*.xml");

    const auto identity = musique::preset::getSynthIdentity(5);
    for (const auto& file : files)
    {
        auto xml = juce::XmlDocument::parse(file);
        if (xml == nullptr)
        {
            juce::Logger::writeToLog("UWdeVST_perc: ignoring invalid preset XML " + file.getFullPathName());
            continue;
        }
        if (!xml->hasTagName(identity.xmlRootTag))
        {
            juce::Logger::writeToLog("UWdeVST_perc: ignoring unsupported preset root in " + file.getFullPathName());
            continue;
        }

        const bool hadCanonicalInstrIndex = xml->hasAttribute(identity.instrumentAttrName);
        const bool hasExplicitInstrIndex = hadCanonicalInstrIndex
            || xml->hasAttribute("instrument_index")
            || xml->hasAttribute("instrumentIndex")
            || xml->hasAttribute("index")
            || xml->hasAttribute("instr");
        const int resolvedInstr = juce::jlimit(0, mpc::kNumInstruments - 1,
            hasExplicitInstrIndex ? musique::preset::readInstrumentIndexFromXml(*xml, identity) : instrIndex);
        if (!hadCanonicalInstrIndex)
            xml->setAttribute(identity.instrumentAttrName, resolvedInstr);

        auto parsed = makeDefaultPresetState(resolvedInstr);
        parsed.name = file.getFileNameWithoutExtension();
        if (!parsePresetXml(*xml, identity.xmlRootTag, resolvedInstr, parsed, false, &parsed))
        {
            juce::Logger::writeToLog("UWdeVST_perc: ignoring unsupported preset payload in " + file.getFullPathName());
            continue;
        }

        const bool needsRewrite = !hadCanonicalInstrIndex
            || xml->getIntAttribute(identity.instrumentAttrName, resolvedInstr) != resolvedInstr
            || shouldRewritePresetXml(*xml, false);
        if (needsRewrite)
        {
            parsed.name = parsed.name.isNotEmpty() ? parsed.name : file.getFileNameWithoutExtension();
            auto normalizedXml = createPresetXml(identity.xmlRootTag, parsed);
            normalizedXml->writeTo(file);
        }

        musique::preset::PresetManifest manifest;
        const auto manifestFile = musique::preset::manifestFileForPresetFile(file);
        const bool manifestValid = musique::preset::loadManifestFromFile(manifestFile, manifest)
            && manifest.synthType == "perc"
            && manifest.instrumentIndex == resolvedInstr
            && manifest.xmlRootTag == identity.xmlRootTag;
        if (!manifestValid)
        {
            writePresetManifest(file,
                                parsed.name.isNotEmpty() ? parsed.name : file.getFileNameWithoutExtension(),
                                resolvedInstr,
                                mpc::getInstrName(resolvedInstr));
        }
    }
}

void PercSynthAudioProcessor::backfillAllUserPresetLibraries() const
{
    for (int instrIndex = 0; instrIndex < mpc::kNumInstruments; ++instrIndex)
        backfillUserPresetLibrary(instrIndex);
}

bool PercSynthAudioProcessor::saveUserPreset(const juce::String& name)
{
    if (name.isEmpty()) return false;
    const int sel = getSelectedInstrIndex();
    storeCurrentInstrumentFxState(sel);
    auto file = getUserPresetsDirectory(sel).getChildFile(
        juce::File::createLegalFileName(name) + ".xml");

    auto state = captureCurrentPresetState(sel);
    state.name = name;
    auto root = createPresetXml("PercPreset", state);

    if (root->writeTo(file))
    {
        writePresetManifest(file, name, sel, mpc::getInstrName(sel));
        currentUserPresetFiles[static_cast<std::size_t>(sel)] = file;
        currentPresetIndices[static_cast<std::size_t>(sel)]   = -1;
        return true;
    }
    return false;
}

bool PercSynthAudioProcessor::updateUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int sel = getSelectedInstrIndex();
    storeCurrentInstrumentFxState(sel);
    auto state = captureCurrentPresetState(sel);
    state.name = file.getFileNameWithoutExtension();
    auto root = createPresetXml("PercPreset", state);

    if (root->writeTo(file))
    {
        writePresetManifest(file, file.getFileNameWithoutExtension(), sel, mpc::getInstrName(sel));
        currentUserPresetFiles[static_cast<std::size_t>(sel)] = file;
        currentPresetIndices[static_cast<std::size_t>(sel)]   = -1;
        return true;
    }
    return false;
}

bool PercSynthAudioProcessor::deleteUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    const int sel = getSelectedInstrIndex();
    if (currentUserPresetFiles[static_cast<std::size_t>(sel)] == file)
        currentUserPresetFiles[static_cast<std::size_t>(sel)] = juce::File{};
    const auto manifestFile = musique::preset::manifestFileForPresetFile(file);
    if (manifestFile.existsAsFile())
        manifestFile.deleteFile();
    return file.deleteFile();
}

bool PercSynthAudioProcessor::loadUserPreset(const juce::File& file)
{
    if (!file.existsAsFile()) return false;
    auto xml = juce::XmlDocument::parse(file);
    if (xml == nullptr) return false;

    const int sel = getSelectedInstrIndex();
    auto parsed = makeDefaultPresetState(sel);
    parsed.name = file.getFileNameWithoutExtension();
    if (!parsePresetXml(*xml, "PercPreset", sel, parsed, false, &parsed))
        return false;

    applyPresetPersistenceState(parsed, false);

    currentUserPresetFiles[static_cast<std::size_t>(sel)] = file;
    currentPresetIndices[static_cast<std::size_t>(sel)]   = -1;
    if (shouldRewritePresetXml(*xml, false))
        createPresetXml("PercPreset", parsed)->writeTo(file);
    writePresetManifest(file, file.getFileNameWithoutExtension(), sel, mpc::getInstrName(sel));
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withProgramChanged(true));
    return true;
}

bool PercSynthAudioProcessor::isCurrentPresetUser() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedInstrIndex())].existsAsFile();
}

juce::File PercSynthAudioProcessor::getCurrentUserPresetFile() const noexcept
{
    return currentUserPresetFiles[static_cast<std::size_t>(getSelectedInstrIndex())];
}

// =============================================================================
int PercSynthAudioProcessor::getSelectedInstrIndex() const
{
    return juce::jlimit(0, mpc::kNumInstruments - 1,
                        static_cast<int>(std::round(readCachedParamValue(globalParamRefs.selectedInstr))));
}

PercSynthAudioProcessor::QualityMode PercSynthAudioProcessor::getQualityMode() const noexcept
{
    return getParamValue(kQualityMode) >= 0.5f ? QualityMode::Studio : QualityMode::Live;
}

bool PercSynthAudioProcessor::isDelaySyncEnabled() const noexcept
{
    return getParamValue(kDelaySync) >= 0.5f;
}

int PercSynthAudioProcessor::getDelayDivisionIndex() const noexcept
{
    return juce::jlimit(0, 5, static_cast<int>(std::round(getParamValue(kDelayDivision))));
}

float PercSynthAudioProcessor::getLastKnownHostTempoBpm() const noexcept
{
    return lastKnownHostTempoBpm.load(std::memory_order_relaxed);
}

float PercSynthAudioProcessor::getMainMeterLevel(int channel) const noexcept
{
    return mainMeterLevels[static_cast<std::size_t>(juce::jlimit(0, 1, channel))].load(std::memory_order_relaxed);
}

float PercSynthAudioProcessor::getAuxMeterLevel(int auxIndex) const noexcept
{
    return auxMeterLevels[static_cast<std::size_t>(juce::jlimit(0, kNumAuxOutputs - 1, auxIndex))]
        .load(std::memory_order_relaxed);
}

bool PercSynthAudioProcessor::isClipLatched() const noexcept
{
    return clipLatched.load(std::memory_order_relaxed);
}

void PercSynthAudioProcessor::clearClipLatch() noexcept
{
    clipLatched.store(false, std::memory_order_relaxed);
}

const mpc::InstrumentPreset* PercSynthAudioProcessor::getFactoryPresetDefinition(int presetIndex) const noexcept
{
    const auto& bank = factoryPresetBanks[static_cast<std::size_t>(getSelectedInstrIndex())];
    if (presetIndex < 0 || presetIndex >= static_cast<int>(bank.size()))
        return nullptr;
    return &bank[static_cast<std::size_t>(presetIndex)];
}

modmatrix::ModSlot PercSynthAudioProcessor::getModMatrixSlot(int index) const
{
    const int safeIndex = juce::jlimit(0, modmatrix::ModulationMatrix::getNumSlots() - 1, index);
    return modulationMatrix.getSlot(safeIndex);
}

void PercSynthAudioProcessor::setModMatrixSlot(int index,
                                               modmatrix::Source source,
                                               modmatrix::Destination destination,
                                               float amount)
{
    const int safeIndex = juce::jlimit(0, modmatrix::ModulationMatrix::getNumSlots() - 1, index);
    modulationMatrix.setSlot(safeIndex, source, destination, amount);
    updateHostDisplay(juce::AudioProcessor::ChangeDetails().withParameterInfoChanged(true));
}

float PercSynthAudioProcessor::getModMatrixLfo2Rate() const noexcept
{
    return modulationMatrix.lfo2.getRate();
}

int PercSynthAudioProcessor::getModMatrixLfo2Wave() const noexcept
{
    return modulationMatrix.lfo2.getWave();
}

void PercSynthAudioProcessor::setModMatrixLfo2Rate(float rateHz)
{
    modulationMatrix.lfo2.setRate(juce::jlimit(0.05f, 12.0f, rateHz));
}

void PercSynthAudioProcessor::setModMatrixLfo2Wave(int waveformIndex)
{
    modulationMatrix.lfo2.setWave(juce::jlimit(0, 3, waveformIndex));
}

float PercSynthAudioProcessor::getParamValue(const juce::String& paramId) const
{
    if (const auto* raw = parameters.getRawParameterValue(paramId))
        return raw->load();
    return 0.0f;
}

void PercSynthAudioProcessor::setParamValue(const juce::String& paramId, float value)
{
    setParamValueInternal(paramId, value, true);
}

mpc::InstrSettings PercSynthAudioProcessor::captureBaseInstrSettings(int instrIndex) const
{
    const auto& refs = instrParamRefs[static_cast<std::size_t>(instrIndex)];
    mpc::InstrSettings s;
    s.level          = readCachedParamValue(refs.level, s.level);
    s.tuneSemitones  = readCachedParamValue(refs.tune, s.tuneSemitones);
    s.brightness     = readCachedParamValue(refs.brightness, s.brightness);
    s.attackSeconds  = readCachedParamValue(refs.attack, s.attackSeconds);
    s.decaySeconds   = readCachedParamValue(refs.decay, s.decaySeconds);
    s.sustainLevel   = readCachedParamValue(refs.sustain, s.sustainLevel);
    s.releaseSeconds = readCachedParamValue(refs.release, s.releaseSeconds);
    s.damping        = readCachedParamValue(refs.damping, s.damping);
    s.body           = readCachedParamValue(refs.body, s.body);
    s.noise          = readCachedParamValue(refs.noise, s.noise);
    s.stereoWidth    = readCachedParamValue(refs.stereoWidth, s.stereoWidth);
    s.color          = readCachedParamValue(refs.color, s.color);
    s.cutoffHz       = readCachedParamValue(refs.cutoff, s.cutoffHz);
    s.pan            = readCachedParamValue(refs.pan, s.pan);
    s.oneShot        = readCachedParamValue(refs.oneShot, s.oneShot) >= 0.5f;
    s.oneShotDecayMs = readCachedParamValue(refs.oneShotDecayMs, s.oneShotDecayMs);
    return sanitizeInstrSettings(instrIndex, s);
}

mpc::InstrSettings PercSynthAudioProcessor::snapshotInstrSettings(int instrIndex) const
{
    auto settings = captureBaseInstrSettings(instrIndex);
    applyPerformanceMacros(instrIndex, settings);
    return settings;
}

mpc::GlobalFxSettings PercSynthAudioProcessor::snapshotFxSettings() const
{
    mpc::GlobalFxSettings f;
    f.satDrive          = readCachedParamValue(globalParamRefs.satDrive, f.satDrive);
    f.satMix            = readCachedParamValue(globalParamRefs.satMix, f.satMix);
    f.transientAttack   = readCachedParamValue(globalParamRefs.transientAttack, f.transientAttack);
    f.transientSustain  = readCachedParamValue(globalParamRefs.transientSustain, f.transientSustain);
    f.transientMix      = readCachedParamValue(globalParamRefs.transientMix, f.transientMix);
    f.compThreshold     = readCachedParamValue(globalParamRefs.compThreshold, f.compThreshold);
    f.compRatio         = readCachedParamValue(globalParamRefs.compRatio, f.compRatio);
    f.compAttack        = readCachedParamValue(globalParamRefs.compAttack, f.compAttack);
    f.compRelease       = readCachedParamValue(globalParamRefs.compRelease, f.compRelease);
    f.compMakeup        = readCachedParamValue(globalParamRefs.compMakeup, f.compMakeup);
    f.compMix           = readCachedParamValue(globalParamRefs.compMix, f.compMix);
    f.eqLowFreq         = readCachedParamValue(globalParamRefs.eqLowFreq, f.eqLowFreq);
    f.eqLowGain         = readCachedParamValue(globalParamRefs.eqLowGain, f.eqLowGain);
    f.eqMidFreq         = readCachedParamValue(globalParamRefs.eqMidFreq, f.eqMidFreq);
    f.eqMidGain         = readCachedParamValue(globalParamRefs.eqMidGain, f.eqMidGain);
    f.eqMidQ            = readCachedParamValue(globalParamRefs.eqMidQ, f.eqMidQ);
    f.eqHighFreq        = readCachedParamValue(globalParamRefs.eqHighFreq, f.eqHighFreq);
    f.eqHighGain        = readCachedParamValue(globalParamRefs.eqHighGain, f.eqHighGain);
    f.chorusRate        = readCachedParamValue(globalParamRefs.chorusRate, f.chorusRate);
    f.chorusDepth       = readCachedParamValue(globalParamRefs.chorusDepth, f.chorusDepth);
    f.chorusMix         = readCachedParamValue(globalParamRefs.chorusMix, f.chorusMix);
    f.delayTime         = readCachedParamValue(globalParamRefs.delayTime, f.delayTime);
    f.delayFeedback     = readCachedParamValue(globalParamRefs.delayFeedback, f.delayFeedback);
    f.delayMix          = readCachedParamValue(globalParamRefs.delayMix, f.delayMix);
    f.reverbSize        = readCachedParamValue(globalParamRefs.reverbSize, f.reverbSize);
    f.reverbDamping     = readCachedParamValue(globalParamRefs.reverbDamping, f.reverbDamping);
    f.reverbWidth       = readCachedParamValue(globalParamRefs.reverbWidth, f.reverbWidth);
    f.reverbMix         = readCachedParamValue(globalParamRefs.reverbMix, f.reverbMix);
    f.reverbPredelay    = readCachedParamValue(globalParamRefs.reverbPredelay, f.reverbPredelay);
    f.limiterThreshold  = readCachedParamValue(globalParamRefs.limiterThreshold, f.limiterThreshold);
    f.limiterRelease    = readCachedParamValue(globalParamRefs.limiterRelease, f.limiterRelease);
    f.saturatorOn       = readCachedParamValue(globalParamRefs.fxSatEnable) >= 0.5f;
    f.transientOn       = readCachedParamValue(globalParamRefs.fxTransientEnable) >= 0.5f;
    f.compressorOn      = readCachedParamValue(globalParamRefs.fxCompEnable) >= 0.5f;
    f.reverbOn          = readCachedParamValue(globalParamRefs.fxReverbEnable) >= 0.5f;
    f.eqOn              = readCachedParamValue(globalParamRefs.fxEqEnable) >= 0.5f;
    f.chorusOn          = readCachedParamValue(globalParamRefs.fxChorusEnable) >= 0.5f;
    f.delayOn           = readCachedParamValue(globalParamRefs.fxDelayEnable) >= 0.5f;
    f.limiterOn         = readCachedParamValue(globalParamRefs.fxLimiterEnable) >= 0.5f;
    return sanitizeFxSettings(f);
}

PercSynthAudioProcessor::PresetPersistenceState
PercSynthAudioProcessor::captureCurrentPresetState(int instrIndex) const
{
    auto state = makeDefaultPresetState(instrIndex);
    state.instrIndex = juce::jlimit(0, mpc::kNumInstruments - 1, instrIndex);
    state.settings = captureBaseInstrSettings(state.instrIndex);
    state.outputBus = captureInstrOutputBus(state.instrIndex);
    state.fx = mpc::maskUnavailableFx(state.instrIndex,
                                      sanitizeFxSettings(instrumentFxStates[static_cast<std::size_t>(state.instrIndex)]));
    state.qualityMode = juce::jlimit(0, 1, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.qualityMode))));
    state.delaySync = juce::jlimit(0, 1, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.delaySync))));
    state.delayDivision = juce::jlimit(0, 5, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.delayDivision, 1.0f))));
    state.lfoRate = juce::jlimit(0.05f, 12.0f, readCachedParamValue(globalParamRefs.lfoRate, 1.8f));
    state.lfoDepth = juce::jlimit(0.0f, 1.0f, readCachedParamValue(globalParamRefs.lfoDepth));
    state.lfoWave = juce::jlimit(0, 3, static_cast<int>(std::round(readCachedParamValue(globalParamRefs.lfoWave))));
    state.macroImpact = clamp01(readCachedParamValue(globalParamRefs.macroImpact, 0.5f));
    state.macroResonance = clamp01(readCachedParamValue(globalParamRefs.macroResonance, 0.5f));
    state.macroSpace = clamp01(readCachedParamValue(globalParamRefs.macroSpace, 0.5f));
    state.macroCouleur = clamp01(readCachedParamValue(globalParamRefs.macroCouleur, 0.5f));
    state.modMatrix = modulationMatrix.captureState();
    state.metadata = makeUserMetadata(state.instrIndex);
    return state;
}

PercSynthAudioProcessor::PresetPersistenceState
PercSynthAudioProcessor::makeFactoryPresetState(int instrIndex,
                                                int presetIndex,
                                                const mpc::InstrumentPreset& preset) const
{
    auto state = makeDefaultPresetState(instrIndex);
    state.name = juce::String(juce::CharPointer_UTF8(preset.name.c_str()));
    state.instrIndex = juce::jlimit(0, mpc::kNumInstruments - 1, instrIndex);
    state.presetIndex = presetIndex;
    state.settings = sanitizeInstrSettings(state.instrIndex, preset.settings);
    state.fx = mpc::maskUnavailableFx(state.instrIndex, sanitizeFxSettings(preset.fx));
    state.outputBus = juce::jlimit(0, kNumAuxOutputs, preset.outputBus);
    state.metadata = makeFactoryMetadata(preset.metadata);
    return state;
}

void PercSynthAudioProcessor::applyPresetPersistenceState(const PresetPersistenceState& state,
                                                          bool notifyHost)
{
    const auto instrIndex = juce::jlimit(0, mpc::kNumInstruments - 1, state.instrIndex);
    applyInstrPresetSettings(instrIndex, state.settings, notifyHost);
    setParamValueInternal(makeInstrParamId(instrIndex, kInstrOutputSuffix),
                          static_cast<float>(juce::jlimit(0, kNumAuxOutputs, state.outputBus)),
                          notifyHost);
    outputBusCache[static_cast<std::size_t>(instrIndex)] = juce::jlimit(0, kNumAuxOutputs, state.outputBus);
    markOutputBusCacheDirty();

    const auto maskedFx = mpc::maskUnavailableFx(instrIndex, sanitizeFxSettings(state.fx));
    instrumentFxStates[static_cast<std::size_t>(instrIndex)] = maskedFx;
    applyFxToParams(instrIndex, maskedFx, notifyHost);

    setParamValueInternal(kQualityMode, static_cast<float>(juce::jlimit(0, 1, state.qualityMode)), notifyHost);
    setParamValueInternal(kDelaySync, static_cast<float>(juce::jlimit(0, 1, state.delaySync)), notifyHost);
    setParamValueInternal(kDelayDivision, static_cast<float>(juce::jlimit(0, 5, state.delayDivision)), notifyHost);
    setParamValueInternal(kLfoRate, juce::jlimit(0.05f, 12.0f, state.lfoRate), notifyHost);
    setParamValueInternal(kLfoDepth, juce::jlimit(0.0f, 1.0f, state.lfoDepth), notifyHost);
    setParamValueInternal(kLfoWave, static_cast<float>(juce::jlimit(0, 3, state.lfoWave)), notifyHost);
    setParamValueInternal(kMacroImpact, clamp01(state.macroImpact), notifyHost);
    setParamValueInternal(kMacroResonance, clamp01(state.macroResonance), notifyHost);
    setParamValueInternal(kMacroSpace, clamp01(state.macroSpace), notifyHost);
    setParamValueInternal(kMacroCouleur, clamp01(state.macroCouleur), notifyHost);

    modulationMatrix.applyState(state.modMatrix);
    modulationMatrix.resetMidiSources();
    sanitizeAllParameters();
}

void PercSynthAudioProcessor::applyFxToParams(const int instrIndex,
                                              const mpc::GlobalFxSettings& f,
                                              bool notifyHost)
{
    const auto masked = mpc::maskUnavailableFx(instrIndex, sanitizeFxSettings(f));

    setParamValueInternal(kSatDrive,          masked.satDrive, notifyHost);
    setParamValueInternal(kSatMix,            masked.satMix, notifyHost);
    setParamValueInternal(kTransientAttack,   masked.transientAttack, notifyHost);
    setParamValueInternal(kTransientSustain,  masked.transientSustain, notifyHost);
    setParamValueInternal(kTransientMix,      masked.transientMix, notifyHost);
    setParamValueInternal(kCompThreshold,     masked.compThreshold, notifyHost);
    setParamValueInternal(kCompRatio,         masked.compRatio, notifyHost);
    setParamValueInternal(kCompAttack,        masked.compAttack, notifyHost);
    setParamValueInternal(kCompRelease,       masked.compRelease, notifyHost);
    setParamValueInternal(kCompMakeup,        masked.compMakeup, notifyHost);
    setParamValueInternal(kCompMix,           masked.compMix, notifyHost);
    setParamValueInternal(kEqLowFreq,         masked.eqLowFreq, notifyHost);
    setParamValueInternal(kEqLowGain,         masked.eqLowGain, notifyHost);
    setParamValueInternal(kEqMidFreq,         masked.eqMidFreq, notifyHost);
    setParamValueInternal(kEqMidGain,         masked.eqMidGain, notifyHost);
    setParamValueInternal(kEqMidQ,            masked.eqMidQ, notifyHost);
    setParamValueInternal(kEqHighFreq,        masked.eqHighFreq, notifyHost);
    setParamValueInternal(kEqHighGain,        masked.eqHighGain, notifyHost);
    setParamValueInternal(kChorusRate,        masked.chorusRate, notifyHost);
    setParamValueInternal(kChorusDepth,       masked.chorusDepth, notifyHost);
    setParamValueInternal(kChorusMix,         masked.chorusMix, notifyHost);
    setParamValueInternal(kDelayTime,         masked.delayTime, notifyHost);
    setParamValueInternal(kDelayFeedback,     masked.delayFeedback, notifyHost);
    setParamValueInternal(kDelayMix,          masked.delayMix, notifyHost);
    setParamValueInternal(kReverbSize,        masked.reverbSize, notifyHost);
    setParamValueInternal(kReverbDamping,     masked.reverbDamping, notifyHost);
    setParamValueInternal(kReverbWidth,       masked.reverbWidth, notifyHost);
    setParamValueInternal(kReverbMix,         masked.reverbMix, notifyHost);
    setParamValueInternal(kReverbPredelay,    masked.reverbPredelay, notifyHost);
    setParamValueInternal(kLimiterThreshold,  masked.limiterThreshold, notifyHost);
    setParamValueInternal(kLimiterRelease,    masked.limiterRelease, notifyHost);

    setParamValueInternal(kFxSatEnable,       masked.saturatorOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kFxTransientEnable, masked.transientOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kFxEqEnable,        masked.eqOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kFxCompEnable,      masked.compressorOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kFxChorusEnable,    masked.chorusOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kFxDelayEnable,     masked.delayOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kFxReverbEnable,    masked.reverbOn ? 1.0f : 0.0f, notifyHost);
    setParamValueInternal(kFxLimiterEnable,   masked.limiterOn ? 1.0f : 0.0f, notifyHost);
}

void PercSynthAudioProcessor::storeCurrentInstrumentFxState(int instrIndex)
{
    if (instrIndex < 0 || instrIndex >= mpc::kNumInstruments)
        return;

    instrumentFxStates[static_cast<std::size_t>(instrIndex)] = snapshotFxSettings();
}

void PercSynthAudioProcessor::restoreInstrumentFxState(int instrIndex)
{
    if (instrIndex < 0 || instrIndex >= mpc::kNumInstruments)
        return;

    applyFxToParams(instrIndex, instrumentFxStates[static_cast<std::size_t>(instrIndex)]);
}

bool PercSynthAudioProcessor::isFxAvailableForCurrentInstrument(mpc::GlobalFxSlot slot) const
{
    return mpc::isFxAvailable(getSelectedInstrIndex(), slot);
}

void PercSynthAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (parameterID.endsWith("_" + juce::String(kInstrOutputSuffix)))
        markOutputBusCacheDirty();

    if (parameterID != kSelectedInstr)
        return;

    pendingSelectedInstrumentIndex.store(juce::jlimit(
        0, mpc::kNumInstruments - 1, static_cast<int>(std::round(newValue))));

    if (isRestoringState.load(std::memory_order_acquire))
        return;

    triggerAsyncUpdate();
}

void PercSynthAudioProcessor::handleAsyncUpdate()
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

void PercSynthAudioProcessor::applyPerformanceMacros(int instrIndex, mpc::InstrSettings& s) const
{
    const auto impact    = (readCachedParamValue(globalParamRefs.macroImpact, 0.5f)    - 0.5f) * 2.0f;
    const auto resonance = (readCachedParamValue(globalParamRefs.macroResonance, 0.5f) - 0.5f) * 2.0f;
    const auto space     = (readCachedParamValue(globalParamRefs.macroSpace, 0.5f)     - 0.5f) * 2.0f;
    const auto couleur   = (readCachedParamValue(globalParamRefs.macroCouleur, 0.5f)   - 0.5f) * 2.0f;

    const auto family = mpc::getFamily(instrIndex);

    // Impact: sharper attack, more noise burst
    s.attackSeconds = juce::jlimit(0.0f, 2.0f, s.attackSeconds * (1.0f - impact * 0.35f));
    s.noise         = clamp01(s.noise + impact * 0.20f);
    s.level         = clamp01(s.level + impact * 0.05f);

    // Résonance: more body, longer decay, less damping
    s.body         = clamp01(s.body + resonance * 0.18f);
    s.decaySeconds = juce::jlimit(0.1f, 10.0f, s.decaySeconds * (1.0f + resonance * 0.30f));
    s.damping      = clamp01(s.damping - resonance * 0.12f);

    // Espace: stereo width, longer release
    s.stereoWidth    = clamp01(s.stereoWidth + space * 0.20f);
    s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + space * 0.30f));

    // Couleur: color shift, brightness, cutoff
    s.color      = clamp01(s.color + couleur * 0.18f);
    s.brightness = clamp01(s.brightness + couleur * 0.15f);
    s.cutoffHz   = juce::jlimit(120.0f, 16000.0f, s.cutoffHz * std::pow(2.0f, couleur * 0.45f));

    // Family-specific tweaks
    if (family == mpc::Family::Percussions)
        s.noise = clamp01(s.noise + impact * 0.08f);
    else if (family == mpc::Family::Ambiance)
        s.releaseSeconds = juce::jlimit(0.01f, 5.0f, s.releaseSeconds * (1.0f + space * 0.10f));
    else if (family == mpc::Family::Metalliques)
        s.damping = clamp01(s.damping - resonance * 0.06f);
}

int PercSynthAudioProcessor::countActiveVoicesForFamily(int familyIdx) const
{
    int count = 0;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        const auto& v = voices[static_cast<std::size_t>(i)];
        if (v.active && v.active->isActive() && v.instrIndex / 3 == familyIdx)
            ++count;
    }
    return count;
}

int PercSynthAudioProcessor::findFreeVoice(int instrIndex) const
{
    // 1. Any inactive voice
    for (int i = 0; i < kMaxVoices; ++i)
        if (!voices[static_cast<std::size_t>(i)].active || !voices[static_cast<std::size_t>(i)].active->isActive())
            return i;

    // 2. Family limit enforcement — steal oldest voice in same family before crossing families
    const int familyIdx = instrIndex / 3;
    if (countActiveVoicesForFamily(familyIdx) >= kMaxVoicesPerFamily[familyIdx])
    {
        int oldest = 0;
        uint64_t oldestAge = UINT64_MAX;
        for (int i = 0; i < kMaxVoices; ++i)
        {
            const auto& v = voices[static_cast<std::size_t>(i)];
            if (v.active && v.instrIndex / 3 == familyIdx && v.activationAge < oldestAge)
            {
                oldest = i;
                oldestAge = v.activationAge;
            }
        }
        if (oldestAge < UINT64_MAX) return oldest;
    }

    // 3. Steal oldest releasing voice (any family)
    int oldest = 0;
    uint64_t oldestAge = UINT64_MAX;
    for (int i = 0; i < kMaxVoices; ++i)
    {
        if (voices[static_cast<std::size_t>(i)].active && voices[static_cast<std::size_t>(i)].active->isReleasing()
            && voices[static_cast<std::size_t>(i)].activationAge < oldestAge)
        {
            oldest = i;
            oldestAge = voices[static_cast<std::size_t>(i)].activationAge;
        }
    }
    if (oldestAge < UINT64_MAX) return oldest;

    // 4. Steal oldest active voice (any family)
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

void PercSynthAudioProcessor::triggerNoteOn(int instrIndex, int midiNote, float velocity)
{
    if (instrIndex < 0 || instrIndex >= mpc::kNumInstruments) return;
    if (preparedSampleRate <= 0.0) return;

    const int slot = findFreeVoice(instrIndex);
    auto& v = voices[static_cast<std::size_t>(slot)];
    if (v.active && v.active->isActive())
    {
        if (v.dying && v.dying->isActive())
            v.dying->reset();

        if (v.instrIndex != instrIndex)
        {
            // Cross-instrument steal: let old voice fade out as dying.
            v.dying = v.active;
            v.dying->forceQuickRelease();
            v.dyingBus = outputBusCache[static_cast<std::size_t>(juce::jlimit(0, mpc::kNumInstruments - 1, v.instrIndex))];
            v.dyingVelocity = v.velocity;
        }
        else
        {
            // Same-instrument steal needs a separate preallocated voice object; otherwise active
            // and dying would alias the same voiceBank entry.
            auto* tailVoice = v.dyingVoiceBank[static_cast<std::size_t>(instrIndex)].get();
            if (tailVoice != nullptr && tailVoice != v.active)
            {
                const auto previousSettings = snapshotInstrSettings(instrIndex);
                tailVoice->noteOn(previousSettings, v.midiNote, v.velocity, preparedSampleRate);
                mpc::VoiceModulation duckingMod;
                duckingMod.duckingMul = 0.15f;
                tailVoice->setVoiceModulation(duckingMod, preparedSampleRate);
                tailVoice->forceQuickRelease();
                v.dying = tailVoice;
                v.dyingBus = outputBusCache[static_cast<std::size_t>(juce::jlimit(0, mpc::kNumInstruments - 1, v.instrIndex))];
                v.dyingVelocity = v.velocity;
            }
            else
            {
                jassertfalse;
                mpc::VoiceModulation duckingMod;
                duckingMod.duckingMul = 0.15f;
                v.active->setVoiceModulation(duckingMod, preparedSampleRate);
                v.active->noteOff();
            }
        }
    }
    v.midiNote = midiNote;
    v.activationAge = ++voiceAgeCounter;
    v.instrIndex = instrIndex;
    v.velocity = velocity;
    v.active = v.voiceBank[static_cast<std::size_t>(instrIndex)].get();

    auto settings = snapshotInstrSettings(instrIndex);
    if (v.active)
        v.active->noteOn(settings, midiNote, velocity, preparedSampleRate);
}

void PercSynthAudioProcessor::triggerNoteOff(int instrIndex, int midiNote)
{
    if (sustainHeld)
    {
        addSustainedNote(instrIndex, midiNote);
        return;
    }

    for (auto& slot : voices)
    {
        if (slot.active && slot.active->isActive() && !slot.active->isReleasing() &&
            slot.midiNote == midiNote && slot.instrIndex == instrIndex)
        {
            slot.active->noteOff();
        }
    }
}

// =============================================================================
void PercSynthAudioProcessor::panicAllVoices()
{
    pitchBend.reset();
    modulationMatrix.resetMidiSources();
    sustainHeld = false;
    clearSustainedNotes();

    for (auto& slot : voices)
    {
        for (auto& voice : slot.voiceBank)
            if (voice)
                voice->reset();
        for (auto& voice : slot.dyingVoiceBank)
            if (voice)
                voice->reset();
        slot.active = nullptr;
        slot.dying = nullptr;
        slot.midiNote = -1;
        slot.instrIndex = 0;
        slot.velocity = 0.0f;
        slot.dyingVelocity = 0.0f;
        slot.dyingBus = 0;
        slot.activationAge = 0;
    }

    compressor.reset();
    compCache = CompressorCache{};
    fxTransient.reset();
    fxEQ.reset();
    fxChorus.reset();
    fxDelay.reset();
    fxReverb.reset();
    fxLimiter.reset();
    saturatorPrevInput = { 0.0f, 0.0f };
    saturatorPrevAdaaInput = { 0.0f, 0.0f };
    lfoPhase = 0.0f;
    modulationMatrix.lfo2.reset();
}

void PercSynthAudioProcessor::releaseVoices(int midiChannel, bool immediate)
{
    juce::ignoreUnused(midiChannel);

    sustainHeld = false;
    clearSustainedNotes();

    for (auto& slot : voices)
    {
        if (!slot.active || !slot.active->isActive())
            continue;

        if (immediate)
        {
            slot.active->forceQuickRelease();
            slot.midiNote = -1;
            slot.instrIndex = 0;
            slot.velocity = 0.0f;
            slot.activationAge = 0;
        }
        else
        {
            slot.active->forceQuickRelease();
        }
    }
}

void PercSynthAudioProcessor::clearSustainedNotes() noexcept
{
    sustainedNoteCount = 0;
}

bool PercSynthAudioProcessor::addSustainedNote(int instrIndex, int midiNote) noexcept
{
    const auto key = std::make_pair(instrIndex, midiNote);
    for (int i = 0; i < sustainedNoteCount; ++i)
        if (sustainedNotes[static_cast<std::size_t>(i)] == key)
            return true;

    if (sustainedNoteCount >= kMaxSustainedNotes)
        return false;

    sustainedNotes[static_cast<std::size_t>(sustainedNoteCount)] = key;
    ++sustainedNoteCount;
    return true;
}

void PercSynthAudioProcessor::releaseSustainedNotes()
{
    sustainHeld = false;
    const int notesToRelease = sustainedNoteCount;
    for (int i = 0; i < notesToRelease; ++i)
    {
        const auto [instrIndex, midiNote] = sustainedNotes[static_cast<std::size_t>(i)];
        triggerNoteOff(instrIndex, midiNote);
    }
    clearSustainedNotes();
}

// =============================================================================
// FLkey Mini CC page system
// =============================================================================
const char* PercSynthAudioProcessor::getCCPageName(int page) noexcept
{
    if (page >= 0 && page < kNumCCPages)
        return kCCPageNames[page];
    return "???";
}

void PercSynthAudioProcessor::handleMidiCC(int ccNumber, int ccValue, int instrIndex)
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

    // --- Knob mapping: CC 21-28 -> paged parameters ---
    if (ccNumber < 21 || ccNumber > 28)
        return;

    const int knobIndex = ccNumber - 21;
    const int page = juce::jlimit(0, kNumCCPages - 1, midiCCPage.load(std::memory_order_relaxed));
    const int safeInstr = juce::jlimit(0, mpc::kNumInstruments - 1, instrIndex);

    auto* parameter = ccGlobalBindings[static_cast<std::size_t>(page)][static_cast<std::size_t>(knobIndex)];
    if (parameter == nullptr)
        parameter = ccInstrumentBindings[static_cast<std::size_t>(safeInstr)]
            [static_cast<std::size_t>(page)]
            [static_cast<std::size_t>(knobIndex)];

    if (parameter == nullptr)
        return;

    queueParamUpdate(parameter, static_cast<float>(ccValue) / 127.0f);
}

// =============================================================================
void PercSynthAudioProcessor::processMasterFxChain(juce::AudioBuffer<float>& mainBuffer,
                                                   const GlobalBlockState& blockState)
{
    if (mainBuffer.getNumChannels() <= 0 || mainBuffer.getNumSamples() <= 0)
        return;

    processGlobalTransient(mainBuffer, blockState);
    processGlobalSaturator(mainBuffer, blockState);
    processGlobalCompressor(mainBuffer, blockState);
    processGlobalEQ(mainBuffer, blockState);
    processGlobalChorus(mainBuffer, blockState);
    applyGlobalLfo(mainBuffer, blockState);
    processGlobalDelay(mainBuffer, blockState);
    processGlobalReverb(mainBuffer, blockState);
    mainBuffer.applyGain(juce::Decibels::decibelsToGain(blockState.outputGainDb));
    processGlobalLimiter(mainBuffer, blockState);
}

// =============================================================================
void PercSynthAudioProcessor::processGlobalTransient(juce::AudioBuffer<float>& mainBuffer,
                                                     const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Transient)) return;
    if (!blockState.fx.transientOn) return;

    mpc::fx::TransientShaper::Params params;
    params.attack = blockState.fx.transientAttack;
    params.sustain = blockState.fx.transientSustain;
    params.mix = blockState.fx.transientMix;

    auto* left = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    fxTransient.process(left, right, mainBuffer.getNumSamples(), params);
}

void PercSynthAudioProcessor::processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer,
                                                     const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Saturator) && satMixCurrent <= 0.0001f)
        return;

    const bool effectActive = mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Saturator)
        && blockState.fx.saturatorOn;
    const auto targetMix = effectActive ? clamp01(blockState.fx.satMix) : 0.0f;
    const auto targetDrive = juce::jlimit(1.0f, 16.0f, blockState.fx.satDrive);
    if (targetMix <= 0.0001f && satMixCurrent <= 0.0001f)
        return;

    const int numSamples = mainBuffer.getNumSamples();
    if (numSamples <= 0)
        return;
    const float mixStep = numSamples > 0 ? (targetMix - satMixCurrent) / static_cast<float>(numSamples) : 0.0f;
    const float driveStep = numSamples > 0 ? (targetDrive - satDriveCurrent) / static_cast<float>(numSamples) : 0.0f;

    if (blockState.qualityMode == static_cast<int>(QualityMode::Studio))
    {
        if (fxDryBuffer.getNumChannels() < mainBuffer.getNumChannels()
            || fxDryBuffer.getNumSamples() < numSamples)
        {
            jassertfalse;
            satDriveCurrent = targetDrive;
            satMixCurrent = targetMix;
            return;
        }

        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
            fxDryBuffer.copyFrom(ch, 0, mainBuffer, ch, 0, numSamples);

        juce::dsp::AudioBlock<float> fullBlock(mainBuffer);
        auto block = fullBlock.getSubsetChannelBlock(0, static_cast<std::size_t>(juce::jmin(2, mainBuffer.getNumChannels())));
        auto& oversampler = mainBuffer.getNumChannels() == 1 ? satOversamplingMono : satOversamplingStereo;
        auto osBlock = oversampler.processSamplesUp(block);
        const float norm = 1.0f / std::max(0.0001f, std::tanh(targetDrive));
        for (size_t ch = 0; ch < osBlock.getNumChannels(); ++ch)
        {
            auto* data = osBlock.getChannelPointer(ch);
            for (size_t i = 0; i < osBlock.getNumSamples(); ++i)
                data[i] = std::tanh(data[i] * targetDrive) * norm;
        }
        oversampler.processSamplesDown(block);

        for (int ch = 0; ch < mainBuffer.getNumChannels(); ++ch)
        {
            auto* wet = mainBuffer.getWritePointer(ch);
            const auto* dry = fxDryBuffer.getReadPointer(ch);
            float mix = satMixCurrent;
            for (int i = 0; i < numSamples; ++i)
            {
                wet[i] = dry[i] + (wet[i] - dry[i]) * mix;
                mix += mixStep;
            }
            saturatorPrevInput[static_cast<std::size_t>(juce::jlimit(0, 1, ch))] = dry[numSamples - 1];
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
            const float wet = tanhAdaa(dry, saturatorPrevAdaaInput[static_cast<std::size_t>(juce::jlimit(0, 1, ch))], drive);

            data[i] = dry + (wet - dry) * mix;
            prevInput = dry;
            drive += driveStep;
            mix += mixStep;
        }
    }

    satDriveCurrent = targetDrive;
    satMixCurrent = targetMix;
}

void PercSynthAudioProcessor::processGlobalEQ(juce::AudioBuffer<float>& mainBuffer,
                                              const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Eq)) return;
    if (!blockState.fx.eqOn) return;

    mpc::fx::ParametricEQ3Band::Params params;
    params.lowFreq = blockState.fx.eqLowFreq;
    params.lowGainDb = blockState.fx.eqLowGain;
    params.midFreq = blockState.fx.eqMidFreq;
    params.midGainDb = blockState.fx.eqMidGain;
    params.midQ = blockState.fx.eqMidQ;
    params.highFreq = blockState.fx.eqHighFreq;
    params.highGainDb = blockState.fx.eqHighGain;

    auto* left = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    fxEQ.process(left, right, mainBuffer.getNumSamples(), params);
}

void PercSynthAudioProcessor::processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer,
                                                      const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Compressor)) return;
    if (!blockState.fx.compressorOn) return;

    const auto mix = clamp01(blockState.fx.compMix);
    const auto makeupGain = juce::Decibels::decibelsToGain(blockState.fx.compMakeup);
    if (mix <= 0.0001f && std::abs(makeupGain - 1.0f) <= 0.0001f)
        return;

    const auto threshold = blockState.fx.compThreshold;
    const auto ratio = blockState.fx.compRatio;
    const auto attack = blockState.fx.compAttack;
    const auto release = blockState.fx.compRelease;

    if (threshold != compCache.threshold) { compressor.setThreshold(threshold); compCache.threshold = threshold; }
    if (ratio != compCache.ratio) { compressor.setRatio(ratio); compCache.ratio = ratio; }
    if (attack != compCache.attack) { compressor.setAttack(attack); compCache.attack = attack; }
    if (release != compCache.release) { compressor.setRelease(release); compCache.release = release; }

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

void PercSynthAudioProcessor::processGlobalChorus(juce::AudioBuffer<float>& mainBuffer,
                                                  const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Chorus)) return;
    if (!blockState.fx.chorusOn) return;

    mpc::fx::StereoChorus::Params params;
    params.rateHz = blockState.fx.chorusRate;
    params.depth = blockState.fx.chorusDepth;
    params.mix = blockState.fx.chorusMix;

    auto* left = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    fxChorus.process(left, right, mainBuffer.getNumSamples(), params);
}

void PercSynthAudioProcessor::processGlobalDelay(juce::AudioBuffer<float>& mainBuffer,
                                                 const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Delay)) return;
    if (!blockState.fx.delayOn) return;

    mpc::fx::StereoDelay::Params params;
    params.timeMs = blockState.fx.delayTime;
    if (blockState.delaySyncToHost)
    {
        static constexpr std::array<float, 6> kDelayBeats { 1.0f, 0.5f, 0.75f, 1.0f / 3.0f, 0.25f, 0.375f };
        const auto bpm = juce::jlimit(20.0f, 320.0f, blockState.hostBpm);
        const auto beatSeconds = 60.0f / juce::jmax(1.0f, bpm);
        const auto beats = kDelayBeats[static_cast<std::size_t>(juce::jlimit(0, 5, blockState.delayDivision))];
        params.timeMs = beatSeconds * beats * 1000.0f;
    }
    params.feedback = blockState.fx.delayFeedback;
    params.mix = blockState.fx.delayMix;

    auto* left = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    fxDelay.process(left, right, mainBuffer.getNumSamples(), params);
}

void PercSynthAudioProcessor::processGlobalReverb(juce::AudioBuffer<float>& mainBuffer,
                                                  const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Reverb)) return;
    if (!blockState.fx.reverbOn) return;

    mpc::fx::DattorroPlateReverb::Params params;
    params.decay = blockState.fx.reverbSize;
    params.damping = blockState.fx.reverbDamping;
    params.width = blockState.fx.reverbWidth;
    params.mix = blockState.fx.reverbMix;
    params.preDelayMs = blockState.fx.reverbPredelay;

    auto* left = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    fxReverb.process(left, right, mainBuffer.getNumSamples(), params);
}

void PercSynthAudioProcessor::processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer,
                                                   const GlobalBlockState& blockState)
{
    if (!mpc::isFxAvailable(blockState.selectedInstrument, mpc::GlobalFxSlot::Limiter)) return;
    if (!blockState.fx.limiterOn) return;

    mpc::fx::OutputLimiter::Params params;
    params.thresholdDb = blockState.fx.limiterThreshold;
    params.releaseMs = blockState.fx.limiterRelease;

    auto* left = mainBuffer.getWritePointer(0);
    auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getWritePointer(1) : nullptr;
    fxLimiter.process(left, right, mainBuffer.getNumSamples(), params);
}

void PercSynthAudioProcessor::applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer,
                                             const GlobalBlockState& blockState)
{
    const auto numCh = mainBuffer.getNumChannels();
    const auto numSamples = mainBuffer.getNumSamples();
    if (numCh <= 0 || numSamples <= 0) return;

    const float rateHz = juce::jlimit(0.05f, 12.0f, blockState.lfoRate);
    const float depth  = clamp01(blockState.lfoDepth);
    if (depth <= 0.0001f) return;

    const int wave = juce::jlimit(0, 3, blockState.lfoWave);
    const float phaseInc = rateHz / static_cast<float>(juce::jmax(1.0, preparedSampleRate));
    constexpr float kTremDepth = 0.25f;
    constexpr float kPanDepth  = 0.80f;

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
            default: lfo = std::sin(lfoPhase * juce::MathConstants<float>::twoPi); break;
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

void PercSynthAudioProcessor::updateOutputMeters(juce::AudioBuffer<float>& fullBuffer,
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
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PercSynthAudioProcessor();
}
