// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Theme/TGUiTheme.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "Styling/UMGCoreStyle.h"

namespace TGUiThemePrivate
{
    int32 ResolveFontSize(const ETGUiTextStyle Style)
    {
        const FTGUiTypography& Typography = TGUiTheme::GetTypography();
        switch (Style)
        {
        case ETGUiTextStyle::Caption:
            return Typography.Caption;
        case ETGUiTextStyle::FieldLabel:
            return Typography.FieldLabel;
        case ETGUiTextStyle::Numeric:
            return Typography.Numeric;
        case ETGUiTextStyle::Body:
        case ETGUiTextStyle::BodyStrong:
            return Typography.Body;
        case ETGUiTextStyle::Section:
            return Typography.Section;
        case ETGUiTextStyle::PanelTitle:
            return Typography.PanelTitle;
        case ETGUiTextStyle::ScreenTitle:
            return Typography.ScreenTitle;
        case ETGUiTextStyle::ProductTitle:
            return Typography.ProductTitle;
        default:
            return Typography.Body;
        }
    }

    bool IsStrong(const ETGUiTextStyle Style)
    {
        return Style == ETGUiTextStyle::BodyStrong
            || Style == ETGUiTextStyle::Section
            || Style == ETGUiTextStyle::PanelTitle
            || Style == ETGUiTextStyle::ScreenTitle;
    }

    bool IsMajorHeading(const ETGUiTextStyle Style)
    {
        return Style == ETGUiTextStyle::Section
            || Style == ETGUiTextStyle::PanelTitle
            || Style == ETGUiTextStyle::ScreenTitle
            || Style == ETGUiTextStyle::ProductTitle;
    }

    FName ResolveTypeface(const ETGUiTextStyle Style)
    {
        if (Style == ETGUiTextStyle::Numeric)
        {
            return FName(TEXT("Mono"));
        }
        if (Style == ETGUiTextStyle::ProductTitle)
        {
            return FName(TEXT("Light"));
        }
        return IsStrong(Style)
            ? FName(TEXT("Bold"))
            : FName(TEXT("Regular"));
    }

    double RelativeLuminance(const FLinearColor& Color)
    {
        return 0.2126 * FMath::Clamp(static_cast<double>(Color.R), 0.0, 1.0)
            + 0.7152 * FMath::Clamp(static_cast<double>(Color.G), 0.0, 1.0)
            + 0.0722 * FMath::Clamp(static_cast<double>(Color.B), 0.0, 1.0);
    }
}

const FTGUiPalette& TGUiTheme::GetPalette()
{
    static const FTGUiPalette Palette = []
    {
        FTGUiPalette Result;

        // Canonical deep blue-black palette used by the configuration workflow.
        Result.Canvas = FLinearColor(0.002428f, 0.005605f, 0.011612f, 1.0f);
        Result.Backdrop = FLinearColor(0.003347f, 0.007499f, 0.014444f, 1.0f);
        Result.Header = Result.Backdrop;
        Result.Navigation = FLinearColor(
            0.004025f, 0.010960f, 0.023153f, 1.0f);
        Result.Panel = Result.Navigation;
        Result.Surface = FLinearColor(0.006512f, 0.021219f, 0.042311f, 1.0f);
        Result.SurfaceRaised = FLinearColor(
            0.010960f, 0.034340f, 0.064803f, 1.0f);
        Result.Input = FLinearColor(0.003035f, 0.008568f, 0.016807f, 1.0f);
        Result.Border = FLinearColor(0.022174f, 0.064803f, 0.109462f, 1.0f);
        Result.BorderStrong = FLinearColor(
            0.061246f, 0.144128f, 0.208637f, 1.0f);

        Result.Accent = FLinearColor(0.030713f, 0.564712f, 0.806952f, 1.0f);
        Result.AccentHover = FLinearColor(0.130136f, 0.693872f, 0.879622f, 1.0f);
        Result.AccentPressed = FLinearColor(0.008568f, 0.313989f, 0.502886f, 1.0f);
        Result.AccentSubtle = FLinearColor(
            0.006049f, 0.045186f, 0.070360f, 1.0f);
        Result.Selection = FLinearColor(
            0.008568f, 0.063010f, 0.122139f, 1.0f);
        Result.Focus = FLinearColor(0.177888f, 0.723055f, 0.887923f, 1.0f);

        Result.TextPrimary = FLinearColor(0.806952f, 0.879622f, 0.913099f, 1.0f);
        Result.TextSecondary = FLinearColor(
            0.401978f, 0.502886f, 0.571125f, 1.0f);
        Result.TextMuted = FLinearColor(
            0.194618f, 0.278894f, 0.332452f, 1.0f);
        Result.TextDisabled = FLinearColor(0.082283f, 0.124772f, 0.152926f, 1.0f);

        Result.Success = FLinearColor(0.056128f, 0.558340f, 0.341914f, 1.0f);
        Result.Warning = FLinearColor(0.871367f, 0.485150f, 0.068478f, 1.0f);
        Result.Error = FLinearColor(0.863157f, 0.144128f, 0.144128f, 1.0f);
        Result.Info = FLinearColor(0.135633f, 0.386429f, 1.000000f, 1.0f);
        Result.Overlay = FLinearColor(0.001518f, 0.003035f, 0.005182f, 0.72f);

        return Result;
    }();

    return Palette;
}

const FTGUiTypography& TGUiTheme::GetTypography()
{
    static const FTGUiTypography Typography;
    return Typography;
}

const FTGUiSpacing& TGUiTheme::GetSpacing()
{
    static const FTGUiSpacing Spacing;
    return Spacing;
}

FSlateFontInfo TGUiTheme::GetSlateFont(const ETGUiTextStyle Style)
{
    return FCoreStyle::GetDefaultFontStyle(
        TGUiThemePrivate::ResolveTypeface(Style),
        TGUiThemePrivate::ResolveFontSize(Style));
}

void TGUiTheme::ApplyTextStyle(
    UTextBlock& Text,
    const ETGUiTextStyle Style,
    const FLinearColor& Color)
{
    Text.SetFont(GetSlateFont(Style));
    if (TGUiThemePrivate::IsMajorHeading(Style))
    {
        Text.SetText(Text.GetText().ToUpper());
        Text.SetColorAndOpacity(FSlateColor(GetPalette().TextPrimary));
        return;
    }
    Text.SetColorAndOpacity(FSlateColor(Color));
}

FSlateBrush TGUiTheme::MakeRoundedBrush(
    const FLinearColor& FillColor,
    const float Radius,
    const FLinearColor& OutlineColor,
    const float OutlineWidth)
{
    return FSlateRoundedBoxBrush(
        FillColor,
        Radius,
        OutlineColor,
        OutlineWidth);
}

FButtonStyle TGUiTheme::MakeButtonStyle(const ETGUiButtonStyle Variant)
{
    const FTGUiPalette& Palette = GetPalette();

    FLinearColor Normal;
    FLinearColor Hovered;
    FLinearColor Pressed;
    FLinearColor Foreground;
    FLinearColor Outline = FLinearColor::Transparent;

    switch (Variant)
    {
    case ETGUiButtonStyle::Primary:
        Normal = Palette.Accent;
        Hovered = Palette.AccentHover;
        Pressed = Palette.AccentPressed;
        Foreground = Palette.Canvas;
        break;
    case ETGUiButtonStyle::Secondary:
        Normal = Palette.Surface;
        Hovered = Palette.SurfaceRaised;
        Pressed = Palette.AccentPressed;
        Foreground = Palette.TextPrimary;
        Outline = Palette.Border;
        break;
    case ETGUiButtonStyle::Destructive:
        Normal = Palette.Error;
        Hovered = Palette.Error.CopyWithNewOpacity(0.88f);
        Pressed = Palette.Error.CopyWithNewOpacity(0.72f);
        Foreground = Palette.Canvas;
        break;
    case ETGUiButtonStyle::Quiet:
    default:
        Normal = FLinearColor::Transparent;
        Hovered = Palette.AccentSubtle;
        Pressed = Palette.Selection;
        Foreground = Palette.TextSecondary;
        break;
    }

    FButtonStyle Style;
    Style.SetNormal(MakeRoundedBrush(Normal, 4.0f, Outline,
            Variant == ETGUiButtonStyle::Secondary ? 1.0f : 0.0f))
        .SetHovered(MakeRoundedBrush(Hovered, 4.0f,
            Variant == ETGUiButtonStyle::Secondary
                ? Palette.Accent : FLinearColor::Transparent,
            Variant == ETGUiButtonStyle::Secondary ? 1.0f : 0.0f))
        .SetPressed(MakeRoundedBrush(Pressed, 4.0f))
        .SetDisabled(MakeRoundedBrush(Palette.Surface, 4.0f))
        .SetNormalForeground(FSlateColor(Foreground))
        .SetHoveredForeground(FSlateColor(Foreground))
        .SetPressedForeground(FSlateColor(Foreground))
        .SetDisabledForeground(FSlateColor(Palette.TextDisabled))
        .SetNormalPadding(FMargin(14.0f, 7.0f))
        .SetPressedPadding(FMargin(14.0f, 7.0f));
    return Style;
}

FScrollBarStyle TGUiTheme::MakeScrollBarStyle()
{
    // The Gravity Sources asset overrides only its vertical background,
    // thumbs, and thickness. Preserve Unreal's inherited UMG top/bottom slot
    // images because those images form the thin scrollbar trail.
    const FLinearColor RestingThumb(
        0.034340f, 0.054480f, 0.080220f, 1.0f);
    const FLinearColor ActiveThumb(
        0.070360f, 0.479320f, 0.686685f, 1.0f);
    const FSlateBrush TransparentBackground = MakeRoundedBrush(
        FLinearColor::Transparent,
        0.0f);

    FScrollBarStyle Style = FUMGCoreStyle::Get()
        .GetWidgetStyle<FScrollBarStyle>("ScrollBar");
    Style.UnlinkColors();
    Style.SetVerticalBackgroundImage(TransparentBackground)
        .SetNormalThumbImage(MakeRoundedBrush(RestingThumb, 0.0f))
        .SetHoveredThumbImage(MakeRoundedBrush(ActiveThumb, 0.0f))
        .SetDraggedThumbImage(MakeRoundedBrush(ActiveThumb, 0.0f))
        .SetThickness(8.0f);
    return Style;
}

FScrollBoxStyle TGUiTheme::MakeScrollBoxStyle()
{
    const FSlateBrush Empty = MakeRoundedBrush(
        FLinearColor::Transparent,
        0.0f);
    FScrollBoxStyle Style;
    Style.SetBarThickness(8.0f)
        .SetTopShadowBrush(Empty)
        .SetBottomShadowBrush(Empty)
        .SetLeftShadowBrush(Empty)
        .SetRightShadowBrush(Empty);
    return Style;
}

FSliderStyle TGUiTheme::MakeSliderStyle()
{
    const FTGUiPalette& Palette = GetPalette();
    FSlateBrush NormalThumb = MakeRoundedBrush(
        Palette.Accent,
        7.0f,
        Palette.Canvas,
        1.0f);
    NormalThumb.SetImageSize(FVector2f(14.0f, 14.0f));
    FSlateBrush HoveredThumb = MakeRoundedBrush(
        Palette.AccentHover,
        8.0f,
        Palette.Canvas,
        1.0f);
    HoveredThumb.SetImageSize(FVector2f(16.0f, 16.0f));
    FSlateBrush DisabledThumb = MakeRoundedBrush(
        Palette.TextDisabled,
        7.0f);
    DisabledThumb.SetImageSize(FVector2f(14.0f, 14.0f));

    FSliderStyle Style;
    Style.SetNormalBarImage(MakeRoundedBrush(Palette.Border, 2.0f))
        .SetHoveredBarImage(MakeRoundedBrush(Palette.Accent, 2.0f))
        .SetDisabledBarImage(MakeRoundedBrush(
            Palette.TextDisabled,
            2.0f))
        .SetNormalThumbImage(NormalThumb)
        .SetHoveredThumbImage(HoveredThumb)
        .SetDisabledThumbImage(DisabledThumb)
        .SetBarThickness(4.0f);
    return Style;
}

FEditableTextBoxStyle TGUiTheme::MakeEditableTextBoxStyle()
{
    const FTGUiPalette& Palette = GetPalette();
    FEditableTextBoxStyle Style;
    Style.SetBackgroundImageNormal(
            MakeRoundedBrush(Palette.Input, 4.0f, Palette.Border, 1.0f))
        .SetBackgroundImageHovered(
            MakeRoundedBrush(Palette.Input, 4.0f, Palette.Accent, 1.0f))
        .SetBackgroundImageFocused(
            MakeRoundedBrush(Palette.Input, 4.0f, Palette.Focus, 2.0f))
        .SetBackgroundImageReadOnly(
            MakeRoundedBrush(Palette.Panel, 4.0f, Palette.Border, 1.0f))
        .SetForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetFocusedForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetReadOnlyForegroundColor(FSlateColor(Palette.TextDisabled))
        .SetFont(GetSlateFont(ETGUiTextStyle::FieldLabel))
        .SetPadding(FMargin(12.0f, 8.0f))
        .SetScrollBarStyle(MakeScrollBarStyle());
    return Style;
}

FSearchBoxStyle TGUiTheme::MakeSearchBoxStyle()
{
    const FTGUiPalette& Palette = GetPalette();
    FSearchBoxStyle Style =
        FCoreStyle::Get().GetWidgetStyle<FSearchBoxStyle>(TEXT("SearchBox"));
    FSlateBrush Glass = Style.GlassImage;
    FSlateBrush Clear = Style.ClearImage;
    FSlateBrush Up = Style.UpArrowImage;
    FSlateBrush Down = Style.DownArrowImage;
    Glass.TintColor = FSlateColor(Palette.TextSecondary);
    Clear.TintColor = FSlateColor(Palette.TextSecondary);
    Up.TintColor = FSlateColor(Palette.TextSecondary);
    Down.TintColor = FSlateColor(Palette.TextSecondary);
    Style.SetTextBoxStyle(MakeEditableTextBoxStyle())
        .SetActiveFont(GetSlateFont(ETGUiTextStyle::FieldLabel))
        .SetGlassImage(Glass)
        .SetClearImage(Clear)
        .SetUpArrowImage(Up)
        .SetDownArrowImage(Down)
        .SetImagePadding(FMargin(8.0f, 0.0f, 10.0f, 0.0f))
        .SetImageSizeOverride(FVector2D(14.0f, 14.0f));
    return Style;
}

FComboBoxStyle TGUiTheme::MakeComboBoxStyle()
{
    const FComboBoxStyle& CoreComboBoxStyle =
        FCoreStyle::Get().GetWidgetStyle<FComboBoxStyle>(TEXT("ComboBox"));

    // These values intentionally mirror the approved live
    // WBP_Config_Atmosphere ComboBoxes. Do not derive them from the broader
    // application palette: the Atmosphere control is the visual reference.
    const FLinearColor ButtonNormal(
        0.012286f, 0.018500f, 0.027321f, 1.0f);
    const FLinearColor ButtonHovered(
        0.019382f, 0.030713f, 0.045186f, 1.0f);
    const FLinearColor ButtonPressed(
        0.008568f, 0.063010f, 0.122139f, 1.0f);
    const FLinearColor Border(
        0.034340f, 0.054480f, 0.080220f, 1.0f);
    const FLinearColor Text(
        0.775822f, 0.814847f, 0.854993f, 1.0f);
    const FLinearColor TextDisabled(
        0.082283f, 0.124772f, 0.152926f, 1.0f);
    const FLinearColor Arrow(
        0.396755f, 0.445201f, 0.491021f, 1.0f);

    FComboButtonStyle ComboButton = CoreComboBoxStyle.ComboButtonStyle;
    FButtonStyle ButtonStyle = ComboButton.ButtonStyle;
    ButtonStyle.Normal.TintColor = FSlateColor(ButtonNormal);
    ButtonStyle.Normal.SetImageSize(FVector2f::ZeroVector);
    ButtonStyle.Normal.OutlineSettings.Color = FSlateColor(Border);
    ButtonStyle.Normal.OutlineSettings.bUseBrushTransparency = false;
    ButtonStyle.Hovered.TintColor = FSlateColor(ButtonHovered);
    ButtonStyle.Hovered.SetImageSize(FVector2f::ZeroVector);
    ButtonStyle.Hovered.OutlineSettings.Color = FSlateColor(Border);
    ButtonStyle.Hovered.OutlineSettings.bUseBrushTransparency = false;
    ButtonStyle.Pressed.TintColor = FSlateColor(ButtonPressed);
    ButtonStyle.Pressed.SetImageSize(FVector2f::ZeroVector);
    ButtonStyle.Pressed.OutlineSettings.Color =
        FSlateColor(FLinearColor::Transparent);
    ButtonStyle.Pressed.OutlineSettings.Width = 0.0f;
    ButtonStyle.Pressed.OutlineSettings.bUseBrushTransparency = false;
    ButtonStyle.Disabled = MakeRoundedBrush(ButtonNormal, 4.0f);
    ButtonStyle.SetNormalForeground(FSlateColor(Text))
        .SetHoveredForeground(FSlateColor(Text))
        .SetPressedForeground(FSlateColor(Text))
        .SetDisabledForeground(FSlateColor(TextDisabled))
        .SetNormalPadding(FMargin(0.0f, 7.0f))
        .SetPressedPadding(FMargin(0.0f, 7.0f));

    FSlateBrush DownArrow = ComboButton.DownArrowImage;
    DownArrow.TintColor = FSlateColor(Arrow);
    DownArrow.SetImageSize(FVector2f(16.0f, 16.0f));
    ComboButton.SetButtonStyle(ButtonStyle)
        .SetDownArrowImage(DownArrow)
        .SetMenuBorderBrush(
            MakeRoundedBrush(ButtonHovered, 4.0f, Border, 1.0f))
        .SetMenuBorderPadding(FMargin(1.0f))
        .SetContentPadding(FMargin(10.0f, 7.0f))
        .SetDownArrowPadding(FMargin(8.0f, 0.0f, 4.0f, 0.0f))
        .SetDownArrowAlignment(VAlign_Center)
        .SetShadowOffset(FVector2f::ZeroVector)
        .SetShadowColorAndOpacity(FLinearColor::Transparent);

    FComboBoxStyle Style = CoreComboBoxStyle;
    Style.SetComboButtonStyle(ComboButton)
        .SetContentPadding(FMargin(10.0f, 7.0f))
        .SetMenuRowPadding(FMargin(10.0f, 8.0f));
    return Style;
}

FCheckBoxStyle TGUiTheme::MakeCheckBoxStyle()
{
    const FTGUiPalette& Palette = GetPalette();
    const FCheckBoxStyle& CoreCheckBoxStyle =
        FCoreStyle::Get().GetWidgetStyle<FCheckBoxStyle>(TEXT("Checkbox"));
    constexpr float IndicatorSize = 18.0f;
    const auto MakeBackgroundBrush = [IndicatorSize](
        const FLinearColor& FillColor,
        const FLinearColor& OutlineColor,
        const float OutlineWidth)
    {
        FSlateBrush Brush = MakeRoundedBrush(
            FillColor,
            3.0f,
            OutlineColor,
            OutlineWidth);
        Brush.SetImageSize(FVector2f(IndicatorSize, IndicatorSize));
        return Brush;
    };
    const auto MakeIndicatorBrush = [IndicatorSize](
        const FSlateBrush& Source,
        const FLinearColor& Tint)
    {
        FSlateBrush Brush = Source;
        Brush.TintColor = FSlateColor(Tint);
        Brush.SetImageSize(FVector2f(IndicatorSize, IndicatorSize));
        return Brush;
    };

    FCheckBoxStyle Style = CoreCheckBoxStyle;
    Style.SetUncheckedImage(
            MakeIndicatorBrush(
                CoreCheckBoxStyle.UncheckedImage,
                Palette.Border))
        .SetUncheckedHoveredImage(
            MakeIndicatorBrush(
                CoreCheckBoxStyle.UncheckedHoveredImage,
                Palette.AccentHover))
        .SetUncheckedPressedImage(
            MakeIndicatorBrush(
                CoreCheckBoxStyle.UncheckedPressedImage,
                Palette.AccentPressed))
        .SetCheckedImage(MakeIndicatorBrush(
            CoreCheckBoxStyle.CheckedImage,
            Palette.Accent))
        .SetCheckedHoveredImage(MakeIndicatorBrush(
            CoreCheckBoxStyle.CheckedHoveredImage,
            Palette.AccentHover))
        .SetCheckedPressedImage(MakeIndicatorBrush(
            CoreCheckBoxStyle.CheckedPressedImage,
            Palette.AccentPressed))
        .SetUndeterminedImage(MakeIndicatorBrush(
            CoreCheckBoxStyle.UndeterminedImage,
            Palette.Warning))
        .SetUndeterminedHoveredImage(MakeIndicatorBrush(
            CoreCheckBoxStyle.UndeterminedHoveredImage,
            Palette.Warning))
        .SetUndeterminedPressedImage(MakeIndicatorBrush(
            CoreCheckBoxStyle.UndeterminedPressedImage,
            Palette.Warning))
        .SetBackgroundImage(MakeBackgroundBrush(
            Palette.Input, Palette.Border, 1.0f))
        .SetBackgroundHoveredImage(MakeBackgroundBrush(
            Palette.Input, Palette.AccentHover, 1.0f))
        .SetBackgroundPressedImage(MakeBackgroundBrush(
            Palette.Input, Palette.AccentPressed, 2.0f))
        .SetForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetHoveredForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetPressedForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetCheckedForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetCheckedHoveredForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetCheckedPressedForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetUndeterminedForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetPadding(FMargin(0.0f));
    return Style;
}

FTableRowStyle TGUiTheme::MakeTableRowStyle()
{
    const FLinearColor RowBackground(
        0.012286f, 0.018500f, 0.027321f, 1.0f);
    const FLinearColor RowHovered(
        0.019382f, 0.030713f, 0.045186f, 1.0f);
    const FLinearColor RowSelected(
        0.008568f, 0.063010f, 0.122139f, 1.0f);
    const FLinearColor Focus(
        0.177888f, 0.723055f, 0.887923f, 1.0f);
    const FLinearColor Text(
        0.775822f, 0.814847f, 0.854993f, 1.0f);

    FTableRowStyle Style;
    Style.SetEvenRowBackgroundBrush(MakeRoundedBrush(RowBackground, 0.0f))
        .SetOddRowBackgroundBrush(MakeRoundedBrush(RowBackground, 0.0f))
        .SetEvenRowBackgroundHoveredBrush(
            MakeRoundedBrush(RowHovered, 0.0f))
        .SetOddRowBackgroundHoveredBrush(
            MakeRoundedBrush(RowHovered, 0.0f))
        .SetActiveBrush(MakeRoundedBrush(RowSelected, 0.0f))
        .SetInactiveBrush(MakeRoundedBrush(RowSelected, 0.0f))
        .SetActiveHoveredBrush(MakeRoundedBrush(RowSelected, 0.0f))
        .SetInactiveHoveredBrush(MakeRoundedBrush(RowSelected, 0.0f))
        .SetSelectorFocusedBrush(
            MakeRoundedBrush(RowSelected, 0.0f, Focus, 2.0f))
        .SetTextColor(FSlateColor(Text))
        .SetSelectedTextColor(FSlateColor(Text));
    return Style;
}

const FButtonStyle& TGUiTheme::GetButtonStyle(
    const ETGUiButtonStyle Variant)
{
    static const FButtonStyle Primary =
        MakeButtonStyle(ETGUiButtonStyle::Primary);
    static const FButtonStyle Secondary =
        MakeButtonStyle(ETGUiButtonStyle::Secondary);
    static const FButtonStyle Quiet =
        MakeButtonStyle(ETGUiButtonStyle::Quiet);
    static const FButtonStyle Tab = []()
    {
        FButtonStyle Style = Quiet;
        // Corner order: TopLeft, TopRight, BottomRight, BottomLeft.
        // Start from the exact cached Quiet style and change ONLY
        // the hovered tab corner radii.
        Style.Hovered.OutlineSettings.CornerRadii =
            FVector4(20.0f, 0.0f, 0.0f, 0.0f);
        return Style;
    }();
    static const FButtonStyle Destructive =
        MakeButtonStyle(ETGUiButtonStyle::Destructive);
    switch (Variant)
    {
    case ETGUiButtonStyle::Primary:
        return Primary;
    case ETGUiButtonStyle::Secondary:
        return Secondary;
    case ETGUiButtonStyle::Destructive:
        return Destructive;
    case ETGUiButtonStyle::Tab:
        return Tab;
    case ETGUiButtonStyle::Quiet:
    default:
        return Quiet;
    }
}

const FEditableTextBoxStyle& TGUiTheme::GetEditableTextBoxStyle()
{
    static const FEditableTextBoxStyle Style = MakeEditableTextBoxStyle();
    return Style;
}

const FSearchBoxStyle& TGUiTheme::GetSearchBoxStyle()
{
    static const FSearchBoxStyle Style = MakeSearchBoxStyle();
    return Style;
}

const FCheckBoxStyle& TGUiTheme::GetCheckBoxStyle()
{
    static const FCheckBoxStyle Style = MakeCheckBoxStyle();
    return Style;
}

const FScrollBarStyle& TGUiTheme::GetScrollBarStyle()
{
    static const FScrollBarStyle Style = MakeScrollBarStyle();
    return Style;
}

const FScrollBoxStyle& TGUiTheme::GetScrollBoxStyle()
{
    static const FScrollBoxStyle Style = MakeScrollBoxStyle();
    return Style;
}

const FSliderStyle& TGUiTheme::GetSliderStyle()
{
    static const FSliderStyle Style = MakeSliderStyle();
    return Style;
}

void TGUiTheme::ApplyRowButtonStyle(UButton& Button)
{
    FButtonStyle Style = MakeButtonStyle(ETGUiButtonStyle::Quiet);
    Style.SetNormalPadding(FMargin(0.0f))
        .SetPressedPadding(FMargin(0.0f));
    Button.SetStyle(Style);
}

void TGUiTheme::ApplyCompactCheckBoxStyle(UCheckBox& CheckBox)
{
    CheckBox.SetWidgetStyle(MakeCheckBoxStyle());
}

FLinearColor TGUiTheme::MakeSemanticWash(
    const FLinearColor& SemanticColor)
{
    return FLinearColor(
        SemanticColor.R,
        SemanticColor.G,
        SemanticColor.B,
        0.14f);
}

double TGUiTheme::ContrastRatio(
    const FLinearColor& First,
    const FLinearColor& Second)
{
    const double FirstLuminance =
        TGUiThemePrivate::RelativeLuminance(First);
    const double SecondLuminance =
        TGUiThemePrivate::RelativeLuminance(Second);
    const double Brighter = FMath::Max(FirstLuminance, SecondLuminance);
    const double Darker = FMath::Min(FirstLuminance, SecondLuminance);
    return (Brighter + 0.05) / (Darker + 0.05);
}
