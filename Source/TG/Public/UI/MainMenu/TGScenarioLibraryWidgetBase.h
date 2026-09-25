// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "Blueprint/UserWidget.h"
#include "Components/ListView.h"
#include "TGScenarioLibraryWidgetBase.generated.h"

class UBorder;
class UButton;
class UComboBoxString;
class UEditableTextBox;
class UTextBlock;
class UTGScenarioLibraryEntryWidgetBase;
class UTGScenarioLibraryItem;
class UTGSimulationSubsystem;

/** List view with the PHAROS scroll-bar treatment. */
UCLASS(meta = (DisplayName = "PHAROS Scenario Library List View"))
class TG_API UTGScenarioLibraryListView final : public UListView
{
    GENERATED_BODY()

public:
    explicit UTGScenarioLibraryListView(
        const FObjectInitializer& ObjectInitializer);
};

/** Native behavior for search, sorting, recents, selection, and actions. */
UCLASS(Abstract, Blueprintable)
class TG_API UTGScenarioLibraryWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual bool Initialize() override;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|UI|Scenario Library")
    void RefreshScenarioList();

protected:
    virtual void SynchronizeProperties() override;
    virtual void OnWidgetRebuilt() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    /** Bridge this event to the existing RequestMainMenu dispatcher. */
    UFUNCTION(BlueprintImplementableEvent, Category = "PHAROS|UI|Scenario Library")
    void BP_RequestMainMenu();

    /** Bridge this event to RequestOpenScenarioConfiguration. */
    UFUNCTION(BlueprintImplementableEvent, Category = "PHAROS|UI|Scenario Library")
    void BP_RequestOpenScenarioConfiguration();

private:
    enum class ESortField : uint8
    {
        LastModified,
        Created,
        Name,
        ResultStatus
    };

    void ApplyPrepassSafeStyles();
    void ConfigureControls();
    void RebuildVisibleLists();
    void RebuildRecentList();
    void UpdateActionState();
    void UpdateSortDirectionText();
    void CloseAllInlineRenameEditors();
    void FinalizeRenameRefresh();

    UTGSimulationSubsystem* GetSimulationSubsystem() const;
    TArray<UTGScenarioLibraryItem*> GetSelectedItems() const;
    UTGScenarioLibraryItem* GetSingleSelectedItem() const;

    void HandleCatalogSelectionChanged(UObject* SelectedItem);
    void HandleRecentSelectionChanged(UObject* SelectedItem);
    void HandleCatalogItemClicked(UObject* ClickedItem);
    void HandleItemDoubleClicked(UObject* SelectedItem);
    void HandleRenameRequested(
        UTGScenarioLibraryItem* Item,
        const FString& NewName);

    UFUNCTION()
    void HandleSearchTextChanged(const FText& Text);

    UFUNCTION()
    void HandleSortFieldChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleSortDirectionClicked();

    UFUNCTION()
    void HandleOpenScenarioClicked();

    UFUNCTION()
    void HandleOpenVisualizationClicked();

    UFUNCTION()
    void HandleContinueClicked();

    UFUNCTION()
    void HandleDeleteClicked();

    UFUNCTION()
    void HandleConfirmDeleteClicked();

    UFUNCTION()
    void HandleCancelDeleteClicked();

    UFUNCTION()
    void HandleRefreshClicked();

    UFUNCTION()
    void HandleBackClicked();

    void SetStatusMessage(
        const FText& Message,
        bool bIsError);

    void ClearStatusMessage();
    void SetDeleteConfirmationVisible(bool bVisible);

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTGScenarioLibraryItem>> ScenarioItems;

    UPROPERTY(Transient)
    TArray<FGuid> PendingDeleteScenarioIds;

    FString SearchText;
    ESortField SortField = ESortField::LastModified;
    bool bSortDescending = true;
    bool bUpdatingSelection = false;
    bool bConfiguringControls = false;
    TWeakObjectPtr<UTGScenarioLibraryItem> PendingRenameItem;
    TWeakObjectPtr<UTGScenarioLibraryEntryWidgetBase> ActiveRenameEntry;
    double PendingRenameClickSeconds = -1.0;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_OpenScenario;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_OpenVisualization;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ContinueFromEnd;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_DeleteSelected;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Refresh;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Back;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_SortDirection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> INPUT_Search;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> INPUT_SortBy;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTGScenarioLibraryListView> LIST_Recent;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTGScenarioLibraryListView> LIST_Scenarios;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SortDirection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ListSummary;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RecentEmpty;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_LibraryEmpty;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Status;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_DeleteConfirmation;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_DeleteDialog;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_DeleteConfirmationTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_DeleteConfirmationBody;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ConfirmDelete;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_CancelDelete;
};
