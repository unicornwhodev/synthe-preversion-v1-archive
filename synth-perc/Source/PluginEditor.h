#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>

#include "PluginProcessor.h"
#include "../../Shared/SynthCommon.h"

// =============================================================================
// Perc Synth editor — inherits CommonSynthEditor from Shared/SynthCommon.h
// =============================================================================
class PercSynthAudioProcessorEditor : public CommonSynthEditor,
                                      private juce::Timer,
                                      private juce::AudioProcessorValueTreeState::Listener,
                                      private juce::AsyncUpdater
{
public:
    explicit PercSynthAudioProcessorEditor(PercSynthAudioProcessor&);
    ~PercSynthAudioProcessorEditor() override;

    // --- CommonSynthEditor pure virtuals ---
    juce::String            pluginNamespace()  const override { return {}; }
    juce::String            pluginTitle()      const override { return "UWdeVST_Perc"; }
    juce::StringArray       hostGetFactoryNames()              override;
    juce::Array<juce::File> hostScanUserPresets()              override;
    bool                    hostIsUserPreset()                 override;
    juce::File              hostCurrentUserFile()              override;
    int                     hostCurrentFactoryIdx()            override;
    void                    hostApplyFactory(int idx)          override;
    void                    hostLoadUser(const juce::File& f)  override;
    bool                    hostSaveUser(const juce::String& n)override;
    void                    hostUpdateUser(const juce::File& f)override;
    void                    hostSaveFactory(int idx)           override;
    void                    hostDeleteUser(const juce::File& f)override;
    juce::File              hostGetUserPresetsDir()            override;
    juce::File              hostGetUserPresetsDirForIndex(int instrumentIndex) override;
    juce::String            hostPresetInstrumentAttr() const   override;
    juce::String            hostFormatFactoryPresetLabel(int presetIndex,
                                                         const juce::String& displayName) const override;
    juce::String            hostFormatUserPresetLabel(const juce::File& presetFile,
                                                      const juce::String& displayName) const override;
    juce::String            hostFactoryPresetSearchText(int presetIndex,
                                                        const juce::String& displayName) const override;
    juce::String            hostUserPresetSearchText(const juce::File& presetFile,
                                                     const juce::String& displayName) const override;
    bool                    hostShouldIncludeFactoryPreset(int presetIndex) const override;
    bool                    hostShouldIncludeUserPreset(const juce::File& presetFile) const override;

    void paint(juce::Graphics&) override;
    void resized() override;

#if defined(UWDEVST_PERC_TEST_BUILD)
    struct LayoutSnapshot
    {
        bool compact = false;
        juce::Rectangle<int> editorBounds;
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> selectorPanelBounds;
        juce::Rectangle<int> selectorCardBounds;
        juce::Rectangle<int> modelSelectorBounds;
        juce::Rectangle<int> presetMetaBounds;
        juce::Rectangle<int> qualitySelectorBounds;
        juce::Rectangle<int> statusControlsBounds;
        juce::Rectangle<int> outputSelectorBounds;
        juce::Rectangle<int> outputBayBounds;
        juce::Rectangle<int> rightPanelTabsBounds;
        juce::Rectangle<int> macroControlsBounds;
        juce::Rectangle<int> lfoVisualBounds;
        juce::Rectangle<int> fxLockBounds;
        juce::Rectangle<int> modMatrixContentBounds;
        juce::Rectangle<int> keyboardBounds;
        juce::Rectangle<int> octaveControlsBounds;
        bool fxLockVisible = false;
    };

    LayoutSnapshot captureLayoutSnapshotForTests() const;
    void setRightPanelSectionForTests(int sectionIndex);
    juce::String formatToneCutValueForTests(double hz);
#endif

private:
    using APVTS          = juce::AudioProcessorValueTreeState;
    using SliderAttach   = APVTS::SliderAttachment;
    using ComboBoxAttach = APVTS::ComboBoxAttachment;

    struct CtrlDef { const char* label; const char* suffix; };
    struct FxDef   { const char* label; const char* paramId; };

    void timerCallback() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;
    void rebuildInstrAttachments();
    void rebuildModelSelectorForFamily(int familyIndex, int preferredInstr = -1);
    void syncSelectionUiFromInstr();
    void syncFxAvailability();
    void syncFxRackState();
    void switchEffectTab(int tabIndex);
    void switchRightPanelSection(int sectionIndex);
    void applyInstrumentTheme(int instrIndex);
    void configureValueDisplays();
    bool isFxTabAvailable(int tabIndex, int instrIndex) const;
    int  firstAvailableFxTab(int instrIndex) const;
    int  selectedInstrFromParam() const;
    void syncAdvancedModUi();
    void refreshPresetFilterChoices();
    void updatePresetMetadataSummary();
    juce::String currentPresetMetadataSummary() const;
    const mpc::InstrumentPreset* currentFactoryPresetDefinition() const noexcept;

#if defined(UWDEVST_PERC_TEST_BUILD)
    struct VisualLayoutSnapshot
    {
        bool compact = false;
    };

    VisualLayoutSnapshot computeVisualLayoutSnapshot(int width, int height) const;
#endif

    static juce::Colour familyColour(int familyIndex);
    static juce::Colour instrCatColour(int instrIndex);

    PercSynthAudioProcessor& proc;

    static constexpr int kEnvN         = 16;
    static constexpr int kFxN          = 31;
    static constexpr int kMacroTotal   = 4;
    static constexpr int kMacroVisible = 4;
    static constexpr int kFxPerTab     = 7;
    static constexpr int kFxTabs       = 8;
    static constexpr int kRightPanelSections = 3;

    std::array<SynthFamilyTab,  mpc::kNumFamilies>    familyTabs;
    std::array<SynthPresetCard, mpc::kNumInstruments> presetCards;

    juce::ComboBox instrSelector;
    std::unique_ptr<ComboBoxAttach> selInstrAtt;

    std::array<juce::Slider, kEnvN> envDials;
    std::array<juce::Label,  kEnvN> envLabels;
    std::array<std::unique_ptr<SliderAttach>, kEnvN> envAttach;
    EnvelopeDisplay envVisual;
    LfoModulationDisplay lfoVisual;
    juce::Slider lfoRateDial, lfoDepthDial;
    juce::ComboBox lfoWaveSelector;
    std::unique_ptr<SliderAttach> lfoRateAtt, lfoDepthAtt;
    std::unique_ptr<ComboBoxAttach> lfoWaveAtt;
    juce::ComboBox qualitySelector;
    std::unique_ptr<ComboBoxAttach> qualityAtt;

    std::array<juce::Slider, kMacroTotal> macroDials;
    std::array<juce::Label,  kMacroTotal> macroLbls;
    std::array<std::unique_ptr<SliderAttach>, kMacroTotal> macroAtt;

    std::array<juce::Slider, kFxN> fxDials;
    std::array<juce::Label,  kFxN> fxLbls;
    std::array<std::unique_ptr<SliderAttach>, kFxN> fxAtt;
    std::array<SynthEffectTab, kRightPanelSections> rightPanelTabs;
    std::array<SynthFxRackItem, kFxTabs> fxRackItems;
    std::array<juce::ToggleButton, kFxTabs> fxBypassBtns;
    using BtnAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::array<std::unique_ptr<BtnAttach>, kFxTabs> fxBypassAtts;
    juce::Label fxDetailTitle;
    juce::Label fxUnavailableLbl;
    juce::ComboBox delaySyncSelector;
    juce::ComboBox delayDivisionSelector;
    juce::Label delaySyncLabel;
    juce::Label delayDivisionLabel;
    std::unique_ptr<ComboBoxAttach> delaySyncAtt;
    std::unique_ptr<ComboBoxAttach> delayDivisionAtt;

    // ── MIDI CC page indicator (FLkey Mini) ────────────────────────────
    juce::Label midiCCPageLabel;
    int cachedMidiCCPage = -1;

    // ── Tooltip mode system ────────────────────────────────────────────
    enum class TooltipMode { Off, Short, Novice };
    TooltipMode tooltipMode = TooltipMode::Short;

    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::TextButton    tooltipModeBtn;
    juce::TextButton    advancedModBtn;
    juce::Label         presetSourceLabel, presetFamilyLabel, presetRoleLabel, presetTagLabel, presetMetaLabel;
    juce::ComboBox      presetSourceFilter, presetFamilyFilter, presetRoleFilter, presetTagFilter;

    static constexpr int kModSlots = modmatrix::ModulationMatrix::getNumSlots();
    std::array<juce::Label, kModSlots> modSlotLabels;
    std::array<juce::ComboBox, kModSlots> modSourceBoxes;
    std::array<juce::ComboBox, kModSlots> modDestBoxes;
    std::array<juce::Slider, kModSlots> modAmountSliders;
    juce::Label modLfo2RateLabel;
    juce::Label modLfo2WaveLabel;
    juce::Slider modLfo2RateDial;
    juce::ComboBox modLfo2WaveSelector;

    void cycleTooltipMode();
    void applyTooltips();

    static constexpr int kTooltipCount = kEnvN + 2 + kMacroTotal + kFxN + 1; // env+lfo+macro+fx+gain = 16+2+4+31+1 = 54
    static const char* kTooltipsShort[kTooltipCount];
    static const char* kTooltipsNovice[kTooltipCount];

    int activeFamilyIndex = 0;
    int activeFxTab       = 0;
    int activeRightPanelSection = 0;
    int cachedInstrIdx    = -1;
    juce::Rectangle<int> outputBayBounds;

    static constexpr int kFxTabMap[kFxTabs][kFxPerTab] = {
        { 11, 12, 13, 14, 15, -1, -1 },
        {  0,  1, -1, -1, -1, -1, -1 },
        {  2,  3,  4, -1, -1, -1, -1 },
        {  5,  6,  7,  8,  9, 10, -1 },
        { 16, 17, 18, 19, 20, 21, 22 },
        { 23, 24, 25, -1, -1, -1, -1 },
        { 26, 27, 28, -1, -1, -1, -1 },
        { 29, 30, -1, -1, -1, -1, -1 }
    };

    static const char* kFxRackSummaries[kFxTabs];
    static const char* kFxBypassParamIds[kFxTabs];

    static const std::array<CtrlDef, kEnvN>       kEnvCtrls;
    static const std::array<FxDef,   kMacroTotal> kMacroCtrls;
    static const std::array<FxDef,   kFxN>        kFxCtrls;

    static const char* kFxTabNames[kFxTabs];
    static const char* kFxTabLabels[kFxTabs][kFxPerTab];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PercSynthAudioProcessorEditor)
};
