// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Forces/GravityModel.h"

// Evaluates multi-body Newtonian gravity, body-fixed harmonics, and optional 1PN terms.

#include "TGSim/Environment/EnvironmentModels.h"
#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <vector>

namespace tgsim
{
    namespace
    {
        double NormalizationFactor(int degree, int order)
        {
            // Convert a fully normalized Cbar_nm/Sbar_nm coefficient to unnormalized form.
            // Spherical-harmonic coefficients exist only for n >= 0 and 0 <= m <= n.
            if (degree < 0 || order < 0 || order > degree)
            {
                return 0.0;
            }

            // alpha_nm = sqrt((2-delta_0m)(2n+1)(n-m)!/(n+m)!).
            const double two_minus_delta = order == 0 ? 1.0 : 2.0;
            // lgamma(k+1) = log(k!), avoiding direct factorial overflow.
            const double log_factorial_ratio =
                std::lgamma(static_cast<double>(degree - order + 1)) -
                std::lgamma(static_cast<double>(degree + order + 1));
            return std::sqrt(two_minus_delta * (2.0 * degree + 1.0) * std::exp(log_factorial_ratio));
        }

        void GetUnnormalizedCoefficient(const GravityBody& body, int degree, int order, double& c, double& s)
        {
            // Look up one requested (n,m) pair; return the implicit monopole or zero if absent.
            // The monopole is implicit and non-overridable: C_00 = 1, S_00 = 0.
            if (degree == 0 && order == 0)
            {
                c = 1.0;
                s = 0.0;
                return;
            }

            // Any perturbation coefficient absent from the model is zero.
            c = 0.0;
            s = 0.0;

            for (const HarmonicCoefficient& coefficient : body.harmonics)
            {
                if (coefficient.degree == degree && coefficient.order == order)
                {
                    const double alpha = NormalizationFactor(degree, order);
                    c = alpha * coefficient.normalized_c;
                    s = alpha * coefficient.normalized_s;
                    return;
                }
            }
        }

        bool IsInsideActivationRadius(
            const GravityBody& body,
            const GravitySettings& settings,
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& spacecraft)
        {
            if (body.automatic_gravity_activation_radius_m <= 0.0) return false;
            const BodyState body_state = ResolveBodyState(
                body, settings, ephemeris_time_tdb_seconds);
            return (spacecraft.position_icrf_m - body_state.position_icrf_m).Norm() <=
                body.automatic_gravity_activation_radius_m;
        }

        struct GravitySystemIndices
        {
            std::size_t barycenter_index = kInvalidIndex;
            std::vector<std::size_t> member_indices;
        };

        std::vector<bool> ResolveActiveGravityBodies(
            const GravitySettings& settings,
            double ephemeris_time_tdb_seconds,
            const SpacecraftState& spacecraft)
        {
            const std::size_t body_count = settings.bodies.size();
            std::vector<bool> active(body_count, false);
            std::unordered_map<std::string, GravitySystemIndices> systems;

            // Rule shared by every source: a checked source is active at every
            // distance; an unchecked source activates only inside its own
            // positive automatic-activation radius.
            for (std::size_t index = 0; index < body_count; ++index)
            {
                const GravityBody& body = settings.bodies[index];
                active[index] = body.gravity_enabled ||
                    IsInsideActivationRadius(
                        body, settings, ephemeris_time_tdb_seconds, spacecraft);

                if (body.gravity_source_role == GravitySourceRole::SystemBarycenter)
                {
                    systems[body.gravity_system_name].barycenter_index = index;
                }
                else if (body.gravity_source_role == GravitySourceRole::SystemMember)
                {
                    systems[body.gravity_system_name].member_indices.push_back(index);
                }
            }

            for (const auto& system_entry : systems)
            {
                const GravitySystemIndices& system = system_entry.second;
                if (system.barycenter_index == kInvalidIndex) continue;

                const GravityBody& barycenter =
                    settings.bodies[system.barycenter_index];
                bool any_explicit_member = false;
                bool any_member_active = false;
                for (const std::size_t member_index : system.member_indices)
                {
                    any_explicit_member = any_explicit_member ||
                        settings.bodies[member_index].gravity_enabled;
                    any_member_active = any_member_active || active[member_index];
                }

                // An explicitly selected member has highest priority. This is
                // the backend equivalent of the HUD unchecking its barycenter.
                if (any_explicit_member)
                {
                    active[system.barycenter_index] = false;
                    continue;
                }

                // An active far-field barycenter switches to the complete
                // resolved member set inside its separate resolution radius.
                if (active[system.barycenter_index] &&
                    barycenter.barycenter_resolution_radius_m > 0.0)
                {
                    const BodyState barycenter_state = ResolveBodyState(
                        barycenter, settings, ephemeris_time_tdb_seconds);
                    const double distance_to_barycenter =
                        (spacecraft.position_icrf_m -
                            barycenter_state.position_icrf_m).Norm();
                    if (distance_to_barycenter <=
                        barycenter.barycenter_resolution_radius_m)
                    {
                        active[system.barycenter_index] = false;
                        for (const std::size_t member_index : system.member_indices)
                        {
                            active[member_index] = true;
                        }
                        continue;
                    }
                }

                // Outside the full-system resolution region, any member that
                // activated through its own radius still suppresses the
                // barycenter. The represented system mass is never summed twice.
                if (any_member_active)
                {
                    active[system.barycenter_index] = false;
                }
            }

            return active;
        }

        double EffectiveGravitationalParameter(const GravityBody& body)
        {
            // A harmonic solution's GM scales both its implicit C00 monopole
            // and every higher-degree coefficient. Point masses use SPICE GM.
            return body.maximum_harmonic_degree > 0
                ? body.harmonic_model_gravitational_parameter_m3ps2
                : body.gravitational_parameter_m3ps2;
        }
    }

    Vec3d GravityModel::EvaluateNewtonianBody(
        const SpacecraftState& state,
        const GravityBody& body,
        const BodyState& body_state,
        const Mat3d& body_fixed_to_icrf) const
    {
        // Return acceleration caused by one body, selecting point-mass or harmonic evaluation.
        const Vec3d relative_icrf = state.position_icrf_m - body_state.position_icrf_m;
        const double distance = relative_icrf.Norm();
        const double gravitational_parameter =
            EffectiveGravitationalParameter(body);
        if (gravitational_parameter <= 0.0 || distance <= 1.0e-9) return {};

        const int maximum_degree = std::max(0, body.maximum_harmonic_degree);
        if (maximum_degree == 0)
        {
            // Point-mass gravity: a = -mu r / |r|^3.
            return -gravitational_parameter * relative_icrf /
                (distance * distance * distance);
        }

        // Evaluate harmonics in body-fixed axes: r_BF = R_BF_to_I^T r_I.
        const Vec3d relative_body =
            body_fixed_to_icrf.Transposed() * relative_icrf;
        const double x = relative_body.x;
        const double y = relative_body.y;
        const double z = relative_body.z;
        const double radius = relative_body.Norm();
        const double reference_radius =
            body.harmonic_model_reference_radius_m;
        const double radius_squared = radius * radius;
        const int recurrence_degree = maximum_degree + 1;

        std::vector<std::vector<double>> v(recurrence_degree + 1, std::vector<double>(recurrence_degree + 1, 0.0));
        std::vector<std::vector<double>> w(recurrence_degree + 1, std::vector<double>(recurrence_degree + 1, 0.0));
        // Harmonic recurrence seed: V_00 = R_ref / r, W_00 = 0.
        v[0][0] = reference_radius / radius;

        // Diagonal terms V_mm and W_mm from report Equations (48)-(49).
        for (int order = 1; order <= recurrence_degree; ++order)
        {
            const double factor = 2.0 * order - 1.0;
            v[order][order] = factor * reference_radius / radius_squared *
                (x * v[order - 1][order - 1] - y * w[order - 1][order - 1]);
            w[order][order] = factor * reference_radius / radius_squared *
                (x * w[order - 1][order - 1] + y * v[order - 1][order - 1]);
        }

        // Remaining terms V_nm and W_nm from report Equations (50)-(51).
        for (int order = 0; order <= recurrence_degree; ++order)
        {
            for (int degree = order + 1; degree <= recurrence_degree; ++degree)
            {
                const double denominator = static_cast<double>(degree - order);
                const double first = (2.0 * degree - 1.0) / denominator * z * reference_radius / radius_squared;
                const double second = (degree + order - 1.0) / denominator * reference_radius * reference_radius / radius_squared;
                v[degree][order] = first * v[degree - 1][order];
                w[degree][order] = first * w[degree - 1][order];
                if (degree >= 2)
                {
                    v[degree][order] -= second * v[degree - 2][order];
                    w[degree][order] -= second * w[degree - 2][order];
                }
            }
        }

        Vec3d acceleration_body;
        // Harmonic acceleration scale: a_BF = (mu / R_ref^2) * sum(a_nm).
        const double acceleration_scale =
            gravitational_parameter / (reference_radius * reference_radius);
        for (int degree = 0; degree <= maximum_degree; ++degree)
        {
            for (int order = 0; order <= degree; ++order)
            {
                double c = 0.0;
                double s = 0.0;
                GetUnnormalizedCoefficient(body, degree, order, c, s);
                if (order == 0)
                {
                    acceleration_body.x += acceleration_scale * (-c * v[degree + 1][1]);
                    acceleration_body.y += acceleration_scale * (-c * w[degree + 1][1]);
                    acceleration_body.z += acceleration_scale * (degree + 1.0) * (-c * v[degree + 1][0]);
                }
                else
                {
                    const double lower_factor = (degree - order + 1.0) * (degree - order + 2.0);
                    acceleration_body.x += 0.5 * acceleration_scale * (
                        -c * v[degree + 1][order + 1] - s * w[degree + 1][order + 1] +
                        lower_factor * (c * v[degree + 1][order - 1] + s * w[degree + 1][order - 1]));
                    acceleration_body.y += 0.5 * acceleration_scale * (
                        -c * w[degree + 1][order + 1] + s * v[degree + 1][order + 1] +
                        lower_factor * (-c * w[degree + 1][order - 1] + s * v[degree + 1][order - 1]));
                    acceleration_body.z += acceleration_scale * (degree - order + 1.0) *
                        (-c * v[degree + 1][order] - s * w[degree + 1][order]);
                }
            }
        }

        return body_fixed_to_icrf * acceleration_body;
    }

    Vec3d GravityModel::EvaluateFirstPostNewtonian(
        const SpacecraftState& state,
        const GravitySettings& settings,
        const std::vector<bool>& active_bodies,
        const std::vector<BodyState>& body_states) const
    {
        // Return the combined mass-independent EIH test-particle acceleration correction.
        const std::size_t count = settings.bodies.size();

        Vec3d correction;
        for (std::size_t c_index = 0; c_index < count; ++c_index)
        {
            const GravityBody& c_body = settings.bodies[c_index];
            if (c_index >= active_bodies.size() || !active_bodies[c_index]) continue;
            const BodyState& c_state = body_states[c_index];
            const Vec3d r_bc = state.position_icrf_m - c_state.position_icrf_m;
            const double distance_bc = r_bc.Norm();
            const double mu_c = EffectiveGravitationalParameter(c_body);
            if (distance_bc <= 1.0e-9 || mu_c <= 0.0) continue;

            double bracket = state.velocity_icrf_mps.NormSquared()
                - 4.0 * Dot(state.velocity_icrf_mps, c_state.velocity_icrf_mps)
                + 2.0 * c_state.velocity_icrf_mps.NormSquared()
                - 1.5 * std::pow(Dot(r_bc, c_state.velocity_icrf_mps) / distance_bc, 2.0)
                - 4.0 * mu_c / distance_bc;

            // Add the D-body potential terms inside the EIH 1PN bracket.
            Vec3d third_body_sum;
            for (std::size_t d_index = 0; d_index < count; ++d_index)
            {
                if (d_index == c_index) continue;
                const GravityBody& d_body = settings.bodies[d_index];
                if (d_index >= active_bodies.size() || !active_bodies[d_index]) continue;
                const double mu_d = EffectiveGravitationalParameter(d_body);
                if (mu_d <= 0.0) continue;
                const Vec3d r_bd = state.position_icrf_m - body_states[d_index].position_icrf_m;
                const Vec3d r_cd = c_state.position_icrf_m - body_states[d_index].position_icrf_m;
                const double distance_bd = r_bd.Norm();
                const double distance_cd = r_cd.Norm();
                if (distance_bd <= 1.0e-9 || distance_cd <= 1.0e-9) continue;

                bracket +=
                    -mu_d / distance_cd
                    -4.0 * mu_d / distance_bd
                    +mu_d * Dot(r_bc, r_cd) /
                        (2.0 * distance_cd * distance_cd * distance_cd);
                third_body_sum += mu_d * r_cd /
                    (distance_cd * distance_cd * distance_cd);
            }

            const double distance_cubed = distance_bc * distance_bc * distance_bc;
            // First post-Newtonian acceleration: the three sums in report Equation (29).
            correction += -mu_c * r_bc / distance_cubed * bracket;
            correction += mu_c * (state.velocity_icrf_mps - c_state.velocity_icrf_mps) / distance_cubed *
                (4.0 * Dot(state.velocity_icrf_mps, r_bc) - 3.0 * Dot(c_state.velocity_icrf_mps, r_bc));
            correction += -3.5 * mu_c / distance_bc * third_body_sum;
        }

        // Equation (29) is written as c^2 a_1PN; divide by c^2 here.
        return correction / (kSpeedOfLightMps * kSpeedOfLightMps);
    }

    GravityEvaluation GravityModel::ComputeGravity(
        double time_seconds,
        const SpacecraftState& state,
        const SimulationConfig& config,
        const Vec3d& center_of_mass_body_m,
        const std::vector<ComponentPose>& component_poses) const
    {
        // Resolve source selection from the spacecraft CM once. Component-CM
        // probes must not independently cross a switching boundary.
        const std::vector<bool> active_bodies = ResolveActiveGravityBodies(
            config.gravity, time_seconds, state);

        // Body ephemerides and body-fixed orientations depend on the epoch, not
        // on which component CM is being probed. Resolve each active source once
        // and reuse it for every component gravity-gradient evaluation below.
        const std::size_t body_count = config.gravity.bodies.size();
        std::vector<BodyState> body_states(body_count);
        std::vector<Mat3d> body_fixed_to_icrf(body_count);
        for (std::size_t body_index = 0; body_index < body_count; ++body_index)
        {
            if (body_index >= active_bodies.size() ||
                !active_bodies[body_index]) continue;

            const GravityBody& body = config.gravity.bodies[body_index];
            body_states[body_index] = ResolveBodyState(
                body, config.gravity, time_seconds);
            if (body.maximum_harmonic_degree > 0)
            {
                body_fixed_to_icrf[body_index] = ResolveBodyFixedToIcrf(
                    body, config.gravity, time_seconds);
            }
        }

        // Evaluate all selected gravity terms at one arbitrary ICRF probe state.
        const auto acceleration_at = [&](const SpacecraftState& probe)
        {
            Vec3d acceleration;
            for (std::size_t body_index = 0;
                body_index < config.gravity.bodies.size();
                ++body_index)
            {
                if (body_index >= active_bodies.size() ||
                    !active_bodies[body_index]) continue;
                const GravityBody& body = config.gravity.bodies[body_index];
                acceleration += EvaluateNewtonianBody(
                    probe,
                    body,
                    body_states[body_index],
                    body_fixed_to_icrf[body_index]);
            }
            if (config.gravity.include_first_post_newtonian_correction)
            {
                acceleration += EvaluateFirstPostNewtonian(
                    probe,
                    config.gravity,
                    active_bodies,
                    body_states);
            }
            return acceleration;
        };

        GravityEvaluation result;
        result.component_loads.resize(config.vehicle.components.size());
        if (config.vehicle.components.empty() ||
            component_poses.size() != config.vehicle.components.size())
        {
            // Defensive fallback for an incomplete component geometry. Valid
            // simulations always take the component-resolved path below.
            result.acceleration_icrf_mps2 = acceleration_at(state);
            result.force_icrf_n = state.mass_kg * result.acceleration_icrf_mps2;
            return result;
        }

        const Mat3d body_to_icrf = state.attitude_body_to_icrf.ToRotationMatrix();
        const Mat3d icrf_to_body = body_to_icrf.Transposed();
        double total_mass_kg = 0.0;
        for (std::size_t component_index = 0;
            component_index < config.vehicle.components.size();
            ++component_index)
        {
            const ComponentDefinition& component =
                config.vehicle.components[component_index];
            const ComponentPose& pose = component_poses[component_index];
            const double component_mass_kg =
                MassPropertiesModel::ComponentMass(component, state);
            if (component_mass_kg <= 0.0) continue;

            const Vec3d component_center_body_m =
                ComponentKinematicsModel::PointToBody(
                    pose, component.center_of_mass_component_m);
            SpacecraftState probe = state;
            // The propagated position is the total CM. Move the probe to this
            // component's CM before evaluating the celestial gravity field.
            probe.position_icrf_m += body_to_icrf *
                (component_center_body_m - center_of_mass_body_m);
            probe.mass_kg = component_mass_kg;

            const Vec3d component_acceleration_icrf_mps2 =
                acceleration_at(probe);
            const Vec3d component_force_icrf_n =
                component_mass_kg * component_acceleration_icrf_mps2;
            const Vec3d component_force_body_n =
                icrf_to_body * component_force_icrf_n;
            const Vec3d component_force_component_n =
                pose.component_to_body.Transposed() * component_force_body_n;

            ComponentLoad& component_load =
                result.component_loads[component_index];
            component_load.force_icrf_n = component_force_icrf_n;
            // Point-force moment about component origin:
            // M_Oi^Ci = r_CMi/Oi^Ci x F_i^Ci.
            component_load.torque_about_component_origin_component_nm = Cross(
                component.center_of_mass_component_m,
                component_force_component_n);

            result.force_icrf_n += component_force_icrf_n;
            // Total gravity-gradient moment about spacecraft CM:
            // M_CM^B = sum_i (r_CMi^B-r_CM^B) x F_i^B.
            result.torque_about_cm_body_nm += Cross(
                component_center_body_m - center_of_mass_body_m,
                component_force_body_n);
            total_mass_kg += component_mass_kg;
        }

        if (total_mass_kg > 0.0)
            result.acceleration_icrf_mps2 = result.force_icrf_n / total_mass_kg;
        return result;
    }
}
