#pragma once

#include <JuceHeader.h>
#include <memory>
#include <cstdint>
#include <cstring>
#include "InstrumentDefs.h"
#include "RareInstrumentModel.h"
#include "SinTable.h"

namespace mis
{

// =========================================================================
// Abstract interface — all instrument voices conform to this
// =========================================================================
class InstrumentVoice
{
public:
    virtual ~InstrumentVoice() = default;

    virtual void noteOn(const InstrumentSettings& settings,
                        int midiNote, float velocity, double sampleRate) = 0;
    virtual void noteOff() = 0;
    virtual void render(juce::AudioBuffer<float>& buffer,
                        int startSample, int numSamples) = 0;

    virtual bool isActive()    const noexcept = 0;
    virtual bool isReleasing() const noexcept = 0;
    virtual int  getMidiNote() const noexcept = 0;
    virtual void setPitchBendFactor(float f) noexcept = 0;
    virtual const InstrumentCharacteristics& getCharacteristics() const noexcept = 0;
    virtual void forceQuickRelease() noexcept = 0;
};

// =========================================================================
// Shared base — common ADSR, pitch, pan, SVF filter, brightness envelope
// =========================================================================
class InstrumentVoiceBase : public InstrumentVoice
{
public:
    InstrumentVoiceBase();
    bool isActive()    const noexcept override { return envState != EnvState::Off; }
    bool isReleasing() const noexcept override { return envState == EnvState::Release; }
    int  getMidiNote() const noexcept override { return midiNote; }
    void setPitchBendFactor(float f) noexcept override { pitchBendFactor = f; }
    void forceQuickRelease() noexcept override;

protected:
    static constexpr float kDcBlockFrequencyHz = 5.0f;
    static constexpr float kDenormalFloor = 1.0e-15f;
    static constexpr float kFilterSmoothingTimeMs = 1.5f;

    enum class EnvState { Off, Attack, Decay, Sustain, Release };

    void  beginNote(const InstrumentSettings& s, int note, float velocity, double sampleRate);
    float advanceEnvelope() noexcept;
    float advanceBrightness() noexcept;
    float applyFilter(float signal) noexcept;
    void  updateFilterCoefficient(float cutoffHz) noexcept;
    float applyDcBlocker(float signal) noexcept;
    float applyOutputDcBlocker(float signal) noexcept;
    float applyVoiceDrive(float signal) noexcept;
    float applyRareDedicatedSignature(float signal, float env, float phaseInc, float noiseRaw) noexcept;
    bool  usesRareDedicatedAlgorithm() const noexcept;
    void  writeOutput(float* left, float* right, int idx, float signal) const noexcept;
    virtual int getInstrumentIndex() const noexcept = 0;

    static float getWaveform(float phase01, float morph, float phaseInc);
    static float polyBlep(float t, float dt);
    static float readComb(const float* buf, int bufSize, int writePos, float delaySamples);

    InstrumentSettings settings{};
    InstrumentCharacteristics chars{};
    double sr       = 44100.0;
    float  vel      = 0.0f;
    int    midiNote = -1;
    int    instrumentIndex = 0;
    RareInstrumentAlgorithm rareAlgorithm = RareInstrumentAlgorithm::NyckelharpaKeyedBowSympathetic;
    bool   rareDedicatedActive = false;

    EnvState envState = EnvState::Off;

    float oscPhase  = 0.0f;
    float oscFreqHz = 440.0f;

    // ADSR
    float envLevel     = 0.0f;
    float attackRate   = 0.0f;
    float decayCoeff   = 1.0f;
    float releaseCoeff = 1.0f;

    // SVF filter
    float svfLow = 0.0f, svfBand = 0.0f;
    float filterF = 0.0f, filterQinv = 0.0f;
    float filterTargetF = 0.0f;
    float filterCoeffSmoothing = 1.0f;

    // DC blocker state
    float dcX1 = 0.0f, dcY1 = 0.0f;
    float outputDcX1 = 0.0f, outputDcY1 = 0.0f;
    float dcBlockerCoeff = 0.9995f;

    // ADAA tanh state for per-voice drive.
    float drivePreviousInput = 0.0f;
    bool  driveHasPreviousInput = false;

    // Pan
    float panL = 0.7071f, panR = 0.7071f;

    // Noise
    float nextRandomFloat() noexcept;
    float nextRandomBipolar() noexcept;
    void seedRandom(std::uint64_t seed) noexcept;

    std::uint64_t rngState = 0x9e3779b97f4a7c15ull;
    float noiseLpState = 0.0f;
    std::uint32_t voiceInstanceSerial = 0;
    std::uint32_t noteTriggerCount = 0;

    float pitchBendFactor = 1.0f;

    // Brightness envelope
    float brightCutoffTarget  = 8000.0f;
    float brightCutoffCurrent = 8000.0f;
    float brightDecayCoeff    = 1.0f;

    // Non-serialized V2 instrument model signature state.
    float rarePhaseA = 0.0f;
    float rarePhaseB = 0.0f;
    float rarePhaseC = 0.0f;
    float rareEnvA = 0.0f;
    float rareEnvB = 0.0f;
    float rareStateA = 0.0f;
    float rareStateB = 0.0f;
    float rareStateC = 0.0f;
    float rareHold = 0.0f;
    float rareSignatureGain = 0.0f;

    // Vibrato (active for Bowed / Blown only)
    float vibratoPhase        = 0.0f;
    float vibratoRateHz       = 5.5f;
    float vibratoDepth        = 0.0f;
    int   vibratoOnsetSamples = 0;
    int   vibratoAge          = 0;

    int ageSamples    = 0;
    int maxAgeSamples = 0;
};

// =========================================================================
// BowedStringVoiceBase — continuous friction excitation + multi-partial
// Instruments: Nyckelharpa, YayliTanbur, Crwth, Yaybahar
// =========================================================================
class BowedStringVoiceBase : public InstrumentVoiceBase
{
public:
    void noteOn(const InstrumentSettings& s, int note, float velocity, double sampleRate) override;
    void noteOff() override;
    void render(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

protected:
    // Body comb (primary)
    static constexpr int kBodyBufSize = 8192;
    float bodyBuf[kBodyBufSize] = {};
    int   bodyWritePos     = 0;
    float bodyDelaySamples = 100.0f;
    float bodyFeedback     = 0.0f;
    float bodyDampState    = 0.0f;

    // Body comb (secondary — for instruments with dual resonator: Crwth, Nyckelharpa)
    static constexpr int kBody2BufSize = 8192;
    float body2Buf[kBody2BufSize] = {};
    int   body2WritePos     = 0;
    float body2DelaySamples = 100.0f;
    float body2Feedback     = 0.0f;
    float body2DampState    = 0.0f;

    // Sympathetic combs (up to 4)
    static constexpr int kSympBufSize = 4096;
    struct SympComb {
        float buf[kSympBufSize] = {};
        int   writePos     = 0;
        float delaySamples = 50.0f;
        float feedback     = 0.0f;
    };
    SympComb sympCombs[kMaxSympathetic];
    int numSympCombs = 0;

    // Friction state
    float frictionLpState = 0.0f;
};

// =========================================================================
// PluckedStringVoiceBase — Karplus-Strong impulse + delay line
// Instruments: Gayageum, ChapmanStick, Mbira
// =========================================================================
class PluckedStringVoiceBase : public InstrumentVoiceBase
{
public:
    void noteOn(const InstrumentSettings& s, int note, float velocity, double sampleRate) override;
    void noteOff() override;
    void render(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

protected:
    // Karplus-Strong delay line
    static constexpr int kKsBufSize = 8192;
    float ksBuf[kKsBufSize] = {};
    int   ksWritePos     = 0;
    float ksDelaySamples = 100.0f;
    float ksDampState    = 0.0f;

    // Body comb
    static constexpr int kBodyBufSize = 8192;
    float bodyBuf[kBodyBufSize] = {};
    int   bodyWritePos     = 0;
    float bodyDelaySamples = 100.0f;
    float bodyFeedback     = 0.0f;
    float bodyDampState    = 0.0f;

    // Sympathetic comb (single)
    static constexpr int kSympBufSize = 4096;
    float sympBuf[kSympBufSize] = {};
    int   sympWritePos     = 0;
    float sympDelaySamples = 50.0f;
    float sympFeedback     = 0.0f;

    // Exciter state
    float exciterEnvLevel = 0.0f;
    float exciterDecay    = 0.0f;

    // Continuous glass-bow excitation for Cristal Baschet-like voices.
    float continuousExcitePhase = 0.0f;
    float continuousExciteLevel = 0.0f;
    float continuousExciteDecay = 1.0f;
};

// =========================================================================
// WindVoiceBase — breath/reed excitation + bore resonator
// Instruments: Carnyx, Aulos, Fujara, Gemshorn, Dizi
// =========================================================================
class WindVoiceBase : public InstrumentVoiceBase
{
public:
    void noteOn(const InstrumentSettings& s, int note, float velocity, double sampleRate) override;
    void noteOff() override;
    void render(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

protected:
    // Bore resonator (comb delay)
    static constexpr int kBoreBufSize = 8192;
    float boreBuf[kBoreBufSize] = {};
    int   boreWritePos     = 0;
    float boreDelaySamples = 100.0f;
    float boreFeedback     = 0.0f;
    float boreDampState    = 0.0f;

    // Tonehole LP state
    float toneholeLpState = 0.0f;

    // Breath noise state
    float breathLpState = 0.0f;

    // Aulos-style dual-pipe beating.
    float secondaryPipePhase = 0.0f;
    float secondaryPipeLevel = 0.0f;
    float secondaryPipeRatio = 1.0f;

    // Instrument-specific wind gestures (Dizi membrane, Fujara overblow, water/air turbulence).
    float reedBuzzPhase = 0.0f;
    float reedBuzzLevel = 0.0f;
    float overblowBlend = 0.0f;
};

// =========================================================================
// StruckResonatorVoiceBase — impulse + multi-mode modal synthesis
// Instruments: Angklung, Udu, Pyeongyeong, CristalBaschet, Handpan
// =========================================================================
class StruckResonatorVoiceBase : public InstrumentVoiceBase
{
public:
    void noteOn(const InstrumentSettings& s, int note, float velocity, double sampleRate) override;
    void noteOff() override;
    void render(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

protected:
    // Modal resonator — biquad state per mode
    struct ModalMode {
        float b0 = 0.0f;           // biquad coefficients
        float a1 = 0.0f, a2 = 0.0f;
        float y1 = 0.0f, y2 = 0.0f; // state
        float gain = 0.0f;
    };
    ModalMode modes[kMaxBodyModes];
    int numModes = 0;
    float modeSpread = 1.0f;
    float cavityBlend = 0.0f;
    float cavityState = 0.0f;
    float sympatheticBlend = 0.0f;
    float sympatheticState = 0.0f;
    float struckOutputTrim = 1.0f;

    // Exciter state
    float exciterEnvLevel = 0.0f;
    float exciterDecay    = 0.0f;

    // Continuous glass-bow excitation for Cristal Baschet-like voices.
    float continuousExcitePhase = 0.0f;
    float continuousExciteLevel = 0.0f;
    float continuousExciteDecay = 1.0f;
};

// =========================================================================
// ElectronicVoiceBase — morphable oscillator + FM + waveshaping
// Instruments: Theremin, OndesMartenot, Pyrophone, Hydraulophone
// =========================================================================
class ElectronicVoiceBase : public InstrumentVoiceBase
{
public:
    void noteOn(const InstrumentSettings& s, int note, float velocity, double sampleRate) override;
    void noteOff() override;
    void render(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) override;

protected:
    // FM modulator
    float fmPhase   = 0.0f;
    float fmRatio   = 2.0f;     // carrier:modulator frequency ratio
    float fmDepth   = 0.0f;     // FM modulation depth

    // Vibrato (shared with base class interface but localized here for Electronic instruments)
    float vibratoPhase       = 0.0f;
    float vibratoRateHz      = 5.0f;  // 5 Hz default
    float vibratoDepth       = 0.0f;  // fraction of pitch (0 = no vibrato)
    int   vibratoAge         = 0;
    int   vibratoOnsetSamples = 512;  // ~10ms delay before vibrato kicks in

    float gestureGainCurrent = 1.0f;
    float gestureGainTarget = 1.0f;
    float gestureGainCoeff = 1.0f;
    float electronicPitchCurrent = 1.0f;
    float electronicPitchTarget = 1.0f;
    float electronicPitchCoeff = 1.0f;
    float spectralMotionPhase = 0.0f;
    float spectralMotionDepth = 0.0f;
};

// =========================================================================
// Thin leaf classes — override getCharacteristics() only
// =========================================================================

// --- Bowed ---
class NyckelharpaVoice final : public BowedStringVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 0; }
};
class YayliTanburVoice final : public BowedStringVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 3; }
};
class CrwthVoice final : public BowedStringVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 4; }
};
class YaybaharVoice final : public BowedStringVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 20; }
};
class GayageumVoice final : public PluckedStringVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 1; }
};


// --- Plucked ---
class ChapmanStickVoice final : public PluckedStringVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 2; }
};
class MbiraVoice final : public PluckedStringVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 14; }
};

// --- Wind ---
class CarnyxVoice final : public WindVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 5; }
};
class AulosVoice final : public WindVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 6; }
};
class FujaraVoice final : public WindVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 7; }
};
class GemshornVoice final : public WindVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 8; }
};
class DiziVoice final : public WindVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 9; }
};

// --- Struck ---
class AngklungVoice final : public StruckResonatorVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 10; }
};
class UduVoice final : public StruckResonatorVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 11; }
};
class PyeongyeongVoice final : public StruckResonatorVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 12; }
};
class CristalBaschetVoice final : public StruckResonatorVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 13; }
};
class HandpanVoice final : public StruckResonatorVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 15; }
};

// --- Electronic ---
class ThereminVoice final : public ElectronicVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 16; }
};
class OndesMartenotVoice final : public ElectronicVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 17; }
};
class PyrophoneVoice final : public ElectronicVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 18; }
};
class HydraulophoneVoice final : public ElectronicVoiceBase {
public: const InstrumentCharacteristics& getCharacteristics() const noexcept override;
protected: int getInstrumentIndex() const noexcept override { return 19; }
};

// =========================================================================
// Factory
// =========================================================================
std::unique_ptr<InstrumentVoice> createVoiceForInstrument(int instrumentIndex);

} // namespace mis
