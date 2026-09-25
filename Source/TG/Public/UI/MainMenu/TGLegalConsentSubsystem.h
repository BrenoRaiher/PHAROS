// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TGLegalConsentSubsystem.generated.h"

class UTGEulaDialogWidget;
class UWorld;

/** Presents and persists versioned EULA consent in packaged builds. */
UCLASS()
class TG_API UTGLegalConsentSubsystem final : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

private:
    void HandlePostLoadMap(UWorld* LoadedWorld);

    void TryShowEula(
        TWeakObjectPtr<UWorld> World,
        int32 RemainingAttempts);

    bool ShouldPrompt() const;
    void RecordAcceptance();
    void HandleAccepted();
    void HandleDeclined();

    FDelegateHandle PostLoadMapHandle;
    TWeakObjectPtr<UTGEulaDialogWidget> ActiveDialog;
};
