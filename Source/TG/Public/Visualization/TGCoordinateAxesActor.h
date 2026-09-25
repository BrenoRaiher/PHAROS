// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TGCoordinateAxesActor.generated.h"

class UArrowComponent;
class USceneComponent;

/**
 * Reusable world-space coordinate axes.
 *
 * The component-tree preview places this actor at body frame B.
 */
UCLASS(BlueprintType, Blueprintable)
class TG_API ATGCoordinateAxesActor : public AActor
{
    GENERATED_BODY()

public:
    ATGCoordinateAxesActor();

    virtual void OnConstruction(
        const FTransform& Transform) override;

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Coordinate Axes",
        meta = (DisplayName = "Set Axes Length"))
    void SetAxesLengthCentimeters(
        double NewLengthCentimeters);

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Coordinate Axes")
    TObjectPtr<USceneComponent> AxesRoot;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Coordinate Axes")
    TObjectPtr<UArrowComponent> XAxisArrow;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Coordinate Axes")
    TObjectPtr<UArrowComponent> YAxisArrow;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Coordinate Axes")
    TObjectPtr<UArrowComponent> ZAxisArrow;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Coordinate Axes",
        meta = (ClampMin = "1.0"))
    double AxesLengthCentimeters = 200.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "PHAROS|Coordinate Axes",
        meta = (ClampMin = "0.01"))
    double ArrowSize = 1.0;

private:
    void RefreshAxesAppearance();
};