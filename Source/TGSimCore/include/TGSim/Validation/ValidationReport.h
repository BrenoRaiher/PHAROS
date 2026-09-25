// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Data structures reserved for numerical/physical validation cases.

#include <string>
#include <vector>

namespace tgsim
{
    struct ValidationCheck
    {
        std::string name;
        bool passed = false;
        double error = 0.0;
        double tolerance = 0.0;
        std::string message;
    };

    struct ValidationReport
    {
        std::string scenario_name;
        std::vector<ValidationCheck> checks;
    };
}
