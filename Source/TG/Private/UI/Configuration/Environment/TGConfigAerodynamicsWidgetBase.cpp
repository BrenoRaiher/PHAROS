// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Environment/TGConfigAerodynamicsWidgetBase.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "UI/Common/TGFileDialogLibrary.h"
#include "UI/Configuration/Environment/TGEnvironmentEditingLibrary.h"
#include "UI/TGHudFormattingLibrary.h"

namespace TGConfigAerodynamicsPrivate
{
    constexpr int32 CsvPreviewRowLimit = 100;

    const FString InverseDistanceText = TEXT("Inverse Distance");
    const FString NearestRowText = TEXT("Nearest Row");
    const FString ConstantDragFallbackText = TEXT("Constant-Drag Fallback");

    void ResetCombo(
        UComboBoxString* Combo,
        const TArray<FString>& Options,
        const FString& Selected)
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

        if (!Selected.IsEmpty())
        {
            Combo->SetSelectedOption(Selected);
        }
    }

    FString InterpolationToString(
        ETGAerodynamicDatabaseInterpolation Value)
    {
        return Value == ETGAerodynamicDatabaseInterpolation::NearestRow
            ? NearestRowText
            : InverseDistanceText;
    }

    FString ExtrapolationToString(
        ETGAerodynamicDatabaseExtrapolation Value)
    {
        return Value == ETGAerodynamicDatabaseExtrapolation::NearestRow
            ? NearestRowText
            : ConstantDragFallbackText;
    }

    bool IsFiniteVector(const FVector& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    void ConfigurePreview(UMultiLineEditableTextBox* Preview)
    {
        if (Preview != nullptr)
        {
            Preview->SetIsReadOnly(true);
        }
    }

    TArray<FString> BuildDatabasePreviewColumns(
        const FTGSimulationScenario& Scenario)
    {
        TArray<FString> Result = {
            TEXT("Speed Ratio"),
            TEXT("Knudsen Number"),
            TEXT("Flow X (Spacecraft)"),
            TEXT("Flow Y (Spacecraft)"),
            TEXT("Flow Z (Spacecraft)")};

        int32 ArticulationIndex = 0;
        for (const FTGComponentConfig& Component : Scenario.Components)
        {
            for (const FTGJointDofConfig& Dof : Component.DegreesOfFreedom)
            {
                ++ArticulationIndex;
                const FString DofName = Dof.Name.TrimStartAndEnd();
                Result.Add(
                    DofName.IsEmpty()
                        ? FString::Printf(
                            TEXT("Articulation Coordinate %d"),
                            ArticulationIndex)
                        : FString::Printf(
                            TEXT("Articulation %d (%s)"),
                            ArticulationIndex,
                            *DofName));
            }
        }

        Result.Append({
            TEXT("Force Coefficient X"),
            TEXT("Force Coefficient Y"),
            TEXT("Force Coefficient Z"),
            TEXT("Moment Coefficient X"),
            TEXT("Moment Coefficient Y"),
            TEXT("Moment Coefficient Z")});
        return Result;
    }

    void RefreshPreview(
        UVerticalBox* Container,
        UMultiLineEditableTextBox* Preview,
        bool bSourceIsValid,
        const FString& CsvFilePath,
        const TArray<FString>& ColumnHeadings)
    {
        if (Preview == nullptr)
        {
            if (Container != nullptr)
            {
                Container->SetVisibility(ESlateVisibility::Collapsed);
            }
            return;
        }

        FText PreviewText;
        FText PreviewError;
        const bool bCanShow =
            bSourceIsValid &&
            UTGEnvironmentEditingLibrary::BuildCsvPreview(
                CsvFilePath,
                ColumnHeadings,
                CsvPreviewRowLimit,
                PreviewText,
                PreviewError);

        Preview->SetText(bCanShow ? PreviewText : FText::GetEmpty());
        if (Container != nullptr)
        {
            Container->SetVisibility(
                bCanShow
                    ? ESlateVisibility::Visible
                    : ESlateVisibility::Collapsed);
        }
        else
        {
            Preview->SetVisibility(
                bCanShow
                    ? ESlateVisibility::Visible
                    : ESlateVisibility::Collapsed);
        }
    }
}

void UTGConfigAerodynamicsWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    TGConfigAerodynamicsPrivate::ConfigurePreview(
        VIEW_DatabaseCsvPreview);

    if (SCROLL_AeroSetup != nullptr)
    {
        SCROLL_AeroSetup->SetScrollbarThickness(FVector2D(8.0f, 8.0f));
        SCROLL_AeroSetup->SetScrollbarPadding(
            FMargin(6.0f, 4.0f, 4.0f, 4.0f));
        SCROLL_AeroSetup->SetAnimateWheelScrolling(true);
        SCROLL_AeroSetup->SetAllowOverscroll(false);
        SCROLL_AeroSetup->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
        SCROLL_AeroSetup->SetWheelScrollMultiplier(4.0f);
    }

    if (CHECK_EnableAerodynamics != nullptr)
    {
        CHECK_EnableAerodynamics->OnCheckStateChanged.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleEnableAerodynamicsChanged);
    }

    if (INPUT_ReferenceArea != nullptr)
    {
        INPUT_ReferenceArea->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleReferenceAreaCommitted);
    }

    if (INPUT_ReferenceLength != nullptr)
    {
        INPUT_ReferenceLength->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleReferenceLengthCommitted);
    }

    if (INPUT_MinDynamicPressure != nullptr)
    {
        INPUT_MinDynamicPressure->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleMinimumDynamicPressureCommitted);
    }

    if (INPUT_MaxDynamicPressure != nullptr)
    {
        INPUT_MaxDynamicPressure->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleMaximumDynamicPressureCommitted);
    }

    if (CHECK_EnableConstantDragFallback != nullptr)
    {
        CHECK_EnableConstantDragFallback->OnCheckStateChanged.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleEnableConstantDragFallbackChanged);
    }

    if (INPUT_FallbackDragCoefficient != nullptr)
    {
        INPUT_FallbackDragCoefficient->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleFallbackDragCoefficientCommitted);
    }

    if (CHECK_EnableDatabase != nullptr)
    {
        CHECK_EnableDatabase->OnCheckStateChanged.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleEnableDatabaseChanged);
    }

    if (BTN_BrowseDatabaseCsv != nullptr)
    {
        BTN_BrowseDatabaseCsv->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleBrowseDatabaseClicked);
    }

    if (BTN_ClearDatabaseCsv != nullptr)
    {
        BTN_ClearDatabaseCsv->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleClearDatabaseClicked);
    }

    if (COMBO_DatabaseInterpolation != nullptr)
    {
        COMBO_DatabaseInterpolation->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleInterpolationSelectionChanged);
    }

    if (COMBO_DatabaseExtrapolation != nullptr)
    {
        COMBO_DatabaseExtrapolation->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleExtrapolationSelectionChanged);
    }

    if (INPUT_DatabaseNeighborCount != nullptr)
    {
        INPUT_DatabaseNeighborCount->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleNeighborCountCommitted);
    }

    if (INPUT_DatabaseInverseDistancePower != nullptr)
    {
        INPUT_DatabaseInverseDistancePower->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleInverseDistancePowerCommitted);
    }

    if (CHECK_UseMaximumNeighborDistance != nullptr)
    {
        CHECK_UseMaximumNeighborDistance->OnCheckStateChanged.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleUseMaximumNeighborDistanceChanged);
    }

    if (INPUT_MaximumNeighborDistance != nullptr)
    {
        INPUT_MaximumNeighborDistance->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAerodynamicsWidgetBase::HandleMaximumNeighborDistanceCommitted);
    }

    RefreshFromCurrentDraft();
}

void UTGConfigAerodynamicsWidgetBase::NativeTick(
    const FGeometry& MyGeometry,
    const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem != nullptr &&
        Subsystem->GetCurrentScenarioDraftRevision() !=
            LastObservedDraftRevision)
    {
        RefreshFromCurrentDraft();
    }
}

UTGSimulationSubsystem*
UTGConfigAerodynamicsWidgetBase::GetSimulationSubsystem() const
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return nullptr;
    }

    UGameInstance* GameInstance = World->GetGameInstance();
    if (GameInstance == nullptr)
    {
        return nullptr;
    }

    return GameInstance->GetSubsystem<UTGSimulationSubsystem>();
}

bool UTGConfigAerodynamicsWidgetBase::PullWorkingScenarioFromDraft()
{
    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        ShowError(FText::FromString(TEXT("The PHAROS simulation service is unavailable.")));
        return false;
    }

    WorkingScenario = Subsystem->GetCurrentScenarioDraft();
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    return true;
}

bool UTGConfigAerodynamicsWidgetBase::CommitWorkingScenarioToDraft()
{
    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr)
    {
        ShowError(FText::FromString(TEXT("The PHAROS simulation service is unavailable.")));
        return false;
    }

    Subsystem->SetCurrentScenarioDraft(WorkingScenario);
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    return true;
}

void UTGConfigAerodynamicsWidgetBase::RefreshFromCurrentDraft()
{
    ClearMessages();
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    RefreshUiFromWorkingScenario();
}

bool UTGConfigAerodynamicsWidgetBase::
NavigateToScenarioReviewIssue_Implementation(
    const FTGScenarioReviewIssue& Issue)
{
    RefreshFromCurrentDraft();

    const FString& Path = Issue.Path;
    UWidget* Target = nullptr;
    if (Path.Contains(TEXT("Database.bEnabled"))) Target = CHECK_EnableDatabase;
    else if (Path.Contains(TEXT("bEnabled"))) Target = CHECK_EnableAerodynamics;
    else if (Path.Contains(TEXT("ReferenceArea"))) Target = INPUT_ReferenceArea;
    else if (Path.Contains(TEXT("ReferenceLength"))) Target = INPUT_ReferenceLength;
    else if (Path.Contains(TEXT("MinimumDynamicPressure"))) Target = INPUT_MinDynamicPressure;
    else if (Path.Contains(TEXT("MaximumValidDynamicPressure"))) Target = INPUT_MaxDynamicPressure;
    else if (Path.Contains(TEXT("ConstantDrag"))) Target = CHECK_EnableConstantDragFallback;
    else if (Path.Contains(TEXT("DragCoefficient"))) Target = INPUT_FallbackDragCoefficient;
    else if (Path.Contains(TEXT("CsvFilePath"))) Target = BTN_BrowseDatabaseCsv;
    else if (Path.Contains(TEXT("Interpolation"))) Target = COMBO_DatabaseInterpolation;
    else if (Path.Contains(TEXT("Extrapolation"))) Target = COMBO_DatabaseExtrapolation;
    else if (Path.Contains(TEXT("NeighborCount"))) Target = INPUT_DatabaseNeighborCount;
    else if (Path.Contains(TEXT("InverseDistancePower"))) Target = INPUT_DatabaseInverseDistancePower;
    else if (Path.Contains(TEXT("MaximumNormalizedNeighborDistance"))) Target = INPUT_MaximumNeighborDistance;
    else if (Path.Contains(TEXT("MomentReferenceCenter"))) Target = INPUT_MomentReferenceCenter;

    if (Issue.Severity == ETGScenarioReviewSeverity::Error)
        ShowError(Issue.Message);
    else
        ShowWarning(Issue.Message);

    if (Target == nullptr)
        return false;
    Target->SetIsEnabled(true);
    Target->SetKeyboardFocus();
    return true;
}

void UTGConfigAerodynamicsWidgetBase::RefreshUiFromWorkingScenario()
{
    ClearMessages();
    TGuardValue<bool> RefreshGuard(bRefreshing, true);

    const FTGAerodynamicsConfig& Aero = WorkingScenario.Aerodynamics;
    const FTGAerodynamicDatabaseConfig& Database = Aero.Database;

    if (CHECK_EnableAerodynamics != nullptr)
    {
        CHECK_EnableAerodynamics->SetIsChecked(Aero.bEnabled);
    }

    if (SIZE_AerodynamicsContentCard != nullptr)
    {
        SIZE_AerodynamicsContentCard->SetIsEnabled(Aero.bEnabled);
    }

    if (INPUT_ReferenceArea != nullptr)
    {
        INPUT_ReferenceArea->SetText(FormatDouble(Aero.ReferenceAreaSquareMeters));
    }

    if (INPUT_ReferenceLength != nullptr)
    {
        INPUT_ReferenceLength->SetText(FormatDouble(Aero.ReferenceLengthMeters));
    }

    if (INPUT_MinDynamicPressure != nullptr)
    {
        INPUT_MinDynamicPressure->SetText(FormatDouble(Aero.MinimumDynamicPressurePascals));
    }

    if (INPUT_MaxDynamicPressure != nullptr)
    {
        INPUT_MaxDynamicPressure->SetText(FormatDouble(Aero.MaximumValidDynamicPressurePascals));
    }

    if (CHECK_EnableConstantDragFallback != nullptr)
    {
        CHECK_EnableConstantDragFallback->SetIsChecked(Aero.bEnableConstantDragFallback);
    }

    if (VBOX_ConstantDragFallbackDetails != nullptr)
    {
        VBOX_ConstantDragFallbackDetails->SetVisibility(
            Aero.bEnableConstantDragFallback
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (INPUT_FallbackDragCoefficient != nullptr)
    {
        INPUT_FallbackDragCoefficient->SetText(FormatDouble(Aero.FallbackDragCoefficient));
    }

    if (CHECK_EnableDatabase != nullptr)
    {
        CHECK_EnableDatabase->SetIsChecked(Database.bEnabled);
    }

    if (VBOX_DatabaseDetails != nullptr)
    {
        VBOX_DatabaseDetails->SetVisibility(
            Database.bEnabled
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (TXT_DatabaseCsvPath != nullptr)
    {
        TXT_DatabaseCsvPath->SetText(
            FText::FromString(
                Database.CsvFilePath.IsEmpty()
                    ? TEXT("(No file selected)")
                    : Database.CsvFilePath));
    }

    TGConfigAerodynamicsPrivate::ResetCombo(
        COMBO_DatabaseInterpolation,
        {
            TGConfigAerodynamicsPrivate::InverseDistanceText,
            TGConfigAerodynamicsPrivate::NearestRowText
        },
        TGConfigAerodynamicsPrivate::InterpolationToString(Database.Interpolation));

    TGConfigAerodynamicsPrivate::ResetCombo(
        COMBO_DatabaseExtrapolation,
        {
            TGConfigAerodynamicsPrivate::ConstantDragFallbackText,
            TGConfigAerodynamicsPrivate::NearestRowText
        },
        TGConfigAerodynamicsPrivate::ExtrapolationToString(Database.Extrapolation));

    if (VBOX_InverseDistanceSettings != nullptr)
    {
        VBOX_InverseDistanceSettings->SetVisibility(
            Database.Interpolation == ETGAerodynamicDatabaseInterpolation::InverseDistance
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (INPUT_DatabaseNeighborCount != nullptr)
    {
        INPUT_DatabaseNeighborCount->SetText(FormatInteger(Database.NeighborCount));
    }

    if (INPUT_DatabaseInverseDistancePower != nullptr)
    {
        INPUT_DatabaseInverseDistancePower->SetText(FormatDouble(Database.InverseDistancePower));
    }

    if (CHECK_UseMaximumNeighborDistance != nullptr)
    {
        CHECK_UseMaximumNeighborDistance->SetIsChecked(
            Database.bUseMaximumNormalizedNeighborDistance);
    }

    if (VBOX_MaximumNeighborDistanceRow != nullptr)
    {
        VBOX_MaximumNeighborDistanceRow->SetVisibility(
            Database.bUseMaximumNormalizedNeighborDistance
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (INPUT_MaximumNeighborDistance != nullptr)
    {
        INPUT_MaximumNeighborDistance->SetText(
            FormatDouble(Database.MaximumNormalizedNeighborDistance));
    }

    SetMomentReferenceCenterWidgetValue(
        Database.MomentReferenceCenterBodyMeters);

    RefreshDatabaseStatus();
}

void UTGConfigAerodynamicsWidgetBase::RefreshDatabaseStatus()
{
    const FTGAerodynamicDatabaseConfig& Database =
        WorkingScenario.Aerodynamics.Database;

    if (Database.CsvFilePath.IsEmpty())
    {
        if (TXT_DatabaseCsvStatus != nullptr)
        {
            TXT_DatabaseCsvStatus->SetText(
                FText::FromString(TEXT("No coefficient database selected.")));
        }
        TGConfigAerodynamicsPrivate::RefreshPreview(
            VBOX_DatabaseCsvPreview,
            VIEW_DatabaseCsvPreview,
            false,
            Database.CsvFilePath,
            {});
        return;
    }

    FText Summary;
    FText Error;
    const bool bDatabaseValid =
        UTGEnvironmentEditingLibrary::ValidateAerodynamicDatabaseCsv(
            Database.CsvFilePath,
            UTGEnvironmentEditingLibrary::GetFlattenedArticulationDofCount(
                WorkingScenario),
            Summary,
            Error);
    if (TXT_DatabaseCsvStatus != nullptr)
    {
        TXT_DatabaseCsvStatus->SetText(
            bDatabaseValid ? Summary : Error);
    }

    TGConfigAerodynamicsPrivate::RefreshPreview(
        VBOX_DatabaseCsvPreview,
        VIEW_DatabaseCsvPreview,
        bDatabaseValid,
        Database.CsvFilePath,
        TGConfigAerodynamicsPrivate::BuildDatabasePreviewColumns(
            WorkingScenario));
}

void UTGConfigAerodynamicsWidgetBase::HandleEnableAerodynamicsChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.Aerodynamics.bEnabled = bIsChecked;
    WorkingScenario.Atmosphere.bEnabled = bIsChecked;

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

bool UTGConfigAerodynamicsWidgetBase::ParsePositiveDouble(
    const FText& Text,
    const TCHAR* FieldName,
    double& OutValue,
    FText& OutError) const
{
    if (!UTGHudFormattingLibrary::ParseHudDouble(Text, OutValue, OutError))
    {
        return false;
    }

    if (!FMath::IsFinite(OutValue) || OutValue <= 0.0)
    {
        OutError = FText::FromString(
            FString::Printf(TEXT("%s must be a positive finite value."), FieldName));
        return false;
    }

    return true;
}

bool UTGConfigAerodynamicsWidgetBase::ParseNonNegativeDouble(
    const FText& Text,
    const TCHAR* FieldName,
    double& OutValue,
    FText& OutError) const
{
    if (!UTGHudFormattingLibrary::ParseHudDouble(Text, OutValue, OutError))
    {
        return false;
    }

    if (!FMath::IsFinite(OutValue) || OutValue < 0.0)
    {
        OutError = FText::FromString(
            FString::Printf(TEXT("%s must be a nonnegative finite value."), FieldName));
        return false;
    }

    return true;
}

void UTGConfigAerodynamicsWidgetBase::HandleReferenceAreaCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    double Value = 0.0;
    FText Error;
    if (!ParsePositiveDouble(Text, TEXT("Reference area"), Value, Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.ReferenceAreaSquareMeters = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleReferenceLengthCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    double Value = 0.0;
    FText Error;
    if (!ParsePositiveDouble(Text, TEXT("Reference length"), Value, Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.ReferenceLengthMeters = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleMinimumDynamicPressureCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    double Value = 0.0;
    FText Error;
    if (!ParseNonNegativeDouble(Text, TEXT("Minimum dynamic pressure"), Value, Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.MinimumDynamicPressurePascals = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleMaximumDynamicPressureCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    double Value = 0.0;
    FText Error;
    if (!ParseNonNegativeDouble(Text, TEXT("Maximum valid dynamic pressure"), Value, Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.MaximumValidDynamicPressurePascals = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleEnableConstantDragFallbackChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.Aerodynamics.bEnableConstantDragFallback = bIsChecked;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleFallbackDragCoefficientCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    double Value = 0.0;
    FText Error;
    if (!ParsePositiveDouble(Text, TEXT("Fallback drag coefficient"), Value, Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.FallbackDragCoefficient = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleEnableDatabaseChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.Aerodynamics.Database.bEnabled = bIsChecked;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

bool UTGConfigAerodynamicsWidgetBase::BrowseDatabaseCsv(FString& OutSelectedPath) const
{
    const FString& CurrentPath = WorkingScenario.Aerodynamics.Database.CsvFilePath;
    const FString DefaultDirectory = CurrentPath.IsEmpty()
        ? FPaths::ProjectDir()
        : FPaths::GetPath(CurrentPath);

    return UTGFileDialogLibrary::OpenSingleFileDialog(
        TEXT("Select Aerodynamic Coefficient Database CSV"),
        DefaultDirectory,
        FString(),
        TEXT("CSV Files"),
        {TEXT("csv")},
        false,
        OutSelectedPath);
}

void UTGConfigAerodynamicsWidgetBase::HandleBrowseDatabaseClicked()
{
    FString SelectedPath;
    if (!BrowseDatabaseCsv(SelectedPath))
    {
        return;
    }

    FText Summary;
    FText Error;
    if (!UTGEnvironmentEditingLibrary::ValidateAerodynamicDatabaseCsv(
            SelectedPath,
            UTGEnvironmentEditingLibrary::GetFlattenedArticulationDofCount(
                WorkingScenario),
            Summary,
            Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.Database.CsvFilePath = NormalizePath(SelectedPath);
    WorkingScenario.Aerodynamics.Database.Rows.Reset();
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleClearDatabaseClicked()
{
    WorkingScenario.Aerodynamics.Database.CsvFilePath.Reset();
    WorkingScenario.Aerodynamics.Database.Rows.Reset();
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleInterpolationSelectionChanged(
    FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    (void)SelectionType;
    if (bRefreshing || SelectedItem.IsEmpty())
    {
        return;
    }

    WorkingScenario.Aerodynamics.Database.Interpolation =
        SelectedItem == TGConfigAerodynamicsPrivate::NearestRowText
            ? ETGAerodynamicDatabaseInterpolation::NearestRow
            : ETGAerodynamicDatabaseInterpolation::InverseDistance;

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleExtrapolationSelectionChanged(
    FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    (void)SelectionType;
    if (bRefreshing || SelectedItem.IsEmpty())
    {
        return;
    }

    WorkingScenario.Aerodynamics.Database.Extrapolation =
        SelectedItem == TGConfigAerodynamicsPrivate::NearestRowText
            ? ETGAerodynamicDatabaseExtrapolation::NearestRow
            : ETGAerodynamicDatabaseExtrapolation::ConstantDragFallback;

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleNeighborCountCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    int32 Value = 0;
    FText Error;
    if (!UTGHudFormattingLibrary::ParseHudInteger(Text, Value, Error) || Value <= 0)
    {
        RefreshUiFromWorkingScenario();
        ShowError(
            Error.IsEmpty()
                ? FText::FromString(TEXT("Neighbor count must be a positive integer."))
                : Error);
        return;
    }

    WorkingScenario.Aerodynamics.Database.NeighborCount = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleInverseDistancePowerCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    double Value = 0.0;
    FText Error;
    if (!ParsePositiveDouble(Text, TEXT("Inverse-distance power"), Value, Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.Database.InverseDistancePower = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleUseMaximumNeighborDistanceChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.Aerodynamics.Database.bUseMaximumNormalizedNeighborDistance = bIsChecked;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleMaximumNeighborDistanceCommitted(
    const FText& Text,
    ETextCommit::Type CommitMethod)
{
    (void)CommitMethod;
    if (bRefreshing)
    {
        return;
    }

    double Value = 0.0;
    FText Error;
    if (!ParsePositiveDouble(Text, TEXT("Maximum normalized neighbor distance"), Value, Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Aerodynamics.Database.MaximumNormalizedNeighborDistance = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::HandleMomentReferenceCenterCommitted(
    FVector NewBackendValue)
{
    if (bRefreshing)
    {
        return;
    }

    if (!TGConfigAerodynamicsPrivate::IsFiniteVector(NewBackendValue))
    {
        RefreshUiFromWorkingScenario();
        ShowError(FText::FromString(TEXT("Moment reference center must contain finite values.")));
        return;
    }

    WorkingScenario.Aerodynamics.Database.MomentReferenceCenterBodyMeters = NewBackendValue;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAerodynamicsWidgetBase::SetMomentReferenceCenterWidgetValue(
    const FVector& Value)
{
    if (INPUT_MomentReferenceCenter == nullptr)
    {
        return;
    }

    UFunction* SetVectorValueFunction =
        INPUT_MomentReferenceCenter->FindFunction(TEXT("SetVectorValue"));

    if (SetVectorValueFunction == nullptr)
    {
        ShowError(FText::FromString(TEXT("The moment reference center control is unavailable.")));
        return;
    }

    struct FSetVectorValueParameters
    {
        FVector NewBackendValue;
    };

    FSetVectorValueParameters Parameters;
    Parameters.NewBackendValue = Value;
    INPUT_MomentReferenceCenter->ProcessEvent(
        SetVectorValueFunction,
        &Parameters);
}

FString UTGConfigAerodynamicsWidgetBase::NormalizePath(const FString& Path)
{
    FString Result = FPaths::ConvertRelativePathToFull(Path.TrimStartAndEnd());
    FPaths::NormalizeFilename(Result);
    return Result;
}

FText UTGConfigAerodynamicsWidgetBase::FormatDouble(double Value)
{
    return UTGHudFormattingLibrary::FormatDoubleForHud(Value, 9);
}

FText UTGConfigAerodynamicsWidgetBase::FormatInteger(int32 Value)
{
    return UTGHudFormattingLibrary::FormatIntegerForHud(Value);
}

void UTGConfigAerodynamicsWidgetBase::ClearMessages()
{
    if (TXT_AerodynamicsWarning != nullptr)
    {
        TXT_AerodynamicsWarning->SetText(FText::GetEmpty());
        TXT_AerodynamicsWarning->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (TXT_AerodynamicsError != nullptr)
    {
        TXT_AerodynamicsError->SetText(FText::GetEmpty());
        TXT_AerodynamicsError->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UTGConfigAerodynamicsWidgetBase::ShowError(const FText& Error)
{
    if (TXT_AerodynamicsError == nullptr || Error.IsEmpty())
    {
        return;
    }

    TXT_AerodynamicsError->SetText(Error);
    TXT_AerodynamicsError->SetVisibility(ESlateVisibility::Visible);
}

void UTGConfigAerodynamicsWidgetBase::ShowWarning(const FText& Warning)
{
    if (TXT_AerodynamicsWarning == nullptr || Warning.IsEmpty())
    {
        return;
    }

    TXT_AerodynamicsWarning->SetText(Warning);
    TXT_AerodynamicsWarning->SetVisibility(ESlateVisibility::Visible);
}
