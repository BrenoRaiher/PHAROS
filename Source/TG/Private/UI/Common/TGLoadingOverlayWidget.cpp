// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGLoadingOverlayWidget.h"

#include "Input/Reply.h"
#include "UI/Theme/TGUiTheme.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace TGLoadingOverlayPrivate
{
    const FSlateBrush& BackdropBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            FLinearColor::White,
            0.0f);
        return Brush;
    }

    const FSlateBrush& PanelBrush()
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

void UTGLoadingOverlayWidget::Configure(
    const FText& InTitle,
    const FText& InMessage)
{
    Title = InTitle;
    Message = InMessage;

    if (TitleWidget.IsValid())
    {
        TitleWidget->SetText(Title);
    }
    if (MessageWidget.IsValid())
    {
        MessageWidget->SetText(Message);
    }
}

void UTGLoadingOverlayWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
}

FReply UTGLoadingOverlayWidget::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    return FReply::Handled();
}

FReply UTGLoadingOverlayWidget::NativeOnMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();
}

FReply UTGLoadingOverlayWidget::NativeOnMouseButtonUp(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();
}

TSharedRef<SWidget> UTGLoadingOverlayWidget::RebuildWidget()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();

    RootSlateWidget =
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(&TGLoadingOverlayPrivate::BackdropBrush())
            .BorderBackgroundColor(Palette.Overlay)
            .Padding(0.0f)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(Spacing.Section)
        [
            SNew(SBox)
            .WidthOverride(480.0f)
            [
                SNew(SBorder)
                .BorderImage(&TGLoadingOverlayPrivate::PanelBrush())
                .Padding(Spacing.Wide)
                [
                    SNew(SHorizontalBox)

                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, Spacing.Large, 0.0f)
                    [
                        SNew(SBox)
                        .WidthOverride(44.0f)
                        .HeightOverride(28.0f)
                        .HAlign(HAlign_Center)
                        .VAlign(VAlign_Center)
                        .Clipping(EWidgetClipping::ClipToBounds)
                        [
                            SNew(SThrobber)
                            .NumPieces(3)
                        ]
                    ]

                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(SVerticalBox)

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        [
                            SAssignNew(TitleWidget, STextBlock)
                            .Text(Title)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::PanelTitle))
                            .ColorAndOpacity(Palette.TextPrimary)
                        ]

                        + SVerticalBox::Slot()
                        .AutoHeight()
                        .Padding(0.0f, Spacing.Small, 0.0f, 0.0f)
                        [
                            SAssignNew(MessageWidget, STextBlock)
                            .Text(Message)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::Body))
                            .ColorAndOpacity(Palette.TextSecondary)
                            .AutoWrapText(true)
                        ]
                    ]
                ]
            ]
        ];

    return RootSlateWidget.ToSharedRef();
}

void UTGLoadingOverlayWidget::ReleaseSlateResources(
    const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    TitleWidget.Reset();
    MessageWidget.Reset();
    RootSlateWidget.Reset();
}
