// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Core/SimulationRequest.h"

namespace tgsim
{
    /// Normalizes a direct SimulationRequest into the immutable run configuration.
    /// This is not a HUD builder and does not read Unreal objects.
    class SimulationConfigBuilder
    {
    public:
        /// Copies settings, normalizes attitude, initializes component/wheel states,
        /// and derives initial total mass. Returns a ready-to-validate configuration.
        SimulationConfig Build(const SimulationRequest& request) const;
    };
}
