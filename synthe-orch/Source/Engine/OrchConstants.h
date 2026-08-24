#pragma once

// =============================================================================
// OrchConstants.h — Named constants for the Orch synth DSP engine.
//
// Replaces hardcoded magic numbers throughout OrchVoice.cpp with
// documented, self-describing constants.
// =============================================================================

namespace mos::constants
{

// ---- Brightness envelope ----
constexpr float kBrightnessBaseScale    = 0.5f;    // base brightness multiplier
constexpr float kBrightnessRangeScale   = 1.5f;    // brightness range per setting
constexpr float kBrightnessVelGain      = 0.25f;   // velocity boost on brightness

// Brightness decay: brass vs woodwind/strings
constexpr float kBrightDecayBrass_pp    = 0.07f;   // 70ms pianissimo
constexpr float kBrightDecayBrass_ff    = 0.11f;   // +110ms at fortissimo
constexpr float kBrightDecayWood_pp     = 0.12f;   // 120ms pianissimo
constexpr float kBrightDecayWood_ff     = 0.18f;   // +180ms at fortissimo

// ---- Partial decay shaping ----
constexpr float kPartialDecayBrightScale = 0.5f;   // brightness dampens partial decay
constexpr float kPartialDecayHarmonicRate = 0.3f;   // per-harmonic decay roll-off

// ---- Register-dependent decay ----
constexpr int   kRegisterRefNote        = 60;       // C4 = 1.0x decay
constexpr float kRegisterDecaySlope     = 240.0f;   // divisor for note-based decay
constexpr float kRegisterDecayFloor     = 0.80f;    // minimum register scale

// ---- SVF filter ----
constexpr float kSVFStabilityMargin     = 0.95f;    // Jury stability safety factor
constexpr float kMinFilterQ             = 0.5f;     // minimum allowed Q

// ---- DC blocker ----
constexpr float kDcBlockR               = 0.9995f;  // ~5 Hz HPF @ 44.1 kHz

// ---- Denormal flushing ----
constexpr float kDenormalFloor          = 1e-15f;

// ---- Pitch transients ----
// Bowed string scoop
constexpr float kBowScoopBaseCents      = 15.0f;    // base overshoot in cents
constexpr float kBowScoopVelCents       = 25.0f;    // velocity-added cents
constexpr float kBowScoopBaseTimeSec    = 0.060f;   // base settle time
constexpr float kBowScoopVelTimeSec     = 0.040f;   // velocity shortens settle

// Brass lip snap
constexpr float kTrumpetSnapBaseCents   = 10.0f;
constexpr float kTrumpetSnapVelCents    = 8.0f;
constexpr float kTrumpetSnapTimeSec     = 0.028f;

// Horn / trombone onset
constexpr float kHornOnsetBaseCents     = 6.0f;
constexpr float kHornOnsetVelCents      = 5.0f;
constexpr float kHornOnsetTimeSec       = 0.040f;

// Tuba onset
constexpr float kTubaOnsetBaseCents     = 3.0f;
constexpr float kTubaOnsetVelCents      = 3.0f;
constexpr float kTubaOnsetTimeSec       = 0.055f;

// ---- Vibrato ----
constexpr float kVibratoJitterPercent   = 0.04f;    // +/-2% rate jitter
constexpr float kVibratoSlowDriftHz     = 0.11f;    // slow human rate wandering
constexpr float kVibratoSlowDriftDepth  = 0.075f;   // +/-7.5% rate drift
constexpr float kVibratoReleaseSec      = 0.24f;    // smooth note-off vibrato fade
constexpr float kVibratoVelocityDepthMin = 0.82f;   // soft notes keep subtler vibrato
constexpr float kVibratoVelocityDepthRange = 0.18f; // hard notes can open vibrato depth

// ---- Bow noise transient ----
constexpr float kBowNoiseVelScale       = 0.8f;     // velocity-to-noise
constexpr float kBowNoiseDecaySec       = 0.030f;   // ~30ms filtered burst
constexpr float kBowNoiseHPAlpha        = 0.15f;    // HP filter coefficient

// ---- Breath noise transient ----
constexpr float kBreathNoiseVelBase     = 0.5f;     // base breath level
constexpr float kBreathNoiseVelRange    = 0.5f;     // velocity-added
constexpr float kBreathNoiseDecaySec    = 0.150f;   // ~150ms LP-filtered burst
constexpr float kBreathReleaseTailSec   = 0.095f;   // short air continuation after note-off
constexpr float kBrassReleaseTailSec    = 0.070f;   // short lip/air continuation after note-off
constexpr float kBreathNoiseLPAlpha     = 0.35f;    // LP filter coefficient
constexpr float kAirColumnNoiseScale    = 0.10f;    // sustained air noise level
constexpr float kAirEffortBase          = 0.7f;     // sustained effort floor
constexpr float kAirEffortRange         = 0.3f;     // velocity-added effort
constexpr float kReedBuzzScale          = 0.40f;    // reed buzz multiplier

// ---- Pluck transient ----
constexpr float kPluckBrightBase        = 0.5f;     // brightness floor for pluck
constexpr float kPluckBrightRange       = 0.5f;     // brightness range for pluck

// ---- Body resonator modes ----
constexpr float kBodyDampingAlpha       = 0.7f;     // body comb damping coefficient
constexpr float kBodyDcMix              = 0.5f;     // body DC-blocked mix level
constexpr float kBodyMode2Ratio         = 0.6f;     // Helmholtz-like lower mode
constexpr float kBodyMode3Ratio         = 2.2f;     // Higher plate resonance
constexpr float kBodyMode2WarmthScale   = 0.10f;    // mode 2 warmth gain
constexpr float kBodyMode3WarmthScale   = 0.05f;    // mode 3 warmth gain

// ---- Ensemble / chorus ----
constexpr float kEnsembleChorusRateHz   = 0.45f;    // slow, non-flangey ensemble movement
constexpr float kEnsembleChorusDepth    = 0.012f;   // subtle pitch modulation

// ---- Modal voice ----
constexpr float kModalMotorSpeedHz      = 3.5f;     // vibraphone motor speed
constexpr float kModalAMDepthScale      = 0.28f;    // AM depth from vibrato knob

// ---- Auto-pan ----
constexpr float kRegisterPanScale       = 0.08f;    // register-to-pan amount
constexpr float kRegisterPanSpan        = 88.0f;    // note range divisor
constexpr float kSqrtHalf              = 0.7071f;   // sqrt(0.5) for equal-power pan

// ---- Oscillator spread ----
constexpr float kDetuneBaseAmount       = 0.3f;     // base detune fraction
constexpr float kDetuneSettingRange     = 0.7f;     // detune from setting
constexpr float kUnisonSpread4          = 0.67f;    // 4-osc spread compression

// ---- Attack shaping ----
constexpr float kAttackShapeScale       = 2.0f;     // slow bow elongation factor

// ---- Max voice age ----
constexpr float kMaxVoiceDecayMult      = 6.0f;     // decay contribution to max age
constexpr float kMaxVoiceReleaseMult    = 3.0f;     // release contribution to max age
constexpr float kMaxVoiceAgeSec         = 60.0f;    // absolute ceiling

// ---- Vibrato ramp ----
constexpr float kVibratoRampTimeSec     = 0.40f;    // ramp-in time after delay

// ---- Envelope thresholds ----
constexpr float kEnvDeathThreshold      = 0.0001f;  // below this = silent
constexpr float kDecay1Threshold        = 0.002f;   // threshold to transition D1→S/D2

// ---- Sustained string body bloom ----
constexpr float kStringBloomAmount      = 0.06f;    // bloom darkening amount

// ---- Sustained scrape noise ----
constexpr float kBowScrapeScale         = 0.12f;    // bow scrape noise level

// ---- Brass bloom ----
constexpr float kBrassBloomThreshold    = 0.4f;     // envelope level for bloom onset
constexpr float kBrassBloomScale        = 0.18f;    // bloom projection boost
constexpr float kBrassSustainAirBase    = 0.010f;   // subtle continuous lip/air texture
constexpr float kBrassSustainAirVel     = 0.010f;   // velocity-added lip/air texture

// ---- Character processing ----
constexpr float kCharacterScale         = 0.10f;    // character harmonic saturation
constexpr float kCharacterDrive         = 2.0f;     // character saturation drive

// ---- Percussion edge ----
constexpr float kPercEdgeAmount         = 0.05f;    // stick edge definition

// ---- Sample rate safety ----
constexpr double kMinSampleRate         = 1.0;      // floor to avoid division by zero
constexpr double kMaxSampleRate         = 192000.0;

} // namespace mos::constants
