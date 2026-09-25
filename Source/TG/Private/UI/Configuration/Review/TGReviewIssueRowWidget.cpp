// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Review/TGReviewIssueRowWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/TextBlock.h"
#include "UI/Theme/TGUiTheme.h"

void UTGReviewIssueRowWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (WidgetTree == nullptr || WidgetTree->RootWidget != nullptr)
        return;

    RootBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();

    RootBorder->SetPadding(FMargin(Spacing.Regular, Spacing.Small));
    WidgetTree->RootWidget = RootBorder;

    SelectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    TGUiTheme::ApplyRowButtonStyle(*SelectButton);
    SelectButton->SetToolTipText(FText::FromString(
        TEXT("Open the configuration panel related to this issue.")));
    RootBorder->SetContent(SelectButton);

    SummaryText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
    SummaryText->SetAutoWrapText(true);
    SummaryText->SetJustification(ETextJustify::Left);
    TGUiTheme::ApplyTextStyle(
        *SummaryText,
        ETGUiTextStyle::Body,
        Palette.TextPrimary);
    SelectButton->SetContent(SummaryText);
    if (UButtonSlot* TextSlot = Cast<UButtonSlot>(SummaryText->Slot))
    {
        TextSlot->SetHorizontalAlignment(HAlign_Fill);
        TextSlot->SetVerticalAlignment(VAlign_Center);
    }

    SelectButton->OnClicked.AddUniqueDynamic(
        this,
        &UTGReviewIssueRowWidget::HandleClicked);
}

void UTGReviewIssueRowWidget::Configure(
    int32 InIssueIndex,
    const FTGScenarioReviewIssue& Issue)
{
    IssueIndex = InIssueIndex;
    if (SummaryText != nullptr)
    {
        SummaryText->SetText(MakeUserFacingSummary(Issue));
    }
    if (RootBorder != nullptr)
    {
        RootBorder->SetBrushColor(
            Issue.Severity == ETGScenarioReviewSeverity::Error
                ? TGUiTheme::MakeSemanticWash(
                    TGUiTheme::GetPalette().Error)
                : TGUiTheme::MakeSemanticWash(
                    TGUiTheme::GetPalette().Warning));
    }
}

FText UTGReviewIssueRowWidget::MakeUserFacingSummary(
    const FTGScenarioReviewIssue& Issue)
{
    const TCHAR* SeverityText =
        Issue.Severity == ETGScenarioReviewSeverity::Error
            ? TEXT("Error")
            : TEXT("Warning");
    return FText::FromString(FString::Printf(
        TEXT("%s: %s"),
        SeverityText,
        *Issue.Message.ToString()));
}

void UTGReviewIssueRowWidget::HandleClicked()
{
    if (IssueIndex != INDEX_NONE)
    {
        OnIssueSelected.Broadcast(IssueIndex);
    }
}
