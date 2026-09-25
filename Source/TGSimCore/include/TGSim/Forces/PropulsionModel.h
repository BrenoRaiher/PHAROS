// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Forces/SolarRadiationPressureModel.h"

namespace tgsim
{
    /// Complete output of one propulsion evaluation. The inherited loads act on the
    /// spacecraft and its ABA nodes; the added rates become mass-state derivatives.
    struct PropulsionEvaluation : LoadEvaluation
    {
        /// Sum of all active propellant rates; negative while onboard mass is consumed.
        double mass_rate_kgps = 0.0;
        /// Per-variable-mass-state rates, indexed like SpacecraftState's mass vector.
        std::vector<double> component_mass_rates_kgps;
        /// Positive accepted discharge for every configured thruster, in config order.
        std::vector<double> thruster_mass_flows_kgps;
        /// Selected derivative of actual outward discharge for every thruster.
        std::vector<double> thruster_mass_flow_derivatives_kgps2;
        /// Per-variable-mass-state second derivatives selected for CM translation.
        std::vector<double> component_mass_second_derivatives_kgps2;
        /// Applied thrust magnitude for every configured thruster, in config order.
        std::vector<double> thruster_thrusts_n;
    };

    /// Converts prescribed or commanded thruster behavior into force, torque, and mass flow.
    class PropulsionModel
    {
    public:
        /// Evaluates all thrusters at one ODE state without integrating propellant mass.
        ///
        /// General6DofDynamics calls this before its final mass-property evaluation because
        /// propulsion supplies m_dot. Prescribed thrust/Isp histories or commanded
        /// maximum-thrust/controller pairs determine performance; current component
        /// poses determine direction and moment arm.
        /// The result contains totals for CM translation/telemetry, mount-node loads for ABA,
        /// and total/per-component mass derivatives for RK4.
        PropulsionEvaluation ComputeLoads(
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& state,
            const SimulationConfig& config,
            const ControlCommand& command,
            const Vec3d& center_of_mass_body_m,
            const std::vector<ComponentPose>& component_poses) const;
    };
}
