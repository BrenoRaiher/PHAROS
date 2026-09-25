// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Controls/TGConfigControlsWidgetBase.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableText.h"
#include "Components/HorizontalBox.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "TimerManager.h"
#include "UI/Common/TGFileDialogLibrary.h"
#include "UI/Configuration/Controls/TGControlEditingLibrary.h"
#include "UI/Configuration/Controls/TGControllerListEntryWidgetBase.h"
#include "UI/Configuration/Controls/TGControllerHelpDialogWidget.h"
#include "UI/Theme/TGUiTheme.h"

namespace TGConfigControlsPrivate
{
    constexpr int32 ScenarioSetupViewIndex = 0;
    constexpr int32 ControllerLibraryViewIndex = 1;

    const FString NoneModeText = TEXT("None");
    const FString UserControllerModeText = TEXT("User C++ controller");
    constexpr float PositiveMessageLifetimeSeconds = 8.0f;

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

        if (!SelectedOption.IsEmpty() &&
            Options.Contains(SelectedOption))
        {
            Combo->SetSelectedOption(SelectedOption);
        }
        else
        {
            Combo->ClearSelection();
        }
    }
}

void UTGConfigControlsWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    if (HBOX_ControllerStatus != nullptr)
    {
        while (HBOX_ControllerStatus->GetChildrenCount() > 2)
        {
            HBOX_ControllerStatus->RemoveChildAt(
                HBOX_ControllerStatus->GetChildrenCount() - 1);
        }
    }

    if (INPUT_ControllerSource != nullptr)
    {
        INPUT_ControllerSource->OnTextChanged.AddUniqueDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerSourceTextChanged);
    }

    if (BTN_ControllerHelp != nullptr)
    {
        BTN_ControllerHelp->OnClicked.AddUniqueDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerHelpClicked);
    }

    if (UTGControllerLibrarySubsystem* Library =
            ResolveControllerLibrary())
    {
        Library->OnControllerLibraryChanged.AddUniqueDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerLibraryChanged);

        Library->OnControllerBuildFinished.AddUniqueDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerBuildFinished);
    }

    InitializeStaticControls();
    CloseCreationModal();
    CloseDeleteModal();
    HideSelectedControllerEditor();

    if (SWITCH_ControlsView != nullptr)
    {
        SWITCH_ControlsView->SetActiveWidgetIndex(
            TGConfigControlsPrivate::ScenarioSetupViewIndex);
    }

    RefreshFromCurrentDraft();
}

void UTGConfigControlsWidgetBase::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ScenarioMessageClearTimer);
        World->GetTimerManager().ClearTimer(LibraryMessageClearTimer);
    }

    if (BTN_ControllerHelp != nullptr)
    {
        BTN_ControllerHelp->OnClicked.RemoveDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerHelpClicked);
    }

    if (ControllerHelpDialog != nullptr)
    {
        ControllerHelpDialog->CloseDialog();
        ControllerHelpDialog = nullptr;
    }

    if (INPUT_ControllerSource != nullptr)
    {
        INPUT_ControllerSource->OnTextChanged.RemoveDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerSourceTextChanged);
    }

    if (UTGControllerLibrarySubsystem* Library =
            ResolveControllerLibrary())
    {
        Library->OnControllerLibraryChanged.RemoveDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerLibraryChanged);

        Library->OnControllerBuildFinished.RemoveDynamic(
            this,
            &UTGConfigControlsWidgetBase::HandleControllerBuildFinished);
    }

    Super::NativeDestruct();
}

void UTGConfigControlsWidgetBase::NativeTick(
    const FGeometry& MyGeometry,
    const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const UTGSimulationSubsystem* SimulationSubsystem =
        ResolveSimulationSubsystem();
    if (SimulationSubsystem != nullptr &&
        SimulationSubsystem->GetCurrentScenarioDraftRevision() !=
            LastObservedDraftRevision)
    {
        RefreshFromCurrentDraft();
    }
}

UTGSimulationSubsystem*
UTGConfigControlsWidgetBase::ResolveSimulationSubsystem() const
{
    UGameInstance* GameInstance = GetGameInstance();

    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
}

UTGControllerLibrarySubsystem*
UTGConfigControlsWidgetBase::ResolveControllerLibrary() const
{
    UGameInstance* GameInstance = GetGameInstance();

    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGControllerLibrarySubsystem>()
        : nullptr;
}

bool UTGConfigControlsWidgetBase::PullWorkingScenarioFromDraft()
{
    UTGSimulationSubsystem* SimulationSubsystem =
        ResolveSimulationSubsystem();

    if (SimulationSubsystem == nullptr ||
        !SimulationSubsystem->HasCurrentScenarioDraft())
    {
        ShowScenarioMessage(
            FText::FromString(TEXT("No simulation scenario draft is available.")),
            true);
        return false;
    }

    WorkingScenario =
        SimulationSubsystem->GetCurrentScenarioDraft();
    LastObservedDraftRevision =
        SimulationSubsystem->GetCurrentScenarioDraftRevision();

    return true;
}

void UTGConfigControlsWidgetBase::CommitWorkingScenario()
{
    if (UTGSimulationSubsystem* SimulationSubsystem =
            ResolveSimulationSubsystem())
    {
        SimulationSubsystem->SetCurrentScenarioDraft(
            WorkingScenario);
        LastObservedDraftRevision =
            SimulationSubsystem->GetCurrentScenarioDraftRevision();
    }
}

void UTGConfigControlsWidgetBase::InitializeStaticControls()
{
    bRefreshingScenarioSetup = true;

    if (COMBO_ControlMode != nullptr)
    {
        COMBO_ControlMode->SetWidgetStyle(
            TGUiTheme::MakeComboBoxStyle());
    }
    if (COMBO_ScenarioController != nullptr)
    {
        COMBO_ScenarioController->SetWidgetStyle(
            TGUiTheme::MakeComboBoxStyle());
    }

    TGConfigControlsPrivate::ResetCombo(
        COMBO_ControlMode,
        {
            TGConfigControlsPrivate::NoneModeText,
            TGConfigControlsPrivate::UserControllerModeText
        },
        TGConfigControlsPrivate::NoneModeText);

    bRefreshingScenarioSetup = false;
}

void UTGConfigControlsWidgetBase::RefreshFromCurrentDraft()
{
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    RefreshScenarioSetup();
}

bool UTGConfigControlsWidgetBase::
NavigateToScenarioReviewIssue_Implementation(
    const FTGScenarioReviewIssue& Issue)
{
    RefreshFromCurrentDraft();
    ShowScenarioControlSetup();

    const FName ControllerId = !Issue.ObjectId.IsNone()
        ? Issue.ObjectId
        : WorkingScenario.Control.ControllerId;
    if (!ControllerId.IsNone())
    {
        RefreshControllerLibraryView();
        if (FindLibraryControllerById(ControllerId) != nullptr)
            SelectLibraryController(ControllerId);
        ShowScenarioControlSetup();
    }

    ShowScenarioMessage(
        Issue.Message,
        Issue.Severity == ETGScenarioReviewSeverity::Error);

    UWidget* Target = Issue.Path.Contains(TEXT("Mode"))
        ? static_cast<UWidget*>(COMBO_ControlMode)
        : static_cast<UWidget*>(COMBO_ScenarioController);
    if (Target == nullptr)
        return false;
    Target->SetIsEnabled(true);
    Target->SetKeyboardFocus();
    return true;
}

void UTGConfigControlsWidgetBase::RefreshScenarioSetup()
{
    bRefreshingScenarioSetup = true;
    ClearScenarioMessage();

    TGConfigControlsPrivate::ResetCombo(
        COMBO_ControlMode,
        {
            TGConfigControlsPrivate::NoneModeText,
            TGConfigControlsPrivate::UserControllerModeText
        },
        ControlModeToString(WorkingScenario.Control.Mode));

    if (BOX_ScenarioControllerArea != nullptr)
    {
        BOX_ScenarioControllerArea->SetVisibility(
            WorkingScenario.Control.Mode ==
                    ETGControlMode::CompiledUserController
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    RebuildReadyControllerCombo();

    if (TXT_ScenarioControllerStatus != nullptr)
    {
        if (WorkingScenario.Control.Mode == ETGControlMode::None)
        {
            TXT_ScenarioControllerStatus->SetText(
                FText::FromString(
                    TEXT("No user controller. Commanded actuator outputs default to zero.")));
        }
        else if (const FTGControllerRecord* ReadyController =
                     FindReadyControllerById(
                         WorkingScenario.Control.ControllerId))
        {
            TXT_ScenarioControllerStatus->SetText(
                FText::FromString(
                    FString::Printf(
                        TEXT("%s is ready."),
                        *ReadyController->DisplayName)));
            TXT_ScenarioControllerStatus->SetToolTipText(FText::GetEmpty());
        }
        else
        {
            const FString& StandaloneDllPath =
                WorkingScenario.Control.StandaloneControllerDllFilePath;
            TXT_ScenarioControllerStatus->SetText(
                FText::FromString(
                    StandaloneDllPath.IsEmpty()
                        ? TEXT("Select a trusted controller built with the current PHAROS SDK.")
                        : FString::Printf(
                            TEXT(
                                "The imported scenario references %s. Add and "
                                "trust this library in Controller Library, then "
                                "select it here."),
                            *FPaths::GetCleanFilename(StandaloneDllPath))));
            TXT_ScenarioControllerStatus->SetToolTipText(
                StandaloneDllPath.IsEmpty()
                    ? FText::GetEmpty()
                    : FText::FromString(StandaloneDllPath));
        }
    }

    bRefreshingScenarioSetup = false;
}

void UTGConfigControlsWidgetBase::RebuildReadyControllerCombo()
{
    ReadyControllers =
        UTGControlEditingLibrary::GetReadyControllers(this);

    TArray<FString> Options;
    Options.Reserve(ReadyControllers.Num());

    FString SelectedDisplayName;

    for (const FTGControllerRecord& Record : ReadyControllers)
    {
        Options.Add(Record.DisplayName);

        if (Record.ControllerId ==
            WorkingScenario.Control.ControllerId)
        {
            SelectedDisplayName = Record.DisplayName;
        }
    }

    TGConfigControlsPrivate::ResetCombo(
        COMBO_ScenarioController,
        Options,
        SelectedDisplayName);
}

void UTGConfigControlsWidgetBase::CommitControlModeFromCurrentDraft(
    const FString& SelectedMode)
{
    if (bRefreshingScenarioSetup)
    {
        return;
    }

    ETGControlMode ParsedMode = ETGControlMode::None;

    if (!TryParseControlMode(SelectedMode, ParsedMode))
    {
        ShowScenarioMessage(
            FText::FromString(TEXT("The selected control mode is unsupported.")),
            true);
        RefreshFromCurrentDraft();
        return;
    }

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    UTGControlEditingLibrary::SetControlMode(
        WorkingScenario.Control,
        ParsedMode);

    CommitWorkingScenario();
    RefreshScenarioSetup();
}

void UTGConfigControlsWidgetBase::CommitScenarioControllerSelectionFromCurrentDraft(
    const FString& SelectedControllerDisplayName)
{
    if (bRefreshingScenarioSetup ||
        SelectedControllerDisplayName.IsEmpty())
    {
        return;
    }

    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    ReadyControllers =
        UTGControlEditingLibrary::GetReadyControllers(this);

    const FTGControllerRecord* SelectedRecord =
        FindReadyControllerByDisplayName(
            SelectedControllerDisplayName);

    if (SelectedRecord == nullptr)
    {
        ShowScenarioMessage(
            FText::FromString(
                TEXT("The selected controller is no longer ready.")),
            true);
        RefreshScenarioSetup();
        return;
    }

    FText Error;

    if (!UTGControlEditingLibrary::SelectReadyController(
            this,
            WorkingScenario.Control,
            SelectedRecord->ControllerId,
            Error))
    {
        RefreshScenarioSetup();
        ShowScenarioMessage(Error, true);
        return;
    }

    CommitWorkingScenario();
    RefreshScenarioSetup();
}

void UTGConfigControlsWidgetBase::ClearScenarioControllerFromCurrentDraft()
{
    if (!PullWorkingScenarioFromDraft())
    {
        return;
    }

    UTGControlEditingLibrary::ClearSelectedController(
        WorkingScenario.Control);

    CommitWorkingScenario();
    RefreshScenarioSetup();
}

void UTGConfigControlsWidgetBase::ShowControllerLibrary()
{
    if (SWITCH_ControlsView != nullptr)
    {
        SWITCH_ControlsView->SetActiveWidgetIndex(
            TGConfigControlsPrivate::ControllerLibraryViewIndex);
    }

    RefreshControllerLibraryView();
}

void UTGConfigControlsWidgetBase::ShowScenarioControlSetup()
{
    if (!SaveDirtyControllerSourceBeforeContextChange())
    {
        return;
    }

    if (SWITCH_ControlsView != nullptr)
    {
        SWITCH_ControlsView->SetActiveWidgetIndex(
            TGConfigControlsPrivate::ScenarioSetupViewIndex);
    }

    RefreshFromCurrentDraft();
}

void UTGConfigControlsWidgetBase::RefreshControllerLibraryView()
{
    UTGControllerLibrarySubsystem* Library =
        ResolveControllerLibrary();

    if (Library == nullptr)
    {
        ShowLibraryMessage(
            FText::FromString(TEXT("The controller library is unavailable.")),
            true);
        HideSelectedControllerEditor();
        return;
    }

    ClearLibraryMessage();

    const FTGControllerToolchainStatus Toolchain =
        Library->InspectControllerToolchain();

    if (TXT_ControllerToolchainStatus != nullptr)
    {
        TXT_ControllerToolchainStatus->SetText(
            Toolchain.StatusMessage);
        TXT_ControllerToolchainStatus->SetToolTipText(
            Toolchain.CompilerVersion.IsEmpty()
                ? FText::GetEmpty()
                : FText::FromString(
                    FString::Printf(
                        TEXT("Controller compiler: %s"),
                        *Toolchain.CompilerVersion)));
    }

    LibraryControllers = Library->GetControllers();

    if (LibraryControllers.IsEmpty() && TXT_LibraryMessage != nullptr)
    {
        TXT_LibraryMessage->SetText(
            FText::FromString(
                TEXT(
                    "No controllers are available. Create one from the "
                    "template or import existing source.")));
        TXT_LibraryMessage->SetColorAndOpacity(
            TGUiTheme::GetPalette().TextSecondary);
        TXT_LibraryMessage->SetVisibility(ESlateVisibility::Visible);
    }

    const bool bSelectedStillExists =
        FindLibraryControllerById(SelectedLibraryControllerId) != nullptr;

    if (!bSelectedStillExists)
    {
        SelectedLibraryControllerId =
            LibraryControllers.IsEmpty()
                ? NAME_None
                : LibraryControllers[0].ControllerId;
    }

    RebuildControllerList();
    LoadSelectedControllerEditor();
}

void UTGConfigControlsWidgetBase::RebuildControllerList()
{
    if (VBOX_ControllerList == nullptr)
    {
        return;
    }

    VBOX_ControllerList->ClearChildren();

    UClass* RowClass = LoadClass<UTGControllerListEntryWidgetBase>(
        nullptr,
        TEXT("/Game/UI/Configuration/Controls/WBP_ControllerListEntry.WBP_ControllerListEntry_C"));

    if (RowClass == nullptr)
    {
        ShowLibraryMessage(
            FText::FromString(
                TEXT("WBP_ControllerListEntry could not be loaded.")),
            true);
        return;
    }

    for (const FTGControllerRecord& Record : LibraryControllers)
    {
        UTGControllerListEntryWidgetBase* Row =
            CreateWidget<UTGControllerListEntryWidgetBase>(
                GetWorld(),
                RowClass);

        if (Row == nullptr)
        {
            continue;
        }

        Row->InitializeControllerListEntry(
            Record.ControllerId,
            Record.DisplayName,
            Record.BuildStatus,
            Record.bTrusted,
            Record.ControllerId == SelectedLibraryControllerId,
            this);

        if (UVerticalBoxSlot* ListSlot =
                VBOX_ControllerList->AddChildToVerticalBox(Row))
        {
            ListSlot->SetHorizontalAlignment(HAlign_Fill);
        }
    }
}

void UTGConfigControlsWidgetBase::SelectLibraryController(
    const FName ControllerId)
{
    if (FindLibraryControllerById(ControllerId) == nullptr)
    {
        RefreshControllerLibraryView();
        return;
    }

    if (ControllerId == SelectedLibraryControllerId)
    {
        return;
    }

    if (!SaveDirtyControllerSourceBeforeContextChange())
    {
        return;
    }

    SelectedLibraryControllerId = ControllerId;
    RebuildControllerList();
    LoadSelectedControllerEditor();
}

void UTGConfigControlsWidgetBase::LoadSelectedControllerEditor()
{
    const FTGControllerRecord* Record =
        FindLibraryControllerById(SelectedLibraryControllerId);

    if (Record == nullptr)
    {
        HideSelectedControllerEditor();
        return;
    }

    const bool bPreserveDirtySource =
        bControllerSourceDirty &&
        LoadedSourceControllerId == SelectedLibraryControllerId;

    SetSelectedControllerEditorEmptyState(false);

    bRefreshingLibraryEditor = true;

    if (INPUT_ControllerName != nullptr)
    {
        INPUT_ControllerName->SetText(
            FText::FromString(Record->DisplayName));
    }

    if (CHECK_ControllerTrusted != nullptr)
    {
        CHECK_ControllerTrusted->SetIsChecked(
            Record->bTrusted);
    }

    if (TXT_ControllerBuildStatus != nullptr)
    {
        TXT_ControllerBuildStatus->SetText(
            FText::FromString(
                BuildStatusToString(Record->BuildStatus)));

        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        FLinearColor StatusColor = Palette.TextSecondary;
        if (Record->BuildStatus == ETGControllerBuildStatus::Ready)
        {
            StatusColor = Palette.Success;
        }
        else if (Record->BuildStatus == ETGControllerBuildStatus::Building)
        {
            StatusColor = Palette.Info;
        }
        else if (Record->BuildStatus == ETGControllerBuildStatus::Failed ||
                 Record->BuildStatus ==
                     ETGControllerBuildStatus::IncompatibleController)
        {
            StatusColor = Palette.Error;
        }
        TXT_ControllerBuildStatus->SetColorAndOpacity(StatusColor);
    }

    if (TXT_ControllerDiagnostics != nullptr)
    {
        TXT_ControllerDiagnostics->SetText(
            FText::FromString(
                Record->LastBuildDiagnostics.IsEmpty()
                    ? TEXT("No build output is available.")
                    : Record->LastBuildDiagnostics));
    }

    const bool bEditableSource =
        Record->bHasEditableSource;

    if (BOX_ControllerSourceEditor != nullptr)
    {
        BOX_ControllerSourceEditor->SetVisibility(
            bEditableSource
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (BTN_BuildController != nullptr)
    {
        BTN_BuildController->SetVisibility(
            bEditableSource
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);

        BTN_BuildController->SetIsEnabled(
            bEditableSource &&
            Record->bTrusted &&
            Record->BuildStatus != ETGControllerBuildStatus::Building);
    }

    if (TXT_BuildController != nullptr)
    {
        TXT_BuildController->SetText(
            FText::FromString(
                Record->BuildStatus == ETGControllerBuildStatus::Building
                    ? TEXT("Building...")
                    : Record->BuildStatus == ETGControllerBuildStatus::Ready
                        ? TEXT("Rebuild Controller")
                        : TEXT("Build Controller")));
    }

    if (INPUT_ControllerSource != nullptr)
    {
        INPUT_ControllerSource->SetIsReadOnly(
            !bEditableSource ||
            Record->BuildStatus == ETGControllerBuildStatus::Building);
    }

    if (!bEditableSource)
    {
        bUpdatingControllerSource = true;
        if (INPUT_ControllerSource != nullptr)
        {
            INPUT_ControllerSource->SetText(FText::GetEmpty());
        }
        bUpdatingControllerSource = false;
        LoadedControllerSource.Reset();
        LoadedSourceControllerId = NAME_None;
        bControllerSourceDirty = false;
    }
    else if (!bPreserveDirtySource)
    {
        if (UTGControllerLibrarySubsystem* Library = ResolveControllerLibrary())
        {
            FString Source;
            FText Error;

            if (Library->LoadControllerSource(
                    Record->ControllerId,
                    Source,
                    Error))
            {
                if (INPUT_ControllerSource != nullptr)
                {
                    bUpdatingControllerSource = true;
                    INPUT_ControllerSource->SetText(
                        FText::FromString(Source));
                    bUpdatingControllerSource = false;
                }
                LoadedControllerSource = Source;
                LoadedSourceControllerId = SelectedLibraryControllerId;
                bControllerSourceDirty = false;
            }
            else
            {
                ShowLibraryMessage(Error, true);
            }
        }
    }

    UpdateControllerSourceState();
    bRefreshingLibraryEditor = false;
}

void UTGConfigControlsWidgetBase::HideSelectedControllerEditor()
{
    SetSelectedControllerEditorEmptyState(true);

    bUpdatingControllerSource = true;
    if (INPUT_ControllerSource != nullptr)
    {
        INPUT_ControllerSource->SetText(FText::GetEmpty());
    }
    bUpdatingControllerSource = false;
    LoadedControllerSource.Reset();
    LoadedSourceControllerId = NAME_None;
    bControllerSourceDirty = false;
    UpdateControllerSourceState();
}

void UTGConfigControlsWidgetBase::EnsureSelectedControllerPlaceholder()
{
    if (TXT_NoControllerSelected != nullptr ||
        VBOX_SelectedControllerEditor == nullptr ||
        WidgetTree == nullptr)
    {
        return;
    }

    TXT_NoControllerSelected =
        WidgetTree->ConstructWidget<UTextBlock>(
            UTextBlock::StaticClass(),
            TEXT("TXT_NoControllerSelected"));
    if (TXT_NoControllerSelected == nullptr)
    {
        return;
    }

    TXT_NoControllerSelected->SetText(FText::FromString(
        TEXT("No controller selected.\nCreate or import a controller, then "
             "select it to view its settings.")));
    TXT_NoControllerSelected->SetColorAndOpacity(
        TGUiTheme::GetPalette().TextSecondary);
    TXT_NoControllerSelected->SetJustification(ETextJustify::Center);
    TXT_NoControllerSelected->SetAutoWrapText(true);

    FSlateFontInfo Font = TXT_NoControllerSelected->GetFont();
    Font.Size = 13;
    TXT_NoControllerSelected->SetFont(Font);

    if (UVerticalBoxSlot* PlaceholderSlot =
            VBOX_SelectedControllerEditor->AddChildToVerticalBox(
                TXT_NoControllerSelected))
    {
        PlaceholderSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        PlaceholderSlot->SetPadding(FMargin(24.0f, 48.0f));
        PlaceholderSlot->SetHorizontalAlignment(HAlign_Fill);
        PlaceholderSlot->SetVerticalAlignment(VAlign_Center);
    }
}

void UTGConfigControlsWidgetBase::SetSelectedControllerEditorEmptyState(
    const bool bEmpty)
{
    if (BOX_SelectedControllerEditor != nullptr)
    {
        BOX_SelectedControllerEditor->SetVisibility(
            ESlateVisibility::Visible);
    }

    EnsureSelectedControllerPlaceholder();

    if (VBOX_SelectedControllerEditor == nullptr)
    {
        return;
    }

    for (UWidget* Child : VBOX_SelectedControllerEditor->GetAllChildren())
    {
        if (Child == TXT_NoControllerSelected)
        {
            Child->SetVisibility(
                bEmpty
                    ? ESlateVisibility::Visible
                    : ESlateVisibility::Collapsed);
        }
        else if (Child == TXT_SelectedControllerTitle)
        {
            Child->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            Child->SetVisibility(
                bEmpty
                    ? ESlateVisibility::Collapsed
                    : ESlateVisibility::Visible);
        }
    }
}

void UTGConfigControlsWidgetBase::UpdateControllerSourceState()
{
    const FTGControllerRecord* Record =
        FindLibraryControllerById(SelectedLibraryControllerId);
    const bool bEditableSource =
        Record != nullptr && Record->bHasEditableSource;
    const bool bBuilding =
        Record != nullptr &&
        Record->BuildStatus == ETGControllerBuildStatus::Building;

    if (TXT_ControllerSourceLabel != nullptr)
    {
        TXT_ControllerSourceLabel->SetText(
            FText::FromString(
                bControllerSourceDirty
                    ? TEXT("C++ Source (Unsaved changes)")
                    : TEXT("C++ Source")));
        TXT_ControllerSourceLabel->SetColorAndOpacity(
            bControllerSourceDirty
                ? TGUiTheme::GetPalette().Warning
                : TGUiTheme::GetPalette().TextPrimary);
    }

    if (BTN_SaveControllerSource != nullptr)
    {
        BTN_SaveControllerSource->SetIsEnabled(
            bEditableSource && bControllerSourceDirty && !bBuilding);
    }
}

bool UTGConfigControlsWidgetBase::SaveSelectedControllerSourceInternal(
    const bool bShowConfirmation)
{
    if (SelectedLibraryControllerId.IsNone() ||
        LoadedSourceControllerId != SelectedLibraryControllerId)
    {
        return true;
    }

    UTGControllerLibrarySubsystem* Library = ResolveControllerLibrary();
    if (Library == nullptr)
    {
        ShowLibraryMessage(
            FText::FromString(TEXT("The controller library is unavailable.")),
            true);
        return false;
    }

    const FString Source =
        INPUT_ControllerSource != nullptr
            ? INPUT_ControllerSource->GetText().ToString()
            : FString();
    const FString PreviousLoadedSource = LoadedControllerSource;
    const bool bWasDirty = bControllerSourceDirty;

    // The subsystem broadcasts synchronously after saving. Mark this editor as
    // clean first so that the resulting refresh reads the saved source.
    LoadedControllerSource = Source;
    bControllerSourceDirty = false;
    UpdateControllerSourceState();

    FText Error;
    if (!Library->SaveControllerSource(
            SelectedLibraryControllerId,
            Source,
            Error))
    {
        LoadedControllerSource = PreviousLoadedSource;
        bControllerSourceDirty = bWasDirty;
        UpdateControllerSourceState();
        ShowLibraryMessage(Error, true);
        return false;
    }

    if (bShowConfirmation)
    {
        ShowLibraryMessage(
            FText::FromString(
                TEXT("Source saved. Build the controller to use this revision.")),
            false);
    }
    return true;
}

bool UTGConfigControlsWidgetBase::SaveDirtyControllerSourceBeforeContextChange()
{
    return !bControllerSourceDirty ||
        SaveSelectedControllerSourceInternal(false);
}

void UTGConfigControlsWidgetBase::HandleControllerSourceTextChanged(
    const FText& SourceText)
{
    if (bUpdatingControllerSource ||
        bRefreshingLibraryEditor ||
        LoadedSourceControllerId.IsNone() ||
        LoadedSourceControllerId != SelectedLibraryControllerId)
    {
        return;
    }

    bControllerSourceDirty =
        SourceText.ToString() != LoadedControllerSource;
    UpdateControllerSourceState();
}

void UTGConfigControlsWidgetBase::BeginCreateControllerFromTemplate()
{
    BeginPendingCreation(
        EPendingControllerCreationKind::Template);
}

void UTGConfigControlsWidgetBase::BeginImportControllerSource()
{
    BeginPendingCreation(
        EPendingControllerCreationKind::Source);
}

void UTGConfigControlsWidgetBase::BeginImportPrebuiltControllerDll()
{
    BeginPendingCreation(
        EPendingControllerCreationKind::PrebuiltDll);

    if (PullWorkingScenarioFromDraft() &&
        !WorkingScenario.Control.StandaloneControllerDllFilePath.IsEmpty())
    {
        PendingImportFilePath =
            WorkingScenario.Control.StandaloneControllerDllFilePath;

        if (INPUT_CreateControllerName != nullptr)
        {
            INPUT_CreateControllerName->SetText(
                FText::FromString(
                    FPaths::GetBaseFilename(PendingImportFilePath)));
        }
        if (TXT_CreateControllerFilePath != nullptr)
        {
            TXT_CreateControllerFilePath->SetText(
                FText::FromString(PendingImportFilePath));
            TXT_CreateControllerFilePath->SetToolTipText(
                FText::FromString(PendingImportFilePath));
        }
    }
}

void UTGConfigControlsWidgetBase::BeginPendingCreation(
    const EPendingControllerCreationKind Kind)
{
    PendingCreationKind = Kind;
    PendingImportFilePath.Reset();
    ClearCreationError();

    if (INPUT_CreateControllerName != nullptr)
    {
        INPUT_CreateControllerName->SetText(FText::GetEmpty());
    }

    if (CHECK_CreateControllerTrusted != nullptr)
    {
        CHECK_CreateControllerTrusted->SetIsChecked(false);
    }

    const bool bNeedsFile =
        Kind == EPendingControllerCreationKind::Source ||
        Kind == EPendingControllerCreationKind::PrebuiltDll;

    if (BOX_CreateControllerFile != nullptr)
    {
        BOX_CreateControllerFile->SetVisibility(
            bNeedsFile
                ? ESlateVisibility::Visible
                : ESlateVisibility::Collapsed);
    }

    if (TXT_CreateControllerFilePath != nullptr)
    {
        TXT_CreateControllerFilePath->SetText(
            FText::FromString(TEXT("No file selected")));
        TXT_CreateControllerFilePath->SetToolTipText(FText::GetEmpty());
    }

    if (Kind == EPendingControllerCreationKind::Template)
    {
        if (TXT_CreateControllerTitle != nullptr)
        {
            TXT_CreateControllerTitle->SetText(
                FText::FromString(TEXT("New Controller from Template")));
        }
    }
    else if (Kind == EPendingControllerCreationKind::Source)
    {
        if (TXT_CreateControllerTitle != nullptr)
        {
            TXT_CreateControllerTitle->SetText(
                FText::FromString(TEXT("Import Controller Source")));
        }

        if (TXT_CreateControllerFileLabel != nullptr)
        {
            TXT_CreateControllerFileLabel->SetText(
                FText::FromString(TEXT("Source file (.cpp)")));
        }

        if (TXT_BrowseCreateControllerFile != nullptr)
        {
            TXT_BrowseCreateControllerFile->SetText(
                FText::FromString(TEXT("Select C++ Source")));
        }
    }
    else if (Kind == EPendingControllerCreationKind::PrebuiltDll)
    {
        if (TXT_CreateControllerTitle != nullptr)
        {
            TXT_CreateControllerTitle->SetText(
                FText::FromString(TEXT("Import Prebuilt Controller Library")));
        }

        if (TXT_CreateControllerFileLabel != nullptr)
        {
            TXT_CreateControllerFileLabel->SetText(
                FText::FromString(TEXT("Controller library (.dll)")));
        }

        if (TXT_BrowseCreateControllerFile != nullptr)
        {
            TXT_BrowseCreateControllerFile->SetText(
                FText::FromString(TEXT("Select Controller Library")));
        }
    }

    if (BORDER_CreateControllerModal != nullptr)
    {
        BORDER_CreateControllerModal->SetVisibility(
            ESlateVisibility::Visible);
    }
}

void UTGConfigControlsWidgetBase::BrowsePendingControllerImportFile()
{
    if (PendingCreationKind != EPendingControllerCreationKind::Source &&
        PendingCreationKind != EPendingControllerCreationKind::PrebuiltDll)
    {
        return;
    }

    const bool bSource =
        PendingCreationKind == EPendingControllerCreationKind::Source;

    FString SelectedFilePath;

    const bool bSelected =
        UTGFileDialogLibrary::OpenSingleFileDialog(
            bSource
                ? TEXT("Select controller source")
                : TEXT("Select controller DLL"),
            FString(),
            FString(),
            bSource
                ? TEXT("C++ source")
                : TEXT("Controller DLL"),
            { bSource ? TEXT("cpp") : TEXT("dll") },
            false,
            SelectedFilePath);

    if (!bSelected)
    {
        return;
    }

    PendingImportFilePath = SelectedFilePath;

    if (TXT_CreateControllerFilePath != nullptr)
    {
        TXT_CreateControllerFilePath->SetText(
            FText::FromString(PendingImportFilePath));
        TXT_CreateControllerFilePath->SetToolTipText(
            FText::FromString(PendingImportFilePath));
    }

    ClearCreationError();
}

void UTGConfigControlsWidgetBase::ConfirmPendingControllerCreation()
{
    UTGControllerLibrarySubsystem* Library =
        ResolveControllerLibrary();

    if (Library == nullptr)
    {
        ShowCreationError(
            FText::FromString(TEXT("The controller library is unavailable.")));
        return;
    }

    if (PendingCreationKind == EPendingControllerCreationKind::None)
    {
        return;
    }

    const FString DisplayName =
        INPUT_CreateControllerName != nullptr
            ? INPUT_CreateControllerName->GetText().ToString()
            : FString();

    const bool bTrusted =
        CHECK_CreateControllerTrusted != nullptr &&
        CHECK_CreateControllerTrusted->IsChecked();

    FTGControllerRecord CreatedController;
    FText Error;
    bool bSucceeded = false;

    switch (PendingCreationKind)
    {
    case EPendingControllerCreationKind::Template:
        bSucceeded = Library->CreateControllerFromTemplate(
            DisplayName,
            bTrusted,
            CreatedController,
            Error);
        break;

    case EPendingControllerCreationKind::Source:
        bSucceeded = Library->ImportControllerSource(
            DisplayName,
            PendingImportFilePath,
            bTrusted,
            CreatedController,
            Error);
        break;

    case EPendingControllerCreationKind::PrebuiltDll:
        bSucceeded = Library->ImportPrebuiltControllerDll(
            DisplayName,
            PendingImportFilePath,
            bTrusted,
            CreatedController,
            Error);
        break;

    default:
        break;
    }

    if (!bSucceeded)
    {
        ShowCreationError(Error);
        return;
    }

    SelectedLibraryControllerId =
        CreatedController.ControllerId;

    CloseCreationModal();
    RefreshControllerLibraryView();
    RefreshFromCurrentDraft();

    ShowLibraryMessage(
        FText::FromString(
            TEXT("Controller added to the managed library.")),
        false);
}

void UTGConfigControlsWidgetBase::CancelPendingControllerCreation()
{
    CloseCreationModal();
}

void UTGConfigControlsWidgetBase::CloseCreationModal()
{
    PendingCreationKind =
        EPendingControllerCreationKind::None;
    PendingImportFilePath.Reset();

    if (BORDER_CreateControllerModal != nullptr)
    {
        BORDER_CreateControllerModal->SetVisibility(
            ESlateVisibility::Collapsed);
    }

    ClearCreationError();
}

void UTGConfigControlsWidgetBase::RenameSelectedControllerFromText(
    const FText NewNameText)
{
    if (bRefreshingLibraryEditor ||
        SelectedLibraryControllerId.IsNone())
    {
        return;
    }

    UTGControllerLibrarySubsystem* Library =
        ResolveControllerLibrary();

    if (Library == nullptr)
    {
        ShowLibraryMessage(
            FText::FromString(TEXT("The controller library is unavailable.")),
            true);
        return;
    }

    FText Error;

    if (!Library->RenameController(
            SelectedLibraryControllerId,
            NewNameText.ToString(),
            Error))
    {
        RefreshControllerLibraryView();
        ShowLibraryMessage(Error, true);
        return;
    }

    RefreshControllerLibraryView();
    RefreshFromCurrentDraft();
}

void UTGConfigControlsWidgetBase::CommitSelectedControllerTrusted(
    const bool bTrusted)
{
    if (bRefreshingLibraryEditor ||
        SelectedLibraryControllerId.IsNone())
    {
        return;
    }

    UTGControllerLibrarySubsystem* Library =
        ResolveControllerLibrary();

    if (Library == nullptr)
    {
        return;
    }

    FText Error;

    if (!Library->SetControllerTrusted(
            SelectedLibraryControllerId,
            bTrusted,
            Error))
    {
        RefreshControllerLibraryView();
        ShowLibraryMessage(Error, true);
        return;
    }

    RefreshControllerLibraryView();
    RefreshFromCurrentDraft();
}

void UTGConfigControlsWidgetBase::SaveSelectedControllerSource()
{
    SaveSelectedControllerSourceInternal(true);
}

void UTGConfigControlsWidgetBase::BuildSelectedController()
{
    if (SelectedLibraryControllerId.IsNone())
    {
        return;
    }

    UTGControllerLibrarySubsystem* Library =
        ResolveControllerLibrary();

    if (Library == nullptr)
    {
        return;
    }

    if (!SaveDirtyControllerSourceBeforeContextChange())
    {
        return;
    }

    FText Error;

    if (!Library->BuildController(
            SelectedLibraryControllerId,
            Error))
    {
        RefreshControllerLibraryView();
        ShowLibraryMessage(Error, true);
        return;
    }

    RefreshControllerLibraryView();

    ShowLibraryMessage(
        FText::FromString(TEXT("Controller build started.")),
        false);
}

void UTGConfigControlsWidgetBase::BeginDeleteSelectedController()
{
    const FTGControllerRecord* Record =
        FindLibraryControllerById(SelectedLibraryControllerId);

    if (Record == nullptr)
    {
        return;
    }

    if (TXT_DeleteControllerQuestion != nullptr)
    {
        TXT_DeleteControllerQuestion->SetText(
            FText::FromString(
                FString::Printf(
                    TEXT(
                        "Delete controller \"%s\"? Its managed source and "
                        "compiled files will be removed from this computer. "
                        "Saved scenarios that reference it will require a "
                        "replacement controller before simulation."),
                    *Record->DisplayName)));
    }

    if (BORDER_DeleteControllerModal != nullptr)
    {
        BORDER_DeleteControllerModal->SetVisibility(
            ESlateVisibility::Visible);
    }
}

void UTGConfigControlsWidgetBase::ConfirmDeleteSelectedController()
{
    if (SelectedLibraryControllerId.IsNone())
    {
        CloseDeleteModal();
        return;
    }

    UTGControllerLibrarySubsystem* Library =
        ResolveControllerLibrary();

    if (Library == nullptr)
    {
        CloseDeleteModal();
        return;
    }

    FText Error;

    if (!Library->DeleteController(
            SelectedLibraryControllerId,
            Error))
    {
        CloseDeleteModal();
        RefreshControllerLibraryView();
        ShowLibraryMessage(Error, true);
        return;
    }

    LoadedControllerSource.Reset();
    LoadedSourceControllerId = NAME_None;
    bControllerSourceDirty = false;
    SelectedLibraryControllerId = NAME_None;
    CloseDeleteModal();
    RefreshControllerLibraryView();
    RefreshFromCurrentDraft();

    ShowLibraryMessage(
        FText::FromString(TEXT("Controller deleted.")),
        false);
}

void UTGConfigControlsWidgetBase::CancelDeleteSelectedController()
{
    CloseDeleteModal();
}

void UTGConfigControlsWidgetBase::CloseDeleteModal()
{
    if (BORDER_DeleteControllerModal != nullptr)
    {
        BORDER_DeleteControllerModal->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

const FTGControllerRecord*
UTGConfigControlsWidgetBase::FindReadyControllerByDisplayName(
    const FString& DisplayName) const
{
    return ReadyControllers.FindByPredicate(
        [&DisplayName](const FTGControllerRecord& Record)
        {
            return Record.DisplayName == DisplayName;
        });
}

const FTGControllerRecord*
UTGConfigControlsWidgetBase::FindReadyControllerById(
    const FName ControllerId) const
{
    return ReadyControllers.FindByPredicate(
        [ControllerId](const FTGControllerRecord& Record)
        {
            return Record.ControllerId == ControllerId;
        });
}

const FTGControllerRecord*
UTGConfigControlsWidgetBase::FindLibraryControllerById(
    const FName ControllerId) const
{
    if (ControllerId.IsNone())
    {
        return nullptr;
    }

    return LibraryControllers.FindByPredicate(
        [ControllerId](const FTGControllerRecord& Record)
        {
            return Record.ControllerId == ControllerId;
        });
}

FString UTGConfigControlsWidgetBase::ControlModeToString(
    const ETGControlMode Mode)
{
    return Mode == ETGControlMode::CompiledUserController
        ? TGConfigControlsPrivate::UserControllerModeText
        : TGConfigControlsPrivate::NoneModeText;
}

bool UTGConfigControlsWidgetBase::TryParseControlMode(
    const FString& Text,
    ETGControlMode& OutMode)
{
    if (Text == TGConfigControlsPrivate::NoneModeText)
    {
        OutMode = ETGControlMode::None;
        return true;
    }

    if (Text == TGConfigControlsPrivate::UserControllerModeText)
    {
        OutMode = ETGControlMode::CompiledUserController;
        return true;
    }

    return false;
}

FString UTGConfigControlsWidgetBase::BuildStatusToString(
    const ETGControllerBuildStatus Status)
{
    switch (Status)
    {
    case ETGControllerBuildStatus::NotBuilt:
        return TEXT("Not built");

    case ETGControllerBuildStatus::Building:
        return TEXT("Build in progress");

    case ETGControllerBuildStatus::Ready:
        return TEXT("Ready");

    case ETGControllerBuildStatus::Failed:
        return TEXT("Failed");

    case ETGControllerBuildStatus::IncompatibleController:
        return TEXT("Incompatible controller");

    default:
        return TEXT("Unknown");
    }
}

void UTGConfigControlsWidgetBase::ClearScenarioMessage()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ScenarioMessageClearTimer);
    }

    if (TXT_ControlMessage != nullptr)
    {
        TXT_ControlMessage->SetText(FText::GetEmpty());
        TXT_ControlMessage->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

void UTGConfigControlsWidgetBase::ShowScenarioMessage(
    const FText& Message,
    const bool bError)
{
    if (TXT_ControlMessage == nullptr)
    {
        return;
    }

    TXT_ControlMessage->SetText(Message);
    TXT_ControlMessage->SetColorAndOpacity(
        bError
            ? FSlateColor(TGUiTheme::GetPalette().Error)
            : FSlateColor(TGUiTheme::GetPalette().Success));
    TXT_ControlMessage->SetVisibility(
        ESlateVisibility::Visible);

    if (!bError)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                ScenarioMessageClearTimer,
                this,
                &UTGConfigControlsWidgetBase::ClearScenarioMessage,
                TGConfigControlsPrivate::PositiveMessageLifetimeSeconds,
                false);
        }
    }
}

void UTGConfigControlsWidgetBase::ClearLibraryMessage()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LibraryMessageClearTimer);
    }

    if (TXT_LibraryMessage != nullptr)
    {
        TXT_LibraryMessage->SetText(FText::GetEmpty());
        TXT_LibraryMessage->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

void UTGConfigControlsWidgetBase::ShowLibraryMessage(
    const FText& Message,
    const bool bError)
{
    if (TXT_LibraryMessage == nullptr)
    {
        return;
    }

    TXT_LibraryMessage->SetText(Message);
    TXT_LibraryMessage->SetColorAndOpacity(
        bError
            ? FSlateColor(TGUiTheme::GetPalette().Error)
            : FSlateColor(TGUiTheme::GetPalette().Success));
    TXT_LibraryMessage->SetVisibility(
        ESlateVisibility::Visible);

    if (!bError)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                LibraryMessageClearTimer,
                this,
                &UTGConfigControlsWidgetBase::ClearLibraryMessage,
                TGConfigControlsPrivate::PositiveMessageLifetimeSeconds,
                false);
        }
    }
}

void UTGConfigControlsWidgetBase::OpenControllerHelp()
{
    if (ControllerHelpDialog != nullptr &&
        ControllerHelpDialog->IsInViewport())
    {
        ControllerHelpDialog->SetKeyboardFocus();
        return;
    }

    ControllerHelpDialog = CreateWidget<UTGControllerHelpDialogWidget>(
        GetOwningPlayer(),
        UTGControllerHelpDialogWidget::StaticClass());
    if (ControllerHelpDialog == nullptr)
    {
        ShowLibraryMessage(
            FText::FromString(
                TEXT("The controller command reference could not be opened.")),
            true);
        return;
    }

    ControllerHelpDialog->AddToViewport(250);
    ControllerHelpDialog->SetKeyboardFocus();
}

void UTGConfigControlsWidgetBase::HandleControllerHelpClicked()
{
    OpenControllerHelp();
}

void UTGConfigControlsWidgetBase::ClearCreationError()
{
    if (TXT_CreateControllerError != nullptr)
    {
        TXT_CreateControllerError->SetText(FText::GetEmpty());
        TXT_CreateControllerError->SetVisibility(
            ESlateVisibility::Collapsed);
    }
}

void UTGConfigControlsWidgetBase::ShowCreationError(
    const FText& Error)
{
    if (TXT_CreateControllerError != nullptr)
    {
        TXT_CreateControllerError->SetText(Error);
        TXT_CreateControllerError->SetVisibility(
            ESlateVisibility::Visible);
    }
}

void UTGConfigControlsWidgetBase::HandleControllerLibraryChanged()
{
    RefreshControllerLibraryView();
    RefreshFromCurrentDraft();
}

void UTGConfigControlsWidgetBase::HandleControllerBuildFinished(
    const FName ControllerId,
    const bool bSucceeded,
    const FText Message)
{
    RefreshControllerLibraryView();
    RefreshFromCurrentDraft();

    if (ControllerId == SelectedLibraryControllerId)
    {
        ShowLibraryMessage(Message, !bSucceeded);
    }
}
