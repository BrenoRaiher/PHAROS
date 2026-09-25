// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Environment/EnvironmentModels.h"

// Resolves celestial-body states/orientations and evaluates the uploaded
// low-density atmosphere contracts used by AerodynamicsModel.

#include <algorithm>
#include <cmath>
#include <limits>

namespace tgsim
{
    namespace
    {
        constexpr double kBoltzmannConstantJpk = 1.380649e-23;
        constexpr double kHarrisPriesterLagRad = kPi / 6.0;
        constexpr double kHarrisPriesterScaleHeightBlendHalfWidthM = 500.0;
        constexpr double kHarrisPriesterExponentEpsilon = 1.0e-3;

        template <typename Sample>
        bool FindAltitudeBracket(
            const std::vector<Sample>& samples,
            double altitude_m,
            std::size_t& lower_index,
            double& interpolation_fraction)
        {
            if (samples.size() < 2 ||
                altitude_m < samples.front().altitude_m ||
                altitude_m > samples.back().altitude_m)
            {
                return false;
            }

            if (altitude_m == samples.back().altitude_m)
            {
                lower_index = samples.size() - 2;
                interpolation_fraction = 1.0;
                return true;
            }

            const auto upper = std::upper_bound(
                samples.begin(),
                samples.end(),
                altitude_m,
                [](double altitude, const Sample& sample)
                {
                    return altitude < sample.altitude_m;
                });
            lower_index = static_cast<std::size_t>(
                std::distance(samples.begin(), upper) - 1);
            const double altitude_span =
                samples[lower_index + 1].altitude_m -
                samples[lower_index].altitude_m;
            interpolation_fraction =
                (altitude_m - samples[lower_index].altitude_m) / altitude_span;
            return true;
        }

        bool EvaluateTabulatedDensity(
            const std::vector<AtmosphereDensitySample>& samples,
            double altitude_m,
            double& density_kgpm3)
        {
            std::size_t lower = 0;
            double alpha = 0.0;
            if (!FindAltitudeBracket(samples, altitude_m, lower, alpha)) return false;

            // Log-linear interpolation is an exponential segment:
            // ln(rho) = ln(rho_0) + alpha [ln(rho_1)-ln(rho_0)].
            const double log_density =
                std::log(samples[lower].density_kgpm3) + alpha *
                (std::log(samples[lower + 1].density_kgpm3) -
                 std::log(samples[lower].density_kgpm3));
            density_kgpm3 = std::exp(log_density);
            return true;
        }

        bool EvaluateThermodynamicProfile(
            const std::vector<AtmosphereThermodynamicSample>& samples,
            double altitude_m,
            AtmosphereState& state)
        {
            std::size_t lower = 0;
            double alpha = 0.0;
            if (!FindAltitudeBracket(samples, altitude_m, lower, alpha)) return false;

            const AtmosphereThermodynamicSample& a = samples[lower];
            const AtmosphereThermodynamicSample& b = samples[lower + 1];
            const auto lerp = [alpha](double first, double second)
            {
                return first + alpha * (second - first);
            };
            state.temperature_k = lerp(a.temperature_k, b.temperature_k);
            state.mean_particle_mass_kg = lerp(
                a.mean_particle_mass_kg, b.mean_particle_mass_kg);
            state.effective_collision_cross_section_m2 = lerp(
                a.effective_collision_cross_section_m2,
                b.effective_collision_cross_section_m2);
            return true;
        }

        double EvaluateCubicPolynomial(
            const std::array<double, 4>& coefficients,
            double f107_sfu)
        {
            // rho(F) = c0 + c1 F + c2 F^2 + c3 F^3.
            return ((coefficients[3] * f107_sfu + coefficients[2]) *
                f107_sfu + coefficients[1]) * f107_sfu + coefficients[0];
        }

        double EvaluateReferenceDensity(
            const CubicHarrisPriesterDensitySample& sample,
            double f107_sfu,
            bool maximum_envelope)
        {
            return EvaluateCubicPolynomial(
                maximum_envelope
                    ? sample.maximum_density_coefficients_kgpm3
                    : sample.minimum_density_coefficients_kgpm3,
                f107_sfu);
        }

        double ScaleHeight(
            double lower_altitude_m,
            double upper_altitude_m,
            double lower_density_kgpm3,
            double upper_density_kgpm3)
        {
            // H_i = -(h_{i+1}-h_i) / ln(rho_{i+1}/rho_i).
            return -(upper_altitude_m - lower_altitude_m) /
                std::log(upper_density_kgpm3 / lower_density_kgpm3);
        }

        double SmoothStepThirdOrder(double x)
        {
            // Junkins/Jancaitis seventh-degree weighting used by the cubic
            // Harris-Priester implementation for third-order continuity:
            // w = x^4 (-20 x^3 + 70 x^2 - 84 x + 35), 0 <= x <= 1.
            x = std::clamp(x, 0.0, 1.0);
            const double x2 = x * x;
            const double x3 = x2 * x;
            const double x4 = x3 * x;
            return x4 * (-20.0 * x3 + 70.0 * x2 - 84.0 * x + 35.0);
        }

        double EvaluateHarrisPriesterEnvelope(
            const std::vector<CubicHarrisPriesterDensitySample>& samples,
            double f107_sfu,
            double altitude_m,
            bool maximum_envelope)
        {
            std::size_t lower = 0;
            double ignored_alpha = 0.0;
            if (!FindAltitudeBracket(samples, altitude_m, lower, ignored_alpha))
                return 0.0;

            const auto density_at = [&](std::size_t index)
            {
                return EvaluateReferenceDensity(
                    samples[index], f107_sfu, maximum_envelope);
            };
            const double lower_altitude = samples[lower].altitude_m;
            const double upper_altitude = samples[lower + 1].altitude_m;
            const double lower_density = density_at(lower);
            const double upper_density = density_at(lower + 1);
            const double nominal_scale_height = ScaleHeight(
                lower_altitude, upper_altitude, lower_density, upper_density);

            double reference_altitude = lower_altitude;
            double reference_density = lower_density;
            double effective_scale_height = nominal_scale_height;

            if (lower > 0 &&
                altitude_m <= lower_altitude +
                    kHarrisPriesterScaleHeightBlendHalfWidthM)
            {
                const double preceding_density = density_at(lower - 1);
                const double preceding_scale_height = ScaleHeight(
                    samples[lower - 1].altitude_m,
                    lower_altitude,
                    preceding_density,
                    lower_density);
                const double blend_start = lower_altitude -
                    kHarrisPriesterScaleHeightBlendHalfWidthM;
                const double blend_fraction =
                    (altitude_m - blend_start) /
                    (2.0 * kHarrisPriesterScaleHeightBlendHalfWidthM);
                effective_scale_height = preceding_scale_height +
                    SmoothStepThirdOrder(blend_fraction) *
                    (nominal_scale_height - preceding_scale_height);
            }
            else if (lower + 2 < samples.size() &&
                altitude_m >= upper_altitude -
                    kHarrisPriesterScaleHeightBlendHalfWidthM)
            {
                const double following_density = density_at(lower + 2);
                const double following_scale_height = ScaleHeight(
                    upper_altitude,
                    samples[lower + 2].altitude_m,
                    upper_density,
                    following_density);
                const double blend_start = upper_altitude -
                    kHarrisPriesterScaleHeightBlendHalfWidthM;
                const double blend_fraction =
                    (altitude_m - blend_start) /
                    (2.0 * kHarrisPriesterScaleHeightBlendHalfWidthM);
                effective_scale_height = nominal_scale_height +
                    SmoothStepThirdOrder(blend_fraction) *
                    (following_scale_height - nominal_scale_height);
                // Referencing the upper station makes the smoothed expression
                // reproduce its tabulated density exactly at the boundary.
                reference_altitude = upper_altitude;
                reference_density = upper_density;
            }

            // rho(h) = rho_ref exp[(h_ref-h)/H'(h)].
            return reference_density * std::exp(
                (reference_altitude - altitude_m) / effective_scale_height);
        }

        double EvaluateEarthEllipsoidAltitude(const Vec3d& position_ecef_m)
        {
            // WGS-84 ellipsoidal height. Cubic Harris-Priester is Earth-specific,
            // so spherical altitude is not used for this model.
            constexpr double semi_major_axis_m = 6378137.0;
            constexpr double inverse_flattening = 298.257223563;
            constexpr double flattening = 1.0 / inverse_flattening;
            constexpr double eccentricity_squared =
                flattening * (2.0 - flattening);
            constexpr double semi_minor_axis_m =
                semi_major_axis_m * (1.0 - flattening);

            const double cylindrical_radius = std::hypot(
                position_ecef_m.x, position_ecef_m.y);
            if (cylindrical_radius <= 1.0e-9)
                return std::abs(position_ecef_m.z) - semi_minor_axis_m;

            double latitude = std::atan2(
                position_ecef_m.z,
                cylindrical_radius * (1.0 - eccentricity_squared));
            double altitude_m = 0.0;
            for (int iteration = 0; iteration < 8; ++iteration)
            {
                const double sin_latitude = std::sin(latitude);
                const double prime_vertical_radius = semi_major_axis_m /
                    std::sqrt(1.0 - eccentricity_squared *
                        sin_latitude * sin_latitude);
                altitude_m = cylindrical_radius / std::cos(latitude) -
                    prime_vertical_radius;
                latitude = std::atan2(
                    position_ecef_m.z,
                    cylindrical_radius *
                        (1.0 - eccentricity_squared * prime_vertical_radius /
                            (prime_vertical_radius + altitude_m)));
            }
            return altitude_m;
        }

        double EvaluateCubicHarrisPriesterDensity(
            const CubicHarrisPriesterSettings& settings,
            double altitude_m,
            const Vec3d& relative_position_icrf_m,
            const Vec3d& central_body_relative_inertial_velocity_icrf_mps,
            const Mat3d& central_body_fixed_to_icrf,
            const Vec3d& sun_direction_icrf)
        {
            if (settings.density_samples.size() < 2 ||
                altitude_m < settings.density_samples.front().altitude_m ||
                altitude_m > settings.density_samples.back().altitude_m)
            {
                return 0.0;
            }

            const double minimum_density = EvaluateHarrisPriesterEnvelope(
                settings.density_samples,
                settings.centered_average_f107_sfu,
                altitude_m,
                false);
            const double maximum_density = EvaluateHarrisPriesterEnvelope(
                settings.density_samples,
                settings.centered_average_f107_sfu,
                altitude_m,
                true);

            const Vec3d orbit_normal = Cross(
                relative_position_icrf_m,
                central_body_relative_inertial_velocity_icrf_mps);
            const Vec3d equatorial_north_icrf =
                central_body_fixed_to_icrf * Vec3d::UnitZ();
            double sine_inclination_squared = 0.0;
            if (orbit_normal.Norm() > 1.0e-12)
            {
                const double cosine_inclination = std::clamp(
                    Dot(orbit_normal.Normalized(), equatorial_north_icrf.Normalized()),
                    -1.0,
                    1.0);
                sine_inclination_squared = std::max(
                    0.0, 1.0 - cosine_inclination * cosine_inclination);
            }
            // n = 2 + epsilon + 4 sin^2(i).
            const double diurnal_exponent = 2.0 +
                kHarrisPriesterExponentEpsilon +
                4.0 * sine_inclination_squared;

            const Vec3d bulge_apex_icrf = RotationAroundAxis(
                equatorial_north_icrf,
                kHarrisPriesterLagRad) * sun_direction_icrf.Normalized();
            const double cosine_psi = std::clamp(
                Dot(relative_position_icrf_m.Normalized(), bulge_apex_icrf),
                -1.0,
                1.0);
            // cos^n(psi/2) = [max(0,(1+cos psi)/2)]^(n/2).
            const double diurnal_weight = std::pow(
                std::max(0.0, 0.5 * (1.0 + cosine_psi)),
                0.5 * diurnal_exponent);
            // rho = rho_min + (rho_max-rho_min) cos^n(psi/2).
            return minimum_density +
                (maximum_density - minimum_density) * diurnal_weight;
        }
    }

    BodyState ResolveBodyState(
        const GravityBody& body,
        const GravitySettings& settings,
        double ephemeris_time_tdb_seconds)
    {
        if (settings.ephemeris_provider)
        {
            BodyState body_state;
            if (settings.ephemeris_provider->TryGetBodyState(
                    body.name, ephemeris_time_tdb_seconds, body_state))
            {
                return body_state;
            }
        }

        const double delta_time = ephemeris_time_tdb_seconds -
            body.epoch_ephemeris_time_tdb_seconds;
        // Constant-velocity fallback:
        // r(t) = r0 + v0 (t-t0), v(t) = v0.
        return {
            body.position_icrf_at_epoch_m + body.velocity_icrf_mps * delta_time,
            body.velocity_icrf_mps
        };
    }

    Mat3d ResolveBodyFixedToIcrf(
        const GravityBody& body,
        const GravitySettings& settings,
        double ephemeris_time_tdb_seconds)
    {
        if (settings.ephemeris_provider)
        {
            Mat3d body_fixed_to_icrf;
            if (settings.ephemeris_provider->TryGetBodyFixedToIcrf(
                    body.name,
                    ephemeris_time_tdb_seconds,
                    body_fixed_to_icrf))
            {
                return body_fixed_to_icrf;
            }
        }

        // Constant-spin fallback: R_I_F(t) = Rot(axis,omega(t-t0)) R_I_F(t0).
        const double angle = body.spin_rate_radps *
            (ephemeris_time_tdb_seconds -
             body.epoch_ephemeris_time_tdb_seconds);
        return RotationAroundAxis(body.spin_axis_icrf, angle) *
            body.body_fixed_to_icrf_at_epoch;
    }

    Vec3d ResolveBodyAngularVelocityIcrf(
        const GravityBody& body,
        const GravitySettings& settings,
        double ephemeris_time_tdb_seconds)
    {
        if (settings.ephemeris_provider)
        {
            Vec3d angular_velocity_icrf_radps;
            if (settings.ephemeris_provider->TryGetBodyAngularVelocityIcrf(
                    body.name,
                    ephemeris_time_tdb_seconds,
                    angular_velocity_icrf_radps))
            {
                return angular_velocity_icrf_radps;
            }
        }

        return body.spin_axis_icrf.Normalized() * body.spin_rate_radps;
    }

    const GravityBody* FindGravityBody(
        const GravitySettings& settings,
        const std::string& name)
    {
        for (const GravityBody& body : settings.bodies)
        {
            if (body.name == name) return &body;
        }
        return nullptr;
    }

    AtmosphereState EvaluateAtmosphereState(
        const AtmosphereSettings& settings,
        double altitude_m,
        const Vec3d& relative_position_icrf_m,
        const Vec3d& central_body_relative_inertial_velocity_icrf_mps,
        const Mat3d& central_body_fixed_to_icrf,
        const Vec3d& sun_direction_from_central_body_icrf)
    {
        AtmosphereState state;
        if (!settings.enabled) return state;

        double model_altitude_m = altitude_m;
        if (settings.model_kind == AtmosphereModelKind::CubicHarrisPriesterEarth)
        {
            const Vec3d position_body_fixed_m =
                central_body_fixed_to_icrf.Transposed() *
                relative_position_icrf_m;
            model_altitude_m = EvaluateEarthEllipsoidAltitude(position_body_fixed_m);
        }

        const bool thermodynamics_available = EvaluateThermodynamicProfile(
            settings.thermodynamic_profile, model_altitude_m, state);
        bool density_available = false;
        if (settings.model_kind == AtmosphereModelKind::TabulatedProfile)
        {
            density_available = EvaluateTabulatedDensity(
                settings.density_profile,
                model_altitude_m,
                state.density_kgpm3);
        }
        else
        {
            state.density_kgpm3 = EvaluateCubicHarrisPriesterDensity(
                settings.cubic_harris_priester,
                model_altitude_m,
                relative_position_icrf_m,
                central_body_relative_inertial_velocity_icrf_mps,
                central_body_fixed_to_icrf,
                sun_direction_from_central_body_icrf);
            density_available = state.density_kgpm3 > 0.0;
        }

        state.within_model_domain = density_available && thermodynamics_available;
        if (!state.within_model_domain) state.density_kgpm3 = 0.0;
        return state;
    }

    double EvaluateMeanFreePath(const AtmosphereState& atmosphere_state)
    {
        if (atmosphere_state.density_kgpm3 <= 0.0 ||
            atmosphere_state.mean_particle_mass_kg <= 0.0 ||
            atmosphere_state.effective_collision_cross_section_m2 <= 0.0)
        {
            return std::numeric_limits<double>::infinity();
        }

        // Number density: n = rho / m_bar.
        const double number_density_per_m3 =
            atmosphere_state.density_kgpm3 /
            atmosphere_state.mean_particle_mass_kg;
        // Hard-sphere kinetic theory:
        // lambda = 1 / (sqrt(2) n sigma_eff).
        return 1.0 /
            (std::sqrt(2.0) * number_density_per_m3 *
             atmosphere_state.effective_collision_cross_section_m2);
    }

    double EvaluateMolecularSpeedRatio(
        const AtmosphereState& atmosphere_state,
        double relative_speed_mps)
    {
        if (relative_speed_mps < 0.0 ||
            atmosphere_state.temperature_k <= 0.0 ||
            atmosphere_state.mean_particle_mass_kg <= 0.0)
        {
            return 0.0;
        }

        // Most-probable molecular thermal speed:
        // c_mp = sqrt(2 k_B T / m_bar), hence s = |v_rel|/c_mp.
        const double most_probable_thermal_speed_mps = std::sqrt(
            2.0 * kBoltzmannConstantJpk * atmosphere_state.temperature_k /
            atmosphere_state.mean_particle_mass_kg);
        return most_probable_thermal_speed_mps > 0.0
            ? relative_speed_mps / most_probable_thermal_speed_mps
            : 0.0;
    }
}
