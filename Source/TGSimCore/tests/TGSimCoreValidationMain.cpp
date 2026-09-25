// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if defined(TGSIM_STANDALONE_VALIDATION)

#include "TGSim/Validation/BackendValidationSuite.h"

#include <iostream>

int main()
{
    const tgsim::ValidationReport report = tgsim::RunBackendValidationSuite();
    bool passed = true;
    std::cout << report.scenario_name << '\n';
    for (const tgsim::ValidationCheck& check : report.checks)
    {
        std::cout << (check.passed ? "PASS" : "FAIL") << " | " << check.name
                  << " | error=" << check.error << " | tolerance=" << check.tolerance
                  << " | " << check.message << '\n';
        passed = passed && check.passed;
    }
    return passed ? 0 : 1;
}

#endif
