// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "Simulation/ComponentTree/TGComponentKinematicsLibrary.h"
#include "TGSpacecraftVisualActor.generated.h"

class UActorComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UMeshComponent;
class UPrimitiveComponent;
class UProceduralMeshComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTexture2D;

/**
 * Reusable visual representation of one configured spacecraft.
 *
 * The actor is used by:
 *
 * 1. the component-tree configuration preview;
 * 2. simulation-result playback.
 *
 * It contains no dynamics. It only creates visual components and applies
 * poses calculated by either the shared HUD kinematics evaluator or the
 * backend result.
 */
UCLASS(BlueprintType, Blueprintable)
class TG_API ATGSpacecraftVisualActor : public AActor
{
    GENERATED_BODY()

public:
    ATGSpacecraftVisualActor();

    /**
     * Clears the previous visual spacecraft, copies the supplied scenario,
     * creates all component-frame and visual-mesh nodes, applies the
     * configured whole-component surface appearance, and applies the
     * configured initial joint coordinates.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Build Spacecraft From Scenario"))
    bool BuildFromScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText);

    /**
     * Replaces the actor's persistent scenario snapshot and reapplies the
     * configured initial kinematic pose without rebuilding visual geometry.
     *
     * This is intended for inspector edits that change only kinematic fields
     * while preserving the component identifiers and generated visual topology.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Refresh Spacecraft Kinematics From Scenario"))
    bool RefreshKinematicsFromScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText);

    /**
     * Rebuilds each component's visual mesh under its existing physical frame,
     * supporting both standard primitives and runtime STL geometry, while
     * preserving the current component-frame poses and viewport framing.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Refresh Spacecraft Visual Appearance From Scenario"))
    bool RefreshVisualAppearanceFromScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText);

    /**
     * Updates only one component's generated-mesh visibility without
     * rebuilding geometry or materials.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Set Component Visual Visibility"))
    bool SetComponentVisualVisibility(
        FGuid ComponentId,
        bool bVisible,
        FText& OutErrorText);

    /**
     * Evaluates and applies temporary preview joint coordinates.
     *
     * Coordinates use the authoritative flattened order:
     * component order, then local DOF order.
     *
     * Values are clamped to their configured coordinate limits.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Apply Joint Coordinates"))
    bool ApplyJointCoordinates(
        const TArray<double>& RequestedCoordinates,
        TArray<double>& OutAcceptedCoordinates,
        FText& OutErrorText);

    /**
     * Restores the spacecraft to the saved initial DOF coordinates.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Apply Initial Joint Coordinates"))
    bool ApplyInitialJointCoordinates(
        TArray<double>& OutAcceptedCoordinates,
        FText& OutErrorText);

    /**
     * Applies already evaluated body-relative component poses.
     *
     * This is the common lower-level path used by preview and later by
     * backend result playback.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Apply Component Poses"))
    bool ApplyComponentPoses(
        const TArray<FTGComponentKinematicPose>& ComponentPoses,
        FText& OutErrorText);

    /**
     * Removes every dynamically generated component frame and mesh.
     * The permanent body-frame root remains.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Clear Spacecraft Visuals"))
    void ClearSpacecraftVisuals();

    /**
     * Resolves a clicked primitive component back to its physical component.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Find Component ID From Primitive"))
    bool FindComponentIdFromPrimitive(
        UPrimitiveComponent* PrimitiveComponent,
        FGuid& OutComponentId) const;

    /** Builds invisible hit-test meshes for the current generated SRP proxy. */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization|SRP")
    bool ShowSrpProxyPreview(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization|SRP")
    void ClearSrpProxyPreview();

    /** Enables the invisible proxy colliders only while the SRP page is the
     * active 3D editor, so other configuration pages keep normal picking. */
    void SetSrpProxyHitTestingEnabled(bool bEnabled);

    /** Resolves a proxy hit and face index to its stable authoring identity. */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Visualization|SRP")
    bool FindSrpFacetFromPrimitive(
        UPrimitiveComponent* PrimitiveComponent,
        int32 HitFaceIndex,
        FGuid& OutComponentId,
        int32& OutStableTriangleIndex) const;

    /** Paints the currently selected proxy faces over the normal spacecraft. */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization|SRP")
    bool SetSrpFacetSelectionHighlight(
        FGuid ComponentId,
        const TArray<int32>& StableTriangleIndices,
        FText& OutErrorText);

    /** Frontend-only visualization of selected primitive logical regions.
     *  It never creates or persists converter proxy triangles. */
    bool SetSrpPrimitiveRegionSelectionHighlight(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const TArray<ETGSrpLogicalRegion>& Regions,
        FText& OutErrorText);

    /**
     * Enables Custom Depth/Stencil rendering on every generated primitive
     * belonging to one physical component.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization|Selection",
        meta = (DisplayName = "Set Component Highlight"))
    bool SetComponentHighlight(
        FGuid ComponentId,
        int32 StencilValue,
        FText& OutErrorText);

    /**
     * Disables Custom Depth/Stencil rendering on all generated component
     * primitives.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Visualization|Selection",
        meta = (DisplayName = "Clear Component Highlight"))
    void ClearComponentHighlight();

    /**
     * Returns the world-space bounds of all visible spacecraft meshes.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Get Spacecraft Bounds"))
    bool GetSpacecraftBounds(
        FVector& OutWorldCenter,
        FVector& OutWorldExtent,
        double& OutSphereRadius) const;

    /**
     * Returns the current world-space transform of one generated physical
     * component frame.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Visualization",
        meta = (DisplayName = "Get Component World Transform By ID"))
    bool GetComponentWorldTransformById(
        FGuid ComponentId,
        FTransform& OutWorldTransform) const;

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Visualization")
    int32 GetGeneratedPhysicalComponentCount() const;

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Visualization")
    bool HasBuiltSpacecraft() const;

protected:
    /**
     * Actor-local origin representing spacecraft body frame B.
     */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Visualization")
    TObjectPtr<USceneComponent> BodyFrameRoot;

    /**
     * Standard Engine primitive meshes.
     *
     * They remain editable on a Blueprint subclass in case the visual style
     * is later replaced.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Visualization|Primitive Meshes")
    TObjectPtr<UStaticMesh> BoxPrimitiveMesh;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Visualization|Primitive Meshes")
    TObjectPtr<UStaticMesh> SpherePrimitiveMesh;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Visualization|Primitive Meshes")
    TObjectPtr<UStaticMesh> CylinderPrimitiveMesh;

    /**
     * Reusable master material for all rigid-component surfaces.
     *
     * Every generated component receives its own dynamic material instance.
     * The default asset path loaded by the constructor is:
     * /Game/UI/Configuration/ComponentTree/Materials/M_TGComponentSurface
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Visualization|Materials")
    TObjectPtr<UMaterialInterface> ComponentSurfaceMaterial;

    /**
     * Translucent counterpart used only when the active surface color has
     * alpha below one.
     *
     * The default asset path loaded by the constructor is:
     * /Game/UI/Configuration/ComponentTree/Materials/
     * M_TGComponentSurface_Translucent
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Visualization|Materials")
    TObjectPtr<UMaterialInterface> ComponentTranslucentSurfaceMaterial;

private:
    enum class EGeneratedTextureUsage : uint8
    {
        BaseColor,
        Normal,
        Mask
    };

    /**
     * Current persistent scenario definition used for preview evaluation.
     */
    UPROPERTY(Transient)
    FTGSimulationScenario ScenarioSnapshot;

    /**
     * Keeps all dynamically generated actor components referenced until they
     * are explicitly destroyed.
     */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UActorComponent>> GeneratedActorComponents;

    /**
     * Keeps transient MIDs and disk-loaded textures alive for the lifetime of
     * the current generated visual spacecraft.
     */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>>
        GeneratedMaterialInstances;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>>
        GeneratedRuntimeTextures;

    TMap<FGuid, USceneComponent*> ComponentFramesById;
    TMap<FGuid, USceneComponent*> ComponentGeometryRootsById;
    TMap<FGuid, UMeshComponent*> ComponentMeshesById;
    TMap<const UPrimitiveComponent*, FGuid> ComponentIdsByPrimitive;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UActorComponent>> SrpPreviewActorComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInstanceDynamic>> SrpPreviewMaterials;

    TMap<const UPrimitiveComponent*, FGuid> SrpComponentIdsByPrimitive;
    TMap<FGuid, TArray<int32>> SrpStableIndicesByComponent;
    TMap<FGuid, TArray<FTGOpticalFacetConfig>> SrpFacetsByComponent;
    TMap<FGuid, UProceduralMeshComponent*> SrpSelectionMeshesByComponent;

    FGuid HighlightedComponentId;
    int32 HighlightStencilValue = 0;

    bool bHasBuiltSpacecraft = false;

    USceneComponent* CreateGeneratedSceneComponent(
        const FString& BaseName,
        USceneComponent* AttachParent);

    UStaticMeshComponent* CreateGeneratedStaticMeshComponent(
        const FString& BaseName,
        USceneComponent* AttachParent);

    UProceduralMeshComponent* CreateGeneratedProceduralMeshComponent(
        const FString& BaseName,
        USceneComponent* AttachParent);

    bool CreateConfiguredVisualMesh(
        const FTGComponentConfig& Component,
        int32 ComponentIndex,
        USceneComponent* GeometryRoot,
        UMeshComponent*& OutMeshComponent,
        FText& OutErrorText);

    UStaticMesh* ResolvePrimitiveMesh(
        ETGPrimitiveGeometryType PrimitiveType) const;

    FVector CalculatePrimitiveMeshScale(
        const FTGComponentVisualConfig& VisualConfig) const;

    bool ConfigurePrimitiveMesh(
        UStaticMeshComponent* MeshComponent,
        const FTGComponentConfig& Component,
        FText& OutErrorText);

    bool ConfigureStlMesh(
        UProceduralMeshComponent* MeshComponent,
        const FTGComponentConfig& Component,
        FText& OutErrorText);

    bool ApplyWholeComponentSurfaceAppearance(
        UMeshComponent* MeshComponent,
        const FTGComponentConfig& Component,
        FText& OutErrorText);

    UTexture2D* LoadRuntimeTexture(
        const FString& FilePath,
        EGeneratedTextureUsage Usage,
        FText& OutErrorText);

    bool ValidateVisualRefreshTopology(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText) const;

    bool ValidateComponentIdentifiers(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText) const;
};
