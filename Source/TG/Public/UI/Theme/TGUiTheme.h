// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"

class UButton;
class UCheckBox;
class UTextBlock;
struct FButtonStyle;
struct FCheckBoxStyle;
struct FComboBoxStyle;
struct FEditableTextBoxStyle;
struct FSearchBoxStyle;
struct FScrollBarStyle;
struct FScrollBoxStyle;
struct FSlateBrush;
struct FSlateFontInfo;
struct FSliderStyle;
struct FTableRowStyle;

/** Semantic colors shared by native, runtime-created TG user-interface widgets. */
struct TG_API FTGUiPalette final
{
    FLinearColor Canvas;
    FLinearColor Backdrop;
    FLinearColor Header;
    FLinearColor Navigation;
    FLinearColor Panel;
    FLinearColor Surface;
    FLinearColor SurfaceRaised;
    FLinearColor Input;
    FLinearColor Border;
    FLinearColor BorderStrong;

    FLinearColor TextPrimary;
    FLinearColor TextSecondary;
    FLinearColor TextMuted;
    FLinearColor TextDisabled;

    FLinearColor Accent;
    FLinearColor AccentHover;
    FLinearColor AccentPressed;
    FLinearColor AccentSubtle;
    FLinearColor Selection;
    FLinearColor Focus;
    FLinearColor Success;
    FLinearColor Warning;
    FLinearColor Error;
    FLinearColor Info;

    FLinearColor Overlay;
};

/** Font sizes used by the compact engineering-tool hierarchy. */
struct TG_API FTGUiTypography final
{
    int32 Caption = 10;
    int32 FieldLabel = 11;
    int32 Numeric = 11;
    int32 Body = 12;
    int32 Section = 12;
    int32 PanelTitle = 16;
    int32 ScreenTitle = 20;
    int32 ProductTitle = 30;
};

/** Spacing scale used by native widgets. Values are Slate units. */
struct TG_API FTGUiSpacing final
{
    float ExtraSmall = 4.0f;
    float Small = 8.0f;
    float Regular = 12.0f;
    float Large = 16.0f;
    float Section = 24.0f;
    float Wide = 32.0f;
    float ExtraWide = 48.0f;
};

enum class ETGUiTextStyle : uint8
{
    Caption,
    FieldLabel,
    Numeric,
    Body,
    BodyStrong,
    Section,
    PanelTitle,
    ScreenTitle,
    ProductTitle
};

enum class ETGUiButtonStyle : uint8
{
    Primary,
    Secondary,
    Quiet,
	Tab,
    Destructive
};

namespace TGUiTheme
{
    /** Canonical linear-space tokens for the TG application interface. */
    TG_API const FTGUiPalette& GetPalette();

    TG_API const FTGUiTypography& GetTypography();

    TG_API const FTGUiSpacing& GetSpacing();

    /** Resolves the shared Slate font size and weight for native widgets. */
    TG_API FSlateFontInfo GetSlateFont(ETGUiTextStyle Style);

    /** Applies the shared size, weight, and semantic color to a text block. */
    TG_API void ApplyTextStyle(
        UTextBlock& Text,
        ETGUiTextStyle Style,
        const FLinearColor& Color);

    /** Asset-free rounded brush used by both runtime controls and exporters. */
    TG_API FSlateBrush MakeRoundedBrush(
        const FLinearColor& FillColor,
        float Radius = 4.0f,
        const FLinearColor& OutlineColor = FLinearColor::Transparent,
        float OutlineWidth = 0.0f);

    TG_API FButtonStyle MakeButtonStyle(ETGUiButtonStyle Variant);

    TG_API FEditableTextBoxStyle MakeEditableTextBoxStyle();

    TG_API FSearchBoxStyle MakeSearchBoxStyle();

    TG_API FComboBoxStyle MakeComboBoxStyle();

    TG_API FCheckBoxStyle MakeCheckBoxStyle();

    TG_API FTableRowStyle MakeTableRowStyle();

    TG_API FScrollBarStyle MakeScrollBarStyle();

    TG_API FScrollBoxStyle MakeScrollBoxStyle();

    TG_API FSliderStyle MakeSliderStyle();

    /** Persistent styles for Slate arguments that retain a style pointer. */
    TG_API const FButtonStyle& GetButtonStyle(ETGUiButtonStyle Variant);

    TG_API const FEditableTextBoxStyle& GetEditableTextBoxStyle();

    TG_API const FSearchBoxStyle& GetSearchBoxStyle();

    TG_API const FCheckBoxStyle& GetCheckBoxStyle();

    TG_API const FScrollBarStyle& GetScrollBarStyle();

    TG_API const FScrollBoxStyle& GetScrollBoxStyle();

    TG_API const FSliderStyle& GetSliderStyle();

    /** Removes the stock grey face while retaining clear hover/press feedback. */
    TG_API void ApplyRowButtonStyle(UButton& Button);

    /** Tints the engine checkbox glyphs without requiring a project UI asset. */
    TG_API void ApplyCompactCheckBoxStyle(UCheckBox& CheckBox);

    /** Returns a canonical semantic color at the standard 14% wash opacity. */
    TG_API FLinearColor MakeSemanticWash(const FLinearColor& SemanticColor);

    /** WCAG contrast ratio for two opaque linear colors. Used by theme audits. */
    TG_API double ContrastRatio(
        const FLinearColor& First,
        const FLinearColor& Second);
}
