// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "UI/Configuration/Review/TGScenarioReviewTypes.h"
#include "TGConfigReviewWidgetBase.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UScrollBox;
class UTextBlock;
class UVerticalBox;
class UTGSimulationSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGReviewIssueNavigationRequested,
    FTGScenarioReviewIssue,
    Issue);

/** Native behavior for WBP_Config_Review. */
UCLASS(Blueprintable)
class TG_API UTGConfigReviewWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Review")
    void RefreshFromCurrentDraft();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Review")
    void NotifySimulationBlocked(const FText& Reason);

    /**
     * Reuses the Review button's validation path for a Simulate attempt.
     * Clean reports are approved immediately. Reports with warnings or
     * errors remain visible for the user to review.
     */
    bool PrepareCurrentDraftForSimulation(FText& OutReason);

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Review")
    FTGReviewIssueNavigationRequested OnIssueNavigationRequested;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleValidateClicked();

    UFUNCTION()
    void HandleAcceptClicked();

    UFUNCTION()
    void HandleWarningAcknowledgementChanged(bool bIsChecked);

    UFUNCTION()
    void HandleIssueSelected(int32 IssueIndex);

    UTGSimulationSubsystem* ResolveSimulationSubsystem() const;
    bool ValidateCurrentDraft(FText& OutError);
    void PresentAcceptedState();
    void InvalidateDisplayedReport(const FText& Message);
    void RenderReport();
    void RebuildIssueRows();
    void UpdateAcceptState();
    static FString GetSectionDisplayName(ETGScenarioReviewSection Section);

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ValidateScenario;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_AcceptValidation;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_AcknowledgeWarnings;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_WarningAcknowledgement;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ScenarioName;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ReviewStatus;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ErrorCount;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_WarningCount;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ValidationRevision;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SelectedIssueDetails;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ReviewMessage;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VBOX_IssueRows;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UScrollBox> SCROLL_Issues;

    FTGScenarioReviewReport CurrentReport;
    FTGSimulationScenario ValidatedSnapshot;
    uint64 DisplayedDraftRevision = 0;
    uint64 ValidatedDraftRevision = 0;
    bool bHasValidatedSnapshot = false;
    bool bValidationRunning = false;
};
