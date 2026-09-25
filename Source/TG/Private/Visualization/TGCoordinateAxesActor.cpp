// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Visualization/TGCoordinateAxesActor.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

ATGCoordinateAxesActor::ATGCoordinateAxesActor()
{
    PrimaryActorTick.bCanEverTick = false;

    AxesRoot =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("AxesRoot"));

    SetRootComponent(AxesRoot);

    XAxisArrow =
        CreateDefaultSubobject<UArrowComponent>(
            TEXT("XAxisArrow"));

    XAxisArrow->SetupAttachment(AxesRoot);

    YAxisArrow =
        CreateDefaultSubobject<UArrowComponent>(
            TEXT("YAxisArrow"));

    YAxisArrow->SetupAttachment(AxesRoot);

    ZAxisArrow =
        CreateDefaultSubobject<UArrowComponent>(
            TEXT("ZAxisArrow"));

    ZAxisArrow->SetupAttachment(AxesRoot);

    XAxisArrow->SetRelativeRotation(
        FRotator::ZeroRotator);

    YAxisArrow->SetRelativeRotation(
        FRotator(
            0.0,
            -90.0,
            0.0));

    ZAxisArrow->SetRelativeRotation(
        FRotator(
            90.0,
            0.0,
            0.0));

    XAxisArrow->SetArrowFColor(FColor::Red);
    YAxisArrow->SetArrowFColor(FColor::Green);
    ZAxisArrow->SetArrowFColor(FColor::Blue);

    XAxisArrow->SetHiddenInGame(false);
    YAxisArrow->SetHiddenInGame(false);
    ZAxisArrow->SetHiddenInGame(false);

    XAxisArrow->SetVisibility(true);
    YAxisArrow->SetVisibility(true);
    ZAxisArrow->SetVisibility(true);

    XAxisArrow->SetTreatAsASprite(false);
    YAxisArrow->SetTreatAsASprite(false);
    ZAxisArrow->SetTreatAsASprite(false);

    XAxisArrow->SetUseInEditorScaling(false);
    YAxisArrow->SetUseInEditorScaling(false);
    ZAxisArrow->SetUseInEditorScaling(false);

    XAxisArrow->SetIsScreenSizeScaled(false);
    YAxisArrow->SetIsScreenSizeScaled(false);
    ZAxisArrow->SetIsScreenSizeScaled(false);

    RefreshAxesAppearance();
}

void ATGCoordinateAxesActor::OnConstruction(
    const FTransform& Transform)
{
    Super::OnConstruction(Transform);

    RefreshAxesAppearance();
}

void ATGCoordinateAxesActor::
    SetAxesLengthCentimeters(
        double NewLengthCentimeters)
{
    AxesLengthCentimeters =
        FMath::Max(
            NewLengthCentimeters,
            1.0);

    RefreshAxesAppearance();
}

void ATGCoordinateAxesActor::
    RefreshAxesAppearance()
{
    const float SafeLength =
        static_cast<float>(
            FMath::Max(
                AxesLengthCentimeters,
                1.0));

    const float SafeArrowSize =
        static_cast<float>(
            FMath::Max(
                ArrowSize,
                0.01));

    XAxisArrow->SetArrowLength(SafeLength);
    YAxisArrow->SetArrowLength(SafeLength);
    ZAxisArrow->SetArrowLength(SafeLength);

    XAxisArrow->SetArrowSize(SafeArrowSize);
    YAxisArrow->SetArrowSize(SafeArrowSize);
    ZAxisArrow->SetArrowSize(SafeArrowSize);

    XAxisArrow->MarkRenderStateDirty();
    YAxisArrow->MarkRenderStateDirty();
    ZAxisArrow->MarkRenderStateDirty();
}