// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Simulation/SimulationEngine.h"

// Owns the synchronous run lifecycle: normalize, validate, propagate, and return results.

#include "TGSim/Control/IController.h"
#include "TGSim/Dynamics/ArticulationConstraintDynamics.h"
#include "TGSim/Dynamics/IDynamicsModel.h"
#include "TGSim/Environment/IEphemerisProvider.h"
#include "TGSim/Forces/AerodynamicCoefficientDatabase.h"
#include "TGSim/Integrators/IIntegrator.h"
#include "TGSim/Integrators/IntegratorUtilities.h"
#include "TGSim/Simulation/ScenarioFactory.h"
#include "TGSim/Simulation/SimulationConfigBuilder.h"
#include "TGSim/Simulation/StateRecorder.h"
#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace tgsim
{
    namespace
    {
        SimulationResult Failure(const std::string& message)
        {
            // Build an early failed result for setup errors detected before StateRecorder
            // owns a column schema or any samples.
            SimulationResult result;
            result.message = message;
            return result;
        }

        double EffectiveDurationSeconds(const SolverSettings& solver)
        {
            return std::isnan(solver.requested_duration_seconds)
                ? solver.final_ephemeris_time_tdb_seconds -
                    solver.start_ephemeris_time_tdb_seconds
                : solver.requested_duration_seconds;
        }

        bool IsFinite(const Vec3d& value)
        {
            return std::isfinite(value.x) && std::isfinite(value.y) &&
                std::isfinite(value.z);
        }

        bool IsFinite(const Mat3d& value)
        {
            for (const auto& row : value.m)
                for (double element : row)
                    if (!std::isfinite(element)) return false;
            return true;
        }

        bool IsRotationMatrix(const Mat3d& value)
        {
            if (!IsFinite(value)) return false;
            const Mat3d gram = value.Transposed() * value;
            double maximum_error = 0.0;
            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 3; ++column)
                {
                    maximum_error = std::max(maximum_error, std::abs(
                        gram.m[row][column] - (row == column ? 1.0 : 0.0)));
                }
            }
            return maximum_error <= 1.0e-9 &&
                std::abs(value.Determinant() - 1.0) <= 1.0e-9;
        }

        bool IsPhysicalInertia(const Mat3d& inertia)
        {
            if (!IsFinite(inertia)) return false;
            double scale = 1.0;
            for (int row = 0; row < 3; ++row)
                for (int column = 0; column < 3; ++column)
                    scale = std::max(scale, std::abs(inertia.m[row][column]));
            const double tolerance = 1.0e-10 * scale;
            for (int row = 0; row < 3; ++row)
                for (int column = row + 1; column < 3; ++column)
                    if (std::abs(inertia.m[row][column] -
                        inertia.m[column][row]) > tolerance) return false;
            if (inertia.m[0][0] < -tolerance ||
                inertia.m[1][1] < -tolerance ||
                inertia.m[2][2] < -tolerance) return false;
            // Principal moments must obey the rigid-body triangle inequalities.
            return inertia.m[0][0] + inertia.m[1][1] + tolerance >= inertia.m[2][2] &&
                inertia.m[0][0] + inertia.m[2][2] + tolerance >= inertia.m[1][1] &&
                inertia.m[1][1] + inertia.m[2][2] + tolerance >= inertia.m[0][0];
        }

        std::string ValidateCurve(const std::string& name, const ScalarCurve& curve)
        {
            if (!std::isfinite(curve.default_value))
                return name + " has a non-finite default value.";
            for (std::size_t index = 0; index < curve.samples.size(); ++index)
            {
                const ScalarSample& sample = curve.samples[index];
                if (!std::isfinite(sample.time_seconds) || !std::isfinite(sample.value))
                    return name + " contains a non-finite sample.";
                if (index > 0 && sample.time_seconds <= curve.samples[index - 1].time_seconds)
                    return name + " sample times must be strictly increasing.";
            }
            return {};
        }

        bool HasPositiveSample(const ScalarCurve& curve)
        {
            return std::any_of(
                curve.samples.begin(),
                curve.samples.end(),
                [](const ScalarSample& sample)
                {
                    return sample.value > 0.0;
                });
        }

        std::string ValidateAtmosphereDensityProfile(
            const std::vector<AtmosphereDensitySample>& samples)
        {
            if (samples.size() < 2)
                return "The tabulated atmosphere needs at least two density rows.";
            for (std::size_t index = 0; index < samples.size(); ++index)
            {
                const AtmosphereDensitySample& sample = samples[index];
                if (!std::isfinite(sample.altitude_m) ||
                    !std::isfinite(sample.density_kgpm3) ||
                    sample.density_kgpm3 <= 0.0)
                {
                    return "Atmosphere density rows need finite altitude and positive density.";
                }
                if (index > 0 &&
                    sample.altitude_m <= samples[index - 1].altitude_m)
                {
                    return "Atmosphere density altitudes must be strictly increasing.";
                }
            }
            return {};
        }

        std::string ValidateAtmosphereThermodynamicProfile(
            const std::vector<AtmosphereThermodynamicSample>& samples)
        {
            if (samples.size() < 2)
                return "The atmosphere needs at least two thermodynamic rows.";
            for (std::size_t index = 0; index < samples.size(); ++index)
            {
                const AtmosphereThermodynamicSample& sample = samples[index];
                if (!std::isfinite(sample.altitude_m) ||
                    !std::isfinite(sample.temperature_k) ||
                    !std::isfinite(sample.mean_particle_mass_kg) ||
                    !std::isfinite(
                        sample.effective_collision_cross_section_m2) ||
                    sample.temperature_k <= 0.0 ||
                    sample.mean_particle_mass_kg <= 0.0 ||
                    sample.effective_collision_cross_section_m2 <= 0.0)
                {
                    return "Atmosphere thermodynamic rows need finite altitude and positive T, mean particle mass, and collision cross-section.";
                }
                if (index > 0 &&
                    sample.altitude_m <= samples[index - 1].altitude_m)
                {
                    return "Atmosphere thermodynamic altitudes must be strictly increasing.";
                }
            }
            return {};
        }

        double EvaluateCubicDensityForValidation(
            const std::array<double, 4>& coefficients,
            double f107_sfu)
        {
            return ((coefficients[3] * f107_sfu + coefficients[2]) *
                f107_sfu + coefficients[1]) * f107_sfu + coefficients[0];
        }

        std::string ValidateCubicHarrisPriester(
            const CubicHarrisPriesterSettings& settings)
        {
            if (!std::isfinite(settings.centered_average_f107_sfu) ||
                settings.centered_average_f107_sfu <= 0.0)
            {
                return "Cubic Harris-Priester needs a positive finite centered-average F10.7 value.";
            }
            if (settings.density_samples.size() < 2)
                return "Cubic Harris-Priester needs at least two coefficient rows.";

            double preceding_minimum = std::numeric_limits<double>::infinity();
            double preceding_maximum = std::numeric_limits<double>::infinity();
            for (std::size_t index = 0;
                 index < settings.density_samples.size();
                 ++index)
            {
                const CubicHarrisPriesterDensitySample& sample =
                    settings.density_samples[index];
                if (!std::isfinite(sample.altitude_m))
                    return "A cubic Harris-Priester altitude is not finite.";
                if (index > 0 && sample.altitude_m <=
                    settings.density_samples[index - 1].altitude_m)
                {
                    return "Cubic Harris-Priester altitudes must be strictly increasing.";
                }
                for (double coefficient :
                    sample.maximum_density_coefficients_kgpm3)
                {
                    if (!std::isfinite(coefficient))
                        return "A cubic Harris-Priester coefficient is not finite.";
                }
                for (double coefficient :
                    sample.minimum_density_coefficients_kgpm3)
                {
                    if (!std::isfinite(coefficient))
                        return "A cubic Harris-Priester coefficient is not finite.";
                }

                const double minimum_density = EvaluateCubicDensityForValidation(
                    sample.minimum_density_coefficients_kgpm3,
                    settings.centered_average_f107_sfu);
                const double maximum_density = EvaluateCubicDensityForValidation(
                    sample.maximum_density_coefficients_kgpm3,
                    settings.centered_average_f107_sfu);
                if (!std::isfinite(minimum_density) ||
                    !std::isfinite(maximum_density) ||
                    minimum_density <= 0.0 || maximum_density < minimum_density)
                {
                    return "Each cubic Harris-Priester row must evaluate to 0 < rho_min <= rho_max at the selected F10.7.";
                }
                if (index > 0 &&
                    (minimum_density >= preceding_minimum ||
                     maximum_density >= preceding_maximum))
                {
                    return "Cubic Harris-Priester minimum and maximum envelopes must decrease strictly with altitude at the selected F10.7.";
                }
                preceding_minimum = minimum_density;
                preceding_maximum = maximum_density;
            }
            return {};
        }

        void ConsiderEventAfter(
            double event_time,
            double current_time,
            double& next_event_time)
        {
            // Distinct representable elapsed values are distinct scheduler events.
            // A scale-dependent tolerance would erase valid future boundaries at
            // long elapsed times and let an RK step cross a discontinuity.
            if (std::isfinite(event_time) &&
                event_time > current_time)
            {
                next_event_time = std::min(next_event_time, event_time);
            }
        }

        void AdvanceStateAlongIncomingDerivative(
            SpacecraftState& state,
            const StateDerivative& derivative,
            double interval_seconds,
            double target_elapsed_time,
            const SimulationConfig& config)
        {
            // The interval is at most the final representable spacing before a
            // discontinuity. Advancing it with the incoming derivative avoids both
            // an RK endpoint sample on the outgoing side and an unadvanced clock gap.
            state.position_icrf_m +=
                derivative.position_rate_mps * interval_seconds;
            state.velocity_icrf_mps +=
                derivative.velocity_rate_mps2 * interval_seconds;
            state.attitude_body_to_icrf +=
                derivative.attitude_rate * interval_seconds;
            state.angular_velocity_body_radps +=
                derivative.angular_acceleration_body_radps2 * interval_seconds;
            state.mass_kg += derivative.mass_rate_kgps * interval_seconds;

            state.variable_component_masses_kg.resize(
                std::max(
                    state.variable_component_masses_kg.size(),
                    derivative.variable_component_mass_rates_kgps.size()),
                0.0);
            for (std::size_t index = 0;
                index < derivative.variable_component_mass_rates_kgps.size();
                ++index)
            {
                state.variable_component_masses_kg[index] +=
                    derivative.variable_component_mass_rates_kgps[index] *
                    interval_seconds;
            }

            state.articulation_coordinates.resize(
                std::max(
                    state.articulation_coordinates.size(),
                    derivative.articulation_coordinate_rates.size()),
                0.0);
            for (std::size_t index = 0;
                index < derivative.articulation_coordinate_rates.size();
                ++index)
            {
                state.articulation_coordinates[index] +=
                    derivative.articulation_coordinate_rates[index] *
                    interval_seconds;
            }

            state.articulation_rates.resize(
                std::max(
                    state.articulation_rates.size(),
                    derivative.articulation_accelerations.size()),
                0.0);
            for (std::size_t index = 0;
                index < derivative.articulation_accelerations.size();
                ++index)
            {
                state.articulation_rates[index] +=
                    derivative.articulation_accelerations[index] *
                    interval_seconds;
            }

            state.internal_angular_momenta_nms.resize(
                std::max(
                    state.internal_angular_momenta_nms.size(),
                    derivative.internal_angular_momentum_rates_nm.size()),
                0.0);
            for (std::size_t index = 0;
                index < derivative.internal_angular_momentum_rates_nm.size();
                ++index)
            {
                state.internal_angular_momenta_nms[index] +=
                    derivative.internal_angular_momentum_rates_nm[index] *
                    interval_seconds;
            }

            state.elapsed_time_seconds = target_elapsed_time;
            state.ephemeris_time_tdb_seconds =
                config.solver.start_ephemeris_time_tdb_seconds +
                target_elapsed_time;
            RestoreIntegratedStateInvariants(config, state);
        }

        void ConsiderCurveEvents(
            const ScalarCurve& curve,
            double time_origin,
            double current_time,
            double& next_event_time)
        {
            for (const ScalarSample& sample : curve.samples)
                ConsiderEventAfter(
                    time_origin + sample.time_seconds,
                    current_time,
                    next_event_time);
        }

        double NextScheduledEvent(
            const SimulationConfig& config,
            double current_elapsed_time)
        {
            double next = std::numeric_limits<double>::infinity();
            const double start_time =
                config.solver.start_ephemeris_time_tdb_seconds;
            for (const ThrusterDefinition& thruster : config.vehicle.thrusters)
            {
                const double ignition_elapsed_time =
                    ThrusterIgnitionElapsedTime(thruster, start_time);
                ConsiderEventAfter(
                    ignition_elapsed_time,
                    current_elapsed_time, next);
                ConsiderEventAfter(
                    ThrusterShutdownElapsedTime(thruster, start_time),
                    current_elapsed_time, next);
                ConsiderCurveEvents(
                    thruster.thrust_profile_n,
                    ignition_elapsed_time,
                    current_elapsed_time, next);
                ConsiderCurveEvents(
                    thruster.specific_impulse_profile_seconds,
                    ignition_elapsed_time,
                    current_elapsed_time, next);
            }
            if (config.control.controller)
            {
                ConsiderEventAfter(
                    config.control.controller->NextDiscontinuityElapsedTime(
                        current_elapsed_time),
                    current_elapsed_time,
                    next);
            }

            return next;
        }

        double NextPredictedWheelSaturation(
            const SimulationConfig& config,
            const SpacecraftState& state,
            const DynamicsEvaluation& evaluation)
        {
            double next = std::numeric_limits<double>::infinity();
            const std::size_t count = std::min({
                config.vehicle.reaction_wheels.size(),
                state.internal_angular_momenta_nms.size(),
                evaluation.derivative.internal_angular_momentum_rates_nm.size()});
            for (std::size_t index = 0; index < count; ++index)
            {
                const double limit =
                    config.vehicle.reaction_wheels[index].maximum_momentum_nms;
                const double momentum =
                    state.internal_angular_momenta_nms[index];
                const double rate =
                    evaluation.derivative.internal_angular_momentum_rates_nm[index];
                if (!std::isfinite(limit) || limit <= 0.0 || rate == 0.0)
                    continue;

                const double remaining = rate > 0.0
                    ? limit - momentum
                    : -limit - momentum;
                const double time_to_limit = remaining / rate;
                if (std::isfinite(time_to_limit) && time_to_limit > 0.0)
                {
                    ConsiderEventAfter(
                        state.elapsed_time_seconds + time_to_limit,
                        state.elapsed_time_seconds,
                        next);
                }
            }
            return next;
        }

        struct WheelLimitCrossing
        {
            std::size_t index = 0;
            double boundary_momentum_nms = 0.0;
        };

        std::vector<WheelLimitCrossing> FindWheelLimitCrossings(
            const SimulationConfig& config,
            const SpacecraftState& start,
            const SpacecraftState& candidate)
        {
            std::vector<WheelLimitCrossing> crossings;
            const std::size_t count = std::min({
                config.vehicle.reaction_wheels.size(),
                start.internal_angular_momenta_nms.size(),
                candidate.internal_angular_momenta_nms.size()});
            for (std::size_t index = 0; index < count; ++index)
            {
                const double limit =
                    config.vehicle.reaction_wheels[index].maximum_momentum_nms;
                if (!std::isfinite(limit)) continue;

                const double initial = start.internal_angular_momenta_nms[index];
                const double final = candidate.internal_angular_momenta_nms[index];
                const double tolerance = 64.0 *
                    std::numeric_limits<double>::epsilon() *
                    std::max(1.0, limit);
                if (initial < limit - tolerance && final >= limit)
                    crossings.push_back({index, limit});
                else if (initial > -limit + tolerance && final <= -limit)
                    crossings.push_back({index, -limit});
            }
            return crossings;
        }

        void SnapWheelMomentaNearLimits(
            const SimulationConfig& config,
            SpacecraftState& state)
        {
            const std::size_t count = std::min(
                config.vehicle.reaction_wheels.size(),
                state.internal_angular_momenta_nms.size());
            for (std::size_t index = 0; index < count; ++index)
            {
                const double limit =
                    config.vehicle.reaction_wheels[index].maximum_momentum_nms;
                if (!std::isfinite(limit)) continue;
                double& momentum = state.internal_angular_momenta_nms[index];
                const double tolerance = 1.0e-10 * std::max(1.0, limit);
                if (std::abs(momentum - limit) <= tolerance)
                    momentum = limit;
                else if (std::abs(momentum + limit) <= tolerance)
                    momentum = -limit;
            }
        }

        enum class ArticulationLimitKind
        {
            LowerCoordinate,
            UpperCoordinate,
            LowerRate,
            UpperRate
        };

        struct ArticulationLimitCrossing
        {
            std::size_t articulation_state_index = kInvalidIndex;
            ArticulationLimitKind kind = ArticulationLimitKind::LowerCoordinate;
            double boundary_coordinate = 0.0;
            double target_rate = 0.0;
            double admissible_direction = 1.0;
        };

        template <typename Callback>
        void ForEachArticulationDof(
            const SimulationConfig& config,
            Callback&& callback)
        {
            for (const ComponentDefinition& component : config.vehicle.components)
            {
                if (component.articulation_state_offset == kInvalidIndex)
                    continue;
                for (std::size_t local_index = 0;
                    local_index < component.articulation_to_parent.dofs.size();
                    ++local_index)
                {
                    callback(
                        component.articulation_state_offset + local_index,
                        component.articulation_to_parent.dofs[local_index]);
                }
            }
        }

        double ArticulationLimitTolerance(double limit)
        {
            // Treat configurations this close to a boundary as the same contact
            // configuration when assembling a simultaneous impact.
            return 1.0e-10 * std::max(1.0, std::abs(limit));
        }

        double ArticulationCrossingTolerance(double limit)
        {
            // Crossing detection must remain at roundoff scale. Reusing the wider
            // contact tolerance here makes a slowly moving joint appear to impact
            // earlier than a faster joint that reaches the same stop at the same time.
            return 64.0 * std::numeric_limits<double>::epsilon() *
                std::max(1.0, std::abs(limit));
        }

        std::vector<ArticulationLimitCrossing> FindArticulationLimitCrossings(
            const SimulationConfig& config,
            const SpacecraftState& start,
            const SpacecraftState& candidate)
        {
            std::vector<ArticulationLimitCrossing> crossings;
            ForEachArticulationDof(
                config,
                [&](std::size_t state_index, const ArticulationDof& dof)
                {
                    if (state_index >= start.articulation_coordinates.size() ||
                        state_index >= candidate.articulation_coordinates.size() ||
                        state_index >= start.articulation_rates.size() ||
                        state_index >= candidate.articulation_rates.size())
                    {
                        return;
                    }

                    const double initial_coordinate =
                        start.articulation_coordinates[state_index];
                    const double final_coordinate =
                        candidate.articulation_coordinates[state_index];
                    const double initial_rate = start.articulation_rates[state_index];
                    const double final_rate = candidate.articulation_rates[state_index];
                    const ArticulationLimits& limits = dof.limits;

                    if (std::isfinite(limits.minimum_coordinate))
                    {
                        const double tolerance = ArticulationCrossingTolerance(
                            limits.minimum_coordinate);
                        const bool crossed =
                            (initial_coordinate > limits.minimum_coordinate + tolerance &&
                                final_coordinate <= limits.minimum_coordinate + tolerance) ||
                            (initial_coordinate >= limits.minimum_coordinate - tolerance &&
                                initial_rate > 0.0 &&
                                final_coordinate <= limits.minimum_coordinate + tolerance &&
                                final_rate < 0.0);
                        if (crossed)
                        {
                            crossings.push_back({
                                state_index,
                                ArticulationLimitKind::LowerCoordinate,
                                limits.minimum_coordinate,
                                0.0,
                                1.0});
                            return;
                        }
                    }
                    if (std::isfinite(limits.maximum_coordinate))
                    {
                        const double tolerance = ArticulationCrossingTolerance(
                            limits.maximum_coordinate);
                        const bool crossed =
                            (initial_coordinate < limits.maximum_coordinate - tolerance &&
                                final_coordinate >= limits.maximum_coordinate - tolerance) ||
                            (initial_coordinate <= limits.maximum_coordinate + tolerance &&
                                initial_rate < 0.0 &&
                                final_coordinate >= limits.maximum_coordinate - tolerance &&
                                final_rate > 0.0);
                        if (crossed)
                        {
                            crossings.push_back({
                                state_index,
                                ArticulationLimitKind::UpperCoordinate,
                                limits.maximum_coordinate,
                                0.0,
                                -1.0});
                            return;
                        }
                    }

                    const double maximum_rate = limits.maximum_absolute_rate;
                    if (!std::isfinite(maximum_rate)) return;
                    const double tolerance =
                        ArticulationCrossingTolerance(maximum_rate);
                    if ((initial_rate < maximum_rate - tolerance &&
                            final_rate >= maximum_rate - tolerance) ||
                        (initial_rate <= maximum_rate + tolerance &&
                            final_rate > maximum_rate + tolerance))
                    {
                        crossings.push_back({
                            state_index,
                            ArticulationLimitKind::UpperRate,
                            0.0,
                            maximum_rate,
                            -1.0});
                    }
                    else if ((initial_rate > -maximum_rate + tolerance &&
                            final_rate <= -maximum_rate + tolerance) ||
                        (initial_rate >= -maximum_rate - tolerance &&
                            final_rate < -maximum_rate - tolerance))
                    {
                        crossings.push_back({
                            state_index,
                            ArticulationLimitKind::LowerRate,
                            0.0,
                            -maximum_rate,
                            1.0});
                    }
                });
            return crossings;
        }

        void ConsiderCoordinateLimitRoot(
            double coordinate,
            double rate,
            double acceleration,
            double boundary,
            double outward_direction,
            double current_elapsed_time,
            double& next_event_time)
        {
            const double constant = coordinate - boundary;
            const double acceleration_tolerance = 1.0e-15;
            if (std::abs(acceleration) <= acceleration_tolerance)
            {
                if (rate * outward_direction <= 0.0) return;
                ConsiderEventAfter(
                    current_elapsed_time - constant / rate,
                    current_elapsed_time,
                    next_event_time);
                return;
            }

            const double discriminant =
                rate * rate - 2.0 * acceleration * constant;
            if (discriminant < 0.0 || !std::isfinite(discriminant)) return;
            const double root = std::sqrt(discriminant);
            for (const double time_to_limit : {
                (-rate - root) / acceleration,
                (-rate + root) / acceleration})
            {
                if (!std::isfinite(time_to_limit) || time_to_limit <= 0.0)
                    continue;
                const double rate_at_limit = rate + acceleration * time_to_limit;
                if (rate_at_limit * outward_direction <= 0.0) continue;
                ConsiderEventAfter(
                    current_elapsed_time + time_to_limit,
                    current_elapsed_time,
                    next_event_time);
            }
        }

        double NextPredictedArticulationLimit(
            const SimulationConfig& config,
            const SpacecraftState& state,
            const DynamicsEvaluation& evaluation)
        {
            double next = std::numeric_limits<double>::infinity();
            ForEachArticulationDof(
                config,
                [&](std::size_t state_index, const ArticulationDof& dof)
                {
                    if (state_index >= state.articulation_coordinates.size() ||
                        state_index >= state.articulation_rates.size() ||
                        state_index >= evaluation.derivative.
                            articulation_accelerations.size())
                    {
                        return;
                    }
                    const double coordinate =
                        state.articulation_coordinates[state_index];
                    const double rate = state.articulation_rates[state_index];
                    const double acceleration = evaluation.derivative.
                        articulation_accelerations[state_index];
                    if (std::isfinite(dof.limits.minimum_coordinate))
                    {
                        ConsiderCoordinateLimitRoot(
                            coordinate,
                            rate,
                            acceleration,
                            dof.limits.minimum_coordinate,
                            -1.0,
                            state.elapsed_time_seconds,
                            next);
                    }
                    if (std::isfinite(dof.limits.maximum_coordinate))
                    {
                        ConsiderCoordinateLimitRoot(
                            coordinate,
                            rate,
                            acceleration,
                            dof.limits.maximum_coordinate,
                            1.0,
                            state.elapsed_time_seconds,
                            next);
                    }
                    const double maximum_rate =
                        dof.limits.maximum_absolute_rate;
                    if (!std::isfinite(maximum_rate) || acceleration == 0.0)
                        return;
                    const double rate_boundary = acceleration > 0.0
                        ? maximum_rate
                        : -maximum_rate;
                    const double time_to_limit =
                        (rate_boundary - rate) / acceleration;
                    if (time_to_limit > 0.0)
                    {
                        ConsiderEventAfter(
                            state.elapsed_time_seconds + time_to_limit,
                            state.elapsed_time_seconds,
                            next);
                    }
                });
            return next;
        }

        std::vector<ArticulationLimitCrossing> FindInitialArticulationConstraints(
            const SimulationConfig& config,
            const SpacecraftState& state)
        {
            std::vector<ArticulationLimitCrossing> constraints;
            ForEachArticulationDof(
                config,
                [&](std::size_t state_index, const ArticulationDof& dof)
                {
                    if (state_index >= state.articulation_coordinates.size() ||
                        state_index >= state.articulation_rates.size())
                    {
                        return;
                    }
                    const double coordinate =
                        state.articulation_coordinates[state_index];
                    const double rate = state.articulation_rates[state_index];
                    if (std::isfinite(dof.limits.minimum_coordinate) &&
                        coordinate <= dof.limits.minimum_coordinate && rate < 0.0)
                    {
                        constraints.push_back({
                            state_index,
                            ArticulationLimitKind::LowerCoordinate,
                            dof.limits.minimum_coordinate,
                            0.0,
                            1.0});
                        return;
                    }
                    if (std::isfinite(dof.limits.maximum_coordinate) &&
                        coordinate >= dof.limits.maximum_coordinate && rate > 0.0)
                    {
                        constraints.push_back({
                            state_index,
                            ArticulationLimitKind::UpperCoordinate,
                            dof.limits.maximum_coordinate,
                            0.0,
                            -1.0});
                        return;
                    }
                    const double maximum_rate = dof.limits.maximum_absolute_rate;
                    if (!std::isfinite(maximum_rate)) return;
                    if (rate > maximum_rate)
                    {
                        constraints.push_back({
                            state_index,
                            ArticulationLimitKind::UpperRate,
                            0.0,
                            maximum_rate,
                            -1.0});
                    }
                    else if (rate < -maximum_rate)
                    {
                        constraints.push_back({
                            state_index,
                            ArticulationLimitKind::LowerRate,
                            0.0,
                            -maximum_rate,
                            1.0});
                    }
                });
            return constraints;
        }

        ArticulationImpulseEvaluation ApplyArticulationLimitImpulse(
            const SimulationConfig& config,
            const std::vector<ArticulationLimitCrossing>& crossings,
            SpacecraftState& state)
        {
            std::vector<ArticulationLimitCrossing> candidates = crossings;
            for (const ArticulationLimitCrossing& crossing : crossings)
            {
                if (crossing.kind == ArticulationLimitKind::LowerCoordinate ||
                    crossing.kind == ArticulationLimitKind::UpperCoordinate)
                {
                    state.articulation_coordinates[
                        crossing.articulation_state_index] =
                            crossing.boundary_coordinate;
                }
            }

            const auto find_candidate = [&candidates](std::size_t state_index)
            {
                return std::find_if(
                    candidates.begin(),
                    candidates.end(),
                    [state_index](const ArticulationLimitCrossing& candidate)
                    {
                        return candidate.articulation_state_index == state_index;
                    });
            };

            // Resting coordinate and speed contacts can receive or release an
            // impulse when another joint hits. Include them in the same unilateral
            // solve instead of considering only newly crossed boundaries.
            ForEachArticulationDof(
                config,
                [&](std::size_t state_index, const ArticulationDof& dof)
                {
                    if (state_index >= state.articulation_coordinates.size() ||
                        state_index >= state.articulation_rates.size() ||
                        find_candidate(state_index) != candidates.end())
                    {
                        return;
                    }
                    const double coordinate =
                        state.articulation_coordinates[state_index];
                    const double rate = state.articulation_rates[state_index];
                    if (std::isfinite(dof.limits.minimum_coordinate) &&
                        coordinate <= dof.limits.minimum_coordinate +
                            ArticulationLimitTolerance(
                                dof.limits.minimum_coordinate))
                    {
                        candidates.push_back({
                            state_index,
                            ArticulationLimitKind::LowerCoordinate,
                            dof.limits.minimum_coordinate,
                            0.0,
                            1.0});
                        return;
                    }
                    if (std::isfinite(dof.limits.maximum_coordinate) &&
                        coordinate >= dof.limits.maximum_coordinate -
                            ArticulationLimitTolerance(
                                dof.limits.maximum_coordinate))
                    {
                        candidates.push_back({
                            state_index,
                            ArticulationLimitKind::UpperCoordinate,
                            dof.limits.maximum_coordinate,
                            0.0,
                            -1.0});
                        return;
                    }
                    const double maximum_rate =
                        dof.limits.maximum_absolute_rate;
                    if (!std::isfinite(maximum_rate) || maximum_rate <= 0.0)
                        return;
                    const double tolerance =
                        ArticulationLimitTolerance(maximum_rate);
                    if (rate >= maximum_rate - tolerance)
                    {
                        candidates.push_back({
                            state_index,
                            ArticulationLimitKind::UpperRate,
                            0.0,
                            maximum_rate,
                            -1.0});
                    }
                    else if (rate <= -maximum_rate + tolerance)
                    {
                        candidates.push_back({
                            state_index,
                            ArticulationLimitKind::LowerRate,
                            0.0,
                            -maximum_rate,
                            1.0});
                    }
                });

            for (const ArticulationLimitCrossing& candidate : candidates)
            {
                if (candidate.kind == ArticulationLimitKind::LowerCoordinate ||
                    candidate.kind == ArticulationLimitKind::UpperCoordinate)
                {
                    state.articulation_coordinates[
                        candidate.articulation_state_index] =
                            candidate.boundary_coordinate;
                }
            }

            const SpacecraftState pre_impulse_state = state;
            ArticulationImpulseEvaluation evaluation;
            const std::size_t maximum_passes =
                std::max<std::size_t>(1, state.articulation_rates.size() + 1);
            for (std::size_t pass = 0; pass < maximum_passes; ++pass)
            {
                std::vector<ArticulationVelocityConstraint> constraints;
                constraints.reserve(candidates.size());
                for (const ArticulationLimitCrossing& candidate : candidates)
                {
                    constraints.push_back({
                        candidate.articulation_state_index,
                        candidate.admissible_direction,
                        candidate.target_rate});
                }

                SpacecraftState trial_state = pre_impulse_state;
                evaluation = ArticulationConstraintDynamics().
                    ApplyIdealVelocityConstraints(
                        config, trial_state, constraints);
                if (!evaluation.solved) return evaluation;

                bool added_rate_constraint = false;
                ForEachArticulationDof(
                    config,
                    [&](std::size_t state_index, const ArticulationDof& dof)
                    {
                        if (added_rate_constraint ||
                            state_index >= trial_state.articulation_rates.size())
                        {
                            return;
                        }
                        const double maximum_rate =
                            dof.limits.maximum_absolute_rate;
                        if (!std::isfinite(maximum_rate)) return;
                        const double tolerance =
                            ArticulationLimitTolerance(maximum_rate);
                        const double rate =
                            trial_state.articulation_rates[state_index];
                        ArticulationLimitCrossing required;
                        if (rate > maximum_rate + tolerance)
                        {
                            required = {
                                state_index,
                                ArticulationLimitKind::UpperRate,
                                0.0,
                                maximum_rate,
                                -1.0};
                        }
                        else if (rate < -maximum_rate - tolerance)
                        {
                            required = {
                                state_index,
                                ArticulationLimitKind::LowerRate,
                                0.0,
                                -maximum_rate,
                                1.0};
                        }
                        else
                        {
                            return;
                        }

                        const auto existing = find_candidate(state_index);
                        if (existing == candidates.end())
                        {
                            candidates.push_back(required);
                            added_rate_constraint = true;
                            return;
                        }
                        const std::size_t existing_index =
                            static_cast<std::size_t>(
                                std::distance(candidates.begin(), existing));
                        if (existing_index < evaluation.active_constraints.size() &&
                            !evaluation.active_constraints[existing_index])
                        {
                            *existing = required;
                            added_rate_constraint = true;
                        }
                    });
                if (!added_rate_constraint)
                {
                    state = std::move(trial_state);
                    return evaluation;
                }
            }

            evaluation.solved = false;
            evaluation.message =
                "The joint-limit impact activated too many secondary speed constraints.";
            return evaluation;
        }

        std::string Validate(const SimulationConfig& config)
        {
            // Gate the propagation workflow after Build() has assigned flattened indices.
            // Returning the first error keeps SimulationEngine::Run synchronous and gives
            // the future Unreal adapter one actionable message instead of a partial run.
            if (config.solver.maximum_integrator_step_seconds <= 0.0)
                return "Maximum integrator step must be positive.";
            if (config.solver.initial_integrator_step_seconds <= 0.0)
                return "Initial integrator step must be positive.";
            if (config.solver.absolute_tolerance <= 0.0 || config.solver.relative_tolerance <= 0.0)
                return "Integrator tolerances must be positive.";
            if (!std::isnan(config.solver.requested_duration_seconds) &&
                (!std::isfinite(config.solver.requested_duration_seconds) ||
                 config.solver.requested_duration_seconds < 0.0))
            {
                return "Requested simulation duration must be finite and nonnegative.";
            }
            if (config.solver.final_ephemeris_time_tdb_seconds <
                config.solver.start_ephemeris_time_tdb_seconds)
                return "Final ephemeris time must not precede start ephemeris time.";
            if (config.solver.output_mode == OutputMode::FixedInterval && config.solver.output_step_seconds <= 0.0)
                return "Output step must be positive in fixed-interval mode.";
            if (config.solver.maximum_integration_steps == 0) return "Maximum integration steps must be positive.";
            if (config.solver.maximum_output_samples == 0) return "Maximum output samples must be positive.";
            if (!std::isfinite(config.solver.start_ephemeris_time_tdb_seconds) ||
                !std::isfinite(config.solver.final_ephemeris_time_tdb_seconds))
                return "Simulation epochs must be finite.";
            if (!IsFinite(config.initial_state.position_icrf_m) ||
                !IsFinite(config.initial_state.velocity_icrf_mps) ||
                !IsFinite(config.initial_state.angular_velocity_body_radps))
                return "Initial translational and angular states must be finite.";
            if (!std::isfinite(config.initial_state.mass_kg) ||
                config.initial_state.mass_kg <= 0.0)
                return "Initial spacecraft mass must be finite and positive.";
            if (config.initial_state.attitude_body_to_icrf.Norm() <= 1.0e-15) return "Initial attitude quaternion must be nonzero.";

            // Verify both the physical tree ordering and the flattened scalar-DOF layout
            // expected by ComponentKinematics and FloatingBaseTreeDynamics.
            std::size_t articulation_count = 0;
            std::unordered_set<std::string> component_names;
            std::unordered_set<std::size_t> variable_mass_indices;
            double total_configured_initial_mass_kg = 0.0;
            double minimum_reachable_mass_kg = 0.0;
            for (std::size_t component_index = 0; component_index < config.vehicle.components.size(); ++component_index)
            {
                const ComponentDefinition& component = config.vehicle.components[component_index];
                if (component.name.empty() ||
                    !component_names.insert(component.name).second)
                    return "Every spacecraft component needs a unique nonempty name.";
                if (!std::isfinite(component.initial_mass_kg) ||
                    !std::isfinite(component.minimum_mass_kg) ||
                    component.initial_mass_kg < 0.0 ||
                    component.minimum_mass_kg < 0.0 ||
                    component.initial_mass_kg < component.minimum_mass_kg)
                    return "A component has invalid initial or minimum mass.";
                total_configured_initial_mass_kg += component.initial_mass_kg;
                minimum_reachable_mass_kg +=
                    component.variable_mass_state_index != kInvalidIndex
                    ? component.minimum_mass_kg
                    : component.initial_mass_kg;
                if (!IsFinite(component.center_of_mass_component_m) ||
                    !IsPhysicalInertia(component.inertia_centroid_component_kgm2))
                    return "A component has invalid CM coordinates or centroidal inertia.";
                if (component.variable_mass_state_index != kInvalidIndex &&
                    !variable_mass_indices.insert(
                        component.variable_mass_state_index).second)
                    return "Variable-mass state indices must be unique per component.";
                if (component_index == 0)
                {
                    if (component.parent_component_index != kInvalidIndex)
                        return "The first component must be the parentless main body component.";
                    if (!component.articulation_to_parent.dofs.empty())
                        return "The main body component cannot articulate relative to a parent.";
                    if (!IsFinite(component.origin_body_m) ||
                        !IsRotationMatrix(component.component_to_body))
                        return "The main body origin or initial rotation is invalid.";
                }
                else if (component.parent_component_index == kInvalidIndex ||
                    component.parent_component_index >= component_index)
                {
                    return "Every non-root component must reference an earlier parent component.";
                }

                const std::size_t dof_count = component.articulation_to_parent.dofs.size();
                if (component_index > 0 &&
                    (!IsFinite(component.articulation_to_parent.parent_anchor_component_m) ||
                     !IsFinite(component.articulation_to_parent.child_anchor_component_m) ||
                     !IsRotationMatrix(component.articulation_to_parent.child_to_parent_at_zero)))
                    return "A child articulation has invalid anchors or zero-pose rotation.";
                if (dof_count == 0)
                {
                    if (component.articulation_state_offset != kInvalidIndex)
                        return "A component without articulation DOFs has an invalid state offset.";
                    continue;
                }
                if (component.articulation_state_offset != articulation_count)
                    return "Articulation state offsets are not contiguous in component order.";

                for (const ArticulationDof& dof : component.articulation_to_parent.dofs)
                {
                    if (!std::isfinite(dof.axis_joint.Norm()) || dof.axis_joint.Norm() <= 1.0e-15)
                        return "An articulation DOF axis must be finite and nonzero.";
                    if (std::isnan(dof.limits.minimum_coordinate) ||
                        std::isnan(dof.limits.maximum_coordinate) ||
                        dof.limits.minimum_coordinate > dof.limits.maximum_coordinate)
                    {
                        return "An articulation DOF has invalid coordinate limits.";
                    }
                    if (std::isnan(dof.limits.maximum_absolute_rate) ||
                        dof.limits.maximum_absolute_rate < 0.0)
                    {
                        return "An articulation DOF has an invalid maximum rate.";
                    }
                    if (std::isnan(dof.limits.maximum_absolute_effort) ||
                        dof.limits.maximum_absolute_effort < 0.0)
                    {
                        return "An articulation DOF has an invalid maximum effort.";
                    }
                    if (!std::isfinite(dof.initial_coordinate) ||
                        !std::isfinite(dof.initial_rate))
                        return "An articulation DOF has a non-finite initial state.";
                }
                articulation_count += dof_count;
            }
            if (!std::isfinite(total_configured_initial_mass_kg) ||
                total_configured_initial_mass_kg <= 0.0)
                return "Total configured initial spacecraft mass must be finite and positive.";
            if (!std::isfinite(minimum_reachable_mass_kg) ||
                minimum_reachable_mass_kg <= 0.0)
                return "Minimum reachable spacecraft mass must be finite and positive.";
            if (config.initial_state.articulation_coordinates.size() != articulation_count ||
                config.initial_state.articulation_rates.size() != articulation_count)
            {
                return "Articulation coordinate and rate state sizes do not match the configured DOFs.";
            }
            for (double coordinate : config.initial_state.articulation_coordinates)
                if (!std::isfinite(coordinate)) return "Initial articulation coordinates must be finite.";
            for (double rate : config.initial_state.articulation_rates)
                if (!std::isfinite(rate)) return "Initial articulation rates must be finite.";
            for (double mass : config.initial_state.variable_component_masses_kg)
                if (!std::isfinite(mass)) return "Initial component masses must be finite.";
            for (double momentum : config.initial_state.internal_angular_momenta_nms)
                if (!std::isfinite(momentum)) return "Initial wheel momenta must be finite.";

            if (config.atmosphere.enabled)
            {
                const std::string thermodynamic_error =
                    ValidateAtmosphereThermodynamicProfile(
                        config.atmosphere.thermodynamic_profile);
                if (!thermodynamic_error.empty()) return thermodynamic_error;

                if (config.atmosphere.model_kind ==
                    AtmosphereModelKind::TabulatedProfile)
                {
                    const std::string density_error =
                        ValidateAtmosphereDensityProfile(
                            config.atmosphere.density_profile);
                    if (!density_error.empty()) return density_error;
                }
                else
                {
                    const std::string harris_priester_error =
                        ValidateCubicHarrisPriester(
                            config.atmosphere.cubic_harris_priester);
                    if (!harris_priester_error.empty())
                        return harris_priester_error;
                }
            }

            if (config.aerodynamics.enabled)
            {
                if (!config.atmosphere.enabled)
                    return "Aerodynamics requires an enabled atmosphere.";
                if (!std::isfinite(config.aerodynamics.reference_area_m2) ||
                    config.aerodynamics.reference_area_m2 <= 0.0 ||
                    !std::isfinite(config.aerodynamics.reference_length_m) ||
                    config.aerodynamics.reference_length_m <= 0.0 ||
                    !std::isfinite(
                        config.aerodynamics.minimum_dynamic_pressure_pa) ||
                    !std::isfinite(
                        config.aerodynamics.maximum_dynamic_pressure_pa) ||
                    config.aerodynamics.minimum_dynamic_pressure_pa < 0.0 ||
                    config.aerodynamics.maximum_dynamic_pressure_pa <
                        config.aerodynamics.minimum_dynamic_pressure_pa)
                {
                    return "Enabled aerodynamics needs positive reference dimensions and valid dynamic-pressure limits.";
                }
                if (config.aerodynamics.constant_drag_fallback_enabled &&
                    (!std::isfinite(
                        config.aerodynamics.fallback_drag_coefficient) ||
                     config.aerodynamics.fallback_drag_coefficient <= 0.0))
                {
                    return "The constant-drag fallback requires a positive finite drag coefficient.";
                }
                if (!config.aerodynamics.coefficient_database.enabled &&
                    !config.aerodynamics.constant_drag_fallback_enabled)
                {
                    return "Enabled aerodynamics needs an aggregate coefficient database or the constant-drag fallback.";
                }
                if (config.aerodynamics.coefficient_database.enabled &&
                    config.aerodynamics.coefficient_database.extrapolation ==
                        AerodynamicDatabaseExtrapolationMethod::UseConstantDragFallback &&
                    !config.aerodynamics.constant_drag_fallback_enabled)
                {
                    return "Constant-drag database extrapolation is selected, but the fallback is disabled.";
                }

                const std::string aerodynamic_database_error =
                    ValidateAerodynamicCoefficientDatabase(
                        config.aerodynamics.coefficient_database,
                        articulation_count);
                if (!aerodynamic_database_error.empty())
                    return aerodynamic_database_error;
            }

            if (std::abs(MassPropertiesModel().Compute(
                    config.vehicle, config.initial_state).inertia_body_kgm2.Determinant()) <= 1.0e-18)
                return "Initial spacecraft inertia tensor must be nonsingular.";

            // Harmonic indexing must obey 0 <= m <= n. C00/S00 are deliberately implicit
            // in GravityModel, so accepting user C00 here would double-define the monopole.
            std::unordered_set<std::string> gravity_body_names;
            std::unordered_set<int> gravity_naif_ids;
            std::unordered_map<std::string, std::size_t> system_barycenter_counts;
            std::unordered_map<std::string, std::size_t> system_member_counts;
            std::unordered_map<std::string, bool> system_requests_resolution;
            for (const GravityBody& body : config.gravity.bodies)
            {
                if (body.name.empty() ||
                    !gravity_body_names.insert(body.name).second)
                    return "Every gravity body needs a unique nonempty name.";
                if (body.naif_id != 0 &&
                    !gravity_naif_ids.insert(body.naif_id).second)
                {
                    return "Every catalog gravity source needs a unique NAIF ID.";
                }
                if (body.gravity_source_role == GravitySourceRole::Independent)
                {
                    if (!body.gravity_system_name.empty())
                        return "An independent gravity source cannot belong to a barycenter system.";
                    if (body.barycenter_resolution_radius_m != 0.0)
                        return "Only a system barycenter may have a resolution radius.";
                }
                else
                {
                    if (body.gravity_system_name.empty())
                        return "Every barycenter or member gravity source needs a system name.";
                    if (body.gravity_source_role == GravitySourceRole::SystemBarycenter)
                    {
                        ++system_barycenter_counts[body.gravity_system_name];
                        system_requests_resolution[body.gravity_system_name] =
                            body.barycenter_resolution_radius_m > 0.0;
                    }
                    else
                        ++system_member_counts[body.gravity_system_name];
                }
                if (config.gravity.ephemeris_provider &&
                    body.gravitational_parameter_m3ps2 <= 0.0)
                {
                    return "The ephemeris provider cannot resolve GM for body '" +
                        body.name + "'.";
                }
                if (!std::isfinite(body.gravitational_parameter_m3ps2) ||
                    body.gravitational_parameter_m3ps2 < 0.0 ||
                    !std::isfinite(body.reference_radius_m) ||
                    body.reference_radius_m < 0.0 ||
                    !std::isfinite(
                        body.harmonic_model_gravitational_parameter_m3ps2) ||
                    body.harmonic_model_gravitational_parameter_m3ps2 < 0.0 ||
                    !std::isfinite(body.harmonic_model_reference_radius_m) ||
                    body.harmonic_model_reference_radius_m < 0.0 ||
                    !IsFinite(body.position_icrf_at_epoch_m) ||
                    !IsFinite(body.velocity_icrf_mps) ||
                    !IsFinite(body.spin_axis_icrf) ||
                    !std::isfinite(body.spin_rate_radps) ||
                    !std::isfinite(body.epoch_ephemeris_time_tdb_seconds))
                    return "A gravity body has invalid physical or fallback ephemeris data.";
                if (body.maximum_harmonic_degree > 0 &&
                    !IsRotationMatrix(body.body_fixed_to_icrf_at_epoch))
                    return "A harmonic gravity body has an invalid body-fixed rotation.";
                if (body.automatic_gravity_activation_radius_m < 0.0)
                    return "A gravity body's automatic activation radius must be nonnegative.";
                if (body.barycenter_resolution_radius_m < 0.0)
                    return "A barycenter's resolution radius must be nonnegative.";
                if (body.maximum_harmonic_degree < 0)
                    return "Gravity maximum harmonic degree must be nonnegative.";
                if (body.maximum_harmonic_degree > 0)
                {
                    if (body.gravity_source_role ==
                        GravitySourceRole::SystemBarycenter)
                    {
                        return "A system barycenter supports point-mass gravity only.";
                    }
                    if (body.harmonic_model_gravitational_parameter_m3ps2 <= 0.0 ||
                        body.harmonic_model_reference_radius_m <= 0.0)
                    {
                        return "Harmonic gravity requires the uploaded model's positive GM and reference radius for body '" +
                            body.name + "'.";
                    }
                    if (body.harmonics.empty())
                    {
                        return "Harmonic gravity requires coefficient rows for body '" +
                            body.name + "'.";
                    }
                }
                for (const HarmonicCoefficient& coefficient : body.harmonics)
                {
                    if (coefficient.degree < 0 || coefficient.order < 0 || coefficient.order > coefficient.degree)
                        return "A gravity harmonic coefficient violates n >= 0 and 0 <= m <= n.";
                    if (coefficient.degree == 0 && coefficient.order == 0)
                        return "Do not provide C00 or S00; the monopole C00 = 1 and S00 = 0 is implicit.";
                    if (coefficient.degree > body.maximum_harmonic_degree)
                        return "A gravity harmonic coefficient exceeds its body's configured maximum degree.";
                }
            }
            for (const auto& system : system_barycenter_counts)
            {
                if (system.second != 1)
                    return "Every gravity system must contain exactly one barycenter.";
                // A point-mass barycenter is complete by itself. Physical members
                // become mandatory only when near-field barycenter resolution is on;
                // otherwise entering the resolution radius would remove all gravity.
                if (system_member_counts[system.first] == 0 &&
                    system_requests_resolution[system.first])
                {
                    return "A barycenter-only gravity system must set its resolution radius to zero.";
                }
            }
            for (const auto& system : system_member_counts)
            {
                if (system_barycenter_counts[system.first] != 1)
                    return "Every grouped gravity member must have exactly one barycenter.";
            }

            if (config.gravity.ephemeris_provider)
            {
                for (const GravityBody& body : config.gravity.bodies)
                {
                    BodyState body_state;
                    if (!config.gravity.ephemeris_provider->TryGetBodyState(
                            body.name,
                            config.solver.start_ephemeris_time_tdb_seconds,
                            body_state) ||
                        !config.gravity.ephemeris_provider->TryGetBodyState(
                            body.name,
                            config.solver.final_ephemeris_time_tdb_seconds,
                            body_state))
                    {
                        return "The ephemeris provider cannot resolve body '" +
                            body.name + "' across the requested interval.";
                    }
                    if (body.maximum_harmonic_degree > 0 ||
                        (config.atmosphere.enabled &&
                         body.name == config.atmosphere.central_body_name))
                    {
                        Mat3d orientation;
                        if (!config.gravity.ephemeris_provider->TryGetBodyFixedToIcrf(
                                body.name,
                                config.solver.start_ephemeris_time_tdb_seconds,
                                orientation) || !IsRotationMatrix(orientation))
                        {
                            return "The ephemeris provider cannot resolve the body-fixed frame for '" +
                                body.name + "'.";
                        }
                    }
                    if (config.atmosphere.enabled &&
                        body.name == config.atmosphere.central_body_name)
                    {
                        Vec3d angular_velocity_icrf_radps;
                        if (!config.gravity.ephemeris_provider->
                                TryGetBodyAngularVelocityIcrf(
                                    body.name,
                                    config.solver.
                                        start_ephemeris_time_tdb_seconds,
                                    angular_velocity_icrf_radps) ||
                            !IsFinite(angular_velocity_icrf_radps))
                        {
                            return "The ephemeris provider cannot resolve angular velocity for atmosphere body '" +
                                body.name + "'.";
                        }
                    }
                }
            }

            if (config.atmosphere.enabled)
            {
                const auto central_body = std::find_if(
                    config.gravity.bodies.begin(),
                    config.gravity.bodies.end(),
                    [&](const GravityBody& body)
                    {
                        return body.name ==
                            config.atmosphere.central_body_name;
                    });
                if (central_body == config.gravity.bodies.end())
                    return "The atmosphere central body is not present in the gravity-body catalog.";
                if (central_body->reference_radius_m <= 0.0)
                    return "The atmosphere central body needs a positive SPICE physical radius.";
                if (config.atmosphere.model_kind ==
                    AtmosphereModelKind::CubicHarrisPriesterEarth)
                {
                    if (central_body->naif_id != 399 &&
                        central_body->name != "Earth")
                    {
                        return "Cubic Harris-Priester is Earth-only; use a tabulated profile for another body.";
                    }
                    if (gravity_body_names.find("Sun") ==
                        gravity_body_names.end())
                    {
                        return "Cubic Harris-Priester requires the Sun in the ephemeris catalog for its diurnal bulge.";
                    }
                }
            }
            if (config.solar_radiation.enabled)
            {
                if (gravity_body_names.find(
                        config.solar_radiation.sun_body_name) ==
                    gravity_body_names.end())
                {
                    return "The SRP Sun body is not present in the gravity-body catalog.";
                }
                if (!std::isfinite(
                        config.solar_radiation.pressure_at_one_au_pa) ||
                    config.solar_radiation.pressure_at_one_au_pa <= 0.0)
                {
                    return "Enabled SRP requires a positive finite pressure at one AU.";
                }
                if (config.solar_radiation.facets.empty())
                {
                    return "Enabled SRP requires converter-generated proxy triangles.";
                }
            }

            // Load-producing objects store component indices that later become ABA-node loads.
            // Reject dangling indices before any force model dereferences component_poses.
            for (const OpticalFacet& facet : config.solar_radiation.facets)
            {
                if (facet.component_index >= config.vehicle.components.size())
                    return "An SRP facet references an invalid component.";
                const Vec3d edge_01 = facet.vertices_component_m[1] -
                    facet.vertices_component_m[0];
                const Vec3d edge_02 = facet.vertices_component_m[2] -
                    facet.vertices_component_m[0];
                if (!IsFinite(facet.vertices_component_m[0]) ||
                    !IsFinite(facet.vertices_component_m[1]) ||
                    !IsFinite(facet.vertices_component_m[2]) ||
                    Cross(edge_01, edge_02).Norm() <= 1.0e-18 ||
                    !std::isfinite(facet.absorption) ||
                    !std::isfinite(facet.specular_reflection) ||
                    !std::isfinite(facet.diffuse_reflection) ||
                    facet.absorption < 0.0 ||
                    facet.specular_reflection < 0.0 ||
                    facet.diffuse_reflection < 0.0 ||
                    facet.absorption > 1.0 ||
                    facet.specular_reflection > 1.0 ||
                    facet.diffuse_reflection > 1.0 ||
                    std::abs(facet.absorption + facet.specular_reflection +
                        facet.diffuse_reflection - 1.0) > 1.0e-6)
                {
                    return "An SRP proxy triangle needs three finite non-collinear vertices and optical fractions in [0,1] that sum to one.";
                }
            }
            for (const ThrusterDefinition& thruster : config.vehicle.thrusters)
            {
                const double ignition_elapsed_time =
                    ThrusterIgnitionElapsedTime(
                        thruster,
                        config.solver.start_ephemeris_time_tdb_seconds);
                const double shutdown_elapsed_time =
                    ThrusterShutdownElapsedTime(
                        thruster,
                        config.solver.start_ephemeris_time_tdb_seconds);
                if (thruster.component_index >= config.vehicle.components.size()) return "A thruster references an invalid mount component.";
                if (thruster.propellant_component_index == kInvalidIndex ||
                    thruster.propellant_component_index >=
                        config.vehicle.components.size())
                {
                    return "Every thruster must reference an existing variable-mass propellant component.";
                }
                const ComponentDefinition& propellant_component =
                    config.vehicle.components[
                        thruster.propellant_component_index];
                if (propellant_component.variable_mass_state_index ==
                    kInvalidIndex)
                {
                    return "Every thruster must reference an existing variable-mass propellant component.";
                }
                if (!IsFinite(thruster.application_point_component_m) ||
                    !IsFinite(thruster.direction_component) ||
                    thruster.direction_component.Norm() <= 1.0e-15 ||
                    !std::isfinite(thruster.ignition_ephemeris_time_tdb_seconds) ||
                    std::isnan(thruster.shutdown_ephemeris_time_tdb_seconds) ||
                    !std::isfinite(ignition_elapsed_time) ||
                    std::isnan(shutdown_elapsed_time) ||
                    shutdown_elapsed_time < ignition_elapsed_time)
                    return "A thruster has invalid geometry, capability, or firing times.";
                std::string curve_error = ValidateCurve(
                    "Thruster thrust profile", thruster.thrust_profile_n);
                if (!curve_error.empty()) return curve_error;
                curve_error = ValidateCurve(
                    "Thruster specific-impulse profile",
                    thruster.specific_impulse_profile_seconds);
                if (!curve_error.empty()) return curve_error;

                if (thruster.mode == ThrusterMode::PrescribedProfile)
                {
                    if (!std::isfinite(thruster.constant_thrust_n) ||
                        thruster.constant_thrust_n < 0.0 ||
                        !std::isfinite(
                            thruster.constant_specific_impulse_seconds) ||
                        thruster.constant_specific_impulse_seconds < 0.0)
                    {
                        return "A prescribed thruster has invalid constant thrust or specific impulse.";
                    }
                    if (thruster.maximum_thrust_n != 0.0)
                        return "Maximum thrust belongs only to commanded thrusters.";
                    if (!thruster.thrust_profile_n.samples.empty() &&
                        thruster.constant_thrust_n != 0.0)
                    {
                        return "A prescribed thruster must select either constant thrust or a thrust curve.";
                    }
                    if (!thruster.specific_impulse_profile_seconds.samples.empty() &&
                        thruster.constant_specific_impulse_seconds != 0.0)
                    {
                        return "A prescribed thruster must select either constant or profiled specific impulse.";
                    }

                    if (!thruster.thrust_profile_n.samples.empty())
                    {
                        if (thruster.thrust_profile_n.samples.size() < 2)
                            return "A prescribed thrust curve needs at least two samples.";
                        if (std::abs(
                                thruster.thrust_profile_n.samples.front().value) >
                                1.0e-12 ||
                            std::abs(
                                thruster.thrust_profile_n.samples.back().value) >
                                1.0e-12)
                        {
                            return "A prescribed thrust curve must start and finish at zero thrust.";
                        }
                        for (const ScalarSample& sample :
                            thruster.thrust_profile_n.samples)
                        {
                            if (sample.value < 0.0)
                                return "A prescribed thrust curve cannot contain negative thrust.";
                        }
                    }

                    for (const ScalarSample& sample :
                        thruster.specific_impulse_profile_seconds.samples)
                    {
                        if (sample.value <= 0.0)
                            return "A prescribed specific-impulse curve must contain only positive values.";
                    }

                    const bool can_produce_thrust =
                        thruster.thrust_profile_n.samples.empty()
                            ? thruster.constant_thrust_n > 0.0
                            : HasPositiveSample(thruster.thrust_profile_n);
                    if (can_produce_thrust &&
                        thruster.specific_impulse_profile_seconds.samples.empty() &&
                        thruster.constant_specific_impulse_seconds <= 0.0)
                    {
                        return "A prescribed thruster that produces thrust needs positive constant or profiled specific impulse.";
                    }
                }
                else
                {
                    if (!std::isfinite(thruster.maximum_thrust_n) ||
                        thruster.maximum_thrust_n <= 0.0)
                    {
                        return "A commanded thruster needs positive maximum thrust.";
                    }
                    if (thruster.constant_thrust_n != 0.0 ||
                        thruster.constant_specific_impulse_seconds != 0.0 ||
                        !thruster.thrust_profile_n.samples.empty() ||
                        !thruster.specific_impulse_profile_seconds.samples.empty())
                    {
                        return "A commanded thruster cannot contain prescribed thrust or specific-impulse data.";
                    }
                }
            }
            for (std::size_t wheel_index = 0;
                wheel_index < config.vehicle.reaction_wheels.size();
                ++wheel_index)
            {
                const ReactionWheelDefinition& wheel =
                    config.vehicle.reaction_wheels[wheel_index];
                if (wheel.component_index >= config.vehicle.components.size())
                    return "A reaction wheel references an invalid mount component.";
                if (!std::isfinite(wheel.axis_component.Norm()) || wheel.axis_component.Norm() <= 1.0e-15)
                    return "A reaction-wheel axis must be finite and nonzero.";
                if (std::isnan(wheel.maximum_momentum_nms) || wheel.maximum_momentum_nms < 0.0)
                    return "A reaction wheel has an invalid maximum momentum.";
                if (!std::isfinite(wheel.initial_momentum_nms))
                    return "A reaction wheel has a non-finite initial momentum.";
                if (wheel_index <
                        config.initial_state.internal_angular_momenta_nms.size() &&
                    std::abs(config.initial_state.
                        internal_angular_momenta_nms[wheel_index]) >
                        wheel.maximum_momentum_nms)
                {
                    return "A reaction wheel starts outside its momentum limit.";
                }
            }
            return {};
        }
    }

    std::string SimulationEngine::ValidateRequest(
        const SimulationRequest& request) const
    {
        return Validate(SimulationConfigBuilder().Build(request));
    }

    SimulationResult SimulationEngine::Run(
        const SimulationRequest& request,
        ISimulationObserver* observer) const
    {
        // Top-level backend workflow, equivalent to AVS Simulate.m: normalize the request,
        // validate it, create the ODE/integrator modules, advance sequentially, and return
        // the complete solution history. It never starts Unreal or uses Unreal physics.
        if (observer) observer->OnProgress(0.0, "Building simulation configuration");
        if (observer && observer->IsCancellationRequested())
            return Failure("Simulation cancelled.");
        const SimulationConfig config = SimulationConfigBuilder().Build(request);
        const std::string validation_error = Validate(config);
        if (!validation_error.empty()) return Failure(validation_error);

        ScenarioModules modules = ScenarioFactory().CreateModules(config);
        if (!modules.dynamics_model || !modules.integrator)
            return Failure("The requested simulation modules could not be created.");

        if (observer)
        {
            observer->OnProgress(5.0, "Initializing 6-DOF propagation");
            observer->OnLog("Sequential General6DofDynamics propagation started.");
        }

        StateRecorder recorder(config);
        SpacecraftState state = config.initial_state;
        const std::vector<ArticulationLimitCrossing> initial_constraints =
            FindInitialArticulationConstraints(config, state);
        if (!initial_constraints.empty())
        {
            const ArticulationImpulseEvaluation impulse =
                ApplyArticulationLimitImpulse(
                    config, initial_constraints, state);
            if (!impulse.solved)
                return Failure(impulse.message);
        }
        recorder.Record(state, modules.dynamics_model->ComputeDynamics(
            state.ephemeris_time_tdb_seconds, state, config));

        const double start_time =
            config.solver.start_ephemeris_time_tdb_seconds;
        // Duration-authored scenarios retain their original small elapsed value.
        // Subtracting two large ET endpoints can otherwise manufacture a terminal tail.
        const double final_elapsed_time =
            EffectiveDurationSeconds(config.solver);
        const double integration_step =
            config.solver.maximum_integrator_step_seconds;
        std::size_t next_output_index = 1;
        double last_recorded_elapsed_time = state.elapsed_time_seconds;
        std::size_t step_count = 0;

        while (state.elapsed_time_seconds < final_elapsed_time)
        {
            const double current_elapsed_time = state.elapsed_time_seconds;

            // Schedule in elapsed time so a microsecond remains representable even
            // when the absolute SPICE epoch is hundreds of millions of seconds.
            double step_endpoint = current_elapsed_time + integration_step;
            if (step_endpoint <= current_elapsed_time)
            {
                step_endpoint = std::nextafter(
                    current_elapsed_time,
                    std::numeric_limits<double>::infinity());
            }
            double target_elapsed_time =
                std::min(step_endpoint, final_elapsed_time);
            const double next_event_time = NextScheduledEvent(
                config, current_elapsed_time);

            double next_wheel_saturation =
                std::numeric_limits<double>::infinity();
            double next_articulation_limit =
                std::numeric_limits<double>::infinity();
            if (!config.vehicle.reaction_wheels.empty() ||
                !state.articulation_rates.empty())
            {
                const DynamicsEvaluation boundary_evaluation =
                    modules.dynamics_model->ComputeDynamics(
                        state.ephemeris_time_tdb_seconds, state, config);
                if (!config.vehicle.reaction_wheels.empty())
                {
                    next_wheel_saturation = NextPredictedWheelSaturation(
                        config, state, boundary_evaluation);
                }
                if (!state.articulation_rates.empty())
                {
                    next_articulation_limit = NextPredictedArticulationLimit(
                        config, state, boundary_evaluation);
                }
            }

            double next_output_time =
                config.solver.output_mode == OutputMode::FixedInterval
                    ? next_output_index * config.solver.output_step_seconds
                    : std::numeric_limits<double>::infinity();
            if (
                next_output_time < final_elapsed_time &&
                start_time + next_output_time ==
                    start_time + final_elapsed_time)
            {
                // At large positive or negative ET, a sub-ULP terminal tail
                // cannot produce a distinct absolute timestamp. Land on the
                // final state directly instead of recording duplicate ET rows.
                next_output_time = final_elapsed_time;
            }
            target_elapsed_time = std::min({
                target_elapsed_time,
                next_event_time,
                next_wheel_saturation,
                next_articulation_limit,
                next_output_time});

            const bool lands_on_scheduled_event =
                std::isfinite(next_event_time) &&
                target_elapsed_time == next_event_time;
            const bool lands_on_predicted_wheel_saturation =
                std::isfinite(next_wheel_saturation) &&
                target_elapsed_time == next_wheel_saturation;
            const bool lands_on_predicted_articulation_limit =
                std::isfinite(next_articulation_limit) &&
                target_elapsed_time == next_articulation_limit;
            const double requested_interval =
                target_elapsed_time - current_elapsed_time;
            if (!std::isfinite(requested_interval) ||
                requested_interval <= 0.0)
            {
                return recorder.BuildResult(
                    false,
                    "The propagation scheduler produced a nonpositive elapsed-time interval.");
            }

            // A discontinuous RHS must use the incoming command at every RK stage.
            // Integrate to the representable value immediately before the boundary;
            // the remaining spacing is advanced below with that incoming derivative.
            double integration_interval = requested_interval;
            double incoming_boundary_tail = 0.0;
            if (lands_on_scheduled_event ||
                lands_on_predicted_wheel_saturation ||
                lands_on_predicted_articulation_limit)
            {
                const double left_elapsed_time = std::nextafter(
                    target_elapsed_time,
                    current_elapsed_time);
                if (left_elapsed_time > current_elapsed_time)
                {
                    integration_interval =
                        left_elapsed_time - current_elapsed_time;
                    incoming_boundary_tail =
                        target_elapsed_time - left_elapsed_time;
                }
                else
                {
                    integration_interval = 0.0;
                    incoming_boundary_tail = requested_interval;
                }
            }

            IntegrationAdvance advance;
            if (integration_interval > 0.0)
            {
                advance = modules.integrator->Advance(
                    *modules.dynamics_model,
                    config,
                    state,
                    integration_interval,
                    observer);
            }
            else
            {
                advance.state = state;
                advance.success = true;
            }
            step_count += advance.attempted_steps;
            if (!advance.success)
                return recorder.BuildResult(false, advance.message);

            SpacecraftState candidate = std::move(advance.state);
            if (incoming_boundary_tail > 0.0)
            {
                const DynamicsEvaluation incoming_evaluation =
                    modules.dynamics_model->ComputeDynamics(
                        candidate.ephemeris_time_tdb_seconds,
                        candidate,
                        config);
                AdvanceStateAlongIncomingDerivative(
                    candidate,
                    incoming_evaluation.derivative,
                    incoming_boundary_tail,
                    target_elapsed_time,
                    config);
            }
            std::vector<WheelLimitCrossing> wheel_crossings =
                FindWheelLimitCrossings(config, state, candidate);
            std::vector<ArticulationLimitCrossing> articulation_crossings =
                FindArticulationLimitCrossings(config, state, candidate);
            if (!wheel_crossings.empty() || !articulation_crossings.empty())
            {
                // A time/state-dependent controller can invalidate the linear
                // start-of-step prediction. Locate the first actual crossing and
                // retain the incoming torque through that instant.
                double low_interval = 0.0;
                double high_interval = requested_interval;
                SpacecraftState low_state = state;
                SpacecraftState high_state = candidate;
                const double root_tolerance = std::max(
                    1.0e-11,
                    64.0 * std::numeric_limits<double>::epsilon() *
                        std::max(1.0, integration_interval));

                for (int iteration = 0;
                    iteration < 64 &&
                    high_interval - low_interval > root_tolerance;
                    ++iteration)
                {
                    const double middle_interval =
                        0.5 * (low_interval + high_interval);
                    IntegrationAdvance trial = modules.integrator->Advance(
                        *modules.dynamics_model,
                        config,
                        state,
                        middle_interval,
                        observer);
                    step_count += trial.attempted_steps;
                    if (!trial.success)
                        return recorder.BuildResult(false, trial.message);

                    const bool wheel_crossed = !FindWheelLimitCrossings(
                        config, state, trial.state).empty();
                    const bool articulation_crossed =
                        !FindArticulationLimitCrossings(
                            config, state, trial.state).empty();
                    if (!wheel_crossed && !articulation_crossed)
                    {
                        low_interval = middle_interval;
                        low_state = std::move(trial.state);
                    }
                    else
                    {
                        high_interval = middle_interval;
                        high_state = std::move(trial.state);
                    }
                }

                wheel_crossings = FindWheelLimitCrossings(
                    config, state, high_state);
                articulation_crossings = FindArticulationLimitCrossings(
                    config, state, high_state);
                state = std::move(low_state);
                const double root_tail = high_interval - low_interval;
                if (root_tail > 0.0)
                {
                    const DynamicsEvaluation incoming_evaluation =
                        modules.dynamics_model->ComputeDynamics(
                            state.ephemeris_time_tdb_seconds,
                            state,
                            config);
                    AdvanceStateAlongIncomingDerivative(
                        state,
                        incoming_evaluation.derivative,
                        root_tail,
                        current_elapsed_time + high_interval,
                        config);
                }
                for (const WheelLimitCrossing& crossing : wheel_crossings)
                {
                    state.internal_angular_momenta_nms[crossing.index] =
                        crossing.boundary_momentum_nms;
                }
                if (!articulation_crossings.empty())
                {
                    const ArticulationImpulseEvaluation impulse =
                        ApplyArticulationLimitImpulse(
                            config, articulation_crossings, state);
                    if (!impulse.solved)
                    {
                        return recorder.BuildResult(false, impulse.message);
                    }
                }
            }
            else
            {
                state = std::move(candidate);
                state.elapsed_time_seconds = target_elapsed_time;
                state.ephemeris_time_tdb_seconds =
                    start_time + target_elapsed_time;
                if (lands_on_predicted_wheel_saturation)
                    SnapWheelMomentaNearLimits(config, state);
            }

            if (step_count > config.solver.maximum_integration_steps)
            {
                return recorder.BuildResult(
                    false,
                    "Maximum integration-step count reached before final time.");
            }

            const bool at_final_time =
                state.elapsed_time_seconds >= final_elapsed_time;
            const bool at_fixed_output_time =
                config.solver.output_mode == OutputMode::FixedInterval &&
                state.elapsed_time_seconds == next_output_time;
            if (config.solver.output_mode == OutputMode::FixedInterval &&
                state.elapsed_time_seconds > next_output_time)
            {
                return recorder.BuildResult(
                    false,
                    "The propagation scheduler advanced past a fixed output time.");
            }
            const bool at_output_time =
                config.solver.output_mode == OutputMode::EveryIntegratorStep ||
                at_fixed_output_time;
            const bool is_new_output_time =
                state.elapsed_time_seconds > last_recorded_elapsed_time;
            if ((at_output_time || at_final_time) && is_new_output_time)
            {
                if (recorder.SampleCount() >=
                    config.solver.maximum_output_samples)
                {
                    return recorder.BuildResult(
                        false,
                        "Maximum output-sample count reached before final time.");
                }
                recorder.Record(state, modules.dynamics_model->ComputeDynamics(
                    state.ephemeris_time_tdb_seconds, state, config));
                last_recorded_elapsed_time = state.elapsed_time_seconds;
                if (at_fixed_output_time)
                {
                    ++next_output_index;
                }
            }

            if (observer)
            {
                const double duration = std::max(1.0e-12, final_elapsed_time);
                const double fraction = state.elapsed_time_seconds / duration;
                observer->OnProgress(
                    std::min(95.0, 5.0 + 90.0 * fraction),
                    "Propagating 6-DOF state");
            }
        }

        if (observer)
        {
            observer->OnProgress(100.0, "Simulation complete");
            observer->OnLog("Solution history assembled.");
        }
        return recorder.BuildResult(true, "Simulation completed successfully.");
    }
}
