// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Forces/PropulsionModel.h"

// Converts thruster profiles/controller commands into loads and propellant mass rates.

#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>
#include <cmath>

namespace tgsim
{
    PropulsionEvaluation PropulsionModel::ComputeLoads(
        double ephemeris_time_tdb_seconds,
        const SpacecraftState& state,
        const SimulationConfig& config,
        const ControlCommand& command,
        const Vec3d& center_of_mass_body_m,
        const std::vector<ComponentPose>& component_poses) const
    {
        // Evaluate every thruster at one ODE/RK stage. General6DofDynamics consumes the
        // totals for CM translation and telemetry, the mount loads for ABA, and m_dot for
        // both the state derivative and time-varying mass properties.
        PropulsionEvaluation result;
        const double elapsed_time_seconds =
            ephemeris_time_tdb_seconds ==
                state.ephemeris_time_tdb_seconds
                ? state.elapsed_time_seconds
                : ephemeris_time_tdb_seconds -
                    config.solver.start_ephemeris_time_tdb_seconds;
        // These vector indices are contracts: mass-state index for rates, component index
        // for loads. Zero-filled entries mean no consumption/load in this evaluation.
        result.component_mass_rates_kgps.assign(state.variable_component_masses_kg.size(), 0.0);
        result.component_mass_second_derivatives_kgps2.assign(
            state.variable_component_masses_kg.size(), 0.0);
        result.component_loads.resize(config.vehicle.components.size());
        result.thruster_mass_flows_kgps.assign(config.vehicle.thrusters.size(), 0.0);
        result.thruster_mass_flow_derivatives_kgps2.assign(
            config.vehicle.thrusters.size(), 0.0);
        result.thruster_thrusts_n.assign(config.vehicle.thrusters.size(), 0.0);

        for (std::size_t thruster_index = 0; thruster_index < config.vehicle.thrusters.size(); ++thruster_index)
        {
            const ThrusterDefinition& thruster = config.vehicle.thrusters[thruster_index];
            const double ignition_elapsed_time =
                ThrusterIgnitionElapsedTime(
                    thruster,
                    config.solver.start_ephemeris_time_tdb_seconds);
            const double shutdown_elapsed_time =
                ThrusterShutdownElapsedTime(
                    thruster,
                    config.solver.start_ephemeris_time_tdb_seconds);
            if (thruster.component_index >= config.vehicle.components.size() ||
                thruster.component_index >= component_poses.size() ||
                elapsed_time_seconds < ignition_elapsed_time ||
                elapsed_time_seconds >= shutdown_elapsed_time)
            {
                continue;
            }

            // Every thrust source consumes mass owned by one physical component.
            // SimulationEngine validates this contract; these checks also keep direct
            // model calls from producing force or an unassigned spacecraft mass loss.
            if (thruster.propellant_component_index >=
                config.vehicle.components.size())
            {
                continue;
            }
            const ComponentDefinition& propellant_component =
                config.vehicle.components[
                    thruster.propellant_component_index];
            const std::size_t propellant_mass_state_index =
                propellant_component.variable_mass_state_index;
            if (propellant_mass_state_index >=
                    state.variable_component_masses_kg.size() ||
                propellant_mass_state_index >=
                    result.component_mass_rates_kgps.size())
            {
                continue;
            }
            if (MassPropertiesModel::ComponentMass(
                    propellant_component, state) <=
                propellant_component.minimum_mass_kg + 1.0e-12)
            {
                continue;
            }

            // Profiles use time since this thruster's ignition, not absolute ephemeris time.
            const double local_time =
                elapsed_time_seconds - ignition_elapsed_time;

            double thrust = 0.0;
            double specific_impulse = 0.0;
            double thrust_rate = 0.0;
            double specific_impulse_rate = 0.0;
            double mass_flow_derivative = 0.0;
            if (thruster.mode == ThrusterMode::PrescribedProfile)
            {
                // TG-1 Eqs. (82)-(85): prescribed performance is a known
                // ignition-relative history and is independent of controller output.
                thrust = thruster.thrust_profile_n.samples.empty()
                    ? thruster.constant_thrust_n
                    : thruster.thrust_profile_n.ValueAt(local_time);
                specific_impulse =
                    thruster.specific_impulse_profile_seconds.samples.empty()
                        ? thruster.constant_specific_impulse_seconds
                        : thruster.specific_impulse_profile_seconds.ValueAt(local_time);
                thrust_rate = thruster.thrust_profile_n.samples.empty()
                    ? 0.0
                    : thruster.thrust_profile_n.RateAt(local_time);
                specific_impulse_rate =
                    thruster.specific_impulse_profile_seconds.samples.empty()
                        ? 0.0
                        : thruster.specific_impulse_profile_seconds.RateAt(
                            local_time);
            }
            else
            {
                // Simplified TG-1 Eqs. (79)-(81):
                // T = T_max clamp(u,0,1), while Isp is supplied by the same
                // user-controller evaluation that supplied u.
                const double throttle =
                    thruster_index < command.thruster_throttles.size()
                        ? std::clamp(
                            command.thruster_throttles[thruster_index],
                            0.0, 1.0)
                        : 0.0;
                thrust = thruster.maximum_thrust_n * throttle;
                specific_impulse =
                    thruster_index <
                        command.thruster_specific_impulses_seconds.size()
                    ? command.thruster_specific_impulses_seconds[thruster_index]
                    : 0.0;
                if (thruster_index <
                        command.thruster_mass_flow_derivatives_kgps2.size() &&
                    command.thruster_mass_flow_derivatives_kgps2[
                        thruster_index].has_value() &&
                    std::isfinite(*command.
                        thruster_mass_flow_derivatives_kgps2[thruster_index]))
                {
                    mass_flow_derivative = *command.
                        thruster_mass_flow_derivatives_kgps2[thruster_index];
                }
            }

            if (!std::isfinite(thrust) || thrust < 0.0) continue;
            // A finite positive Isp is also required at a zero-thrust endpoint
            // where a smooth prescribed ramp can have nonzero q_dot.
            if (!std::isfinite(specific_impulse) ||
                specific_impulse <= 0.0)
            {
                continue;
            }

            if (thruster.mode == ThrusterMode::PrescribedProfile)
            {
                mass_flow_derivative =
                    (thrust_rate / specific_impulse -
                        thrust * specific_impulse_rate /
                            (specific_impulse * specific_impulse)) /
                    kStandardGravityMps2;
                if (!std::isfinite(mass_flow_derivative))
                    mass_flow_derivative = 0.0;
            }

            result.thruster_mass_flow_derivatives_kgps2[thruster_index] =
                mass_flow_derivative;
            result.component_mass_second_derivatives_kgps2[
                propellant_mass_state_index] -= mass_flow_derivative;

            // Preserve a valid one-sided derivative at T=0 without creating
            // force or first-order mass consumption at that instant.
            if (thrust == 0.0) continue;

            result.thruster_thrusts_n[thruster_index] = thrust;
            const ComponentPose& mount_pose = component_poses[thruster.component_index];
            // Report Eq. 75: e_B,T = R_B,component(eta) e_component,T.
            const Vec3d direction_body = ComponentKinematicsModel::DirectionToBody(
                mount_pose, thruster.direction_component).Normalized();
            // Thruster force: F_T,B = T * e_T,B.
            const Vec3d force_body = thrust * direction_body;
            const Vec3d force_icrf = state.attitude_body_to_icrf.Rotate(force_body);
            // Articulated mount position: r_B,T = s_B,component(eta) + R_B,component(eta) r_component,T.
            const Vec3d application_point_body = ComponentKinematicsModel::PointToBody(
                mount_pose, thruster.application_point_component_m);

            result.force_icrf_n += force_icrf;
            // Thruster torque about spacecraft CM: M_T = (r_T - r_CM) x F_T.
            result.torque_body_nm += Cross(application_point_body - center_of_mass_body_m, force_body);
            // Preserve the physical mount load for the multibody inward pass. The total
            // force/CM torque above and this node load describe the same thrust; the former
            // is later subtracted from the equivalent root wrench to prevent double counting.
            ComponentLoad& component_load = result.component_loads[thruster.component_index];
            component_load.force_icrf_n += force_icrf;
            const Vec3d force_component = thrust * thruster.direction_component.Normalized();
            component_load.torque_about_component_origin_component_nm +=
                Cross(thruster.application_point_component_m, force_component);

            // TG-1 Eqs. (78), (81), and (85):
            // m_dot_propellant = T/(Isp*g0), so spacecraft m_dot is negative.
            const double mass_flow =
                thrust / (specific_impulse * kStandardGravityMps2);
            result.thruster_mass_flows_kgps[thruster_index] = mass_flow;
            // The spacecraft total and the owning component state receive the same
            // physical depletion once; they are two representations used downstream.
            result.mass_rate_kgps -= mass_flow;
            result.component_mass_rates_kgps[
                propellant_mass_state_index] -= mass_flow;
        }
        return result;
    }
}
