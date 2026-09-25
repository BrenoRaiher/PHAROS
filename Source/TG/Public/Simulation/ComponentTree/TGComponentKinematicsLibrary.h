// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGComponentKinematicsLibrary.generated.h"

/**
 * Pose of one physical component relative to spacecraft body frame B.
 *
 * OriginInBodyMeters is s^B_BC.
 * ComponentToBodyOrientation is the active rotation R_BC.
 *
 * The FQuat fields store the quaternion numerically as Unreal X/Y/Z/W fields,
 * but the quaternion represented here still follows the backend's
 * right-handed active-rotation convention.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentKinematicPose
{
    GENERATED_BODY()

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Component Pose")
    FGuid ComponentId;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Component Pose")
    FString ComponentName;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Component Pose")
    FVector OriginInBodyMeters = FVector::ZeroVector;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Component Pose")
    FQuat ComponentToBodyOrientation = FQuat::Identity;
};

/**
 * Exact component-tree kinematics shared by the configuration preview and
 * legacy playback fallback.
 *
 * Physics is not implemented here. This library only evaluates the
 * authoritative rigid transform chain defined by the HUD/backend contract.
 */
UCLASS()
class TG_API UTGComponentKinematicsLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Evaluates every component pose relative to body frame B.
     *
     * Flattening order:
     *   component array order;
     *   then DegreesOfFreedom array order.
     *
     * When RequestedArticulationCoordinates is empty, each DOF's
     * InitialCoordinate is used.
     *
     * When it is not empty, its length must exactly match the total number of
     * DOFs. Requested coordinates are clamped to configured coordinate limits,
     * matching the backend preview policy.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Kinematics",
        meta = (DisplayName = "Evaluate Component Tree Kinematics"))
    static bool EvaluateComponentTreeKinematics(
        const FTGSimulationScenario& Scenario,
        const TArray<double>& RequestedArticulationCoordinates,
        TArray<FTGComponentKinematicPose>& OutComponentPoses,
        TArray<double>& OutAcceptedArticulationCoordinates,
        FText& OutErrorText);

    /**
     * Returns the flattened articulation-coordinate count.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Kinematics",
        meta = (DisplayName =
            "Get Component Tree Articulation Coordinate Count"))
    static int32 GetArticulationCoordinateCount(
        const FTGSimulationScenario& Scenario);

    /**
     * Converts a backend position in meters to Unreal centimeters.
     *
     * p_UE_cm = 100 * (x, -y, z).
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Conversion",
        meta = (DisplayName =
            "Convert Backend Position To Unreal"))
    static FVector ConvertBackendPositionMetersToUnrealCentimeters(
        FVector BackendPositionMeters);

    /**
     * Converts a backend direction using C = diag(1, -1, 1).
     *
     * No length-unit scaling is applied.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Conversion",
        meta = (DisplayName =
            "Convert Backend Direction To Unreal"))
    static FVector ConvertBackendDirectionToUnreal(
        FVector BackendDirection);

    /**
     * Converts an active backend orientation using:
     *
     * R_UE = C * R_backend * C
     *
     * For C = diag(1, -1, 1), the equivalent normalized quaternion mapping is:
     *
     * (x, y, z, w)_UE = (-x, y, -z, w)_backend
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Conversion",
        meta = (DisplayName =
            "Convert Backend Orientation To Unreal"))
    static FQuat ConvertBackendOrientationToUnreal(
        FQuat BackendOrientation);

    /**
     * Converts one body-relative backend component pose into an Unreal
     * transform, assuming body frame B is located at the Unreal world origin.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Conversion",
        meta = (DisplayName =
            "Convert Component Pose To Unreal Transform"))
    static FTransform ConvertComponentPoseToUnrealTransform(
        const FTGComponentKinematicPose& BackendPose);
};