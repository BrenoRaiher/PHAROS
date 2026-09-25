// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TGSpacecraftOrbitCameraActor.generated.h"

class ATGSimulationPlaybackActor;
class UCameraComponent;
class USceneComponent;

/**
 * Runtime camera for the spacecraft-centered result view.
 *
 * Right-drag orbits continuously around the target and the mouse wheel changes
 * distance. Its view direction is rebuilt every frame, so the spacecraft
 * target cannot drift away from the center of the viewport.
 */
UCLASS(NotBlueprintable)
class TG_API ATGSpacecraftOrbitCameraActor final : public AActor
{
    GENERATED_BODY()

public:
    ATGSpacecraftOrbitCameraActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void InitializeForPlayback(
        ATGSimulationPlaybackActor* InPlayback,
        AActor* InTarget,
        double InitialDistanceCentimeters);

    void SetTargetActor(AActor* InTarget);

    double GetOrbitDistanceCentimeters() const;

    void SetFieldOfViewDegrees(double FieldOfViewDegrees);
    double GetFieldOfViewDegrees() const;

    void SetOrbitSensitivityDegreesPerPixel(
        double SensitivityDegreesPerPixel);
    double GetOrbitSensitivityDegreesPerPixel() const;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USceneComponent> CameraRoot;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UCameraComponent> Camera;

    UPROPERTY(Transient)
    TObjectPtr<ATGSimulationPlaybackActor> PlaybackActor;

    UPROPERTY(Transient)
    TObjectPtr<AActor> TargetActor;

    double OrbitDistanceCentimeters = 600.0;
    FQuat OrbitRotation = FQuat(FRotator(18.0, 35.0, 0.0));
    double MinimumDistanceCentimeters = 10.0;
    double MaximumDistanceCentimeters = 12000.0;
    double OrbitDegreesPerMousePixel = 1.0;
    double ZoomStepFactor = 0.82;
    bool bRightMouseDragBlockedByUi = false;

    void UpdateCameraTransform();
};
