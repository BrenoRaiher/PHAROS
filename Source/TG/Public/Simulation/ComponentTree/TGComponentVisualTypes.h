// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "TGComponentVisualTypes.generated.h"

/**
 * Source used to represent a physical spacecraft component visually.
 */
UENUM(BlueprintType)
enum class ETGComponentGeometrySource : uint8
{
    NoGeometry UMETA(Hidden),
    Primitive UMETA(DisplayName = "Standard Primitive"),
    CustomStl UMETA(DisplayName = "Custom STL")
};

/**
 * Standard geometry supplied directly by the application.
 */
UENUM(BlueprintType)
enum class ETGPrimitiveGeometryType : uint8
{
    Box UMETA(DisplayName = "Box / Cuboid"),
    Sphere UMETA(DisplayName = "Sphere"),
    Cylinder UMETA(DisplayName = "Cylinder")
};

/**
 * Whole-component surface appearance.
 *
 * Materials are intentionally configured per rigid component, never per face.
 */
UENUM(BlueprintType)
enum class ETGComponentSurfaceAppearanceMode : uint8
{
    SolidColor UMETA(DisplayName = "Solid Color"),
    Textured UMETA(DisplayName = "Textured")
};

/**
 * Unit assumed for coordinates stored in an imported STL file.
 */
UENUM(BlueprintType)
enum class ETGStlLengthUnit : uint8
{
    Millimeters UMETA(DisplayName = "Millimeters"),
    Centimeters UMETA(DisplayName = "Centimeters"),
    Meters UMETA(DisplayName = "Meters")
};

/**
 * Internal STL recentering setting.
 *
 * The Component Visual Appearance editor always uses KeepImportedOrigin so
 * the user's STL origin and orientation remain authoritative.
 */
UENUM(BlueprintType)
enum class ETGStlRecenterMode : uint8
{
    KeepImportedOrigin UMETA(DisplayName = "Keep Imported Origin"),
    CenterOnBounds UMETA(DisplayName = "Center on Bounds"),
    PlaceBaseAtOrigin UMETA(DisplayName = "Place Base at Origin")
};

/**
 * Persistent visualization-only configuration for one physical component.
 *
 * These values must never modify the component's physical mass, center of
 * mass, inertia, anchors, joint definition, or backend dynamics.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentVisualConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    ETGComponentGeometrySource GeometrySource =
        ETGComponentGeometrySource::Primitive;

    // ---------------------------------------------------------------------
    // Standard primitive
    // ---------------------------------------------------------------------

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Primitive")
    ETGPrimitiveGeometryType PrimitiveType =
        ETGPrimitiveGeometryType::Box;

    /**
     * Full X/Y/Z dimensions of a box or cuboid, in meters.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Primitive")
    FVector BoxDimensionsMeters = FVector(1.0, 1.0, 1.0);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Primitive")
    double SphereRadiusMeters = 0.5;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Primitive")
    double CylinderRadiusMeters = 0.5;

    /**
     * Full end-to-end cylinder length, in meters.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Primitive")
    double CylinderLengthMeters = 1.0;

    // ---------------------------------------------------------------------
    // Custom STL
    // ---------------------------------------------------------------------

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|STL")
    FString StlFilePath;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|STL")
    ETGStlLengthUnit StlLengthUnit =
        ETGStlLengthUnit::Millimeters;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|STL")
    ETGStlRecenterMode StlRecenterMode =
        ETGStlRecenterMode::KeepImportedOrigin;

    // ---------------------------------------------------------------------
    // Internal visual transform
    // ---------------------------------------------------------------------

    /**
     * Internal visual offset. The Component Visual Appearance editor
     * fixes this to zero and does not expose it to the user.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Transform")
    FVector VisualOffsetMeters = FVector::ZeroVector;

    /**
     * Internal visual orientation. The Component Visual Appearance
     * editor fixes this to identity and does not expose it to the user.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Transform")
    FQuat VisualOrientation = FQuat::Identity;

    /**
     * Internal visual scale. The Component Visual Appearance editor
     * fixes this to one and does not expose it to the user.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Transform")
    FVector VisualScale = FVector::OneVector;

    // ---------------------------------------------------------------------
    // Whole-component surface appearance
    // ---------------------------------------------------------------------

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display")
    ETGComponentSurfaceAppearanceMode SurfaceAppearanceMode =
        ETGComponentSurfaceAppearanceMode::SolidColor;

    /**
     * Whole-component color used when SurfaceAppearanceMode is SolidColor.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display")
    FLinearColor DisplayColor =
        FLinearColor(0.18f, 0.55f, 1.0f, 1.0f);

    /**
     * Tint multiplied into the base-color texture in Textured mode. White
     * leaves the source texture unchanged.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display")
    FLinearColor BaseColorTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    /**
     * Whole-component texture files. Base color is required in Textured mode;
     * the other PBR maps are optional. Per-face material assignment is outside
     * the Component Visual Appearance contract.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display|Textures")
    FString BaseColorTextureFilePath;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display|Textures")
    FString NormalTextureFilePath;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display|Textures")
    FString RoughnessTextureFilePath;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display|Textures")
    FString MetallicTextureFilePath;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Geometry|Display")
    bool bVisible = true;
};