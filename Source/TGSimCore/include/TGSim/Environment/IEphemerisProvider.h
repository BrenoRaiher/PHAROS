// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/Types.h"

#include <string>

namespace tgsim
{
    struct BodyState
    {
        Vec3d position_icrf_m;
        Vec3d velocity_icrf_mps;
    };

    struct BodyGravityMetadata
    {
        double gravitational_parameter_m3ps2 = 0.0;
        double reference_radius_m = 0.0;
    };

    /// External source of body ephemerides and physical metadata, such as SPICE.
    class IEphemerisProvider
    {
    public:
        virtual ~IEphemerisProvider() = default;

        /// Returns the named body's ICRF position [m] and velocity [m/s] at ephemeris time.
        virtual bool TryGetBodyState(
            const std::string& body_name,
            double ephemeris_time_tdb_seconds,
            BodyState& body_state) const = 0;

        /// Optionally returns R_I,BF, which maps body-fixed components into
        /// J2000/ICRF components. Providers that only supply center states may
        /// return false and the configured constant-spin model remains available.
        virtual bool TryGetBodyFixedToIcrf(
            const std::string& body_name,
            double ephemeris_time_tdb_seconds,
            Mat3d& body_fixed_to_icrf) const
        {
            (void)body_name;
            (void)ephemeris_time_tdb_seconds;
            (void)body_fixed_to_icrf;
            return false;
        }

        /// Optionally returns the angular velocity of the body-fixed frame
        /// relative to ICRF, expressed in ICRF [rad/s]. Aerodynamics uses it to
        /// form the velocity of a rigidly co-rotating atmosphere.
        virtual bool TryGetBodyAngularVelocityIcrf(
            const std::string& body_name,
            double ephemeris_time_tdb_seconds,
            Vec3d& angular_velocity_icrf_radps) const
        {
            (void)body_name;
            (void)ephemeris_time_tdb_seconds;
            (void)angular_velocity_icrf_radps;
            return false;
        }

        /// Returns the SPICE gravity parameter and physical shape radius. The
        /// GM drives point-mass gravity; the radius drives atmosphere/eclipses.
        /// Uploaded harmonic models carry their own separate GM and radius.
        /// A point-like system barycenter may return radius zero.
        virtual bool TryGetBodyGravityMetadata(
            const std::string& body_name,
            BodyGravityMetadata& metadata) const
        {
            (void)body_name;
            (void)metadata;
            return false;
        }
    };
}
