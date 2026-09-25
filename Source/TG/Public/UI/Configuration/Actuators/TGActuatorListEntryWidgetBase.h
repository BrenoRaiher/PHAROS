// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Styling/SlateTypes.h"
#include "TGActuatorListEntryWidgetBase.generated.h"

class UButton;
class UTextBlock;
class UTGConfigActuatorsWidgetBase;

/**
 * Native behavior for one reusable actuator-list row.
 *
 * The existing Blueprint BTN_SelectActuator.OnClicked bridge continues to call
 * RequestSelectActuator(). The row then routes directly to the owning panel.
 */
UCLASS(Blueprintable)
class TG_API UTGActuatorListEntryWidgetBase
    : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators")
    void InitializeActuatorListEntry(
        int32 InActuatorIndex,
        const FString& InActuatorName,
        UTGConfigActuatorsWidgetBase* InOwnerPanel,
        bool bInReactionWheel);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators")
    void RequestSelectActuator();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Actuators")
    void SetActuatorListEntrySelected(bool bInSelected);

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_SelectActuator;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ActuatorName;

    UPROPERTY()
    TObjectPtr<UTGConfigActuatorsWidgetBase> OwnerPanel;

    int32 ActuatorIndex = INDEX_NONE;
    bool bReactionWheel = true;
    bool bUnselectedStyleCaptured = false;
    FButtonStyle UnselectedButtonStyle;
};
