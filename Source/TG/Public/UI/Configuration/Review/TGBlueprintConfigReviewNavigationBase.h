// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/ComboBoxString.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"
#include "TGBlueprintConfigReviewNavigationBase.generated.h"

/**
 * Native interface adapter for configuration pages whose editing logic still
 * lives entirely in Blueprint. Reparenting preserves their graphs while
 * giving Review a common structured navigation entry point.
 */
UCLASS(Blueprintable)
class TG_API UTGBlueprintConfigReviewNavigationBase
    : public UUserWidget,
      public ITGScenarioReviewNavigationTarget
{
    GENERATED_BODY()

public:
    virtual bool NavigateToScenarioReviewIssue_Implementation(
        const FTGScenarioReviewIssue& Issue) override;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    /**
     * Optional exact Blueprint route for pages with dynamic private state
     * (currently Component Tree and Gravity catalog rows).
     */
    UFUNCTION(BlueprintImplementableEvent, Category = "PHAROS|Review|Navigation")
    bool NavigateDynamicScenarioReviewIssue(
        const FTGScenarioReviewIssue& Issue);

private:
    UFUNCTION()
    void HandleIntegratorKindSelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    void CommitIntegratorKindSelection(const FString& SelectedItem);

    bool FocusWidgetNamed(FName WidgetName);
    bool NavigateComponentTreeIssue(
        const FTGScenarioReviewIssue& Issue);
    bool NavigateGravityIssue(
        const FTGScenarioReviewIssue& Issue);

    UPROPERTY(Transient)
    TObjectPtr<UComboBoxString> BoundIntegratorKindCombo;
};
