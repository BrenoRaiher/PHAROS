// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TGSpacecraftPreviewPawn.generated.h"

class ATGCoordinateAxesActor;
class ATGSpacecraftVisualActor;
class UCameraComponent;
class USceneComponent;
class USpringArmComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGPreviewComponentSelectionChanged,
    FGuid,
    SelectedComponentId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGPreviewSrpFacetClicked,
    FGuid,
    ComponentId,
    int32,
    StableTriangleIndex);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGPreviewSrpPrimitiveSurfaceClicked,
    FGuid,
    ComponentId,
    FVector,
    PrimitiveLocalNormal);

/**
 * Camera and interaction pawn used by the spacecraft component-tree editor.
 *
 * Controls:
 *
 *   Left click        -> select component
 *   Right mouse drag  -> orbit
 *   Middle mouse drag -> pan
 *   Mouse wheel       -> zoom
 */
UCLASS(BlueprintType, Blueprintable)
class TG_API ATGSpacecraftPreviewPawn : public APawn
{
    GENERATED_BODY()

public:
    ATGSpacecraftPreviewPawn();

    virtual void Tick(float DeltaSeconds) override;

    /**
     * Broadcast whenever the selected physical component changes.
     *
     * An invalid GUID means that selection was cleared.
     */
    UPROPERTY(
        BlueprintAssignable,
        Category = "PHAROS|Spacecraft Preview|Selection")
    FTGPreviewComponentSelectionChanged
        OnPreviewComponentSelectionChanged;

    UPROPERTY(
        BlueprintAssignable,
        Category = "PHAROS|Spacecraft Preview|SRP")
    FTGPreviewSrpFacetClicked OnPreviewSrpFacetClicked;

    /** Frontend-only primitive logical-surface selection. This does not
     * generate or preprocess an SRP proxy. */
    UPROPERTY(
        BlueprintAssignable,
        Category = "PHAROS|Spacecraft Preview|SRP")
    FTGPreviewSrpPrimitiveSurfaceClicked
        OnPreviewSrpPrimitiveSurfaceClicked;

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview",
        meta = (DisplayName = "Set 3D Interaction Enabled"))
    void Set3DInteractionEnabled(bool bEnabled);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Preview",
        meta = (DisplayName = "Is 3D Interaction Enabled"))
    bool Is3DInteractionEnabled() const;

    /**
     * Assigns the spacecraft actor that can be selected and framed.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview|Selection",
        meta = (DisplayName = "Set Preview Spacecraft Actor"))
    void SetPreviewSpacecraftActor(
        ATGSpacecraftVisualActor* SpacecraftActor);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Preview|Selection")
    ATGSpacecraftVisualActor* GetPreviewSpacecraftActor() const;

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview|SRP")
    void SetSrpFacetSelectionEnabled(bool bEnabled);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Preview|SRP")
    bool IsSrpFacetSelectionEnabled() const;

    /**
     * Selects one component programmatically.
     *
     * The future UMG tree will call this when a tree entry is selected.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview|Selection",
        meta = (DisplayName = "Select Preview Component"))
    bool SelectPreviewComponent(
        FGuid ComponentId,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview|Selection",
        meta = (DisplayName = "Clear Preview Component Selection"))
    void ClearPreviewComponentSelection();

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Preview|Selection",
        meta = (DisplayName = "Get Selected Preview Component ID"))
    bool GetSelectedPreviewComponentId(
        FGuid& OutComponentId) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview",
        meta = (DisplayName = "Frame Spacecraft"))
    bool FrameSpacecraft(
        ATGSpacecraftVisualActor* SpacecraftActor);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview",
        meta = (DisplayName = "Frame World Bounds"))
    bool FrameWorldBounds(
        FVector WorldCenter,
        double SphereRadius);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview",
        meta = (DisplayName = "Reset Preview Camera"))
    void ResetPreviewCamera();

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview|Views",
        meta = (DisplayName = "View From Positive X"))
    void ViewFromPositiveX();

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview|Views",
        meta = (DisplayName = "View From Positive Y"))
    void ViewFromPositiveY();

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Spacecraft Preview|Views",
        meta = (DisplayName = "View From Positive Z"))
    void ViewFromPositiveZ();

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Spacecraft Preview")
    UCameraComponent* GetPreviewCameraComponent() const;

protected:
    virtual void BeginPlay() override;

    virtual void EndPlay(
        const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Preview")
    TObjectPtr<USceneComponent> PreviewRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Preview")
    TObjectPtr<USceneComponent> OrbitPivot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Preview")
    TObjectPtr<USpringArmComponent> CameraArm;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Spacecraft Preview")
    TObjectPtr<UCameraComponent> PreviewCamera;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Orbit")
    double OrbitDegreesPerMouseUnit = 0.65;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Orbit")
    double MinimumPitchDegrees = -89.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Orbit")
    double MaximumPitchDegrees = 89.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Pan")
    double PanFractionPerMouseUnit = 0.0045;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Zoom")
    double ZoomExponentPerWheelStep = 0.18;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Zoom")
    double MinimumCameraDistanceCentimeters = 10.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Zoom")
    double MaximumCameraDistanceCentimeters = 100000000.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Framing")
    double FramePadding = 1.25;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Defaults")
    double DefaultCameraDistanceCentimeters = 500.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Defaults")
    double DefaultYawDegrees = -45.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Defaults")
    double DefaultPitchDegrees = -25.0;

    /**
     * Length of the local axes displayed on the selected component.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Selection",
        meta = (ClampMin = "1.0"))
    double SelectionAxesLengthCentimeters = 120.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Spacecraft Preview|Selection")
    bool bClearSelectionWhenClickingEmptySpace = true;

private:
    UPROPERTY(Transient)
    TObjectPtr<ATGSpacecraftVisualActor>
        PreviewSpacecraftActor;

    UPROPERTY(Transient)
    TObjectPtr<ATGCoordinateAxesActor>
        SelectionAxesActor;

    FGuid SelectedComponentId;

    bool b3DInteractionEnabled = true;
    bool bSrpFacetSelectionEnabled = false;

    double CurrentYawDegrees = -45.0;
    double CurrentPitchDegrees = -25.0;

    void ProcessCameraInput();

    void OrbitByMouseDelta(
        double DeltaX,
        double DeltaY);

    void PanByMouseDelta(
        double DeltaX,
        double DeltaY);

    void ZoomByWheelDelta(
        double WheelDelta);

    void ApplyOrbitRotation();

    void SetViewRotation(
        double NewYawDegrees,
        double NewPitchDegrees);

    bool EnsureSelectionAxesActor();

    void TrySelectComponentUnderCursor();

    void RefreshSelectionIndicatorTransform();
};
