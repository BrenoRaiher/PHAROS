// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationRequest.h"
#include "TGSim/Scenario/ScenarioDiagnostics.h"
#include "TGSim/Scenario/ScenarioDocument.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>

namespace tgsim::scenario
{
    struct ScenarioCompilerServices
    {
        // Converts an authored UTC string to SPICE ET/TDB seconds. Both Unreal
        // and the standalone runner supply this from CSPICE.
        std::function<bool(
            const std::string& utc,
            double& ephemeris_time_tdb_seconds,
            std::string& error)> utc_to_ephemeris_time;

        std::shared_ptr<const IEphemerisProvider> ephemeris_provider;
        std::shared_ptr<const IController> controller;
    };

    TGSIMCORE_API bool CompileScenario(
        const ScenarioDocument& document,
        const std::filesystem::path& scenario_file_path,
        const ScenarioCompilerServices& services,
        SimulationRequest& request,
        Diagnostics& diagnostics);
}
