// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UI/Configuration/Review/TGScenarioReviewTypes.h"
#include "TGScenarioReviewNavigationTarget.generated.h"

/**
 * Public issue-navigation contract implemented by configuration panels.
 *
 * Review and the owning screen pass the complete immutable issue. The target
 * panel remains responsible for selecting its own model object, refreshing
 * its presentation, and focusing the most specific available editor control.
 */
UINTERFACE(BlueprintType)
class TG_API UTGScenarioReviewNavigationTarget : public UInterface
{
    GENERATED_BODY()
};

class TG_API ITGScenarioReviewNavigationTarget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "PHAROS|Review|Navigation")
    bool NavigateToScenarioReviewIssue(const FTGScenarioReviewIssue& Issue);
};

