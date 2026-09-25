// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Visualization/TGSpacecraftPreviewPawn.h"

#include "Camera/CameraComponent.h"
#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "Visualization/TGCoordinateAxesActor.h"
#include "Visualization/TGSpacecraftVisualActor.h"

ATGSpacecraftPreviewPawn::ATGSpacecraftPreviewPawn()
{
    PrimaryActorTick.bCanEverTick = true;

    AutoPossessPlayer = EAutoReceiveInput::Player0;

    PreviewRoot =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("PreviewRoot"));

    SetRootComponent(PreviewRoot);

    OrbitPivot =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("OrbitPivot"));

    OrbitPivot->SetupAttachment(PreviewRoot);

    CameraArm =
        CreateDefaultSubobject<USpringArmComponent>(
            TEXT("CameraArm"));

    CameraArm->SetupAttachment(OrbitPivot);

    CameraArm->TargetArmLength =
        DefaultCameraDistanceCentimeters;

    CameraArm->bDoCollisionTest = false;
    CameraArm->bUsePawnControlRotation = false;

    CameraArm->bInheritPitch = true;
    CameraArm->bInheritYaw = true;
    CameraArm->bInheritRoll = false;

    PreviewCamera =
        CreateDefaultSubobject<UCameraComponent>(
            TEXT("PreviewCamera"));

    PreviewCamera->SetupAttachment(
        CameraArm,
        USpringArmComponent::SocketName);

    PreviewCamera->bUsePawnControlRotation = false;
}

void ATGSpacecraftPreviewPawn::BeginPlay()
{
    Super::BeginPlay();

    CurrentYawDegrees = DefaultYawDegrees;
    CurrentPitchDegrees = DefaultPitchDegrees;

    CameraArm->TargetArmLength =
        FMath::Clamp(
            DefaultCameraDistanceCentimeters,
            MinimumCameraDistanceCentimeters,
            MaximumCameraDistanceCentimeters);

    ApplyOrbitRotation();
    EnsureSelectionAxesActor();

    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (PlayerController != nullptr)
    {
        PlayerController->bShowMouseCursor = true;
        PlayerController->bEnableClickEvents = true;
        PlayerController->bEnableMouseOverEvents = true;
    }
}

void ATGSpacecraftPreviewPawn::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    if (IsValid(SelectionAxesActor))
    {
        SelectionAxesActor->Destroy();
        SelectionAxesActor = nullptr;
    }

    Super::EndPlay(EndPlayReason);
}

void ATGSpacecraftPreviewPawn::Tick(
    float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (b3DInteractionEnabled)
    {
        ProcessCameraInput();
    }

    if (IsValid(PreviewSpacecraftActor))
    {
        PreviewSpacecraftActor->SetSrpProxyHitTestingEnabled(
            IsSrpFacetSelectionEnabled());
    }

    RefreshSelectionIndicatorTransform();
}

void ATGSpacecraftPreviewPawn::
    Set3DInteractionEnabled(bool bEnabled)
{
    b3DInteractionEnabled = bEnabled;
}

bool ATGSpacecraftPreviewPawn::
    Is3DInteractionEnabled() const
{
    return b3DInteractionEnabled;
}

void ATGSpacecraftPreviewPawn::
    SetPreviewSpacecraftActor(
        ATGSpacecraftVisualActor* SpacecraftActor)
{
    if (PreviewSpacecraftActor == SpacecraftActor)
    {
        return;
    }

    ClearPreviewComponentSelection();
    PreviewSpacecraftActor = SpacecraftActor;
}

ATGSpacecraftVisualActor*
ATGSpacecraftPreviewPawn::GetPreviewSpacecraftActor() const
{
    return PreviewSpacecraftActor;
}

void ATGSpacecraftPreviewPawn::SetSrpFacetSelectionEnabled(bool bEnabled)
{
    bSrpFacetSelectionEnabled = bEnabled;
    if (!bEnabled && IsValid(PreviewSpacecraftActor))
    {
        PreviewSpacecraftActor->SetSrpProxyHitTestingEnabled(false);
    }
}

bool ATGSpacecraftPreviewPawn::IsSrpFacetSelectionEnabled() const
{
    return bSrpFacetSelectionEnabled && b3DInteractionEnabled;
}

bool ATGSpacecraftPreviewPawn::
    SelectPreviewComponent(
        FGuid ComponentId,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (!IsValid(PreviewSpacecraftActor))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "No spacecraft actor is assigned to "
                "the preview camera pawn."));

        return false;
    }

    if (!ComponentId.IsValid())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The requested component identifier "
                "is invalid."));

        return false;
    }

    FTransform ComponentWorldTransform;

    if (!PreviewSpacecraftActor->
            GetComponentWorldTransformById(
                ComponentId,
                ComponentWorldTransform))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected component does not have "
                "a generated visual frame."));

        return false;
    }

    if (!EnsureSelectionAxesActor())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The component selection indicator "
                "could not be created."));

        return false;
    }

    FText HighlightError;

    if (!PreviewSpacecraftActor->
            SetComponentHighlight(
                ComponentId,
                1,
                HighlightError))
    {
        OutErrorText = HighlightError;
        return false;
    }

    SelectedComponentId = ComponentId;

    SelectionAxesActor->SetActorTransform(
        ComponentWorldTransform);

    SelectionAxesActor->SetActorHiddenInGame(false);

    OnPreviewComponentSelectionChanged.Broadcast(
        SelectedComponentId);

    return true;
}

void ATGSpacecraftPreviewPawn::
    ClearPreviewComponentSelection()
{
    const bool bHadSelection =
        SelectedComponentId.IsValid();

    if (IsValid(PreviewSpacecraftActor))
    {
        PreviewSpacecraftActor->
            ClearComponentHighlight();
    }

    SelectedComponentId.Invalidate();

    if (IsValid(SelectionAxesActor))
    {
        SelectionAxesActor->
            SetActorHiddenInGame(true);
    }

    if (bHadSelection)
    {
        OnPreviewComponentSelectionChanged.Broadcast(
            FGuid());
    }
}

bool ATGSpacecraftPreviewPawn::
    GetSelectedPreviewComponentId(
        FGuid& OutComponentId) const
{
    OutComponentId.Invalidate();

    if (!SelectedComponentId.IsValid())
    {
        return false;
    }

    OutComponentId = SelectedComponentId;
    return true;
}

bool ATGSpacecraftPreviewPawn::FrameSpacecraft(
    ATGSpacecraftVisualActor* SpacecraftActor)
{
    if (
        SpacecraftActor == nullptr ||
        !SpacecraftActor->HasBuiltSpacecraft())
    {
        return false;
    }

    FVector WorldCenter;
    FVector WorldExtent;
    double SphereRadius = 0.0;

    if (!SpacecraftActor->GetSpacecraftBounds(
            WorldCenter,
            WorldExtent,
            SphereRadius))
    {
        return false;
    }

    return FrameWorldBounds(
        WorldCenter,
        SphereRadius);
}

bool ATGSpacecraftPreviewPawn::FrameWorldBounds(
    FVector WorldCenter,
    double SphereRadius)
{
    if (
        !FMath::IsFinite(SphereRadius) ||
        SphereRadius <= 0.0)
    {
        return false;
    }

    SetActorLocation(WorldCenter);

    const double HalfFieldOfViewRadians =
        FMath::DegreesToRadians(
            0.5 *
            static_cast<double>(
                PreviewCamera->FieldOfView));

    const double HalfFieldOfViewSine =
        FMath::Sin(HalfFieldOfViewRadians);

    if (HalfFieldOfViewSine <= UE_DOUBLE_SMALL_NUMBER)
    {
        return false;
    }

    const double RequiredDistance =
        FramePadding *
        SphereRadius /
        HalfFieldOfViewSine;

    CameraArm->TargetArmLength =
        FMath::Clamp(
            RequiredDistance,
            MinimumCameraDistanceCentimeters,
            MaximumCameraDistanceCentimeters);

    return true;
}

void ATGSpacecraftPreviewPawn::ResetPreviewCamera()
{
    SetActorLocation(FVector::ZeroVector);

    CurrentYawDegrees = DefaultYawDegrees;
    CurrentPitchDegrees = DefaultPitchDegrees;

    CameraArm->TargetArmLength =
        FMath::Clamp(
            DefaultCameraDistanceCentimeters,
            MinimumCameraDistanceCentimeters,
            MaximumCameraDistanceCentimeters);

    ApplyOrbitRotation();
}

void ATGSpacecraftPreviewPawn::ViewFromPositiveX()
{
    SetViewRotation(
        180.0,
        0.0);
}

void ATGSpacecraftPreviewPawn::ViewFromPositiveY()
{
    SetViewRotation(
        90.0,
        0.0);
}

void ATGSpacecraftPreviewPawn::ViewFromPositiveZ()
{
    SetViewRotation(
        0.0,
        -90.0);
}

UCameraComponent*
ATGSpacecraftPreviewPawn::
    GetPreviewCameraComponent() const
{
    return PreviewCamera;
}

void ATGSpacecraftPreviewPawn::ProcessCameraInput()
{
    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (PlayerController == nullptr)
    {
        return;
    }

    if (PlayerController->WasInputKeyJustPressed(
            EKeys::LeftMouseButton))
    {
        TrySelectComponentUnderCursor();
    }

    float MouseDeltaX = 0.0f;
    float MouseDeltaY = 0.0f;

    PlayerController->GetInputMouseDelta(
        MouseDeltaX,
        MouseDeltaY);

    if (
        PlayerController->IsInputKeyDown(
            EKeys::RightMouseButton))
    {
        OrbitByMouseDelta(
            static_cast<double>(MouseDeltaX),
            static_cast<double>(MouseDeltaY));
    }
    else if (
        PlayerController->IsInputKeyDown(
            EKeys::MiddleMouseButton))
    {
        PanByMouseDelta(
            static_cast<double>(MouseDeltaX),
            static_cast<double>(MouseDeltaY));
    }

    const double WheelDelta =
        static_cast<double>(
            PlayerController->GetInputAnalogKeyState(
                EKeys::MouseWheelAxis));

    if (!FMath::IsNearlyZero(WheelDelta))
    {
        ZoomByWheelDelta(WheelDelta);
    }
}

void ATGSpacecraftPreviewPawn::OrbitByMouseDelta(
    double DeltaX,
    double DeltaY)
{
    CurrentYawDegrees +=
        DeltaX *
        OrbitDegreesPerMouseUnit;

    CurrentPitchDegrees =
        FMath::Clamp(
            CurrentPitchDegrees +
                DeltaY *
                OrbitDegreesPerMouseUnit,
            MinimumPitchDegrees,
            MaximumPitchDegrees);

    ApplyOrbitRotation();
}

void ATGSpacecraftPreviewPawn::PanByMouseDelta(
    double DeltaX,
    double DeltaY)
{
    const double WorldUnitsPerMouseUnit =
        CameraArm->TargetArmLength *
        PanFractionPerMouseUnit;

    const FVector PanDelta =
        (
            -PreviewCamera->GetRightVector() *
                DeltaX -
            PreviewCamera->GetUpVector() *
                DeltaY
        ) *
        WorldUnitsPerMouseUnit;

    AddActorWorldOffset(
        PanDelta,
        false);
}

void ATGSpacecraftPreviewPawn::ZoomByWheelDelta(
    double WheelDelta)
{
    const double ZoomMultiplier =
        FMath::Exp(
            -WheelDelta *
            ZoomExponentPerWheelStep);

    CameraArm->TargetArmLength =
        FMath::Clamp(
            CameraArm->TargetArmLength *
                ZoomMultiplier,
            MinimumCameraDistanceCentimeters,
            MaximumCameraDistanceCentimeters);
}

void ATGSpacecraftPreviewPawn::ApplyOrbitRotation()
{
    OrbitPivot->SetRelativeRotation(
        FRotator(
            CurrentPitchDegrees,
            CurrentYawDegrees,
            0.0));
}

void ATGSpacecraftPreviewPawn::SetViewRotation(
    double NewYawDegrees,
    double NewPitchDegrees)
{
    CurrentYawDegrees = NewYawDegrees;

    CurrentPitchDegrees =
        FMath::Clamp(
            NewPitchDegrees,
            MinimumPitchDegrees,
            MaximumPitchDegrees);

    ApplyOrbitRotation();
}

bool ATGSpacecraftPreviewPawn::
    EnsureSelectionAxesActor()
{
    if (IsValid(SelectionAxesActor))
    {
        return true;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    FActorSpawnParameters SpawnParameters;

    SpawnParameters.Owner = this;

    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    SelectionAxesActor =
        World->SpawnActor<ATGCoordinateAxesActor>(
            ATGCoordinateAxesActor::StaticClass(),
            FTransform::Identity,
            SpawnParameters);

    if (!IsValid(SelectionAxesActor))
    {
        return false;
    }

    SelectionAxesActor->SetAxesLengthCentimeters(
        SelectionAxesLengthCentimeters);

    SelectionAxesActor->SetActorEnableCollision(false);
    SelectionAxesActor->SetActorHiddenInGame(true);

    return true;
}

void ATGSpacecraftPreviewPawn::
    TrySelectComponentUnderCursor()
{
    APlayerController* PlayerController =
        Cast<APlayerController>(GetController());

    if (
        PlayerController == nullptr ||
        !IsValid(PreviewSpacecraftActor))
    {
        return;
    }

    FHitResult HitResult;

    const bool bUseSrpFacetSelection =
        IsSrpFacetSelectionEnabled();
    PreviewSpacecraftActor->SetSrpProxyHitTestingEnabled(
        bUseSrpFacetSelection);

    FVector2D MousePosition;
    if (!PlayerController->GetMousePosition(
            MousePosition.X,
            MousePosition.Y))
    {
        return;
    }

    FCollisionQueryParams QueryParams;
    QueryParams.bTraceComplex = bUseSrpFacetSelection;
    QueryParams.bReturnFaceIndex = bUseSrpFacetSelection;

    const bool bHitSomething =
        PlayerController->GetHitResultAtScreenPosition(
            MousePosition,
            ECC_Visibility,
            QueryParams,
            HitResult);

    if (bHitSomething)
    {
        UPrimitiveComponent* HitPrimitive =
            Cast<UPrimitiveComponent>(
                HitResult.GetComponent());

        FGuid HitComponentId;

        if (bUseSrpFacetSelection && HitPrimitive != nullptr)
        {
            int32 StableTriangleIndex = INDEX_NONE;
            if (PreviewSpacecraftActor->FindSrpFacetFromPrimitive(
                    HitPrimitive,
                    HitResult.FaceIndex,
                    HitComponentId,
                    StableTriangleIndex))
            {
                OnPreviewSrpFacetClicked.Broadcast(
                    HitComponentId,
                    StableTriangleIndex);
                return;
            }

            if (PreviewSpacecraftActor->FindComponentIdFromPrimitive(
                    HitPrimitive,
                    HitComponentId))
            {
                const FVector PrimitiveLocalNormal =
                    HitPrimitive->GetComponentTransform().
                        InverseTransformVectorNoScale(
                            HitResult.ImpactNormal).
                        GetSafeNormal();
                OnPreviewSrpPrimitiveSurfaceClicked.Broadcast(
                    HitComponentId,
                    PrimitiveLocalNormal);
                return;
            }
        }

        if (
            HitPrimitive != nullptr &&
            PreviewSpacecraftActor->
                FindComponentIdFromPrimitive(
                    HitPrimitive,
                    HitComponentId))
        {
            FText SelectionError;

            SelectPreviewComponent(
                HitComponentId,
                SelectionError);

            return;
        }
    }

    if (bClearSelectionWhenClickingEmptySpace)
    {
        ClearPreviewComponentSelection();
    }
}

void ATGSpacecraftPreviewPawn::
    RefreshSelectionIndicatorTransform()
{
    if (
        !SelectedComponentId.IsValid() ||
        !IsValid(PreviewSpacecraftActor) ||
        !IsValid(SelectionAxesActor))
    {
        return;
    }

    FTransform ComponentWorldTransform;

    if (!PreviewSpacecraftActor->
            GetComponentWorldTransformById(
                SelectedComponentId,
                ComponentWorldTransform))
    {
        ClearPreviewComponentSelection();
        return;
    }

    SelectionAxesActor->SetActorTransform(
        ComponentWorldTransform);
}
