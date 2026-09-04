#include "FactoryPresets.h"

#include <algorithm>

namespace mpc
{

namespace
{
const char* familyGroupLabel(const int instrIndex)
{
    switch (getFamily(instrIndex))
    {
        case Family::Percussions: return "percussions";
        case Family::Ambiance:    return "ambiance";
        case Family::Metalliques: return "metalliques";
    }
    return "percussions";
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

void appendTagIfMissing(std::vector<std::string>& tags, const std::string& tag)
{
    if (tag.empty())
        return;
    if (std::find(tags.begin(), tags.end(), tag) == tags.end())
        tags.push_back(tag);
}

int defaultOutputBusForRole(const std::string& mixRole)
{
    if (mixRole == "tight") return 1;
    if (mixRole == "cinematic") return 2;
    if (mixRole == "ambient") return 3;
    if (mixRole == "fx") return 4;
    return 0;
}

std::string outputProfileFor(const std::string& mixRole, const int outputBus)
{
    if (mixRole == "tight") return outputBus > 0 ? "main-plus-aux1-transient" : "main-transient";
    if (mixRole == "cinematic") return outputBus > 0 ? "main-plus-aux2-tail" : "main-score";
    if (mixRole == "ambient") return outputBus > 0 ? "main-plus-aux3-texture" : "main-texture";
    if (mixRole == "fx") return outputBus > 0 ? "main-plus-aux4-design" : "main-design";
    if (mixRole == "room") return "main-room";
    return "main-dry";
}

std::string descriptionFor(const std::string& instrumentLabel, const std::string& mixRole)
{
    if (mixRole == "dry")
        return instrumentLabel + " close preset for dry production layers and defined transient detail.";
    if (mixRole == "room")
        return instrumentLabel + " natural room preset with controlled bloom and mix-ready ambience.";
    if (mixRole == "cinematic")
        return instrumentLabel + " extended tail preset for score accents, trailers and dramatic transitions.";
    if (mixRole == "ambient")
        return instrumentLabel + " wide texture preset for beds, drones and atmospheric percussion layers.";
    if (mixRole == "tight")
        return instrumentLabel + " focused transient preset for short rhythmic accents and dense arrangements.";
    if (mixRole == "fx")
        return instrumentLabel + " designed layer preset for hybrid percussion, motion and sound design stacks.";
    return instrumentLabel + " production preset.";
}

PresetMetadata makeMetadata(const char* familyLabel,
                            const char* mixRole,
                            float nominalPeakDb,
                            std::initializer_list<const char*> extraTags)
{
    PresetMetadata metadata;
    metadata.mixRole = mixRole;
    metadata.familyLabel = familyLabel;
    metadata.nominalPeakDb = nominalPeakDb;
    metadata.tags = { "perc", "factory", familyLabel, mixRole };
    for (const auto* tag : extraTags)
        metadata.tags.emplace_back(tag);
    metadata.description = descriptionFor(humanizeSlug(familyLabel), mixRole);
    metadata.outputProfile = outputProfileFor(metadata.mixRole, defaultOutputBusForRole(metadata.mixRole));
    return metadata;
}

float instrumentLevelScale(int instrIndex)
{
    static constexpr float scales[kNumInstruments] = {
        0.66f, // Timbales
        0.76f, // Marimba
        0.80f, // Djembe
        0.95f, // Baton de pluie
        0.42f, // Bol chantant
        0.48f, // Carillon eolien
        0.32f, // Cloche tubulaire
        0.32f, // Triangle
        0.30f  // Glockenspiel
    };

    return scales[std::clamp(instrIndex, 0, kNumInstruments - 1)];
}

float roleLevelScale(const std::string& mixRole)
{
    if (mixRole == "dry") return 0.80f;
    if (mixRole == "ambient") return 0.50f;
    if (mixRole == "cinematic") return 0.50f;
    if (mixRole == "fx") return 0.50f;
    if (mixRole == "tight") return 0.50f;
    return 1.0f;
}

std::vector<InstrumentPreset> finalizeBank(int instrIndex, std::vector<InstrumentPreset> bank)
{
    const auto levelScale = instrumentLevelScale(instrIndex);
    const auto familyLabel = std::string(familyGroupLabel(instrIndex));
    for (auto& preset : bank)
    {
        float roleScale = roleLevelScale(preset.metadata.mixRole);
        if (instrIndex == 3)
        {
            if (preset.metadata.mixRole == "dry" || preset.metadata.mixRole == "room")
                roleScale *= 0.55f;
            else
                roleScale *= 0.55f;
        }
        if (instrIndex == 8 && preset.metadata.mixRole != "dry" && preset.metadata.mixRole != "room")
            roleScale *= 1.25f;

        preset.settings.level = std::clamp(preset.settings.level * levelScale * roleScale, 0.0f, 1.0f);
        preset.fx = maskUnavailableFx(instrIndex, preset.fx);
        preset.metadata.nominalPeakDb = std::clamp(preset.metadata.nominalPeakDb, -24.0f, -1.0f);
        preset.outputBus = std::clamp(defaultOutputBusForRole(preset.metadata.mixRole), 0, 4);
        preset.metadata.familyLabel = familyLabel;
        preset.metadata.outputProfile = outputProfileFor(preset.metadata.mixRole, preset.outputBus);
        preset.metadata.description = descriptionFor(humanizeSlug(preset.metadata.tags.size() > 2 ? preset.metadata.tags[2] : preset.metadata.familyLabel),
                                                     preset.metadata.mixRole);
        appendTagIfMissing(preset.metadata.tags, familyLabel);
        appendTagIfMissing(preset.metadata.tags, preset.metadata.outputProfile);
    }

    return bank;
}

// =========================================================================
// FX-preset helpers — returns GlobalFxSettings for each aesthetic
// =========================================================================

static GlobalFxSettings makeDryFx()
{
    GlobalFxSettings fx;
    fx.satMix           = 0.0f;
    fx.transientMix     = 0.0f;
    fx.compMix          = 0.0f;
    fx.chorusOn         = false;
    fx.delayOn          = false;
    fx.reverbMix        = 0.0f;
    fx.reverbOn         = false;
    fx.limiterThreshold = -0.3f;
    fx.limiterRelease   = 50.0f;
    return fx;
}

static GlobalFxSettings makeConcertFx()
{
    GlobalFxSettings fx;
    fx.reverbSize     = 0.45f;
    fx.reverbDamping  = 0.55f;
    fx.reverbWidth    = 0.80f;
    fx.reverbMix      = 0.22f;
    fx.reverbPredelay = 12.0f;
    fx.limiterThreshold = -0.3f;
    fx.limiterRelease   = 50.0f;
    return fx;
}

static GlobalFxSettings makeCinematicFx()
{
    GlobalFxSettings fx;
    fx.compThreshold  = -22.0f;
    fx.compRatio      = 4.0f;
    fx.compAttack     = 8.0f;
    fx.compRelease    = 150.0f;
    fx.compMakeup     = 3.0f;
    fx.compMix        = 0.7f;
    // Reduced reverb: smaller size, more damping, narrower width, less mix — mix-friendly
    fx.reverbSize     = 0.50f;    // was 0.72
    fx.reverbDamping  = 0.55f;    // was 0.40 (more damping = drier)
    fx.reverbWidth    = 0.75f;    // was 0.90
    fx.reverbMix      = 0.22f;    // was 0.35
    fx.reverbPredelay = 30.0f;
    // Delay BPM-synced by default (values will be overridden by host BPM in processor)
    fx.delayTime      = 375.0f;   // ~1/8 at 120BPM
    fx.delayFeedback  = 0.25f;
    fx.delayMix       = 0.08f;    // reduced from 0.10
    fx.eqLowFreq     = 150.0f;
    fx.eqLowGain     = 2.0f;
    fx.eqHighFreq    = 6000.0f;
    fx.eqHighGain    = 1.5f;
    fx.limiterThreshold = -0.5f;
    fx.limiterRelease   = 60.0f;
    return fx;
}

static GlobalFxSettings makeAmbientFx()
{
    GlobalFxSettings fx;
    fx.reverbSize     = 0.90f;
    fx.reverbDamping  = 0.25f;
    fx.reverbWidth    = 1.0f;
    fx.reverbMix      = 0.55f;
    fx.reverbPredelay = 50.0f;
    fx.chorusRate     = 0.6f;
    fx.chorusDepth    = 0.45f;
    fx.chorusMix      = 0.25f;
    fx.delayTime      = 600.0f;
    fx.delayFeedback  = 0.45f;
    fx.delayMix       = 0.20f;
    fx.eqLowFreq     = 250.0f;
    fx.eqLowGain     = -2.0f;
    fx.eqHighFreq    = 4000.0f;
    fx.eqHighGain    = 3.0f;
    fx.limiterThreshold = -0.5f;
    fx.limiterRelease   = 80.0f;
    return fx;
}

static GlobalFxSettings makeStaccatoFx()
{
    GlobalFxSettings fx;
    fx.transientAttack  = 0.7f;
    fx.transientSustain = -0.3f;
    fx.transientMix     = 0.6f;
    fx.compThreshold    = -16.0f;
    fx.compRatio        = 5.0f;
    fx.compAttack       = 3.0f;
    fx.compRelease      = 80.0f;
    fx.compMakeup       = 2.0f;
    fx.compMix          = 0.8f;
    fx.reverbSize       = 0.20f;
    fx.reverbDamping    = 0.70f;
    fx.reverbWidth      = 0.50f;
    fx.reverbMix        = 0.10f;
    fx.reverbPredelay   = 5.0f;
    fx.eqMidFreq       = 2500.0f;
    fx.eqMidGain       = 2.5f;
    fx.eqMidQ          = 1.5f;
    fx.limiterThreshold = -0.3f;
    fx.limiterRelease   = 30.0f;
    return fx;
}

static GlobalFxSettings makeExperimentalFx()
{
    GlobalFxSettings fx;
    fx.satDrive       = 5.0f;
    fx.satMix         = 0.40f;
    fx.transientAttack = 0.4f;
    fx.transientSustain = 0.3f;
    fx.transientMix   = 0.35f;
    fx.compThreshold  = -25.0f;
    fx.compRatio      = 6.0f;
    fx.compAttack     = 5.0f;
    fx.compRelease    = 100.0f;
    fx.compMakeup     = 4.0f;
    fx.compMix        = 0.6f;
    fx.eqLowFreq     = 120.0f;
    fx.eqLowGain     = 4.0f;
    fx.eqMidFreq     = 3000.0f;
    fx.eqMidGain     = -3.0f;
    fx.eqMidQ        = 2.0f;
    fx.eqHighFreq    = 8000.0f;
    fx.eqHighGain    = 5.0f;
    fx.chorusRate    = 1.8f;
    fx.chorusDepth   = 0.65f;
    fx.chorusMix     = 0.35f;
    fx.delayTime     = 200.0f;
    fx.delayFeedback = 0.55f;
    fx.delayMix      = 0.25f;
    fx.reverbSize    = 0.60f;
    fx.reverbDamping = 0.35f;
    fx.reverbWidth   = 0.95f;
    fx.reverbMix     = 0.30f;
    fx.reverbPredelay = 20.0f;
    fx.limiterThreshold = -1.0f;
    fx.limiterRelease   = 40.0f;
    return fx;
}

// =========================================================================
// Preset builder helper — prefixes name with instrument for clarity
// =========================================================================
static InstrumentPreset makePreset(const char* instrName,
                                   const char* presetName,
                                   const InstrSettings& s,
                                   const GlobalFxSettings& fx,
                                   const PresetMetadata& metadata)
{
    std::string fullName = std::string(instrName) + " / " + presetName;
    return { fullName.c_str(), s, fx, 0, metadata };
}

// =========================================================================
// Per-instrument preset banks (6 presets each)
// =========================================================================

// --- Timbales (0) ---
static std::vector<InstrumentPreset> makeTimbalesBank()
{
    auto def = getDefaultSettings(0);
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Timbales", "Dry Close", def, makeDryFx(),
                              makeMetadata("timbales", "dry", -12.0f, { "close", "mix" })));
    bank.push_back(makePreset("Timbales", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("timbales", "room", -11.0f, { "hall", "wide" })));

    { auto s = def; s.decaySeconds = 3.0f; s.body = 0.65f; s.damping = 0.35f;
      bank.push_back(makePreset("Timbales", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("timbales", "cinematic", -10.0f, { "score", "tail" }))); }

    { auto s = def; s.decaySeconds = 4.0f; s.releaseSeconds = 1.5f; s.brightness = 0.30f;
      bank.push_back(makePreset("Timbales", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("timbales", "ambient", -13.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 100.0f; s.attackSeconds = 0.001f; s.damping = 0.80f;
      bank.push_back(makePreset("Timbales", "Sharp Accent", s, makeStaccatoFx(),
                                makeMetadata("timbales", "tight", -9.0f, { "accent", "one-shot" }))); }

    { auto s = def; s.brightness = 0.65f; s.noise = 0.50f; s.body = 0.70f;
      bank.push_back(makePreset("Timbales", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("timbales", "fx", -8.0f, { "design", "layer" }))); }

    return bank;
}

// --- Marimba (1) ---
static std::vector<InstrumentPreset> makeMarimbaBank()
{
    auto def = getDefaultSettings(1);
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Marimba", "Dry Close", def, makeDryFx(),
                              makeMetadata("marimba", "dry", -12.0f, { "close", "mix" })));
    bank.push_back(makePreset("Marimba", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("marimba", "room", -11.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 2.5f; s.body = 0.75f; s.brightness = 0.35f;
      bank.push_back(makePreset("Marimba", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("marimba", "cinematic", -10.0f, { "score", "tail" }))); }

    { auto s = def; s.decaySeconds = 3.5f; s.releaseSeconds = 1.0f; s.stereoWidth = 0.70f;
      bank.push_back(makePreset("Marimba", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("marimba", "ambient", -13.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 120.0f; s.attackSeconds = 0.001f; s.damping = 0.75f;
      bank.push_back(makePreset("Marimba", "Medium Accent", s, makeStaccatoFx(),
                                makeMetadata("marimba", "tight", -9.0f, { "accent", "one-shot" }))); }

    { auto s = def; s.brightness = 0.70f; s.color = 0.70f; s.noise = 0.30f;
      bank.push_back(makePreset("Marimba", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("marimba", "fx", -8.0f, { "design", "hybrid" }))); }

    return bank;
}

// --- Djembé (2) ---
static std::vector<InstrumentPreset> makeDjembeBank()
{
    auto def = getDefaultSettings(2);
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Djembé", "Dry Close", def, makeDryFx(),
                              makeMetadata("djembe", "dry", -12.0f, { "close", "mix" })));
    bank.push_back(makePreset("Djembé", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("djembe", "room", -11.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 1.0f; s.noise = 0.65f; s.body = 0.55f;
      bank.push_back(makePreset("Djembé", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("djembe", "cinematic", -10.0f, { "score", "body" }))); }

    { auto s = def; s.decaySeconds = 1.5f; s.releaseSeconds = 0.8f; s.stereoWidth = 0.60f;
      bank.push_back(makePreset("Djembé", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("djembe", "ambient", -13.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 80.0f; s.attackSeconds = 0.001f; s.brightness = 0.75f;
      bank.push_back(makePreset("Djembé", "Tight Accent", s, makeStaccatoFx(),
                                makeMetadata("djembe", "tight", -9.0f, { "accent", "one-shot" }))); }

    { auto s = def; s.noise = 0.75f; s.color = 0.75f; s.brightness = 0.65f;
      bank.push_back(makePreset("Djembé", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("djembe", "fx", -8.0f, { "design", "hybrid" }))); }

    return bank;
}

// --- B\xC3\xa2ton de Pluie (3) ---
static std::vector<InstrumentPreset> makeBatonDePluieBank()
{
    auto def = getDefaultSettings(3);
    def.attackSeconds = 0.006f;
    def.level = 1.0f;
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Baton de Pluie", "Dry Close", def, makeDryFx(),
                              makeMetadata("rainstick", "dry", -15.0f, { "close", "texture" })));
    bank.push_back(makePreset("Baton de Pluie", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("rainstick", "room", -14.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 7.0f; s.noise = 0.90f; s.damping = 0.55f;
      bank.push_back(makePreset("Baton de Pluie", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("rainstick", "cinematic", -13.0f, { "score", "tail" }))); }

    { auto s = def; s.decaySeconds = 10.0f; s.releaseSeconds = 3.0f; s.stereoWidth = 0.90f;
      bank.push_back(makePreset("Baton de Pluie", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("rainstick", "ambient", -14.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 150.0f; s.noise = 0.70f; s.brightness = 0.50f; s.damping = 0.60f;
      bank.push_back(makePreset("Baton de Pluie", "Infinite Accent", s, makeStaccatoFx(),
                                makeMetadata("rainstick", "tight", -12.0f, { "sustained", "one-shot" }))); }

    { auto s = def; s.noise = 0.95f; s.color = 0.65f; s.brightness = 0.55f;
      bank.push_back(makePreset("Baton de Pluie", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("rainstick", "fx", -11.0f, { "design", "motion" }))); }

    return bank;
}

// --- Bol Chantant (4) ---
static std::vector<InstrumentPreset> makeBolChantantBank()
{
    auto def = getDefaultSettings(4);
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Bol Chantant", "Dry Close", def, makeDryFx(),
                              makeMetadata("singing_bowl", "dry", -14.0f, { "close", "sustain" })));
    bank.push_back(makePreset("Bol Chantant", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("singing_bowl", "room", -13.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 14.0f; s.brightness = 0.55f; s.body = 0.10f;
      bank.push_back(makePreset("Bol Chantant", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("singing_bowl", "cinematic", -12.0f, { "score", "tail" }))); }

    { auto s = def; s.decaySeconds = 18.0f; s.releaseSeconds = 4.0f; s.stereoWidth = 0.80f;
      bank.push_back(makePreset("Bol Chantant", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("singing_bowl", "ambient", -14.0f, { "texture", "drone" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 200.0f; s.damping = 0.65f; s.attackSeconds = 0.005f;
      bank.push_back(makePreset("Bol Chantant", "Infinite Accent", s, makeStaccatoFx(),
                                makeMetadata("singing_bowl", "tight", -11.0f, { "sustained", "one-shot" }))); }

    { auto s = def; s.brightness = 0.70f; s.color = 0.70f; s.decaySeconds = 12.0f;
      bank.push_back(makePreset("Bol Chantant", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("singing_bowl", "fx", -10.0f, { "design", "layer" }))); }

    return bank;
}

// --- Carillon Éolien (5) ---
static std::vector<InstrumentPreset> makeCarillonEolienBank()
{
    auto def = getDefaultSettings(5);
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Carillon Éolien", "Dry Close", def, makeDryFx(),
                              makeMetadata("wind_chimes", "dry", -14.0f, { "close", "detail" })));
    bank.push_back(makePreset("Carillon Éolien", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("wind_chimes", "room", -13.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 4.0f; s.stereoWidth = 0.90f; s.brightness = 0.55f;
      bank.push_back(makePreset("Carillon Éolien", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("wind_chimes", "cinematic", -12.0f, { "score", "motion" }))); }

    { auto s = def; s.decaySeconds = 6.0f; s.releaseSeconds = 2.0f; s.stereoWidth = 1.0f;
      bank.push_back(makePreset("Carillon Éolien", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("wind_chimes", "ambient", -14.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 120.0f; s.damping = 0.65f; s.brightness = 0.75f;
      bank.push_back(makePreset("Carillon Éolien", "Infinite Accent", s, makeStaccatoFx(),
                                makeMetadata("wind_chimes", "tight", -11.0f, { "sustained", "one-shot" }))); }

    { auto s = def; s.brightness = 0.80f; s.color = 0.70f; s.noise = 0.25f;
      bank.push_back(makePreset("Carillon Éolien", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("wind_chimes", "fx", -10.0f, { "design", "motion" }))); }

    return bank;
}

// --- Cloche Tubulaire (6) ---
static std::vector<InstrumentPreset> makeClocheTubulaireBank()
{
    auto def = getDefaultSettings(6);
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Cloche Tubulaire", "Dry Close", def, makeDryFx(),
                              makeMetadata("tubular_bell", "dry", -13.0f, { "close", "mix" })));
    bank.push_back(makePreset("Cloche Tubulaire", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("tubular_bell", "room", -12.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 5.0f; s.brightness = 0.60f; s.body = 0.20f;
      bank.push_back(makePreset("Cloche Tubulaire", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("tubular_bell", "cinematic", -11.0f, { "score", "tail" }))); }

    { auto s = def; s.decaySeconds = 6.0f; s.releaseSeconds = 1.5f; s.stereoWidth = 0.70f;
      bank.push_back(makePreset("Cloche Tubulaire", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("tubular_bell", "ambient", -13.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 150.0f; s.damping = 0.70f; s.brightness = 0.70f;
      bank.push_back(makePreset("Cloche Tubulaire", "Infinite Accent", s, makeStaccatoFx(),
                                makeMetadata("tubular_bell", "tight", -10.0f, { "sustained", "one-shot" }))); }

    { auto s = def; s.brightness = 0.80f; s.color = 0.75f; s.decaySeconds = 4.0f;
      bank.push_back(makePreset("Cloche Tubulaire", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("tubular_bell", "fx", -9.0f, { "design", "layer" }))); }

    return bank;
}

// --- Triangle (7) ---
static std::vector<InstrumentPreset> makeTriangleBank()
{
    auto def = getDefaultSettings(7);
    std::vector<InstrumentPreset> bank;

    bank.push_back(makePreset("Triangle", "Dry Close", def, makeDryFx(),
                              makeMetadata("triangle", "dry", -13.0f, { "close", "mix" })));
    bank.push_back(makePreset("Triangle", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("triangle", "room", -12.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 5.0f; s.brightness = 0.60f; s.stereoWidth = 0.60f;
      bank.push_back(makePreset("Triangle", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("triangle", "cinematic", -11.0f, { "score", "tail" }))); }

    { auto s = def; s.decaySeconds = 7.0f; s.releaseSeconds = 2.0f; s.stereoWidth = 0.80f;
      bank.push_back(makePreset("Triangle", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("triangle", "ambient", -13.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 80.0f; s.damping = 0.55f; s.attackSeconds = 0.0005f;
      bank.push_back(makePreset("Triangle", "Infinite Accent", s, makeStaccatoFx(),
                                makeMetadata("triangle", "tight", -10.0f, { "sustained", "one-shot" }))); }

    { auto s = def; s.brightness = 0.80f; s.color = 0.75f; s.noise = 0.35f;
      bank.push_back(makePreset("Triangle", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("triangle", "fx", -9.0f, { "design", "layer" }))); }

    return bank;
}

// --- Glockenspiel (8) ---
static std::vector<InstrumentPreset> makeGlockenspielBank()
{
    auto def = getDefaultSettings(8);
    std::vector<InstrumentPreset> bank;

    { auto s = def; s.level *= 1.12f;
      bank.push_back(makePreset("Glockenspiel", "Dry Close", s, makeDryFx(),
                                makeMetadata("glockenspiel", "dry", -12.5f, { "close", "mix" }))); }
    bank.push_back(makePreset("Glockenspiel", "Hall Bloom", def, makeConcertFx(),
                              makeMetadata("glockenspiel", "room", -12.0f, { "hall", "natural" })));

    { auto s = def; s.decaySeconds = 6.0f; s.brightness = 0.65f; s.body = 0.15f;
      bank.push_back(makePreset("Glockenspiel", "Film Tail", s, makeCinematicFx(),
                                makeMetadata("glockenspiel", "cinematic", -11.0f, { "score", "tail" }))); }

    { auto s = def; s.decaySeconds = 8.0f; s.releaseSeconds = 2.0f; s.stereoWidth = 0.70f;
      bank.push_back(makePreset("Glockenspiel", "Air Bloom", s, makeAmbientFx(),
                                makeMetadata("glockenspiel", "ambient", -13.0f, { "texture", "wide" }))); }

    { auto s = def; s.oneShot = true; s.oneShotDecayMs = 100.0f; s.damping = 0.55f; s.attackSeconds = 0.0005f;
      bank.push_back(makePreset("Glockenspiel", "Infinite Accent", s, makeStaccatoFx(),
                                makeMetadata("glockenspiel", "tight", -10.0f, { "sustained", "one-shot" }))); }

    { auto s = def; s.brightness = 0.85f; s.color = 0.70f; s.decaySeconds = 5.0f;
      bank.push_back(makePreset("Glockenspiel", "Design Layer", s, makeExperimentalFx(),
                                makeMetadata("glockenspiel", "fx", -9.0f, { "design", "layer" }))); }

    return bank;
}

} // namespace

// =========================================================================
// Public accessor
// =========================================================================
const std::array<std::vector<InstrumentPreset>, kNumInstruments>& getFactoryPresetBanks()
{
    static const std::array<std::vector<InstrumentPreset>, kNumInstruments> banks = {{
        finalizeBank(0, makeTimbalesBank()),
        finalizeBank(1, makeMarimbaBank()),
        finalizeBank(2, makeDjembeBank()),
        finalizeBank(3, makeBatonDePluieBank()),
        finalizeBank(4, makeBolChantantBank()),
        finalizeBank(5, makeCarillonEolienBank()),
        finalizeBank(6, makeClocheTubulaireBank()),
        finalizeBank(7, makeTriangleBank()),
        finalizeBank(8, makeGlockenspielBank())
    }};
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

} // namespace mpc
