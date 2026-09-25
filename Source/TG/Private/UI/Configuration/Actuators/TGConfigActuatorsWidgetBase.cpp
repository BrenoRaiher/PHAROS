// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Actuators/TGConfigActuatorsWidgetBase.h"

#include "UI/Configuration/Actuators/TGActuatorEditingLibrary.h"
#include "UI/Configuration/Actuators/TGActuatorListEntryWidgetBase.h"
#include "UI/Common/TGFileDialogLibrary.h"
#include "UI/Common/TGUtcDateTimeInput.h"
#include "UI/Configuration/Environment/TGEnvironmentEditingLibrary.h"
#include "UI/TGHudFormattingLibrary.h"
#include "UI/Theme/TGUiTheme.h"

#include "Simulation/TGSimulationSubsystem.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/EditableTextBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Misc/Paths.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace TGConfigActuatorsPrivate
{
    const TCHAR* ActuatorRowClassPath =
        TEXT(
            "/Game/UI/Configuration/Actuators/"
            "WBP_ActuatorListEntry."
            "WBP_ActuatorListEntry_C");

    const TCHAR* ConfirmDialogClassPath =
        TEXT(
            "/Game/UI/Common/WBP_ConfirmDialog."
            "WBP_ConfirmDialog_C");

    const FString PrescribedModeText =
        TEXT("Prescribed Profile");

    const FString CommandedModeText =
        TEXT("Commanded");

    const FString ElapsedTimeModeText =
        TEXT("Elapsed Simulation Time");

    const FString AbsoluteUtcModeText =
        TEXT("Absolute UTC");

    const FString ConstantSourceText =
        TEXT("Constant");

    const FString CsvSourceText =
        TEXT("CSV Profile");

    constexpr int32 CsvPreviewRowLimit = 100;

    void ConfigureCsvPreview(
        UMultiLineEditableTextBox* Preview)
    {
        if (Preview != nullptr)
        {
            Preview->SetIsReadOnly(true);
        }
    }

    void RefreshCsvPreview(
        UVerticalBox* Container,
        UMultiLineEditableTextBox* Preview,
        const bool bProfileIsValid,
        const FString& CsvFilePath,
        const FString& ValueHeading)
    {
        if (Container == nullptr || Preview == nullptr)
        {
            return;
        }

        FText PreviewText;
        FText PreviewError;
        const bool bCanShow =
            bProfileIsValid &&
            UTGEnvironmentEditingLibrary::BuildCsvPreview(
                CsvFilePath,
                {
                    TEXT("Time Since Ignition [s]"),
                    ValueHeading
                },
                CsvPreviewRowLimit,
                PreviewText,
                PreviewError);

        Preview->SetText(
            bCanShow
                ? PreviewText
                : FText::GetEmpty());
        Container->SetVisibility(
            bCanShow
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    FString ThrusterModeToString(
        const ETGThrusterMode Mode)
    {
        return Mode == ETGThrusterMode::Commanded
            ? CommandedModeText
            : PrescribedModeText;
    }

    bool TryParseThrusterMode(
        const FString& Text,
        ETGThrusterMode& OutMode)
    {
        if (Text.Equals(
                CommandedModeText,
                ESearchCase::IgnoreCase))
        {
            OutMode = ETGThrusterMode::Commanded;
            return true;
        }

        if (Text.Equals(
                PrescribedModeText,
                ESearchCase::IgnoreCase))
        {
            OutMode = ETGThrusterMode::PrescribedProfile;
            return true;
        }

        return false;
    }

    FString TimeModeToString(
        const ETGThrusterTimeMode Mode)
    {
        return Mode == ETGThrusterTimeMode::AbsoluteUtc
            ? AbsoluteUtcModeText
            : ElapsedTimeModeText;
    }

    bool TryParseTimeMode(
        const FString& Text,
        ETGThrusterTimeMode& OutMode)
    {
        if (Text.Equals(
                AbsoluteUtcModeText,
                ESearchCase::IgnoreCase))
        {
            OutMode = ETGThrusterTimeMode::AbsoluteUtc;
            return true;
        }

        if (Text.Equals(
                ElapsedTimeModeText,
                ESearchCase::IgnoreCase))
        {
            OutMode = ETGThrusterTimeMode::ElapsedSimulationTime;
            return true;
        }

        return false;
    }

    FString ProfileSourceToString(
        const ETGScalarProfileSource Source)
    {
        return Source == ETGScalarProfileSource::CsvProfile
            ? CsvSourceText
            : ConstantSourceText;
    }

    bool TryParseProfileSource(
        const FString& Text,
        ETGScalarProfileSource& OutSource)
    {
        if (Text.Equals(
                CsvSourceText,
                ESearchCase::IgnoreCase))
        {
            OutSource = ETGScalarProfileSource::CsvProfile;
            return true;
        }

        if (Text.Equals(
                ConstantSourceText,
                ESearchCase::IgnoreCase))
        {
            OutSource = ETGScalarProfileSource::Constant;
            return true;
        }

        return false;
    }

    void ResetCombo(
        UComboBoxString* Combo,
        const TArray<FString>& Options,
        const FString& SelectedOption)
    {
        if (Combo == nullptr)
        {
            return;
        }

        Combo->ClearOptions();

        for (const FString& Option : Options)
        {
            Combo->AddOption(Option);
        }

        if (!SelectedOption.IsEmpty())
        {
            Combo->SetSelectedOption(SelectedOption);
        }
    }

    FDateTime ResolveThrusterTime(
        const FTGSimulationScenario& Scenario,
        const ETGThrusterTimeMode Mode,
        const FDateTime& AbsoluteUtc,
        const double ElapsedSeconds)
    {
        if (Mode == ETGThrusterTimeMode::AbsoluteUtc)
        {
            if (AbsoluteUtc != FDateTime::MinValue())
            {
                return AbsoluteUtc;
            }
        }

        const FDateTime StartUtc =
            Scenario.ScenarioAndSolver.StartUtc != FDateTime::MinValue()
                ? Scenario.ScenarioAndSolver.StartUtc
                : FDateTime(2000, 1, 1);
        const double SafeElapsedSeconds = FMath::IsFinite(ElapsedSeconds)
            ? ElapsedSeconds
            : 0.0;

        return StartUtc + FTimespan::FromSeconds(SafeElapsedSeconds);
    }

    double ResolveElapsedSeconds(
        const FTGSimulationScenario& Scenario,
        const FDateTime& AbsoluteUtc)
    {
        const FDateTime StartUtc =
            Scenario.ScenarioAndSolver.StartUtc != FDateTime::MinValue()
                ? Scenario.ScenarioAndSolver.StartUtc
                : FDateTime(2000, 1, 1);

        return (AbsoluteUtc - StartUtc)
            .GetTotalSeconds();
    }
}

void UTGConfigActuatorsWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    if (SWITCH_ActuatorType != nullptr)
    {
        SWITCH_ActuatorType->SetActiveWidgetIndex(0);
    }

    for (UScrollBox* ScrollBox :
         {
             SCROLL_ThrusterList.Get(),
             SCROLL_ThrusterEditor.Get(),
             SCROLL_ReactionWheelList.Get(),
             SCROLL_ReactionWheelEditor.Get()
         })
    {
        if (ScrollBox != nullptr)
        {
            FScrollBoxStyle ScrollBoxStyle =
                TGUiTheme::MakeScrollBoxStyle();
            ScrollBoxStyle.BarThickness = 8.0f;
            ScrollBox->SetWidgetStyle(ScrollBoxStyle);

            FScrollBarStyle ScrollBarStyle =
                TGUiTheme::MakeScrollBarStyle();
            ScrollBarStyle.Thickness = 8.0f;
            ScrollBox->SetWidgetBarStyle(ScrollBarStyle);
            ScrollBox->SetScrollbarThickness(FVector2D(8.0f, 8.0f));
            ScrollBox->SetScrollbarPadding(
                FMargin(6.0f, 4.0f, 4.0f, 4.0f));
            ScrollBox->SetAlwaysShowScrollbarTrack(true);
            ScrollBox->SetAnimateWheelScrolling(true);
            ScrollBox->SetAllowOverscroll(false);
            ScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
            ScrollBox->SetWheelScrollMultiplier(4.0f);
        }
    }

    if (INPUT_ThrusterIgnitionUtc != nullptr)
    {
        INPUT_ThrusterIgnitionUtc->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigActuatorsWidgetBase::
                HandleThrusterIgnitionUtcCommitted);
        INPUT_ThrusterIgnitionUtc->OnUtcValidationFailed.AddUniqueDynamic(
            this,
            &UTGConfigActuatorsWidgetBase::
                HandleThrusterUtcValidationFailed);
    }

    if (INPUT_ThrusterShutdownUtc != nullptr)
    {
        INPUT_ThrusterShutdownUtc->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigActuatorsWidgetBase::
                HandleThrusterShutdownUtcCommitted);
        INPUT_ThrusterShutdownUtc->OnUtcValidationFailed.AddUniqueDynamic(
            this,
            &UTGConfigActuatorsWidgetBase::
                HandleThrusterUtcValidationFailed);
    }

    BTN_ClearThrusterThrustCsv->OnClicked.AddUniqueDynamic(
        this,
        &UTGConfigActuatorsWidgetBase::
            HandleClearThrusterThrustCsvClicked);

    BTN_ClearThrusterIspCsv->OnClicked.AddUniqueDynamic(
        this,
        &UTGConfigActuatorsWidgetBase::
            HandleClearThrusterIspCsvClicked);

    TGConfigActuatorsPrivate::ConfigureCsvPreview(
        VIEW_ThrusterThrustCsvPreview);
    TGConfigActuatorsPrivate::ConfigureCsvPreview(
        VIEW_ThrusterIspCsvPreview);

    HideThrusterEditor();
    HideReactionWheelEditor();
    ClearThrusterMessages();
    ClearReactionWheelMessages();
    RefreshFromCurrentDraft();
    UpdateActuatorTabPresentation();
}

void UTGConfigActuatorsWidgetBase::NativeDestruct()
{
    CloseActuatorDeleteDialog();
    Super::NativeDestruct();
}

void UTGConfigActuatorsWidgetBase::NativeTick(
    const FGeometry& MyGeometry,
    const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const UTGSimulationSubsystem* Subsystem =
        GetSimulationSubsystem();
    if (Subsystem != nullptr &&
        Subsystem->GetCurrentScenarioDraftRevision() !=
            LastObservedDraftRevision)
    {
        RefreshFromCurrentDraft();
    }

    UpdateActuatorTabPresentation();
}

FReply UTGConfigActuatorsWidgetBase::NativeOnPreviewMouseButtonDown(
    const FGeometry& InGeometry,
    const FPointerEvent& InMouseEvent)
{
    if (InMouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
    {
        return Super::NativeOnPreviewMouseButtonDown(
            InGeometry,
            InMouseEvent);
    }

    const FVector2D ScreenPosition =
        InMouseEvent.GetScreenSpacePosition();

    const auto IsInsideWidget =
        [&ScreenPosition](const UWidget* Widget)
        {
            if (Widget == nullptr ||
                Widget->GetVisibility() == ESlateVisibility::Collapsed)
            {
                return false;
            }

            const FGeometry& Geometry = Widget->GetCachedGeometry();
            const FVector2D LocalPosition =
                Geometry.AbsoluteToLocal(ScreenPosition);
            const FVector2D LocalSize = Geometry.GetLocalSize();
            return LocalPosition.X >= 0.0 && LocalPosition.Y >= 0.0 &&
                LocalPosition.X <= LocalSize.X &&
                LocalPosition.Y <= LocalSize.Y;
        };

    const bool bInsideThrustClear =
        IsInsideWidget(BTN_ClearThrusterThrustCsv);
    const bool bInsideIspClear =
        IsInsideWidget(BTN_ClearThrusterIspCsv);

    if (bInsideThrustClear)
    {
        HandleClearThrusterThrustCsvClicked();
        return FReply::Handled();
    }

    if (bInsideIspClear)
    {
        HandleClearThrusterIspCsvClicked();
        return FReply::Handled();
    }

    return Super::NativeOnPreviewMouseButtonDown(
        InGeometry,
        InMouseEvent);
}

UTGSimulationSubsystem*
UTGConfigActuatorsWidgetBase::
GetSimulationSubsystem() const
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return nullptr;
    }

    UGameInstance* GameInstance =
        World->GetGameInstance();

    if (GameInstance == nullptr)
    {
        return nullptr;
    }

    return GameInstance
        ->GetSubsystem<UTGSimulationSubsystem>();
}

bool UTGConfigActuatorsWidgetBase::
PullWorkingScenarioFromDraft()
{
    UTGSimulationSubsystem* Subsystem =
        GetSimulationSubsystem();

    if (Subsystem == nullptr)
    {
        const FText Error = FText::FromString(
            TEXT(
                "Scenario data is unavailable. Reopen the "
                "configuration screen and try again."));

        ShowThrusterError(Error);
        ShowReactionWheelError(Error);
        return false;
    }

    WorkingScenario =
        Subsystem->GetCurrentScenarioDraft();
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();

    return true;
}

bool UTGConfigActuatorsWidgetBase::
CommitWorkingScenarioToDraft()
{
    UTGSimulationSubsystem* Subsystem =
        GetSimulationSubsystem();

    if (Subsystem == nullptr)
    {
        const FText Error = FText::FromString(
            TEXT(
                "Scenario data is unavailable. Reopen the "
                "configuration screen and try again."));

        ShowThrusterError(Error);
        ShowReactionWheelError(Error);
        return false;
    }

    Subsystem->SetCurrentScenarioDraft(
        WorkingScenario);
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();

    return true;
}

void UTGConfigActuatorsWidgetBase::
RefreshFromCurrentDraft()
{
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    if (!WorkingScenario.Thrusters.IsValidIndex(
            SelectedThrusterIndex))
    {
        SelectedThrusterIndex = INDEX_NONE;
    }

    RebuildThrusterList();

    if (SelectedThrusterIndex == INDEX_NONE)
    {
        HideThrusterEditor();
    }
    else
    {
        LoadSelectedThrusterEditor();
    }

    if (!WorkingScenario.ReactionWheels.IsValidIndex(
            SelectedReactionWheelIndex))
    {
        SelectedReactionWheelIndex = INDEX_NONE;
    }

    RebuildReactionWheelList();

    if (SelectedReactionWheelIndex == INDEX_NONE)
    {
        HideReactionWheelEditor();
    }
    else
    {
        LoadSelectedReactionWheelEditor();
    }
}

bool UTGConfigActuatorsWidgetBase::
NavigateToScenarioReviewIssue_Implementation(
    const FTGScenarioReviewIssue& Issue)
{
    RefreshFromCurrentDraft();

    const FString Code = Issue.Code.ToString();
    const FString& Path = Issue.Path;
    const bool bWheelIssue = Code.StartsWith(TEXT("WHL-"))
        || Path.StartsWith(TEXT("ReactionWheels"));

    if (Issue.ArrayIndex != INDEX_NONE)
    {
        if (bWheelIssue
            && WorkingScenario.ReactionWheels.IsValidIndex(Issue.ArrayIndex))
        {
            SelectReactionWheelFromCurrentDraft(Issue.ArrayIndex);
        }
        else if (!bWheelIssue
            && WorkingScenario.Thrusters.IsValidIndex(Issue.ArrayIndex))
        {
            SelectThrusterFromCurrentDraft(Issue.ArrayIndex);
        }
    }

    UWidget* Target = nullptr;
    if (bWheelIssue)
    {
        if (Path.Contains(TEXT(".Name"))) Target = INPUT_WheelName;
        else if (Path.Contains(TEXT("MountComponent"))) Target = COMBO_WheelMountComponent;
        else if (Path.Contains(TEXT(".Axis"))) Target = INPUT_WheelAxis;
        else if (Path.Contains(TEXT("InitialMomentum"))) Target = INPUT_WheelInitialMomentum;
        else if (Path.Contains(TEXT("MaximumAbsoluteMomentum"))) Target = INPUT_WheelMaximumMomentum;
        else Target = BOX_SelectedReactionWheelEditor;

        if (Issue.Severity == ETGScenarioReviewSeverity::Error)
            ShowReactionWheelError(Issue.Message);
        else
            ShowReactionWheelWarning(Issue.Message);
    }
    else
    {
        if (Path.Contains(TEXT(".Name"))) Target = INPUT_ThrusterName;
        else if (Path.Contains(TEXT(".Mode"))) Target = COMBO_ThrusterMode;
        else if (Path.Contains(TEXT("MountComponent"))) Target = COMBO_ThrusterMountComponent;
        else if (Path.Contains(TEXT("PropellantComponent"))) Target = COMBO_ThrusterPropellantComponent;
        else if (Path.Contains(TEXT("ApplicationPoint"))) Target = INPUT_ThrusterApplicationPoint;
        else if (Path.Contains(TEXT("Direction"))) Target = INPUT_ThrusterDirection;
        else if (Path.Contains(TEXT("Ignition")))
            Target = COMBO_ThrusterIgnitionTimeMode != nullptr &&
                    COMBO_ThrusterIgnitionTimeMode->GetSelectedOption().Equals(
                        TGConfigActuatorsPrivate::AbsoluteUtcModeText)
                ? static_cast<UWidget*>(INPUT_ThrusterIgnitionUtc)
                : static_cast<UWidget*>(INPUT_ThrusterIgnitionValue);
        else if (Path.Contains(TEXT("Shutdown")))
            Target = COMBO_ThrusterShutdownTimeMode != nullptr &&
                    COMBO_ThrusterShutdownTimeMode->GetSelectedOption().Equals(
                        TGConfigActuatorsPrivate::AbsoluteUtcModeText)
                ? static_cast<UWidget*>(INPUT_ThrusterShutdownUtc)
                : static_cast<UWidget*>(INPUT_ThrusterShutdownValue);
        else if (Path.Contains(TEXT("PrescribedThrust")))
            Target = Path.Contains(TEXT("Csv")) ? static_cast<UWidget*>(BTN_BrowseThrusterThrustCsv) : static_cast<UWidget*>(INPUT_ThrusterConstantThrust);
        else if (Path.Contains(TEXT("PrescribedSpecificImpulse")))
            Target = Path.Contains(TEXT("Csv")) ? static_cast<UWidget*>(BTN_BrowseThrusterIspCsv) : static_cast<UWidget*>(INPUT_ThrusterConstantIsp);
        else if (Path.Contains(TEXT("MaximumThrust"))) Target = INPUT_ThrusterMaximumThrust;
        else Target = BOX_SelectedThrusterEditor;

        if (Issue.Severity == ETGScenarioReviewSeverity::Error)
            ShowThrusterError(Issue.Message);
        else
            ShowThrusterWarning(Issue.Message);
    }

    if (Target == nullptr)
        return false;
    Target->SetIsEnabled(true);
    Target->SetKeyboardFocus();
    return true;
}

void UTGConfigActuatorsWidgetBase::
UpdateActuatorTabPresentation()
{
    if (SWITCH_ActuatorType == nullptr)
    {
        return;
    }

    const bool bThrustersSelected =
        SWITCH_ActuatorType->GetActiveWidgetIndex() == 0;
    const FLinearColor SelectedColor(
        0.806952f,
        0.879623f,
        0.913099f,
        1.0f);
    const FLinearColor InactiveColor(
        0.396755f,
        0.445201f,
        0.491021f,
        1.0f);

    if (BTN_ThrustersTab != nullptr)
    {
        BTN_ThrustersTab->SetRenderOpacity(
            bThrustersSelected ? 1.0f : 0.72f);
    }

    if (TXT_ThrustersTab != nullptr)
    {
        TXT_ThrustersTab->SetColorAndOpacity(
            bThrustersSelected
                ? SelectedColor
                : InactiveColor);
    }

    if (BTN_ReactionWheelsTab != nullptr)
    {
        BTN_ReactionWheelsTab->SetRenderOpacity(
            bThrustersSelected ? 0.72f : 1.0f);
    }

    if (TXT_ReactionWheelsTab != nullptr)
    {
        TXT_ReactionWheelsTab->SetColorAndOpacity(
            bThrustersSelected
                ? InactiveColor
                : SelectedColor);
    }
}

// ============================================================================
// Thrusters
// ============================================================================

void UTGConfigActuatorsWidgetBase::
AddThrusterFromCurrentDraft()
{
    if (!CommitPendingThrusterDirection())
    {
        return;
    }

    ClearThrusterMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    int32 NewThrusterIndex = INDEX_NONE;
    FText Error;

    if (!UTGActuatorEditingLibrary::
            AddDefaultThruster(
                WorkingScenario,
                NewThrusterIndex,
                Error))
    {
        ShowThrusterError(Error);
        return;
    }

    SelectedThrusterIndex =
        NewThrusterIndex;

    if (!CommitWorkingScenarioToDraft())
    {
        return;
    }

    RebuildThrusterList();
    LoadSelectedThrusterEditor();
}

void UTGConfigActuatorsWidgetBase::
DeleteSelectedThrusterFromCurrentDraft()
{
    if (!CommitPendingThrusterDirection())
    {
        return;
    }

    ClearThrusterMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    FTGThrusterConfig Thruster;
    if (!GetSelectedThruster(Thruster))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Select a thruster first.")));
        return;
    }

    OpenActuatorDeleteDialog(
        EPendingActuatorDelete::Thruster,
        Thruster.Name);
}

void UTGConfigActuatorsWidgetBase::
SelectThrusterFromCurrentDraft(
    const int32 ThrusterIndex)
{
    if (!CommitPendingThrusterDirection())
    {
        return;
    }

    ClearThrusterMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    if (!WorkingScenario.Thrusters.IsValidIndex(
            ThrusterIndex))
    {
        ShowThrusterError(
            FText::FromString(
                TEXT("The selected thruster no longer exists.")));
        return;
    }

    if (SelectedThrusterIndex == ThrusterIndex)
    {
        SelectedThrusterIndex = INDEX_NONE;
        RebuildThrusterList();
        HideThrusterEditor();
        return;
    }

    SelectedThrusterIndex = ThrusterIndex;
    RebuildThrusterList();
    LoadSelectedThrusterEditor();
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterNameFromCurrentDraft(
    const FText NewNameText)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.Name =
        NewNameText.ToString();

    ApplySelectedThrusterCandidate(
        Candidate,
        true);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterModeFromCurrentDraft(
    const FString SelectedMode)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    ETGThrusterMode NewMode;
    if (!TGConfigActuatorsPrivate::TryParseThrusterMode(
            SelectedMode,
            NewMode))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Unknown thruster mode.")));
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.Mode = NewMode;

    if (NewMode == ETGThrusterMode::Commanded &&
        (!FMath::IsFinite(Candidate.MaximumThrustNewtons) ||
         Candidate.MaximumThrustNewtons <= 0.0))
    {
        Candidate.MaximumThrustNewtons = 1.0;
    }

    if (NewMode == ETGThrusterMode::PrescribedProfile &&
        Candidate.PrescribedSpecificImpulse.Source ==
            ETGScalarProfileSource::Constant &&
        Candidate.PrescribedSpecificImpulse.ConstantValue <= 0.0)
    {
        Candidate.PrescribedSpecificImpulse.ConstantValue = 1.0;
    }

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterMountComponentFromCurrentDraft(
    FString NewMountComponentName)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.MountComponentName =
        MoveTemp(NewMountComponentName);

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterPropellantComponentFromCurrentDraft(
    FString NewPropellantComponentName)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.PropellantComponentName =
        MoveTemp(NewPropellantComponentName);

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
HandleThrusterApplicationPointCommitted(
    const FVector NewBackendValue)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.ApplicationPointMeters =
        NewBackendValue;

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
HandleThrusterDirectionCommitted(
    const FVector NewBackendValue)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    PendingThrusterDirection =
        NewBackendValue;

    bHasPendingThrusterDirection =
        true;

    ClearThrusterMessages();
}

void UTGConfigActuatorsWidgetBase::
ApplyPendingThrusterDirectionFromCurrentDraft()
{
    CommitPendingThrusterDirection();
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterIgnitionTimeModeFromCurrentDraft(
    const FString SelectedMode)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    ETGThrusterTimeMode NewMode;
    if (!TGConfigActuatorsPrivate::TryParseTimeMode(
            SelectedMode,
            NewMode))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Unknown ignition time mode.")));
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    FDateTime EffectiveTime =
        TGConfigActuatorsPrivate::ResolveThrusterTime(
            WorkingScenario,
            Candidate.IgnitionTimeMode,
            Candidate.IgnitionUtc,
            Candidate.IgnitionElapsedSeconds);

    if (EffectiveTime == FDateTime::MinValue())
    {
        EffectiveTime = FDateTime(2000, 1, 1);
    }

    Candidate.IgnitionTimeMode = NewMode;

    if (NewMode == ETGThrusterTimeMode::AbsoluteUtc)
    {
        Candidate.IgnitionUtc = EffectiveTime;
    }
    else
    {
        Candidate.IgnitionElapsedSeconds =
            TGConfigActuatorsPrivate::ResolveElapsedSeconds(
                WorkingScenario,
                EffectiveTime);
    }

    CommitSelectedThrusterDraftWithoutValidation(Candidate);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterIgnitionValueFromCurrentDraft(
    const FText NewValueText)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    // Preserve the current positive firing duration while the user edits
    // ignition. This prevents an otherwise-valid new ignition time from being
    // rejected only because the old shutdown time temporarily lies before it.
    FTimespan PreviousFiringDuration =
        FTimespan::FromSeconds(1.0);

    if (!Candidate.bNeverShutsDown)
    {
        const FDateTime PreviousIgnitionTime =
            TGConfigActuatorsPrivate::ResolveThrusterTime(
                WorkingScenario,
                Candidate.IgnitionTimeMode,
                Candidate.IgnitionUtc,
                Candidate.IgnitionElapsedSeconds);

        const FDateTime PreviousShutdownTime =
            TGConfigActuatorsPrivate::ResolveThrusterTime(
                WorkingScenario,
                Candidate.ShutdownTimeMode,
                Candidate.ShutdownUtc,
                Candidate.ShutdownElapsedSeconds);

        const FTimespan ExistingDuration =
            PreviousShutdownTime - PreviousIgnitionTime;

        if (ExistingDuration.GetTotalSeconds() > 0.0)
        {
            PreviousFiringDuration =
                ExistingDuration;
        }
    }

    if (Candidate.IgnitionTimeMode ==
        ETGThrusterTimeMode::ElapsedSimulationTime)
    {
        double ParsedValue = 0.0;
        FText ParseError;

        if (!UTGHudFormattingLibrary::ParseHudDouble(
                NewValueText,
                ParsedValue,
                ParseError))
        {
            LoadSelectedThrusterEditor();
            ShowThrusterError(ParseError);
            return;
        }

        Candidate.IgnitionElapsedSeconds =
            ParsedValue;
    }
    else
    {
        FDateTime ParsedUtc;
        FText ParseError;

        if (!UTGHudFormattingLibrary::ParseUtcDateTime(
                NewValueText,
                ParsedUtc,
                ParseError))
        {
            LoadSelectedThrusterEditor();
            ShowThrusterError(ParseError);
            return;
        }

        Candidate.IgnitionUtc =
            ParsedUtc;
    }

    if (!Candidate.bNeverShutsDown)
    {
        const FDateTime NewIgnitionTime =
            TGConfigActuatorsPrivate::ResolveThrusterTime(
                WorkingScenario,
                Candidate.IgnitionTimeMode,
                Candidate.IgnitionUtc,
                Candidate.IgnitionElapsedSeconds);

        const FDateTime NewShutdownTime =
            NewIgnitionTime + PreviousFiringDuration;

        if (Candidate.ShutdownTimeMode ==
            ETGThrusterTimeMode::AbsoluteUtc)
        {
            Candidate.ShutdownUtc =
                NewShutdownTime;
        }
        else
        {
            Candidate.ShutdownElapsedSeconds =
                TGConfigActuatorsPrivate::ResolveElapsedSeconds(
                    WorkingScenario,
                    NewShutdownTime);
        }
    }

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
HandleThrusterIgnitionUtcCommitted(
    const FText& Text,
    const ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnCleared)
    {
        LoadSelectedThrusterEditor();
        return;
    }

    CommitThrusterIgnitionValueFromCurrentDraft(Text);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterNeverShutsDownFromCurrentDraft(
    const bool bNewNeverShutsDown)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.bNeverShutsDown =
        bNewNeverShutsDown;

    if (!bNewNeverShutsDown)
    {
        const FDateTime IgnitionTime =
            TGConfigActuatorsPrivate::ResolveThrusterTime(
                WorkingScenario,
                Candidate.IgnitionTimeMode,
                Candidate.IgnitionUtc,
                Candidate.IgnitionElapsedSeconds);

        const FDateTime ShutdownTime =
            TGConfigActuatorsPrivate::ResolveThrusterTime(
                WorkingScenario,
                Candidate.ShutdownTimeMode,
                Candidate.ShutdownUtc,
                Candidate.ShutdownElapsedSeconds);

        if (ShutdownTime <= IgnitionTime)
        {
            Candidate.ShutdownTimeMode =
                Candidate.IgnitionTimeMode;

            if (Candidate.ShutdownTimeMode ==
                ETGThrusterTimeMode::AbsoluteUtc)
            {
                Candidate.ShutdownUtc =
                    IgnitionTime + FTimespan::FromSeconds(1.0);
            }
            else
            {
                Candidate.ShutdownElapsedSeconds =
                    Candidate.IgnitionElapsedSeconds + 1.0;
            }
        }
    }

    CommitSelectedThrusterDraftWithoutValidation(Candidate);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterShutdownTimeModeFromCurrentDraft(
    const FString SelectedMode)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    ETGThrusterTimeMode NewMode;
    if (!TGConfigActuatorsPrivate::TryParseTimeMode(
            SelectedMode,
            NewMode))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Unknown shutdown time mode.")));
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    FDateTime EffectiveTime =
        TGConfigActuatorsPrivate::ResolveThrusterTime(
            WorkingScenario,
            Candidate.ShutdownTimeMode,
            Candidate.ShutdownUtc,
            Candidate.ShutdownElapsedSeconds);

    const FDateTime IgnitionTime =
        TGConfigActuatorsPrivate::ResolveThrusterTime(
            WorkingScenario,
            Candidate.IgnitionTimeMode,
            Candidate.IgnitionUtc,
            Candidate.IgnitionElapsedSeconds);
    if (EffectiveTime <= IgnitionTime)
    {
        EffectiveTime = IgnitionTime + FTimespan::FromSeconds(1.0);
    }

    Candidate.ShutdownTimeMode = NewMode;

    if (NewMode == ETGThrusterTimeMode::AbsoluteUtc)
    {
        Candidate.ShutdownUtc = EffectiveTime;
    }
    else
    {
        Candidate.ShutdownElapsedSeconds =
            TGConfigActuatorsPrivate::ResolveElapsedSeconds(
                WorkingScenario,
                EffectiveTime);
    }

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterShutdownValueFromCurrentDraft(
    const FText NewValueText)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    if (Candidate.ShutdownTimeMode ==
        ETGThrusterTimeMode::ElapsedSimulationTime)
    {
        double ParsedValue = 0.0;
        FText ParseError;

        if (!UTGHudFormattingLibrary::ParseHudDouble(
                NewValueText,
                ParsedValue,
                ParseError))
        {
            LoadSelectedThrusterEditor();
            ShowThrusterError(ParseError);
            return;
        }

        Candidate.ShutdownElapsedSeconds =
            ParsedValue;
    }
    else
    {
        FDateTime ParsedUtc;
        FText ParseError;

        if (!UTGHudFormattingLibrary::ParseUtcDateTime(
                NewValueText,
                ParsedUtc,
                ParseError))
        {
            LoadSelectedThrusterEditor();
            ShowThrusterError(ParseError);
            return;
        }

        Candidate.ShutdownUtc = ParsedUtc;
    }

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
HandleThrusterShutdownUtcCommitted(
    const FText& Text,
    const ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnCleared)
    {
        LoadSelectedThrusterEditor();
        return;
    }

    CommitThrusterShutdownValueFromCurrentDraft(Text);
}

void UTGConfigActuatorsWidgetBase::
HandleThrusterUtcValidationFailed(
    const FText& ValidationMessage)
{
    LoadSelectedThrusterEditor();
    ShowThrusterError(ValidationMessage);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterThrustSourceFromCurrentDraft(
    const FString SelectedSource)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    ETGScalarProfileSource NewSource;
    if (!TGConfigActuatorsPrivate::TryParseProfileSource(
            SelectedSource,
            NewSource))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Unknown thrust source.")));
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.PrescribedThrust.Source =
        NewSource;

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
ClearThrusterThrustCsvFromCurrentDraft()
{
    HandleClearThrusterThrustCsvClicked();
}

void UTGConfigActuatorsWidgetBase::
HandleClearThrusterThrustCsvClicked()
{
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    if (!WorkingScenario.Thrusters.IsValidIndex(
            SelectedThrusterIndex))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Select a thruster first.")));
        return;
    }

    WorkingScenario
        .Thrusters[SelectedThrusterIndex]
        .PrescribedThrust
        .CsvFilePath
        .Reset();

    if (CommitWorkingScenarioToDraft())
    {
        RefreshFromCurrentDraft();
    }
}

void UTGConfigActuatorsWidgetBase::
BrowseThrusterThrustCsvFromCurrentDraft()
{
    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    FString SelectedPath;

    if (!SelectCsvFile(
            TEXT("Select thrust profile CSV"),
            Candidate.PrescribedThrust.CsvFilePath,
            SelectedPath))
    {
        return;
    }

    Candidate.PrescribedThrust.Source =
        ETGScalarProfileSource::CsvProfile;

    Candidate.PrescribedThrust.CsvFilePath =
        MoveTemp(SelectedPath);

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterConstantThrustFromCurrentDraft(
    const FText NewValueText)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    double ParsedValue = 0.0;
    FText ParseError;

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            NewValueText,
            ParsedValue,
            ParseError))
    {
        LoadSelectedThrusterEditor();
        ShowThrusterError(ParseError);
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.PrescribedThrust.Source =
        ETGScalarProfileSource::Constant;

    Candidate.PrescribedThrust.ConstantValue =
        ParsedValue;

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterIspSourceFromCurrentDraft(
    const FString SelectedSource)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    ETGScalarProfileSource NewSource;
    if (!TGConfigActuatorsPrivate::TryParseProfileSource(
            SelectedSource,
            NewSource))
    {
        ShowThrusterError(
            FText::FromString(
                TEXT("Unknown specific impulse source.")));
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.PrescribedSpecificImpulse.Source =
        NewSource;

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
BrowseThrusterIspCsvFromCurrentDraft()
{
    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    FString SelectedPath;

    if (!SelectCsvFile(
            TEXT("Select specific impulse profile CSV"),
            Candidate.PrescribedSpecificImpulse.CsvFilePath,
            SelectedPath))
    {
        return;
    }

    Candidate.PrescribedSpecificImpulse.Source =
        ETGScalarProfileSource::CsvProfile;

    Candidate.PrescribedSpecificImpulse.CsvFilePath =
        MoveTemp(SelectedPath);

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
ClearThrusterIspCsvFromCurrentDraft()
{
    HandleClearThrusterIspCsvClicked();
}

void UTGConfigActuatorsWidgetBase::
HandleClearThrusterIspCsvClicked()
{
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    if (!WorkingScenario.Thrusters.IsValidIndex(
            SelectedThrusterIndex))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Select a thruster first.")));
        return;
    }

    WorkingScenario
        .Thrusters[SelectedThrusterIndex]
        .PrescribedSpecificImpulse
        .CsvFilePath
        .Reset();

    if (CommitWorkingScenarioToDraft())
    {
        RefreshFromCurrentDraft();
    }
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterConstantIspFromCurrentDraft(
    const FText NewValueText)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    double ParsedValue = 0.0;
    FText ParseError;

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            NewValueText,
            ParsedValue,
            ParseError))
    {
        LoadSelectedThrusterEditor();
        ShowThrusterError(ParseError);
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.PrescribedSpecificImpulse.Source =
        ETGScalarProfileSource::Constant;

    Candidate.PrescribedSpecificImpulse.ConstantValue =
        ParsedValue;

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
CommitThrusterMaximumThrustFromCurrentDraft(
    const FText NewValueText)
{
    if (bRefreshingThrusterEditor)
    {
        return;
    }

    double ParsedValue = 0.0;
    FText ParseError;

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            NewValueText,
            ParsedValue,
            ParseError))
    {
        LoadSelectedThrusterEditor();
        ShowThrusterError(ParseError);
        return;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.MaximumThrustNewtons =
        ParsedValue;

    ApplySelectedThrusterCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
RebuildThrusterList()
{
    if (VBOX_ThrusterList == nullptr)
    {
        return;
    }

    VBOX_ThrusterList->ClearChildren();

    UClass* RowClass =
        LoadClass<UTGActuatorListEntryWidgetBase>(
            nullptr,
            TGConfigActuatorsPrivate::ActuatorRowClassPath);

    if (RowClass == nullptr)
    {
        ShowThrusterError(
            FText::FromString(
                TEXT("The thruster list could not be displayed.")));
        return;
    }

    for (int32 Index = 0;
         Index < WorkingScenario.Thrusters.Num();
         ++Index)
    {
        UTGActuatorListEntryWidgetBase* Row = nullptr;

        if (APlayerController* OwningPlayer =
                GetOwningPlayer())
        {
            Row = CreateWidget<UTGActuatorListEntryWidgetBase>(
                OwningPlayer,
                RowClass);
        }
        else
        {
            Row = CreateWidget<UTGActuatorListEntryWidgetBase>(
                GetWorld(),
                RowClass);
        }

        if (Row == nullptr)
        {
            continue;
        }

        Row->InitializeActuatorListEntry(
            Index,
            WorkingScenario.Thrusters[Index].Name,
            this,
            false);
        Row->SetActuatorListEntrySelected(
            Index == SelectedThrusterIndex);

        if (UVerticalBoxSlot* RowSlot =
                VBOX_ThrusterList->AddChildToVerticalBox(Row))
        {
            RowSlot->SetHorizontalAlignment(HAlign_Fill);
        }
    }
}

void UTGConfigActuatorsWidgetBase::
LoadSelectedThrusterEditor()
{
    FTGThrusterConfig Thruster;

    if (!GetSelectedThruster(Thruster))
    {
        SelectedThrusterIndex = INDEX_NONE;
        HideThrusterEditor();
        return;
    }

    bRefreshingThrusterEditor = true;

    if (BOX_SelectedThrusterEditor != nullptr)
    {
        BOX_SelectedThrusterEditor->SetVisibility(
            ESlateVisibility::Visible);
    }

    if (TXT_SelectedThrusterTitle != nullptr)
    {
        TXT_SelectedThrusterTitle->SetText(
            FText::FromString(Thruster.Name));
    }

    if (INPUT_ThrusterName != nullptr)
    {
        INPUT_ThrusterName->SetText(
            FText::FromString(Thruster.Name));
    }

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_ThrusterMode,
        {
            TGConfigActuatorsPrivate::PrescribedModeText,
            TGConfigActuatorsPrivate::CommandedModeText
        },
        TGConfigActuatorsPrivate::ThrusterModeToString(
            Thruster.Mode));

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_ThrusterMountComponent,
        UTGActuatorEditingLibrary::
            GetActuatorMountComponentNames(WorkingScenario),
        Thruster.MountComponentName);

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_ThrusterPropellantComponent,
        UTGActuatorEditingLibrary::
            GetVariableMassComponentNames(WorkingScenario),
        Thruster.PropellantComponentName);

    SetVectorWidgetValue(
        INPUT_ThrusterApplicationPoint,
        Thruster.ApplicationPointMeters,
        TEXT("thruster application point"));

    SetVectorWidgetValue(
        INPUT_ThrusterDirection,
        Thruster.Direction,
        TEXT("thruster direction"));

    PendingThrusterDirection =
        Thruster.Direction;

    bHasPendingThrusterDirection =
        false;

    if (TXT_ThrusterDirectionStatus != nullptr)
    {
        TXT_ThrusterDirectionStatus->SetText(
            FText::GetEmpty());

        TXT_ThrusterDirectionStatus->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_ThrusterIgnitionTimeMode,
        {
            TGConfigActuatorsPrivate::ElapsedTimeModeText,
            TGConfigActuatorsPrivate::AbsoluteUtcModeText
        },
        TGConfigActuatorsPrivate::TimeModeToString(
            Thruster.IgnitionTimeMode));

    if (TXT_ThrusterIgnitionValueLabel != nullptr)
    {
        TXT_ThrusterIgnitionValueLabel->SetText(
            FText::FromString(
                Thruster.IgnitionTimeMode ==
                    ETGThrusterTimeMode::AbsoluteUtc
                    ? TEXT("Ignition UTC")
                    : TEXT("Ignition Time [s]")));
    }

    if (SWITCH_ThrusterIgnitionInput != nullptr)
    {
        SWITCH_ThrusterIgnitionInput->SetActiveWidgetIndex(
            Thruster.IgnitionTimeMode ==
                    ETGThrusterTimeMode::AbsoluteUtc
                ? 1
                : 0);
    }

    if (INPUT_ThrusterIgnitionValue != nullptr)
    {
        INPUT_ThrusterIgnitionValue->SetText(
            SWITCH_ThrusterIgnitionInput == nullptr &&
                    Thruster.IgnitionTimeMode ==
                        ETGThrusterTimeMode::AbsoluteUtc
                ? UTGHudFormattingLibrary::FormatUtcDateTime(
                    Thruster.IgnitionUtc)
                : FormatDouble(Thruster.IgnitionElapsedSeconds));
    }

    if (INPUT_ThrusterIgnitionUtc != nullptr)
    {
        INPUT_ThrusterIgnitionUtc->SetText(
            UTGHudFormattingLibrary::FormatUtcDateTime(
                Thruster.IgnitionUtc));
    }

    if (CHECK_ThrusterNeverShutsDown != nullptr)
    {
        CHECK_ThrusterNeverShutsDown->SetIsChecked(
            Thruster.bNeverShutsDown);
    }

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_ThrusterShutdownTimeMode,
        {
            TGConfigActuatorsPrivate::ElapsedTimeModeText,
            TGConfigActuatorsPrivate::AbsoluteUtcModeText
        },
        TGConfigActuatorsPrivate::TimeModeToString(
            Thruster.ShutdownTimeMode));

    if (TXT_ThrusterShutdownValueLabel != nullptr)
    {
        TXT_ThrusterShutdownValueLabel->SetText(
            FText::FromString(
                Thruster.ShutdownTimeMode ==
                    ETGThrusterTimeMode::AbsoluteUtc
                    ? TEXT("Shutdown UTC")
                    : TEXT("Shutdown Time [s]")));
    }

    if (SWITCH_ThrusterShutdownInput != nullptr)
    {
        SWITCH_ThrusterShutdownInput->SetActiveWidgetIndex(
            Thruster.ShutdownTimeMode ==
                    ETGThrusterTimeMode::AbsoluteUtc
                ? 1
                : 0);
    }

    if (INPUT_ThrusterShutdownValue != nullptr)
    {
        INPUT_ThrusterShutdownValue->SetText(
            SWITCH_ThrusterShutdownInput == nullptr &&
                    Thruster.ShutdownTimeMode ==
                        ETGThrusterTimeMode::AbsoluteUtc
                ? UTGHudFormattingLibrary::FormatUtcDateTime(
                    Thruster.ShutdownUtc)
                : FormatDouble(Thruster.ShutdownElapsedSeconds));
    }

    if (INPUT_ThrusterShutdownUtc != nullptr)
    {
        INPUT_ThrusterShutdownUtc->SetText(
            UTGHudFormattingLibrary::FormatUtcDateTime(
                Thruster.ShutdownUtc));
    }

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_ThrusterThrustSource,
        {
            TGConfigActuatorsPrivate::ConstantSourceText,
            TGConfigActuatorsPrivate::CsvSourceText
        },
        TGConfigActuatorsPrivate::ProfileSourceToString(
            Thruster.PrescribedThrust.Source));

    if (INPUT_ThrusterConstantThrust != nullptr)
    {
        INPUT_ThrusterConstantThrust->SetText(
            FormatDouble(
                Thruster.PrescribedThrust.ConstantValue));
    }

    if (TXT_ThrusterThrustCsvPath != nullptr)
    {
        TXT_ThrusterThrustCsvPath->SetText(
            FText::FromString(
                Thruster.PrescribedThrust.CsvFilePath.IsEmpty()
                    ? TEXT("(No file selected)")
                    : Thruster.PrescribedThrust.CsvFilePath));
    }

    if (BTN_ClearThrusterThrustCsv != nullptr)
    {
        BTN_ClearThrusterThrustCsv->SetIsEnabled(true);
    }

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_ThrusterIspSource,
        {
            TGConfigActuatorsPrivate::ConstantSourceText,
            TGConfigActuatorsPrivate::CsvSourceText
        },
        TGConfigActuatorsPrivate::ProfileSourceToString(
            Thruster.PrescribedSpecificImpulse.Source));

    if (INPUT_ThrusterConstantIsp != nullptr)
    {
        INPUT_ThrusterConstantIsp->SetText(
            FormatDouble(
                Thruster.PrescribedSpecificImpulse.ConstantValue));
    }

    if (TXT_ThrusterIspCsvPath != nullptr)
    {
        TXT_ThrusterIspCsvPath->SetText(
            FText::FromString(
                Thruster.PrescribedSpecificImpulse.CsvFilePath.IsEmpty()
                    ? TEXT("(No file selected)")
                    : Thruster.PrescribedSpecificImpulse.CsvFilePath));
    }

    if (BTN_ClearThrusterIspCsv != nullptr)
    {
        BTN_ClearThrusterIspCsv->SetIsEnabled(true);
    }

    FString NormalizedProfilePath;
    FText ProfileError;
    const bool bThrustProfileValid =
        !Thruster.PrescribedThrust.CsvFilePath.IsEmpty() &&
        UTGActuatorEditingLibrary::ValidateThrustProfileCsv(
            Thruster.PrescribedThrust.CsvFilePath,
            NormalizedProfilePath,
            ProfileError);

    if (TXT_ThrusterThrustCsvStatus != nullptr)
    {
        TXT_ThrusterThrustCsvStatus->SetText(
            Thruster.PrescribedThrust.CsvFilePath.IsEmpty()
                ? FText::FromString(TEXT("No thrust profile selected."))
                : bThrustProfileValid
                    ? FText::FromString(TEXT("Thrust profile loaded."))
                    : ProfileError);
    }

    TGConfigActuatorsPrivate::RefreshCsvPreview(
        VBOX_ThrusterThrustCsvPreview,
        VIEW_ThrusterThrustCsvPreview,
        bThrustProfileValid,
        NormalizedProfilePath,
        TEXT("Thrust [N]"));

    NormalizedProfilePath.Reset();
    ProfileError = FText::GetEmpty();
    const bool bSpecificImpulseProfileValid =
        !Thruster.PrescribedSpecificImpulse.CsvFilePath.IsEmpty() &&
        UTGActuatorEditingLibrary::ValidateSpecificImpulseProfileCsv(
            Thruster.PrescribedSpecificImpulse.CsvFilePath,
            NormalizedProfilePath,
            ProfileError);

    if (TXT_ThrusterIspCsvStatus != nullptr)
    {
        TXT_ThrusterIspCsvStatus->SetText(
            Thruster.PrescribedSpecificImpulse.CsvFilePath.IsEmpty()
                ? FText::FromString(
                    TEXT("No specific impulse profile selected."))
                : bSpecificImpulseProfileValid
                    ? FText::FromString(
                        TEXT("Specific impulse profile loaded."))
                    : ProfileError);
    }

    TGConfigActuatorsPrivate::RefreshCsvPreview(
        VBOX_ThrusterIspCsvPreview,
        VIEW_ThrusterIspCsvPreview,
        bSpecificImpulseProfileValid,
        NormalizedProfilePath,
        TEXT("Specific Impulse [s]"));

    if (INPUT_ThrusterMaximumThrust != nullptr)
    {
        INPUT_ThrusterMaximumThrust->SetText(
            FormatDouble(
                Thruster.MaximumThrustNewtons));
    }

    UpdateThrusterConditionalVisibility(Thruster);

    bRefreshingThrusterEditor = false;
    ClearThrusterMessages();

    FTGThrusterConfig ValidationThruster = Thruster;
    if (ValidationThruster.PrescribedThrust.Source ==
            ETGScalarProfileSource::CsvProfile &&
        ValidationThruster.PrescribedThrust.CsvFilePath
            .TrimStartAndEnd()
            .IsEmpty())
    {
        ValidationThruster.PrescribedThrust.Source =
            ETGScalarProfileSource::Constant;
        ValidationThruster.PrescribedThrust.ConstantValue =
            FMath::Max(
                0.0,
                ValidationThruster.PrescribedThrust.ConstantValue);
    }
    if (ValidationThruster.PrescribedSpecificImpulse.Source ==
            ETGScalarProfileSource::CsvProfile &&
        ValidationThruster.PrescribedSpecificImpulse.CsvFilePath
            .TrimStartAndEnd()
            .IsEmpty())
    {
        ValidationThruster.PrescribedSpecificImpulse.Source =
            ETGScalarProfileSource::Constant;
        ValidationThruster.PrescribedSpecificImpulse.ConstantValue =
            FMath::Max(
                1.0,
                ValidationThruster.PrescribedSpecificImpulse.ConstantValue);
    }

    FTGThrusterConfig AcceptedThruster;
    FText Warning;
    FText Error;

    if (!UTGActuatorEditingLibrary::ValidateThruster(
            WorkingScenario,
            ValidationThruster,
            AcceptedThruster,
            Warning,
            Error))
    {
        ShowThrusterError(Error);
    }
    else if (!Warning.IsEmpty())
    {
        ShowThrusterWarning(Warning);
    }
}

void UTGConfigActuatorsWidgetBase::
HideThrusterEditor()
{
    if (BOX_SelectedThrusterEditor != nullptr)
    {
        BOX_SelectedThrusterEditor->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

void UTGConfigActuatorsWidgetBase::
UpdateThrusterConditionalVisibility(
    const FTGThrusterConfig& Thruster)
{
    if (VBOX_ThrusterShutdownControls != nullptr)
    {
        VBOX_ThrusterShutdownControls->SetVisibility(
            Thruster.bNeverShutsDown
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }

    const bool bPrescribed =
        Thruster.Mode == ETGThrusterMode::PrescribedProfile;

    if (VBOX_ThrusterPrescribedPerformance != nullptr)
    {
        VBOX_ThrusterPrescribedPerformance->SetVisibility(
            bPrescribed
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (VBOX_ThrusterCommandedPerformance != nullptr)
    {
        VBOX_ThrusterCommandedPerformance->SetVisibility(
            bPrescribed
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }

    const bool bThrustCsv =
        Thruster.PrescribedThrust.Source ==
            ETGScalarProfileSource::CsvProfile;

    if (VBOX_ThrusterConstantThrustRow != nullptr)
    {
        VBOX_ThrusterConstantThrustRow->SetVisibility(
            bThrustCsv
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }

    if (VBOX_ThrusterThrustCsvRow != nullptr)
    {
        VBOX_ThrusterThrustCsvRow->SetVisibility(
            bThrustCsv
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    const bool bIspCsv =
        Thruster.PrescribedSpecificImpulse.Source ==
            ETGScalarProfileSource::CsvProfile;

    if (VBOX_ThrusterConstantIspRow != nullptr)
    {
        VBOX_ThrusterConstantIspRow->SetVisibility(
            bIspCsv
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::Visible);
    }

    if (VBOX_ThrusterIspCsvRow != nullptr)
    {
        VBOX_ThrusterIspCsvRow->SetVisibility(
            bIspCsv
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }
}

bool UTGConfigActuatorsWidgetBase::
GetSelectedThruster(
    FTGThrusterConfig& OutThruster) const
{
    if (!WorkingScenario.Thrusters.IsValidIndex(
            SelectedThrusterIndex))
    {
        OutThruster = FTGThrusterConfig{};
        return false;
    }

    OutThruster =
        WorkingScenario.Thrusters[SelectedThrusterIndex];

    return true;
}

bool UTGConfigActuatorsWidgetBase::
PrepareSelectedThrusterFromCurrentDraft(
    FTGThrusterConfig& OutThruster)
{
    ClearThrusterMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return false;
    }

    if (!GetSelectedThruster(OutThruster))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Select a thruster first.")));
        return false;
    }

    if (bHasPendingThrusterDirection)
    {
        OutThruster.Direction = PendingThrusterDirection;
    }

    return true;
}

bool UTGConfigActuatorsWidgetBase::
CommitPendingThrusterDirection()
{
    if (!bHasPendingThrusterDirection)
    {
        return true;
    }

    FTGThrusterConfig Candidate;
    if (!PrepareSelectedThrusterFromCurrentDraft(Candidate))
    {
        return false;
    }

    Candidate.Direction = PendingThrusterDirection;
    const bool bApplied = ApplySelectedThrusterCandidate(Candidate, false);
    if (bApplied)
    {
        bHasPendingThrusterDirection = false;
    }
    return bApplied;
}

bool UTGConfigActuatorsWidgetBase::
ApplySelectedThrusterCandidate(
    const FTGThrusterConfig& Candidate,
    const bool bRebuildListAfterSuccess)
{
    const bool bAwaitingThrustCsv =
        Candidate.PrescribedThrust.Source ==
            ETGScalarProfileSource::CsvProfile &&
        Candidate.PrescribedThrust.CsvFilePath.TrimStartAndEnd().IsEmpty();
    const bool bAwaitingIspCsv =
        Candidate.PrescribedSpecificImpulse.Source ==
            ETGScalarProfileSource::CsvProfile &&
        Candidate.PrescribedSpecificImpulse.CsvFilePath
            .TrimStartAndEnd()
            .IsEmpty();

    FTGThrusterConfig ValidationCandidate = Candidate;
    if (bAwaitingThrustCsv)
    {
        ValidationCandidate.PrescribedThrust.Source =
            ETGScalarProfileSource::Constant;
        ValidationCandidate.PrescribedThrust.ConstantValue =
            FMath::Max(
                0.0,
                ValidationCandidate.PrescribedThrust.ConstantValue);
    }
    if (bAwaitingIspCsv)
    {
        ValidationCandidate.PrescribedSpecificImpulse.Source =
            ETGScalarProfileSource::Constant;
        ValidationCandidate.PrescribedSpecificImpulse.ConstantValue =
            FMath::Max(
                1.0,
                ValidationCandidate.PrescribedSpecificImpulse.ConstantValue);
    }

    FTGThrusterConfig AcceptedThruster;
    FText Warning;
    FText Error;

    if (!UTGActuatorEditingLibrary::ApplyThruster(
            WorkingScenario,
            SelectedThrusterIndex,
            ValidationCandidate,
            AcceptedThruster,
            Warning,
            Error))
    {
        // Restore the last accepted values first, then present the reason.
        // LoadSelectedThrusterEditor clears the message area as part of its
        // normal refresh, so the error must be shown after the reload.
        LoadSelectedThrusterEditor();
        ShowThrusterError(Error);
        return false;
    }

    if (bAwaitingThrustCsv)
    {
        AcceptedThruster.PrescribedThrust =
            Candidate.PrescribedThrust;
    }
    if (bAwaitingIspCsv)
    {
        AcceptedThruster.PrescribedSpecificImpulse =
            Candidate.PrescribedSpecificImpulse;
    }

    WorkingScenario.Thrusters[SelectedThrusterIndex] =
        AcceptedThruster;

    if (!CommitWorkingScenarioToDraft())
    {
        return false;
    }

    if (bRebuildListAfterSuccess)
    {
        RebuildThrusterList();
    }

    LoadSelectedThrusterEditor();

    if (!Warning.IsEmpty())
    {
        ShowThrusterWarning(Warning);
    }

    return true;
}

bool UTGConfigActuatorsWidgetBase::
CommitSelectedThrusterDraftWithoutValidation(
    const FTGThrusterConfig& Candidate)
{
    if (!WorkingScenario.Thrusters.IsValidIndex(
            SelectedThrusterIndex))
    {
        ShowThrusterError(
            FText::FromString(TEXT("Select a thruster first.")));
        return false;
    }

    WorkingScenario.Thrusters[SelectedThrusterIndex] = Candidate;

    if (!CommitWorkingScenarioToDraft())
    {
        return false;
    }

    LoadSelectedThrusterEditor();
    return true;
}

bool UTGConfigActuatorsWidgetBase::
SelectCsvFile(
    const FString& DialogTitle,
    const FString& ExistingPath,
    FString& OutSelectedPath) const
{
    FString DefaultDirectory;
    FString DefaultFileName;

    if (!ExistingPath.IsEmpty())
    {
        DefaultDirectory =
            FPaths::GetPath(ExistingPath);

        DefaultFileName =
            FPaths::GetCleanFilename(ExistingPath);
    }

    return UTGFileDialogLibrary::OpenSingleFileDialog(
        DialogTitle,
        DefaultDirectory,
        DefaultFileName,
        TEXT("CSV profile"),
        {TEXT("csv")},
        false,
        OutSelectedPath);
}

void UTGConfigActuatorsWidgetBase::
SetVectorWidgetValue(
    UUserWidget* VectorWidget,
    const FVector& Value,
    const FString& FieldNameForError)
{
    if (VectorWidget == nullptr)
    {
        return;
    }

    UFunction* SetVectorValueFunction =
        VectorWidget->FindFunction(
            TEXT("SetVectorValue"));

    if (SetVectorValueFunction == nullptr)
    {
        ShowThrusterError(
            FText::FromString(
                FString::Printf(
                    TEXT("The %s input could not be refreshed."),
                    *FieldNameForError)));
        return;
    }

    struct FSetVectorValueParameters
    {
        FVector NewBackendValue;
    };

    FSetVectorValueParameters Parameters;
    Parameters.NewBackendValue = Value;

    VectorWidget->ProcessEvent(
        SetVectorValueFunction,
        &Parameters);
}

void UTGConfigActuatorsWidgetBase::
ClearThrusterMessages()
{
    if (TXT_ThrusterWarning != nullptr)
    {
        TXT_ThrusterWarning->SetText(
            FText::GetEmpty());

        TXT_ThrusterWarning->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    if (TXT_ThrusterError != nullptr)
    {
        TXT_ThrusterError->SetText(
            FText::GetEmpty());

        TXT_ThrusterError->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

void UTGConfigActuatorsWidgetBase::
ShowThrusterWarning(
    const FText& Message)
{
    if (TXT_ThrusterWarning == nullptr)
    {
        return;
    }

    TXT_ThrusterWarning->SetText(Message);
    TXT_ThrusterWarning->SetVisibility(
        Message.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::Visible);
}

void UTGConfigActuatorsWidgetBase::
ShowThrusterError(
    const FText& Message)
{
    if (TXT_ThrusterError == nullptr)
    {
        return;
    }

    TXT_ThrusterError->SetText(Message);
    TXT_ThrusterError->SetVisibility(
        Message.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::Visible);
}

// ============================================================================
// Reaction wheels - preserved working implementation
// ============================================================================

void UTGConfigActuatorsWidgetBase::
AddReactionWheelFromCurrentDraft()
{
    if (!CommitPendingReactionWheelAxis())
    {
        return;
    }

    ClearReactionWheelMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    int32 NewWheelIndex = INDEX_NONE;
    FText Error;

    if (!UTGActuatorEditingLibrary::AddDefaultReactionWheel(
            WorkingScenario,
            NewWheelIndex,
            Error))
    {
        ShowReactionWheelError(Error);
        return;
    }

    SelectedReactionWheelIndex =
        NewWheelIndex;

    if (!CommitWorkingScenarioToDraft())
    {
        return;
    }

    RebuildReactionWheelList();
    LoadSelectedReactionWheelEditor();
}

void UTGConfigActuatorsWidgetBase::
DeleteSelectedReactionWheelFromCurrentDraft()
{
    if (!CommitPendingReactionWheelAxis())
    {
        return;
    }

    ClearReactionWheelMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    FTGReactionWheelConfig Wheel;
    if (!GetSelectedWheel(Wheel))
    {
        ShowReactionWheelError(
            FText::FromString(TEXT("Select a wheel first.")));
        return;
    }

    OpenActuatorDeleteDialog(
        EPendingActuatorDelete::ReactionWheel,
        Wheel.Name);
}

void UTGConfigActuatorsWidgetBase::
SelectReactionWheelFromCurrentDraft(
    const int32 ReactionWheelIndex)
{
    if (!CommitPendingReactionWheelAxis())
    {
        return;
    }

    ClearReactionWheelMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    if (!WorkingScenario.ReactionWheels.IsValidIndex(
            ReactionWheelIndex))
    {
        ShowReactionWheelError(
            FText::FromString(
                TEXT(
                    "The selected wheel no longer "
                    "exists.")));
        return;
    }

    if (SelectedReactionWheelIndex == ReactionWheelIndex)
    {
        SelectedReactionWheelIndex = INDEX_NONE;
        RebuildReactionWheelList();
        HideReactionWheelEditor();
        return;
    }

    SelectedReactionWheelIndex = ReactionWheelIndex;
    RebuildReactionWheelList();
    LoadSelectedReactionWheelEditor();
}

void UTGConfigActuatorsWidgetBase::
CommitReactionWheelNameFromCurrentDraft(
    const FText NewNameText)
{
    if (bRefreshingReactionWheelEditor)
    {
        return;
    }

    FTGReactionWheelConfig Candidate;
    if (!PrepareSelectedWheelFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.Name =
        NewNameText.ToString();

    ApplySelectedWheelCandidate(
        Candidate,
        true);
}

void UTGConfigActuatorsWidgetBase::
CommitReactionWheelMountComponentFromCurrentDraft(
    FString NewMountComponentName)
{
    if (bRefreshingReactionWheelEditor)
    {
        return;
    }

    FTGReactionWheelConfig Candidate;
    if (!PrepareSelectedWheelFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.MountComponentName =
        MoveTemp(NewMountComponentName);

    ApplySelectedWheelCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
HandleReactionWheelAxisCommitted(
    const FVector NewBackendValue)
{
    if (bRefreshingReactionWheelEditor)
    {
        return;
    }

    PendingReactionWheelAxis =
        NewBackendValue;

    bHasPendingReactionWheelAxis =
        true;

    ClearReactionWheelMessages();
}

void UTGConfigActuatorsWidgetBase::
ApplyPendingReactionWheelAxisFromCurrentDraft()
{
    CommitPendingReactionWheelAxis();
}

void UTGConfigActuatorsWidgetBase::
CommitReactionWheelInitialMomentumFromCurrentDraft(
    const FText NewValueText)
{
    if (bRefreshingReactionWheelEditor)
    {
        return;
    }

    double ParsedValue = 0.0;
    FText ParseError;

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            NewValueText,
            ParsedValue,
            ParseError))
    {
        LoadSelectedReactionWheelEditor();
        ShowReactionWheelError(ParseError);
        return;
    }

    FTGReactionWheelConfig Candidate;
    if (!PrepareSelectedWheelFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.InitialMomentumNewtonMeterSeconds =
        ParsedValue;

    ApplySelectedWheelCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
CommitReactionWheelMaximumMomentumFromCurrentDraft(
    const FText NewValueText)
{
    if (bRefreshingReactionWheelEditor)
    {
        return;
    }

    double ParsedValue = 0.0;
    FText ParseError;

    if (!UTGHudFormattingLibrary::ParseHudDouble(
            NewValueText,
            ParsedValue,
            ParseError))
    {
        LoadSelectedReactionWheelEditor();
        ShowReactionWheelError(ParseError);
        return;
    }

    FTGReactionWheelConfig Candidate;
    if (!PrepareSelectedWheelFromCurrentDraft(Candidate))
    {
        return;
    }

    Candidate.MaximumAbsoluteMomentumNewtonMeterSeconds =
        ParsedValue;

    ApplySelectedWheelCandidate(
        Candidate,
        false);
}

void UTGConfigActuatorsWidgetBase::
RebuildReactionWheelList()
{
    if (VBOX_ReactionWheelList == nullptr)
    {
        return;
    }

    VBOX_ReactionWheelList->ClearChildren();

    UClass* RowClass =
        LoadClass<UTGActuatorListEntryWidgetBase>(
            nullptr,
            TGConfigActuatorsPrivate::ActuatorRowClassPath);

    if (RowClass == nullptr)
    {
        ShowReactionWheelError(
            FText::FromString(
                TEXT(
                    "The wheel list could not be "
                    "displayed.")));
        return;
    }

    for (int32 Index = 0;
         Index < WorkingScenario.ReactionWheels.Num();
         ++Index)
    {
        UTGActuatorListEntryWidgetBase* Row = nullptr;

        if (APlayerController* OwningPlayer =
                GetOwningPlayer())
        {
            Row = CreateWidget<UTGActuatorListEntryWidgetBase>(
                OwningPlayer,
                RowClass);
        }
        else
        {
            Row = CreateWidget<UTGActuatorListEntryWidgetBase>(
                GetWorld(),
                RowClass);
        }

        if (Row == nullptr)
        {
            continue;
        }

        Row->InitializeActuatorListEntry(
            Index,
            WorkingScenario.ReactionWheels[Index].Name,
            this,
            true);
        Row->SetActuatorListEntrySelected(
            Index == SelectedReactionWheelIndex);

        if (UVerticalBoxSlot* RowSlot =
                VBOX_ReactionWheelList->AddChildToVerticalBox(Row))
        {
            RowSlot->SetHorizontalAlignment(HAlign_Fill);
        }
    }
}

void UTGConfigActuatorsWidgetBase::
LoadSelectedReactionWheelEditor()
{
    FTGReactionWheelConfig Wheel;

    if (!GetSelectedWheel(Wheel))
    {
        SelectedReactionWheelIndex = INDEX_NONE;
        HideReactionWheelEditor();
        return;
    }

    bRefreshingReactionWheelEditor = true;

    if (BOX_SelectedReactionWheelEditor != nullptr)
    {
        BOX_SelectedReactionWheelEditor->SetVisibility(
            ESlateVisibility::Visible);
    }

    if (TXT_SelectedReactionWheelTitle != nullptr)
    {
        TXT_SelectedReactionWheelTitle->SetText(
            FText::FromString(Wheel.Name));
    }

    if (INPUT_WheelName != nullptr)
    {
        INPUT_WheelName->SetText(
            FText::FromString(Wheel.Name));
    }

    TGConfigActuatorsPrivate::ResetCombo(
        COMBO_WheelMountComponent,
        UTGActuatorEditingLibrary::
            GetActuatorMountComponentNames(WorkingScenario),
        Wheel.MountComponentName);

    SetWheelAxisWidgetValue(Wheel.Axis);

    PendingReactionWheelAxis =
        Wheel.Axis;

    bHasPendingReactionWheelAxis =
        false;

    if (TXT_WheelAxisStatus != nullptr)
    {
        TXT_WheelAxisStatus->SetText(
            FText::GetEmpty());

        TXT_WheelAxisStatus->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    if (INPUT_WheelInitialMomentum != nullptr)
    {
        INPUT_WheelInitialMomentum->SetText(
            FormatDouble(
                Wheel.InitialMomentumNewtonMeterSeconds));
    }

    if (INPUT_WheelMaximumMomentum != nullptr)
    {
        INPUT_WheelMaximumMomentum->SetText(
            FormatDouble(
                Wheel.MaximumAbsoluteMomentumNewtonMeterSeconds));
    }

    bRefreshingReactionWheelEditor = false;
    ClearReactionWheelMessages();

    FTGReactionWheelConfig AcceptedWheel;
    FText Warning;
    FText Error;

    if (!UTGActuatorEditingLibrary::ValidateReactionWheel(
            WorkingScenario,
            Wheel,
            AcceptedWheel,
            Warning,
            Error))
    {
        ShowReactionWheelError(Error);
    }
    else if (!Warning.IsEmpty())
    {
        ShowReactionWheelWarning(Warning);
    }
}

void UTGConfigActuatorsWidgetBase::
HideReactionWheelEditor()
{
    if (BOX_SelectedReactionWheelEditor != nullptr)
    {
        BOX_SelectedReactionWheelEditor->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

bool UTGConfigActuatorsWidgetBase::
GetSelectedWheel(
    FTGReactionWheelConfig& OutWheel) const
{
    if (!WorkingScenario.ReactionWheels.IsValidIndex(
            SelectedReactionWheelIndex))
    {
        OutWheel = FTGReactionWheelConfig{};
        return false;
    }

    OutWheel =
        WorkingScenario.ReactionWheels[
            SelectedReactionWheelIndex];

    return true;
}

bool UTGConfigActuatorsWidgetBase::
PrepareSelectedWheelFromCurrentDraft(
    FTGReactionWheelConfig& OutWheel)
{
    ClearReactionWheelMessages();

    if (!PullWorkingScenarioFromDraft())
    {
        return false;
    }

    if (!GetSelectedWheel(OutWheel))
    {
        ShowReactionWheelError(
            FText::FromString(
                TEXT("Select a wheel first.")));
        return false;
    }

    if (bHasPendingReactionWheelAxis)
    {
        OutWheel.Axis = PendingReactionWheelAxis;
    }

    return true;
}

bool UTGConfigActuatorsWidgetBase::
CommitPendingReactionWheelAxis()
{
    if (!bHasPendingReactionWheelAxis)
    {
        return true;
    }

    FTGReactionWheelConfig Candidate;
    if (!PrepareSelectedWheelFromCurrentDraft(Candidate))
    {
        return false;
    }

    Candidate.Axis = PendingReactionWheelAxis;
    const bool bApplied = ApplySelectedWheelCandidate(Candidate, false);
    if (bApplied)
    {
        bHasPendingReactionWheelAxis = false;
    }
    return bApplied;
}

bool UTGConfigActuatorsWidgetBase::
ApplySelectedWheelCandidate(
    const FTGReactionWheelConfig& Candidate,
    const bool bRebuildListAfterSuccess)
{
    FTGReactionWheelConfig AcceptedWheel;
    FText Warning;
    FText Error;

    if (!UTGActuatorEditingLibrary::ApplyReactionWheel(
            WorkingScenario,
            SelectedReactionWheelIndex,
            Candidate,
            AcceptedWheel,
            Warning,
            Error))
    {
        LoadSelectedReactionWheelEditor();
        ShowReactionWheelError(Error);
        return false;
    }

    if (!CommitWorkingScenarioToDraft())
    {
        return false;
    }

    if (bRebuildListAfterSuccess)
    {
        RebuildReactionWheelList();
    }

    LoadSelectedReactionWheelEditor();

    if (!Warning.IsEmpty())
    {
        ShowReactionWheelWarning(Warning);
    }

    return true;
}

void UTGConfigActuatorsWidgetBase::
SetWheelAxisWidgetValue(
    const FVector& Value)
{
    if (INPUT_WheelAxis == nullptr)
    {
        return;
    }

    UFunction* SetVectorValueFunction =
        INPUT_WheelAxis->FindFunction(
            TEXT("SetVectorValue"));

    if (SetVectorValueFunction == nullptr)
    {
        ShowReactionWheelError(
            FText::FromString(
                TEXT(
                    "The wheel direction input could not be "
                    "refreshed.")));
        return;
    }

    struct FSetVectorValueParameters
    {
        FVector NewBackendValue;
    };

    FSetVectorValueParameters Parameters;
    Parameters.NewBackendValue = Value;

    INPUT_WheelAxis->ProcessEvent(
        SetVectorValueFunction,
        &Parameters);
}

void UTGConfigActuatorsWidgetBase::
OpenActuatorDeleteDialog(
    const EPendingActuatorDelete DeleteType,
    const FString& ActuatorName)
{
    CloseActuatorDeleteDialog();

    UClass* DialogClass = LoadClass<UUserWidget>(
        nullptr,
        TGConfigActuatorsPrivate::ConfirmDialogClassPath);
    if (DialogClass == nullptr)
    {
        const FText Error = FText::FromString(
            TEXT("The deletion confirmation could not be displayed."));
        if (DeleteType == EPendingActuatorDelete::Thruster)
        {
            ShowThrusterError(Error);
        }
        else
        {
            ShowReactionWheelError(Error);
        }
        return;
    }

    ActiveDeleteDialog = GetOwningPlayer() != nullptr
        ? CreateWidget<UUserWidget>(GetOwningPlayer(), DialogClass)
        : CreateWidget<UUserWidget>(GetWorld(), DialogClass);
    if (ActiveDeleteDialog == nullptr)
    {
        return;
    }

    PendingActuatorDelete = DeleteType;

    if (UFunction* InitializeFunction =
            ActiveDeleteDialog->FindFunction(TEXT("InitializeDialog")))
    {
        FStructOnScope Parameters(InitializeFunction);
        auto SetTextParameter = [InitializeFunction, &Parameters](
                                    const FName Name,
                                    const FText& Value)
        {
            if (FTextProperty* Property =
                    FindFProperty<FTextProperty>(InitializeFunction, Name))
            {
                Property->SetPropertyValue_InContainer(
                    Parameters.GetStructMemory(),
                    Value);
            }
        };

        SetTextParameter(
            TEXT("Title"),
            FText::FromString(
                DeleteType == EPendingActuatorDelete::Thruster
                    ? TEXT("Delete Thruster?")
                    : TEXT("Delete Wheel?")));
        SetTextParameter(
            TEXT("Message"),
            FText::FromString(
                FString::Printf(
                    TEXT(
                        "Delete \"%s\"? This action cannot be undone."),
                    *ActuatorName)));
        SetTextParameter(
            TEXT("ConfirmLabel"),
            FText::FromString(TEXT("Delete")));

        ActiveDeleteDialog->ProcessEvent(
            InitializeFunction,
            Parameters.GetStructMemory());
    }

    auto BindDialogEvent = [this](
                               const FName PropertyName,
                               const FName HandlerName)
    {
        if (FMulticastDelegateProperty* Property =
                FindFProperty<FMulticastDelegateProperty>(
                    ActiveDeleteDialog->GetClass(),
                    PropertyName))
        {
            if (FMulticastScriptDelegate* Delegate =
                    Property->ContainerPtrToValuePtr<
                        FMulticastScriptDelegate>(ActiveDeleteDialog))
            {
                FScriptDelegate ScriptDelegate;
                ScriptDelegate.BindUFunction(this, HandlerName);
                Delegate->AddUnique(ScriptDelegate);
            }
        }
    };

    BindDialogEvent(
        TEXT("OnConfirmDialogAccepted"),
        GET_FUNCTION_NAME_CHECKED(
            UTGConfigActuatorsWidgetBase,
            HandleActuatorDeleteDialogAccepted));
    BindDialogEvent(
        TEXT("OnConfirmDialogCancelled"),
        GET_FUNCTION_NAME_CHECKED(
            UTGConfigActuatorsWidgetBase,
            HandleActuatorDeleteDialogCancelled));

    ActiveDeleteDialog->AddToViewport(100);
}

void UTGConfigActuatorsWidgetBase::
CloseActuatorDeleteDialog()
{
    if (ActiveDeleteDialog != nullptr)
    {
        ActiveDeleteDialog->RemoveFromParent();
        ActiveDeleteDialog = nullptr;
    }

    PendingActuatorDelete = EPendingActuatorDelete::None;
}

void UTGConfigActuatorsWidgetBase::
HandleActuatorDeleteDialogAccepted()
{
    const EPendingActuatorDelete DeleteType = PendingActuatorDelete;
    CloseActuatorDeleteDialog();

    if (DeleteType == EPendingActuatorDelete::Thruster)
    {
        ConfirmDeleteSelectedThruster();
    }
    else if (DeleteType == EPendingActuatorDelete::ReactionWheel)
    {
        ConfirmDeleteSelectedReactionWheel();
    }
}

void UTGConfigActuatorsWidgetBase::
HandleActuatorDeleteDialogCancelled()
{
    CloseActuatorDeleteDialog();
}

void UTGConfigActuatorsWidgetBase::
ConfirmDeleteSelectedThruster()
{
    ClearThrusterMessages();
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    FText Error;
    if (!UTGActuatorEditingLibrary::DeleteThruster(
            WorkingScenario,
            SelectedThrusterIndex,
            Error))
    {
        ShowThrusterError(Error);
        return;
    }

    if (!CommitWorkingScenarioToDraft())
    {
        return;
    }

    SelectedThrusterIndex = INDEX_NONE;
    RebuildThrusterList();
    HideThrusterEditor();
}

void UTGConfigActuatorsWidgetBase::
ConfirmDeleteSelectedReactionWheel()
{
    ClearReactionWheelMessages();
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    FText Error;
    if (!UTGActuatorEditingLibrary::DeleteReactionWheel(
            WorkingScenario,
            SelectedReactionWheelIndex,
            Error))
    {
        ShowReactionWheelError(Error);
        return;
    }

    if (!CommitWorkingScenarioToDraft())
    {
        return;
    }

    SelectedReactionWheelIndex = INDEX_NONE;
    RebuildReactionWheelList();
    HideReactionWheelEditor();
}

FText UTGConfigActuatorsWidgetBase::
FormatDouble(
    const double Value)
{
    return UTGHudFormattingLibrary::FormatDoubleForHud(
        Value,
        12);
}

void UTGConfigActuatorsWidgetBase::
ClearReactionWheelMessages()
{
    if (TXT_ReactionWheelWarning != nullptr)
    {
        TXT_ReactionWheelWarning->SetText(
            FText::GetEmpty());

        TXT_ReactionWheelWarning->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    if (TXT_ReactionWheelError != nullptr)
    {
        TXT_ReactionWheelError->SetText(
            FText::GetEmpty());

        TXT_ReactionWheelError->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

void UTGConfigActuatorsWidgetBase::
ShowReactionWheelWarning(
    const FText& Message)
{
    if (TXT_ReactionWheelWarning == nullptr)
    {
        return;
    }

    TXT_ReactionWheelWarning->SetText(Message);
    TXT_ReactionWheelWarning->SetVisibility(
        Message.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::Visible);
}

void UTGConfigActuatorsWidgetBase::
ShowReactionWheelError(
    const FText& Message)
{
    if (TXT_ReactionWheelError == nullptr)
    {
        return;
    }

    TXT_ReactionWheelError->SetText(Message);
    TXT_ReactionWheelError->SetVisibility(
        Message.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::Visible);
}
