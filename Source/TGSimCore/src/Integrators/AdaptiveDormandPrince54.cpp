// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Integrators/AdaptiveDormandPrince54.h"

#include "TGSim/Core/ISimulationObserver.h"
#include "TGSim/Integrators/IntegratorUtilities.h"

#include <algorithm>

// Prevent Unreal's check assertion macro from rewriting Boost declarations.
#pragma push_macro("check")
#ifdef check
#undef check
#endif
#include <boost/numeric/odeint/stepper/controlled_step_result.hpp>
#include <boost/numeric/odeint/stepper/generation/generation_controlled_runge_kutta.hpp>
#include <boost/numeric/odeint/stepper/generation/generation_runge_kutta_dopri5.hpp>
#include <boost/numeric/odeint/stepper/generation/make_controlled.hpp>
#include <boost/numeric/odeint/stepper/runge_kutta_dopri5.hpp>
#pragma pop_macro("check")

#include <cmath>
#include <utility>
#include <vector>

namespace tgsim
{
    namespace
    {
        using OdeState = std::vector<double>;

        OdeState PackState(const SpacecraftState& state)
        {
            OdeState values;
            values.reserve(14 + state.variable_component_masses_kg.size() +
                2 * state.articulation_coordinates.size() +
                state.internal_angular_momenta_nms.size());
            values.insert(values.end(), {
                state.position_icrf_m.x, state.position_icrf_m.y, state.position_icrf_m.z,
                state.velocity_icrf_mps.x, state.velocity_icrf_mps.y, state.velocity_icrf_mps.z,
                state.attitude_body_to_icrf.w, state.attitude_body_to_icrf.x,
                state.attitude_body_to_icrf.y, state.attitude_body_to_icrf.z,
                state.angular_velocity_body_radps.x, state.angular_velocity_body_radps.y,
                state.angular_velocity_body_radps.z, state.mass_kg});
            values.insert(values.end(), state.variable_component_masses_kg.begin(),
                state.variable_component_masses_kg.end());
            values.insert(values.end(), state.articulation_coordinates.begin(),
                state.articulation_coordinates.end());
            values.insert(values.end(), state.articulation_rates.begin(),
                state.articulation_rates.end());
            values.insert(values.end(), state.internal_angular_momenta_nms.begin(),
                state.internal_angular_momenta_nms.end());
            return values;
        }

        SpacecraftState UnpackState(
            const OdeState& values,
            double elapsed_time_seconds,
            double start_ephemeris_time_tdb_seconds,
            const SpacecraftState& shape)
        {
            SpacecraftState state = shape;
            state.elapsed_time_seconds = elapsed_time_seconds;
            state.ephemeris_time_tdb_seconds =
                start_ephemeris_time_tdb_seconds + elapsed_time_seconds;
            state.position_icrf_m = {values[0], values[1], values[2]};
            state.velocity_icrf_mps = {values[3], values[4], values[5]};
            state.attitude_body_to_icrf = {values[6], values[7], values[8], values[9]};
            state.angular_velocity_body_radps = {values[10], values[11], values[12]};
            state.mass_kg = values[13];

            std::size_t offset = 14;
            for (double& value : state.variable_component_masses_kg) value = values[offset++];
            for (double& value : state.articulation_coordinates) value = values[offset++];
            for (double& value : state.articulation_rates) value = values[offset++];
            for (double& value : state.internal_angular_momenta_nms) value = values[offset++];
            return state;
        }

        void PackDerivative(const StateDerivative& derivative, OdeState& values)
        {
            values[0] = derivative.position_rate_mps.x;
            values[1] = derivative.position_rate_mps.y;
            values[2] = derivative.position_rate_mps.z;
            values[3] = derivative.velocity_rate_mps2.x;
            values[4] = derivative.velocity_rate_mps2.y;
            values[5] = derivative.velocity_rate_mps2.z;
            values[6] = derivative.attitude_rate.w;
            values[7] = derivative.attitude_rate.x;
            values[8] = derivative.attitude_rate.y;
            values[9] = derivative.attitude_rate.z;
            values[10] = derivative.angular_acceleration_body_radps2.x;
            values[11] = derivative.angular_acceleration_body_radps2.y;
            values[12] = derivative.angular_acceleration_body_radps2.z;
            values[13] = derivative.mass_rate_kgps;

            std::size_t offset = 14;
            for (double value : derivative.variable_component_mass_rates_kgps)
                if (offset < values.size()) values[offset++] = value;
            for (double value : derivative.articulation_coordinate_rates)
                if (offset < values.size()) values[offset++] = value;
            for (double value : derivative.articulation_accelerations)
                if (offset < values.size()) values[offset++] = value;
            for (double value : derivative.internal_angular_momentum_rates_nm)
                if (offset < values.size()) values[offset++] = value;
        }
    }

    IntegrationAdvance AdaptiveDormandPrince54::Advance(
        const IDynamicsModel& dynamics_model,
        const SimulationConfig& config,
        const SpacecraftState& state,
        double maximum_elapsed_seconds,
        const ISimulationObserver* observer) const
    {
        if (maximum_elapsed_seconds <= 0.0)
            return {state, false, "Adaptive integration interval must be positive.", 0};

        OdeState values = PackState(state);
        const SpacecraftState state_shape = state;
        const auto system = [&](const OdeState& current, OdeState& rate, double time)
        {
            // Some Odeint stage buffers are initially empty and are resized lazily.
            // The RHS must nevertheless return exactly one derivative per state entry.
            rate.assign(current.size(), 0.0);
            const SpacecraftState trial = UnpackState(
                current,
                time,
                config.solver.start_ephemeris_time_tdb_seconds,
                state_shape);
            const DynamicsEvaluation evaluation = dynamics_model.ComputeDynamics(
                trial.ephemeris_time_tdb_seconds, trial, config);
            PackDerivative(evaluation.derivative, rate);
        };

        using namespace boost::numeric::odeint;
        using Stepper = runge_kutta_dopri5<OdeState>;
        auto controlled = make_controlled(
            config.solver.absolute_tolerance,
            config.solver.relative_tolerance,
            Stepper{});

        double time = state.elapsed_time_seconds;
        const double target_time = time + maximum_elapsed_seconds;
        double attempted_step = std::min(
            config.solver.initial_integrator_step_seconds,
            maximum_elapsed_seconds);
        std::size_t attempts = 0;

        while (time + 1.0e-12 < target_time)
        {
            if (observer && observer->IsCancellationRequested())
            {
                return {
                    UnpackState(
                        values, time,
                        config.solver.start_ephemeris_time_tdb_seconds,
                        state_shape),
                    false,
                    "Simulation cancelled.",
                    attempts};
            }
            if (++attempts > config.solver.maximum_integration_steps)
            {
                return {
                    UnpackState(
                        values, time,
                        config.solver.start_ephemeris_time_tdb_seconds,
                        state_shape),
                    false,
                    "Adaptive integrator exceeded the configured step-attempt limit at t=" +
                        std::to_string(time) + " s with proposed dt=" +
                        std::to_string(attempted_step) + " s.",
                    attempts};
            }
            attempted_step = std::min({
                attempted_step,
                config.solver.maximum_integrator_step_seconds,
                target_time - time});
            if (!std::isfinite(attempted_step) || attempted_step <= 0.0)
            {
                return {
                    UnpackState(
                        values, time,
                        config.solver.start_ephemeris_time_tdb_seconds,
                        state_shape),
                    false,
                    "Adaptive integrator produced a nonpositive step.",
                    attempts};
            }

            // Do not project the state between accepted Dormand-Prince substeps:
            // dopri5 is an FSAL method and caches the final derivative as the next
            // first derivative. Mutating the accepted state here would invalidate
            // that cache and force the controller toward zero step size.
            controlled.try_step(system, values, time, attempted_step);
        }

        SpacecraftState final_state = UnpackState(
            values,
            target_time,
            config.solver.start_ephemeris_time_tdb_seconds,
            state_shape);
        RestoreIntegratedStateInvariants(config, final_state);
        return {std::move(final_state), true, {}, attempts};
    }
}
