// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Visualization/TGSimulationPlaybackActor.h"
#include "TGVisualizationArrowRowWidget.generated.h"

class ATGSimulationPlaybackActor;
class UBorder;
class UCheckBox;
class USizeBox;
class UTextBlock;

/** Native behavior for one Designer-authored vector-selection row. */
UCLASS(Abstract, Blueprintable)
class TG_API UTGVisualizationArrowRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeArrowRow(
        ATGSimulationPlaybackActor* InPlaybackActor,
        const FTGVisualizationArrowInfo& InArrowInfo);

    const FString& GetArrowLabel() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> ROOT_ArrowRow;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BORDER_RowSurface;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_Visible;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BORDER_ColorSwatch;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<USizeBox> SIZE_ColorSwatch;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ArrowLabel;

    UPROPERTY(Transient)
    TObjectPtr<ATGSimulationPlaybackActor> PlaybackActor;

    FName ArrowId;
    FString ArrowLabel;
    FLinearColor ArrowColor = FLinearColor::White;
    bool bInitialVisibility = false;

    void RefreshDesignerWidgets();

    UFUNCTION()
    void HandleCheckStateChanged(bool bChecked);
};
