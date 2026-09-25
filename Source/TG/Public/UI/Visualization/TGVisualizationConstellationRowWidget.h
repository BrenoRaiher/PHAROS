// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGVisualizationConstellationRowWidget.generated.h"

class UCheckBox;
class UBorder;
class UTextBlock;

/**
 * One constellation-selection row owned by the visualization HUD.
 *
 * The row talks directly to the existing BP_ConstellationShell actors and
 * controls only their outline. Constellation artwork is deliberately not part
 * of the visualization workspace anymore.
 */
UCLASS(Abstract, Blueprintable)
class TG_API UTGVisualizationConstellationRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void InitializeConstellationRow(
        const FString& InConstellationId,
        const FString& InConstellationName);

    const FString& GetConstellationName() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UBorder> BORDER_ConstellationRow;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHECK_Outline;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ConstellationName;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_OutlineLabel;

    FString ConstellationId;
    FString ConstellationName;
    bool bOutlineVisible = false;
    bool bRefreshingDesignerWidgets = false;

    void RefreshDesignerWidgets();
    void ApplyOutlineVisibility(bool bVisible);

    UFUNCTION()
    void HandleOutlineCheckStateChanged(bool bChecked);
};
