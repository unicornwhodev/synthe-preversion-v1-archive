#pragma once

#include <array>
#include <string>

namespace mis
{
constexpr int kNumInstruments         = 21;
constexpr int kNumFamilies            = 4;
constexpr int kMaxInstrumentsPerFamily = 6;

constexpr int kFamilySize[]  = { 5, 5, 6, 5 };
constexpr int kFamilyStart[] = { 0, 5, 10, 16 };

// =========================================================================
// Instrument families
// =========================================================================
enum class Family { Strings = 0, Winds, Percussion, Conceptual };

// =========================================================================
// Synthesis mode — determines which intermediate voice base is used
// =========================================================================
enum class SynthesisMode
{
    Bowed,       // continuous friction excitation  → BowedStringVoiceBase
    Plucked,     // Karplus-Strong impulse          → PluckedStringVoice
    Blown,       // breath / reed / embouchure      → WindVoiceBase
    Struck,      // percussive impulse + modal body → StruckResonatorVoiceBase
    Electronic   // oscillator + FM / waveshaping   → ElectronicVoiceBase
};

enum class GlobalFxSlot
{
    Saturator = 0,
    Transient,
    Eq,
    Compressor,
    Chorus,
    Delay,
    Reverb,
    Limiter
};

// =========================================================================
// Per-instrument synthesis character (not user-editable)
// =========================================================================
constexpr int kMaxPartials   = 16;
constexpr int kMaxBodyModes  = 8;
constexpr int kMaxSympathetic = 4;

struct InstrumentCharacteristics
{
    // --- original fields (kept for compatibility) ---
    float waveformMorph;      // 0 = sine, 0.5 = triangle, 1.0 = sawtooth
    float bodyDelayRatio;     // body resonator delay relative to note period
    float bodyDamping;        // 0-1 HF damping inside the body feedback loop
    float sympatheticSemis;   // semitone offset for sympathetic comb filter (legacy single)
    float partialRatios[8];   // frequency ratios partial/fundamental (0.0f = unused slot)
    float partialAmps[8];     // relative amplitude of each partial
    bool  sustained;          // true = ADSR;  false = AD (percussive one-shot)

    // --- Phase 1 extensions ---
    SynthesisMode synthesisMode = SynthesisMode::Bowed;
    int   partialCount       = 4;       // actual number of partials used (up to kMaxPartials)
    float inharmonicityB     = 0.0f;    // piano-style inharmonicity coefficient B
    float exciterBrightness  = 0.5f;    // spectral brightness of exciter noise / friction
    float exciterDecayMs     = 5.0f;    // exciter impulse decay (short = struck, long = bowed)
    float bodyResonanceQ     = 1.0f;    // body resonator Q factor (modal synthesis)
    int   numBodyModes       = 1;       // number of biquad body modes (up to kMaxBodyModes)
    float sympatheticMatrix[kMaxSympathetic] = { 0.0f, 0.0f, 0.0f, 0.0f }; // semitone offsets
    bool  hasContinuousExcitation = false; // true for bowed / blown instruments
    bool  oddHarmonicsOnly = false;          // true for cylindrical bore (Aulos) — filters even-indexed partials

    // Internal engine profile. These are not APVTS parameters and are not serialized
    // in presets; they only steer per-instrument behavior inside the five engines.
    float engineAttackAccent = 1.0f;   // transient/exciter emphasis
    float engineTailDamping  = 1.0f;   // >1 shortens resonator feedback tails
    float engineSpectralMotion = 0.0f; // breath/buzz/turbulence/spectral movement
    float enginePitchFocus   = 1.0f;   // >1 restrains vibrato/pitch wander
    float engineDensityLimit = 1.0f;   // <1 restrains sympathetic/modal buildup
};

// =========================================================================
// Per-instrument user parameters
// =========================================================================
struct InstrumentSettings
{
    float level           = 0.8f;
    float tuneSemitones   = 0.0f;
    float attackSeconds   = 0.01f;
    float decaySeconds    = 0.5f;
    float sustainLevel    = 0.7f;
    float releaseSeconds  = 0.3f;
    float exciter         = 0.3f;    // exciter energy (noise / friction blend)
    float body            = 0.5f;    // body resonance amount
    float sympathetic     = 0.2f;    // sympathetic resonance amount
    float noiseAmount     = 0.1f;    // additional noise layer
    float drive           = 1.5f;    // waveshaping / saturation
    float cutoffHz        = 8000.0f; // low-pass cutoff
    float filterQ         = 0.7f;    // filter resonance
    float pan             = 0.0f;    // -1 .. +1

    // --- Phase 1 extensions ---
    float breathPressure  = 0.5f;    // wind instrument blow pressure (0..1)
    float bowSpeed        = 0.5f;    // bowed string bow speed (0..1)
    float bowPressure     = 0.5f;    // bowed string bow pressure (0..1)
    float strikePosition  = 0.5f;    // percussion strike position (0=edge, 1=center)
    float brightness      = 0.5f;    // unified spectral brightness control (0..1)
};

// =========================================================================
// Global FX snapshot stored per preset
// =========================================================================
struct GlobalFxSettings
{
    // Saturator
    float satDrive        = 1.6f;
    float satMix          = 0.15f;

    // Transient
    float transientAttack  = 0.15f;
    float transientSustain = 0.0f;
    float transientMix     = 0.5f;

    // EQ
    float eqLowFreq   = 200.0f;
    float eqLowGain   = 0.0f;
    float eqMidFreq   = 1000.0f;
    float eqMidGain   = 0.0f;
    float eqMidQ      = 1.0f;
    float eqHighFreq  = 5000.0f;
    float eqHighGain  = 0.0f;

    // Compressor
    float compThreshold = -24.0f;
    float compRatio     = 2.5f;
    float compAttack    = 8.0f;
    float compRelease   = 140.0f;
    float compMakeup    = 3.0f;
    float compMix       = 0.8f;

    // Chorus
    float chorusRate    = 1.0f;
    float chorusDepth   = 0.5f;
    float chorusMix     = 0.0f;

    // Delay
    float delayTime     = 300.0f;
    float delayFeedback = 0.30f;
    float delayMix      = 0.0f;

    // Reverb (Dattorro)
    float reverbSize    = 0.55f;
    float reverbDamping = 0.45f;
    float reverbWidth   = 0.85f;
    float reverbMix     = 0.12f;
    float reverbPredelay = 15.0f;

    // Limiter
    float limiterThreshold = -3.0f;
    float limiterRelease   = 80.0f;

    // Enable toggles
    bool saturatorOn  = true;
    bool transientOn  = true;
    bool eqOn         = true;
    bool compressorOn = true;
    bool chorusOn     = false;
    bool delayOn      = false;
    bool reverbOn     = true;
    bool limiterOn    = true;
};

struct FxAvailability
{
    bool saturator  = true;
    bool transient  = true;
    bool eq         = true;
    bool compressor = true;
    bool chorus     = false;
    bool delay      = false;
    bool reverb     = true;
    bool limiter    = true;
};

// =========================================================================
// Accessors (definitions in InstrumentDefs.cpp)
// =========================================================================
Family      getFamily                (int instrumentIndex);
int         getFamilyStartIndex      (Family family);
const char* getFamilyName            (int familyIndex);
const char* getInstrumentName        (int instrumentIndex);
const char* getInstrumentShortName   (int instrumentIndex);
const InstrumentCharacteristics& getCharacteristics(int instrumentIndex);
InstrumentSettings               getDefaultSettings (int instrumentIndex);
const char*                       getInstrumentDescription(int instrumentIndex);
const FxAvailability&             getFxAvailability(int instrumentIndex);
bool                              isFxAvailable(int instrumentIndex, GlobalFxSlot slot);
GlobalFxSettings                  maskUnavailableFx(int instrumentIndex, const GlobalFxSettings& fx);

} // namespace mis
