// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Visualization/TGVisualizationArrowRowWidget.h"

#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/Theme/TGUiTheme.h"

void UTGVisualizationArrowRowWidget::InitializeArrowRow(
    ATGSimulationPlaybackActor* InPlaybackActor,
    const FTGVisualizationArrowInfo& InArrowInfo)
{
    PlaybackActor = InPlaybackActor;
    ArrowId = InArrowInfo.ArrowId;
    ArrowLabel = InArrowInfo.DisplayName;
    ArrowColor = InArrowInfo.Color;
    bInitialVisibility = InArrowInfo.bVisible;
    RefreshDesignerWidgets();
}

const FString& UTGVisualizationArrowRowWidget::GetArrowLabel() const
{
    return ArrowLabel;
}

void UTGVisualizationArrowRowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    if (ROOT_ArrowRow != nullptr)
    {
        ROOT_ArrowRow->SetMinDesiredHeight(44.0f);
    }
    if (BORDER_RowSurface != nullptr)
    {
        BORDER_RowSurface->SetBrush(TGUiTheme::MakeRoundedBrush(
            Palette.Surface,
            4.0f,
            Palette.Border,
            1.0f));
        BORDER_RowSurface->SetPadding(FMargin(12.0f, 8.0f));
    }
    if (SIZE_ColorSwatch != nullptr)
    {
        SIZE_ColorSwatch->SetWidthOverride(14.0f);
        SIZE_ColorSwatch->SetHeightOverride(14.0f);
    }
    TGUiTheme::ApplyCompactCheckBoxStyle(*CHECK_Visible);
    TGUiTheme::ApplyTextStyle(
        *TXT_ArrowLabel,
        ETGUiTextStyle::FieldLabel,
        Palette.TextPrimary);
    if (UHorizontalBoxSlot* CheckSlot =
            Cast<UHorizontalBoxSlot>(CHECK_Visible->Slot))
    {
        CheckSlot->SetPadding(FMargin(0.0f, 0.0f, 12.0f, 0.0f));
    }
    if (UHorizontalBoxSlot* SwatchSlot =
            SIZE_ColorSwatch != nullptr
                ? Cast<UHorizontalBoxSlot>(SIZE_ColorSwatch->Slot)
                : nullptr)
    {
        SwatchSlot->SetPadding(FMargin(0.0f, 0.0f, 10.0f, 0.0f));
    }
    CHECK_Visible->OnCheckStateChanged.RemoveDynamic(
        this,
        &UTGVisualizationArrowRowWidget::HandleCheckStateChanged);
    CHECK_Visible->OnCheckStateChanged.AddUniqueDynamic(
        this,
        &UTGVisualizationArrowRowWidget::HandleCheckStateChanged);
    RefreshDesignerWidgets();
}

void UTGVisualizationArrowRowWidget::NativeDestruct()
{
    CHECK_Visible->OnCheckStateChanged.RemoveDynamic(
        this,
        &UTGVisualizationArrowRowWidget::HandleCheckStateChanged);
    Super::NativeDestruct();
}

void UTGVisualizationArrowRowWidget::RefreshDesignerWidgets()
{
    if (TXT_ArrowLabel != nullptr)
    {
        TXT_ArrowLabel->SetText(FText::FromString(ArrowLabel));
    }
    if (BORDER_ColorSwatch != nullptr)
    {
        BORDER_ColorSwatch->SetBrushColor(ArrowColor);
    }
    if (CHECK_Visible != nullptr)
    {
        CHECK_Visible->SetIsChecked(bInitialVisibility);
    }
}

void UTGVisualizationArrowRowWidget::HandleCheckStateChanged(
    const bool bChecked)
{
    bInitialVisibility = bChecked;
    if (IsValid(PlaybackActor))
    {
        PlaybackActor->SetVisualizationArrowVisible(ArrowId, bChecked);
    }
}
