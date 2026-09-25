// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "UI/Configuration/Review/TGScenarioReviewTypes.h"
#include "TGScenarioReviewValidationLibrary.generated.h"

class UWidgetSwitcher;

/** Read-only, aggregate authoring validation used by the Review panel. */
UCLASS()
class TG_API UTGScenarioReviewValidationLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Review",
        meta = (WorldContext = "WorldContextObject"))
    static bool CanSimulateCurrentScenario(
        const UObject* WorldContextObject,
        FText& OutReason);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Review",
        meta = (WorldContext = "WorldContextObject"))
    static FTGScenarioReviewReport ValidateScenarioForAuthoringReview(
        const UObject* WorldContextObject,
        const FTGSimulationScenario& Scenario);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Review|HUD")
    static bool RefreshReviewPanelInSwitcher(UWidgetSwitcher* Switcher);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Review|HUD")
    static bool NotifyReviewPanelSimulationBlockedInSwitcher(
        UWidgetSwitcher* Switcher,
        FText Reason);

    /**
     * Switches to the requested configuration page and routes the complete
     * issue through its public navigation interface. If the target cannot
     * provide exact routing, it is still focused and the function returns
     * false so the owning screen may keep a diagnostic fallback visible.
     */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Review|HUD")
    static bool NavigateReviewIssueInSwitcher(
        UWidgetSwitcher* Switcher,
        int32 TargetPanelIndex,
        const FTGScenarioReviewIssue& Issue);
};
