// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/ISimulationObserver.h"
#include "TGSim/Core/SimulationRequest.h"
#include "TGSim/Core/SimulationResult.h"

#include <string>

namespace tgsim
{
    /// Public synchronous entry point corresponding to AVS Simulate.m.
    class SimulationEngine
    {
    public:
        /// Builds and validates the immutable run configuration without
        /// evaluating dynamics or advancing the state. Empty means valid.
        std::string ValidateRequest(const SimulationRequest& request) const;

        /// Validates and propagates one request sequentially, then returns the complete
        /// typed and numeric solution history. No Unreal physics or visualization is run.
        SimulationResult Run(const SimulationRequest& request, ISimulationObserver* observer = nullptr) const;
    };
}
