// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Integrators/IIntegrator.h"

namespace tgsim
{
    /// Classical explicit fourth-order Runge-Kutta integrator used by SimulationEngine.
    class FixedStepRK4 final : public IIntegrator
    {
    public:
        /// Advances the complete SpacecraftState from t to t+step_seconds.
        ///
        /// Each of k1 through k4 calls IDynamicsModel::ComputeDynamics(), so the controller,
        /// environment, force models, mass properties, and ABA are all reevaluated at
        /// the corresponding intermediate state. After the weighted RK4 update, this
        /// function normalizes attitude, enforces component mass floors, and recomputes
        /// total mass. SimulationEngine owns discontinuous articulation constraints.
        IntegrationAdvance Advance(
            const IDynamicsModel& dynamics_model,
            const SimulationConfig& config,
            const SpacecraftState& state,
            double maximum_elapsed_seconds,
            const ISimulationObserver* observer = nullptr) const override;
    };
}
