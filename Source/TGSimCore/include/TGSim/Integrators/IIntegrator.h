// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Core/Types.h"
#include "TGSim/Dynamics/IDynamicsModel.h"

#include <string>

namespace tgsim
{
    class ISimulationObserver;

    struct IntegrationAdvance
    {
        SpacecraftState state;
        bool success = false;
        std::string message;
        std::size_t attempted_steps = 0;
    };

    /// Interface for advancing a state through one numerical integration step.
    class IIntegrator
    {
    public:
        virtual ~IIntegrator() = default;

        /// Advances through at most maximum_elapsed_seconds. A fixed method takes one
        /// step; an adaptive method may take and reject multiple smaller internal steps.
        virtual IntegrationAdvance Advance(
            const IDynamicsModel& dynamics_model,
            const SimulationConfig& config,
            const SpacecraftState& state,
            double maximum_elapsed_seconds,
            const ISimulationObserver* observer = nullptr) const = 0;
    };
}
