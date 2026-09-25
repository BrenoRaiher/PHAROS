// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Validation/ValidationReport.h"

namespace tgsim
{
    /// Runs deterministic backend checks without Unreal, rendering, or parallel work.
    ValidationReport RunBackendValidationSuite();
}
