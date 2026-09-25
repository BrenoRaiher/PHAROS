// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Scenario/ScenarioDiagnostics.h"

#include <sstream>

namespace tgsim::scenario
{
    bool HasErrors(const Diagnostics& diagnostics)
    {
        for (const Diagnostic& diagnostic : diagnostics)
        {
            if (diagnostic.severity == DiagnosticSeverity::Error) return true;
        }
        return false;
    }

    std::string FormatDiagnostics(const Diagnostics& diagnostics)
    {
        std::ostringstream output;
        for (const Diagnostic& diagnostic : diagnostics)
        {
            output << (diagnostic.severity == DiagnosticSeverity::Error
                           ? "error"
                           : "warning");
            if (!diagnostic.code.empty()) output << " " << diagnostic.code;
            if (!diagnostic.field_path.empty())
                output << " at " << diagnostic.field_path;
            if (diagnostic.line != 0)
            {
                output << " (line " << diagnostic.line;
                if (diagnostic.column != 0)
                    output << ", column " << diagnostic.column;
                output << ")";
            }
            output << ": " << diagnostic.message << '\n';
        }
        return output.str();
    }
}
