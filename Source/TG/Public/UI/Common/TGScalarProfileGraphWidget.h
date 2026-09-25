// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGScalarProfileGraphWidget.generated.h"

/*
 * Read-only graph for HUD scalar profiles.
 *
 * The widget displays the samples using the same piecewise-linear
 * interpolation required by the HUD contract. It does not edit samples.
 */
UCLASS(BlueprintType, Blueprintable)
class TG_API UTGScalarProfileGraphWidget final : public UUserWidget
{
    GENERATED_BODY()

public:
    /*
     * Replaces the currently displayed samples.
     *
     * The scalar-profile CSV parser is responsible for validating order
     * and quantity-specific restrictions before calling this function.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Scalar Profile Graph")
    void SetProfileSamples(
        const TArray<FTGScalarCurveSample>& InSamples);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|UI|Scalar Profile Graph")
    void ClearProfileSamples();

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|UI|Scalar Profile Graph")
    int32 GetProfileSampleCount() const;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Graph|Appearance",
        meta = (ClampMin = "0.0"))
    float PlotPadding = 28.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Graph|Appearance",
        meta = (ClampMin = "0.1"))
    float AxisThickness = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Graph|Appearance",
        meta = (ClampMin = "0.1"))
    float CurveThickness = 2.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Graph|Appearance",
        meta = (ClampMin = "1.0"))
    float MarkerHalfSize = 3.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Graph|Appearance")
    FLinearColor AxisColor =
        FLinearColor(0.55f, 0.55f, 0.55f, 1.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Graph|Appearance")
    FLinearColor CurveColor =
        FLinearColor(0.10f, 0.65f, 1.0f, 1.0f);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Graph|Appearance")
    FLinearColor MarkerColor =
        FLinearColor::White;

protected:
    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

private:
    UPROPERTY(Transient)
    TArray<FTGScalarCurveSample> ProfileSamples;
};