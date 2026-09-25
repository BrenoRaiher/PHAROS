// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "Types/SlateEnums.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"
#include "TGConfigAerodynamicsWidgetBase.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableText;
class UMultiLineEditableTextBox;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UUserWidget;
class UVerticalBox;
class UTGSimulationSubsystem;

UCLASS(Blueprintable)
class TG_API UTGConfigAerodynamicsWidgetBase
    : public UUserWidget,
      public ITGScenarioReviewNavigationTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Aerodynamics")
    void RefreshFromCurrentDraft();

    virtual bool NavigateToScenarioReviewIssue_Implementation(
        const FTGScenarioReviewIssue& Issue) override;

    // WBP_Vector3Input dispatcher bridge.
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Aerodynamics")
    void HandleMomentReferenceCenterCommitted(
        FVector NewBackendValue);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleEnableAerodynamicsChanged(bool bIsChecked);

    UFUNCTION()
    void HandleReferenceAreaCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleReferenceLengthCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleMinimumDynamicPressureCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleMaximumDynamicPressureCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleEnableConstantDragFallbackChanged(bool bIsChecked);

    UFUNCTION()
    void HandleFallbackDragCoefficientCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleEnableDatabaseChanged(bool bIsChecked);

    UFUNCTION()
    void HandleBrowseDatabaseClicked();

    UFUNCTION()
    void HandleClearDatabaseClicked();

    UFUNCTION()
    void HandleInterpolationSelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleExtrapolationSelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleNeighborCountCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleInverseDistancePowerCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleUseMaximumNeighborDistanceChanged(bool bIsChecked);

    UFUNCTION()
    void HandleMaximumNeighborDistanceCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UTGSimulationSubsystem* GetSimulationSubsystem() const;
    bool PullWorkingScenarioFromDraft();
    bool CommitWorkingScenarioToDraft();

    void RefreshUiFromWorkingScenario();
    void RefreshDatabaseStatus();
    void SetMomentReferenceCenterWidgetValue(const FVector& Value);

    bool BrowseDatabaseCsv(FString& OutSelectedPath) const;
    static FString NormalizePath(const FString& Path);
    static FText FormatDouble(double Value);
    static FText FormatInteger(int32 Value);

    bool ParsePositiveDouble(
        const FText& Text,
        const TCHAR* FieldName,
        double& OutValue,
        FText& OutError) const;

    bool ParseNonNegativeDouble(
        const FText& Text,
        const TCHAR* FieldName,
        double& OutValue,
        FText& OutError) const;

    void ClearMessages();
    void ShowError(const FText& Error);
    void ShowWarning(const FText& Warning);

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_EnableAerodynamics;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> SCROLL_AeroSetup;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> SIZE_AerodynamicsContentCard;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ReferenceArea;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ReferenceLength;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_MinDynamicPressure;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_MaxDynamicPressure;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_EnableConstantDragFallback;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_ConstantDragFallbackDetails;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_FallbackDragCoefficient;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_EnableDatabase;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_DatabaseDetails;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_DatabaseCsvPath;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BrowseDatabaseCsv;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearDatabaseCsv;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_DatabaseCsvStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_DatabaseCsvPreview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UMultiLineEditableTextBox> VIEW_DatabaseCsvPreview;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_DatabaseInterpolation;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_DatabaseExtrapolation;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_InverseDistanceSettings;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_DatabaseNeighborCount;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_DatabaseInverseDistancePower;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_UseMaximumNeighborDistance;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_MaximumNeighborDistanceRow;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_MaximumNeighborDistance;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> INPUT_MomentReferenceCenter;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_AerodynamicsWarning;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_AerodynamicsError;

    UPROPERTY(Transient)
    FTGSimulationScenario WorkingScenario;

    uint64 LastObservedDraftRevision = 0;
    bool bRefreshing = false;
};
