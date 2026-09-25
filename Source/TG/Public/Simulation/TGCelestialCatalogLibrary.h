// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TGCelestialCatalogLibrary.generated.h"

/*
 * Fixed role assigned by the authoritative celestial catalog.
 * This is metadata, not a user-selectable setting.
 */
UENUM(BlueprintType)
enum class ETGCelestialSourceRole : uint8
{
    IndependentBody
        UMETA(DisplayName = "Independent Body"),

    SystemBarycenter
        UMETA(DisplayName = "System Barycenter"),

    PhysicalSystemMember
        UMETA(DisplayName = "Physical System Member")
};

/*
 * One immutable source in the supported SPICE catalog.
 *
 * Scenario files store only the CatalogKey and editable gravity settings.
 * The converter later obtains the authoritative identity from this catalog.
 */
USTRUCT(BlueprintType)
struct TG_API FTGCelestialCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    FName CatalogKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "SPICE")
    FString SpiceTarget;

    UPROPERTY(BlueprintReadOnly, Category = "SPICE")
    int32 NaifId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "System")
    FName SystemKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "System")
    FText SystemDisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "System")
    ETGCelestialSourceRole SourceRole =
        ETGCelestialSourceRole::IndependentBody;

    /*
     * True for physical bodies that may receive a user-selected
     * spherical-harmonic CSV.
     *
     * False for barycenters and the point-mass-only minor planets.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
    bool bSupportsHarmonicGravity = false;

    /*
     * Marks moons whose availability can require the compact
     * 2000-01-01 through 2050-01-01 catalog interval.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Capabilities")
    bool bIsPrincipalMoon = false;
};

/*
 * Presentation-oriented grouping used by the Gravity panel.
 *
 * This prevents Blueprint from having to manually classify or regroup
 * the fixed catalog.
 */
USTRUCT(BlueprintType)
struct TG_API FTGCelestialCatalogGroup
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    FName GroupKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Catalog")
    TArray<FTGCelestialCatalogEntry> Sources;
};

UCLASS()
class TG_API UTGCelestialCatalogLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /*
     * Returns all 44 selectable gravity sources in stable catalog order.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Celestial Catalog")
    static TArray<FTGCelestialCatalogEntry>
    GetCelestialCatalog();

    /*
     * Returns the same catalog already arranged into HUD groups.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Celestial Catalog")
    static TArray<FTGCelestialCatalogGroup>
    GetCelestialCatalogGroups();

    /*
     * Finds one authoritative entry using its stable internal key.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Celestial Catalog")
    static bool FindCelestialCatalogEntry(
        FName CatalogKey,
        FTGCelestialCatalogEntry& OutEntry);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Celestial Catalog",
        meta = (DisplayName = "Get Celestial Source Role Display Text"))
    static FText GetCelestialSourceRoleDisplayText(
        ETGCelestialSourceRole SourceRole);

    /*
     * Returns true when resolving this system can require one or more
     * principal-moon ephemerides.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "TGSim|Celestial Catalog")
    static bool SystemContainsPrincipalMoon(
        FName SystemKey);
};
