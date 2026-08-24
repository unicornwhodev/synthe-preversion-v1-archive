#include "PercDefs.h"
#include <algorithm>

namespace mpc
{
namespace
{

// -------------------------------------------------------------------------
// Names
// -------------------------------------------------------------------------
constexpr const char* kNames[kNumInstruments] = {
    "Timbales",           // 0  PERCUSSIONS
    "Marimba",            // 1
    "Djemb\xC3\xa9",     // 2
    "B\xC3\xa2ton de Pluie",  // 3  AMBIANCE
    "Bol Chantant",       // 4
    "Carillon \xC3\x89olien", // 5
    "Cloche Tubulaire",   // 6  M\xC3\x89TALLIQUES
    "Triangle",           // 7
    "Glockenspiel"        // 8
};

constexpr const char* kShortNames[kNumInstruments] = {
    "TIMB", "MRMB", "DJMB",
    "RAIN", "BOWL", "WIND",
    "CLCH", "TRNL", "GLCK"
};

constexpr const char* kFamilyNames[kNumFamilies] = {
    "PERCUSSIONS",
    "AMBIANCE",
    "M\xC3\x89TALLIQUES"
};

constexpr FxAvailability FX(bool saturator,
                            bool transient,
                            bool eq,
                            bool compressor,
                            bool chorus,
                            bool delay,
                            bool reverb,
                            bool limiter)
{
    return FxAvailability{ saturator, transient, eq, compressor, chorus, delay, reverb, limiter };
}

constexpr std::array<FxAvailability, kNumInstruments> kFxAvailability = {{
    FX(true,  true,  true,  true,  false, false, true,  true),
    FX(true,  true,  true,  true,  false, false, true,  true),
    FX(true,  true,  true,  true,  false, false, true,  true),
    FX(false, false, true,  false, false, true,  true,  true),
    FX(false, false, true,  false, true,  true,  true,  true),
    FX(false, false, true,  false, true,  true,  true,  true),
    FX(false, true,  true,  true,  false, false, true,  true),
    FX(false, false, true,  false, true,  true,  true,  true),
    FX(false, true,  true,  false, false, false, true,  true)
}};

// -------------------------------------------------------------------------
// Characteristics
// -------------------------------------------------------------------------
constexpr InstrCharacteristics kChars[kNumInstruments] = {
    // Timbales (timpani) — membrane drum, Bessel function zeros (Rossing 1982)
    {
        SynthMode::Modal,
        /*numModes*/ 6, /*modeSpread*/ 2.6f, /*modeDecayBase*/ 2.5f,
        /*modeDecaySpread*/ 1.8f,
        /*noiseAmount*/ 0.42f, /*noiseDecay*/ 0.025f, /*noiseBrightness*/ 0.48f,
        /*bodyResonance*/ 0.48f, /*bodyDamping*/ 0.52f, /*bodyDelay*/ 1.0f, /*hasBodyResonator*/ true,
        /*metallic*/ 0.30f, /*ringTime*/ 0.82f, /*pitchFollowing*/ 1.0f,
        /*randomization*/ 0.0f, /*builtInBrightness*/ 0.52f,
        /*clickAmount*/ 0.48f, /*clickDecayMs*/ 2.0f, /*brightBaseMul*/ 6.6f,
        /*useFixedRatios*/ true,
        /*fixedRatios*/ { 1.000f, 1.593f, 2.136f, 2.295f, 2.653f, 2.917f,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 5.0f   // 2.5 * 2.0
    },
    // Marimba — wooden bars, undercut to tune (Rossing & Braasch 1999)
    {
        SynthMode::Modal,
        /*numModes*/ 6, /*modeSpread*/ 4.0f, /*modeDecayBase*/ 1.8f,
        /*modeDecaySpread*/ 2.5f,
        /*noiseAmount*/ 0.18f, /*noiseDecay*/ 0.009f, /*noiseBrightness*/ 0.36f,
        /*bodyResonance*/ 0.58f, /*bodyDamping*/ 0.50f, /*bodyDelay*/ 0.7f, /*hasBodyResonator*/ true,
        /*metallic*/ 0.08f, /*ringTime*/ 0.72f, /*pitchFollowing*/ 1.0f,
        /*randomization*/ 0.0f, /*builtInBrightness*/ 0.50f,
        /*clickAmount*/ 0.34f, /*clickDecayMs*/ 4.5f, /*brightBaseMul*/ 3.9f,
        /*useFixedRatios*/ true,
        /*fixedRatios*/ { 1.000f, 3.930f, 9.540f, 17.72f, 25.70f, 37.20f,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 2.7f   // 1.8 * 1.5
    },
    // Djemb\xC3\xa9 — hand drum, inharmonic membrane + noise
    {
        SynthMode::Hybrid,
        /*numModes*/ 4, /*modeSpread*/ 2.4f, /*modeDecayBase*/ 0.8f,
        /*modeDecaySpread*/ 2.0f,
        /*noiseAmount*/ 0.65f, /*noiseDecay*/ 0.04f, /*noiseBrightness*/ 0.5f,
        /*bodyResonance*/ 0.45f, /*bodyDamping*/ 0.7f, /*bodyDelay*/ 0.6f, /*hasBodyResonator*/ true,
        /*metallic*/ 0.05f, /*ringTime*/ 0.5f, /*pitchFollowing*/ 0.5f,
        /*randomization*/ 0.0f, /*builtInBrightness*/ 0.50f,
        /*clickAmount*/ 0.50f, /*clickDecayMs*/ 5.0f, /*brightBaseMul*/ 4.0f,
        /*useFixedRatios*/ false,
        /*fixedRatios*/ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 0.48f  // 0.8 * 0.6
    },
    // B\xC3\xa2ton de Pluie — continuous noise, filtered
    {
        SynthMode::Noise,
        /*numModes*/ 2, /*modeSpread*/ 1.0f, /*modeDecayBase*/ 6.0f,
        /*modeDecaySpread*/ 1.0f,
        /*noiseAmount*/ 0.85f, /*noiseDecay*/ 4.0f, /*noiseBrightness*/ 0.35f,
        /*bodyResonance*/ 0.30f, /*bodyDamping*/ 0.8f, /*bodyDelay*/ 0.3f, /*hasBodyResonator*/ true,
        /*metallic*/ 0.0f, /*ringTime*/ 3.0f, /*pitchFollowing*/ 0.1f,
        /*randomization*/ 0.7f, /*builtInBrightness*/ 0.30f,
        /*clickAmount*/ 0.05f, /*clickDecayMs*/ 8.0f, /*brightBaseMul*/ 4.0f,
        /*useFixedRatios*/ false,
        /*fixedRatios*/ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 30.0f  // 6.0 * 5.0
    },
    // Bol Chantant — Tibetan singing bowl (In\xC3\xa1cio et al. 2006)
    {
        SynthMode::Modal,
        /*numModes*/ 8, /*modeSpread*/ 2.8f, /*modeDecayBase*/ 12.0f,
        /*modeDecaySpread*/ 1.2f,
        /*noiseAmount*/ 0.05f, /*noiseDecay*/ 0.005f, /*noiseBrightness*/ 0.4f,
        /*bodyResonance*/ 0.0f, /*bodyDamping*/ 0.2f, /*bodyDelay*/ 0.0f, /*hasBodyResonator*/ false,
        /*metallic*/ 0.85f, /*ringTime*/ 5.0f, /*pitchFollowing*/ 1.0f,
        /*randomization*/ 0.0f, /*builtInBrightness*/ 0.55f,
        /*clickAmount*/ 0.10f, /*clickDecayMs*/ 2.0f, /*brightBaseMul*/ 8.0f,
        /*useFixedRatios*/ true,
        /*fixedRatios*/ { 1.000f, 2.730f, 5.000f, 8.200f, 12.24f, 17.22f, 23.2f, 30.4f,
                          0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 120.0f // 12.0 * 10.0
    },
    // Carillon \xC3\x89olien — wind chimes, random tinkling
    {
        SynthMode::Modal,
        /*numModes*/ 6, /*modeSpread*/ 3.2f, /*modeDecayBase*/ 3.0f,
        /*modeDecaySpread*/ 1.5f,
        /*noiseAmount*/ 0.10f, /*noiseDecay*/ 0.01f, /*noiseBrightness*/ 0.7f,
        /*bodyResonance*/ 0.0f, /*bodyDamping*/ 0.3f, /*bodyDelay*/ 0.0f, /*hasBodyResonator*/ false,
        /*metallic*/ 0.90f, /*ringTime*/ 2.5f, /*pitchFollowing*/ 0.7f,
        /*randomization*/ 0.8f, /*builtInBrightness*/ 0.65f,
        /*clickAmount*/ 0.15f, /*clickDecayMs*/ 2.0f, /*brightBaseMul*/ 7.0f,
        /*useFixedRatios*/ false,
        /*fixedRatios*/ { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                          0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 7.5f   // 3.0 * 2.5
    },
    // Cloche Tubulaire — tubular bell (Rossing & Perrin 1987 — hum at 0.5x)
    {
        SynthMode::Modal,
        /*numModes*/ 10, /*modeSpread*/ 2.4f, /*modeDecayBase*/ 8.0f,
        /*modeDecaySpread*/ 1.4f,
        /*noiseAmount*/ 0.12f, /*noiseDecay*/ 0.008f, /*noiseBrightness*/ 0.5f,
        /*bodyResonance*/ 0.0f, /*bodyDamping*/ 0.25f, /*bodyDelay*/ 0.0f, /*hasBodyResonator*/ false,
        /*metallic*/ 1.0f, /*ringTime*/ 4.0f, /*pitchFollowing*/ 1.0f,
        /*randomization*/ 0.0f, /*builtInBrightness*/ 0.50f,
        /*clickAmount*/ 0.35f, /*clickDecayMs*/ 2.5f, /*brightBaseMul*/ 8.0f,
        /*useFixedRatios*/ true,
        /*fixedRatios*/ { 0.5f, 1.000f, 1.500f, 2.000f, 2.730f,
                          4.000f, 6.000f, 8.000f, 10.0f, 12.0f, 0.0f, 0.0f },
        /*decayNorm*/ 48.0f  // 8.0 * 6.0
    },
    // Triangle — prismatic steel bar (Rossing 1992)
    {
        SynthMode::Modal,
        /*numModes*/ 8, /*modeSpread*/ 3.5f, /*modeDecayBase*/ 4.5f,
        /*modeDecaySpread*/ 1.3f,
        /*noiseAmount*/ 0.20f, /*noiseDecay*/ 0.005f, /*noiseBrightness*/ 0.8f,
        /*bodyResonance*/ 0.0f, /*bodyDamping*/ 0.15f, /*bodyDelay*/ 0.0f, /*hasBodyResonator*/ false,
        /*metallic*/ 1.0f, /*ringTime*/ 3.5f, /*pitchFollowing*/ 0.8f,
        /*randomization*/ 0.0f, /*builtInBrightness*/ 0.70f,
        /*clickAmount*/ 0.40f, /*clickDecayMs*/ 2.0f, /*brightBaseMul*/ 8.0f,
        /*useFixedRatios*/ true,
        /*fixedRatios*/ { 1.000f, 2.756f, 5.404f, 8.933f, 13.35f, 18.70f, 25.1f, 32.4f,
                          0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 15.75f // 4.5 * 3.5
    },
    // Glockenspiel — bright metallic pitched steel bars (fixed ratios, Rossing 1992)
    {
        SynthMode::Modal,
        /*numModes*/ 8, /*modeSpread*/ 2.6f, /*modeDecayBase*/ 5.0f,
        /*modeDecaySpread*/ 1.6f,
        /*noiseAmount*/ 0.08f, /*noiseDecay*/ 0.003f, /*noiseBrightness*/ 0.6f,
        /*bodyResonance*/ 0.0f, /*bodyDamping*/ 0.2f, /*bodyDelay*/ 0.0f, /*hasBodyResonator*/ false,
        /*metallic*/ 0.95f, /*ringTime*/ 3.0f, /*pitchFollowing*/ 1.0f,
        /*randomization*/ 0.0f, /*builtInBrightness*/ 0.75f,
        /*clickAmount*/ 0.30f, /*clickDecayMs*/ 2.0f, /*brightBaseMul*/ 8.0f,
        /*useFixedRatios*/ true,
        /*fixedRatios*/ { 1.000f, 2.756f, 5.404f, 8.933f, 13.35f, 18.70f, 25.1f, 32.4f,
                          0.0f, 0.0f, 0.0f, 0.0f },
        /*decayNorm*/ 15.0f  // 5.0 * 3.0
    }
};

// -------------------------------------------------------------------------
// Default settings
// -------------------------------------------------------------------------
constexpr InstrSettings kDefaults[kNumInstruments] = {
    // Timbales
    { 0.88f, 0.0f, 0.58f, 0.0015f, 1.35f, 0.0f, 0.20f,
      0.42f, 0.48f, 0.42f, 0.42f, 0.56f, 9200.0f, 0.0f },
    // Marimba
    { 0.84f, 0.0f, 0.56f, 0.0015f, 1.35f, 0.0f, 0.22f,
      0.38f, 0.58f, 0.18f, 0.38f, 0.52f, 9200.0f, 0.0f },
    // Djemb\xC3\xa9
    { 0.82f, 0.0f, 0.55f, 0.002f, 0.6f, 0.0f, 0.15f,
      0.60f, 0.45f, 0.55f, 0.30f, 0.55f, 8000.0f, 0.0f },
    // B\xC3\xa2ton de Pluie
    { 0.70f, 0.0f, 0.30f, 0.50f, 5.0f, 0.30f, 1.50f,
      0.70f, 0.30f, 0.85f, 0.70f, 0.40f, 4000.0f, 0.0f },
    // Bol Chantant
    { 0.75f, 0.0f, 0.50f, 0.01f, 10.0f, 0.10f, 2.00f,
      0.30f, 0.0f, 0.05f, 0.50f, 0.50f, 10000.0f, 0.0f },
    // Carillon \xC3\x89olien
    { 0.70f, 0.0f, 0.60f, 0.005f, 2.5f, 0.0f, 0.50f,
      0.35f, 0.0f, 0.10f, 0.80f, 0.55f, 9000.0f, 0.0f },
    // Cloche Tubulaire
    { 0.78f, 0.0f, 0.50f, 0.003f, 6.0f, 0.0f, 1.00f,
      0.30f, 0.0f, 0.12f, 0.55f, 0.50f, 10000.0f, 0.0f },
    // Triangle
    { 0.75f, 0.0f, 0.65f, 0.001f, 3.5f, 0.0f, 0.60f,
      0.25f, 0.0f, 0.20f, 0.45f, 0.60f, 12000.0f, 0.0f },
    // Glockenspiel
    { 0.78f, 0.0f, 0.70f, 0.001f, 4.0f, 0.0f, 0.40f,
      0.30f, 0.0f, 0.08f, 0.40f, 0.55f, 11000.0f, 0.0f }
};

// -------------------------------------------------------------------------
// Descriptions (French, UTF-8)
// -------------------------------------------------------------------------
constexpr const char* kDescriptions[kNumInstruments] = {
    // Timbales
    "Les timbales, ou timpani, sont les percussions les plus nobles de "
    "l'orchestre symphonique. Ces grands f\xC3\xBBts de cuivre recouverts de peau "
    "produisent des sons graves et r\xC3\xa9sonnants dont la hauteur est ajustable "
    "\xC3\xa0 l'aide de p\xC3\xa9""dales. De Beethoven \xC3\xa0 Mahler, les timbales marquent "
    "les moments dramatiques et ponctuent les climax orchestraux.",

    // Marimba
    "Le marimba est un instrument \xC3\xa0 lames de bois, h\xC3\xa9ritier des xylophones "
    "africains et centr\xC3\xa9ricains. Ses tubes r\xC3\xa9sonateurs en m\xC3\xa9tal amplifient "
    "un son chaud, rond et profond. Jou\xC3\xa9 avec des mailloches douces, "
    "il offre une gamme allant du grave velout\xC3\xa9 \xC3\xa0 l'aigu cristallin. "
    "Incontournable en musique contemporaine et en percussion solo.",

    // Djemb\xC3\xa9
    "Le djemb\xC3\xa9 est un tambour calice originaire d'Afrique de l'Ouest. "
    "Taill\xC3\xa9 dans un tronc d'arbre et couvert de peau de ch\xC3\xa8vre, il produit "
    "trois sons fondamentaux : la basse profonde au centre, le ton\xC3\xa9 "
    "m\xC3\xa9""dium sur le bord, et la claque aigu\xC3\xAB perçante. Son expressivit\xC3\xa9 "
    "en fait l'un des tambours \xC3\xa0 main les plus populaires au monde.",

    // B\xC3\xa2ton de Pluie
    "Le b\xC3\xa2ton de pluie est un instrument de la famille des idiophones, "
    "originaire d'Am\xC3\xa9rique du Sud. Ce tube creux rempli de graines ou de "
    "petits cailloux produit un son continu de pluie lorsqu'on le retourne "
    "lentement. Son timbre doux et hypnotique en fait un outil id\xC3\xa9""al "
    "pour la relaxation, la m\xC3\xa9""ditation et les ambiances sonores.",

    // Bol Chantant
    "Le bol chantant tib\xC3\xa9tain est forg\xC3\xa9 \xC3\xa0 partir d'un alliage de sept "
    "m\xC3\xa9taux. Frapp\xC3\xa9 ou frott\xC3\xa9 avec un maillet, il produit un son riche "
    "en harmoniques qui r\xC3\xa9sonne longuement avec un battement lent "
    "entre ses modes. Utilis\xC3\xa9 dans la m\xC3\xa9""ditation, le yoga et la "
    "th\xC3\xa9rapie sonore, il cr\xC3\xa9""e des textures envo\xC3\xBBtantes et apaisantes.",

    // Carillon \xC3\x89olien
    "Le carillon \xC3\xa9olien, ou wind chimes, est activé par le vent. "
    "Ses tubes m\xC3\xa9talliques suspendus s'entrechoquent pour cr\xC3\xa9""er "
    "des cascades de tintements al\xC3\xa9""atoires et \xC3\xa9th\xC3\xa9r\xC3\xa9""es. "
    "Symbole du hasard et de la nature, il ajoute une dimension "
    "magique aux ambiances et aux bandes originales de films.",

    // Cloche Tubulaire
    "Les cloches tubulaires, ou chimes, sont des tubes m\xC3\xa9talliques "
    "suspendus verticalement. Frapp\xC3\xa9""es avec un maillet de cuir, elles "
    "produisent un son riche et majesteux qui \xC3\xa9voque les cloches "
    "d'\xC3\xa9glise. De Tchai\xC3\xAFkovski \xC3\xa0 Pink Floyd, les cloches tubulaires "
    "ajoutent une dimension c\xC3\xa9r\xC3\xa9monielle et \xC3\xa9pique \xC3\xa0 toute \xC5\x93uvre.",

    // Triangle
    "Le triangle est un petit instrument de percussion en acier "
    "pli\xC3\xa9 en forme de triangle ouvert. Malgr\xC3\xa9 sa simplicit\xC3\xa9, il produit "
    "un son brillant, pur et p\xC3\xa9n\xC3\xa9trant qui traverse tout l'orchestre. "
    "Utilis\xC3\xa9 depuis le XVIIIe si\xC3\xa8""cle, il ajoute \xC3\xa9""clat et "
    "l\xC3\xa9g\xC3\xa8ret\xC3\xa9 aux passages musicaux les plus d\xC3\xa9licats.",

    // Glockenspiel
    "Le glockenspiel, ou jeu de timbres, est compos\xC3\xa9 de lames "
    "m\xC3\xa9talliques chromatiques jou\xC3\xa9""es avec des mailloches dures. "
    "Son son cristallin et brillant, deux octaves au-dessus du piano, "
    "ajoute une touche f\xC3\xa9""erique aux orchestrations. De la Fl\xC3\xBBte "
    "enchant\xC3\xa9""e de Mozart aux musiques de films modernes, il illumine "
    "chaque partition de sa clart\xC3\xa9 c\xC3\xa9leste."
};

} // anonymous namespace

// =========================================================================
// Accessor implementations
// =========================================================================
Family getFamily(int instrIndex)
{
    int idx = std::clamp(instrIndex, 0, kNumInstruments - 1);
    if (idx < kFamilyStart[1]) return Family::Percussions;
    if (idx < kFamilyStart[2]) return Family::Ambiance;
    return Family::Metalliques;
}

int getFamilyStartIndex(Family family)
{
    return kFamilyStart[static_cast<int>(family)];
}

const char* getFamilyName(int familyIndex)
{
    return kFamilyNames[std::clamp(familyIndex, 0, kNumFamilies - 1)];
}

const char* getInstrName(int instrIndex)
{
    return kNames[std::clamp(instrIndex, 0, kNumInstruments - 1)];
}

const char* getInstrShortName(int instrIndex)
{
    return kShortNames[std::clamp(instrIndex, 0, kNumInstruments - 1)];
}

const InstrCharacteristics& getCharacteristics(int instrIndex)
{
    return kChars[std::clamp(instrIndex, 0, kNumInstruments - 1)];
}

bool hasBodyResonator(int instrIndex)
{
    return kChars[std::clamp(instrIndex, 0, kNumInstruments - 1)].hasBodyResonator;
}

InstrSettings getDefaultSettings(int instrIndex)
{
    return kDefaults[std::clamp(instrIndex, 0, kNumInstruments - 1)];
}

const char* getInstrDescription(int instrIndex)
{
    return kDescriptions[std::clamp(instrIndex, 0, kNumInstruments - 1)];
}

const FxAvailability& getFxAvailability(int instrIndex)
{
    return kFxAvailability[std::clamp(instrIndex, 0, kNumInstruments - 1)];
}

bool isFxAvailable(int instrIndex, GlobalFxSlot slot)
{
    const auto& fx = getFxAvailability(instrIndex);
    switch (slot)
    {
        case GlobalFxSlot::Saturator:  return fx.saturator;
        case GlobalFxSlot::Transient:  return fx.transient;
        case GlobalFxSlot::Eq:         return fx.eq;
        case GlobalFxSlot::Compressor: return fx.compressor;
        case GlobalFxSlot::Chorus:     return fx.chorus;
        case GlobalFxSlot::Delay:      return fx.delay;
        case GlobalFxSlot::Reverb:     return fx.reverb;
        case GlobalFxSlot::Limiter:    return fx.limiter;
        default:                       return true;
    }
}

GlobalFxSettings maskUnavailableFx(int instrIndex, const GlobalFxSettings& fx)
{
    auto masked = fx;
    const auto& availability = getFxAvailability(instrIndex);
    masked.saturatorOn  = availability.saturator  && masked.saturatorOn;
    masked.transientOn  = availability.transient  && masked.transientOn;
    masked.eqOn         = availability.eq         && masked.eqOn;
    masked.compressorOn = availability.compressor && masked.compressorOn;
    masked.chorusOn     = availability.chorus     && masked.chorusOn;
    masked.delayOn      = availability.delay      && masked.delayOn;
    masked.reverbOn     = availability.reverb     && masked.reverbOn;
    masked.limiterOn    = availability.limiter    && masked.limiterOn;
    return masked;
}

} // namespace mpc
