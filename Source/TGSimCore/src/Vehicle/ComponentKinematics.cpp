// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Vehicle/ComponentKinematics.h"

// Builds the component tree transforms s_B_l(eta) and R_B_l(eta) from the report.

#include <algorithm>

namespace tgsim
{
    namespace
    {
        double CoordinateForDof(
            const ComponentDefinition& component,
            const ArticulationDof& dof,
            std::size_t local_dof_index,
            const SpacecraftState& state)
        {
            // A missing offset can occur before SimulationConfigBuilder normalizes a request.
            // In that case the component remains at the DOF's configured initial coordinate.
            const bool has_state_offset = component.articulation_state_offset != kInvalidIndex;
            const std::size_t state_index = has_state_offset
                ? component.articulation_state_offset + local_dof_index
                : kInvalidIndex;
            const double coordinate = has_state_offset && state_index < state.articulation_coordinates.size()
                ? state.articulation_coordinates[state_index]
                : dof.initial_coordinate;
            return std::clamp(
                coordinate,
                dof.limits.minimum_coordinate,
                dof.limits.maximum_coordinate);
        }

        double RateForDof(
            const ComponentDefinition& component,
            const ArticulationDof& dof,
            std::size_t local_dof_index,
            const SpacecraftState& state,
            const std::vector<double>& articulation_rates)
        {
            const bool has_state_offset = component.articulation_state_offset != kInvalidIndex;
            const std::size_t state_index = has_state_offset
                ? component.articulation_state_offset + local_dof_index
                : kInvalidIndex;
            const std::vector<double>& rates = articulation_rates.empty()
                ? state.articulation_rates
                : articulation_rates;
            const double rate = has_state_offset && state_index < rates.size()
                ? rates[state_index]
                : dof.initial_rate;
            return std::clamp(
                rate,
                -dof.limits.maximum_absolute_rate,
                dof.limits.maximum_absolute_rate);
        }
    }

    std::vector<ComponentPose> ComponentKinematicsModel::ComputePoses(
        const VehicleSettings& vehicle,
        const SpacecraftState& state,
        const std::vector<double>& articulation_rates) const
    {
        std::vector<ComponentPose> poses(vehicle.components.size());
        for (std::size_t component_index = 0; component_index < vehicle.components.size(); ++component_index)
        {
            const ComponentDefinition& component = vehicle.components[component_index];
            if (component.parent_component_index == kInvalidIndex)
            {
                // The root component is fixed directly in spacecraft body axes.
                poses[component_index].origin_body_m = component.origin_body_m;
                poses[component_index].component_to_body = component.component_to_body;
                continue;
            }
            if (component.parent_component_index >= component_index)
            {
                // Validation reports forward/out-of-range parents. Keep pre-validation
                // mass-property evaluation memory-safe by leaving this pose at defaults.
                continue;
            }

            const ComponentPose& parent_pose = poses[component.parent_component_index];
            const ArticulationDefinition& joint = component.articulation_to_parent;
            Mat3d joint_rotation = Mat3d::Identity();
            Vec3d joint_translation_parent;
            Vec3d joint_translation_rate_parent;
            Vec3d joint_angular_velocity_parent;

            // Ordered homogeneous chain: T_joint = T_dof_0 T_dof_1 ... T_dof_n.
            for (std::size_t dof_index = 0; dof_index < joint.dofs.size(); ++dof_index)
            {
                const ArticulationDof& dof = joint.dofs[dof_index];
                const Vec3d axis = dof.axis_joint.Normalized();
                const double coordinate = CoordinateForDof(component, dof, dof_index, state);
                const double rate = RateForDof(
                    component, dof, dof_index, state, articulation_rates);
                // The DOF axis is defined in the intermediate joint frame after all
                // preceding DOFs. Rotate it into parent-component coordinates before
                // applying either q or q_dot.
                const Vec3d axis_parent = joint_rotation * axis;
                if (dof.motion == ArticulationMotion::Rotation)
                {
                    // Rotational DOF: R_joint <- R_joint R(axis, eta).
                    // Relative angular velocity: omega_J/P += axis_P eta_dot.
                    joint_angular_velocity_parent += axis_parent * rate;
                    joint_rotation = joint_rotation * RotationAroundAxis(axis, coordinate);
                }
                else
                {
                    // Translational DOF: p_joint <- p_joint + R_joint axis eta.
                    const Vec3d displacement_parent = axis_parent * coordinate;
                    joint_translation_parent += displacement_parent;
                    // d(axis_P eta)/dt = omega_J/P x (axis_P eta) + axis_P eta_dot.
                    joint_translation_rate_parent +=
                        Cross(joint_angular_velocity_parent, displacement_parent) +
                        axis_parent * rate;
                }
            }

            // Child orientation relative to parent: R_P_C = R_joint R_P_C(eta=0).
            const Mat3d component_to_parent = joint_rotation * joint.child_to_parent_at_zero;
            // Align child anchor to the translated parent anchor:
            // s_P_C = r_parent_anchor + p_joint - R_P_C r_child_anchor.
            const Vec3d component_origin_parent = joint.parent_anchor_component_m + joint_translation_parent
                - component_to_parent * joint.child_anchor_component_m;
            // d(s_P,C)/dt = p_dot_joint - omega_C/P x (R_P,C r_C,anchor).
            const Vec3d component_origin_rate_parent = joint_translation_rate_parent -
                Cross(
                    joint_angular_velocity_parent,
                    component_to_parent * joint.child_anchor_component_m);

            // Nested composition: R_B_C = R_B_P R_P_C and s_B_C = s_B_P + R_B_P s_P_C.
            poses[component_index].component_to_body = parent_pose.component_to_body * component_to_parent;
            poses[component_index].origin_body_m = parent_pose.origin_body_m
                + parent_pose.component_to_body * component_origin_parent;
            poses[component_index].angular_velocity_relative_body_radps =
                parent_pose.angular_velocity_relative_body_radps +
                parent_pose.component_to_body * joint_angular_velocity_parent;
            // d(s_B,C)/dt = s_dot_B,P + omega_P/B x R_B,P s_P,C
            //                 + R_B,P s_dot_P,C.
            poses[component_index].origin_rate_body_mps =
                parent_pose.origin_rate_body_mps +
                Cross(
                    parent_pose.angular_velocity_relative_body_radps,
                    parent_pose.component_to_body * component_origin_parent) +
                parent_pose.component_to_body * component_origin_rate_parent;
        }
        return poses;
    }

    Vec3d ComponentKinematicsModel::PointToBody(const ComponentPose& pose, const Vec3d& point_component_m)
    {
        // r_B = s_B_C + R_B_C r_C.
        return pose.origin_body_m + pose.component_to_body * point_component_m;
    }

    Vec3d ComponentKinematicsModel::DirectionToBody(const ComponentPose& pose, const Vec3d& direction_component)
    {
        // Directions rotate but do not translate: d_B = R_B_C d_C.
        return pose.component_to_body * direction_component;
    }

    Vec3d ComponentKinematicsModel::PointRateToBody(
        const ComponentPose& pose,
        const Vec3d& point_component_m)
    {
        // r_B = s_B,C + R_B,C r_C and R_dot_B,C = [omega_C/B^B]x R_B,C.
        // Therefore r_dot_B = s_dot_B,C + omega_C/B^B x (R_B,C r_C).
        return pose.origin_rate_body_mps + Cross(
            pose.angular_velocity_relative_body_radps,
            pose.component_to_body * point_component_m);
    }
}
