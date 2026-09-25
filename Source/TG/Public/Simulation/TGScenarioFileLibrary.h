// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGScenarioFileLibrary.generated.h"

/** Blueprint callbacks for importing and exporting the portable .tgscn file. */
UCLASS()
class TG_API UTGScenarioFileLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Scenario File",
        meta = (WorldContext = "WorldContextObject"))
    static bool ExportScenarioToTgscn(
        const UObject* WorldContextObject,
        const FTGSimulationScenario& Scenario,
        const FString& FilePath,
        FText& OutMessage);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Scenario File",
        meta = (WorldContext = "WorldContextObject"))
    static bool ImportScenarioFromTgscn(
        const UObject* WorldContextObject,
        const FString& FilePath,
        FTGSimulationScenario& OutScenario,
        FText& OutMessage);

    /**
     * Opens the operating-system file picker, imports one .tgscn file, and
     * installs it as the current unsaved configuration draft.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Scenario File",
        meta = (
            WorldContext = "WorldContextObject",
            DisplayName = "Choose and Import PHAROS Scenario as Current Draft"))
    static bool ChooseAndImportTgscnAsCurrentDraft(
        const UObject* WorldContextObject,
        FString& OutSelectedFilePath,
        FText& OutMessage);
};
