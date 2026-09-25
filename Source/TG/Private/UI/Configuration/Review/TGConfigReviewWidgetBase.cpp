// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Review/TGConfigReviewWidgetBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "UI/Configuration/Review/TGReviewIssueRowWidget.h"
#include "UI/Configuration/Review/TGScenarioReviewValidationLibrary.h"
#include "UI/Theme/TGUiTheme.h"

void UTGConfigReviewWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);

    if (BTN_ValidateScenario != nullptr)
        BTN_ValidateScenario->OnClicked.AddUniqueDynamic(
            this, &UTGConfigReviewWidgetBase::HandleValidateClicked);
    if (BTN_AcceptValidation != nullptr)
        BTN_AcceptValidation->OnClicked.AddUniqueDynamic(
            this, &UTGConfigReviewWidgetBase::HandleAcceptClicked);
    if (CHECK_AcknowledgeWarnings != nullptr)
        CHECK_AcknowledgeWarnings->OnCheckStateChanged.AddUniqueDynamic(
            this, &UTGConfigReviewWidgetBase::HandleWarningAcknowledgementChanged);

    if (SCROLL_Issues != nullptr)
    {
        FScrollBoxStyle ScrollBoxStyle = TGUiTheme::MakeScrollBoxStyle();
        ScrollBoxStyle.BarThickness = 8.0f;
        SCROLL_Issues->SetWidgetStyle(ScrollBoxStyle);

        FScrollBarStyle ScrollBarStyle = TGUiTheme::MakeScrollBarStyle();
        ScrollBarStyle.Thickness = 8.0f;
        SCROLL_Issues->SetWidgetBarStyle(ScrollBarStyle);
        SCROLL_Issues->SetScrollbarThickness(FVector2D(8.0f, 8.0f));
        SCROLL_Issues->SetScrollbarPadding(
            FMargin(6.0f, 4.0f, 4.0f, 4.0f));
        SCROLL_Issues->SetAlwaysShowScrollbarTrack(true);
        SCROLL_Issues->SetAnimateWheelScrolling(true);
        SCROLL_Issues->SetAllowOverscroll(false);
        SCROLL_Issues->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
        SCROLL_Issues->SetWheelScrollMultiplier(4.0f);
    }

    RefreshFromCurrentDraft();
}

void UTGConfigReviewWidgetBase::NativeDestruct()
{
    if (BTN_ValidateScenario != nullptr)
        BTN_ValidateScenario->OnClicked.RemoveDynamic(
            this, &UTGConfigReviewWidgetBase::HandleValidateClicked);
    if (BTN_AcceptValidation != nullptr)
        BTN_AcceptValidation->OnClicked.RemoveDynamic(
            this, &UTGConfigReviewWidgetBase::HandleAcceptClicked);
    if (CHECK_AcknowledgeWarnings != nullptr)
        CHECK_AcknowledgeWarnings->OnCheckStateChanged.RemoveDynamic(
            this, &UTGConfigReviewWidgetBase::HandleWarningAcknowledgementChanged);

    Super::NativeDestruct();
}

void UTGConfigReviewWidgetBase::NativeTick(
    const FGeometry& MyGeometry,
    float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (UTGSimulationSubsystem* Subsystem = ResolveSimulationSubsystem())
    {
        const uint64 Revision = Subsystem->GetCurrentScenarioDraftRevision();
        if (Revision != DisplayedDraftRevision)
        {
            RefreshFromCurrentDraft();
        }
    }
}

UTGSimulationSubsystem*
UTGConfigReviewWidgetBase::ResolveSimulationSubsystem() const
{
    const UGameInstance* GameInstance = GetGameInstance();
    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
}

void UTGConfigReviewWidgetBase::RefreshFromCurrentDraft()
{
    UTGSimulationSubsystem* Subsystem = ResolveSimulationSubsystem();
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        DisplayedDraftRevision = 0;
        InvalidateDisplayedReport(
            FText::FromString(TEXT("No scenario is currently open.")));
        if (TXT_ScenarioName != nullptr)
            TXT_ScenarioName->SetText(FText::FromString(
                TEXT("Scenario: No scenario")));
        return;
    }

    DisplayedDraftRevision = Subsystem->GetCurrentScenarioDraftRevision();
    const FTGSimulationScenario Scenario = Subsystem->GetCurrentScenarioDraft();
    if (TXT_ScenarioName != nullptr)
    {
        const FString ScenarioName =
            Scenario.ScenarioAndSolver.ScenarioName.TrimStartAndEnd();
        TXT_ScenarioName->SetText(FText::FromString(FString::Printf(
            TEXT("Scenario: %s"),
            ScenarioName.IsEmpty() ? TEXT("(unnamed)") : *ScenarioName)));
    }

    if (bHasValidatedSnapshot && ValidatedDraftRevision != DisplayedDraftRevision)
    {
        InvalidateDisplayedReport(FText::FromString(
            TEXT("The scenario changed after validation. Validate it again.")));
    }
    else if (!bHasValidatedSnapshot)
    {
        InvalidateDisplayedReport(FText::FromString(
            TEXT("Validate the scenario to see its results.")));
    }
    else
    {
        RenderReport();
    }
}

void UTGConfigReviewWidgetBase::NotifySimulationBlocked(const FText& Reason)
{
    RefreshFromCurrentDraft();
    if (TXT_ReviewMessage != nullptr)
    {
        TXT_ReviewMessage->SetText(Reason);
        TXT_ReviewMessage->SetColorAndOpacity(
            FSlateColor(TGUiTheme::GetPalette().Warning));
    }
}

void UTGConfigReviewWidgetBase::HandleValidateClicked()
{
    FText Error;
    ValidateCurrentDraft(Error);
}

bool UTGConfigReviewWidgetBase::ValidateCurrentDraft(FText& OutError)
{
    OutError = FText::GetEmpty();
    if (bValidationRunning)
    {
        OutError = FText::FromString(
            TEXT("Scenario validation is already running."));
        return false;
    }

    UTGSimulationSubsystem* Subsystem = ResolveSimulationSubsystem();
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        OutError = FText::FromString(
            TEXT("No scenario is currently open."));
        NotifySimulationBlocked(OutError);
        return false;
    }

    bValidationRunning = true;
    Subsystem->InvalidateCurrentScenarioReview();
    UpdateAcceptState();

    const uint64 SnapshotRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    const FTGSimulationScenario Snapshot =
        Subsystem->GetCurrentScenarioDraft();

    FTGScenarioReviewReport NewReport =
        UTGScenarioReviewValidationLibrary::
            ValidateScenarioForAuthoringReview(this, Snapshot);

    if (SnapshotRevision != Subsystem->GetCurrentScenarioDraftRevision())
    {
        bValidationRunning = false;
        DisplayedDraftRevision = Subsystem->GetCurrentScenarioDraftRevision();
        OutError = FText::FromString(
            TEXT("The scenario changed while validation was running. Validate it again."));
        InvalidateDisplayedReport(OutError);
        return false;
    }

    ValidatedSnapshot = Snapshot;
    CurrentReport = MoveTemp(NewReport);
    ValidatedDraftRevision = SnapshotRevision;
    DisplayedDraftRevision = SnapshotRevision;
    bHasValidatedSnapshot = true;
    if (CHECK_AcknowledgeWarnings != nullptr)
        CHECK_AcknowledgeWarnings->SetIsChecked(false);
    if (TXT_SelectedIssueDetails != nullptr)
        TXT_SelectedIssueDetails->SetText(FText::FromString(
            TEXT("Select an issue for more information.")));
    if (TXT_ReviewMessage != nullptr)
        TXT_ReviewMessage->SetText(FText::GetEmpty());
    bValidationRunning = false;
    RenderReport();
    return true;
}

void UTGConfigReviewWidgetBase::HandleAcceptClicked()
{
    if (!bHasValidatedSnapshot || !CurrentReport.IsAcceptable())
        return;
    if (CurrentReport.WarningCount > 0
        && (CHECK_AcknowledgeWarnings == nullptr
            || !CHECK_AcknowledgeWarnings->IsChecked()))
        return;

    UTGSimulationSubsystem* Subsystem = ResolveSimulationSubsystem();
    FText Error;
    if (Subsystem == nullptr
        || !Subsystem->AcceptCurrentScenarioReview(
            ValidatedDraftRevision, Error))
    {
        InvalidateDisplayedReport(Error.IsEmpty()
            ? FText::FromString(TEXT("The scenario could not be approved. Validate it again."))
            : Error);
        return;
    }

    PresentAcceptedState();
}

bool UTGConfigReviewWidgetBase::PrepareCurrentDraftForSimulation(
    FText& OutReason)
{
    OutReason = FText::GetEmpty();

    UTGSimulationSubsystem* Subsystem = ResolveSimulationSubsystem();
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        OutReason = FText::FromString(
            TEXT("No scenario is currently open."));
        return false;
    }

    if (Subsystem->CanSimulateCurrentScenario(OutReason))
    {
        return true;
    }

    const uint64 CurrentRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    if (!bHasValidatedSnapshot
        || ValidatedDraftRevision != CurrentRevision)
    {
        if (!ValidateCurrentDraft(OutReason))
        {
            return false;
        }
    }

    if (!CurrentReport.bValidationCompleted)
    {
        OutReason = FText::FromString(
            TEXT("Scenario validation did not complete."));
        return false;
    }
    if (CurrentReport.ErrorCount > 0)
    {
        OutReason = FText::FromString(FString::Printf(
            TEXT("Validation found %d error%s. Correct the reported issues before simulation."),
            CurrentReport.ErrorCount,
            CurrentReport.ErrorCount == 1 ? TEXT("") : TEXT("s")));
        return false;
    }
    if (CurrentReport.WarningCount > 0)
    {
        OutReason = FText::FromString(FString::Printf(
            TEXT("Validation found %d warning%s. Review and approve the scenario manually before simulation."),
            CurrentReport.WarningCount,
            CurrentReport.WarningCount == 1 ? TEXT("") : TEXT("s")));
        return false;
    }

    FText ApprovalError;
    if (!Subsystem->AcceptCurrentScenarioReview(
            ValidatedDraftRevision,
            ApprovalError))
    {
        OutReason = ApprovalError.IsEmpty()
            ? FText::FromString(
                TEXT("The clean validation report could not be approved."))
            : ApprovalError;
        return false;
    }

    PresentAcceptedState();
    OutReason = FText::GetEmpty();
    return true;
}

void UTGConfigReviewWidgetBase::PresentAcceptedState()
{
    if (TXT_ReviewStatus != nullptr)
    {
        TXT_ReviewStatus->SetText(FText::FromString(
            TEXT("APPROVED - READY TO SIMULATE")));
        TXT_ReviewStatus->SetColorAndOpacity(
            FSlateColor(TGUiTheme::GetPalette().Success));
    }
    if (TXT_ReviewMessage != nullptr)
    {
        TXT_ReviewMessage->SetText(FText::FromString(
            TEXT("The scenario is approved for simulation.")));
        TXT_ReviewMessage->SetColorAndOpacity(
            FSlateColor(TGUiTheme::GetPalette().Success));
    }
    UpdateAcceptState();
}

void UTGConfigReviewWidgetBase::HandleWarningAcknowledgementChanged(
    bool bIsChecked)
{
    UpdateAcceptState();
}

void UTGConfigReviewWidgetBase::HandleIssueSelected(int32 IssueIndex)
{
    if (!CurrentReport.Issues.IsValidIndex(IssueIndex))
        return;

    const FTGScenarioReviewIssue& Issue = CurrentReport.Issues[IssueIndex];
    if (TXT_SelectedIssueDetails != nullptr)
    {
        TXT_SelectedIssueDetails->SetText(FText::FromString(FString::Printf(
            TEXT("%s\n\nPanel: %s"),
            *Issue.Message.ToString(),
            *GetSectionDisplayName(Issue.Section))));
    }

    OnIssueNavigationRequested.Broadcast(Issue);
}

void UTGConfigReviewWidgetBase::InvalidateDisplayedReport(
    const FText& Message)
{
    CurrentReport = FTGScenarioReviewReport{};
    ValidatedSnapshot = FTGSimulationScenario{};
    ValidatedDraftRevision = 0;
    bHasValidatedSnapshot = false;

    if (VBOX_IssueRows != nullptr)
        VBOX_IssueRows->ClearChildren();
    if (TXT_ReviewStatus != nullptr)
    {
        TXT_ReviewStatus->SetText(FText::FromString(TEXT("NOT CHECKED")));
        TXT_ReviewStatus->SetColorAndOpacity(
            FSlateColor(TGUiTheme::GetPalette().TextSecondary));
    }
    if (TXT_ErrorCount != nullptr)
        TXT_ErrorCount->SetText(FText::FromString(TEXT("Errors: -")));
    if (TXT_WarningCount != nullptr)
        TXT_WarningCount->SetText(FText::FromString(TEXT("Warnings: -")));
    if (TXT_ValidationRevision != nullptr)
        TXT_ValidationRevision->SetText(FText::FromString(FString::Printf(
            TEXT("Draft revision: %llu"),
            static_cast<unsigned long long>(DisplayedDraftRevision))));
    if (TXT_ReviewMessage != nullptr)
    {
        TXT_ReviewMessage->SetText(Message);
        TXT_ReviewMessage->SetColorAndOpacity(
            FSlateColor(TGUiTheme::GetPalette().TextSecondary));
    }
    if (TXT_SelectedIssueDetails != nullptr)
        TXT_SelectedIssueDetails->SetText(FText::FromString(
            TEXT("Validate the scenario, then select an issue for more information.")));
    if (CHECK_AcknowledgeWarnings != nullptr)
        CHECK_AcknowledgeWarnings->SetIsChecked(false);
    if (BORDER_WarningAcknowledgement != nullptr)
        BORDER_WarningAcknowledgement->SetVisibility(ESlateVisibility::Collapsed);
    UpdateAcceptState();
}

void UTGConfigReviewWidgetBase::RenderReport()
{
    if (!bHasValidatedSnapshot)
        return;

    if (TXT_ErrorCount != nullptr)
        TXT_ErrorCount->SetText(FText::FromString(FString::Printf(
            TEXT("Errors: %d"), CurrentReport.ErrorCount)));
    if (TXT_WarningCount != nullptr)
        TXT_WarningCount->SetText(FText::FromString(FString::Printf(
            TEXT("Warnings: %d"), CurrentReport.WarningCount)));
    if (TXT_ValidationRevision != nullptr)
        TXT_ValidationRevision->SetText(FText::FromString(FString::Printf(
            TEXT("Validated draft revision: %llu"),
            static_cast<unsigned long long>(ValidatedDraftRevision))));

    if (TXT_ReviewStatus != nullptr)
    {
        const UTGSimulationSubsystem* Subsystem = ResolveSimulationSubsystem();
        if (Subsystem != nullptr
            && Subsystem->IsCurrentScenarioReviewAccepted())
        {
            TXT_ReviewStatus->SetText(FText::FromString(
                TEXT("APPROVED - READY TO SIMULATE")));
            TXT_ReviewStatus->SetColorAndOpacity(
                FSlateColor(TGUiTheme::GetPalette().Success));
        }
        else if (CurrentReport.ErrorCount > 0)
        {
            TXT_ReviewStatus->SetText(FText::FromString(TEXT("ERRORS")));
            TXT_ReviewStatus->SetColorAndOpacity(
                FSlateColor(TGUiTheme::GetPalette().Error));
        }
        else if (CurrentReport.WarningCount > 0)
        {
            TXT_ReviewStatus->SetText(FText::FromString(TEXT("READY WITH WARNINGS")));
            TXT_ReviewStatus->SetColorAndOpacity(
                FSlateColor(TGUiTheme::GetPalette().Warning));
        }
        else
        {
            TXT_ReviewStatus->SetText(FText::FromString(TEXT("READY")));
            TXT_ReviewStatus->SetColorAndOpacity(
                FSlateColor(TGUiTheme::GetPalette().Success));
        }
    }

    if (BORDER_WarningAcknowledgement != nullptr)
        BORDER_WarningAcknowledgement->SetVisibility(
            CurrentReport.WarningCount > 0 && CurrentReport.ErrorCount == 0
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);

    RebuildIssueRows();
    UpdateAcceptState();
}

void UTGConfigReviewWidgetBase::RebuildIssueRows()
{
    if (VBOX_IssueRows == nullptr || WidgetTree == nullptr)
        return;
    VBOX_IssueRows->ClearChildren();

    if (CurrentReport.Issues.IsEmpty())
    {
        UTextBlock* EmptyText =
            WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
        EmptyText->SetText(FText::FromString(
            TEXT("No errors or warnings were found.")));
        TGUiTheme::ApplyTextStyle(
            *EmptyText,
            ETGUiTextStyle::Body,
            TGUiTheme::GetPalette().Success);
        EmptyText->SetJustification(ETextJustify::Left);
        if (UVerticalBoxSlot* EmptySlot =
                VBOX_IssueRows->AddChildToVerticalBox(EmptyText))
        {
            EmptySlot->SetHorizontalAlignment(HAlign_Fill);
        }
        return;
    }

    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();
    TOptional<ETGScenarioReviewSection> PreviousSection;
    for (int32 Index = 0; Index < CurrentReport.Issues.Num(); ++Index)
    {
        const FTGScenarioReviewIssue& Issue = CurrentReport.Issues[Index];
        if (!PreviousSection.IsSet() || PreviousSection.GetValue() != Issue.Section)
        {
            UTextBlock* SectionText =
                WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
            SectionText->SetText(FText::FromString(
                GetSectionDisplayName(Issue.Section).ToUpper()));
            TGUiTheme::ApplyTextStyle(
                *SectionText,
                ETGUiTextStyle::Section,
                TGUiTheme::GetPalette().TextPrimary);
            SectionText->SetJustification(ETextJustify::Left);
            if (UVerticalBoxSlot* SectionSlot =
                    VBOX_IssueRows->AddChildToVerticalBox(SectionText))
            {
                SectionSlot->SetHorizontalAlignment(HAlign_Fill);
                SectionSlot->SetPadding(FMargin(
                    0.0f,
                    PreviousSection.IsSet() ? Spacing.Regular : 0.0f,
                    0.0f,
                    Spacing.Small));
            }
            PreviousSection = Issue.Section;
        }

        UTGReviewIssueRowWidget* Row = CreateWidget<UTGReviewIssueRowWidget>(
            GetWorld(), UTGReviewIssueRowWidget::StaticClass());
        if (Row != nullptr)
        {
            Row->Configure(Index, Issue);
            Row->OnIssueSelected.AddUniqueDynamic(
                this, &UTGConfigReviewWidgetBase::HandleIssueSelected);
            if (UVerticalBoxSlot* RowSlot =
                    VBOX_IssueRows->AddChildToVerticalBox(Row))
            {
                RowSlot->SetHorizontalAlignment(HAlign_Fill);
                RowSlot->SetPadding(FMargin(
                    0.0f, 0.0f, 0.0f, Spacing.ExtraSmall));
            }
        }
    }
}

void UTGConfigReviewWidgetBase::UpdateAcceptState()
{
    if (BTN_ValidateScenario != nullptr)
        BTN_ValidateScenario->SetIsEnabled(!bValidationRunning);

    bool bCanAccept = !bValidationRunning
        && bHasValidatedSnapshot
        && CurrentReport.IsAcceptable();
    if (bCanAccept && CurrentReport.WarningCount > 0)
        bCanAccept = CHECK_AcknowledgeWarnings != nullptr
            && CHECK_AcknowledgeWarnings->IsChecked();

    if (UTGSimulationSubsystem* Subsystem = ResolveSimulationSubsystem())
        bCanAccept &= !Subsystem->IsCurrentScenarioReviewAccepted();
    if (BTN_AcceptValidation != nullptr)
        BTN_AcceptValidation->SetIsEnabled(bCanAccept);
}

FString UTGConfigReviewWidgetBase::GetSectionDisplayName(
    ETGScenarioReviewSection Section)
{
    switch (Section)
    {
        case ETGScenarioReviewSection::ScenarioAndSolver:
            return TEXT("Scenario and Solver");
        case ETGScenarioReviewSection::InitialState:
            return TEXT("Initial State");
        case ETGScenarioReviewSection::ComponentsAndJoints:
            return TEXT("Components and Joints");
        case ETGScenarioReviewSection::Actuators:
            return TEXT("Actuators");
        case ETGScenarioReviewSection::Controller:
            return TEXT("Controller");
        case ETGScenarioReviewSection::Gravity:
            return TEXT("Gravity and Bodies");
        case ETGScenarioReviewSection::SolarRadiationPressure:
            return TEXT("Solar Radiation Pressure");
        case ETGScenarioReviewSection::Atmosphere:
            return TEXT("Atmosphere");
        case ETGScenarioReviewSection::Aerodynamics:
            return TEXT("Aerodynamics");
        case ETGScenarioReviewSection::CrossSystemAndSpice:
            return TEXT("Cross-System and SPICE");
        default:
            return TEXT("Review");
    }
}
