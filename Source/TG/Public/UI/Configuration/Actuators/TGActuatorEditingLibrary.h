// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGActuatorEditingLibrary.generated.h"

class UWidgetSwitcher;

/**
 * Reusable Blueprint/C++ editing and validation helpers for actuator records.
 */
UCLASS()
class TG_API UTGActuatorEditingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    // Shared component helpers ------------------------------------------------

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Get Actuator Mount Component Names"))
    static TArray<FString> GetActuatorMountComponentNames(
        const FTGSimulationScenario& Scenario);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Get Variable Mass Component Names"))
    static TArray<FString> GetVariableMassComponentNames(
        const FTGSimulationScenario& Scenario);

    // Thrusters ---------------------------------------------------------------

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Get Thruster Names"))
    static TArray<FString> GetThrusterNames(
        const FTGSimulationScenario& Scenario);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Is Valid Thruster Index"))
    static bool IsValidThrusterIndex(
        const FTGSimulationScenario& Scenario,
        int32 ThrusterIndex);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Add Default Thruster"))
    static bool AddDefaultThruster(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        int32& OutNewThrusterIndex,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Get Thruster At Index"))
    static bool GetThrusterAtIndex(
        const FTGSimulationScenario& Scenario,
        int32 ThrusterIndex,
        FTGThrusterConfig& OutThruster);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Apply Thruster"))
    static bool ApplyThruster(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        int32 ThrusterIndex,
        const FTGThrusterConfig& UpdatedThruster,
        FTGThrusterConfig& OutAcceptedThruster,
        FText& OutWarningText,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Delete Thruster"))
    static bool DeleteThruster(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        int32 ThrusterIndex,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Validate Thruster"))
    static bool ValidateThruster(
        const FTGSimulationScenario& Scenario,
        const FTGThrusterConfig& Thruster,
        FTGThrusterConfig& OutAcceptedThruster,
        FText& OutWarningText,
        FText& OutErrorText);

    static bool ValidateThrustProfileCsv(
        const FString& CsvFilePath,
        FString& OutNormalizedPath,
        FText& OutErrorText);

    static bool ValidateSpecificImpulseProfileCsv(
        const FString& CsvFilePath,
        FString& OutNormalizedPath,
        FText& OutErrorText);

    // Reaction wheels ---------------------------------------------------------

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Get Reaction Wheel Names"))
    static TArray<FString> GetReactionWheelNames(
        const FTGSimulationScenario& Scenario);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Is Valid Reaction Wheel Index"))
    static bool IsValidReactionWheelIndex(
        const FTGSimulationScenario& Scenario,
        int32 WheelIndex);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Add Default Reaction Wheel"))
    static bool AddDefaultReactionWheel(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        int32& OutNewWheelIndex,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Get Reaction Wheel At Index"))
    static bool GetReactionWheelAtIndex(
        const FTGSimulationScenario& Scenario,
        int32 WheelIndex,
        FTGReactionWheelConfig& OutWheel);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Apply Reaction Wheel"))
    static bool ApplyReactionWheel(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        int32 WheelIndex,
        const FTGReactionWheelConfig& UpdatedWheel,
        FTGReactionWheelConfig& OutAcceptedWheel,
        FText& OutWarningText,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Delete Reaction Wheel"))
    static bool DeleteReactionWheel(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        int32 WheelIndex,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Validate Reaction Wheel"))
    static bool ValidateReactionWheel(
        const FTGSimulationScenario& Scenario,
        const FTGReactionWheelConfig& Wheel,
        FTGReactionWheelConfig& OutAcceptedWheel,
        FText& OutWarningText,
        FText& OutErrorText);

    // Panel integration -------------------------------------------------------

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Actuators",
        meta = (DisplayName = "Refresh Actuators Panel In Switcher"))
    static bool RefreshActuatorsPanelInSwitcher(
        UWidgetSwitcher* Switcher);
};
