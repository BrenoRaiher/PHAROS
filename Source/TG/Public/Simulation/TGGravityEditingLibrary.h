// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "Simulation/TGSimulationScenarioTypes.h"

#include "TGGravityEditingLibrary.generated.h"

/*
 * Reusable editing and normalization operations for the fixed celestial
 * gravity catalog.
 *
 * Blueprint widgets should call these helpers instead of manually searching,
 * breaking and rebuilding the 43-element source array.
 */
UCLASS()
class TG_API UTGGravityEditingLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /*
     * Creates exactly one configuration row for every catalog source.
     *
     * Defaults:
     * - Sun enabled.
     * - Every system barycenter disabled.
     * - Physical system members disabled.
     * - Minor planets disabled.
     * - All radii zero.
     * - Point-mass gravity for every source.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    MakeDefaultCelestialBodyConfigs();

    /*
     * Produces exactly one row per fixed catalog entry, in catalog order.
     *
     * Existing valid rows are preserved.
     * Missing rows are inserted disabled so normalization cannot activate a
     * gravity source omitted by an imported or otherwise authored scenario.
     * Duplicate rows keep their first occurrence.
     * Unknown catalog keys are discarded.
     * Fields unsupported by a catalog source are cleared.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    NormalizeCelestialBodyConfigs(
        const TArray<FTGCelestialBodyConfig>& ExistingConfigs);

    /*
     * Finds one editable row by its stable catalog key.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static bool FindCelestialBodyConfig(
        const TArray<FTGCelestialBodyConfig>& Configs,
        FName CatalogKey,
        FTGCelestialBodyConfig& OutConfig);

    /*
     * Replaces the complete editable row for one catalog source.
     *
     * The catalog key determines the row being replaced.
     * Fixed metadata remains outside the scenario structure.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    ReplaceCelestialBodyConfig(
        const TArray<FTGCelestialBodyConfig>& Configs,
        const FTGCelestialBodyConfig& UpdatedConfig);

    /*
     * Changes explicit gravity selection and applies the HUD exclusivity rule:
     *
     * - Enabling a physical member disables its system barycenter.
     * - Enabling a barycenter disables every physical member of its system.
     * - Disabling a source does not enable anything else.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    SetGravityEnabledWithExclusivity(
        const TArray<FTGCelestialBodyConfig>& Configs,
        FName CatalogKey,
        bool bGravityEnabled);

    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    SetAutomaticActivationRadiusMeters(
        const TArray<FTGCelestialBodyConfig>& Configs,
        FName CatalogKey,
        double RadiusMeters);

    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    SetBarycenterResolutionRadiusMeters(
        const TArray<FTGCelestialBodyConfig>& Configs,
        FName CatalogKey,
        double RadiusMeters);

    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    SetHarmonicModelCsvFilePath(
        const TArray<FTGCelestialBodyConfig>& Configs,
        FName CatalogKey,
        const FString& CsvFilePath);

    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static TArray<FTGCelestialBodyConfig>
    SetMaximumHarmonicDegreeUsed(
        const TArray<FTGCelestialBodyConfig>& Configs,
        FName CatalogKey,
        int32 MaximumDegreeUsed);

    /*
     * Returns true when the scenario may request principal-moon ephemerides.
     *
     * This occurs when:
     * - A principal moon is explicitly enabled.
     * - A principal moon has a nonzero automatic activation radius.
     * - A barycenter that can become active has a nonzero resolution radius
     *   and its system contains one or more principal moons.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Gravity")
    static bool RequiresCompactMoonCatalogInterval(
        const TArray<FTGCelestialBodyConfig>& Configs);
};
