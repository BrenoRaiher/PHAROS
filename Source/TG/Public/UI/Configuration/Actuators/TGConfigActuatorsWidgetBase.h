// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "Types/SlateEnums.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"
#include "TGConfigActuatorsWidgetBase.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableText;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UScrollBox;
class UTextBlock;
class UUserWidget;
class UVerticalBox;
class UWidgetSwitcher;
class UTGSimulationSubsystem;
class UTGUtcDateTimeInput;

/**
 * Native behavior for WBP_Config_Actuators.
 *
 * UMG events are intentionally bridged explicitly from Blueprint into the
 * BlueprintCallable methods below. This matches the working Reaction Wheels
 * pattern and keeps all stateful CRUD/validation in C++.
 */
UCLASS(Blueprintable)
class TG_API UTGConfigActuatorsWidgetBase
    : public UUserWidget,
      public ITGScenarioReviewNavigationTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators")
    void RefreshFromCurrentDraft();

    virtual bool NavigateToScenarioReviewIssue_Implementation(
        const FTGScenarioReviewIssue& Issue) override;

    // Thrusters ---------------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void AddThrusterFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void DeleteSelectedThrusterFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void SelectThrusterFromCurrentDraft(int32 ThrusterIndex);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterNameFromCurrentDraft(FText NewNameText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterModeFromCurrentDraft(FString SelectedMode);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterMountComponentFromCurrentDraft(
        FString NewMountComponentName);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterPropellantComponentFromCurrentDraft(
        FString NewPropellantComponentName);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void HandleThrusterApplicationPointCommitted(
        FVector NewBackendValue);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void HandleThrusterDirectionCommitted(
        FVector NewBackendValue);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void ApplyPendingThrusterDirectionFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterIgnitionTimeModeFromCurrentDraft(
        FString SelectedMode);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterIgnitionValueFromCurrentDraft(
        FText NewValueText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterNeverShutsDownFromCurrentDraft(
        bool bNewNeverShutsDown);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterShutdownTimeModeFromCurrentDraft(
        FString SelectedMode);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterShutdownValueFromCurrentDraft(
        FText NewValueText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterThrustSourceFromCurrentDraft(
        FString SelectedSource);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void BrowseThrusterThrustCsvFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void ClearThrusterThrustCsvFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterConstantThrustFromCurrentDraft(
        FText NewValueText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterIspSourceFromCurrentDraft(
        FString SelectedSource);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void BrowseThrusterIspCsvFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void ClearThrusterIspCsvFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterConstantIspFromCurrentDraft(
        FText NewValueText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Thrusters")
    void CommitThrusterMaximumThrustFromCurrentDraft(
        FText NewValueText);

    // Reaction wheels ---------------------------------------------------------

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void AddReactionWheelFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void DeleteSelectedReactionWheelFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void SelectReactionWheelFromCurrentDraft(
        int32 ReactionWheelIndex);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void CommitReactionWheelNameFromCurrentDraft(
        FText NewNameText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void CommitReactionWheelMountComponentFromCurrentDraft(
        FString NewMountComponentName);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void HandleReactionWheelAxisCommitted(
        FVector NewBackendValue);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void ApplyPendingReactionWheelAxisFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void CommitReactionWheelInitialMomentumFromCurrentDraft(
        FText NewValueText);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators|Reaction Wheels")
    void CommitReactionWheelMaximumMomentumFromCurrentDraft(
        FText NewValueText);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

    virtual FReply NativeOnPreviewMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

private:
    // Shared Designer widgets ------------------------------------------------

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_ThrustersTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_ThrustersTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UButton> BTN_ReactionWheelsTab;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_ReactionWheelsTab;

    UPROPERTY(
        BlueprintReadOnly,
        meta = (BindWidgetOptional, AllowPrivateAccess = "true"))
    TObjectPtr<UWidgetSwitcher> SWITCH_ActuatorType;

    // Thruster Designer widgets ----------------------------------------------

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_AddThruster;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterList;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> SCROLL_ThrusterList;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> SCROLL_ThrusterEditor;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> BOX_SelectedThrusterEditor;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SelectedThrusterTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ThrusterError;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ThrusterWarning;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ThrusterName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ThrusterMode;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ThrusterMountComponent;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ThrusterPropellantComponent;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> INPUT_ThrusterApplicationPoint;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> INPUT_ThrusterDirection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ApplyThrusterDirection;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ThrusterDirectionStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ThrusterIgnitionTimeMode;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ThrusterIgnitionValueLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ThrusterIgnitionValue;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> SWITCH_ThrusterIgnitionInput;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTGUtcDateTimeInput> INPUT_ThrusterIgnitionUtc;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_ThrusterNeverShutsDown;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterShutdownControls;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ThrusterShutdownTimeMode;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ThrusterShutdownValueLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ThrusterShutdownValue;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UWidgetSwitcher> SWITCH_ThrusterShutdownInput;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTGUtcDateTimeInput> INPUT_ThrusterShutdownUtc;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterPrescribedPerformance;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ThrusterThrustSource;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterConstantThrustRow;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ThrusterConstantThrust;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterThrustCsvRow;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ThrusterThrustCsvPath;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BrowseThrusterThrustCsv;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearThrusterThrustCsv;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_ThrusterThrustCsvStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_ThrusterThrustCsvPreview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UMultiLineEditableTextBox> VIEW_ThrusterThrustCsvPreview;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ThrusterIspSource;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterConstantIspRow;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ThrusterConstantIsp;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterIspCsvRow;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ThrusterIspCsvPath;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BrowseThrusterIspCsv;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearThrusterIspCsv;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_ThrusterIspCsvStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_ThrusterIspCsvPreview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UMultiLineEditableTextBox> VIEW_ThrusterIspCsvPreview;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ThrusterCommandedPerformance;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ThrusterMaximumThrust;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_DeleteThruster;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_DeleteThruster;

    // Reaction-wheel Designer widgets ----------------------------------------

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_AddReactionWheel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ReactionWheelList;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> SCROLL_ReactionWheelList;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UScrollBox> SCROLL_ReactionWheelEditor;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> BOX_SelectedReactionWheelEditor;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SelectedReactionWheelTitle;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_WheelName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_WheelMountComponent;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> INPUT_WheelAxis;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ApplyWheelAxis;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_WheelAxisStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_WheelInitialMomentum;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_WheelMaximumMomentum;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ReactionWheelWarning;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ReactionWheelError;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_DeleteReactionWheel;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_DeleteReactionWheel;

    // State ------------------------------------------------------------------

    FTGSimulationScenario WorkingScenario;

    int32 SelectedThrusterIndex = INDEX_NONE;
    bool bRefreshingThrusterEditor = false;
    FVector PendingThrusterDirection = FVector::ForwardVector;
    bool bHasPendingThrusterDirection = false;

    int32 SelectedReactionWheelIndex = INDEX_NONE;
    bool bRefreshingReactionWheelEditor = false;
    FVector PendingReactionWheelAxis = FVector::ForwardVector;
    bool bHasPendingReactionWheelAxis = false;
    uint64 LastObservedDraftRevision = 0;

    enum class EPendingActuatorDelete : uint8
    {
        None,
        Thruster,
        ReactionWheel
    };

    EPendingActuatorDelete PendingActuatorDelete =
        EPendingActuatorDelete::None;

    UPROPERTY(Transient)
    TObjectPtr<UUserWidget> ActiveDeleteDialog;

    // Draft ------------------------------------------------------------------

    UTGSimulationSubsystem* GetSimulationSubsystem() const;
    bool PullWorkingScenarioFromDraft();
    bool CommitWorkingScenarioToDraft();

    void UpdateActuatorTabPresentation();

    // Thruster helpers --------------------------------------------------------

    void RebuildThrusterList();
    void LoadSelectedThrusterEditor();
    void HideThrusterEditor();
    void UpdateThrusterConditionalVisibility(
        const FTGThrusterConfig& Thruster);

    bool GetSelectedThruster(
        FTGThrusterConfig& OutThruster) const;

    bool PrepareSelectedThrusterFromCurrentDraft(
        FTGThrusterConfig& OutThruster);

    bool ApplySelectedThrusterCandidate(
        const FTGThrusterConfig& Candidate,
        bool bRebuildListAfterSuccess);

    bool CommitSelectedThrusterDraftWithoutValidation(
        const FTGThrusterConfig& Candidate);

    bool CommitPendingThrusterDirection();

    bool SelectCsvFile(
        const FString& DialogTitle,
        const FString& ExistingPath,
        FString& OutSelectedPath) const;

    void SetVectorWidgetValue(
        UUserWidget* VectorWidget,
        const FVector& Value,
        const FString& FieldNameForError);

    void ClearThrusterMessages();
    void ShowThrusterWarning(const FText& Message);
    void ShowThrusterError(const FText& Message);

    UFUNCTION()
    void HandleThrusterIgnitionUtcCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleThrusterShutdownUtcCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleThrusterUtcValidationFailed(
        const FText& ValidationMessage);

    UFUNCTION()
    void HandleClearThrusterThrustCsvClicked();

    UFUNCTION()
    void HandleClearThrusterIspCsvClicked();

    // Reaction-wheel helpers --------------------------------------------------

    void RebuildReactionWheelList();
    void LoadSelectedReactionWheelEditor();
    void HideReactionWheelEditor();

    bool GetSelectedWheel(
        FTGReactionWheelConfig& OutWheel) const;

    bool ApplySelectedWheelCandidate(
        const FTGReactionWheelConfig& Candidate,
        bool bRebuildListAfterSuccess);

    bool CommitPendingReactionWheelAxis();

    bool PrepareSelectedWheelFromCurrentDraft(
        FTGReactionWheelConfig& OutWheel);

    void SetWheelAxisWidgetValue(
        const FVector& Value);

    void ClearReactionWheelMessages();
    void ShowReactionWheelWarning(const FText& Message);
    void ShowReactionWheelError(const FText& Message);

    void ConfirmDeleteSelectedThruster();
    void ConfirmDeleteSelectedReactionWheel();
    void OpenActuatorDeleteDialog(
        EPendingActuatorDelete DeleteType,
        const FString& ActuatorName);
    void CloseActuatorDeleteDialog();

    UFUNCTION()
    void HandleActuatorDeleteDialogAccepted();

    UFUNCTION()
    void HandleActuatorDeleteDialogCancelled();

    // Shared presentation -----------------------------------------------------

    static FText FormatDouble(double Value);
};
