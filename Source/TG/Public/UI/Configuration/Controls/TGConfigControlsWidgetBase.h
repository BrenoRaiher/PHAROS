// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/Control/TGControllerLibrarySubsystem.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"

#include "TGConfigControlsWidgetBase.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableText;
class UHorizontalBox;
class UMultiLineEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UTGControllerHelpDialogWidget;

/**
 * Native coordinator for WBP_Config_Controls.
 *
 * Blueprint is intentionally limited to forwarding UMG events into the public
 * functions below. Scenario editing, controller-library operations, async build
 * refresh, selection persistence, and validation stay in C++.
 */
UCLASS(Blueprintable)
class TG_API UTGConfigControlsWidgetBase
    : public UUserWidget,
      public ITGScenarioReviewNavigationTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controls")
    void RefreshFromCurrentDraft();

    virtual bool NavigateToScenarioReviewIssue_Implementation(
        const FTGScenarioReviewIssue& Issue) override;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controls")
    void CommitControlModeFromCurrentDraft(
        const FString& SelectedMode);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controls")
    void CommitScenarioControllerSelectionFromCurrentDraft(
        const FString& SelectedControllerDisplayName);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controls")
    void ClearScenarioControllerFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controls")
    void ShowControllerLibrary();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controls")
    void ShowScenarioControlSetup();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void BeginCreateControllerFromTemplate();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void BeginImportControllerSource();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void BeginImportPrebuiltControllerDll();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void BrowsePendingControllerImportFile();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void ConfirmPendingControllerCreation();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void CancelPendingControllerCreation();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void SelectLibraryController(FName ControllerId);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void RenameSelectedControllerFromText(FText NewNameText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void CommitSelectedControllerTrusted(bool bTrusted);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void SaveSelectedControllerSource();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void BuildSelectedController();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void BeginDeleteSelectedController();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void ConfirmDeleteSelectedController();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|HUD|Controllers")
    void CancelDeleteSelectedController();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

private:
    enum class EPendingControllerCreationKind : uint8
    {
        None,
        Template,
        Source,
        PrebuiltDll
    };

    // ---------------------------------------------------------------------
    // Scenario setup widgets
    // ---------------------------------------------------------------------

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> SWITCH_ControlsView;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ControlMode;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BOX_ScenarioControllerArea;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ScenarioController;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ScenarioControllerStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControlMessage;

    // ---------------------------------------------------------------------
    // Controller library widgets
    // ---------------------------------------------------------------------

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControllerToolchainStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ControllerList;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BOX_SelectedControllerEditor;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_SelectedControllerEditor;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_SelectedControllerTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ControllerName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_ControllerTrusted;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControllerBuildStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UHorizontalBox> HBOX_ControllerStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BOX_ControllerSourceEditor;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControllerSourceLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UMultiLineEditableTextBox> INPUT_ControllerSource;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_SaveControllerSource;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BuildController;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_BuildController;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ControllerDiagnostics;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_LibraryMessage;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_ControllerHelp;

    // ---------------------------------------------------------------------
    // Creation/import modal
    // ---------------------------------------------------------------------

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_CreateControllerModal;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_CreateControllerTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_CreateControllerName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BOX_CreateControllerFile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_CreateControllerFileLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_CreateControllerFilePath;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BrowseCreateControllerFile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_BrowseCreateControllerFile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_CreateControllerTrusted;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_CreateControllerError;

    // ---------------------------------------------------------------------
    // Delete modal
    // ---------------------------------------------------------------------

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_DeleteControllerModal;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_DeleteControllerQuestion;

    // ---------------------------------------------------------------------
    // State
    // ---------------------------------------------------------------------

    UPROPERTY(Transient)
    FTGSimulationScenario WorkingScenario;

    UPROPERTY(Transient)
    TArray<FTGControllerRecord> ReadyControllers;

    UPROPERTY(Transient)
    TArray<FTGControllerRecord> LibraryControllers;

    FName SelectedLibraryControllerId = NAME_None;
    EPendingControllerCreationKind PendingCreationKind =
        EPendingControllerCreationKind::None;
    FString PendingImportFilePath;
    FString LoadedControllerSource;
    FName LoadedSourceControllerId = NAME_None;

    bool bRefreshingScenarioSetup = false;
    bool bRefreshingLibraryEditor = false;
    bool bUpdatingControllerSource = false;
    bool bControllerSourceDirty = false;
    uint64 LastObservedDraftRevision = 0;

    FTimerHandle ScenarioMessageClearTimer;
    FTimerHandle LibraryMessageClearTimer;

    UPROPERTY(Transient)
    TObjectPtr<UTGControllerHelpDialogWidget> ControllerHelpDialog;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> TXT_NoControllerSelected;

    // ---------------------------------------------------------------------
    // Helpers
    // ---------------------------------------------------------------------

    class UTGSimulationSubsystem* ResolveSimulationSubsystem() const;
    UTGControllerLibrarySubsystem* ResolveControllerLibrary() const;

    bool PullWorkingScenarioFromDraft();
    void CommitWorkingScenario();

    void InitializeStaticControls();
    void RefreshScenarioSetup();
    void RefreshControllerLibraryView();
    void RebuildReadyControllerCombo();
    void RebuildControllerList();
    void LoadSelectedControllerEditor();
    void HideSelectedControllerEditor();
    void EnsureSelectedControllerPlaceholder();
    void SetSelectedControllerEditorEmptyState(bool bEmpty);
    void UpdateControllerSourceState();
    bool SaveSelectedControllerSourceInternal(bool bShowConfirmation);
    bool SaveDirtyControllerSourceBeforeContextChange();

    const FTGControllerRecord* FindReadyControllerByDisplayName(
        const FString& DisplayName) const;
    const FTGControllerRecord* FindReadyControllerById(
        FName ControllerId) const;
    const FTGControllerRecord* FindLibraryControllerById(
        FName ControllerId) const;

    static FString ControlModeToString(ETGControlMode Mode);
    static bool TryParseControlMode(
        const FString& Text,
        ETGControlMode& OutMode);
    static FString BuildStatusToString(ETGControllerBuildStatus Status);

    void BeginPendingCreation(EPendingControllerCreationKind Kind);
    void CloseCreationModal();
    void CloseDeleteModal();
    void OpenControllerHelp();

    void ClearScenarioMessage();
    void ShowScenarioMessage(const FText& Message, bool bError);
    void ClearLibraryMessage();
    void ShowLibraryMessage(const FText& Message, bool bError);
    void ClearCreationError();
    void ShowCreationError(const FText& Error);

    UFUNCTION()
    void HandleControllerSourceTextChanged(const FText& SourceText);

    UFUNCTION()
    void HandleControllerHelpClicked();

    UFUNCTION()
    void HandleControllerLibraryChanged();

    UFUNCTION()
    void HandleControllerBuildFinished(
        FName ControllerId,
        bool bSucceeded,
        FText Message);
};
