#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "Engine/InstrumentVoice.h"
#include "Engine/FxProcessors.h"
#include "Engine/FactoryPresets.h"
#include "../../Shared/PitchBendState.h"
#include "../../Shared/ModulationMatrix.h"

class InstrSynthAudioProcessor : public juce::AudioProcessor,
                                 private juce::AudioProcessorValueTreeState::Listener,
                                 private juce::AsyncUpdater
{
public:
    static constexpr int kNumAuxOutputs      = 4;
    static constexpr int kMaxVoices          = 32;
    static constexpr int kMaxVoicesPerInstr  = 8;
    static constexpr const char* kProcessorName = "UWdeVST_instr";

    InstrSynthAudioProcessor();
    ~InstrSynthAudioProcessor() override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static juce::String makeInstParamId(int instrIndex, const juce::String& suffix);
    static auto createBusLayout() -> BusesProperties;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override
    {
#if defined(JucePlugin_Name)
        return JucePlugin_Name;
#else
        return kProcessorName;
#endif
    }
    bool acceptsMidi()    const override { return true; }
    bool producesMidi()   const override { return false; }
    bool isMidiEffect()   const override { return false; }
    double getTailLengthSeconds() const override;

    int  getNumPrograms() override;
    int  getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return parameters; }
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    juce::StringArray getFactoryPresetNames() const;
    int  getCurrentFactoryPresetIndex() const noexcept;
    void applyFactoryPreset(int presetIndex);
    bool saveFactoryPreset(int presetIndex);

    // User preset management
    static juce::File getUserPresetsDirectory(int instrIndex);
    static juce::File getFactoryOverridesDirectory();
    juce::Array<juce::File> scanUserPresets() const;
    bool saveUserPreset(const juce::String& name);
    bool updateUserPreset(const juce::File& file);
    bool deleteUserPreset(const juce::File& file);
    bool loadUserPreset(const juce::File& file);
    bool isCurrentPresetUser() const noexcept;
    juce::File getCurrentUserPresetFile() const noexcept;

    int getSelectedInstrumentIndex() const;

    // ── Modulation matrix ─────────────────────────────────────────────
    modmatrix::ModulationMatrix&       getModulationMatrix()       noexcept { return modulationMatrix; }
    const modmatrix::ModulationMatrix& getModulationMatrix() const noexcept { return modulationMatrix; }
    modmatrix::ModSlot getModMatrixSlot(int index) const;
    void setModMatrixSlot(int index, modmatrix::Source source,
                          modmatrix::Destination destination, float amount);
    float getModMatrixLfo2Rate() const noexcept;
    int   getModMatrixLfo2Wave() const noexcept;
    void  setModMatrixLfo2Rate(float rateHz);
    void  setModMatrixLfo2Wave(int waveformIndex);

    // ── MIDI CC page system (FLkey Mini) ──────────────────────────────
    static constexpr int kNumCCPages = 7;
    int  getMidiCCPage() const noexcept { return midiCCPage.load(std::memory_order_relaxed); }
    static const char* getCCPageName(int page) noexcept;

    struct PersistedPresetMetadata
    {
        juce::String mixRole { "custom" };
        juce::String family;
        juce::String tags;
        juce::String description;
        juce::String outputProfile;
        float nominalPeakDb = -12.0f;
    };

    struct PresetPersistenceState
    {
        juce::String name;
        int instrIndex = 0;
        int presetIndex = -1;
        mis::InstrumentSettings settings;
        mis::GlobalFxSettings fx;
        int outputBus = 0;
        mis::PerformanceSettings performance {};
        modmatrix::MatrixState modMatrix {};
        PersistedPresetMetadata metadata;
    };

private:
    struct VoiceSlot
    {
        mis::InstrumentVoice* voice = nullptr;   // borrowed from voiceBank
        int midiNote        = -1;
        int midiChannel     = 0;
        int instrumentIndex = -1;
        int poolSlot        = -1;
        uint64_t activationAge = 0;
    };

    struct DyingVoiceSlot
    {
        mis::InstrumentVoice* voice = nullptr;
        int midiChannel     = 0;
        int instrumentIndex = -1;
        int poolSlot        = -1;
        int outputBus       = 0;
        uint64_t activationAge = 0;
    };

    struct SustainedNote
    {
        int instrumentIndex = -1;
        int midiNote = -1;
        int midiChannel = 0;
    };

    static constexpr int kMaxSustainedNotes = 128;

    float getParamValue(const juce::String& paramId) const;
    void  setParamValueInternal(const juce::String& paramId, float value, bool notifyHost);
    void  setParamValue(const juce::String& paramId, float value);
    void  queueParamUpdate(juce::RangedAudioParameter* parameter, float normalisedValue);
    PresetPersistenceState captureCurrentPresetState(int instrIndex) const;
    PresetPersistenceState makeFactoryPresetState(int instrIndex,
                                                  int presetIndex,
                                                  const mis::InstrumentPreset& preset) const;
    void applyPresetPersistenceState(const PresetPersistenceState& state);
    mis::InstrumentSettings snapshotInstrumentSettings(int instrIndex) const;
    mis::GlobalFxSettings snapshotGlobalFxSettings() const;
    void applyGlobalFxSettings(int instrIndex, const mis::GlobalFxSettings& settings, bool notifyHost = true);
    void applyPerformanceMacros(int instrIndex, mis::InstrumentSettings& s) const;
    bool poolSlotOwnedByAnyVoice(int instrumentIndex, int poolSlot) const;
    static void resetVoiceSlotMetadata(VoiceSlot& slot);
    static bool writePresetWithManifestRollback(const juce::File& file,
                                                juce::XmlElement& root,
                                                const juce::String& presetName,
                                                int instrumentIndex,
                                                const juce::String& sourceModel);
    int  findFreeVoice() const;
    void clearVoice(VoiceSlot& slot);
    void clearDyingVoice(DyingVoiceSlot& slot);
    void panicAllVoices();
    void panicVoicesOnChannel(int midiChannel);
    void releaseVoices(int midiChannel, bool immediate);
    void triggerNoteOn(int instrIndex, int midiChannel, int midiNote, float velocity);
    void triggerNoteOff(int midiChannel, int midiNote);
    void clearSustainedNotes() noexcept;
    void removeSustainedNotesForChannel(int midiChannel) noexcept;
    bool addSustainedNote(int instrumentIndex, int midiNote, int midiChannel) noexcept;
    void releaseSustainedNotesForChannel(int midiChannel);
    void storeCurrentInstrumentFxState(int instrIndex);
    void restoreInstrumentFxState(int instrIndex);
    bool isFxAvailableForCurrentInstrument(mis::GlobalFxSlot slot) const;
    void updateGlobalEffectParameters();
    void processGlobalTransient(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalSaturator(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalEQ(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalCompressor(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalChorus(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalDelay(juce::AudioBuffer<float>& mainBuffer);
    void applyGlobalLfo(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalReverb(juce::AudioBuffer<float>& mainBuffer);
    void processGlobalLimiter(juce::AudioBuffer<float>& mainBuffer);
    void loadFactoryOverrides();
    void applyInstPresetSettings(int instrIndex, const mis::InstrumentSettings& s);
    void handleMidiCC(int ccNumber, int ccValue, int instrIndex);
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState parameters;
    juce::MidiKeyboardState keyboardState;
    std::array<std::vector<mis::InstrumentPreset>, mis::kNumInstruments> factoryPresetBanks;
    std::array<std::vector<PresetPersistenceState>, mis::kNumInstruments> factoryPresetStates;

    // Voice pool (owns all voices — no RT allocation)
    std::array<std::array<std::unique_ptr<mis::InstrumentVoice>, kMaxVoicesPerInstr>, mis::kNumInstruments> voiceBank;
    std::array<std::array<std::atomic_bool, kMaxVoicesPerInstr>, mis::kNumInstruments> voicePoolInUse{};

    std::array<VoiceSlot, kMaxVoices> voices;
    std::array<DyingVoiceSlot, kMaxVoices> dyingVoices;
    uint64_t voiceAgeCounter = 0;

    // FX processors
    juce::dsp::Compressor<float> compressor;
    mis::fx::DattorroPlateReverb   reverbProcessor;
    mis::fx::ParametricEQ3Band     eqProcessor;
    mis::fx::StereoChorus          chorusProcessor;
    mis::fx::StereoDelay           delayProcessor;
    mis::fx::OutputLimiter         limiterProcessor;
    juce::dsp::Oversampling<float> satOversamplingMono {
        1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false
    };
    juce::dsp::Oversampling<float> satOversamplingStereo {
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, false
    };

    juce::AudioBuffer<float> fxDryBuffer;
    juce::MidiBuffer oversizedChunkMidi;
    std::array<float, 2> transientFastEnv = { 0.0f, 0.0f };
    std::array<float, 2> transientSlowEnv = { 0.0f, 0.0f };
    double preparedSampleRate = 0.0;
    float lfoPhase = 0.0f;

    struct CompressorCache
    {
        float threshold =  1.0f;
        float ratio     = -1.0f;
        float attack    = -1.0f;
        float release   = -1.0f;
    } compCache;

    PitchBendState pitchBend;
    VelocityCurve  velocityCurve = VelocityCurve::Linear;
    std::array<bool, 17> sustainPedalHeld {};
    std::array<SustainedNote, kMaxSustainedNotes> sustainedNotes {};
    int sustainedNoteCount = 0;
    juce::SmoothedValue<float> outputGainSmoother;

    modmatrix::ModulationMatrix modulationMatrix;
    modmatrix::ModResult cachedModResult;

    std::array<int,        mis::kNumInstruments> currentPresetIndices;
    std::array<juce::File, mis::kNumInstruments> currentUserPresetFiles;
    std::array<mis::GlobalFxSettings, mis::kNumInstruments> instrumentFxStates;
    mis::GlobalFxSettings blockRuntimeFxSettings;
    std::array<bool, 8> latchedFxAvailability {};
    std::array<bool, 8> blockFxAvailability {};
    int latchedFxOwnerInstrumentIndex = -1;
    struct PendingParamUpdate
    {
        juce::RangedAudioParameter* parameter = nullptr;
        float normalisedValue = 0.0f;
    };
    static constexpr int kPendingParamQueueSize = 32;
    juce::AbstractFifo pendingParamUpdateFifo { kPendingParamQueueSize };
    std::array<PendingParamUpdate, kPendingParamQueueSize> pendingParamUpdates {};
    mutable std::mutex stateMutex;
    std::atomic<bool> isRestoringState { false };
    std::atomic<int> pendingSelectedInstrumentIndex { 0 };
    int cachedSelectedInstrumentIndex = 0;
    std::atomic<int> midiCCPage { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(InstrSynthAudioProcessor)
};
