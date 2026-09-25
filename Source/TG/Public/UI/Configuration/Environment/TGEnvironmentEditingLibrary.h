// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGEnvironmentEditingLibrary.generated.h"

class UWidgetSwitcher;
class ATGSpacecraftPreviewPawn;
class ATGSpacecraftVisualActor;

/**
 * Shared validation and refresh helpers for environment configuration panels.
 */
UCLASS()
class TG_API UTGEnvironmentEditingLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|HUD")
    static bool RefreshSolarRadiationPressurePanelInSwitcher(
        UWidgetSwitcher* Switcher);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|HUD")
    static bool ActivateSolarRadiationPressurePanelInSwitcher(
        UWidgetSwitcher* Switcher,
        ATGSpacecraftPreviewPawn* PreviewPawn,
        ATGSpacecraftVisualActor* SpacecraftActor);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|HUD")
    static bool DeactivateSolarRadiationPressurePanelInSwitcher(
        UWidgetSwitcher* Switcher);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|Atmosphere")
    static TArray<FString> GetAtmosphereCentralBodyNames();

    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|Aerodynamics")
    static int32 GetFlattenedArticulationDofCount(
        const FTGSimulationScenario& Scenario);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|CSV")
    static bool BuildCsvPreview(
        const FString& CsvFilePath,
        const TArray<FString>& ColumnHeadings,
        int32 MaximumRows,
        FText& OutPreview,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Atmosphere")
    static bool ValidateGeneralAtmosphereProfileCsv(
        const FString& CsvFilePath,
        FText& OutSummary,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Atmosphere")
    static bool ValidateChpCoefficientCsv(
        const FString& CsvFilePath,
        FText& OutSummary,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Atmosphere")
    static bool ValidateChpMolecularProfileCsv(
        const FString& CsvFilePath,
        FText& OutSummary,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Aerodynamics")
    static bool ValidateAerodynamicDatabaseCsv(
        const FString& CsvFilePath,
        int32 ExpectedArticulationCoordinateCount,
        FText& OutSummary,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Atmosphere")
    static bool ValidateAtmosphereScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutWarning,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|Aerodynamics")
    static bool ValidateAerodynamicsScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutWarning,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|HUD")
    static bool RefreshAtmospherePanelInSwitcher(
        UWidgetSwitcher* Switcher);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|HUD")
    static bool RefreshAerodynamicsPanelInSwitcher(
        UWidgetSwitcher* Switcher);
};
