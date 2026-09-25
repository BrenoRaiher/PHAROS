// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "Types/SlateEnums.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"
#include "TGConfigAtmosphereWidgetBase.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UEditableText;
class UMultiLineEditableTextBox;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UVerticalBox;
class UWidgetSwitcher;
class UTGSimulationSubsystem;

UCLASS(Blueprintable)
class TG_API UTGConfigAtmosphereWidgetBase
    : public UUserWidget,
      public ITGScenarioReviewNavigationTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Atmosphere")
    void RefreshFromCurrentDraft();

    virtual bool NavigateToScenarioReviewIssue_Implementation(
        const FTGScenarioReviewIssue& Issue) override;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleEnableAtmosphereChanged(bool bIsChecked);

    UFUNCTION()
    void HandleCentralBodySelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleModelSelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleBrowseGeneralProfileClicked();

    UFUNCTION()
    void HandleClearGeneralProfileClicked();

    UFUNCTION()
    void HandleChpF107Committed(
        const FText& Text,
        ETextCommit::Type CommitMethod);

    UFUNCTION()
    void HandleBrowseChpCoefficientClicked();

    UFUNCTION()
    void HandleClearChpCoefficientClicked();

    UFUNCTION()
    void HandleBrowseChpMolecularClicked();

    UFUNCTION()
    void HandleClearChpMolecularClicked();

    UTGSimulationSubsystem* GetSimulationSubsystem() const;
    bool PullWorkingScenarioFromDraft();
    bool CommitWorkingScenarioToDraft();

    void RefreshUiFromWorkingScenario();
    void RefreshFileStatuses();

    bool BrowseCsv(
        const FString& DialogTitle,
        const FString& CurrentPath,
        FString& OutSelectedPath) const;

    static FString NormalizePath(const FString& Path);
    static FText FormatDouble(double Value);

    void ClearMessages();
    void ShowError(const FText& Error);
    void ShowWarning(const FText& Warning);

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_EnableAtmosphere;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> SCROLL_Atmosphere;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> SIZE_AtmosphereContentCard;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_AtmosphereCentralBody;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_AtmosphereModel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> SWITCH_AtmosphereModel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_GeneralProfilePath;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BrowseGeneralProfile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearGeneralProfile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_GeneralProfileStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_GeneralProfilePreview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UMultiLineEditableTextBox> VIEW_GeneralProfileCsvPreview;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableText> INPUT_ChpF107;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ChpCoefficientPath;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BrowseChpCoefficient;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearChpCoefficient;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ChpCoefficientStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_ChpCoefficientPreview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UMultiLineEditableTextBox> VIEW_ChpCoefficientCsvPreview;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ChpMolecularProfilePath;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BrowseChpMolecularProfile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ClearChpMolecularProfile;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ChpMolecularProfileStatus;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UVerticalBox> VBOX_ChpMolecularProfilePreview;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UMultiLineEditableTextBox> VIEW_ChpMolecularProfileCsvPreview;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_AtmosphereWarning;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_AtmosphereError;

    UPROPERTY(Transient)
    FTGSimulationScenario WorkingScenario;

    uint64 LastObservedDraftRevision = 0;
    bool bRefreshing = false;
};
