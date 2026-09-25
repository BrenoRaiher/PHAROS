// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Typed and numeric outputs returned after propagation completes or stops.

#include "TGSim/Core/Types.h"

#include <string>
#include <vector>

namespace tgsim
{
    /// One configured celestial body's barycentric ICRF ephemeris at a recorded
    /// simulation epoch. Unreal can use these states to place visualization actors
    /// without making a second SPICE query during playback.
    struct CelestialBodyTelemetry
    {
        std::string name;
        Vec3d position_icrf_m;
        Vec3d velocity_icrf_mps;
        double reference_radius_m = 0.0;
        Mat3d body_fixed_to_icrf = Mat3d::Identity();
    };

    /// One component transform relative to the main-body frame at an output epoch.
    struct ComponentPoseTelemetry
    {
        Vec3d origin_body_m;
        Mat3d component_to_body = Mat3d::Identity();
    };

    /// One recorded output instant with state, loads, center of mass, and inertia.
    struct TelemetrySample
    {
        SpacecraftState state;
        ForceTorqueSample applied_force_torque;
        Vec3d center_of_mass_body_m;
        Mat3d inertia_body_kgm2;
        Vec3d base_origin_acceleration_body_mps2;
        Vec3d total_linear_momentum_icrf_kgmps;
        Vec3d total_angular_momentum_about_cm_icrf_kgm2ps;
        std::vector<double> applied_joint_efforts;
        std::vector<double> thruster_thrusts_n;
        std::vector<double> joint_constraint_efforts;
        std::vector<ComponentPoseTelemetry> component_poses;
        std::vector<CelestialBodyTelemetry> celestial_bodies;
        bool multibody_solve_succeeded = false;
    };

    /// Complete return value from SimulationEngine::Run().
    struct SimulationResult
    {
        bool success = false;
        std::string message;
        std::vector<TelemetrySample> samples; // Strongly typed history for C++ consumers.

        // Numeric AVS-style table for serialization and future Unreal playback.
        std::vector<std::string> column_names; // Describes every solution_array column and unit.
        std::vector<std::vector<double>> solution_array; // AVS-style numeric [time row][quantity column].
    };
}
