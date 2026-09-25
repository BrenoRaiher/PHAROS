// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Dynamics/IDynamicsModel.h"
#include "TGSim/Forces/AerodynamicsModel.h"
#include "TGSim/Forces/SolarRadiationPressureModel.h"

namespace tgsim
{
    /// Coupled translation, attitude, angular rate, articulated joints, mass, and wheel dynamics.
    class General6DofDynamics final : public IDynamicsModel
    {
    public:
        /// Builds run-constant force-model acceleration structures.
        explicit General6DofDynamics(const SimulationConfig& config);

        /// Sums gravity, propulsion, SRP, aerodynamics, and control loads,
        /// then returns the complete generalized state derivative and load breakdown.
        DynamicsEvaluation ComputeDynamics(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& state,
            const SimulationConfig& config) const override;

    private:
        SolarRadiationPressureModel solar_radiation_model_;
        AerodynamicsModel aerodynamics_model_;
    };
}
