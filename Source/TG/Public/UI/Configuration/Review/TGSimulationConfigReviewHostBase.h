// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Configuration/Review/TGScenarioReviewTypes.h"
#include "TGSimulationConfigReviewHostBase.generated.h"

class UWidgetSwitcher;
class UWidget;
class UButton;
class UBorder;
class UEditableText;
class USizeBox;
class UTGExecutionTimeLimitDialogWidget;
class UTGConfigReviewWidgetBase;
class UTGSimulationSubsystem;
class UTGUnsavedChangesDialogWidgetBase;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FTGSimulationConfigScenarioLibraryRequested);

/**
 * Native Review-navigation host for WBP_SimulationConfig.
 *
 * The host invokes a typed Blueprint panel-switch override first, preserving
 * Component Tree/SRP preview lifecycles, and then routes the full structured
 * issue to the activated panel interface.
 */
UCLASS(Blueprintable)
class TG_API UTGSimulationConfigReviewHostBase : public UUserWidget
{
    GENERATED_BODY()

public:
    explicit UTGSimulationConfigReviewHostBase(
        const FObjectInitializer& ObjectInitializer);

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Navigation")
    FTGSimulationConfigScenarioLibraryRequested
        OnScenarioLibraryRequested;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnPreviewMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    /**
     * Entry point for BTN_Back. Clean drafts navigate immediately; dirty
     * drafts open the unsaved-changes dialog.
     */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Navigation")
    void RequestBackWithUnsavedGuard();

    /** Opens the Scenario Library after applying the same dirty-draft guard. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Navigation")
    void RequestScenarioLibraryWithUnsavedGuard();

    /** Saves the current draft while presenting the shared loading overlay. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Persistence")
    void RequestSaveCurrentScenarioWithLoading();

    /** Defers an expensive 3D panel switch until its loading UI has painted. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Navigation")
    void RequestConfigPanelSwitchWithLoading(
        int32 TargetPanelIndex,
        const FText& PanelName);

    /**
     * Blueprint bridge that should call the existing RequestMainMenu
     * dispatcher. Keeping the dispatcher in Blueprint preserves AppShell
     * navigation exactly as authored.
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "PHAROS|Navigation")
    void PerformMainMenuNavigation();

    /**
     * Typed bridge to WBP_SimulationConfig's panel-switch lifecycle. Blueprint
     * should override this and call its existing SwitchConfigPanel function.
     */
    UFUNCTION(BlueprintNativeEvent, Category = "PHAROS|Review")
    void PerformConfigPanelSwitch(int32 TargetPanelIndex);
    virtual void PerformConfigPanelSwitch_Implementation(
        int32 TargetPanelIndex);

private:
    enum class EPendingNavigationDestination : uint8
    {
        None,
        MainMenu,
        ScenarioLibrary
    };

    UFUNCTION()
    void HandleReviewIssueNavigationRequested(
        FTGScenarioReviewIssue Issue);

    UWidget* FindConfigPanelForReviewSection(
        ETGScenarioReviewSection Section) const;
    UTGConfigReviewWidgetBase* FindReviewPanel() const;
    void RefreshNavigationVisualState();
    void RefreshScenarioDirtyVisualState();
    void RefreshExecutionPolicyMirror();
    void ApplyProductBranding();

    UTGSimulationSubsystem* GetSimulationSubsystem() const;

    void RequestNavigationWithUnsavedGuard(
        EPendingNavigationDestination Destination);
    void ShowUnsavedChangesDialog();
    void CloseUnsavedChangesDialog();
    void CompletePendingNavigation();
    void ExecuteNavigation(
        EPendingNavigationDestination Destination);
    void BeginScenarioSave(bool bNavigateAfterSave);
    void FinishScenarioSave(
        bool bSucceeded,
        const FGuid& ScenarioId,
        bool bNavigateAfterSave);
    void ExecuteConfigPanelSwitch(int32 TargetPanelIndex);

    void ShowSimulationContextMenu(
        const FVector2D& ScreenPosition);
    void ShowExecutionTimeLimitDialog();
    bool IsScreenPositionInsideWidget(
        const UWidget* Widget,
        const FVector2D& ScreenPosition) const;

    FReply HandleExecutionTimeLimitMenuClicked();

    UFUNCTION()
    void HandleDialogSaveAndBack();

    UFUNCTION()
    void HandleDialogDiscardAndBack();

    UFUNCTION()
    void HandleDialogCancel();

    bool SaveCurrentScenarioForBackNavigation();

    bool IsNavigationResizeHit(
        const FVector2D& ScreenPosition) const;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "PHAROS|Review",
        meta = (BindWidget, AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetSwitcher> ConfigPanelSwitcher;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "PHAROS|Navigation",
        meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<USizeBox> SIZE_NavigationRail;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "PHAROS|Navigation",
        meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UBorder> BORDER_NavigationResizeHandle;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "PHAROS|Navigation",
        meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UButton> BTN_SaveScenario;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "PHAROS|Execution",
        meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UButton> SimulateButton;

    UPROPERTY(
        BlueprintReadOnly,
        Category = "PHAROS|Execution",
        meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UEditableText> INPUT_MaximumWallClockRuntimeSeconds;

    UPROPERTY(EditDefaultsOnly, Category = "PHAROS|Navigation")
    float NavigationRailMinimumWidth = 0.0f;

    UPROPERTY(EditDefaultsOnly, Category = "PHAROS|Navigation")
    float NavigationRailMaximumWidth = 360.0f;

    UPROPERTY(EditDefaultsOnly, Category = "PHAROS|Navigation")
    float NavigationResizeHitWidth = 10.0f;

    UPROPERTY(EditDefaultsOnly, Category = "PHAROS|Navigation")
    TSubclassOf<UTGUnsavedChangesDialogWidgetBase>
        UnsavedChangesDialogClass;

    UPROPERTY(Transient)
    TObjectPtr<UTGConfigReviewWidgetBase> BoundReviewPanel;

    /** Last switcher index whose presentation was applied to the nav rail. */
    int32 LastPresentedPanelIndex = INDEX_NONE;

    bool bHasPresentedDirtyState = false;
    bool bLastPresentedDirtyState = false;

    bool bResizingNavigationRail = false;
    FVector2D ResizeStartScreenPosition = FVector2D::ZeroVector;
    float ResizeStartWidth = 288.0f;

    EPendingNavigationDestination PendingNavigationDestination =
        EPendingNavigationDestination::None;

    bool bScenarioSaveRequestInProgress = false;
    bool bConfigPanelSwitchInProgress = false;

    UPROPERTY(Transient)
    TObjectPtr<UTGUnsavedChangesDialogWidgetBase>
        ActiveUnsavedChangesDialog;

    UPROPERTY(Transient)
    TObjectPtr<UTGExecutionTimeLimitDialogWidget>
        ActiveExecutionTimeLimitDialog;
};
