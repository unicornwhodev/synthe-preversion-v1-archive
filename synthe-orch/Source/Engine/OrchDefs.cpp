#include "OrchDefs.h"

#include <algorithm>

namespace mos
{
namespace
{
InstrCharacteristics withFormants(InstrCharacteristics c,
                                  float f1, float q1, float g1,
                                  float f2, float q2, float g2,
                                  float f3, float q3, float g3)
{
    c.hasFormants = true;
    c.formantFreqs[0] = f1; c.formantQs[0] = q1; c.formantGains[0] = g1;
    c.formantFreqs[1] = f2; c.formantQs[1] = q2; c.formantGains[1] = g2;
    c.formantFreqs[2] = f3; c.formantQs[2] = q3; c.formantGains[2] = g3;
    return c;
}

InstrCharacteristics withFormantRegisterScale(InstrCharacteristics c, float amount)
{
    c.formantRegisterScale = amount;
    return c;
}

InstrCharacteristics makePiccolo()
{
    // Piccolo: very high register, bright, cylindrical bore flute
    InstrCharacteristics c{};
    c.oscMode = OscMode::Additive;
    c.numPartials = 6;
    c.oddHarmonicBias = 0.02f;
    c.vibratoRateHz = 7.2f;
    c.vibratoDepthCents = 7.0f;
    c.breathNoiseAmount = 0.055f;
    c.attackShape = 0.06f;
    c.builtInWarmth = 0.02f;
    c.decay1Ratio = 0.55f;
    c.decay2Time = 1.8f;
    c.sustainPlatform = 0.38f;
    c.brightnessCutoffScale = 3.6f;
    c.vibratoDelaySec = 0.0f;
    return c;
}

InstrCharacteristics makeCorAnglais()
{
    // Cor anglais: lower oboe family, richer formants, melancholic quality
    InstrCharacteristics c{};
    c.oscMode = OscMode::Additive;
    c.numPartials = 8;
    c.oddHarmonicBias = 0.56f;
    c.vibratoRateHz = 5.0f;
    c.vibratoDepthCents = 4.2f;
    c.breathNoiseAmount = 0.16f;
    c.attackShape = 0.24f;
    c.builtInWarmth = 0.34f;
    c.decay1Ratio = 0.60f;
    c.decay2Time = 2.6f;
    c.sustainPlatform = 0.50f;
    c.brightnessCutoffScale = 2.4f;
    c.vibratoDelaySec = 0.0f;
    c.hasFormants = true;
    c.formantFreqs[0] = 390.0f;  c.formantQs[0] = 2.4f;  c.formantGains[0] = 0.86f;
    c.formantFreqs[1] = 1040.0f; c.formantQs[1] = 3.5f;  c.formantGains[1] = 0.54f;
    c.formantFreqs[2] = 2300.0f; c.formantQs[2] = 4.6f;  c.formantGains[2] = 0.22f;
    c.formantRegisterScale = 0.78f;
    return c;
}

InstrCharacteristics makeClarinetteBasse()
{
    // Clarinette basse: low register clarinet, rich low harmonics
    InstrCharacteristics c{};
    c.oscMode = OscMode::Additive;
    c.numPartials = 9;
    c.oddHarmonicBias = 0.88f;
    c.vibratoRateHz = 5.4f;
    c.vibratoDepthCents = 8.0f;
    c.breathNoiseAmount = 0.10f;
    c.attackShape = 0.12f;
    c.builtInWarmth = 0.28f;
    c.decay1Ratio = 0.58f;
    c.decay2Time = 3.2f;
    c.sustainPlatform = 0.52f;
    c.brightnessCutoffScale = 2.2f;
    c.vibratoDelaySec = 0.0f;
    c.hasFormants = true;
    c.formantFreqs[0] = 340.0f;  c.formantQs[0] = 2.4f;  c.formantGains[0] = 1.00f;
    c.formantFreqs[1] = 1100.0f; c.formantQs[1] = 3.8f;  c.formantGains[1] = 0.82f;
    c.formantFreqs[2] = 2500.0f; c.formantQs[2] = 4.8f;  c.formantGains[2] = 0.46f;
    c.formantRegisterScale = 0.78f;
    return c;
}

InstrCharacteristics makeBowString(float detuneAmount, int numOscillators,
                                   float vibratoRateHz, float vibratoDepthCents,
                                   float attackShape, float bowNoiseAmount,
                                   float builtInWarmth, bool isEnsemble,
                                   float bodyDelayRatio = 0.0f,
                                   float bodyMaxFeedback = 0.0f,
                                   float bodyDamping = 0.0f,
                                   float decay2Time = 3.4f,
                                   float sustainPlatform = 0.58f)
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Saw;
    c.detuneAmount = detuneAmount;
    c.numOscillators = numOscillators;
    c.vibratoRateHz = vibratoRateHz;
    c.vibratoDepthCents = vibratoDepthCents;
    c.attackShape = attackShape;
    c.decay1Ratio = 0.72f;
    c.decay2Time = decay2Time;
    c.sustainPlatform = sustainPlatform;
    c.bodyDelayRatio = bodyDelayRatio;
    c.bodyMaxFeedback = bodyMaxFeedback;
    c.bodyDamping = bodyDamping;
    c.builtInWarmth = builtInWarmth;
    c.isEnsemble = isEnsemble;
    c.bowNoiseAmount = bowNoiseAmount;
    c.brightnessCutoffScale = 4.0f;
    c.vibratoDelaySec = 0.15f;
    return c;
}

InstrCharacteristics makeWoodwind(int numPartials, float oddBias,
                                  float vibratoRateHz, float vibratoDepthCents,
                                  float breathNoiseAmount, float attackShape,
                                  float builtInWarmth,
                                  float decay2Time = 2.3f,
                                  float sustainPlatform = 0.46f,
                                  float brightCutoff = 3.2f)
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Additive;
    c.numPartials = numPartials;
    c.vibratoRateHz = vibratoRateHz;
    c.vibratoDepthCents = vibratoDepthCents;
    c.oddHarmonicBias = oddBias;
    c.attackShape = attackShape;
    c.decay1Ratio = 0.62f;
    c.decay2Time = decay2Time;
    c.sustainPlatform = sustainPlatform;
    c.builtInWarmth = builtInWarmth;
    c.breathNoiseAmount = breathNoiseAmount;
    c.brightnessCutoffScale = brightCutoff;
    c.vibratoDelaySec = 0.0f;
    return c;
}

InstrCharacteristics makeBrass(int numPartials, float oddBias,
                               float vibratoRateHz, float vibratoDepthCents,
                               float attackShape, float builtInWarmth,
                               float detuneAmount = 0.0f,
                               int numOscillators = 1,
                               float decay2Time = 2.8f,
                               float sustainPlatform = 0.52f,
                               float brightCutoff = 2.8f)
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Additive;
    c.numPartials = numPartials;
    c.detuneAmount = detuneAmount;
    c.numOscillators = numOscillators;
    c.vibratoRateHz = vibratoRateHz;
    c.vibratoDepthCents = vibratoDepthCents;
    c.oddHarmonicBias = oddBias;
    c.attackShape = attackShape;
    c.decay1Ratio = 0.68f;
    c.decay2Time = decay2Time;
    c.sustainPlatform = sustainPlatform;
    c.builtInWarmth = builtInWarmth;
    c.brightnessCutoffScale = brightCutoff;
    c.vibratoDelaySec = 0.0f;
    return c;
}

InstrCharacteristics makeHarp()
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Additive;
    c.numPartials = 14;
    c.inharmonicity = 0.00008f;
    c.decay1Ratio = 0.45f;
    c.decay2Time = 3.2f;
    c.sustainPlatform = 0.15f;
    c.bodyDelayRatio = 0.66f;
    c.bodyMaxFeedback = 0.65f;
    c.bodyDamping = 0.20f;
    c.pluckAmount = 1.00f;
    c.pluckSeconds = 0.028f;
    c.builtInWarmth = 0.05f;
    c.brightnessCutoffScale = 4.0f;
    c.vibratoDelaySec = 0.0f;
    return c;
}

InstrCharacteristics makeSnare()
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Modal;
    c.numPartials = 4;
    c.decay1Ratio = 1.0f;
    c.decay2Time = 2.8f;
    c.sustainPlatform = 1.0f;
    c.pluckAmount = 0.65f;
    c.pluckSeconds = 0.002f;
    c.modalRatios[0] = 1.0f;
    c.modalRatios[1] = 2.35f;
    c.modalRatios[2] = 3.5f;
    c.modalRatios[3] = 5.2f;
    c.modalDecayMults[0] = 0.45f;
    c.modalDecayMults[1] = 0.30f;
    c.modalDecayMults[2] = 0.18f;
    c.modalDecayMults[3] = 0.10f;
    c.modalAmpScales[0] = 1.00f;
    c.modalAmpScales[1] = 0.55f;
    c.modalAmpScales[2] = 0.28f;
    c.modalAmpScales[3] = 0.14f;
    c.brightnessCutoffScale = 5.0f;
    c.vibratoRateHz = 0.0f;
    c.vibratoDelaySec = 0.0f;
    return c;
}

InstrCharacteristics makeXylophone()
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Modal;
    c.numPartials = 4;
    c.decay1Ratio = 1.0f;
    c.decay2Time = 6.0f;
    c.sustainPlatform = 1.0f;
    c.pluckAmount = 0.55f;
    c.pluckSeconds = 0.001f;
    c.modalRatios[0] = 1.0f;
    c.modalRatios[1] = 3.0f;
    c.modalRatios[2] = 8.0f;
    c.modalRatios[3] = 15.0f;
    c.modalDecayMults[0] = 1.0f;
    c.modalDecayMults[1] = 0.65f;
    c.modalDecayMults[2] = 0.38f;
    c.modalDecayMults[3] = 0.20f;
    c.modalAmpScales[0] = 1.00f;
    c.modalAmpScales[1] = 0.50f;
    c.modalAmpScales[2] = 0.22f;
    c.modalAmpScales[3] = 0.10f;
    c.brightnessCutoffScale = 5.5f;
    c.vibratoRateHz = 0.0f;
    c.vibratoDelaySec = 0.0f;
    return c;
}

InstrCharacteristics makeTimpani()
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Modal;
    c.numPartials = 4;
    c.decay1Ratio = 1.0f;
    c.decay2Time = 8.0f;
    c.sustainPlatform = 1.0f;
    c.pluckAmount = 0.42f;
    c.pluckSeconds = 0.005f;
    c.modalRatios[0] = 1.0f;
    c.modalRatios[1] = 1.50f;
    c.modalRatios[2] = 1.99f;
    c.modalRatios[3] = 2.44f;
    c.modalDecayMults[0] = 0.72f;
    c.modalDecayMults[1] = 0.58f;
    c.modalDecayMults[2] = 0.38f;
    c.modalDecayMults[3] = 0.24f;
    c.modalAmpScales[0] = 1.00f;
    c.modalAmpScales[1] = 0.70f;
    c.modalAmpScales[2] = 0.34f;
    c.modalAmpScales[3] = 0.20f;
    c.brightnessCutoffScale = 4.0f;
    c.vibratoRateHz = 3.2f;
    c.vibratoDelaySec = 0.0f;
    return c;
}

InstrCharacteristics makeCelesta()
{
    InstrCharacteristics c{};
    c.oscMode = OscMode::Modal;
    c.numPartials = 4;
    c.decay1Ratio = 1.0f;
    c.decay2Time = 14.0f;
    c.sustainPlatform = 1.0f;
    c.pluckAmount = 0.32f;
    c.pluckSeconds = 0.003f;
    c.modalRatios[0] = 1.0f;
    c.modalRatios[1] = 3.0f;   // FIX: was 4.0 (perfect square) — odd harmonics for bell-like tone
    c.modalRatios[2] = 5.0f;   // FIX: was 9.0 (perfect square)
    c.modalRatios[3] = 7.0f;  // FIX: was 16.0 (perfect square)
    c.modalDecayMults[0] = 1.20f;
    c.modalDecayMults[1] = 0.78f;
    c.modalDecayMults[2] = 0.42f;
    c.modalDecayMults[3] = 0.22f;
    c.modalAmpScales[0] = 1.00f;
    c.modalAmpScales[1] = 0.54f;
    c.modalAmpScales[2] = 0.26f;
    c.modalAmpScales[3] = 0.12f;
    c.brightnessCutoffScale = 4.0f;
    c.vibratoRateHz = 4.1f;
    c.vibratoDelaySec = 0.0f;
    c.vibratoDepthCents = 8.0f;  // FIX: Celesta vibrato depth (was missing, defaulting to 0)
    return c;
}

constexpr std::array<const char*, kNumInstruments> kNames = {
    "Violon", "Alto", "Violoncelle", "Contrebasse", "Harpe",
    "Fl\xC3\xBBte", "Hautbois", "Clarinette", "Basson", "Piccolo", "Cor anglais", "Clarinette basse",
    "Cor fran\xC3\xA7" "ais", "Trompette", "Trombone", "Tuba",
    "Timbales", "C\xC3\xA9lesta", "Snare", "Xylophone"
};

constexpr std::array<const char*, kNumInstruments> kShortNames = {
    "VLON", "ALTO", "VCEL", "CBAS", "HARP",
    "FLUT", "HBOI", "CLAR", "BSN", "PICCO", "CORANG", "CLBAS",
    "COR", "TRPT", "TRMB", "TUBA",
    "TIMP", "CELE", "SNAR", "XYLO"
};

constexpr std::array<const char*, kNumFamilies> kFamilyNames = {
    "CORDES", "BOIS", "CUIVRES", "PERCUSSION"
};

constexpr std::array<MidiNoteRange, kNumInstruments> kMidiRanges = {{
    { 55, 105 }, // Violon
    { 48, 88  }, // Alto
    { 36, 79  }, // Violoncelle
    { 28, 67  }, // Contrebasse
    { 24, 103 }, // Harpe
    { 60, 98  }, // Flute
    { 58, 93  }, // Hautbois
    { 50, 94  }, // Clarinette
    { 34, 76  }, // Basson
    { 72, 108 }, // Piccolo
    { 46, 80  }, // Cor anglais
    { 34, 78  }, // Clarinette basse
    { 42, 84  }, // Cor
    { 54, 86  }, // Trompette
    { 40, 77  }, // Trombone
    { 26, 65  }, // Tuba
    { 38, 57  }, // Timbales
    { 60, 108 }, // Celesta
    { 48, 72  }, // Snare
    { 60, 96  }  // Xylophone
}};

const std::array<InstrCharacteristics, kNumInstruments> kChars = {{
    makeBowString(0.0018f, 2, 6.1f, 10.0f, 0.34f, 0.09f, 0.08f, false, 0.10f, 0.08f, 0.18f, 2.6f, 0.64f),
    makeBowString(0.0010f, 1, 5.0f, 11.0f, 0.52f, 0.15f, 0.18f, false, 0.24f, 0.18f, 0.22f, 4.2f, 0.60f),
    makeBowString(0.0014f, 2, 4.7f, 7.5f, 0.62f, 0.18f, 0.34f, false, 0.62f, 0.46f, 0.16f, 5.6f, 0.58f),
    makeBowString(0.0007f, 1, 4.0f, 3.6f, 0.80f, 0.09f, 0.52f, false, 0.32f, 0.46f, 0.28f, 7.0f, 0.50f),
    makeHarp(),
    withFormantRegisterScale(withFormants(makeWoodwind(8, 0.03f, 5.8f, 3.4f, 0.18f, 0.12f, 0.04f, 2.4f, 0.40f, 4.4f),
                                          980.0f, 2.8f, 0.54f,
                                          2560.0f, 4.0f, 0.42f,
                                          6200.0f, 5.8f, 0.18f), 0.95f),
    withFormantRegisterScale(withFormants(makeWoodwind(8, 0.48f, 5.0f, 4.0f, 0.13f, 0.30f, 0.28f, 3.2f, 0.54f, 2.3f),
                                          880.0f, 3.2f, 0.74f,
                                          1700.0f, 3.8f, 0.46f,
                                          2850.0f, 4.6f, 0.18f), 0.78f),
    withFormantRegisterScale(withFormants(makeWoodwind(7, 0.86f, 4.8f, 3.2f, 0.07f, 0.22f, 0.14f, 3.5f, 0.56f, 2.6f),
                                          540.0f, 2.6f, 1.00f,
                                          1260.0f, 4.2f, 0.78f,
                                          2360.0f, 5.4f, 0.36f), 0.88f),
    withFormantRegisterScale(withFormants(makeWoodwind(8, 0.70f, 4.4f, 2.8f, 0.06f, 0.36f, 0.34f, 4.2f, 0.60f, 2.2f),
                                          280.0f, 2.2f, 1.00f,
                                          680.0f, 3.0f, 0.80f,
                                          1380.0f, 4.2f, 0.36f), 0.85f),
    makePiccolo(),
    makeCorAnglais(),
    makeClarinetteBasse(),
    withFormantRegisterScale(withFormants(makeBrass(9, 0.32f, 4.5f, 6.4f, 0.34f, 0.30f, 4.0f, 2, 4.0f, 0.60f),
                                          360.0f, 2.0f, 1.00f,
                                          820.0f, 2.6f, 0.78f,
                                          1620.0f, 3.2f, 0.36f), 0.92f),
    withFormantRegisterScale(withFormants(makeBrass(10, 0.14f, 5.4f, 5.8f, 0.12f, 0.10f, 3.0f, 1, 2.0f, 0.46f, 3.8f),
                                          980.0f, 2.2f, 1.00f,
                                          1980.0f, 3.0f, 0.90f,
                                          3520.0f, 4.2f, 0.48f), 1.00f),
    withFormantRegisterScale(withFormants(makeBrass(8, 0.46f, 4.3f, 4.0f, 0.26f, 0.26f, 2.2f, 1, 3.4f, 0.58f),
                                          300.0f, 2.0f, 1.00f,
                                          720.0f, 2.8f, 0.80f,
                                          1520.0f, 3.8f, 0.36f), 0.90f),
    withFormantRegisterScale(withFormants(makeBrass(7, 0.60f, 3.9f, 2.4f, 0.38f, 0.42f, 1.8f, 1, 4.8f, 0.62f),
                                          150.0f, 1.7f, 1.00f,
                                          360.0f, 2.2f, 0.85f,
                                          860.0f, 2.8f, 0.36f), 0.82f),
    makeTimpani(),
    makeCelesta(),
    makeSnare(),
    makeXylophone(),
}};

constexpr std::array<InstrSettings, kNumInstruments> kDefaults = {{
    { 0.81f, 0.0f, 0.54f, 0.075f, 3.20f, 0.65f, 0.44f, 0.42f, 0.32f, 0.03f, 0.10f, 0.55f, 6200.0f, -0.08f },
    { 0.79f, 0.0f, 0.42f, 0.10f, 3.80f, 0.64f, 0.50f, 0.34f, 0.42f, 0.02f, 0.08f, 0.54f, 4600.0f, -0.02f },
    { 0.83f, 0.0f, 0.34f, 0.12f, 4.50f, 0.60f, 0.58f, 0.28f, 0.52f, 0.015f, 0.07f, 0.56f, 3600.0f, 0.06f },
    { 0.86f, 0.0f, 0.22f, 0.18f, 5.60f, 0.54f, 0.74f, 0.10f, 0.64f, 0.005f, 0.05f, 0.56f, 2200.0f, 0.10f },
    { 0.78f, 0.0f, 0.64f, 0.001f, 5.30f, 0.12f, 0.58f, 0.00f, 0.24f, 0.00f, 0.14f, 0.52f, 7800.0f, 0.00f },
    { 0.73f, 0.0f, 0.62f, 0.035f, 2.50f, 0.48f, 0.26f, 0.14f, 0.14f, 0.005f, 0.07f, 0.54f, 7800.0f, -0.08f },
    { 0.74f, 0.0f, 0.38f, 0.070f, 2.45f, 0.52f, 0.34f, 0.18f, 0.30f, 0.005f, 0.05f, 0.58f, 4200.0f, -0.02f },
    { 0.75f, 0.0f, 0.38f, 0.060f, 3.20f, 0.58f, 0.38f, 0.12f, 0.34f, 0.005f, 0.06f, 0.56f, 4200.0f, 0.02f },
    { 0.80f, 0.0f, 0.30f, 0.080f, 3.80f, 0.56f, 0.46f, 0.10f, 0.48f, 0.005f, 0.05f, 0.58f, 3200.0f, 0.06f },
    { 0.74f, 0.0f, 0.68f, 0.025f, 1.80f, 0.38f, 0.20f, 0.12f, 0.10f, 0.005f, 0.08f, 0.52f, 9800.0f, 0.00f },
    { 0.75f, 0.0f, 0.42f, 0.090f, 2.80f, 0.52f, 0.34f, 0.16f, 0.34f, 0.005f, 0.06f, 0.56f, 4800.0f, -0.04f },
    { 0.78f, 0.0f, 0.32f, 0.110f, 3.40f, 0.54f, 0.42f, 0.10f, 0.40f, 0.005f, 0.05f, 0.58f, 3600.0f, 0.04f },
    { 0.81f, 0.0f, 0.36f, 0.090f, 3.80f, 0.60f, 0.50f, 0.18f, 0.44f, 0.02f, 0.08f, 0.54f, 4200.0f, -0.04f },
    { 0.82f, 0.0f, 0.58f, 0.040f, 2.30f, 0.52f, 0.24f, 0.10f, 0.18f, 0.01f, 0.06f, 0.56f, 7200.0f, 0.02f },
    { 0.83f, 0.0f, 0.42f, 0.060f, 3.00f, 0.56f, 0.38f, 0.10f, 0.34f, 0.005f, 0.06f, 0.54f, 4800.0f, 0.06f },
    { 0.88f, 0.0f, 0.24f, 0.100f, 4.90f, 0.58f, 0.52f, 0.04f, 0.56f, 0.00f, 0.04f, 0.56f, 2600.0f, 0.08f },
    { 0.86f, 0.0f, 0.44f, 0.001f, 3.60f, 1.00f, 0.48f, 0.00f, 0.16f, 0.00f, 0.08f, 0.56f, 4200.0f, 0.00f },
    { 0.75f, 0.0f, 0.68f, 0.001f, 3.40f, 1.00f, 0.48f, 0.00f, 0.08f, 0.00f, 0.12f, 0.52f, 9000.0f, 0.00f },
    { 0.74f, 0.0f, 0.58f, 0.001f, 2.80f, 1.00f, 0.36f, 0.00f, 0.02f, 0.00f, 0.08f, 0.52f, 6500.0f, 0.00f },
    { 0.76f, 0.0f, 0.72f, 0.001f, 5.20f, 1.00f, 0.34f, 0.00f, 0.02f, 0.00f, 0.10f, 0.54f, 10000.0f, 0.00f },
}};

constexpr std::array<const char*, kNumInstruments> kDescriptions = {{
    "Le violon solo vise une ligne de tete brillante et lisible, avec une attaque d'archet synthetique et un vibrato expressif mais controle.",
    "L'alto solo couvre le registre median avec une couleur chaleureuse et boisee, utile pour les contrechants et les harmonies internes.",
    "Le violoncelle privilegie la profondeur, le chant lyrique et la tenue harmonique, davantage comme voix orchestralo-synthetique que comme legato detaille.",
    "La contrebasse fournit l'assise harmonique avec une attaque plus lente et une resonance grave pensee pour le poids d'ensemble.",
    "La harpe apporte des attaques pincees, des arpeges lumineux et une resonance ample, utile pour les transitions et les textures.",
    "La flute vise une ligne claire et aerienne, avec un souffle discret et une brillance adaptee aux doublures et aux tetes de phrase.",
    "Le hautbois propose un timbre nasal et chantant, pense pour ressortir dans un arrangement sans promettre une imitation acoustique exhaustive.",
    "La clarinette recherche une couleur ronde et stable, avec des harmoniques impaires marquees pour les lignes souples et les doublures.",
    "Le basson ajoute une assise boisee dans le grave et le medium grave, avec un grain rugueux adapte au soutien harmonique.",
    "Le piccolo delivre une brillance extreme dans le registre aigue, avec un timbre aigu et penetrant adapte aux lignes de tete et aux accents.",
    "Le cor anglais offre une couleur chaleureuse et melancholique, plus grave que le hautbois avec des formants bas et une richesse expressive.",
    "La clarinette basse couvre le registre grave avec une chaleur boisee et des harmoniques impaires marquees, soutien du grave orchestral.",
    "Le cor francais relie cordes et cuivres avec une chaleur ample et une couleur volontiers cinematographique.",
    "La trompette se distingue par une attaque franche, une projection claire et une brillance pensee pour traverser l'arrangement.",
    "Le trombone fournit un medium grave puissant, moins incisif que la trompette mais plus massif, utile pour les accents et les appuis.",
    "Le tuba ancre la section de cuivres dans le grave avec une masse harmonique dense et une resonance ample.",
    "Les timbales utilisent une synthese modale pour recreer un impact tonal et accordable, oriente rythme, appui et crescendo court.",
    "La celesta combine attaque percussive et cloche lumineuse pour les passages feeriques, les ostinatos rapides et les doublures hautes.",
    "La snare offre un impact sec et tunable avec des harmoniques aigues, adaptee aux frappes et aux accents rytmiques.",
    "Le xylophone delivre une brillanteur cristalline avec des harmoniques en octave, idal pour les ostinatos et les solos.",
}};

} // namespace

Family getFamily(const int instrIndex)
{
    const auto idx = std::clamp(instrIndex, 0, kNumInstruments - 1);
    for (int f = kNumFamilies - 1; f > 0; --f)
        if (idx >= kFamilyStart[f]) return static_cast<Family>(f);
    return Family::Cordes;
}

int getFamilyStartIndex(const Family family)
{
    return kFamilyStart[std::clamp(static_cast<int>(family), 0, kNumFamilies - 1)];
}

const char* getFamilyName(const int familyIndex)
{
    return kFamilyNames[static_cast<std::size_t>(std::clamp(familyIndex, 0, kNumFamilies - 1))];
}

const char* getInstrName(const int instrIndex)
{
    return kNames[static_cast<std::size_t>(std::clamp(instrIndex, 0, kNumInstruments - 1))];
}

const char* getInstrShortName(const int instrIndex)
{
    return kShortNames[static_cast<std::size_t>(std::clamp(instrIndex, 0, kNumInstruments - 1))];
}

MidiNoteRange getInstrMidiNoteRange(const int instrIndex)
{
    return kMidiRanges[static_cast<std::size_t>(std::clamp(instrIndex, 0, kNumInstruments - 1))];
}

const InstrCharacteristics& getCharacteristics(const int instrIndex)
{
    return kChars[static_cast<std::size_t>(std::clamp(instrIndex, 0, kNumInstruments - 1))];
}

InstrSettings getDefaultSettings(const int instrIndex)
{
    return kDefaults[static_cast<std::size_t>(std::clamp(instrIndex, 0, kNumInstruments - 1))];
}

const char* getInstrDescription(const int instrIndex)
{
    return kDescriptions[static_cast<std::size_t>(std::clamp(instrIndex, 0, kNumInstruments - 1))];
}

// =========================================================================
// FX availability per instrument
// =========================================================================
static constexpr FxAvailability kFxAvailability[kNumInstruments] =
{
    //                       Sat    Trans  EQ     Comp   Chor   Delay  Rev    Lim
    /* 0  Violon       */ { false, false, true,  true,  true,  true,  true,  true  },
    /* 1  Alto         */ { false, false, true,  true,  true,  true,  true,  true  },
    /* 2  Violoncelle  */ { false, false, true,  true,  true,  true,  true,  true  },
    /* 3  Contrebasse  */ { false, false, true,  true,  true,  true,  true,  true  },
    /* 4  Harpe        */ { false, true,  true,  true,  true,  true,  true,  true  },
    /* 5  Flûte        */ { false, false, true,  true,  false, true,  true,  true  },
    /* 6  Hautbois     */ { false, false, true,  true,  false, true,  true,  true  },
    /* 7  Clarinette   */ { false, false, true,  true,  false, true,  true,  true  },
    /* 8  Basson       */ { false, false, true,  true,  false, true,  true,  true  },
    /* 9  Piccolo      */ { false, false, true,  true,  false, true,  true,  true  },
    /* 10 Cor anglais  */ { false, false, true,  true,  false, true,  true,  true  },
    /* 11 Clar. basse  */ { false, false, true,  true,  false, true,  true,  true  },
    /* 12 Cor          */ { false, false, true,  true,  true,  true,  true,  true  },
    /* 13 Trompette    */ { true,  true,  true,  true,  true,  true,  true,  true  },
    /* 14 Trombone     */ { true,  false, true,  true,  true,  true,  true,  true  },
    /* 15 Tuba         */ { false, false, true,  true,  true,  true,  true,  true  },
    /* 16 Timbales     */ { false, true,  true,  true,  false, false, true,  true  },
    /* 17 Célesta      */ { false, false, true,  false, true,  true,  true,  true  },
    /* 18 Snare        */ { false, true,  true,  true,  false, false, true,  true  },
    /* 19 Xylophone    */ { false, true,  true,  true,  false, true,  true,  true  },
};

const FxAvailability& getFxAvailability(const int instrIndex)
{
    return kFxAvailability[static_cast<std::size_t>(std::clamp(instrIndex, 0, kNumInstruments - 1))];
}

bool isFxAvailable(const int instrIndex, const GlobalFxSlot slot)
{
    const auto& a = getFxAvailability(instrIndex);
    switch (slot)
    {
        case GlobalFxSlot::Saturator:  return a.saturator;
        case GlobalFxSlot::Transient:  return a.transient;
        case GlobalFxSlot::Eq:         return a.eq;
        case GlobalFxSlot::Compressor: return a.compressor;
        case GlobalFxSlot::Chorus:     return a.chorus;
        case GlobalFxSlot::Delay:      return a.delay;
        case GlobalFxSlot::Reverb:     return a.reverb;
        case GlobalFxSlot::Limiter:    return a.limiter;
        default:                       return false;
    }
}

} // namespace mos
