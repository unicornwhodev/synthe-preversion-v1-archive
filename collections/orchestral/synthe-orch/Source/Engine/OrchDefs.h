#pragma once

#include <array>
#include <cstdint>

namespace mos   // Musique Orchestral Synth
{

// =========================================================================
// Instrument count & families
// =========================================================================
constexpr int kNumInstruments = 20;
constexpr int kNumFamilies    = 4;

//  Family 0 – CORDES      (0-4)   Violon, Alto, Violoncelle, Contrebasse, Harpe
//  Family 1 – BOIS        (5-11)  Flute, Hautbois, Clarinette, Basson, Piccolo, Cor anglais, Clarinette basse
//  Family 2 – CUIVRES     (12-15) Cor, Trompette, Trombone, Tuba
//  Family 3 – PERCUSSION  (16-19) Timbales, Celesta, Snare, Xylophone

constexpr int kFamilySize[]  = { 5, 7, 4, 4 };
constexpr int kFamilyStart[] = { 0, 5, 12, 16 };

enum class Family { Cordes = 0, Bois, Cuivres, Percussions };

// =========================================================================
// Oscillator mode
// =========================================================================
enum class OscMode { Additive = 0, Saw, Sine, Square, Modal };

struct MidiNoteRange
{
    int low;
    int high;
};

// =========================================================================
// Per-instrument characteristics
// =========================================================================
struct InstrCharacteristics
{
    OscMode oscMode;
    int     numPartials;
    float   inharmonicity;
    float   detuneAmount;
    int     numOscillators;

    float   vibratoRateHz;
    float   vibratoDepthCents;

    float   oddHarmonicBias;
    float   attackShape;

    float   decay1Ratio;
    float   decay2Time;
    float   sustainPlatform;

    float   bodyDelayRatio;
    float   bodyMaxFeedback;
    float   bodyDamping;

    float   pluckAmount;
    float   pluckSeconds;

    float   builtInWarmth;
    bool    isEnsemble;

    float   bowNoiseAmount;
    float   breathNoiseAmount;

    float   brightnessCutoffScale;   // brightness envelope base multiplier over fundamental
    float   vibratoDelaySec;         // delay before vibrato onset (0 = immediate)

    float   modalRatios[4];
    float   modalDecayMults[4];
    float   modalAmpScales[4];

    bool    hasFormants;
    float   formantFreqs[3];
    float   formantQs[3];
    float   formantGains[3];
    float   formantRegisterScale;
};

// =========================================================================
// Per-instrument adjustable settings (14 knobs)
// =========================================================================
struct InstrSettings
{
    float level          = 0.80f;
    float tuneSemitones  = 0.0f;
    float brightness     = 0.50f;
    float attackSeconds  = 0.10f;
    float decaySeconds   = 3.0f;
    float sustainLevel   = 0.60f;
    float releaseSeconds = 0.40f;
    float vibrato        = 0.50f;
    float warmth         = 0.40f;
    float detune         = 0.30f;
    float stereoWidth    = 0.50f;
    float character      = 0.50f;
    float cutoffHz       = 6000.0f;
    float pan            = 0.0f;
};

// =========================================================================
// Global FX settings stored per-preset
// =========================================================================
struct GlobalFxSettings
{
    // Saturator
    float satDrive         = 1.5f;
    float satMix           = 0.10f;

    // Transient
    float transientAttack  = 0.05f;
    float transientSustain = 0.0f;
    float transientMix     = 0.3f;

    // EQ
    float eqLowFreq   = 200.0f;
    float eqLowGain   = 0.0f;
    float eqMidFreq   = 1000.0f;
    float eqMidGain   = 0.0f;
    float eqMidQ      = 1.0f;
    float eqHighFreq  = 5000.0f;
    float eqHighGain  = 0.0f;

    // Compressor
    float compThreshold = -19.0f;
    float compRatio     = 3.0f;
    float compAttack    = 10.0f;
    float compRelease   = 120.0f;
    float compMix       = 1.0f;

    // Chorus
    float chorusRate    = 1.0f;
    float chorusDepth   = 0.5f;
    float chorusMix     = 0.0f;

    // Delay
    float delayTime     = 300.0f;
    float delayFeedback = 0.30f;
    float delayMix      = 0.0f;

    // Reverb (Dattorro)
    float reverbSize     = 0.65f;
    float reverbDamping  = 0.40f;
    float reverbWidth    = 0.90f;
    float reverbMix      = 0.28f;
    float reverbPredelay = 0.0f;
    int   reverbType     = 0;

    // Limiter
    float limiterThreshold = -1.0f;
    float limiterRelease   = 200.0f;
};

// =========================================================================
// FX availability per instrument
// =========================================================================
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

struct FxAvailability
{
    bool saturator  = false;
    bool transient  = false;
    bool eq         = true;
    bool compressor = true;
    bool chorus     = false;
    bool delay      = true;
    bool reverb     = true;
    bool limiter    = true;
};

// =========================================================================
// Accessors
// =========================================================================
Family                      getFamily(int instrIndex);
int                         getFamilyStartIndex(Family family);
const char*                 getFamilyName(int familyIndex);
const char*                 getInstrName(int instrIndex);
const char*                 getInstrShortName(int instrIndex);
MidiNoteRange               getInstrMidiNoteRange(int instrIndex);
const InstrCharacteristics& getCharacteristics(int instrIndex);
InstrSettings               getDefaultSettings(int instrIndex);
const char*                 getInstrDescription(int instrIndex);
const FxAvailability&       getFxAvailability(int instrIndex);
bool                        isFxAvailable(int instrIndex, GlobalFxSlot slot);

} // namespace mos