#include "InstrumentDefs.h"

#include <algorithm>

namespace mis
{
namespace
{
// =========================================================================
// Names
// =========================================================================
constexpr std::array<const char*, kNumInstruments> kNames = {
    // Strings
    "Nyckelharpa", "Gayageum", "Chapman Stick",
    "Yayli Tanbur", "Crwth",
    // Winds
    "Carnyx", "Aulos", "Fujara", "Gemshorn", "Dizi",
    // Percussion
    "Angklung", "Udu", "Pyeongyeong", "Cristal Baschet", "Mbira",
    "Handpan",
    // Conceptual
    "Theremine", "Ondes Martenot", "Pyrophone", "Hydraulophone", "Yaybahar"
};

constexpr std::array<const char*, kNumInstruments> kShortNames = {
    "NYCK", "GAYA", "STICK", "TANB", "CRWTH",
    "CRNX", "AULO", "FUJA", "GEMS", "DIZI",
    "ANGK", "UDU",  "PYEO", "CRIS", "MBIR",
    "HPAN",
    "THER", "ONDE", "PYRO", "HYDR", "YAYB"
};

constexpr std::array<const char*, kNumFamilies> kFamilyNames = {
    "CORDES", "VENTS", "PERCUSSIONS", "CONCEPTUELS"
};

// =========================================================================
// Instrument characteristics
// =========================================================================
constexpr std::array<InstrumentCharacteristics, kNumInstruments> kChars = {{
    // waveformMorph, bodyDelayRatio, bodyDamping, sympatheticSemis,
    // partialRatios[8], partialAmps[8], sustained,
    // synthesisMode, partialCount, inharmonicityB, exciterBrightness, exciterDecayMs,
    // bodyResonanceQ, numBodyModes, sympatheticMatrix[4], hasContinuousExcitation,
    // oddHarmonicsOnly, engineAttackAccent, engineTailDamping,
    // engineSpectralMotion, enginePitchFocus, engineDensityLimit

    // --- Strings ---
    // Nyckelharpa: bowed string — harmonic series with slight inharmonicity
    { 0.70f, 1.00f, 0.30f, 12.0f,
      { 2.0f, 3.0f, 4.02f, 5.0f, 6.0f, 7.0f, 8.04f, 9.0f }, { 0.30f, 0.15f, 0.08f, 0.04f, 0.02f, 0.01f, 0.006f, 0.003f }, true,
      SynthesisMode::Bowed, 8, 0.0001f, 0.45f, 200.0f,
      1.2f, 2, { 12.0f, 7.0f, 5.0f, 0.0f }, true },
    // Gayageum: plucked silk zither - nearly harmonic, short cithare bloom
    { 0.30f, 1.00f, 0.34f,  7.0f,
      { 2.0f, 2.99f, 4.01f, 5.02f }, { 0.24f, 0.14f, 0.07f, 0.03f }, false,
      SynthesisMode::Plucked, 6, 0.00005f, 0.42f, 4.0f,
      1.0f, 1, { 7.0f, 12.0f, 0.0f, 0.0f }, false, false,
      1.35f, 1.18f, 0.0f, 1.08f, 1.0f },
    // Chapman Stick: tapped strings — clean balanced harmonics
    { 0.15f, 1.00f, 0.20f, 12.0f,
      { 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f }, { 0.35f, 0.18f, 0.09f, 0.04f, 0.02f, 0.01f, 0.005f, 0.003f }, true,
      SynthesisMode::Plucked, 8, 0.00001f, 0.50f, 3.0f,
      0.8f, 1, { 12.0f, 0.0f, 0.0f, 0.0f }, false },
    // Yayli Tanbur: long-neck bowed lute — slightly flat upper partials
    { 0.80f, 0.80f, 0.15f,  5.0f,
      { 2.0f, 2.97f, 3.95f, 0.0f }, { 0.28f, 0.12f, 0.06f, 0.0f }, true,
      SynthesisMode::Bowed, 6, 0.0002f, 0.40f, 250.0f,
      1.5f, 2, { 5.0f, 12.0f, 0.0f, 0.0f }, true },
    // Crwth: bowed lyre — harmonic with double resonator
    { 0.75f, 0.95f, 0.20f, 12.0f,
      { 2.0f, 3.0f, 4.05f, 5.0f, 6.1f, 7.0f, 8.15f, 9.0f }, { 0.30f, 0.15f, 0.08f, 0.04f, 0.02f, 0.01f, 0.006f, 0.003f }, true,
      SynthesisMode::Bowed, 8, 0.00015f, 0.42f, 180.0f,
      1.3f, 2, { 12.0f, 7.0f, 5.0f, 19.0f }, true },

    // --- Winds ---
    // Carnyx: Celtic bronze trumpet — strong harmonic series
    { 0.55f, 1.00f, 0.15f, 12.0f,
      { 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f }, { 0.40f, 0.25f, 0.15f, 0.08f, 0.04f, 0.02f, 0.01f, 0.005f }, true,
      SynthesisMode::Blown, 10, 0.0f, 0.70f, 15.0f,
      2.0f, 1, { 12.0f, 0.0f, 0.0f, 0.0f }, true },
    // Aulos: cylindrical bore (clarinet-like) — odd harmonics only
    { 0.00f, 2.00f, 0.10f, 12.0f,
      { 3.0f, 5.0f, 7.0f, 0.0f }, { 0.40f, 0.20f, 0.10f, 0.0f }, true,
      SynthesisMode::Blown, 6, 0.0f, 0.55f, 20.0f,
      1.8f, 1, { 12.0f, 0.0f, 0.0f, 0.0f }, true, true,
      1.0f, 1.0f, 0.04f, 1.15f, 1.0f },
    // Fujara: overtone flute — 2nd octave dominant
    { 0.10f, 1.00f, 0.12f, 12.0f,
      { 2.0f, 3.0f, 4.0f, 0.0f }, { 0.20f, 0.35f, 0.15f, 0.0f }, true,
      SynthesisMode::Blown, 6, 0.0f, 0.30f, 25.0f,
      1.0f, 1, { 12.0f, 0.0f, 0.0f, 0.0f }, true, false,
      0.90f, 1.08f, 0.08f, 1.20f, 1.0f },
    // Gemshorn: near-sine, few harmonics
    { 0.05f, 1.00f, 0.35f, 12.0f,
      { 2.0f, 3.0f, 0.0f, 0.0f }, { 0.15f, 0.05f, 0.0f, 0.0f }, true,
      SynthesisMode::Blown, 4, 0.0f, 0.25f, 30.0f,
      0.8f, 1, { 12.0f, 0.0f, 0.0f, 0.0f }, true },
    // Dizi: bamboo flute with membrane buzz — bright harmonic timbre
    { 0.35f, 1.00f, 0.18f, 12.0f,
      { 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f }, { 0.30f, 0.20f, 0.12f, 0.06f, 0.03f, 0.015f, 0.008f, 0.004f }, true,
      SynthesisMode::Blown, 8, 0.0f, 0.65f, 18.0f,
      1.5f, 1, { 12.0f, 0.0f, 0.0f, 0.0f }, true, false,
      1.05f, 1.0f, 0.18f, 1.08f, 1.0f },

    // --- Percussion ---
    // Angklung: bamboo tubes — inharmonic bar modes anchored by a light pitch center
    { 0.20f, 1.10f, 0.45f, 12.0f,
      { 1.0f, 2.70f, 5.40f, 8.93f }, { 0.14f, 0.25f, 0.12f, 0.06f }, false,
      SynthesisMode::Struck, 6, 0.02f, 0.60f, 2.0f,
      3.0f, 5, { 12.0f, 0.0f, 0.0f, 0.0f }, false },
    // Udu: clay pot Helmholtz resonator — explicit air mode plus upper cavity resonances
    { 0.05f, 0.50f, 0.20f, 12.0f,
      { 1.0f, 1.50f, 3.0f, 4.5f }, { 0.42f, 0.35f, 0.15f, 0.06f }, false,
      SynthesisMode::Struck, 4, 0.0f, 0.40f, 3.0f,
      5.0f, 4, { 12.0f, 0.0f, 0.0f, 0.0f }, false, false,
      1.20f, 1.35f, 0.0f, 0.85f, 0.82f },
    // Pyeongyeong: stone chime — inharmonic triangle-like modes with a restrained root anchor
    { 0.10f, 1.00f, 0.08f, 12.0f,
      { 1.0f, 2.756f, 5.404f, 8.933f }, { 0.12f, 0.30f, 0.15f, 0.07f }, false,
      SynthesisMode::Struck, 6, 0.03f, 0.55f, 1.5f,
      8.0f, 7, { 12.0f, 0.0f, 0.0f, 0.0f }, false },
    // Cristal Baschet: glass rods + metallic resonators with an explicit singing fundamental
    { 0.08f, 1.20f, 0.15f,  7.0f,
      { 1.0f, 1.50f, 2.0f, 3.0f, 4.0f }, { 0.12f, 0.20f, 0.30f, 0.18f, 0.08f }, true,
      SynthesisMode::Struck, 8, 0.001f, 0.35f, 500.0f,
      6.0f, 6, { 7.0f, 12.0f, 19.0f, 0.0f }, true, false,
      0.95f, 1.10f, 0.04f, 1.0f, 0.90f },
    // Mbira: metal lamellae — slight inharmonicity
    { 0.25f, 1.00f, 0.30f, 12.0f,
      { 2.0f, 3.0f, 4.1f, 5.2f }, { 0.30f, 0.18f, 0.09f, 0.04f }, false,
      SynthesisMode::Plucked, 6, 0.005f, 0.50f, 4.0f,
      2.0f, 2, { 12.0f, 7.0f, 0.0f, 0.0f }, false },
    // Handpan: nitrided steel — tuned center note plus near-harmonic upper partials
    { 0.15f, 1.00f, 0.25f, 12.0f,
      { 1.0f, 2.0f, 3.0f, 4.0f }, { 0.20f, 0.35f, 0.15f, 0.06f }, false,
      SynthesisMode::Struck, 6, 0.0005f, 0.45f, 2.5f,
      4.0f, 5, { 12.0f, 7.0f, 5.0f, 3.0f }, false, false,
      1.05f, 1.25f, 0.0f, 1.05f, 0.86f },

    // --- Conceptual ---
    // Theremine: pure sine — no partials
    { 0.00f, 1.00f, 0.50f, 12.0f,
      { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, true,
      SynthesisMode::Electronic, 1, 0.0f, 0.10f, 50.0f,
      0.5f, 1, { 0.0f, 0.0f, 0.0f, 0.0f }, true, false,
      0.85f, 1.0f, 0.0f, 1.30f, 1.0f },
    // Ondes Martenot: rich electronic timbre
    { 0.45f, 1.00f, 0.25f,  7.0f,
      { 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f }, { 0.35f, 0.20f, 0.12f, 0.06f, 0.03f, 0.015f, 0.008f, 0.004f }, true,
      SynthesisMode::Electronic, 8, 0.0f, 0.50f, 40.0f,
      1.0f, 1, { 7.0f, 12.0f, 0.0f, 0.0f }, true, false,
      0.95f, 1.0f, 0.03f, 1.20f, 1.0f },
    // Pyrophone: flame-driven tube (Rijke tube modes)
    { 0.20f, 1.00f, 0.18f, 12.0f,
      { 2.0f, 3.0f, 5.0f, 0.0f }, { 0.25f, 0.15f, 0.06f, 0.0f }, true,
      SynthesisMode::Electronic, 6, 0.0f, 0.60f, 30.0f,
      2.0f, 2, { 12.0f, 0.0f, 0.0f, 0.0f }, true, false,
      0.90f, 1.0f, 0.12f, 1.05f, 1.0f },
    // Hydraulophone: water resonance — slightly inharmonic
    { 0.15f, 0.80f, 0.22f, 12.0f,
      { 2.0f, 3.50f, 5.0f, 0.0f }, { 0.20f, 0.15f, 0.08f, 0.0f }, true,
      SynthesisMode::Electronic, 6, 0.001f, 0.45f, 35.0f,
      1.5f, 2, { 12.0f, 0.0f, 0.0f, 0.0f }, true, false,
      0.85f, 1.10f, 0.16f, 0.95f, 1.0f },
    // Yaybahar: springs + membranes — complex inharmonic
    { 0.65f, 0.60f, 0.12f,  5.0f,
      { 1.50f, 2.80f, 4.20f, 6.0f, 7.85f, 9.80f, 12.0f, 14.25f }, { 0.25f, 0.20f, 0.12f, 0.06f, 0.03f, 0.015f, 0.008f, 0.004f }, true,
      SynthesisMode::Bowed, 10, 0.003f, 0.55f, 300.0f,
      3.0f, 4, { 5.0f, 12.0f, 7.0f, 19.0f }, true, false,
      0.90f, 1.45f, 0.08f, 0.85f, 0.62f },
}};

// =========================================================================
// Default settings per instrument
// level, tune, attack, decay, sustain, release, exciter, body, sympathetic,
// noise, drive, cutoff, filterQ, pan,
// breathPressure, bowSpeed, bowPressure, strikePosition, brightness
// =========================================================================
constexpr std::array<InstrumentSettings, kNumInstruments> kDefaults = {{
    // --- Strings ---
    // Nyckelharpa: bowed, keys, sympathetic
    { 0.82f, 0.0f, 0.050f, 1.50f, 0.60f, 0.40f, 0.35f, 0.60f, 0.50f, 0.08f, 1.3f, 6000.0f, 1.2f, 0.0f,
      0.0f, 0.55f, 0.50f, 0.5f, 0.50f },
    // Gayageum: plucked silk zither, short readable decay
    { 0.78f, 0.0f, 0.004f, 1.20f, 0.20f, 0.35f, 0.35f, 0.50f, 0.18f, 0.04f, 1.15f, 5200.0f, 0.8f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.42f, 0.50f },
    // Chapman Stick: tapped, clean
    { 0.80f, 0.0f, 0.002f, 0.80f, 0.40f, 0.20f, 0.10f, 0.30f, 0.05f, 0.02f, 1.2f, 8000.0f, 0.6f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.5f, 0.55f },
    // Yayli Tanbur: long bow, pitch glide
    { 0.76f, 0.0f, 0.080f, 2.50f, 0.50f, 0.60f, 0.45f, 0.70f, 0.30f, 0.12f, 1.5f, 4500.0f, 1.0f, 0.0f,
      0.0f, 0.45f, 0.55f, 0.5f, 0.42f },
    // Crwth: bowed lyre, double resonator
    { 0.80f, 0.0f, 0.060f, 1.80f, 0.55f, 0.50f, 0.40f, 0.65f, 0.55f, 0.10f, 1.4f, 5000.0f, 1.1f, 0.0f,
      0.0f, 0.50f, 0.48f, 0.5f, 0.45f },
    // --- Winds ---
    // Carnyx: bronze trumpet, brassy
    { 0.85f, 0.0f, 0.030f, 1.00f, 0.70f, 0.30f, 0.50f, 0.60f, 0.10f, 0.15f, 2.0f, 7000.0f, 1.5f, 0.0f,
      0.70f, 0.0f, 0.0f, 0.5f, 0.65f },
    // Aulos: double reed, cylindrical
    { 0.78f, 0.0f, 0.020f, 1.20f, 0.65f, 0.25f, 0.35f, 0.55f, 0.08f, 0.20f, 1.3f, 5500.0f, 1.3f, 0.0f,
      0.60f, 0.0f, 0.0f, 0.5f, 0.55f },
    // Fujara: overtone flute, breathy
    { 0.72f, 0.0f, 0.050f, 2.00f, 0.50f, 0.60f, 0.25f, 0.50f, 0.05f, 0.30f, 1.1f, 3500.0f, 0.7f, 0.0f,
      0.45f, 0.0f, 0.0f, 0.5f, 0.35f },
    // Gemshorn: nearly pure sine
    { 0.75f, 0.0f, 0.030f, 1.50f, 0.60f, 0.40f, 0.15f, 0.35f, 0.02f, 0.10f, 1.0f, 6000.0f, 0.5f, 0.0f,
      0.40f, 0.0f, 0.0f, 0.5f, 0.30f },
    // Dizi: membrane flutter
    { 0.80f, 0.0f, 0.015f, 1.00f, 0.65f, 0.20f, 0.30f, 0.45f, 0.10f, 0.18f, 1.4f, 9000.0f, 1.2f, 0.0f,
      0.55f, 0.0f, 0.0f, 0.5f, 0.60f },
    // --- Percussion ---
    // Angklung: bamboo tubes
    { 0.82f, 0.0f, 0.005f, 0.60f, 0.00f, 0.30f, 0.20f, 0.40f, 0.10f, 0.15f, 1.2f, 7000.0f, 0.8f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.45f, 0.55f },
    // Udu: clay pot, Helmholtz
    { 0.80f, 0.0f, 0.010f, 1.00f, 0.00f, 0.50f, 0.15f, 0.70f, 0.05f, 0.08f, 1.1f, 2000.0f, 2.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.60f, 0.35f },
    // Pyeongyeong: stone chime, long decay
    { 0.78f, 0.0f, 0.001f, 3.00f, 0.00f, 1.00f, 0.10f, 0.30f, 0.15f, 0.03f, 1.0f, 6000.0f, 3.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.50f, 0.50f },
    // Cristal Baschet: glass rods, metal resonators
    { 0.74f, 0.0f, 0.100f, 4.00f, 0.40f, 1.50f, 0.30f, 0.50f, 0.40f, 0.06f, 1.2f, 5000.0f, 1.5f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.50f, 0.40f },
    // Mbira: metal lamellae, inharmonic
    { 0.84f, 0.0f, 0.001f, 2.50f, 0.00f, 0.20f, 0.15f, 0.35f, 0.20f, 0.04f, 1.6f, 8000.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.55f, 0.55f },
    // Handpan: steel tongue drum, harmonic resonance
    { 0.82f, 0.0f, 0.003f, 2.50f, 0.00f, 1.50f, 0.15f, 0.65f, 0.35f, 0.04f, 1.1f, 6000.0f, 1.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.50f, 0.48f },
    // --- Conceptual ---
    // Theremine: pure sine, vibrato
    { 0.76f, 0.0f, 0.020f, 0.50f, 0.80f, 0.20f, 0.05f, 0.10f, 0.00f, 0.02f, 1.0f, 12000.0f, 0.5f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.5f, 0.50f },
    // Ondes Martenot: rich waveform, diffusers
    { 0.80f, 0.0f, 0.010f, 1.00f, 0.75f, 0.30f, 0.10f, 0.40f, 0.20f, 0.03f, 1.2f, 8000.0f, 0.8f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.5f, 0.55f },
    // Pyrophone: flame-driven tubes
    { 0.78f, 0.0f, 0.005f, 0.80f, 0.50f, 0.30f, 0.60f, 0.50f, 0.10f, 0.35f, 1.8f, 6000.0f, 1.2f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.5f, 0.58f },
    // Hydraulophone: water turbulence
    { 0.72f, 0.0f, 0.030f, 1.50f, 0.40f, 0.50f, 0.40f, 0.55f, 0.10f, 0.40f, 1.3f, 4000.0f, 0.9f, 0.0f,
      0.0f, 0.0f, 0.0f, 0.5f, 0.45f },
    // Yaybahar: springs + membranes
    { 0.78f, 0.0f, 0.060f, 3.00f, 0.50f, 1.00f, 0.35f, 0.70f, 0.60f, 0.08f, 1.4f, 5000.0f, 1.1f, 0.0f,
      0.0f, 0.50f, 0.45f, 0.5f, 0.48f },
}};

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
    FX(true,  true,  true,  true,  true,  true,  true,  true),
    FX(true,  true,  true,  true,  false, false, true,  true),
    FX(true,  true,  true,  true,  false, false, true,  true),

    FX(true,  true,  true,  true,  false, false, true,  true),
    FX(true,  true,  true,  false, false, false, true,  true),
    FX(false, true,  true,  false, false, true,  true,  true),
    FX(false, false, true,  false, true,  false, true,  true),
    FX(true,  true,  true,  false, true,  true,  true,  true),

    FX(false, true,  true,  false, false, false, true,  true),
    FX(false, true,  true,  false, false, false, true,  true),
    FX(false, true,  true,  false, false, false, true,  true),
    FX(false, true,  true,  false, true,  true,  true,  true),
    FX(true,  true,  true,  true,  false, true,  true,  true),
    FX(false, true,  true,  false, false, true,  true,  true),

    FX(false, false, true,  false, true,  true,  true,  true),
    FX(true,  false, true,  true,  true,  true,  true,  true),
    FX(true,  true,  true,  true,  false, false, true,  true),
    FX(false, false, true,  false, true,  true,  true,  true),
    FX(false, true,  true,  false, true,  true,  true,  true),
  }};

// =========================================================================
// Instrument descriptions (100-150 words, French)
// =========================================================================
constexpr std::array<const char*, kNumInstruments> kDescriptions = {{
    // Nyckelharpa
    "Le nyckelharpa est un instrument a cordes frottees suedois dont les origines remontent au "
    "XIVe siecle. Son nom signifie \"vielle a clavier\". Le musicien frotte les cordes avec un "
    "archet tout en actionnant des touches en bois qui pressent des tangentes contre les cordes "
    "pour modifier la hauteur. L'instrument possede generalement quatre cordes melodiques et "
    "douze cordes sympathiques qui vibrent par resonance, conferant au nyckelharpa sa sonorite "
    "riche et enveloppante. Redecouvert au XXe siecle, il est devenu embleme du patrimoine "
    "musical scandinave et figure desormais sur la liste du patrimoine culturel immateriel de l'UNESCO.",

    // Gayageum
    "Le gayageum (ou gayageum) est une cithare coreenne a douze cordes en soie, jouee en "
    "position assise, l'instrument pose sur les genoux. Datant du VIe siecle, il est "
    "attribue au roi Gasil du royaume de Gaya. Les cordes sont tendues sur des chevalets "
    "mobiles (anjok) places sur une caisse de resonance en paulownia. Le musicien pince "
    "les cordes de la main droite et module le son de la main gauche par des techniques "
    "de vibrato (nonghyeon) et de glissement expressif. Le gayageum occupe une place "
    "centrale dans la musique traditionnelle coreenne, du repertoire de cour (jeongak) "
    "aux compositions contemporaines.",

    // Chapman Stick
    "Le Chapman Stick est un instrument electrique invente par Emmett Chapman en 1974. "
    "Il se compose de dix a douze cordes tendues sur un manche large, joue par la technique "
    "du tapping a deux mains. Chaque main couvre une section : la gauche pour les basses, "
    "la droite pour la melodie et les accords. Cette approche permet au musicien de jouer "
    "simultanement ligne de basse, harmonie et melodie, comme un orchestre miniature. "
    "Le Stick utilise des micros piezoelectriques ou magnetiques et offre une palette "
    "sonore allant du jazz fusion au rock progressif. Tony Levin et Greg Howard comptent "
    "parmi ses interpretes les plus celebres.",

    // Yayli Tanbur
    "Le yayli tanbur est un luth a manche long de la tradition ottomane, joue avec un "
    "archet. Son manche extremement long permet de produire les micro-intervalles "
    "caracteristiques des maqams turcs. L'instrument possede generalement quatre cordes "
    "principales et des frettes mobiles en boyau. L'archet, frotte sur les cordes au-dessus "
    "d'une petite caisse hemispherique, cree un son continu et meditatif. Le yayli tanbur "
    "est un pilier de la musique classique ottomane et de la musique soufie, ou ses longues "
    "lignes melodiques accompagnent les ceremonies de derviches tourneurs. Sa sonorite "
    "nasale et vibrante evoque la spiritualite orientale.",

    // Crwth
    "Le crwth (prononce \"crouth\") est une lyre a archet galloise dont l'histoire remonte "
    "a l'epoque medievale. Il possede six cordes : quatre melodiques jouees a l'archet et "
    "deux bourdons pinces par le pouce. Sa caisse de resonance rectangulaire est taillee "
    "dans une seule piece de bois. Un chevalet asymetrique, dont un pied traverse la table "
    "d'harmonie pour toucher le fond, cree un systeme de resonance double unique. Le crwth "
    "a accompagne les bardes gallois pendant des siecles avant de decliner au XIXe siecle. "
    "Des luthiers contemporains ont relance sa fabrication, preservant cet heritage musical celtique.",

    // Carnyx
    "Le carnyx est une trompette de guerre celtique utilisee entre le IIIe siecle "
    "avant J.-C. et le IIe siecle apres J.-C. Cet instrument en bronze, haut de "
    "pres de deux metres, se tenait verticalement sur le champ de bataille. Son "
    "pavillon etait sculpte en forme de tete d'animal, generalement un sanglier, "
    "dont la gueule ouverte projetait un son grave et terrifiant. Des exemplaires "
    "ont ete decouverts en Ecosse (Deskford) et en France (Tintignac). Le carnyx "
    "a ete reconstitue par des archeologues-musiciens, revelant une richesse "
    "harmonique surprenante. Il symbolise la puissance guerriere des peuples celtes "
    "et figure dans de nombreuses representations antiques.",

    // Aulos
    "L'aulos est un instrument a vent grec antique compose de deux tuyaux joues "
    "simultanement. Chaque tuyau possede une anche double en roseau et plusieurs "
    "trous de jeu. Le musicien (aulete) tenait un tuyau dans chaque main, produisant "
    "melodie et bourdon ou deux voix melodiques en parallele. Un bandeau de cuir "
    "(phorbeia) maintenait les joues pour supporter la pression. L'aulos accompagnait "
    "les competitions athletiques, les banquets, le theatre et les rituels dionysiaques. "
    "Souvent confondu avec une flute, il s'apparente davantage au hautbois. "
    "Aristote et Platon ont debattu de sa valeur educative, tant son pouvoir "
    "emotionnel etait reconnu dans la Grece antique.",

    // Fujara
    "La fujara est une flute a harmoniques slovaque pouvant atteindre 1,80 metre "
    "de longueur. Classee au patrimoine immateriel de l'UNESCO, elle etait jouee "
    "par les bergers des Carpates. Le musicien souffle dans un tuyau lateral relie "
    "au corps principal et produit des sons en utilisant les harmoniques naturels. "
    "Seulement trois trous de jeu suffisent pour creer des melodies contemplatives "
    "riches en harmoniques. Le son est a la fois aerien et profond, avec un souffle "
    "perceptible qui fait partie integrante du timbre. La fujara est traditionnellement "
    "fabriquee en bois de sureau et decoree de motifs graves symboliques.",

    // Gemshorn
    "Le gemshorn est un instrument a vent medieval fabrique a partir d'une corne "
    "de chamois (Gemse en allemand). C'est un ancetre de l'ocarina : un instrument "
    "a embouchure de type flute avec des trous de jeu perces dans la corne. Son "
    "timbre est doux et veloute, proche d'une flute a bec mais avec une chaleur "
    "particuliere due au materiau naturel. La corne est bouchee a l'extremite large "
    "et l'embouchure est taillee a la pointe. Le gemshorn produit environ une octave "
    "et demie de notes. Populaire du XIVe au XVIe siecle, il a ete redecouvert par "
    "le mouvement de musique ancienne au XXe siecle.",

    // Dizi
    "Le dizi est une flute traversiere chinoise en bambou, l'un des instruments "
    "les plus anciens et populaires de la musique traditionnelle chinoise. Sa "
    "particularite est le mokon, une fine membrane de roseau collee sur un trou "
    "situe entre l'embouchure et les trous de jeu. Cette membrane vibre par "
    "sympathie et confere au dizi son timbre brillant et nasillard caracteristique. "
    "Le dizi existe en plusieurs tailles couvrant differents registres, du qudi "
    "grave au bangdi aigu. Il est omnipresent dans l'opera chinois, la musique "
    "folklorique et les ensembles orchestraux. Sa technique inclut des ornements "
    "rapides, des glissandi et des vibratos expressifs.",

    // Angklung
    "L'angklung est un instrument de percussion indonesien compose de deux a "
    "quatre tubes de bambou suspendus dans un cadre en bambou. Lorsqu'on secoue "
    "le cadre, les tubes coulissent et frappent contre les parois, produisant "
    "un son resonant et cristallin. Chaque angklung ne produit qu'une seule note, "
    "d'ou la necessite de jouer en ensemble pour creer des melodies. Originaire "
    "de Java occidental (Sunda), l'angklung est inscrit au patrimoine immateriel "
    "de l'UNESCO depuis 2010. Les orchestres d'angklung peuvent reunir des dizaines "
    "de musiciens, chacun responsable d'une note, creant une musique collective "
    "d'une grande beaute harmonique.",

    // Udu
    "L'udu est un instrument de percussion nigerien en forme de jarre en argile "
    "comportant un trou supplementaire sur le flanc. Issu de la tradition Igbo, "
    "il etait a l'origine un simple pot a eau dont les femmes ont decouvert les "
    "qualites musicales. Le musicien frappe les orifices et la surface avec les "
    "mains, produisant des sons graves et resonants bases sur le principe de "
    "resonance de Helmholtz. En couvrant et decouvrant les trous, on module "
    "la hauteur et le timbre. L'udu est devenu un instrument de percussion "
    "moderne utilise en world music, jazz et musique contemporaine, apprecie "
    "pour ses basses profondes et organiques.",

    // Pyeongyeong
    "Le pyeongyeong est un lithophone coreen compose de seize pierres en forme "
    "de L suspendues a un cadre en bois ouvrage. Chaque pierre, taillee dans "
    "du jade ou du calcaire, est accordee a une note precise par meulage. "
    "Le musicien frappe les pierres avec un maillet en corne de buffle, "
    "produisant un son pur, cristallin et d'une grande sustentation. Le "
    "pyeongyeong remonte a l'epoque Goryeo (Xe siecle) et est utilise dans "
    "la musique rituelle confuceenne (aak) et la musique de cour coreenne. "
    "Sa sonorite etheree symbolise l'harmonie celeste et accompagne les "
    "ceremonies les plus solennelles du patrimoine musical coreen.",

    // Cristal Baschet
    "Le cristal Baschet est un instrument invente en 1952 par les freres "
    "Bernard et Francois Baschet. Il se joue en frottant avec les doigts "
    "mouilles des tiges de verre chromatiquement accordees. Les vibrations "
    "sont transmises par des tiges metalliques a de grands reflecteurs en "
    "forme de flamme qui amplifient le son sans electricite. Le timbre est "
    "etheree, proche du verre chantant mais avec une richesse harmonique "
    "remarquable et un sustain quasi infini. L'instrument couvre environ "
    "cinq octaves. Utilise dans la musique contemporaine, le cinema et "
    "les installations sonores, le cristal Baschet fascine par sa beaute "
    "visuelle autant que par sa sonorite envoûtante.",

    // Mbira
    "La mbira (ou sanza, kalimba) est un instrument a lamelles metalliques "
    "originaire d'Afrique australe, particulierement du Zimbabwe ou elle "
    "occupe une place sacree dans la culture Shona. Des lamelles en metal "
    "de longueurs differentes sont fixees sur une planche de bois et "
    "pincees avec les pouces. La mbira est souvent placee dans une "
    "calebasse (deze) qui amplifie et enrichit le son par resonance. "
    "Des capsules vibrent par sympathie, ajoutant un bourdonnement "
    "caracteristique. La mbira dzavadzimu est jouee lors de ceremonies "
    "rituelles pour communiquer avec les esprits ancestraux. Ses "
    "motifs cycliques et hypnotiques en font un instrument meditattif "
    "et captivant.",

    // Handpan
    "Le handpan est un instrument de percussion melodique en acier, cree au "
    "debut des annees 2000 en Suisse par PANArt sous le nom de Hang. Il se "
    "compose de deux coques d'acier nitride assemblees, formant une lentille "
    "convexe. La face superieure comporte un dome central (ding) entoure de "
    "sept a neuf zones tonales disposees en cercle. Le musicien frappe ces "
    "zones avec les doigts et les paumes, produisant des sons riches en "
    "harmoniques, a la fois percussifs et melodiques. Chaque handpan est "
    "accorde selon une gamme specifique (pentatonique, mineure, etc.). Sa "
    "sonorite enveloppante et meditative a conquis le monde entier, devenant "
    "un symbole de la musique contemplative de rue et des pratiques de "
    "bien-etre sonore.",

    // Theremine
    "Le theremine est le premier instrument de musique electronique, "
    "invente en 1920 par le physicien russe Leon Theremine. C'est le "
    "seul instrument que l'on joue sans contact physique : deux antennes "
    "detectent la position des mains du musicien dans l'espace. La main "
    "droite controle la hauteur en s'approchant ou s'eloignant de l'antenne "
    "verticale, tandis que la main gauche module le volume pres de l'antenne "
    "horizontale en boucle. Le son, une onde sinusoidale pure, evoque une "
    "voix etheree et planante. Le theremine a marque l'histoire du cinema "
    "(films de science-fiction) et de la musique, de Clara Rockmore aux "
    "compositions contemporaines. Sa maitrise requiert une oreille absolue "
    "et une gestuelle d'une extreme precision.",

    // Ondes Martenot
    "Les ondes Martenot sont un instrument electronique invente en 1928 "
    "par Maurice Martenot. Le musicien controle la hauteur par un clavier "
    "et un fil tendu (ruban) deplace avec un anneau au doigt, permettant "
    "des glissandi d'une fluidite remarquable. L'originalite reside dans "
    "ses diffuseurs : le principal, un haut-parleur classique, le resonance "
    "(cordes sympathiques), le metallique et le palme (avec ressorts). "
    "Chaque diffuseur colore le son differemment. Olivier Messiaen a ecrit "
    "de nombreuses oeuvres majeures pour cet instrument, dont la Turangalila-Symphonie. "
    "Les ondes Martenot sont enseignees au Conservatoire de Paris et restent "
    "un instrument vivant de la musique contemporaine francaise.",

    // Pyrophone
    "Le pyrophone est un orgue a flammes invente en 1875 par Georges "
    "Frederic Kastner. Des tubes de verre ou de metal de differentes "
    "longueurs sont chauffes par des flammes de gaz a leur base. La "
    "combustion cree des oscillations d'air dans les tubes selon le "
    "principe du tube de Rijke, produisant des sons a la frequence de "
    "resonance de chaque tube. Le timbre est unique : a la fois organique "
    "et mineral, avec un souffle chaud perceptible. Les pyrophones modernes "
    "utilisent du propane ou du butane avec des electrovannes pour un "
    "controle precis. Cet instrument spectaculaire combine musique et element "
    "pyrotechnique, creant des performances visuelles et sonores fascinantes.",

    // Hydraulophone
    "L'hydraulophone est un instrument invente par Steve Mann au debut des "
    "annees 2000. Il produit du son par la manipulation directe de jets d'eau : "
    "le musicien bouche et debouche des orifices d'ou jaillit l'eau sous "
    "pression, ce qui module le flux hydraulique dans des chambres de "
    "resonance. Chaque jet correspond a une note. Le contact avec l'eau cree "
    "une experience tactile et sensorielle unique. Certains modeles sont "
    "installes comme fontaines musicales interactives dans des espaces publics. "
    "Le son est doux, avec des textures liquides naturelles. L'hydraulophone "
    "est egalement utilise comme instrument therapeutique pour les personnes "
    "malvoyantes, le retour haptique de l'eau offrant une dimension "
    "supplementaire a l'expression musicale.",

    // Yaybahar
    "Le yaybahar est un instrument acoustique invente par le musicien turc "
    "Gorkem Sen. Il combine un manche a cordes frottees avec un archet, "
    "deux grosses membranes tendues (similaires a des peaux de tambour) et "
    "de longs ressorts helicoidaux reliant les cordes aux membranes. "
    "Les vibrations des cordes voyagent a travers les ressorts jusqu'aux "
    "membranes qui les amplifient et les transforment. Le resultat est un "
    "son acoustique qui ressemble etonnamment a un synthétiseur : des nappes "
    "reverberantes, des echos naturels et des textures sonores futuristes. "
    "Sans aucune electronique, le yaybahar produit des sons que l'on croirait "
    "generes par ordinateur, brouillant la frontiere entre acoustique et "
    "electronique.",
}};

} // namespace

Family getFamily(const int instrumentIndex)
{
    const auto idx = std::clamp(instrumentIndex, 0, kNumInstruments - 1);
    for (int f = kNumFamilies - 1; f > 0; --f)
        if (idx >= kFamilyStart[f]) return static_cast<Family>(f);
    return Family::Strings;
}

int getFamilyStartIndex(const Family family)
{
    return kFamilyStart[std::clamp(static_cast<int>(family), 0, kNumFamilies - 1)];
}

const char* getFamilyName(const int familyIndex)
{
    return kFamilyNames[static_cast<std::size_t>(std::clamp(familyIndex, 0, kNumFamilies - 1))];
}

const char* getInstrumentName(const int instrumentIndex)
{
    return kNames[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))];
}

const char* getInstrumentShortName(const int instrumentIndex)
{
    return kShortNames[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))];
}

const InstrumentCharacteristics& getCharacteristics(const int instrumentIndex)
{
    return kChars[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))];
}

InstrumentSettings getDefaultSettings(const int instrumentIndex)
{
    return kDefaults[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))];
}

const char* getInstrumentDescription(const int instrumentIndex)
{
    return kDescriptions[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))];
}

const FxAvailability& getFxAvailability(const int instrumentIndex)
{
  return kFxAvailability[static_cast<std::size_t>(std::clamp(instrumentIndex, 0, kNumInstruments - 1))];
}

bool isFxAvailable(const int instrumentIndex, const GlobalFxSlot slot)
{
  const auto& availability = getFxAvailability(instrumentIndex);
  switch (slot)
  {
    case GlobalFxSlot::Saturator:  return availability.saturator;
    case GlobalFxSlot::Transient:  return availability.transient;
    case GlobalFxSlot::Eq:         return availability.eq;
    case GlobalFxSlot::Compressor: return availability.compressor;
    case GlobalFxSlot::Chorus:     return availability.chorus;
    case GlobalFxSlot::Delay:      return availability.delay;
    case GlobalFxSlot::Reverb:     return availability.reverb;
    case GlobalFxSlot::Limiter:    return availability.limiter;
  }

  return true;
}

GlobalFxSettings maskUnavailableFx(const int instrumentIndex, const GlobalFxSettings& fx)
{
  auto masked = fx;
  const auto& availability = getFxAvailability(instrumentIndex);
  masked.saturatorOn  = availability.saturator  ? masked.saturatorOn  : false;
  masked.transientOn  = availability.transient  ? masked.transientOn  : false;
  masked.eqOn         = availability.eq         ? masked.eqOn         : false;
  masked.compressorOn = availability.compressor ? masked.compressorOn : false;
  masked.chorusOn     = availability.chorus     ? masked.chorusOn     : false;
  masked.delayOn      = availability.delay      ? masked.delayOn      : false;
  masked.reverbOn     = availability.reverb     ? masked.reverbOn     : false;
  masked.limiterOn    = availability.limiter    ? masked.limiterOn    : false;
  return masked;
}

} // namespace mis
