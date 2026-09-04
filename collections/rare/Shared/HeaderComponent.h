#pragma once
// =============================================================================
// HeaderComponent.h — Standalone header component for UWdeVST synth editors
//
// Replaces the paint-only paintHeader() / computeHeaderZones() approach from
// CommonSynthEditor with a proper juce::Component subclass.
//
// Layout: 3 horizontal zones (left→right)
//   identity  — plugin logo + title
//   preset    — preset combobox + search + nav buttons + save/saveAs/delete
//   status    — gain dial + status chips
//
// Theme: UIThemeV5 tokens (dark palette) + accent colour per-plugin
// Responsive: adapts to available width
// =============================================================================
#include <JuceHeader.h>
#include "UIThemeV5.h"
#include "KnobComponentV5.h"

// Forward declaration
namespace synthui { enum class TooltipMode; }

class HeaderComponent : public juce::Component
{
public:
    // =============================================================================
    // Sub-component slots — owned by this component
    // =============================================================================

    /** Preset zone widgets */
    juce::ComboBox       presetBox;
    juce::TextEditor     presetSearch;
    juce::TextButton     prevPresetBtn{ "◀" };
    juce::TextButton     nextPresetBtn{ "▶" };
    juce::TextButton     savePresetBtn{ "Save" };
    juce::TextButton     saveAsPresetBtn{ "Save As" };
    juce::TextButton     deletePresetBtn{ "Delete" };
    juce::TextButton     importPresetsBtn{ "Import" };

    /** Status zone widgets — gain dial uses KnobComponentV5 for V5 theming */
    KnobComponentV5      gainDial{ "GAIN", -24.0, 12.0, 0.0 };
    juce::ToggleButton   singleBtn;

    // =============================================================================
    // Configuration
    // =============================================================================

    /** Set the per-plugin accent colour */
    void setAccent(juce::Colour accent);

    /** Set an optional logo image for the identity zone */
    void setLogo(juce::Image logo);

    /** Returns the accent colour */
    juce::Colour getAccent() const { return accent_; }

    // =============================================================================
    // Layout geometry
    // =============================================================================

    struct [[nodiscard]] Zones
    {
        juce::Rectangle<int> headerBounds;
        juce::Rectangle<int> contentBounds;
        juce::Rectangle<int> identityZone;
        juce::Rectangle<int> presetZone;
        juce::Rectangle<int> statusZone;
        juce::Rectangle<int> presetPrimaryRow;
        juce::Rectangle<int> presetSecondaryRow;
        juce::Rectangle<int> statusPrimaryRow;
        juce::Rectangle<int> statusSecondaryRow;
    };

    /** Returns layout zones based on current component bounds */
    Zones computeZones() const;

    // =============================================================================
    // Component overrides
    // =============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Apply synth-style colours to preset search, buttons, etc. */
    void applySynthStyle(const juce::Colour& accent);

    /** Set layout density (0=compact, 0.5=normal, 1=roomy) for responsive layout */
    void setDensity(float density) { density_ = juce::jlimit(0.0f, 1.0f, density); }

    /** Returns the current layout density */
    float getDensity() const { return density_; }

private:
    float density_         { 0.5f };
    juce::Colour accent_     { 0xffD4A017 };
    juce::Image  logo_;
    bool         logoLoaded_{ false };

    void draw_identity_zone(juce::Graphics& g, const juce::Rectangle<int>& zone);
    void draw_preset_zone (juce::Graphics& g, const juce::Rectangle<int>& zone);
    void draw_status_zone (juce::Graphics& g, const juce::Rectangle<int>& zone);
    void apply_v5_style_to_editor(juce::TextEditor& ed) const;
};
