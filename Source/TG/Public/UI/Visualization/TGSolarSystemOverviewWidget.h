// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGSolarSystemOverviewWidget.generated.h"

class ATGSimulationPlaybackActor;

/** Native interactive projection painted inside a Designer-authored widget. */
UCLASS(Abstract, Blueprintable)
class TG_API UTGSolarSystemOverviewWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeForPlayback(ATGSimulationPlaybackActor* InPlaybackActor);
    void SetBodyCenteredView(bool bEnabled);
    void FocusSpacecraftPath();
    void ResetSolarSystemView();

protected:
    virtual void NativeDestruct() override;

    virtual int32 NativePaint(
        const FPaintArgs& Args,
        const FGeometry& AllottedGeometry,
        const FSlateRect& MyCullingRect,
        FSlateWindowElementList& OutDrawElements,
        int32 LayerId,
        const FWidgetStyle& InWidgetStyle,
        bool bParentEnabled) const override;

    virtual FReply NativeOnMouseButtonDown(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseButtonUp(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseMove(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseWheel(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual void NativeOnMouseEnter(
        const FGeometry& InGeometry,
        const FPointerEvent& InMouseEvent) override;

    virtual void NativeOnMouseLeave(
        const FPointerEvent& InMouseEvent) override;

private:
    UPROPERTY(Transient)
    TObjectPtr<ATGSimulationPlaybackActor> PlaybackActor;

    FQuat ViewRotation = FQuat(FRotator(55.0f, -25.0f, 0.0f));
    double ViewZoom = 1.0;
    FVector2D ViewPanPixels = FVector2D::ZeroVector;
    bool bRotating = false;
    bool bPanning = false;
    bool bBodyCenteredView = false;

    int32 PaintClosestBodyView(
        const FGeometry& AllottedGeometry,
        FSlateWindowElementList& OutDrawElements,
        int32 BaseLayer,
        bool bParentEnabled) const;

    UFUNCTION()
    void HandlePlaybackTimeChanged(
        double ElapsedSimulationSeconds,
        double EphemerisTimeTdbSeconds,
        double NormalizedTime);
};
