// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Scenario/ScenarioDiagnostics.h"
#include "TGSim/Scenario/ScenarioDocument.h"

#include <filesystem>
#include <string>

namespace tgsim::scenario
{
    // Parses/serializes the complete portable scenario, including Unreal-only
    // presentation and generated-cache fields.
    TGSIMCORE_API bool ParseScenarioText(
        const std::string& text,
        ScenarioDocument& document,
        Diagnostics& diagnostics,
        const std::string& source_name = "<memory>");

    TGSIMCORE_API std::string SerializeScenarioText(
        const ScenarioDocument& document);

    TGSIMCORE_API bool LoadScenarioFile(
        const std::filesystem::path& file_path,
        ScenarioDocument& document,
        Diagnostics& diagnostics);

    TGSIMCORE_API bool SaveScenarioFile(
        const std::filesystem::path& file_path,
        const ScenarioDocument& document,
        Diagnostics& diagnostics);

    // Relative references are anchored to the directory containing the .tgscn.
    // Absolute references are normalized but otherwise left unchanged.
    TGSIMCORE_API std::filesystem::path ResolveReferencedPath(
        const std::filesystem::path& scenario_file_path,
        const std::string& authored_path);
}
