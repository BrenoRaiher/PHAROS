// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGSimulationAdapter.h"

#include "Simulation/Control/TGControllerLibrarySubsystem.h"
#include "Simulation/Control/TGDynamicControllerAdapter.h"
#include "Simulation/TGSpiceEphemerisProvider.h"
#include "SpiceBridge.h"

#include <memory>

bool FTGSimulationAdapter::AttachSpiceEphemerides(
    tgsim::SimulationRequest& Request,
    FString& OutMessage)
{
    if (!FSpiceBridge::LoadKernels(OutMessage)) return false;
    Request.gravity.ephemeris_provider = std::make_shared<FTGSpiceEphemerisProvider>();
    OutMessage = TEXT("CSPICE ephemerides attached to the simulation request.");
    return true;
}

bool FTGSimulationAdapter::AttachController(
    const FTGControlConfig& ControlConfig,
    const UTGControllerLibrarySubsystem* ControllerLibrary,
    tgsim::SimulationRequest& Request,
    FString& OutMessage)
{
    Request.control.controller.reset();

    if (ControlConfig.Mode == ETGControlMode::None)
    {
        OutMessage = TEXT("No user controller selected.");
        return true;
    }

    if (ControlConfig.Mode !=
        ETGControlMode::CompiledUserController)
    {
        OutMessage = TEXT("The selected control mode is unsupported.");
        return false;
    }

    if (ControlConfig.ControllerId.IsNone())
    {
        OutMessage = TEXT("Select a ready user controller.");
        return false;
    }

    if (ControllerLibrary == nullptr)
    {
        OutMessage = TEXT("The controller library is unavailable.");
        return false;
    }

    FString DllPath;

    if (!ControllerLibrary->ResolveReadyControllerDllPath(
            ControlConfig.ControllerId,
            DllPath,
            OutMessage))
    {
        return false;
    }

    std::shared_ptr<FTGDynamicControllerAdapter> Controller =
        FTGDynamicControllerAdapter::Create(
            DllPath,
            OutMessage);

    if (!Controller)
    {
        return false;
    }

    Request.control.controller = MoveTemp(Controller);
    OutMessage = TEXT("User controller attached to the simulation request.");
    return true;
}

bool FTGSimulationAdapter::ConvertUtcToEphemerisTimeTdbSeconds(
    const FString& Utc,
    double& OutEphemerisTimeTdbSeconds,
    FString& OutMessage)
{
    return FSpiceBridge::ConvertUTCToET(
        Utc, OutEphemerisTimeTdbSeconds, OutMessage);
}

bool FTGSimulationAdapter::ConvertEphemerisTimeTdbSecondsToUtc(
    double EphemerisTimeTdbSeconds,
    FString& OutUtc,
    FString& OutMessage)
{
    return FSpiceBridge::ConvertETToUTC(
        EphemerisTimeTdbSeconds, OutUtc, OutMessage);
}
