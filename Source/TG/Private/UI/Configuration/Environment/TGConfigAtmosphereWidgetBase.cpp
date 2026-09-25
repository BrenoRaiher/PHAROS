// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Environment/TGConfigAtmosphereWidgetBase.h"

#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "UI/Common/TGFileDialogLibrary.h"
#include "UI/Configuration/Environment/TGEnvironmentEditingLibrary.h"
#include "UI/TGHudFormattingLibrary.h"

namespace TGConfigAtmospherePrivate
{
    constexpr int32 CsvPreviewRowLimit = 100;

    const FString UploadedProfileText = TEXT("Uploaded Profile");
    const FString ChpText = TEXT("Cubic Harris-Priester (Earth only)");

    const TArray<FString> GeneralProfileColumns = {
        TEXT("Altitude [m]"),
        TEXT("Density [kg/m^3]"),
        TEXT("Temperature [K]"),
        TEXT("Mean Particle Mass [kg]"),
        TEXT("Collision Cross Section [m^2]")};

    const TArray<FString> ChpCoefficientColumns = {
        TEXT("Maximum Envelope 0"),
        TEXT("Maximum Envelope 1"),
        TEXT("Maximum Envelope 2"),
        TEXT("Maximum Envelope 3"),
        TEXT("Minimum Envelope 0"),
        TEXT("Minimum Envelope 1"),
        TEXT("Minimum Envelope 2"),
        TEXT("Minimum Envelope 3")};

    const TArray<FString> ChpMolecularColumns = {
        TEXT("Altitude [m]"),
        TEXT("Temperature [K]"),
        TEXT("Mean Particle Mass [kg]"),
        TEXT("Collision Cross Section [m^2]")};

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

    FString ModelToString(ETGAtmosphereModel Model)
    {
        return Model == ETGAtmosphereModel::CubicHarrisPriesterEarth
            ? ChpText
            : UploadedProfileText;
    }

    void ConfigurePreview(UMultiLineEditableTextBox* Preview)
    {
        if (Preview != nullptr)
        {
            Preview->SetIsReadOnly(true);
        }
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

void UTGConfigAtmosphereWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    TGConfigAtmospherePrivate::ConfigurePreview(
        VIEW_GeneralProfileCsvPreview);
    TGConfigAtmospherePrivate::ConfigurePreview(
        VIEW_ChpCoefficientCsvPreview);
    TGConfigAtmospherePrivate::ConfigurePreview(
        VIEW_ChpMolecularProfileCsvPreview);

    if (SCROLL_Atmosphere != nullptr)
    {
        SCROLL_Atmosphere->SetScrollbarThickness(FVector2D(8.0f, 8.0f));
        SCROLL_Atmosphere->SetScrollbarPadding(
            FMargin(6.0f, 4.0f, 4.0f, 4.0f));
        SCROLL_Atmosphere->SetAlwaysShowScrollbarTrack(true);
        SCROLL_Atmosphere->SetAnimateWheelScrolling(true);
        SCROLL_Atmosphere->SetAllowOverscroll(false);
        SCROLL_Atmosphere->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
        SCROLL_Atmosphere->SetWheelScrollMultiplier(4.0f);
    }

    if (CHECK_EnableAtmosphere != nullptr)
    {
        CHECK_EnableAtmosphere->OnCheckStateChanged.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleEnableAtmosphereChanged);
    }

    if (COMBO_AtmosphereCentralBody != nullptr)
    {
        COMBO_AtmosphereCentralBody->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleCentralBodySelectionChanged);
    }

    if (COMBO_AtmosphereModel != nullptr)
    {
        COMBO_AtmosphereModel->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleModelSelectionChanged);
    }

    if (BTN_BrowseGeneralProfile != nullptr)
    {
        BTN_BrowseGeneralProfile->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleBrowseGeneralProfileClicked);
    }

    if (BTN_ClearGeneralProfile != nullptr)
    {
        BTN_ClearGeneralProfile->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleClearGeneralProfileClicked);
    }

    if (INPUT_ChpF107 != nullptr)
    {
        INPUT_ChpF107->OnTextCommitted.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleChpF107Committed);
    }

    if (BTN_BrowseChpCoefficient != nullptr)
    {
        BTN_BrowseChpCoefficient->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleBrowseChpCoefficientClicked);
    }

    if (BTN_ClearChpCoefficient != nullptr)
    {
        BTN_ClearChpCoefficient->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleClearChpCoefficientClicked);
    }

    if (BTN_BrowseChpMolecularProfile != nullptr)
    {
        BTN_BrowseChpMolecularProfile->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleBrowseChpMolecularClicked);
    }

    if (BTN_ClearChpMolecularProfile != nullptr)
    {
        BTN_ClearChpMolecularProfile->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigAtmosphereWidgetBase::HandleClearChpMolecularClicked);
    }

    RefreshFromCurrentDraft();
}

void UTGConfigAtmosphereWidgetBase::NativeTick(
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
UTGConfigAtmosphereWidgetBase::GetSimulationSubsystem() const
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

bool UTGConfigAtmosphereWidgetBase::PullWorkingScenarioFromDraft()
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

bool UTGConfigAtmosphereWidgetBase::CommitWorkingScenarioToDraft()
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

void UTGConfigAtmosphereWidgetBase::RefreshFromCurrentDraft()
{
    ClearMessages();
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    RefreshUiFromWorkingScenario();
}

bool UTGConfigAtmosphereWidgetBase::
NavigateToScenarioReviewIssue_Implementation(
    const FTGScenarioReviewIssue& Issue)
{
    RefreshFromCurrentDraft();

    const FString& Path = Issue.Path;
    UWidget* Target = nullptr;
    if (Path.Contains(TEXT("bEnabled"))) Target = CHECK_EnableAtmosphere;
    else if (Path.Contains(TEXT("CentralBody"))) Target = COMBO_AtmosphereCentralBody;
    else if (Path.Contains(TEXT("Model"))) Target = COMBO_AtmosphereModel;
    else if (Path.Contains(TEXT("F107"))) Target = INPUT_ChpF107;
    else if (Path.Contains(TEXT("Coefficient"))) Target = BTN_BrowseChpCoefficient;
    else if (Path.Contains(TEXT("Molecular"))) Target = BTN_BrowseChpMolecularProfile;
    else if (Path.Contains(TEXT("Profile"))) Target = BTN_BrowseGeneralProfile;

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

void UTGConfigAtmosphereWidgetBase::RefreshUiFromWorkingScenario()
{
    ClearMessages();
    TGuardValue<bool> RefreshGuard(bRefreshing, true);

    const FTGAtmosphereConfig& Atmosphere = WorkingScenario.Atmosphere;

    if (CHECK_EnableAtmosphere != nullptr)
    {
        CHECK_EnableAtmosphere->SetIsChecked(Atmosphere.bEnabled);
    }

    if (SIZE_AtmosphereContentCard != nullptr)
    {
        SIZE_AtmosphereContentCard->SetIsEnabled(Atmosphere.bEnabled);
    }

    TGConfigAtmospherePrivate::ResetCombo(
        COMBO_AtmosphereCentralBody,
        UTGEnvironmentEditingLibrary::GetAtmosphereCentralBodyNames(),
        Atmosphere.CentralBodyName);

    TGConfigAtmospherePrivate::ResetCombo(
        COMBO_AtmosphereModel,
        {
            TGConfigAtmospherePrivate::UploadedProfileText,
            TGConfigAtmospherePrivate::ChpText
        },
        TGConfigAtmospherePrivate::ModelToString(Atmosphere.Model));

    const bool bChp =
        Atmosphere.Model == ETGAtmosphereModel::CubicHarrisPriesterEarth;

    if (SWITCH_AtmosphereModel != nullptr)
    {
        SWITCH_AtmosphereModel->SetActiveWidgetIndex(bChp ? 1 : 0);
    }

    if (COMBO_AtmosphereCentralBody != nullptr)
    {
        COMBO_AtmosphereCentralBody->SetIsEnabled(!bChp);
    }

    if (TXT_GeneralProfilePath != nullptr)
    {
        TXT_GeneralProfilePath->SetText(
            FText::FromString(
                Atmosphere.GeneralProfileCsvPath.IsEmpty()
                    ? TEXT("(No file selected)")
                    : Atmosphere.GeneralProfileCsvPath));
    }

    if (INPUT_ChpF107 != nullptr)
    {
        INPUT_ChpF107->SetText(
            FormatDouble(Atmosphere.CenteredAverageF107SolarFluxUnits));
    }

    if (TXT_ChpCoefficientPath != nullptr)
    {
        TXT_ChpCoefficientPath->SetText(
            FText::FromString(
                Atmosphere.ChpCoefficientCsvPath.IsEmpty()
                    ? TEXT("(No file selected)")
                    : Atmosphere.ChpCoefficientCsvPath));
    }

    if (TXT_ChpMolecularProfilePath != nullptr)
    {
        TXT_ChpMolecularProfilePath->SetText(
            FText::FromString(
                Atmosphere.ChpMolecularProfileCsvPath.IsEmpty()
                    ? TEXT("(No file selected)")
                    : Atmosphere.ChpMolecularProfileCsvPath));
    }

    RefreshFileStatuses();
}

void UTGConfigAtmosphereWidgetBase::RefreshFileStatuses()
{
    const FTGAtmosphereConfig& Atmosphere = WorkingScenario.Atmosphere;

    FText Summary;
    FText Error;

    bool bGeneralProfileValid = false;
    bool bChpCoefficientValid = false;
    bool bChpMolecularProfileValid = false;

    if (!Atmosphere.GeneralProfileCsvPath.IsEmpty())
    {
        bGeneralProfileValid =
            UTGEnvironmentEditingLibrary::ValidateGeneralAtmosphereProfileCsv(
                Atmosphere.GeneralProfileCsvPath,
                Summary,
                Error);
    }
    if (TXT_GeneralProfileStatus != nullptr)
    {
        if (Atmosphere.GeneralProfileCsvPath.IsEmpty())
        {
            TXT_GeneralProfileStatus->SetText(FText::FromString(TEXT("No atmosphere profile selected.")));
        }
        else if (bGeneralProfileValid)
        {
            TXT_GeneralProfileStatus->SetText(Summary);
        }
        else
        {
            TXT_GeneralProfileStatus->SetText(Error);
        }
    }

    TGConfigAtmospherePrivate::RefreshPreview(
        VBOX_GeneralProfilePreview,
        VIEW_GeneralProfileCsvPreview,
        bGeneralProfileValid,
        Atmosphere.GeneralProfileCsvPath,
        TGConfigAtmospherePrivate::GeneralProfileColumns);

    Summary = FText::GetEmpty();
    Error = FText::GetEmpty();
    if (!Atmosphere.ChpCoefficientCsvPath.IsEmpty())
    {
        bChpCoefficientValid =
            UTGEnvironmentEditingLibrary::ValidateChpCoefficientCsv(
                Atmosphere.ChpCoefficientCsvPath,
                Summary,
                Error);
    }

    if (TXT_ChpCoefficientStatus != nullptr)
    {
        if (Atmosphere.ChpCoefficientCsvPath.IsEmpty())
        {
            TXT_ChpCoefficientStatus->SetText(
                FText::FromString(TEXT("No density-envelope coefficient table selected.")));
        }
        else if (bChpCoefficientValid)
        {
            TXT_ChpCoefficientStatus->SetText(Summary);
        }
        else
        {
            TXT_ChpCoefficientStatus->SetText(Error);
        }
    }

    TGConfigAtmospherePrivate::RefreshPreview(
        VBOX_ChpCoefficientPreview,
        VIEW_ChpCoefficientCsvPreview,
        bChpCoefficientValid,
        Atmosphere.ChpCoefficientCsvPath,
        TGConfigAtmospherePrivate::ChpCoefficientColumns);

    Summary = FText::GetEmpty();
    Error = FText::GetEmpty();
    if (!Atmosphere.ChpMolecularProfileCsvPath.IsEmpty())
    {
        bChpMolecularProfileValid =
            UTGEnvironmentEditingLibrary::ValidateChpMolecularProfileCsv(
                Atmosphere.ChpMolecularProfileCsvPath,
                Summary,
                Error);
    }

    if (TXT_ChpMolecularProfileStatus != nullptr)
    {
        if (Atmosphere.ChpMolecularProfileCsvPath.IsEmpty())
        {
            TXT_ChpMolecularProfileStatus->SetText(
                FText::FromString(TEXT("No thermodynamic and molecular profile selected.")));
        }
        else if (bChpMolecularProfileValid)
        {
            TXT_ChpMolecularProfileStatus->SetText(Summary);
        }
        else
        {
            TXT_ChpMolecularProfileStatus->SetText(Error);
        }
    }

    TGConfigAtmospherePrivate::RefreshPreview(
        VBOX_ChpMolecularProfilePreview,
        VIEW_ChpMolecularProfileCsvPreview,
        bChpMolecularProfileValid,
        Atmosphere.ChpMolecularProfileCsvPath,
        TGConfigAtmospherePrivate::ChpMolecularColumns);
}

void UTGConfigAtmosphereWidgetBase::HandleEnableAtmosphereChanged(bool bIsChecked)
{
    if (bRefreshing)
    {
        return;
    }

    WorkingScenario.Atmosphere.bEnabled = bIsChecked;
    WorkingScenario.Aerodynamics.bEnabled = bIsChecked;

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleCentralBodySelectionChanged(
    FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    (void)SelectionType;
    if (bRefreshing || SelectedItem.IsEmpty())
    {
        return;
    }

    WorkingScenario.Atmosphere.CentralBodyName = SelectedItem;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleModelSelectionChanged(
    FString SelectedItem,
    ESelectInfo::Type SelectionType)
{
    (void)SelectionType;
    if (bRefreshing || SelectedItem.IsEmpty())
    {
        return;
    }

    if (SelectedItem == TGConfigAtmospherePrivate::ChpText)
    {
        WorkingScenario.Atmosphere.Model =
            ETGAtmosphereModel::CubicHarrisPriesterEarth;
        WorkingScenario.Atmosphere.CentralBodyName = TEXT("Earth");
    }
    else
    {
        WorkingScenario.Atmosphere.Model =
            ETGAtmosphereModel::UploadedProfile;
    }

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

bool UTGConfigAtmosphereWidgetBase::BrowseCsv(
    const FString& DialogTitle,
    const FString& CurrentPath,
    FString& OutSelectedPath) const
{
    const FString DefaultDirectory = CurrentPath.IsEmpty()
        ? FPaths::ProjectDir()
        : FPaths::GetPath(CurrentPath);

    return UTGFileDialogLibrary::OpenSingleFileDialog(
        DialogTitle,
        DefaultDirectory,
        FString(),
        TEXT("CSV Files"),
        {TEXT("csv")},
        false,
        OutSelectedPath);
}

void UTGConfigAtmosphereWidgetBase::HandleBrowseGeneralProfileClicked()
{
    FString SelectedPath;
    if (!BrowseCsv(
            TEXT("Select Atmosphere Profile"),
            WorkingScenario.Atmosphere.GeneralProfileCsvPath,
            SelectedPath))
    {
        return;
    }

    FText Summary;
    FText Error;
    if (!UTGEnvironmentEditingLibrary::ValidateGeneralAtmosphereProfileCsv(
            SelectedPath,
            Summary,
            Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    // Selecting a general-profile file is an explicit choice of the
    // Uploaded Profile model. Commit the model and file atomically so hidden
    // CHP-only values (such as F10.7) can never invalidate this workflow.
    WorkingScenario.Atmosphere.Model =
        ETGAtmosphereModel::UploadedProfile;
    WorkingScenario.Atmosphere.GeneralProfileCsvPath =
        NormalizePath(SelectedPath);

    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleClearGeneralProfileClicked()
{
    WorkingScenario.Atmosphere.GeneralProfileCsvPath.Reset();
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleChpF107Committed(
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
    if (!UTGHudFormattingLibrary::ParseHudDouble(Text, Value, Error) ||
        !FMath::IsFinite(Value) ||
        Value <= 0.0)
    {
        RefreshUiFromWorkingScenario();
        ShowError(
            Error.IsEmpty()
                ? FText::FromString(TEXT("Centered 81-day average F10.7 must be a positive finite value."))
                : Error);
        return;
    }

    WorkingScenario.Atmosphere.CenteredAverageF107SolarFluxUnits = Value;
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleBrowseChpCoefficientClicked()
{
    FString SelectedPath;
    if (!BrowseCsv(
            TEXT("Select Density-Envelope Coefficient Table"),
            WorkingScenario.Atmosphere.ChpCoefficientCsvPath,
            SelectedPath))
    {
        return;
    }

    FText Summary;
    FText Error;
    if (!UTGEnvironmentEditingLibrary::ValidateChpCoefficientCsv(
            SelectedPath,
            Summary,
            Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Atmosphere.ChpCoefficientCsvPath = NormalizePath(SelectedPath);
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleClearChpCoefficientClicked()
{
    WorkingScenario.Atmosphere.ChpCoefficientCsvPath.Reset();
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleBrowseChpMolecularClicked()
{
    FString SelectedPath;
    if (!BrowseCsv(
            TEXT("Select Thermodynamic and Molecular Profile"),
            WorkingScenario.Atmosphere.ChpMolecularProfileCsvPath,
            SelectedPath))
    {
        return;
    }

    FText Summary;
    FText Error;
    if (!UTGEnvironmentEditingLibrary::ValidateChpMolecularProfileCsv(
            SelectedPath,
            Summary,
            Error))
    {
        RefreshUiFromWorkingScenario();
        ShowError(Error);
        return;
    }

    WorkingScenario.Atmosphere.ChpMolecularProfileCsvPath = NormalizePath(SelectedPath);
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

void UTGConfigAtmosphereWidgetBase::HandleClearChpMolecularClicked()
{
    WorkingScenario.Atmosphere.ChpMolecularProfileCsvPath.Reset();
    CommitWorkingScenarioToDraft();
    RefreshUiFromWorkingScenario();
}

FString UTGConfigAtmosphereWidgetBase::NormalizePath(const FString& Path)
{
    FString Result = FPaths::ConvertRelativePathToFull(Path.TrimStartAndEnd());
    FPaths::NormalizeFilename(Result);
    return Result;
}

FText UTGConfigAtmosphereWidgetBase::FormatDouble(double Value)
{
    return UTGHudFormattingLibrary::FormatDoubleForHud(Value, 9);
}

void UTGConfigAtmosphereWidgetBase::ClearMessages()
{
    if (TXT_AtmosphereWarning != nullptr)
    {
        TXT_AtmosphereWarning->SetText(FText::GetEmpty());
        TXT_AtmosphereWarning->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (TXT_AtmosphereError != nullptr)
    {
        TXT_AtmosphereError->SetText(FText::GetEmpty());
        TXT_AtmosphereError->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UTGConfigAtmosphereWidgetBase::ShowError(const FText& Error)
{
    if (TXT_AtmosphereError == nullptr || Error.IsEmpty())
    {
        return;
    }

    TXT_AtmosphereError->SetText(Error);
    TXT_AtmosphereError->SetVisibility(ESlateVisibility::Visible);
}

void UTGConfigAtmosphereWidgetBase::ShowWarning(const FText& Warning)
{
    if (TXT_AtmosphereWarning == nullptr || Warning.IsEmpty())
    {
        return;
    }

    TXT_AtmosphereWarning->SetText(Warning);
    TXT_AtmosphereWarning->SetVisibility(ESlateVisibility::Visible);
}
