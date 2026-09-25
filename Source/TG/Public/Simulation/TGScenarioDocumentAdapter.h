// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGSim/Core/SimulationRequest.h"
#include "TGSim/Scenario/ScenarioDocument.h"

class UTGControllerLibrarySubsystem;

/**
 * Native boundary shared by HUD file I/O and the Run Simulation action.
 * It contains no physics: it translates Unreal value types to the portable
 * scenario document, then delegates all request construction to TGSimCore.
 */
class TG_API FTGScenarioDocumentAdapter
{
public:
    static void ToPortableDocument(
        const FTGSimulationScenario& Scenario,
        const FString& ResolvedControllerDllPath,
        tgsim::scenario::ScenarioDocument& OutDocument);

    static bool FromPortableDocument(
        const tgsim::scenario::ScenarioDocument& Document,
        const FString& SourceScenarioFilePath,
        FTGSimulationScenario& OutScenario,
        FString& OutMessage);

    static bool BuildSimulationRequest(
        const FTGSimulationScenario& Scenario,
        const UTGControllerLibrarySubsystem* ControllerLibrary,
        const FString& SourceScenarioFilePath,
        tgsim::SimulationRequest& OutRequest,
        FString& OutMessage);
};
