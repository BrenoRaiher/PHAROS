// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "TGImportScenarioAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGScenarioImportAsyncResult,
    FString,
    SelectedFilePath,
    FText,
    Message);

/** Chooses and imports a PHAROS scenario without blocking the Slate thread. */
UCLASS()
class TG_API UTGImportScenarioAsyncAction final
    : public UBlueprintAsyncActionBase
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintAssignable)
    FTGScenarioImportAsyncResult Succeeded;

    UPROPERTY(BlueprintAssignable)
    FTGScenarioImportAsyncResult Failed;

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Scenario",
        meta = (
            BlueprintInternalUseOnly = "true",
            WorldContext = "WorldContextObject",
            DisplayName = "Import PHAROS Scenario Async"))
    static UTGImportScenarioAsyncAction* ImportPharosScenarioAsync(
        UObject* WorldContextObject);

    virtual void Activate() override;

private:
    UPROPERTY(Transient)
    TObjectPtr<UObject> ContextObject;

    FString SelectedFilePath;

    void BeginBackgroundImport();
    void CompleteWithoutImport(const FText& Message);
};
