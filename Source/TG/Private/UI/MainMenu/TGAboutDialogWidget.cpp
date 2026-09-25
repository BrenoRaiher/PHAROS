#include "UI/MainMenu/TGAboutDialogWidget.h"

#include "HAL/PlatformProcess.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateTypes.h"
#include "UI/MainMenu/TGLegalDocumentUtils.h"
#include "UI/Theme/TGUiTheme.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SHyperlink.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace TGAboutDialogPrivate
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

    const FSlateBrush& DividerBrush()
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette.Border,
            0.0f);
        return Brush;
    }

    const FTextBlockStyle& RepositoryLinkTextStyle()
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        static const FTextBlockStyle Style = [Palette]()
        {
            FTextBlockStyle Result =
                FCoreStyle::Get().GetWidgetStyle<FTextBlockStyle>(
                    TEXT("NormalText"));
            Result.SetFont(TGUiTheme::GetSlateFont(
                    ETGUiTextStyle::BodyStrong))
                .SetColorAndOpacity(FSlateColor(Palette.Accent));
            return Result;
        }();
        return Style;
    }
}

void UTGAboutDialogWidget::SetRepositoryUrl(
    const FString& InRepositoryUrl)
{
    RepositoryUrl = InRepositoryUrl;
    RepositoryUrl.TrimStartAndEndInline();
}

void UTGAboutDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
}

FReply UTGAboutDialogWidget::NativeOnKeyDown(
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

TSharedRef<SWidget> UTGAboutDialogWidget::RebuildWidget()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();
    const bool bHasRepositoryUrl = HasUsableRepositoryUrl();
    FString EulaPath;
    FString NoticesPath;
    const bool bHasEula = TGLegalDocumentUtils::ResolvePath(
        TEXT("EULA.txt"),
        EulaPath);
    const bool bHasNotices = TGLegalDocumentUtils::ResolvePath(
        TEXT("THIRD_PARTY_NOTICES.txt"),
        NoticesPath);

    const FText RepositoryText = bHasRepositoryUrl
        ? FText::FromString(RepositoryUrl)
        : FText::FromString(TEXT("Repository URL not configured."));

    RootSlateWidget =
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(&TGAboutDialogPrivate::BackdropBrush())
            .BorderBackgroundColor(Palette.Overlay)
            .Padding(0.0f)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(Spacing.Section)
        [
            SNew(SBox)
            .WidthOverride(680.0f)
            [
                SNew(SBorder)
                .BorderImage(&TGAboutDialogPrivate::DialogBrush())
                .Padding(Spacing.Wide)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("ABOUT")))
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
                            "PHAROS is a spacecraft dynamics and mission-"
                            "simulation platform for configuring hierarchical "
                            "articulated spacecraft, propagating coupled "
                            "orbital, attitude, and multibody motion, and "
                            "examining the resulting trajectory and telemetry "
                            "in an interactive Solar System visualization. It "
                            "supports configurable gravity and environmental "
                            "models, variable mass, actuators, and user-defined "
                            "control laws.")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::Body))
                        .ColorAndOpacity(Palette.TextSecondary)
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .HeightOverride(1.0f)
                        [
                            SNew(SBorder)
                            .BorderImage(
                                &TGAboutDialogPrivate::DividerBrush())
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("LEGAL")))
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
                            "PHAROS 1.0 | Copyright (c) 2026 Breno Raiher\n"
                            "PHAROS uses Unreal\u00ae Engine. Unreal\u00ae is a "
                            "trademark or registered trademark of Epic Games, "
                            "Inc. in the United States of America and "
                            "elsewhere.\n"
                            "Unreal\u00ae Engine, Copyright 1998 \u2013 2026, Epic "
                            "Games, Inc. All rights reserved.")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::Caption))
                        .ColorAndOpacity(Palette.TextSecondary)
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Regular, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SHyperlink)
                            .Text(FText::FromString(
                                TEXT("End User License Agreement")))
                            .TextStyle(
                                &TGAboutDialogPrivate::
                                    RepositoryLinkTextStyle())
                            .Padding(0.0f)
                            .IsEnabled(bHasEula)
                            .ToolTipText(FText::FromString(
                                bHasEula
                                    ? TEXT("Open the PHAROS EULA.")
                                    : TEXT("The PHAROS EULA is unavailable.")))
                            .OnNavigate(FSimpleDelegate::CreateUObject(
                                this,
                                &UTGAboutDialogWidget::HandleEulaNavigate))
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        .Padding(Spacing.Section, 0.0f, 0.0f, 0.0f)
                        [
                            SNew(SHyperlink)
                            .Text(FText::FromString(
                                TEXT("Third-Party Notices")))
                            .TextStyle(
                                &TGAboutDialogPrivate::
                                    RepositoryLinkTextStyle())
                            .Padding(0.0f)
                            .IsEnabled(bHasNotices)
                            .ToolTipText(FText::FromString(
                                bHasNotices
                                    ? TEXT(
                                        "Open the PHAROS third-party notices.")
                                    : TEXT(
                                        "The third-party notices are "
                                        "unavailable.")))
                            .OnNavigate(FSimpleDelegate::CreateUObject(
                                this,
                                &UTGAboutDialogWidget::
                                    HandleNoticesNavigate))
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .HeightOverride(1.0f)
                        [
                            SNew(SBorder)
                            .BorderImage(
                                &TGAboutDialogPrivate::DividerBrush())
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT("DOCUMENTATION")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::PanelTitle))
                        .ColorAndOpacity(Palette.TextPrimary)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Regular, 0.0f, 0.0f)
                    [
                        SNew(SHorizontalBox)

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(STextBlock)
                            .Font(TGUiTheme::GetSlateFont(
                                ETGUiTextStyle::Body))
                            .ColorAndOpacity(Palette.TextSecondary)
                            .Text(FText::FromString(TEXT(
                                "Source code and documentation: ")))
                        ]

                        + SHorizontalBox::Slot()
                        .AutoWidth()
                        .VAlign(VAlign_Center)
                        [
                            SNew(SHyperlink)
                            .Text(RepositoryText)
                            .TextStyle(
                                &TGAboutDialogPrivate::
                                    RepositoryLinkTextStyle())
                            .Padding(FMargin(2.0f, 0.0f))
                            .IsEnabled(bHasRepositoryUrl)
                            .ToolTipText(FText::FromString(
                                bHasRepositoryUrl
                                    ? TEXT(
                                        "Open the PHAROS GitHub repository in "
                                        "the default browser.")
                                    : TEXT(
                                        "Provide a valid HTTP or HTTPS "
                                        "repository URL when opening this "
                                        "dialog.")))
                            .OnNavigate(FSimpleDelegate::CreateUObject(
                                this,
                                &UTGAboutDialogWidget::
                                    HandleRepositoryNavigate))
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .WidthOverride(132.0f)
                        .HeightOverride(38.0f)
                        [
                            SNew(SButton)
                            .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                ETGUiButtonStyle::Secondary))
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            .ToolTipText(FText::FromString(
                                TEXT("Close the About dialog.")))
                            .OnClicked(FOnClicked::CreateUObject(
                                this,
                                &UTGAboutDialogWidget::HandleCloseClicked))
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Close Dialog")))
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::BodyStrong))
                                .ColorAndOpacity(Palette.TextPrimary)
                            ]
                        ]
                    ]
                ]
            ]
        ];

    return RootSlateWidget.ToSharedRef();
}

void UTGAboutDialogWidget::ReleaseSlateResources(
    const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RootSlateWidget.Reset();
}

void UTGAboutDialogWidget::CloseDialog()
{
    RemoveFromParent();
}

bool UTGAboutDialogWidget::HasUsableRepositoryUrl() const
{
    return RepositoryUrl.StartsWith(
            TEXT("https://"),
            ESearchCase::IgnoreCase)
        || RepositoryUrl.StartsWith(
            TEXT("http://"),
            ESearchCase::IgnoreCase);
}

void UTGAboutDialogWidget::HandleRepositoryNavigate()
{
    if (!HasUsableRepositoryUrl())
    {
        return;
    }

    FString LaunchError;
    FPlatformProcess::LaunchURL(
        *RepositoryUrl,
        nullptr,
        &LaunchError);

    if (!LaunchError.IsEmpty())
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PHAROS: Could not open repository URL: %s"),
            *LaunchError);
    }
}

void UTGAboutDialogWidget::HandleEulaNavigate()
{
    TGLegalDocumentUtils::Open(TEXT("EULA.txt"));
}

void UTGAboutDialogWidget::HandleNoticesNavigate()
{
    TGLegalDocumentUtils::Open(TEXT("THIRD_PARTY_NOTICES.txt"));
}

FReply UTGAboutDialogWidget::HandleCloseClicked()
{
    CloseDialog();
    return FReply::Handled();
}
