// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGSolarRadiationPressureEditingLibrary.generated.h"

/**
 * Shared normalization, validation, inheritance, and generated-proxy cache
 * helpers for the Solar Radiation Pressure configuration panel.
 *
 * The library generates component-frame surface triangles from the authored
 * primitive or STL geometry, validates and stores the result, and resolves
 * every triangle's final optical fractions.
 */
UCLASS()
class TG_API UTGSolarRadiationPressureEditingLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /** Converts nonnegative optical weights to fractions summing to one. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static void NormalizeOpticalPropertyWeights(
        UPARAM(ref) FTGSrpOpticalProperties& Properties);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static void NormalizeSolarRadiationPressureScenario(
        UPARAM(ref) FTGSimulationScenario& Scenario);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|SRP")
    static TArray<FString> GetAvailableOccultingBodyNames();

    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|SRP")
    static TArray<FString> GetLogicalRegionNames(
        const FTGComponentConfig& Component);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|SRP")
    static FString GetLogicalRegionDisplayName(
        ETGSrpLogicalRegion Region);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|SRP")
    static bool TryParseLogicalRegionDisplayName(
        const FString& DisplayName,
        ETGSrpLogicalRegion& OutRegion);

    /**
     * Fingerprint of every geometry/resolution input that can invalidate a
     * generated proxy. Display appearance and articulated pose are excluded.
     */
    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|SRP")
    static FString BuildComponentProxyGeometrySignature(
        const FTGComponentConfig& Component);

    UFUNCTION(BlueprintPure, Category = "PHAROS|Environment|SRP")
    static int32 GetTotalGeneratedTriangleCount(
        const FTGSimulationScenario& Scenario);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static bool ValidateSolarRadiationPressureScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutWarning,
        FText& OutError);

    /**
     * Validates only inputs owned by the authoring HUD. Missing or stale
     * generated triangles are handled by automatic surface preparation.
     */
    static bool ValidateSolarRadiationPressureAuthoring(
        const FTGSimulationScenario& Scenario,
        FText& OutWarning,
        FText& OutError);

    /**
     * Generates every missing or stale SRP surface required by an enabled
     * scenario. Standard primitives honor the requested detail; STL topology
     * is preserved exactly so generation never opens or distorts a surface.
     */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static bool PrepareSolarRadiationPressureGeometry(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FText& OutSummary,
        FText& OutWarning,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static bool MarkComponentProxyGenerationRequired(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FString& Reason,
        FText& OutError);

    /**
     * Explicit confirmation endpoint used before stale triangle overrides,
     * incompatible logical-region overrides, and cached triangles are
     * discarded. Logical regions still supported by the current primitive
     * geometry are preserved.
     */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static bool DiscardTriangleOverridesAndGeneratedProxy(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FText& OutError);

    /**
     * Stores generated triangles. The supplied triangles must represent only
     * external surfaces, use component-local right-handed coordinates in
     * meters, and have outward winding. Optical fractions are resolved here.
     */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static bool ApplyGeneratedComponentProxy(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const TArray<FTGOpticalFacetConfig>& GeneratedTriangles,
        const FString& GeometrySignature,
        FText& OutSummary,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Environment|SRP")
    static bool ResolveComponentOpticalProperties(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FText& OutError);

    static bool IsValidOpticalProperties(
        const FTGSrpOpticalProperties& Properties,
        FString& OutReason);

private:
    static bool ValidateSolarRadiationPressureScenarioInternal(
        const FTGSimulationScenario& Scenario,
        bool bRequireGeneratedProxy,
        FText& OutWarning,
        FText& OutError);
};
