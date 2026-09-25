// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Public boundary of the multibody forward-dynamics stage. General6DofDynamics
// evaluates loads and actuator commands first, then supplies them here so ABA can
// solve the unknown bus and joint accelerations at the current state.

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Dynamics/SpatialAlgebra.h"
#include "TGSim/Vehicle/MassProperties.h"

namespace tgsim
{
    struct FloatingBaseTreeInput
    {
        /// Equivalent force not already assigned to a component node, in ICRF axes.
        /// Gravity and body-level control loads normally enter ABA through this field.
        Vec3d external_force_icrf_n;
        /// Equivalent torque about the instantaneous spacecraft CM, in body axes.
        /// SolveForwardDynamics() shifts this moment to the base origin before forming the root wrench.
        Vec3d external_torque_about_cm_body_nm;
        /// Loads that must act at their physical component nodes, indexed like vehicle.components.
        /// Thruster, SRP, and aerodynamic loads use this path so joint reactions are preserved.
        std::vector<ComponentLoad> component_external_loads;
        /// Current total-spacecraft CM position measured from the base origin, in body axes.
        Vec3d center_of_mass_body_m;
        /// beta = sum(m_dot_i (r_i-r_CM))/M in body axes. The propagated
        /// translational velocity is geometric-CM velocity, while retained
        /// lumped material momentum uses v_CM-beta.
        Vec3d center_of_mass_mass_redistribution_rate_body_mps;
        /// Current pose of each component; used to transform loads, momenta, and CM results.
        std::vector<ComponentPose> component_poses;
        /// Current eta_dot values used in ABA's outward velocity pass.
        std::vector<double> articulation_rates;
        /// Known generalized actuator forces/torques tau, one per flattened scalar DOF.
        std::vector<double> applied_joint_efforts;
        /// Current m_dot values used by the selected variable-mass convention.
        std::vector<double> variable_component_mass_rates_kgps;
        /// Positive accepted nozzle discharge, indexed like vehicle.thrusters.
        std::vector<double> thruster_mass_flows_kgps;
        /// Commanded h_dot for each reaction wheel; these enter the mount's bias wrench.
        std::vector<double> wheel_momentum_rates_nm;
    };

    struct FloatingBaseTreeEvaluation
    {
        bool solved = false;
        /// ABA spatial acceleration a_0=[alpha_B; a_O_B-omega_B x v_O_B], in main-body axes.
        SpatialMotion base_acceleration_body;
        /// Ordinary inertial acceleration of the main-body reference origin, in main-body axes.
        Vec3d base_origin_acceleration_body_mps2;
        /// Mass-weighted ordinary acceleration of the component centroids, in ICRF.
        /// During asymmetric depletion this is not the changing geometric-CM acceleration.
        Vec3d mass_weighted_component_acceleration_icrf_mps2;
        /// Total inertial retained-material linear momentum.
        Vec3d total_linear_momentum_icrf_kgmps;
        /// Total inertial angular momentum about the instantaneous spacecraft CM.
        Vec3d total_angular_momentum_about_cm_icrf_kgm2ps;
        /// External carrier-momentum flux in the solver's Galilean-boosted observer,
        /// resolved in body axes. The angular value is about the instantaneous CM.
        Vec3d nozzle_carrier_linear_momentum_flux_solver_body_n;
        Vec3d nozzle_carrier_angular_momentum_flux_about_cm_solver_body_nm;
        /// Solved eta_ddot for every flattened articulation state coordinate.
        std::vector<double> joint_accelerations;
        /// Reaction supplied by an active coordinate/rate stop; zero for a free joint.
        std::vector<double> joint_constraint_efforts;
    };

    /// Implements Featherstone's O(N) Articulated-Body Algorithm with a free base.
    ///
    /// Workflow position: General6DofDynamics calls SolveForwardDynamics() once per ODE evaluation,
    /// including each RK4 stage, after component poses, masses, external loads, wheel
    /// commands, and joint efforts are known. SolveForwardDynamics() expands the physical component
    /// hierarchy into scalar-DOF ABA nodes, runs the velocity/inward/root/outward passes,
    /// enforces active joint stops, and returns alpha_B plus every eta_ddot. It computes
    /// accelerations only; FixedStepRK4 later integrates them into the propagated state.
    class FloatingBaseTreeDynamics
    {
    public:
        FloatingBaseTreeEvaluation SolveForwardDynamics(
            const SpacecraftState& state,
            const SimulationConfig& config,
            const FloatingBaseTreeInput& input,
            bool enforce_joint_acceleration_limits = true) const;
    };
}
