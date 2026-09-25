// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TGLoadingSubsystem.generated.h"

class UTGLoadingOverlayWidget;

/** Owns the single modal loading presentation for one application instance. */
UCLASS()
class TG_API UTGLoadingSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;

    uint64 Show(const FText& Title, const FText& Message);
    void Update(const FText& Title, const FText& Message);
    void Hide();
    void HideIfCurrent(uint64 OperationGeneration);

    bool IsShowing() const;
    uint64 GetOperationGeneration() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UTGLoadingOverlayWidget> ActiveOverlay;

    uint64 OperationGeneration = 0;
};
