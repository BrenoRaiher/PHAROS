// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/Control/TGControllerLibrarySubsystem.h"
#include "Simulation/TGSimulationScenarioTypes.h"

#include "TGControlEditingLibrary.generated.h"

class UWidgetSwitcher;

/** Thin scenario-editing helpers used by callbacks in WBP_Config_Controls. */
UCLASS()
class TG_API UTGControlEditingLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|HUD|Controls",
        meta = (DisplayName = "Set Control Mode"))
    static void SetControlMode(
        UPARAM(ref) FTGControlConfig& Control,
        ETGControlMode Mode);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|HUD|Controls",
        meta = (
            DisplayName = "Select Ready Controller",
            WorldContext = "WorldContextObject"))
    static bool SelectReadyController(
        const UObject* WorldContextObject,
        UPARAM(ref) FTGControlConfig& Control,
        FName ControllerId,
        FText& OutError);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|HUD|Controls",
        meta = (DisplayName = "Clear Selected Controller"))
    static void ClearSelectedController(
        UPARAM(ref) FTGControlConfig& Control);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|HUD|Controls",
        meta = (
            DisplayName = "Get Ready Controllers",
            WorldContext = "WorldContextObject"))
    static TArray<FTGControllerRecord> GetReadyControllers(
        const UObject* WorldContextObject);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|HUD|Controls",
        meta = (
            DisplayName = "Validate Control Selection",
            WorldContext = "WorldContextObject"))
    static bool ValidateControlSelection(
        const UObject* WorldContextObject,
        const FTGControlConfig& Control,
        FText& OutError);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|HUD|Controls",
        meta = (DisplayName = "Refresh Controls Panel In Switcher"))
    static bool RefreshControlsPanelInSwitcher(
        UWidgetSwitcher* Switcher);

private:
    static UTGControllerLibrarySubsystem* ResolveLibrary(
        const UObject* WorldContextObject);
};