// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/Export.h"

#include <cstddef>
#include <string>
#include <vector>

namespace tgsim::scenario
{
    enum class DiagnosticSeverity
    {
        Warning,
        Error
    };

    struct Diagnostic
    {
        DiagnosticSeverity severity = DiagnosticSeverity::Error;
        std::string code;
        std::string field_path;
        std::string message;
        std::size_t line = 0;
        std::size_t column = 0;
    };

    using Diagnostics = std::vector<Diagnostic>;

    TGSIMCORE_API bool HasErrors(const Diagnostics& diagnostics);
    TGSIMCORE_API std::string FormatDiagnostics(const Diagnostics& diagnostics);
}
