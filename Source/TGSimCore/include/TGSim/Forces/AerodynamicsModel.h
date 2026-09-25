// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Forces/AerodynamicCoefficientDatabase.h"
#include "TGSim/Forces/SolarRadiationPressureModel.h"

#include <memory>

namespace tgsim
{
    /// Complete output of one aerodynamic evaluation. The inherited totals feed CM
    /// translation and telemetry; inherited component loads are placed on ABA nodes.
    struct AerodynamicsEvaluation : LoadEvaluation
    {
        /// True when pressure/profile/database/regime limits invalidate the load.
        bool outside_validity = false;
        /// q_dyn = rho |v_rel|^2 / 2 at the current state.
        double dynamic_pressure_pa = 0.0;
        /// Kn = lambda/L, reported so callers can judge the selected aerodynamic regime.
        double knudsen_number = 0.0;
        /// s = |v_rel|/sqrt(2 k_B T/m_bar), the molecular speed ratio.
        double molecular_speed_ratio = 0.0;
        Vec3d force_coefficients_body;
        Vec3d moment_coefficients_body_about_cm;
        bool used_coefficient_database = false;
        bool used_fallback_model = false;
    };

    /// Low-density atmospheric load model attached to a translating and rotating central body.
    class AerodynamicsModel
    {
    public:
        AerodynamicsModel() = default;
        /// Preprocesses the database and builds its nearest-neighbor index once.
        explicit AerodynamicsModel(const AerodynamicsSettings& settings);

        /// Evaluates aerodynamic loads at one ODE state without advancing time.
        ///
        /// General6DofDynamics calls this after evaluating the current total CM. The
        /// function resolves local gas properties, co-rotating
        /// atmosphere-relative velocity, dynamic pressure, Knudsen number, and molecular
        /// speed ratio. It first queries the aggregate coefficient database and otherwise
        /// uses a constant-C_D drag force at the total CM. Neither aggregate path can
        /// assign aerodynamic loads to individual articulated-body nodes.
        AerodynamicsEvaluation ComputeLoads(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& state,
            const SimulationConfig& config,
            const Vec3d& center_of_mass_body_m) const;

    private:
        std::shared_ptr<const AerodynamicCoefficientInterpolator> interpolator_;
    };
}
