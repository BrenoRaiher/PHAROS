// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Integrators/FixedStepRK4.h"

// Advances the complete variable-length spacecraft state with classical explicit RK4.

#include "TGSim/Integrators/IntegratorUtilities.h"
#include "TGSim/Core/ISimulationObserver.h"

#include <algorithm>
#include <utility>

namespace tgsim
{
    namespace
    {
        SpacecraftState AddScaledDerivative(
            const SpacecraftState& state,
            const StateDerivative& derivative,
            double scale,
            double start_ephemeris_time_tdb_seconds)
        {
            // Construct one trial state y_trial = y + scale*f. RK4 uses this helper for
            // its midpoint/end stages and once more for the final weighted update; scale
            // has seconds, while each StateDerivative member carries the matching rate unit.
            SpacecraftState result = state;
            // Integrate the small mission clock and derive absolute ET from the fixed
            // run epoch. This avoids repeatedly adding small dt values to a large ET.
            result.elapsed_time_seconds += scale;
            result.ephemeris_time_tdb_seconds =
                start_ephemeris_time_tdb_seconds + result.elapsed_time_seconds;
            result.position_icrf_m += derivative.position_rate_mps * scale;
            result.velocity_icrf_mps += derivative.velocity_rate_mps2 * scale;
            result.attitude_body_to_icrf += derivative.attitude_rate * scale;
            result.angular_velocity_body_radps += derivative.angular_acceleration_body_radps2 * scale;
            result.mass_kg += derivative.mass_rate_kgps * scale;

            // Dynamic vectors may be absent in a caller state but present in its derivative.
            // Resize before element-wise integration; missing previous entries are zero.
            result.variable_component_masses_kg.resize(
                std::max(result.variable_component_masses_kg.size(), derivative.variable_component_mass_rates_kgps.size()), 0.0);
            for (std::size_t index = 0; index < derivative.variable_component_mass_rates_kgps.size(); ++index)
                result.variable_component_masses_kg[index] += derivative.variable_component_mass_rates_kgps[index] * scale;

            result.articulation_coordinates.resize(
                std::max(result.articulation_coordinates.size(), derivative.articulation_coordinate_rates.size()), 0.0);
            for (std::size_t index = 0; index < derivative.articulation_coordinate_rates.size(); ++index)
                result.articulation_coordinates[index] += derivative.articulation_coordinate_rates[index] * scale;

            result.articulation_rates.resize(
                std::max(result.articulation_rates.size(), derivative.articulation_accelerations.size()), 0.0);
            for (std::size_t index = 0; index < derivative.articulation_accelerations.size(); ++index)
                result.articulation_rates[index] += derivative.articulation_accelerations[index] * scale;

            result.internal_angular_momenta_nms.resize(
                std::max(result.internal_angular_momenta_nms.size(), derivative.internal_angular_momentum_rates_nm.size()), 0.0);
            for (std::size_t index = 0; index < derivative.internal_angular_momentum_rates_nm.size(); ++index)
                result.internal_angular_momenta_nms[index] += derivative.internal_angular_momentum_rates_nm[index] * scale;
            return result;
        }

        StateDerivative WeightedAverage(
            const StateDerivative& k1,
            const StateDerivative& k2,
            const StateDerivative& k3,
            const StateDerivative& k4)
        {
            // Collapse four RHS evaluations into the effective slope used for the completed
            // step. Discontinuous articulation limits remain scheduler-owned events.
            StateDerivative result;
            // Classical RK4 slope: k = (k1 + 2 k2 + 2 k3 + k4) / 6.
            result.position_rate_mps = (k1.position_rate_mps + 2.0 * k2.position_rate_mps + 2.0 * k3.position_rate_mps + k4.position_rate_mps) / 6.0;
            result.velocity_rate_mps2 = (k1.velocity_rate_mps2 + 2.0 * k2.velocity_rate_mps2 + 2.0 * k3.velocity_rate_mps2 + k4.velocity_rate_mps2) / 6.0;
            result.attitude_rate = (k1.attitude_rate + 2.0 * k2.attitude_rate + 2.0 * k3.attitude_rate + k4.attitude_rate) * (1.0 / 6.0);
            result.angular_acceleration_body_radps2 = (k1.angular_acceleration_body_radps2 + 2.0 * k2.angular_acceleration_body_radps2 + 2.0 * k3.angular_acceleration_body_radps2 + k4.angular_acceleration_body_radps2) / 6.0;
            result.mass_rate_kgps = (k1.mass_rate_kgps + 2.0 * k2.mass_rate_kgps + 2.0 * k3.mass_rate_kgps + k4.mass_rate_kgps) / 6.0;

            // Use the largest stage size for each dynamic vector. value_at below treats a
            // missing stage entry as zero, matching AddScaledDerivative's resize semantics.
            const std::size_t component_count = std::max({
                k1.variable_component_mass_rates_kgps.size(), k2.variable_component_mass_rates_kgps.size(),
                k3.variable_component_mass_rates_kgps.size(), k4.variable_component_mass_rates_kgps.size()});
            result.variable_component_mass_rates_kgps.resize(component_count, 0.0);
            const std::size_t articulation_coordinate_count = std::max({
                k1.articulation_coordinate_rates.size(), k2.articulation_coordinate_rates.size(),
                k3.articulation_coordinate_rates.size(), k4.articulation_coordinate_rates.size()});
            result.articulation_coordinate_rates.resize(articulation_coordinate_count, 0.0);
            const std::size_t articulation_rate_count = std::max({
                k1.articulation_accelerations.size(), k2.articulation_accelerations.size(),
                k3.articulation_accelerations.size(), k4.articulation_accelerations.size()});
            result.articulation_accelerations.resize(articulation_rate_count, 0.0);
            const std::size_t wheel_count = std::max({
                k1.internal_angular_momentum_rates_nm.size(), k2.internal_angular_momentum_rates_nm.size(),
                k3.internal_angular_momentum_rates_nm.size(), k4.internal_angular_momentum_rates_nm.size()});
            result.internal_angular_momentum_rates_nm.resize(wheel_count, 0.0);

            // Safe scalar access lets all four stage vectors participate even if a custom
            // dynamics model returns shorter optional state derivatives.
            const auto value_at = [](const std::vector<double>& values, std::size_t index)
            {
                return index < values.size() ? values[index] : 0.0;
            };
            for (std::size_t index = 0; index < component_count; ++index)
                result.variable_component_mass_rates_kgps[index] = (value_at(k1.variable_component_mass_rates_kgps, index) +
                    2.0 * value_at(k2.variable_component_mass_rates_kgps, index) +
                    2.0 * value_at(k3.variable_component_mass_rates_kgps, index) +
                    value_at(k4.variable_component_mass_rates_kgps, index)) / 6.0;
            for (std::size_t index = 0; index < articulation_coordinate_count; ++index)
                result.articulation_coordinate_rates[index] = (value_at(k1.articulation_coordinate_rates, index) +
                    2.0 * value_at(k2.articulation_coordinate_rates, index) +
                    2.0 * value_at(k3.articulation_coordinate_rates, index) +
                    value_at(k4.articulation_coordinate_rates, index)) / 6.0;
            for (std::size_t index = 0; index < articulation_rate_count; ++index)
                result.articulation_accelerations[index] = (value_at(k1.articulation_accelerations, index) +
                    2.0 * value_at(k2.articulation_accelerations, index) +
                    2.0 * value_at(k3.articulation_accelerations, index) +
                    value_at(k4.articulation_accelerations, index)) / 6.0;
            for (std::size_t index = 0; index < wheel_count; ++index)
                result.internal_angular_momentum_rates_nm[index] = (value_at(k1.internal_angular_momentum_rates_nm, index) +
                    2.0 * value_at(k2.internal_angular_momentum_rates_nm, index) +
                    2.0 * value_at(k3.internal_angular_momentum_rates_nm, index) +
                    value_at(k4.internal_angular_momentum_rates_nm, index)) / 6.0;
            return result;
        }
    }

    IntegrationAdvance FixedStepRK4::Advance(
        const IDynamicsModel& dynamics_model,
        const SimulationConfig& config,
        const SpacecraftState& state,
        double maximum_elapsed_seconds,
        const ISimulationObserver* observer) const
    {
        if (observer && observer->IsCancellationRequested())
            return {state, false, "Simulation cancelled.", 0};
        // Advance one SimulationEngine step. Every ComputeDynamics call below recomputes all
        // state-dependent physics, including articulated poses and floating-base ABA.
        // k1 = f(t, y)
        const StateDerivative k1 = dynamics_model.ComputeDynamics(
            state.ephemeris_time_tdb_seconds, state, config).derivative;
        // k2 = f(t + dt/2, y + dt*k1/2)
        const SpacecraftState state2 = AddScaledDerivative(
            state, k1, 0.5 * maximum_elapsed_seconds,
            config.solver.start_ephemeris_time_tdb_seconds);
        const StateDerivative k2 = dynamics_model.ComputeDynamics(
            state2.ephemeris_time_tdb_seconds, state2, config).derivative;
        if (observer && observer->IsCancellationRequested())
            return {state, false, "Simulation cancelled.", 1};
        // k3 = f(t + dt/2, y + dt*k2/2)
        const SpacecraftState state3 = AddScaledDerivative(
            state, k2, 0.5 * maximum_elapsed_seconds,
            config.solver.start_ephemeris_time_tdb_seconds);
        const StateDerivative k3 = dynamics_model.ComputeDynamics(
            state3.ephemeris_time_tdb_seconds, state3, config).derivative;
        // k4 = f(t + dt, y + dt*k3)
        const SpacecraftState state4 = AddScaledDerivative(
            state, k3, maximum_elapsed_seconds,
            config.solver.start_ephemeris_time_tdb_seconds);
        const StateDerivative k4 = dynamics_model.ComputeDynamics(
            state4.ephemeris_time_tdb_seconds, state4, config).derivative;
        if (observer && observer->IsCancellationRequested())
            return {state, false, "Simulation cancelled.", 1};

        // y_(n+1) = y_n + dt (k1 + 2k2 + 2k3 + k4) / 6.
        SpacecraftState next = AddScaledDerivative(
            state,
            WeightedAverage(k1, k2, k3, k4),
            maximum_elapsed_seconds,
            config.solver.start_ephemeris_time_tdb_seconds);
        // Set both clocks from the step endpoints rather than retaining any arithmetic
        // path through the weighted helper.
        next.elapsed_time_seconds =
            state.elapsed_time_seconds + maximum_elapsed_seconds;
        next.ephemeris_time_tdb_seconds =
            config.solver.start_ephemeris_time_tdb_seconds +
            next.elapsed_time_seconds;
        RestoreIntegratedStateInvariants(config, next);
        return {std::move(next), true, {}, 1};
    }
}
