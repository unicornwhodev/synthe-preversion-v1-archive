// filepath: Shared/HeaderComponent.cpp
#include "HeaderComponent.h"
#include "SynthCommon.h"

// =============================================================================
// HeaderComponent
// =============================================================================

void HeaderComponent::setAccent(juce::Colour accent)
{
    accent_ = accent;
    repaint();
}

void HeaderComponent::setLogo(juce::Image logo)
{
    logo_ = logo;
    logoLoaded_ = logo.isValid();
    repaint();
}

// =============================================================================
// computeZones — mirrors CommonSynthEditor::computeHeaderZones() but uses
// actual component bounds instead of a passed headerH parameter
// =============================================================================
HeaderComponent::Zones HeaderComponent::computeZones() const
{
    Zones zones;

    // Responsive header height based on density
    const int minHeaderH = density_ < 0.35f ? 64 : (density_ > 0.65f ? 100 : 80);
    const int safeHeaderH = juce::jmax(minHeaderH, getHeight());
    zones.headerBounds = { 12, 10, juce::jmax(240, getWidth() - 24), juce::jmax(44, safeHeaderH - 14) };
    zones.contentBounds = zones.headerBounds.reduced(14, 8);

    const int zoneGap = 12;
    const int contentW = zones.contentBounds.getWidth();

    // Density affects zone widths — compact gives more to preset, roomy gives more to identity
    const float identityFraction = juce::jmap(density_, 0.0f, 1.0f, 0.14f, 0.22f);
    const int identityW = juce::jlimit(196, 260, static_cast<int>(std::round(contentW * identityFraction)));
    const float statusFraction = juce::jmap(density_, 0.0f, 1.0f, 0.26f, 0.22f);
    const int statusW   = juce::jlimit(260, 400, static_cast<int>(std::round(contentW * statusFraction)));
    const int presetW   = juce::jmax(320, contentW - identityW - statusW - zoneGap * 2);

    auto strip = zones.contentBounds;
    zones.identityZone = strip.removeFromLeft(identityW);
    strip.removeFromLeft(zoneGap);
    zones.presetZone = strip.removeFromLeft(juce::jmin(presetW, strip.getWidth()));
    strip.removeFromLeft(juce::jmin(zoneGap, strip.getWidth()));
    zones.statusZone = strip;

    // Compact mode: single row; normal/roomy: two rows
    if (density_ < 0.35f)
    {
        // Compact single-row layout
        zones.presetPrimaryRow   = zones.presetZone;
        zones.presetSecondaryRow = {};
        zones.statusPrimaryRow   = zones.statusZone;
        zones.statusSecondaryRow = {};
    }
    else
    {
        auto presetZoneRows = zones.presetZone;
        zones.presetPrimaryRow   = presetZoneRows.removeFromTop(32);
        zones.presetSecondaryRow  = zones.presetZone.withTrimmedTop(36);

        auto statusZoneRows = zones.statusZone;
        zones.statusPrimaryRow   = statusZoneRows.removeFromTop(24);
        zones.statusSecondaryRow  = zones.statusZone.withTrimmedTop(28);
    }

    return zones;
}

// =============================================================================
// paint — uses UIThemeV5 theme tokens + accent for glow strips
// =============================================================================
void HeaderComponent::paint(juce::Graphics& g)
{
    const auto zones = computeZones();
    auto headerBounds = zones.headerBounds.toFloat();

    // Drop shadow
    g.setColour(juce::Colours::black.withAlpha(0.14f));
    g.fillRoundedRectangle(headerBounds.translated(0.0f, 3.0f), 10.0f);

    // Header panel background using V5 theme
    UIThemeV5::fillPanel(g, headerBounds, 10.0f);

    // Accent glow strip (bottom-left corner marker)
    auto accentLane = juce::Rectangle<float>(
        headerBounds.getX() + 5.0f, headerBounds.getBottom() - 3.0f,
        76.0f, 1.6f);
    UIThemeV5::drawGlowStrip(g, accentLane, 2.5f, 0.65f);

    // Identity zone
    draw_identity_zone(g, zones.identityZone);

    // Preset zone
    draw_preset_zone(g, zones.presetZone);

    // Status zone
    draw_status_zone(g, zones.statusZone);

    // Top accent glow line across full header width
    auto accentGlow = headerBounds.reduced(22.0f, 0.0f);
    accentGlow = accentGlow.removeFromBottom(2.2f);
    UIThemeV5::drawGlowStrip(g, accentGlow, 2.5f, 0.80f);
}

// =============================================================================
// draw_identity_zone
// =============================================================================
void HeaderComponent::draw_identity_zone(juce::Graphics& g,
                                          const juce::Rectangle<int>& zone)
{
    auto titleWell = zone.toFloat();
    titleWell.removeFromBottom(16.0f);
    titleWell.setHeight(28.0f);

    UIThemeV5::fillRecess(g, titleWell, 6.0f);

    auto titleSheen = titleWell;
    titleSheen.setHeight(titleSheen.getHeight() * 0.48f);
    juce::ColourGradient titleSheenGrad(
        juce::Colours::white.withAlpha(0.035f), titleSheen.getCentreX(), titleSheen.getY(),
        juce::Colours::transparentWhite, titleSheen.getCentreX(), titleSheen.getBottom(), false);
    g.setGradientFill(titleSheenGrad);
    g.fillRoundedRectangle(titleSheen, 6.0f);

    if (logoLoaded_)
    {
        const int logoX = static_cast<int>(titleWell.getX()) - 2;
        const int logoY = static_cast<int>(titleWell.getY()) - 5;
        const int logoW = static_cast<int>(juce::jmin(208.0f, titleWell.getWidth() + 56.0f));
        const int logoH = static_cast<int>(titleWell.getHeight() + 22.0f);
        g.setOpacity(1.0f);
        g.drawImageWithin(logo_, logoX, logoY, logoW, logoH,
                          juce::RectanglePlacement::xLeft | juce::RectanglePlacement::yMid
                              | juce::RectanglePlacement::onlyReduceInSize,
                          false);
    }
}

// =============================================================================
// draw_preset_zone
// =============================================================================
void HeaderComponent::draw_preset_zone(juce::Graphics& g,
                                       const juce::Rectangle<int>& zone)
{
    auto presetPrimary   = zone.reduced(0, 1).toFloat();
    auto presetSecondary  = zone.reduced(0, 1).toFloat();
    presetSecondary = presetSecondary.withTrimmedTop(4.0f);

    UIThemeV5::fillRecess(g, presetPrimary, 7.0f);
    UIThemeV5::fillPanel(g, presetSecondary, 7.0f);

    g.setColour(UIThemeV5::accent().withAlpha(0.035f));
    g.fillRoundedRectangle(
        presetPrimary.withWidth(juce::jmin(260.0f, presetPrimary.getWidth() * 0.34f)), 7.0f);

    g.setColour(juce::Colour(0xff303742).withAlpha(0.20f));
    g.drawRoundedRectangle(presetPrimary.reduced(0.5f), 7.0f, 0.75f);
    g.drawRoundedRectangle(presetSecondary.reduced(0.5f), 7.0f, 0.75f);
}

// =============================================================================
// draw_status_zone
// =============================================================================
void HeaderComponent::draw_status_zone(juce::Graphics& g,
                                    const juce::Rectangle<int>& zone)
{
    auto statusPrimary   = zone.reduced(0, 1).toFloat();
    auto statusSecondary = zone.reduced(0, 1).toFloat();
    statusSecondary = statusSecondary.withTrimmedTop(4.0f);

    UIThemeV5::fillPanel(g, statusPrimary, 7.0f);
    UIThemeV5::fillRecess(g, statusSecondary, 7.0f);

    g.setColour(UIThemeV5::accent().withAlpha(0.035f));
    g.fillRoundedRectangle(
        statusPrimary.withWidth(juce::jmin(150.0f, statusPrimary.getWidth() * 0.46f)), 7.0f);

    g.setColour(juce::Colour(0xff303742).withAlpha(0.20f));
    g.drawRoundedRectangle(statusPrimary.reduced(0.5f), 7.0f, 0.75f);
    g.drawRoundedRectangle(statusSecondary.reduced(0.5f), 7.0f, 0.75f);
}

// =============================================================================
// resized — position all child widgets (mirrors CommonSynthEditor::resizeChrome)
// Responsive: compact mode collapses to single-row; normal/roomy uses two rows
// =============================================================================
void HeaderComponent::resized()
{
    const auto zones = computeZones();
    const bool compact = density_ < 0.35f;
    const int ctrlH = compact ? 28 : 30;

    // ── Status zone ────────────────────────────────────────────────────────
    const int dialSize = compact ? 42 : 54;
    const int dialY = compact
        ? zones.statusZone.getCentreY() - dialSize / 2
        : zones.contentBounds.getY() - 1;
    int rightX = zones.statusZone.getRight() - dialSize;
    gainDial.getSlider().setBounds(rightX, dialY, dialSize, dialSize);

    if (!compact)
    {
        rightX -= 68;
        singleBtn.setBounds(rightX, zones.statusPrimaryRow.getY() + 1, 64, 24);
    }

    // ── Preset zone — primary row (always present) ─────────────────────────
    const int searchW = compact
        ? juce::jlimit(80, 120, zones.presetZone.getWidth() / 4)
        : juce::jlimit(120, 180, zones.presetPrimaryRow.getWidth() / 4);
    int x = zones.presetZone.getX();
    const int topRowY = compact
        ? zones.presetZone.getCentreY() - ctrlH / 2
        : zones.presetPrimaryRow.getY()
            + (zones.presetPrimaryRow.getHeight() - ctrlH) / 2;
    presetSearch.setBounds(x, topRowY, searchW, ctrlH);
    apply_v5_style_to_editor(presetSearch);
    x += searchW + 8;

    const int navW = compact ? 24 : 28;
    const int presetBoxW = compact
        ? juce::jmax(140, zones.presetZone.getRight() - x - navW * 2 - 8)
        : juce::jmax(180, zones.presetPrimaryRow.getRight() - x - navW * 2 - 8);
    prevPresetBtn.setBounds(x, topRowY, navW, ctrlH);
    x += navW + 4;
    presetBox.setBounds(x, topRowY, presetBoxW, ctrlH);
    x += presetBoxW + 4;
    nextPresetBtn.setBounds(x, topRowY, navW, ctrlH);

    // ── Preset zone — secondary row (action buttons, normal/roomy only) ───
    if (!compact)
    {
        const int btnY = zones.presetSecondaryRow.getY()
                         + juce::jmax(0, (zones.presetSecondaryRow.getHeight() - 22) / 2);
        const int btnX = zones.presetSecondaryRow.getX();
        savePresetBtn.setBounds(    btnX,        btnY, 64, 22);
        saveAsPresetBtn.setBounds( btnX + 72,   btnY, 78, 22);
        deletePresetBtn.setBounds( btnX + 158,  btnY, 70, 22);
        importPresetsBtn.setBounds(btnX + 236,  btnY, 70, 22);
    }

    // ── Add all children to HeaderComponent (owned by this component) ─────
    addAndMakeVisible(presetSearch);
    addAndMakeVisible(prevPresetBtn);
    addAndMakeVisible(presetBox);
    addAndMakeVisible(nextPresetBtn);
    addAndMakeVisible(savePresetBtn);
    addAndMakeVisible(saveAsPresetBtn);
    addAndMakeVisible(deletePresetBtn);
    addAndMakeVisible(importPresetsBtn);
    addAndMakeVisible(gainDial);
    addAndMakeVisible(singleBtn);
}

// =============================================================================
// applySynthStyle — style sub-components with synth V4 palette (accent-driven)
// Called by the owner after construction (so the parent L&F is already set)
// =============================================================================
void HeaderComponent::applySynthStyle(const juce::Colour& accent)
{
    accent_ = accent;

    presetSearch.setTextToShowWhenEmpty("Search...", synthcol::textDim);
    presetSearch.setJustification(juce::Justification::centredLeft);
    presetSearch.setBorder(juce::BorderSize<int>(0));
    presetSearch.setIndents(10, 0);
    presetSearch.setTooltip("Filter presets");
    presetSearch.setColour(juce::TextEditor::backgroundColourId, synthcol::surfHi);
    presetSearch.setColour(juce::TextEditor::outlineColourId, synthcol::ink);
    presetSearch.setColour(juce::TextEditor::focusedOutlineColourId, accent_.withAlpha(0.42f));
    presetSearch.setColour(juce::TextEditor::textColourId, synthcol::text);
    presetSearch.setColour(juce::TextEditor::highlightColourId, accent_.withAlpha(0.22f));
    presetSearch.setColour(juce::TextEditor::highlightedTextColourId, synthcol::text);

    prevPresetBtn.setButtonText(juce::String::charToString(0x25C0));
    prevPresetBtn.setColour(juce::TextButton::buttonColourId, synthcol::surfHi);
    nextPresetBtn.setButtonText(juce::String::charToString(0x25B6));
    nextPresetBtn.setColour(juce::TextButton::buttonColourId, synthcol::surfHi);

    savePresetBtn.setButtonText("Save");
    saveAsPresetBtn.setButtonText("Save As");
    deletePresetBtn.setButtonText("Delete");
    importPresetsBtn.setButtonText("Import");

    singleBtn.setButtonText("SINGLE");
    singleBtn.setClickingTogglesState(true);
}

// =============================================================================
// apply_v5_style_to_editor — V5 theme for TextEditor
// =============================================================================
void HeaderComponent::apply_v5_style_to_editor(juce::TextEditor& ed) const
{
    ed.setColour(juce::TextEditor::backgroundColourId,
                 UIThemeV5::recessMid().withAlpha(0.6f));
    ed.setColour(juce::TextEditor::textColourId, UIThemeV5::textMain());
    ed.setColour(juce::TextEditor::highlightColourId,
                 UIThemeV5::accent().withAlpha(0.25f));
    ed.setColour(juce::TextEditor::outlineColourId,
                 juce::Colour(0xff303742).withAlpha(0.5f));
    ed.setFont(UIThemeV5::labelFont());
}
