// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Simulation/SimulationConfigBuilder.h"

// Converts direct caller inputs into a normalized, self-consistent run configuration.

#include "TGSim/Environment/IEphemerisProvider.h"
#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace tgsim
{
    SimulationConfig SimulationConfigBuilder::Build(const SimulationRequest& request) const
    {
        // Convert the public request into the normalized, index-consistent configuration
        // consumed by validation, force models, ABA, RK4, and StateRecorder. This is setup
        // only: no controller, force model, or equation of motion is evaluated here.
        SimulationConfig config;
        config.scenario_name = request.scenario_name;
        config.simulation_kind = request.simulation_kind;
        config.mass_flow_convention = request.mass_flow_convention;
        config.initial_state = request.initial_state;
        // The independent time state starts at the requested absolute ephemeris time.
        config.initial_state.ephemeris_time_tdb_seconds =
            request.start_ephemeris_time_tdb_seconds;
        config.initial_state.elapsed_time_seconds = 0.0;
        // Integrators require a unit quaternion, but retain an invalid zero input so
        // validation can report it instead of silently replacing it with identity.
        if (config.initial_state.attitude_body_to_icrf.Norm() > 1.0e-15)
        {
            config.initial_state.attitude_body_to_icrf =
                config.initial_state.attitude_body_to_icrf.Normalized();
        }
        config.vehicle = request.vehicle;
        config.gravity = request.gravity;
        // SPICE owns point-mass GM and physical shape radius. Uploaded harmonic
        // model GM/R remain untouched because they belong to the coefficient file.
        if (config.gravity.ephemeris_provider)
        {
            for (GravityBody& body : config.gravity.bodies)
            {
                // A failed provider lookup must not silently use caller-side
                // placeholders for SPICE metadata.
                body.gravitational_parameter_m3ps2 = 0.0;
                body.reference_radius_m = 0.0;
                BodyGravityMetadata metadata;
                if (config.gravity.ephemeris_provider->
                    TryGetBodyGravityMetadata(body.name, metadata))
                {
                    body.gravitational_parameter_m3ps2 =
                        metadata.gravitational_parameter_m3ps2;
                    body.reference_radius_m = metadata.reference_radius_m;
                }
            }
        }
        // Explicit physical-member selection always wins over an explicitly
        // selected barycenter from the same system. The HUD should already
        // make these choices mutually exclusive; this normalization is the
        // backend safety net that prevents double-counting.
        std::unordered_map<std::string, bool> system_has_enabled_member;
        for (const GravityBody& body : config.gravity.bodies)
        {
            if (body.gravity_source_role == GravitySourceRole::SystemMember &&
                body.gravity_enabled)
            {
                system_has_enabled_member[body.gravity_system_name] = true;
            }
        }
        for (GravityBody& body : config.gravity.bodies)
        {
            if (body.gravity_source_role == GravitySourceRole::SystemBarycenter &&
                system_has_enabled_member[body.gravity_system_name])
            {
                body.gravity_enabled = false;
            }
        }
        config.solar_radiation = request.solar_radiation;
        config.atmosphere = request.atmosphere;
        config.aerodynamics = request.aerodynamics;
        config.control = request.control;

        config.solver.integrator_kind = request.integrator_kind;
        config.solver.start_ephemeris_time_tdb_seconds =
            request.start_ephemeris_time_tdb_seconds;
        config.solver.final_ephemeris_time_tdb_seconds =
            request.final_ephemeris_time_tdb_seconds;
        config.solver.requested_duration_seconds =
            request.requested_duration_seconds;
        if (std::isfinite(config.solver.requested_duration_seconds))
        {
            config.solver.final_ephemeris_time_tdb_seconds =
                config.solver.start_ephemeris_time_tdb_seconds +
                config.solver.requested_duration_seconds;
        }
        config.solver.maximum_integrator_step_seconds = request.maximum_integrator_step_seconds;
        config.solver.initial_integrator_step_seconds = request.initial_integrator_step_seconds;
        config.solver.absolute_tolerance = request.absolute_tolerance;
        config.solver.relative_tolerance = request.relative_tolerance;
        config.solver.output_step_seconds = request.output_step_seconds;
        config.solver.output_mode = request.output_mode;
        config.solver.maximum_integration_steps = request.maximum_integration_steps;
        config.solver.maximum_output_samples = request.maximum_output_samples;

        // Canonicalize both time representations once. TGSCN compilation supplies
        // exact elapsed boundaries; direct callers may still supply only ET.
        for (ThrusterDefinition& thruster : config.vehicle.thrusters)
        {
            thruster.ignition_elapsed_time_seconds =
                ThrusterIgnitionElapsedTime(
                    thruster,
                    config.solver.start_ephemeris_time_tdb_seconds);
            thruster.shutdown_elapsed_time_seconds =
                ThrusterShutdownElapsedTime(
                    thruster,
                    config.solver.start_ephemeris_time_tdb_seconds);
            // Keep compatibility metadata synchronized. These absolute values may
            // intentionally coincide when elapsed spacing is below the ET ULP.
            thruster.ignition_ephemeris_time_tdb_seconds =
                config.solver.start_ephemeris_time_tdb_seconds +
                thruster.ignition_elapsed_time_seconds;
            thruster.shutdown_ephemeris_time_tdb_seconds =
                std::isinf(thruster.shutdown_elapsed_time_seconds)
                    ? std::numeric_limits<double>::infinity()
                    : config.solver.start_ephemeris_time_tdb_seconds +
                        thruster.shutdown_elapsed_time_seconds;
        }

        // Build the flattened mass-state storage. variable_mass_state_index is the contract
        // shared by components, propulsion output, RK4, and MassPropertiesModel.
        std::size_t variable_mass_count = config.initial_state.variable_component_masses_kg.size();
        for (const ComponentDefinition& component : config.vehicle.components)
        {
            if (component.variable_mass_state_index != kInvalidIndex)
                variable_mass_count = std::max(variable_mass_count, component.variable_mass_state_index + 1);
        }
        config.initial_state.variable_component_masses_kg.resize(variable_mass_count, 0.0);
        // Caller-provided positive masses win. Missing/nonpositive entries start from the
        // corresponding component's configured initial mass.
        for (const ComponentDefinition& component : config.vehicle.components)
        {
            if (component.variable_mass_state_index < variable_mass_count &&
                config.initial_state.variable_component_masses_kg[component.variable_mass_state_index] <= 0.0)
            {
                config.initial_state.variable_component_masses_kg[component.variable_mass_state_index] = component.initial_mass_kg;
            }
        }

        // Flatten every child's ordered joint DOFs into eta and eta_dot state vectors.
        // Component order is also hierarchy order, so each component owns one contiguous
        // range beginning at articulation_state_offset. ABA uses that same index to return
        // eta_ddot to the correct RK4 state entry.
        const std::size_t requested_coordinate_count = config.initial_state.articulation_coordinates.size();
        const std::size_t requested_rate_count = config.initial_state.articulation_rates.size();
        std::size_t articulation_count = 0;
        for (ComponentDefinition& component : config.vehicle.components)
        {
            const std::size_t dof_count = component.articulation_to_parent.dofs.size();
            component.articulation_state_offset = dof_count > 0 ? articulation_count : kInvalidIndex;
            articulation_count += dof_count;
        }
        config.initial_state.articulation_coordinates.resize(articulation_count, 0.0);
        config.initial_state.articulation_rates.resize(articulation_count, 0.0);
        // Preserve coordinates/rates explicitly supplied by the caller; fill only newly
        // created entries from each DOF's initial values. Coordinates are brought onto
        // their admissible interval here; velocity-limit corrections are applied later
        // by SimulationEngine through the coupled articulated inertia.
        for (const ComponentDefinition& component : config.vehicle.components)
        {
            if (component.articulation_state_offset == kInvalidIndex) continue;
            for (std::size_t local_index = 0; local_index < component.articulation_to_parent.dofs.size(); ++local_index)
            {
                const ArticulationDof& dof = component.articulation_to_parent.dofs[local_index];
                const std::size_t state_index = component.articulation_state_offset + local_index;
                if (state_index >= requested_coordinate_count)
                    config.initial_state.articulation_coordinates[state_index] = dof.initial_coordinate;
                if (state_index >= requested_rate_count)
                    config.initial_state.articulation_rates[state_index] = dof.initial_rate;

                if (dof.limits.minimum_coordinate <= dof.limits.maximum_coordinate)
                {
                    config.initial_state.articulation_coordinates[state_index] = std::clamp(
                        config.initial_state.articulation_coordinates[state_index],
                        dof.limits.minimum_coordinate,
                        dof.limits.maximum_coordinate);
                }
            }
        }

        // Give each configured wheel one propagated h state. As above, caller values are
        // preserved and only missing entries receive the wheel's configured initial h.
        config.initial_state.internal_angular_momenta_nms.resize(config.vehicle.reaction_wheels.size(), 0.0);
        for (std::size_t index = 0; index < config.vehicle.reaction_wheels.size(); ++index)
        {
            if (index >= request.initial_state.internal_angular_momenta_nms.size())
                config.initial_state.internal_angular_momenta_nms[index] = config.vehicle.reaction_wheels[index].initial_momentum_nms;
        }

        // Total mass is algebraic: derive it from component states so state.mass_kg cannot
        // disagree with the component tree at the first ODE evaluation.
        if (!config.vehicle.components.empty())
            config.initial_state.mass_kg = MassPropertiesModel().Compute(
                config.vehicle, config.initial_state).mass_kg;
        return config;
    }
}
