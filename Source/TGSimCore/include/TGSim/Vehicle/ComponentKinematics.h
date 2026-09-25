// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Resolves nested component poses from the articulation coordinates in SpacecraftState.

#include "TGSim/Core/SimulationConfig.h"

namespace tgsim
{
    /// Instantaneous rigid transform from one component's local axes into body axes.
    struct ComponentPose
    {
        Vec3d origin_body_m;
        Mat3d component_to_body = Mat3d::Identity();
        // Derivatives caused only by articulation relative to B. They exclude the
        // translational and angular velocity of the free base itself.
        Vec3d origin_rate_body_mps;
        Vec3d angular_velocity_relative_body_radps;
    };

    class ComponentKinematicsModel
    {
    public:
        /// Evaluates every component pose in parent-before-child order.
        /// A child's transform automatically includes every ancestor articulation.
        std::vector<ComponentPose> ComputePoses(
            const VehicleSettings& vehicle,
            const SpacecraftState& state,
            const std::vector<double>& articulation_rates = {}) const;

        /// Transforms one component-local point into spacecraft body coordinates.
        static Vec3d PointToBody(const ComponentPose& pose, const Vec3d& point_component_m);

        /// Rotates one component-local direction into spacecraft body axes.
        static Vec3d DirectionToBody(const ComponentPose& pose, const Vec3d& direction_component);

        /// Returns d(r_B)/dt for a point fixed in the component, considering only
        /// articulation relative to the spacecraft body frame B.
        static Vec3d PointRateToBody(
            const ComponentPose& pose,
            const Vec3d& point_component_m);
    };
}
