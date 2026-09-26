// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "UI/Configuration/Review/TGBlueprintConfigReviewNavigationBase.h"
#include "TGConfigInitialStateWidgetBase.generated.h"

class UTGConfigInitialStateFrameWidgetBase;

/**
 * Native host for the Initial State configuration page.
 *
 * The nested frame-aware widget owns all state editing and always commits the
 * canonical FTGSimulationScenario state in ICRF. This host keeps the public
 * refresh entry point used by WBP_SimulationConfig.
 */
UCLASS(Blueprintable)
class TG_API UTGConfigInitialStateWidgetBase
    : public UTGBlueprintConfigReviewNavigationBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Initial State")
    void RefreshFromCurrentDraft();

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTGConfigInitialStateFrameWidgetBase>
        WBP_Config_InitialStateFrame;
};
