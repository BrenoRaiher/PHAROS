// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Controls/TGControlEditingLibrary.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Components/WidgetSwitcher.h"
#include "UI/Configuration/Controls/TGConfigControlsWidgetBase.h"

void UTGControlEditingLibrary::SetControlMode(
    FTGControlConfig& Control,
    const ETGControlMode Mode)
{
    Control.Mode = Mode;

    if (Mode == ETGControlMode::None)
    {
        Control.ControllerId = NAME_None;
        Control.StandaloneControllerDllFilePath.Reset();
    }
}

bool UTGControlEditingLibrary::SelectReadyController(
    const UObject* WorldContextObject,
    FTGControlConfig& Control,
    const FName ControllerId,
    FText& OutError)
{
    UTGControllerLibrarySubsystem* Library =
        ResolveLibrary(WorldContextObject);

    if (Library == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The controller library is unavailable."));
        return false;
    }

    FText Reason;

    if (!Library->IsControllerReady(ControllerId, Reason))
    {
        OutError = Reason;
        return false;
    }

    Control.Mode = ETGControlMode::CompiledUserController;
    Control.ControllerId = ControllerId;
    Control.StandaloneControllerDllFilePath.Reset();
    OutError = FText::GetEmpty();
    return true;
}

void UTGControlEditingLibrary::ClearSelectedController(
    FTGControlConfig& Control)
{
    Control.ControllerId = NAME_None;
}

TArray<FTGControllerRecord>
UTGControlEditingLibrary::GetReadyControllers(
    const UObject* WorldContextObject)
{
    UTGControllerLibrarySubsystem* Library =
        ResolveLibrary(WorldContextObject);

    if (Library == nullptr)
    {
        return {};
    }

    TArray<FTGControllerRecord> Result;

    for (const FTGControllerRecord& Record :
         Library->GetControllers())
    {
        FText IgnoredReason;

        if (Library->IsControllerReady(
                Record.ControllerId,
                IgnoredReason))
        {
            Result.Add(Record);
        }
    }

    return Result;
}

bool UTGControlEditingLibrary::ValidateControlSelection(
    const UObject* WorldContextObject,
    const FTGControlConfig& Control,
    FText& OutError)
{
    if (Control.Mode == ETGControlMode::None)
    {
        OutError = FText::GetEmpty();
        return true;
    }

    if (Control.Mode != ETGControlMode::CompiledUserController)
    {
        OutError = FText::FromString(
            TEXT("The selected control mode is unsupported."));
        return false;
    }

    if (Control.ControllerId.IsNone())
    {
        OutError = FText::FromString(
            Control.StandaloneControllerDllFilePath.IsEmpty()
                ? TEXT("Select a ready user controller.")
                : TEXT(
                    "The imported PHAROS scenario references a standalone controller DLL. "
                    "Import and trust it in the Controller Library, then select it for this scenario."));
        return false;
    }

    UTGControllerLibrarySubsystem* Library =
        ResolveLibrary(WorldContextObject);

    if (Library == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The controller library is unavailable."));
        return false;
    }

    return Library->IsControllerReady(
        Control.ControllerId,
        OutError);
}

UTGControllerLibrarySubsystem*
UTGControlEditingLibrary::ResolveLibrary(
    const UObject* WorldContextObject)
{
    if (WorldContextObject == nullptr)
    {
        return nullptr;
    }

    const UWorld* World = WorldContextObject->GetWorld();
    UGameInstance* GameInstance =
        World != nullptr
            ? World->GetGameInstance()
            : nullptr;

    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<
            UTGControllerLibrarySubsystem>()
        : nullptr;
}


bool UTGControlEditingLibrary::RefreshControlsPanelInSwitcher(
    UWidgetSwitcher* Switcher)
{
    if (Switcher == nullptr)
    {
        return false;
    }

    const int32 ChildCount = Switcher->GetChildrenCount();

    for (int32 Index = 0; Index < ChildCount; ++Index)
    {
        if (UTGConfigControlsWidgetBase* ControlsPanel =
                Cast<UTGConfigControlsWidgetBase>(
                    Switcher->GetChildAt(Index)))
        {
            ControlsPanel->RefreshFromCurrentDraft();
            return true;
        }
    }

    return false;
}
