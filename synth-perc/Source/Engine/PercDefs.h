#pragma once

#include <array>
#include <cstdint>

namespace mpc   // Musique Percussion
{

// =========================================================================
// Instrument count & families
// =========================================================================
constexpr int kNumInstruments = 9;
constexpr int kNumFamilies    = 3;

//  Family 0 – PERCUSSIONS  (0-2)   Timbales, Marimba, Djemb\xC3\xa9
//  Family 1 – AMBIANCE     (3-5)   B\xC3\xa2ton de Pluie, Bol Chantant, Carillon \xC3\x89olien
//  Family 2 – M\xC3\x89TALLIQUES  (6-8)   Cloche Tubulaire, Triangle, Glockenspiel

constexpr int kFamilySize[]  = { 3, 3, 3 };
constexpr int kFamilyStart[] = { 0, 3, 6 };

enum class Family { Percussions = 0, Ambiance, Metalliques };

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
// Synthesis mode
// =========================================================================
enum class SynthMode { Modal = 0, Noise, Hybrid };

// =========================================================================
// Per-instrument characteristics (compile-time-friendly)
// =========================================================================
struct InstrCharacteristics
{
    SynthMode synthMode;
    int       numModes;          // number of resonant modes (1-16)
    float     modeSpread;        // inharmonicity factor (1.0 = harmonic)
    float     modeDecayBase;     // base decay time for fundamental (sec)
    float     modeDecaySpread;   // how much higher modes decay faster

    float     noiseAmount;       // initial noise burst (0-1)
    float     noiseDecay;        // noise decay time (sec)
    float     noiseBrightness;   // noise filter (0 dark, 1 bright)

    float     bodyResonance;     // comb filter feedback (0 = off)
    float     bodyDamping;       // comb HF damping
    float     bodyDelay;         // comb delay ratio
    bool      hasBodyResonator;  // true = body resonator active, false = no body

    float     metallic;          // 0 = wood, 1 = metal
    float     ringTime;          // ring-out multiplier
    float     pitchFollowing;    // 0 = noise-only, 1 = fully pitched

    float     randomization;     // timing/pitch random (wind chimes etc.)
    float     builtInBrightness; // timbral base brightness

    float     clickAmount;       // attack click transient (mallet/stick, 0=none)
    float     clickDecayMs;      // click decay time in ms (wood ~6, metal ~2)
    float     brightBaseMultiplier; // brightness base multiplier (wood ~3, metal ~8)
    bool      useFixedRatios;    // if true, use fixedRatios[] instead of power law
    float     fixedRatios[12];   // physical mode frequency ratios (0.0f = unused slot)

    float     decayNorm;         // inertance normalization: natural decay duration
                                 // (modeDecayBase × default decaySeconds).
                                 // Used to normalize Decay knob response across instruments.
};

// =========================================================================
// Per-instrument adjustable settings (16 knobs)
// =========================================================================
struct InstrSettings
{
    float level          = 0.80f;
    float tuneSemitones  = 0.0f;
    float brightness     = 0.50f;
    float attackSeconds  = 0.005f;
    float decaySeconds   = 1.5f;
    float sustainLevel   = 0.0f;
    float releaseSeconds = 0.20f;
    float damping        = 0.50f;   // high-frequency decay rate
    float body           = 0.50f;   // resonance amount
    float noise          = 0.50f;   // noise burst amount
    float stereoWidth    = 0.50f;   // stereo spread
    float color          = 0.50f;   // timbral shift
    float cutoffHz       = 8000.0f;
    float pan            = 0.0f;

    // One-shot mode: short decay for rhythmic/groove use (10-100ms)
    bool  oneShot        = false;
    float oneShotDecayMs = 50.0f;   // decay time in ms when oneShot=true
};

// =========================================================================
// Global FX settings (30 floats — one per FX param)
// =========================================================================
struct GlobalFxSettings
{
    // Saturator
    float satDrive          = 1.0f;     // 1 .. 16
    float satMix            = 0.0f;     // 0 .. 1

    // Transient Shaper
    float transientAttack   = 0.0f;     // -1 .. +1
    float transientSustain  = 0.0f;     // -1 .. +1
    float transientMix      = 0.0f;     // 0 .. 1

    // Compressor
    float compThreshold     = -19.0f;   // -60 .. 0 dB
    float compRatio         = 3.0f;     // 1 .. 20
    float compAttack        = 10.0f;    // 0.1 .. 100 ms
    float compRelease       = 120.0f;   // 10 .. 500 ms
    float compMakeup        = 0.0f;     // 0 .. 24 dB
    float compMix           = 1.0f;     // 0 .. 1

    // EQ
    float eqLowFreq         = 200.0f;   // 20 .. 2000
    float eqLowGain         = 0.0f;     // -12 .. 12 dB
    float eqMidFreq         = 1000.0f;  // 200 .. 8000
    float eqMidGain         = 0.0f;     // -12 .. 12 dB
    float eqMidQ            = 1.0f;     // 0.1 .. 10
    float eqHighFreq        = 5000.0f;  // 2000 .. 20000
    float eqHighGain        = 0.0f;     // -12 .. 12 dB

    // Chorus
    float chorusRate        = 1.0f;     // 0.1 .. 5 Hz
    float chorusDepth       = 0.5f;     // 0 .. 1
    float chorusMix         = 0.0f;     // 0 .. 1

    // Delay
    float delayTime         = 300.0f;   // 1 .. 2000 ms
    float delayFeedback     = 0.30f;    // 0 .. 0.95
    float delayMix          = 0.0f;     // 0 .. 1

    // Reverb (Dattorro)
    float reverbSize        = 0.45f;    // 0 .. 1 (decay)
    float reverbDamping     = 0.55f;    // 0 .. 1
    float reverbWidth       = 0.80f;    // 0 .. 1
    float reverbMix         = 0.0f;     // 0 .. 1
    float reverbPredelay    = 0.0f;     // 0 .. 100 ms

    // Limiter
    float limiterThreshold  = -0.3f;    // -12 .. 0 dB
    float limiterRelease    = 50.0f;    // 1 .. 200 ms

    // Enable toggles
    bool  saturatorOn       = true;
    bool  transientOn       = true;
    bool  eqOn              = true;
    bool  compressorOn      = true;
    bool  chorusOn          = false;
    bool  delayOn           = false;
    bool  reverbOn          = true;
    bool  limiterOn         = true;
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
// Accessors (implemented in PercDefs.cpp)
// =========================================================================
Family                      getFamily(int instrIndex);
int                         getFamilyStartIndex(Family family);
const char*                 getFamilyName(int familyIndex);
const char*                 getInstrName(int instrIndex);
const char*                 getInstrShortName(int instrIndex);
const InstrCharacteristics& getCharacteristics(int instrIndex);
bool hasBodyResonator(int instrIndex);
InstrSettings               getDefaultSettings(int instrIndex);
const char*                 getInstrDescription(int instrIndex);
const FxAvailability&       getFxAvailability(int instrIndex);
bool                        isFxAvailable(int instrIndex, GlobalFxSlot slot);
GlobalFxSettings            maskUnavailableFx(int instrIndex, const GlobalFxSettings& fx);

} // namespace mpc
