// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/MainMenu/TGLegalConsentSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProperties.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "TimerManager.h"
#include "UI/MainMenu/TGEulaDialogWidget.h"
#include "UI/MainMenu/TGLegalDocumentUtils.h"
#include "UObject/UObjectGlobals.h"

namespace TGLegalConsentPrivate
{
    constexpr TCHAR ConfigSection[] =
        TEXT("/Script/TG.PHAROSLegalConsent");
    constexpr TCHAR AcceptedVersionKey[] =
        TEXT("AcceptedEulaVersion");
    constexpr TCHAR CurrentEulaVersion[] = TEXT("1.0");
    constexpr TCHAR EulaFileName[] = TEXT("EULA.txt");
    constexpr int32 MaximumShowAttempts = 100;
}

void UTGLegalConsentSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const bool bForcePrompt = FParse::Param(
        FCommandLine::Get(),
        TEXT("PHAROSForceEula"));
    if (!FPlatformProperties::RequiresCookedData() && !bForcePrompt)
    {
        return;
    }

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this,
        &UTGLegalConsentSubsystem::HandlePostLoadMap);

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        HandlePostLoadMap(GameInstance->GetWorld());
    }
}

void UTGLegalConsentSubsystem::Deinitialize()
{
    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(
            PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }

    if (ActiveDialog.IsValid())
    {
        ActiveDialog->RemoveFromParent();
        ActiveDialog.Reset();
    }

    Super::Deinitialize();
}

void UTGLegalConsentSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!ShouldPrompt() ||
        LoadedWorld == nullptr ||
        !LoadedWorld->IsGameWorld() ||
        LoadedWorld->GetGameInstance() != GetGameInstance())
    {
        return;
    }

    LoadedWorld->GetTimerManager().SetTimerForNextTick(
        FTimerDelegate::CreateUObject(
            this,
            &UTGLegalConsentSubsystem::TryShowEula,
            TWeakObjectPtr<UWorld>(LoadedWorld),
            TGLegalConsentPrivate::MaximumShowAttempts));
}

void UTGLegalConsentSubsystem::TryShowEula(
    TWeakObjectPtr<UWorld> World,
    const int32 RemainingAttempts)
{
    if (!ShouldPrompt() || ActiveDialog.IsValid() || !World.IsValid())
    {
        return;
    }

    APlayerController* OwningPlayer = World->GetFirstPlayerController();
    if (OwningPlayer == nullptr)
    {
        if (RemainingAttempts > 0)
        {
            FTimerHandle RetryHandle;
            World->GetTimerManager().SetTimer(
                RetryHandle,
                FTimerDelegate::CreateUObject(
                    this,
                    &UTGLegalConsentSubsystem::TryShowEula,
                    World,
                    RemainingAttempts - 1),
                0.1f,
                false);
        }
        return;
    }

    FString EulaText;
    FString EulaPath;
    const bool bLoaded = TGLegalDocumentUtils::LoadText(
        TGLegalConsentPrivate::EulaFileName,
        EulaText,
        EulaPath);

    if (!bLoaded)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PHAROS: The packaged EULA could not be loaded."));
        EulaText = TEXT(
            "The PHAROS End User License Agreement is unavailable. "
            "Acceptance is disabled to prevent use without the required "
            "terms. Reinstall PHAROS or restore its Legal directory.");
    }

    UTGEulaDialogWidget* Dialog =
        CreateWidget<UTGEulaDialogWidget>(
            OwningPlayer,
            UTGEulaDialogWidget::StaticClass());
    if (Dialog == nullptr)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PHAROS: Could not create the EULA dialog."));
        return;
    }

    Dialog->Configure(
        TGLegalConsentPrivate::CurrentEulaVersion,
        FText::FromString(EulaText),
        bLoaded,
        FSimpleDelegate::CreateUObject(
            this,
            &UTGLegalConsentSubsystem::HandleAccepted),
        FSimpleDelegate::CreateUObject(
            this,
            &UTGLegalConsentSubsystem::HandleDeclined));
    Dialog->AddToViewport(10000);
    Dialog->SetKeyboardFocus();
    ActiveDialog = Dialog;
}

bool UTGLegalConsentSubsystem::ShouldPrompt() const
{
    if (FParse::Param(FCommandLine::Get(), TEXT("PHAROSForceEula")))
    {
        return true;
    }

    FString AcceptedVersion;
    return GConfig == nullptr ||
        !GConfig->GetString(
            TGLegalConsentPrivate::ConfigSection,
            TGLegalConsentPrivate::AcceptedVersionKey,
            AcceptedVersion,
            GGameUserSettingsIni) ||
        !AcceptedVersion.Equals(
            TGLegalConsentPrivate::CurrentEulaVersion,
            ESearchCase::CaseSensitive);
}

void UTGLegalConsentSubsystem::RecordAcceptance()
{
    if (GConfig == nullptr)
    {
        return;
    }

    GConfig->SetString(
        TGLegalConsentPrivate::ConfigSection,
        TGLegalConsentPrivate::AcceptedVersionKey,
        TGLegalConsentPrivate::CurrentEulaVersion,
        GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void UTGLegalConsentSubsystem::HandleAccepted()
{
    RecordAcceptance();
    if (ActiveDialog.IsValid())
    {
        ActiveDialog->RemoveFromParent();
        ActiveDialog.Reset();
    }
}

void UTGLegalConsentSubsystem::HandleDeclined()
{
    FPlatformMisc::RequestExit(false);
}
