// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/Configuration/Review/TGScenarioReviewTypes.h"
#include "TGReviewIssueRowWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGReviewIssueSelected,
    int32,
    IssueIndex);

/** Native row created for each aggregate Review issue. */
UCLASS()
class TG_API UTGReviewIssueRowWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    void Configure(int32 InIssueIndex, const FTGScenarioReviewIssue& Issue);

    static FText MakeUserFacingSummary(const FTGScenarioReviewIssue& Issue);

    UPROPERTY()
    FTGReviewIssueSelected OnIssueSelected;

protected:
    virtual void NativeOnInitialized() override;

private:
    UFUNCTION()
    void HandleClicked();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UButton> SelectButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> SummaryText;

    int32 IssueIndex = INDEX_NONE;
};
