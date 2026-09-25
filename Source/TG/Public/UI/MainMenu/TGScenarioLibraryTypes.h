// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "TGSimulationSaveGame.h"
#include "TGScenarioLibraryTypes.generated.h"

UENUM()
enum class ETGScenarioLibraryResultStatus : uint8
{
    ResultAndContinuation,
    ResultWithoutContinuation,
    ScenarioOnly
};

class UTGScenarioLibraryItem;

DECLARE_MULTICAST_DELEGATE_TwoParams(
    FTGOnScenarioLibraryRenameRequested,
    UTGScenarioLibraryItem*,
    const FString&);

/** Transient presentation model shared by the catalog and recent lists. */
UCLASS()
class TG_API UTGScenarioLibraryItem final : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(Transient)
    FTGSavedScenarioRecord Record;

    UPROPERTY(Transient)
    FText ScenarioName;

    UPROPERTY(Transient)
    FText ResultStatus;

    UPROPERTY(Transient)
    FText ResultStatusDetail;

    UPROPERTY(Transient)
    FText LastModified;

    UPROPERTY(Transient)
    FText Created;

    UPROPERTY(Transient)
    FDateTime LatestActivityUtc;

    UPROPERTY(Transient)
    ETGScenarioLibraryResultStatus Status =
        ETGScenarioLibraryResultStatus::ScenarioOnly;

    UPROPERTY(Transient)
    bool bCanVisualize = false;

    UPROPERTY(Transient)
    bool bCanContinue = false;

    FTGOnScenarioLibraryRenameRequested OnRenameRequested;
};
