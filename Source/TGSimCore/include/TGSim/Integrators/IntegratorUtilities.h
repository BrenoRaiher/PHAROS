// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"

namespace tgsim
{
    /// Restores continuous state invariants after an accepted numerical step.
    /// Discontinuous articulation constraints are resolved by SimulationEngine at
    /// their event times and must not be independently clipped by an integrator.
    void RestoreIntegratedStateInvariants(
        const SimulationConfig& config,
        SpacecraftState& state);
}
