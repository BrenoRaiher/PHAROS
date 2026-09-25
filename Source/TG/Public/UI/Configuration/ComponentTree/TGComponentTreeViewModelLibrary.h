// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "UI/Configuration/ComponentTree/TGComponentTreeItemObject.h"
#include "TGComponentTreeViewModelLibrary.generated.h"

/**
 * Builds and searches the temporary UObject hierarchy used by UMG Tree View.
 */
UCLASS()
class TG_API UTGComponentTreeViewModelLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Builds one tree-item object for every physical component.
     *
     * Root items are returned as generic UObject references for direct use by
     * UTreeView. OutAllItems retains typed references for selection and lookup.
     *
     * ItemOuter should normally be the component-tree widget itself.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|View Model",
        meta = (
            DisplayName = "Build Component Tree Items",
            DefaultToSelf = "ItemOuter"))
    static bool BuildComponentTreeItems(
        UObject* ItemOuter,
        const FTGSimulationScenario& Scenario,
        TArray<UObject*>& OutRootItems,
        TArray<UTGComponentTreeItemObject*>& OutAllItems,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|View Model",
        meta = (DisplayName = "Find Component Tree Item By ID"))
    static bool FindComponentTreeItemById(
        const TArray<UTGComponentTreeItemObject*>& Items,
        FGuid ComponentId,
        UTGComponentTreeItemObject*& OutItem);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|View Model",
        meta = (
            DisplayName =
                "Find Component Tree Item By Component Index"))
    static bool FindComponentTreeItemByIndex(
        const TArray<UTGComponentTreeItemObject*>& Items,
        int32 ComponentIndex,
        UTGComponentTreeItemObject*& OutItem);
};