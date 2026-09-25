// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/MainMenu/TGEulaDialogWidget.h"

#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "UI/Theme/TGUiTheme.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace TGEulaDialogPrivate
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

    const FSlateBrush& DocumentBrush()
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette.Canvas,
            0.0f,
            Palette.Border,
            1.0f);
        return Brush;
    }
}

void UTGEulaDialogWidget::Configure(
    const FString& InEulaVersion,
    const FText& InDocumentText,
    const bool bInCanAccept,
    FSimpleDelegate InAccepted,
    FSimpleDelegate InDeclined)
{
    EulaVersion = InEulaVersion;
    DocumentText = InDocumentText;
    bCanAccept = bInCanAccept;
    Accepted = MoveTemp(InAccepted);
    Declined = MoveTemp(InDeclined);
}

void UTGEulaDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
}

FReply UTGEulaDialogWidget::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

TSharedRef<SWidget> UTGEulaDialogWidget::RebuildWidget()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();

    const FText StatusText = bCanAccept
        ? FText::FromString(FString::Printf(
            TEXT("Acceptance is stored for EULA version %s."),
            *EulaVersion))
        : FText::FromString(TEXT(
            "The EULA document could not be loaded. Reinstall PHAROS or "
            "restore its Legal directory before continuing."));

    RootSlateWidget =
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(&TGEulaDialogPrivate::BackdropBrush())
            .BorderBackgroundColor(Palette.Overlay)
            .Padding(0.0f)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(Spacing.Section)
        [
            SNew(SBox)
            .WidthOverride(860.0f)
            .HeightOverride(720.0f)
            [
                SNew(SBorder)
                .BorderImage(&TGEulaDialogPrivate::DialogBrush())
                .Padding(Spacing.Wide)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(
                            TEXT("END USER LICENSE AGREEMENT")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::PanelTitle))
                        .ColorAndOpacity(Palette.TextPrimary)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Small, 0.0f, Spacing.Regular)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(
                            "Review the PHAROS terms below. You must accept "
                            "them before using the packaged application.")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::Body))
                        .ColorAndOpacity(Palette.TextSecondary)
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SBorder)
                        .BorderImage(&TGEulaDialogPrivate::DocumentBrush())
                        .Padding(Spacing.Regular)
                        [
                            SNew(SScrollBox)
                            .Style(&TGUiTheme::GetScrollBoxStyle())
                            .ScrollBarStyle(
                                &TGUiTheme::GetScrollBarStyle())
                            .ScrollBarThickness(FVector2D(8.0f, 8.0f))
                            .ScrollBarAlwaysVisible(true)
                            .AllowOverscroll(EAllowOverscroll::No)
                            .WheelScrollMultiplier(4.0f)

                            + SScrollBox::Slot()
                            .Padding(0.0f, 0.0f, Spacing.Regular, 0.0f)
                            [
                                SNew(STextBlock)
                                .Text(DocumentText)
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::Body))
                                .ColorAndOpacity(Palette.TextSecondary)
                                .AutoWrapText(true)
                            ]
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Regular, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(StatusText)
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::Caption))
                        .ColorAndOpacity(
                            bCanAccept ? Palette.TextMuted : Palette.Error)
                        .AutoWrapText(true)
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
                            .WidthOverride(150.0f)
                            .HeightOverride(38.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                    ETGUiButtonStyle::Secondary))
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                .OnClicked(FOnClicked::CreateUObject(
                                    this,
                                    &UTGEulaDialogWidget::
                                        HandleDeclineClicked))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(
                                        TEXT("Decline and Exit")))
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
                            .WidthOverride(200.0f)
                            .HeightOverride(38.0f)
                            [
                                SNew(SButton)
                                .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                    ETGUiButtonStyle::Primary))
                                .HAlign(HAlign_Center)
                                .VAlign(VAlign_Center)
                                .IsEnabled(bCanAccept)
                                .OnClicked(FOnClicked::CreateUObject(
                                    this,
                                    &UTGEulaDialogWidget::
                                        HandleAcceptClicked))
                                [
                                    SNew(STextBlock)
                                    .Text(FText::FromString(
                                        TEXT("Accept and Continue")))
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

void UTGEulaDialogWidget::ReleaseSlateResources(
    const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RootSlateWidget.Reset();
}

FReply UTGEulaDialogWidget::HandleAcceptClicked()
{
    if (bCanAccept)
    {
        Accepted.ExecuteIfBound();
    }
    return FReply::Handled();
}

FReply UTGEulaDialogWidget::HandleDeclineClicked()
{
    Declined.ExecuteIfBound();
    return FReply::Handled();
}
