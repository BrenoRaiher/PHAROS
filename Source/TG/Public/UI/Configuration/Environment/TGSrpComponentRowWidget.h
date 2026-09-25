// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TGSrpComponentRowWidget.generated.h"

class UBorder;
class UButton;
class UCheckBox;
class UTextBlock;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGSrpComponentRowSelected,
    FGuid,
    ComponentId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGSrpComponentRowIncludeChanged,
    FGuid,
    ComponentId,
    bool,
    bIncluded);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FTGSrpComponentRowVisibilityChanged,
    FGuid,
    ComponentId,
    bool,
    bVisible);

/** Native, dynamically-created row used by the SRP component scroll area. */
UCLASS()
class TG_API UTGSrpComponentRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void Configure(
        FGuid InComponentId,
        const FString& ComponentName,
        bool bIncluded,
        bool bPreviewVisible,
        bool bSelected);

    UPROPERTY()
    FTGSrpComponentRowSelected OnComponentSelected;

    UPROPERTY()
    FTGSrpComponentRowIncludeChanged OnIncludeChanged;

    UPROPERTY()
    FTGSrpComponentRowVisibilityChanged OnPreviewVisibilityChanged;

protected:
    virtual void NativeOnInitialized() override;

private:
    UFUNCTION()
    void HandleSelectClicked();

    UFUNCTION()
    void HandleIncludeChanged(bool bIsChecked);

    UFUNCTION()
    void HandleVisibilityClicked();

    UPROPERTY(Transient)
    TObjectPtr<UBorder> RootBorder;

    UPROPERTY(Transient)
    TObjectPtr<UCheckBox> IncludeCheckBox;

    UPROPERTY(Transient)
    TObjectPtr<UButton> SelectButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> NameText;

    UPROPERTY(Transient)
    TObjectPtr<UButton> VisibilityButton;

    UPROPERTY(Transient)
    TObjectPtr<UTextBlock> VisibilityText;

    FGuid ComponentId;
    bool bIsPreviewVisible = true;
    bool bUpdating = false;
};
