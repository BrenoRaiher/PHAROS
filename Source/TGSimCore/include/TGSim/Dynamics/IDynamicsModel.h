// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Core/Types.h"

namespace tgsim
{
    /// Interface for an ODE right-hand side consumed by a numerical integrator.
    class IDynamicsModel
    {
    public:
        virtual ~IDynamicsModel() = default;

        /// Evaluates dy/dt and diagnostic loads at one time/state without advancing time.
        /// The returned DynamicsEvaluation contains both StateDerivative and telemetry.
        virtual DynamicsEvaluation ComputeDynamics(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& state,
            const SimulationConfig& config) const = 0;
    };
}
