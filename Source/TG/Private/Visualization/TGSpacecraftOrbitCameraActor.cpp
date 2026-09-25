// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Visualization/TGSpacecraftOrbitCameraActor.h"

#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Visualization/TGSimulationPlaybackActor.h"

ATGSpacecraftOrbitCameraActor::ATGSpacecraftOrbitCameraActor()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;

    CameraRoot = CreateDefaultSubobject<USceneComponent>(TEXT("CameraRoot"));
    SetRootComponent(CameraRoot);

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(CameraRoot);
    Camera->SetFieldOfView(55.0f);
}

void ATGSpacecraftOrbitCameraActor::BeginPlay()
{
    Super::BeginPlay();

    if (APlayerController* PlayerController =
            GetWorld() != nullptr
                ? GetWorld()->GetFirstPlayerController()
                : nullptr)
    {
        PlayerController->SetViewTarget(this);
    }
    UpdateCameraTransform();
}

void ATGSpacecraftOrbitCameraActor::InitializeForPlayback(
    ATGSimulationPlaybackActor* InPlayback,
    AActor* InTarget,
    const double InitialDistanceCentimeters)
{
    PlaybackActor = InPlayback;
    TargetActor = InTarget;
    OrbitDistanceCentimeters = FMath::Clamp(
        InitialDistanceCentimeters,
        MinimumDistanceCentimeters,
        MaximumDistanceCentimeters);

    if (APlayerController* PlayerController =
            GetWorld() != nullptr
                ? GetWorld()->GetFirstPlayerController()
                : nullptr)
    {
        PlayerController->SetViewTarget(this);
    }
    UpdateCameraTransform();
}

void ATGSpacecraftOrbitCameraActor::SetTargetActor(AActor* InTarget)
{
    TargetActor = InTarget;
    UpdateCameraTransform();
}

double ATGSpacecraftOrbitCameraActor::GetOrbitDistanceCentimeters() const
{
    return OrbitDistanceCentimeters;
}

void ATGSpacecraftOrbitCameraActor::SetFieldOfViewDegrees(
    const double FieldOfViewDegrees)
{
    if (Camera != nullptr)
    {
        Camera->SetFieldOfView(static_cast<float>(FMath::Clamp(
            FieldOfViewDegrees,
            20.0,
            120.0)));
    }
}

double ATGSpacecraftOrbitCameraActor::GetFieldOfViewDegrees() const
{
    return Camera != nullptr
        ? static_cast<double>(Camera->FieldOfView)
        : 55.0;
}

void ATGSpacecraftOrbitCameraActor::SetOrbitSensitivityDegreesPerPixel(
    const double SensitivityDegreesPerPixel)
{
    OrbitDegreesPerMousePixel = FMath::Clamp(
        SensitivityDegreesPerPixel,
        0.05,
        2.0);
}

double ATGSpacecraftOrbitCameraActor::
    GetOrbitSensitivityDegreesPerPixel() const
{
    return OrbitDegreesPerMousePixel;
}

void ATGSpacecraftOrbitCameraActor::Tick(const float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APlayerController* PlayerController =
        GetWorld() != nullptr
            ? GetWorld()->GetFirstPlayerController()
            : nullptr;

    const bool bMenuOpen =
        PlaybackActor != nullptr &&
        PlaybackActor->IsVisualizationMenuOpen();
    const bool bHudConsumesCameraInput =
        PlaybackActor != nullptr &&
        PlaybackActor->IsVisualizationUiCapturingCameraInput();
    const bool bRightMouseButtonDown =
        PlayerController != nullptr &&
        PlayerController->IsInputKeyDown(EKeys::RightMouseButton);

    // Once a right-button drag touches the HUD, keep it owned by the HUD until
    // release. Viewport mouse capture must not restart camera orbit mid-drag.
    if (!bRightMouseButtonDown)
    {
        bRightMouseDragBlockedByUi = false;
    }
    else if (bMenuOpen || bHudConsumesCameraInput)
    {
        bRightMouseDragBlockedByUi = true;
    }

    bool bOrbitDistanceChanged = false;
    if (
        PlayerController != nullptr &&
        !bMenuOpen &&
        !bHudConsumesCameraInput)
    {
        if (bRightMouseButtonDown && !bRightMouseDragBlockedByUi)
        {
            float MouseDeltaX = 0.0f;
            float MouseDeltaY = 0.0f;
            PlayerController->GetInputMouseDelta(MouseDeltaX, MouseDeltaY);

            const FQuat BaseCameraRotation(FRotator(0.0, 180.0, 0.0));
            const FQuat CameraRotation =
                (OrbitRotation * BaseCameraRotation).GetNormalized();
            const FVector CameraUp =
                CameraRotation.RotateVector(FVector::ZAxisVector);
            const FVector CameraRight =
                CameraRotation.RotateVector(FVector::YAxisVector);

            const FQuat HorizontalOrbit(
                CameraUp,
                FMath::DegreesToRadians(
                    -static_cast<double>(MouseDeltaX) *
                    OrbitDegreesPerMousePixel));
            const FQuat VerticalOrbit(
                HorizontalOrbit.RotateVector(CameraRight),
                FMath::DegreesToRadians(
                    static_cast<double>(MouseDeltaY) *
                    OrbitDegreesPerMousePixel));
            OrbitRotation =
                (VerticalOrbit * HorizontalOrbit * OrbitRotation)
                    .GetNormalized();
        }

        if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollUp))
        {
            OrbitDistanceCentimeters = FMath::Max(
                MinimumDistanceCentimeters,
                OrbitDistanceCentimeters * ZoomStepFactor);
            bOrbitDistanceChanged = true;
        }
        if (PlayerController->WasInputKeyJustPressed(EKeys::MouseScrollDown))
        {
            OrbitDistanceCentimeters = FMath::Min(
                MaximumDistanceCentimeters,
                OrbitDistanceCentimeters / ZoomStepFactor);
            bOrbitDistanceChanged = true;
        }
    }

    UpdateCameraTransform();
    if (bOrbitDistanceChanged && PlaybackActor != nullptr)
    {
        FText IgnoredError;
        PlaybackActor->RefreshCurrentFrame(IgnoredError);
    }
}

void ATGSpacecraftOrbitCameraActor::UpdateCameraTransform()
{
    const FVector TargetLocation =
        IsValid(TargetActor)
            ? TargetActor->GetActorLocation()
            : FVector::ZeroVector;

    const FVector DirectionFromTarget =
        OrbitRotation.RotateVector(FVector::XAxisVector).GetSafeNormal();

    const FVector CameraLocation =
        TargetLocation + DirectionFromTarget * OrbitDistanceCentimeters;
    const FQuat BaseCameraRotation(FRotator(0.0, 180.0, 0.0));
    const FQuat CameraRotation =
        (OrbitRotation * BaseCameraRotation).GetNormalized();
    SetActorLocationAndRotation(
        CameraLocation,
        CameraRotation);
}
