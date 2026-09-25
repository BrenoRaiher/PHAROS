// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGVisualizationTelemetryCardWidget.generated.h"

class UBorder;
class UTextBlock;

/** One styled, live telemetry datum in the visualization dashboard. */
UCLASS(Abstract, Blueprintable)
class TG_API UTGVisualizationTelemetryCardWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetTelemetryValue(
        const FString& InLabel,
        const FString& InValue,
        const FLinearColor& InAccentColor);

protected:
    virtual void NativeConstruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BORDER_TelemetryCard;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_TelemetryAccent;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_TelemetryLabel;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_TelemetryValue;

    FString TelemetryLabel;
    FString TelemetryValue;
    FLinearColor AccentColor = FLinearColor::White;

    void RefreshDesignerWidgets();
};
