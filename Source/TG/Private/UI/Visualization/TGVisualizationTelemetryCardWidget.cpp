// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Visualization/TGVisualizationTelemetryCardWidget.h"

#include "Components/Border.h"
#include "Components/TextBlock.h"
#include "UI/Theme/TGUiTheme.h"

void UTGVisualizationTelemetryCardWidget::SetTelemetryValue(
    const FString& InLabel,
    const FString& InValue,
    const FLinearColor& InAccentColor)
{
    TelemetryLabel = InLabel;
    TelemetryValue = InValue;
    AccentColor = InAccentColor;
    RefreshDesignerWidgets();
}

void UTGVisualizationTelemetryCardWidget::NativeConstruct()
{
    Super::NativeConstruct();
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    if (BORDER_TelemetryCard != nullptr)
    {
        BORDER_TelemetryCard->SetBrush(TGUiTheme::MakeRoundedBrush(
            Palette.Surface,
            5.0f,
            Palette.Border,
            1.0f));
    }
    TGUiTheme::ApplyTextStyle(
        *TXT_TelemetryLabel,
        ETGUiTextStyle::FieldLabel,
        Palette.TextSecondary);
    TGUiTheme::ApplyTextStyle(
        *TXT_TelemetryValue,
        ETGUiTextStyle::Numeric,
        Palette.TextPrimary);
    RefreshDesignerWidgets();
}

void UTGVisualizationTelemetryCardWidget::RefreshDesignerWidgets()
{
    if (BORDER_TelemetryAccent != nullptr)
    {
        BORDER_TelemetryAccent->SetBrushColor(AccentColor);
    }
    if (TXT_TelemetryLabel != nullptr)
    {
        TXT_TelemetryLabel->SetText(FText::FromString(TelemetryLabel));
    }
    if (TXT_TelemetryValue != nullptr)
    {
        TXT_TelemetryValue->SetText(FText::FromString(TelemetryValue));
    }
}
