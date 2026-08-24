#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include <vector>

#include "PluginProcessor.h"
#include "../../Shared/SynthCommon.h"

// =============================================================================
// UWdeVST_Orch editor — inherits CommonSynthEditor from Shared/SynthCommon.h
// =============================================================================
class OrchSynthAudioProcessorEditor : public CommonSynthEditor,
                                      private juce::Timer
{
public:
    explicit OrchSynthAudioProcessorEditor(OrchSynthAudioProcessor&);

    // --- CommonSynthEditor pure virtuals ---
    juce::String            pluginNamespace()  const override { return {}; }
    juce::String            pluginTitle()      const override { return "UWdeVST_Orch"; }
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

private:
    using APVTS          = juce::AudioProcessorValueTreeState;
    using SliderAttach   = APVTS::SliderAttachment;
    using ComboBoxAttach = APVTS::ComboBoxAttachment;

    struct CtrlDef { const char* label; const char* suffix; };
    struct FxDef   { const char* label; const char* paramId; };

    void timerCallback() override;
    void rebuildInstrAttachments();
    void rebuildModelSelectorForFamily(int familyIndex, int preferredInstr = -1);
    void applyInstrumentTheme(int instrIndex);
    void syncSelectionUiFromInstr();
    bool isFxTabAvailable(int tabIndex) const;
    int  firstAvailableFxTab() const;
    void syncFxAvailability();
    void syncFxRackState();
    void switchEffectTab(int tabIndex);
    void switchRightPanelSection(int sectionIndex);
    void syncNoteReleaseModeUi();
    void updateOutputGainUi();
    int  selectedInstrFromParam() const;
    void refreshPresetFilterChoices();
    void updatePresetMetadataSummary();
    juce::String currentPresetMetadataSummary() const;
    const mos::InstrumentPreset* currentFactoryPresetDefinition() const noexcept;
    void applyInstrumentControlAvailability(int instrIdx);
    void rebuildFactoryPresetDisplayOrder();
    int resolveFactoryDisplayIndex(int displayIndex) const;
    int resolveFactoryActualIndex(int actualIndex) const;

    static juce::Colour familyColour(int familyIndex);
    static juce::Colour instrCatColour(int instrIndex);

    OrchSynthAudioProcessor& proc;

    static constexpr int kEnvN         = 14;
    static constexpr int kFxN          = 30;
    static constexpr int kMacroTotal   = 4;
    static constexpr int kMacroVisible = 4;
    static constexpr int kModSlots     = OrchSynthAudioProcessor::kNumModMatrixSlots;
    static constexpr int kFxPerTab     = 7;
    static constexpr int kFxTabs       = 8;
    static constexpr int kRightPanelSections = 3;

    std::array<SynthFamilyTab, mos::kNumFamilies> familyTabs;

    juce::ComboBox instrSelector;
    std::unique_ptr<ComboBoxAttach> selInstrAtt;

    std::array<juce::Slider, kEnvN> envDials;
    std::array<juce::Label,  kEnvN> envLabels;
    std::array<std::unique_ptr<SliderAttach>, kEnvN> envAttach;
    EnvelopeDisplay envVisual;
    LfoModulationDisplay lfoVisual;
    juce::Slider lfoRateDial, lfoDepthDial;
    juce::ComboBox lfoWaveSelector;
    juce::ComboBox velocityCurveSelector;
    juce::ComboBox delaySyncSelector;
    juce::ComboBox delayDivisionSelector;
    juce::Slider portamentoSlider;
    juce::Slider legatoSlider;
    juce::Slider roundRobinSlider;
    juce::ComboBox reverbTypeSelector;
    juce::ToggleButton fxLockButton;
    juce::ComboBox qualitySelector;
    juce::ComboBox outputSelector;
    juce::Label velocityCurveLabel;
    juce::Label delaySyncLabel;
    juce::Label delayDivisionLabel;
    juce::Label portamentoLabel;
    juce::Label legatoLabel;
    juce::Label roundRobinLabel;
    juce::Label reverbTypeLabel;
    juce::Label qualityLabel;
    juce::Label outputLabel;
    std::unique_ptr<SliderAttach> lfoRateAtt, lfoDepthAtt;
    std::unique_ptr<ComboBoxAttach> lfoWaveAtt;
    std::unique_ptr<ComboBoxAttach> velocityCurveAtt;
    std::unique_ptr<ComboBoxAttach> delaySyncAtt;
    std::unique_ptr<ComboBoxAttach> delayDivisionAtt;
    std::unique_ptr<SliderAttach> portamentoAtt;
    std::unique_ptr<SliderAttach> legatoAtt;
    std::unique_ptr<SliderAttach> roundRobinAtt;
    std::unique_ptr<ComboBoxAttach> reverbTypeAtt;
    std::unique_ptr<ComboBoxAttach> qualityAtt;
    std::unique_ptr<ComboBoxAttach> outputAtt;

    std::array<juce::Slider, kMacroTotal> macroDials;
    std::array<juce::Label,  kMacroTotal> macroLbls;
    std::array<std::unique_ptr<SliderAttach>, kMacroTotal> macroAtt;
    std::array<SynthEffectTab, kRightPanelSections> rightPanelTabs;

    juce::Label modMatrixTitle;
    juce::Label modMatrixSourceHdr;
    juce::Label modMatrixDestHdr;
    juce::Label modMatrixAmountHdr;
    juce::Viewport modMatrixViewport;
    juce::Component modMatrixRowsComponent;
    std::array<juce::ComboBox, kModSlots> modSourceBoxes;
    std::array<juce::ComboBox, kModSlots> modDestBoxes;
    std::array<juce::Slider,   kModSlots> modAmountSliders;
    std::array<std::unique_ptr<ComboBoxAttach>, kModSlots> modSourceAtt;
    std::array<std::unique_ptr<ComboBoxAttach>, kModSlots> modDestAtt;
    std::array<std::unique_ptr<SliderAttach>,   kModSlots> modAmountAtt;

    std::array<juce::Slider, kFxN> fxDials;
    std::array<juce::Label,  kFxN> fxLbls;
    std::array<std::unique_ptr<SliderAttach>, kFxN> fxAtt;
    std::array<SynthFxRackItem, kFxTabs> fxRackItems;
    std::array<juce::ToggleButton, kFxTabs> fxBypassBtns;
    juce::Label fxDetailTitle;
    juce::Label fxUnavailableLbl;
    using BtnAttach = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<BtnAttach> fxLockAtt;
    std::array<std::unique_ptr<BtnAttach>, kFxTabs> fxBypassAtts;

    int activeFamilyIndex = 0;
    int activeFxTab       = 0;
    int activeRightPanelSection = 0;
    int cachedInstrIdx    = -1;
    int cachedVoiceCount = -1;
    float cachedOutputGainValue = -1000.0f;
    bool cachedFxAvailabilityValid = false;
    std::array<bool, kFxTabs> cachedFxAvailability {};
    juce::String cachedPresetMetadataKey;
    juce::String cachedPresetMetadataSummary;
    std::vector<int> factoryPresetDisplayOrder;

    static constexpr int kFxTabMap[kFxTabs][kFxPerTab] = {
        {  0,  1,  2,  3,  4, -1, -1 },   // REVERB: Size Damp Width Mix Predelay
        {  5,  6, -1, -1, -1, -1, -1 },   // SAT: Drive Mix
        {  7,  8,  9, -1, -1, -1, -1 },   // TRANS: Attack Sustain Mix
        { 10, 11, 12, 13, 14, -1, -1 },   // COMP: Thresh Ratio Atk Rel Mix
        { 15, 16, 17, 18, 19, 20, 21 },   // EQ: LF LG MF MG MQ HF HG
        { 22, 23, 24, -1, -1, -1, -1 },   // CHORUS: Rate Depth Mix
        { 25, 26, 27, -1, -1, -1, -1 },   // DELAY: Time Feedback Mix
        { 28, 29, -1, -1, -1, -1, -1 }    // LIMITER: Threshold Release
    };

    static const char* kFxBypassParamIds[kFxTabs];

    static const std::array<CtrlDef, kEnvN>       kEnvCtrls;
    static const std::array<FxDef,   kMacroTotal> kMacroCtrls;
    static const std::array<FxDef,   kFxN>        kFxCtrls;

    static const char* kFxTabNames[kFxTabs];
    static const char* kFxRackSummaries[kFxTabs];
    static const char* kFxTabLabels[kFxTabs][kFxPerTab];

    // ── MIDI CC page indicator (FLkey Mini) ────────────────────────────
    juce::Label midiCCPageLabel;
    juce::Label outputGainLabel;
    int cachedMidiCCPage = -1;
    bool cachedStopNotesOnKeyRelease = true;
    AmberShakeButton noteReleaseModeBtn;
    AmberShakeButton randButton;
    AmberShakeButton stopNotesButton;
    juce::Label voiceCountLabel;

    // ── Tooltip mode system ────────────────────────────────────────────
    enum class TooltipMode { Off, Short, Novice };
    TooltipMode tooltipMode = TooltipMode::Short;

    juce::TooltipWindow tooltipWindow { this, 600 };
    juce::TextButton    tooltipModeBtn;
    juce::Label         presetBrowserHintLabel;
    juce::Label         presetMetaLabel;

    void cycleTooltipMode();
    void applyTooltips();

    static constexpr int kTooltipCount = kEnvN + 2 + kMacroTotal + kFxN + 1; // 14+2+4+30+1 = 51
    static const char* kTooltipsShort[kTooltipCount];
    static const char* kTooltipsNovice[kTooltipCount];

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OrchSynthAudioProcessorEditor)
};
