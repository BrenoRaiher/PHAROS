// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/TGExecutionTimeLimitDialogWidget.h"

#include "Engine/GameInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "UI/Theme/TGUiTheme.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace TGExecutionTimeLimitDialogPrivate
{
    const FSlateBrush& BackdropBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            FLinearColor::White,
            0.0f);
        return Brush;
    }

    const FSlateBrush& DialogBrush()
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette.Panel,
            4.0f,
            Palette.BorderStrong,
            1.0f);
        return Brush;
    }
}

void UTGExecutionTimeLimitDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);

    if (TimeoutInput.IsValid())
    {
        FSlateApplication::Get().SetKeyboardFocus(
            TimeoutInput,
            EFocusCause::SetDirectly);
        TimeoutInput->SelectAllText();
    }
}

FReply UTGExecutionTimeLimitDialogWidget::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        CloseDialog();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

TSharedRef<SWidget> UTGExecutionTimeLimitDialogWidget::RebuildWidget()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();

    double CurrentLimitSeconds = 300.0;
    if (const UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem())
    {
        const double StoredLimit =
            Subsystem->GetCurrentScenarioMaximumWallClockRuntimeSeconds();
        if (FMath::IsFinite(StoredLimit) && StoredLimit > 0.0)
        {
            CurrentLimitSeconds = StoredLimit;
        }
    }

    RootSlateWidget =
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(
                &TGExecutionTimeLimitDialogPrivate::BackdropBrush())
            .BorderBackgroundColor(Palette.Overlay)
            .Padding(0.0f)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(Spacing.Section)
        [
            SNew(SBox)
            .WidthOverride(460.0f)
            [
                SNew(SBorder)
                .BorderImage(
                    &TGExecutionTimeLimitDialogPrivate::DialogBrush())
                .Padding(Spacing.Wide)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(
                            TEXT("LIMIT EXECUTION TIME")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::PanelTitle))
                        .ColorAndOpacity(Palette.TextPrimary)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Regular, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(
                            "Set the maximum real-world time allowed for a "
                            "simulation to complete. This safety limit stops "
                            "an unresponsive custom controller or another "
                            "non-terminating configuration from running "
                            "indefinitely.")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::Body))
                        .ColorAndOpacity(Palette.TextSecondary)
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(
                            TEXT("Maximum Runtime [s]")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::FieldLabel))
                        .ColorAndOpacity(Palette.TextSecondary)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Small, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .HeightOverride(38.0f)
                        [
                            SAssignNew(TimeoutInput, SEditableTextBox)
                            .Style(&TGUiTheme::GetEditableTextBoxStyle())
                            .Text(FText::FromString(
                                FString::SanitizeFloat(
                                    CurrentLimitSeconds,
                                    0)))
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::Numeric))
                            .SelectAllTextWhenFocused(true)
                            .SelectAllTextOnCommit(true)
                            .OnTextCommitted(FOnTextCommitted::CreateUObject(
                                this,
                                &UTGExecutionTimeLimitDialogWidget::
                                    HandleTextCommitted))
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Small, 0.0f, 0.0f)
                    [
                        SAssignNew(ErrorText, STextBlock)
                        .Text(FText::GetEmpty())
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::Caption))
                        .ColorAndOpacity(Palette.Error)
                        .AutoWrapText(true)
                        .Visibility(EVisibility::Collapsed)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        [
                            SNew(SBox)
                            .WidthOverride(112.0f)
                            .HeightOverride(38.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                    ETGUiButtonStyle::Secondary))
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                .OnClicked(FOnClicked::CreateUObject(
                                    this,
                                    &UTGExecutionTimeLimitDialogWidget::
                                        HandleCancelClicked))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Cancel")))
                                    .Font(TGUiTheme::GetSlateFont(
                                        ETGUiTextStyle::BodyStrong))
                                    .ColorAndOpacity(Palette.TextPrimary)
                                ]
                            ]
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .Padding(Spacing.Small, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(SBox)
                            .WidthOverride(112.0f)
                            .HeightOverride(38.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                    ETGUiButtonStyle::Primary))
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                .OnClicked(FOnClicked::CreateUObject(
                                    this,
                                    &UTGExecutionTimeLimitDialogWidget::
                                        HandleApplyClicked))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(TEXT("Apply")))
                                    .Font(TGUiTheme::GetSlateFont(
                                        ETGUiTextStyle::BodyStrong))
                                    .ColorAndOpacity(Palette.Canvas)
                                ]
                            ]
                        ]
                    ]
                ]
            ]
        ];

    return RootSlateWidget.ToSharedRef();
}

void UTGExecutionTimeLimitDialogWidget::ReleaseSlateResources(
    const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    TimeoutInput.Reset();
    ErrorText.Reset();
    RootSlateWidget.Reset();
}

void UTGExecutionTimeLimitDialogWidget::CloseDialog()
{
    RemoveFromParent();
}

UTGSimulationSubsystem*
UTGExecutionTimeLimitDialogWidget::GetSimulationSubsystem() const
{
    UGameInstance* GameInstance = GetGameInstance();
    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
}

bool UTGExecutionTimeLimitDialogWidget::ApplyExecutionTimeLimit()
{
    if (!TimeoutInput.IsValid())
    {
        return false;
    }

    FString Input = TimeoutInput->GetText().ToString();
    Input.TrimStartAndEndInline();

    double MaximumSeconds = 0.0;
    if (!LexTryParseString(MaximumSeconds, *Input) ||
        !FMath::IsFinite(MaximumSeconds) ||
        MaximumSeconds <= 0.0)
    {
        ShowInputError(FText::FromString(
            TEXT("Enter a positive finite number of seconds.")));
        return false;
    }

    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        ShowInputError(FText::FromString(
            TEXT("The current scenario is unavailable.")));
        return false;
    }

    FText Error;
    if (!Subsystem->SetCurrentScenarioMaximumWallClockRuntimeSeconds(
            MaximumSeconds,
            Error))
    {
        ShowInputError(Error);
        return false;
    }

    CloseDialog();
    return true;
}

void UTGExecutionTimeLimitDialogWidget::ShowInputError(
    const FText& Message)
{
    if (ErrorText.IsValid())
    {
        ErrorText->SetText(Message);
        ErrorText->SetVisibility(EVisibility::Visible);
    }
}

FReply UTGExecutionTimeLimitDialogWidget::HandleApplyClicked()
{
    ApplyExecutionTimeLimit();
    return FReply::Handled();
}

FReply UTGExecutionTimeLimitDialogWidget::HandleCancelClicked()
{
    CloseDialog();
    return FReply::Handled();
}

void UTGExecutionTimeLimitDialogWidget::HandleTextCommitted(
    const FText& Text,
    const ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        ApplyExecutionTimeLimit();
    }
}
