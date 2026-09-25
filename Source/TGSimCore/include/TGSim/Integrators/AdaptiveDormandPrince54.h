// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Integrators/IIntegrator.h"

namespace tgsim
{
    /// Boost.Odeint Dormand-Prince 5(4) integrator with local error control.
    /// The fifth-order solution advances the state while the embedded fourth-order
    /// solution estimates local error and drives step acceptance/rejection.
    class AdaptiveDormandPrince54 final : public IIntegrator
    {
    public:
        IntegrationAdvance Advance(
            const IDynamicsModel& dynamics_model,
            const SimulationConfig& config,
            const SpacecraftState& state,
            double maximum_elapsed_seconds,
            const ISimulationObserver* observer = nullptr) const override;
    };
}
