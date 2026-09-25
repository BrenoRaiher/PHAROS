// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Core/Types.h"

namespace tgsim
{
    /// Generic extension interface for a model that contributes force and torque.
    /// The current General6DofDynamics uses the concrete model classes directly.
    class IForceModel
    {
    public:
        virtual ~IForceModel() = default;

        /// Evaluates loads at one time/state without advancing the state.
        /// Returns force in ICRF and torque in body axes through ForceTorqueSample.
        virtual ForceTorqueSample ComputeLoads(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& state,
            const SimulationConfig& config) const = 0;
    };
}
