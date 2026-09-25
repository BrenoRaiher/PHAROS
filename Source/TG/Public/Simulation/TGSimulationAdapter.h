// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGSim/Core/SimulationRequest.h"

class UTGControllerLibrarySubsystem;

class TG_API FTGSimulationAdapter
{
public:
    FTGSimulationAdapter() = default;

    static bool AttachSpiceEphemerides(
        tgsim::SimulationRequest& Request,
        FString& OutMessage);

    /** Resolves the scenario's opaque controller ID into one run-local DLL instance. */
    static bool AttachController(
        const FTGControlConfig& ControlConfig,
        const UTGControllerLibrarySubsystem* ControllerLibrary,
        tgsim::SimulationRequest& Request,
        FString& OutMessage);

    static bool ConvertUtcToEphemerisTimeTdbSeconds(
        const FString& Utc,
        double& OutEphemerisTimeTdbSeconds,
        FString& OutMessage);

    static bool ConvertEphemerisTimeTdbSecondsToUtc(
        double EphemerisTimeTdbSeconds,
        FString& OutUtc,
        FString& OutMessage);
};
