// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Dynamics/General6DofDynamics.h"

// Central right-hand-side assembly called at every RK4 stage. It converts the current
// state into actuator commands, mass properties, external loads, floating-base/joint
// accelerations, and finally the complete derivative consumed by the integrator.

#include "TGSim/Control/IController.h"
#include "TGSim/Dynamics/FloatingBaseTreeDynamics.h"
#include "TGSim/Environment/EnvironmentModels.h"
#include "TGSim/Forces/AerodynamicsModel.h"
#include "TGSim/Forces/GravityModel.h"
#include "TGSim/Forces/PropulsionModel.h"
#include "TGSim/Forces/SolarRadiationPressureModel.h"
#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>
#include <vector>

namespace tgsim
{
    General6DofDynamics::General6DofDynamics(const SimulationConfig& config)
        : solar_radiation_model_(config.solar_radiation)
        , aerodynamics_model_(config.aerodynamics)
    {
    }

    DynamicsEvaluation General6DofDynamics::ComputeDynamics(
        double ephemeris_time_tdb_seconds,
        const SpacecraftState& state,
        const SimulationConfig& config) const
    {
        // Return f(t,y)=dy/dt plus diagnostics at exactly the supplied state. This function
        // performs the physics evaluation only; FixedStepRK4 decides how those slopes update y.
        DynamicsEvaluation result;
        // Integrators keep both clocks synchronized. Direct RHS callers may
        // deliberately evaluate an existing state at another supplied epoch,
        // in which case preserve the public time-argument behavior.
        const double elapsed_time_seconds =
            ephemeris_time_tdb_seconds ==
                state.ephemeris_time_tdb_seconds
                ? state.elapsed_time_seconds
                : ephemeris_time_tdb_seconds -
                    config.solver.start_ephemeris_time_tdb_seconds;
        // Resolve the state/status information before calling user control code.
        const std::size_t articulation_count = std::max(
            state.articulation_coordinates.size(), state.articulation_rates.size());
        // effective_articulation_rates is the rate used for this derivative evaluation.
        // Ideal speed caps are respected without mutating the caller's state object.
        // Coordinate impacts are located and resolved by SimulationEngine; suppressing
        // the incoming rate inside an RK stage would delay the physical impact time.
        std::vector<double> effective_articulation_rates = state.articulation_rates;
        effective_articulation_rates.resize(articulation_count, 0.0);
        std::vector<JointControlState> joint_states;
        joint_states.reserve(articulation_count);
        constexpr double limit_tolerance = 1.0e-12;
        for (std::size_t component_index = 0;
            component_index < config.vehicle.components.size();
            ++component_index)
        {
            const ComponentDefinition& component =
                config.vehicle.components[component_index];
            if (component.articulation_state_offset == kInvalidIndex) continue;
            for (std::size_t local_index = 0; local_index < component.articulation_to_parent.dofs.size(); ++local_index)
            {
                const std::size_t state_index = component.articulation_state_offset + local_index;
                if (state_index >= articulation_count) continue;

                const ArticulationDof& dof = component.articulation_to_parent.dofs[local_index];
                const ArticulationLimits& limits = dof.limits;
                // Enforce |eta_dot| <= eta_dot_max before kinematics sees the state.
                effective_articulation_rates[state_index] = std::clamp(
                    effective_articulation_rates[state_index],
                    -limits.maximum_absolute_rate,
                    limits.maximum_absolute_rate);

                const double coordinate = state_index < state.articulation_coordinates.size()
                    ? state.articulation_coordinates[state_index]
                    : dof.initial_coordinate;
                const bool at_lower_limit = coordinate <= limits.minimum_coordinate + limit_tolerance;
                const bool at_upper_limit = coordinate >= limits.maximum_coordinate - limit_tolerance;
                joint_states.push_back({
                    state_index,
                    component_index,
                    local_index,
                    coordinate,
                    effective_articulation_rates[state_index],
                    at_lower_limit,
                    at_upper_limit});
            }
        }

        // Current geometry and mass properties are control inputs and are also needed
        // later for articulated thruster direction and moment arms.
        const MassPropertiesModel mass_model;
        const MassProperties initial_mass_properties = mass_model.Compute(config.vehicle, state);

        std::vector<ComponentControlState> component_states;
        component_states.reserve(config.vehicle.components.size());
        for (std::size_t index = 0;
            index < config.vehicle.components.size();
            ++index)
        {
            const ComponentPose& pose =
                initial_mass_properties.component_poses[index];
            component_states.push_back({
                index,
                MassPropertiesModel::ComponentMass(
                    config.vehicle.components[index], state),
                pose.origin_body_m,
                pose.component_to_body});
        }

        std::vector<ThrusterControlState> thruster_states;
        thruster_states.reserve(config.vehicle.thrusters.size());
        for (std::size_t index = 0;
            index < config.vehicle.thrusters.size();
            ++index)
        {
            const ThrusterDefinition& thruster =
                config.vehicle.thrusters[index];
            ThrusterControlState status;
            status.thruster_index = index;
            const double ignition_elapsed_time =
                ThrusterIgnitionElapsedTime(
                    thruster,
                    config.solver.start_ephemeris_time_tdb_seconds);
            const double shutdown_elapsed_time =
                ThrusterShutdownElapsedTime(
                    thruster,
                    config.solver.start_ephemeris_time_tdb_seconds);
            status.firing_window_open =
                elapsed_time_seconds >= ignition_elapsed_time &&
                elapsed_time_seconds < shutdown_elapsed_time;
            status.has_propellant_component =
                thruster.propellant_component_index <
                    config.vehicle.components.size();
            if (status.has_propellant_component)
            {
                const ComponentDefinition& propellant_component =
                    config.vehicle.components[
                        thruster.propellant_component_index];
                status.current_propellant_component_mass_kg =
                    MassPropertiesModel::ComponentMass(
                        propellant_component, state);
                status.propellant_available =
                    status.current_propellant_component_mass_kg >
                    propellant_component.minimum_mass_kg + 1.0e-12;
            }
            thruster_states.push_back(status);
        }

        std::vector<ReactionWheelControlState> wheel_states;
        wheel_states.reserve(config.vehicle.reaction_wheels.size());
        for (std::size_t index = 0;
            index < config.vehicle.reaction_wheels.size();
            ++index)
        {
            const double momentum =
                index < state.internal_angular_momenta_nms.size()
                    ? state.internal_angular_momenta_nms[index]
                    : 0.0;
            const double limit =
                config.vehicle.reaction_wheels[index].maximum_momentum_nms;
            wheel_states.push_back({
                index,
                momentum,
                limit,
                momentum <= -limit,
                momentum >= limit});
        }

        // The backend owns output construction. With no controller, every command
        // stays zero. The user's function cannot resize or misalign command arrays.
        ControlCommandWriter command_writer(
            config.vehicle.thrusters.size(),
            articulation_count,
            config.vehicle.reaction_wheels.size());
        if (config.control.controller)
        {
            // Celestial states are controller inputs only. Avoid resolving every
            // catalog body at every integrator stage when no controller is active.
            std::vector<CelestialBodyControlState> celestial_body_states;
            celestial_body_states.reserve(config.gravity.bodies.size());
            for (const GravityBody& body : config.gravity.bodies)
            {
                const BodyState body_state = ResolveBodyState(
                    body, config.gravity, ephemeris_time_tdb_seconds);
                celestial_body_states.push_back({
                    body.name,
                    body_state.position_icrf_m,
                    body_state.velocity_icrf_mps,
                    ResolveBodyFixedToIcrf(
                        body, config.gravity, ephemeris_time_tdb_seconds)});
            }

            const ControlInput control_input{
                ephemeris_time_tdb_seconds,
                elapsed_time_seconds,
                state,
                config,
                initial_mass_properties.center_of_mass_body_m,
                initial_mass_properties.inertia_body_kgm2,
                component_states,
                thruster_states,
                joint_states,
                wheel_states,
                celestial_body_states};
            config.control.controller->ComputeControl(
                control_input, command_writer);
        }
        ControlCommand command = command_writer.Command();

        // Clamp generalized efforts to physical actuator limits.
        for (const JointControlState& joint : joint_states)
        {
            const ArticulationDof& dof =
                config.vehicle.components[joint.child_component_index].
                    articulation_to_parent.dofs[joint.local_dof_index];
            command.joint_efforts[joint.articulation_state_index] = std::clamp(
                command.joint_efforts[joint.articulation_state_index],
                -dof.limits.maximum_absolute_effort,
                dof.limits.maximum_absolute_effort);
        }

        // Suppress only a wheel command that would increase saturation. A command
        // directed back toward zero remains valid.
        for (const ReactionWheelControlState& wheel : wheel_states)
        {
            double& momentum_rate =
                command.wheel_momentum_rates_nm[wheel.wheel_index];
            if ((wheel.saturated_positive && momentum_rate > 0.0) ||
                (wheel.saturated_negative && momentum_rate < 0.0))
            {
                momentum_rate = 0.0;
            }
        }

        const PropulsionEvaluation propulsion = PropulsionModel().ComputeLoads(
            ephemeris_time_tdb_seconds, state, config, command,
            initial_mass_properties.center_of_mass_body_m,
            initial_mass_properties.component_poses);
        result.thruster_thrusts_n = propulsion.thruster_thrusts_n;

        // Use one actual-rate evaluation for instantaneous mass properties, complete
        // inertia derivatives, and geometric-CM redistribution kinematics.
        const MassProperties mass_properties = mass_model.Compute(
            config.vehicle,
            state,
            propulsion.component_mass_rates_kgps,
            effective_articulation_rates);
        result.center_of_mass_body_m = mass_properties.center_of_mass_body_m;
        result.inertia_body_kgm2 = mass_properties.inertia_body_kgm2;
        result.component_origins_body_m.reserve(
            mass_properties.component_poses.size());
        result.component_to_body_rotations.reserve(
            mass_properties.component_poses.size());
        for (const ComponentPose& pose : mass_properties.component_poses)
        {
            result.component_origins_body_m.push_back(pose.origin_body_m);
            result.component_to_body_rotations.push_back(pose.component_to_body);
        }

        // Evaluate all environmental loads against the same final mass geometry and RK-stage
        // state. This keeps total CM moments and component application points consistent.
        const GravityEvaluation gravity = GravityModel().ComputeGravity(
            ephemeris_time_tdb_seconds, state, config,
            mass_properties.center_of_mass_body_m,
            mass_properties.component_poses);
        const SolarRadiationEvaluation solar = solar_radiation_model_.ComputeLoads(
            ephemeris_time_tdb_seconds, state, config,
            mass_properties.center_of_mass_body_m, mass_properties.component_poses);
        const AerodynamicsEvaluation aerodynamics = aerodynamics_model_.ComputeLoads(
            ephemeris_time_tdb_seconds, state, config,
            mass_properties.center_of_mass_body_m);

        // First retain each named contribution for telemetry, then form the two totals used
        // by Newton translation and the equivalent-root portion of multibody dynamics.
        ForceTorqueSample& loads = result.applied_force_torque;
        loads.gravity_force_icrf_n = gravity.force_icrf_n;
        loads.thrust_force_icrf_n = propulsion.force_icrf_n;
        loads.solar_radiation_force_icrf_n = solar.force_icrf_n;
        loads.aerodynamic_force_icrf_n = aerodynamics.force_icrf_n;
        loads.gravity_torque_body_nm = gravity.torque_about_cm_body_nm;
        loads.thrust_torque_body_nm = propulsion.torque_body_nm;
        loads.solar_radiation_torque_body_nm = solar.torque_body_nm;
        loads.aerodynamic_torque_body_nm = aerodynamics.torque_body_nm;
        loads.control_torque_body_nm = command.external_torque_body_nm;
        // Net force: F = F_g + F_T + F_SRP + F_aero.
        loads.force_icrf_n = loads.gravity_force_icrf_n + loads.thrust_force_icrf_n +
            loads.solar_radiation_force_icrf_n + loads.aerodynamic_force_icrf_n;
        // Net external torque: M = M_g + M_T + M_SRP + M_aero + M_control.
        loads.torque_body_nm = loads.gravity_torque_body_nm +
            loads.thrust_torque_body_nm + loads.solar_radiation_torque_body_nm +
            loads.aerodynamic_torque_body_nm + loads.control_torque_body_nm;
        loads.mass_rate_kgps = propulsion.mass_rate_kgps;
        loads.visible_sun_fraction = solar.visible_sun_fraction;
        loads.aerodynamics_outside_validity = aerodynamics.outside_validity;
        loads.aerodynamic_dynamic_pressure_pa = aerodynamics.dynamic_pressure_pa;
        loads.aerodynamic_molecular_speed_ratio =
            aerodynamics.molecular_speed_ratio;
        loads.aerodynamic_knudsen_number = aerodynamics.knudsen_number;
        loads.aerodynamic_force_coefficients_body =
            aerodynamics.force_coefficients_body;
        loads.aerodynamic_moment_coefficients_body_about_cm =
            aerodynamics.moment_coefficients_body_about_cm;
        loads.aerodynamic_database_used = aerodynamics.used_coefficient_database;
        loads.aerodynamic_fallback_used = aerodynamics.used_fallback_model;

        StateDerivative& derivative = result.derivative;
        // position and velocity are the geometric total-spacecraft CM state.
        derivative.position_rate_mps = state.velocity_icrf_mps;
        // Attitude kinematics: q_dot = 1/2 q (x) [0, omega_body].
        derivative.attitude_rate = QuaternionRateBodyToInertial(
            state.attitude_body_to_icrf, state.angular_velocity_body_radps);
        // Mass, component masses, wheel momenta, and eta are direct first-order state
        // equations; their rates are already known once propulsion/controller limits are evaluated.
        derivative.mass_rate_kgps = propulsion.mass_rate_kgps;
        derivative.variable_component_mass_rates_kgps = propulsion.component_mass_rates_kgps;
        // Generalized joint kinematics: eta_dot is known here; eta_ddot is filled by ABA below.
        derivative.articulation_coordinate_rates = effective_articulation_rates;
        derivative.articulation_accelerations.assign(articulation_count, 0.0);
        derivative.internal_angular_momentum_rates_nm = command.wheel_momentum_rates_nm;

        const double mass = mass_properties.mass_kg > 0.0 ? mass_properties.mass_kg : state.mass_kg;
        if (mass > 0.0)
        {
            // Assemble geometric-CM kinematics in body axes. For a component
            // centroid or nozzle point P, u_P is v_P-v_CM without subtracting
            // large absolute inertial velocities.
            const Vec3d omega_body = state.angular_velocity_body_radps;
            const Vec3d center_of_mass_body =
                mass_properties.center_of_mass_body_m;
            const Vec3d center_of_mass_rate_body =
                mass_properties.center_of_mass_rate_body_mps;
            const auto relative_point_velocity_body =
                [&](const ComponentPose& pose, const Vec3d& point_component)
                {
                    const Vec3d point_body =
                        ComponentKinematicsModel::PointToBody(
                            pose, point_component);
                    const Vec3d rho_body =
                        point_body - center_of_mass_body;
                    return Cross(omega_body, rho_body) +
                        ComponentKinematicsModel::PointRateToBody(
                            pose, point_component) -
                        center_of_mass_rate_body;
                };

            Vec3d mass_second_derivative_moment_body;
            Vec3d mass_rate_velocity_body;
            for (std::size_t component_index = 0;
                component_index < config.vehicle.components.size() &&
                component_index < mass_properties.component_poses.size();
                ++component_index)
            {
                const ComponentDefinition& component =
                    config.vehicle.components[component_index];
                const std::size_t mass_index =
                    component.variable_mass_state_index;
                if (mass_index == kInvalidIndex) continue;

                const double mass_rate = mass_index <
                        propulsion.component_mass_rates_kgps.size()
                    ? propulsion.component_mass_rates_kgps[mass_index]
                    : 0.0;
                const double mass_second_derivative = mass_index <
                        propulsion.
                            component_mass_second_derivatives_kgps2.size()
                    ? propulsion.
                        component_mass_second_derivatives_kgps2[mass_index]
                    : 0.0;
                const ComponentPose& pose =
                    mass_properties.component_poses[component_index];
                const Vec3d centroid_body =
                    ComponentKinematicsModel::PointToBody(
                        pose, component.center_of_mass_component_m);
                const Vec3d rho_body =
                    centroid_body - center_of_mass_body;
                mass_second_derivative_moment_body +=
                    mass_second_derivative * rho_body;
                mass_rate_velocity_body += mass_rate *
                    relative_point_velocity_body(
                        pose, component.center_of_mass_component_m);
            }

            Vec3d nozzle_carrier_velocity_body;
            for (std::size_t thruster_index = 0;
                thruster_index < config.vehicle.thrusters.size() &&
                thruster_index < propulsion.thruster_mass_flows_kgps.size();
                ++thruster_index)
            {
                const double mass_flow =
                    propulsion.thruster_mass_flows_kgps[thruster_index];
                const ThrusterDefinition& thruster =
                    config.vehicle.thrusters[thruster_index];
                if (mass_flow == 0.0 ||
                    thruster.component_index >=
                        mass_properties.component_poses.size())
                {
                    continue;
                }
                nozzle_carrier_velocity_body += mass_flow *
                    relative_point_velocity_body(
                        mass_properties.component_poses[
                            thruster.component_index],
                        thruster.application_point_component_m);
            }

            // Gravity is already an acceleration. Effective thrust and other
            // physical loads enter once through F/M; the remaining terms are
            // geometric-CM corrections, not additional applied forces.
            const Vec3d non_gravitational_force =
                loads.force_icrf_n - loads.gravity_force_icrf_n;
            const Vec3d center_of_mass_correction_body =
                (mass_second_derivative_moment_body +
                    mass_rate_velocity_body -
                    nozzle_carrier_velocity_body) / mass;
            derivative.velocity_rate_mps2 =
                gravity.acceleration_icrf_mps2 +
                non_gravitational_force / mass +
                state.attitude_body_to_icrf.Rotate(
                    center_of_mass_correction_body);
        }

        // Assemble the load distribution needed by ABA. Totals alone determine CM motion,
        // but joint/base accelerations also depend on which component receives each wrench.
        FloatingBaseTreeInput multibody_input;
        multibody_input.component_external_loads.resize(config.vehicle.components.size());
        // Sum propulsion, SRP, and aerodynamic entries component by component:
        // F_i = F_T,i + F_SRP,i + F_aero,i, and likewise for M_Oi.
        const auto add_component_loads = [&multibody_input](const std::vector<ComponentLoad>& component_loads)
        {
            const std::size_t count = std::min(
                multibody_input.component_external_loads.size(), component_loads.size());
            for (std::size_t index = 0; index < count; ++index)
                multibody_input.component_external_loads[index] += component_loads[index];
        };
        add_component_loads(propulsion.component_loads);
        add_component_loads(solar.component_loads);
        add_component_loads(aerodynamics.component_loads);
        add_component_loads(gravity.component_loads);

        // Reconstruct the spacecraft-level equivalent of all node-resolved loads:
        // F_dist = sum_i F_i,
        // M_CM,dist = sum_i [R_BCi M_Oi^Ci + (r_Oi-r_CM) x F_i^B].
        Vec3d distributed_force_icrf;
        Vec3d distributed_torque_about_cm_body;
        for (std::size_t component_index = 0;
            component_index < multibody_input.component_external_loads.size() &&
            component_index < mass_properties.component_poses.size();
            ++component_index)
        {
            const ComponentLoad& component_load = multibody_input.component_external_loads[component_index];
            const ComponentPose& pose = mass_properties.component_poses[component_index];
            const Vec3d force_body = state.attitude_body_to_icrf.InverseRotate(component_load.force_icrf_n);
            distributed_force_icrf += component_load.force_icrf_n;
            distributed_torque_about_cm_body +=
                pose.component_to_body * component_load.torque_about_component_origin_component_nm +
                Cross(pose.origin_body_m - mass_properties.center_of_mass_body_m, force_body);
        }
        // Body-level coefficient and control loads remain an equivalent wrench on B.
        // Component-resolved gravity, thruster, and facet loads are subtracted here
        // because ABA also receives them at their actual tree nodes. Thus:
        // root wrench = total spacecraft wrench - equivalent(component-node wrenches).
        multibody_input.external_force_icrf_n = loads.force_icrf_n - distributed_force_icrf;
        multibody_input.external_torque_about_cm_body_nm =
            loads.torque_body_nm - distributed_torque_about_cm_body;
        multibody_input.center_of_mass_body_m = mass_properties.center_of_mass_body_m;
        multibody_input.center_of_mass_mass_redistribution_rate_body_mps =
            mass_properties.
                center_of_mass_mass_redistribution_rate_body_mps;
        multibody_input.component_poses = mass_properties.component_poses;
        multibody_input.articulation_rates = effective_articulation_rates;
        multibody_input.applied_joint_efforts = command.joint_efforts;
        multibody_input.variable_component_mass_rates_kgps =
            propulsion.component_mass_rates_kgps;
        multibody_input.thruster_mass_flows_kgps =
            propulsion.thruster_mass_flows_kgps;
        multibody_input.wheel_momentum_rates_nm = command.wheel_momentum_rates_nm;
        // This is the instantaneous forward-dynamics solve: known state, wrenches, tau,
        // h_dot, and m_dot enter; unknown base acceleration and eta_ddot return.
        const FloatingBaseTreeEvaluation multibody =
            FloatingBaseTreeDynamics().SolveForwardDynamics(state, config, multibody_input);
        result.multibody_solve_succeeded = multibody.solved;
        result.base_origin_acceleration_body_mps2 =
            multibody.base_origin_acceleration_body_mps2;
        result.total_linear_momentum_icrf_kgmps = multibody.total_linear_momentum_icrf_kgmps;
        result.total_angular_momentum_about_cm_icrf_kgm2ps =
            multibody.total_angular_momentum_about_cm_icrf_kgm2ps;
        result.applied_joint_efforts = command.joint_efforts;
        result.joint_constraint_efforts = multibody.joint_constraint_efforts;
        if (multibody.solved)
        {
            // Forward ABA solves both the free-base acceleration and every eta_ddot
            // from known external wrenches and actuator efforts.
            derivative.angular_acceleration_body_radps2 = multibody.base_acceleration_body.angular;
            derivative.articulation_accelerations = multibody.joint_accelerations;
        }
        else
        {
            // A malformed/singular component model still receives the report's original
            // equivalent-rigid-body equation so one failed telemetry sample can be returned.
            const Vec3d rigid_body_momentum =
                mass_properties.inertia_body_kgm2 * state.angular_velocity_body_radps;
            // Keep this diagnostic in the tree solver's boosted observer so its
            // complete aggregate inertia rate and nozzle carrier flux are paired.
            // omega_dot = I^-1 [M - I_dot*omega - omega x (I*omega) - Phi_H,C].
            const Vec3d rotational_rhs = loads.torque_body_nm
                - mass_properties.inertia_rate_body_kgm2ps * state.angular_velocity_body_radps
                - Cross(state.angular_velocity_body_radps, rigid_body_momentum)
                - multibody.
                    nozzle_carrier_angular_momentum_flux_about_cm_solver_body_nm;
            derivative.angular_acceleration_body_radps2 =
                mass_properties.inertia_body_kgm2.Inverse() * rotational_rhs;
        }
        return result;
    }
}
