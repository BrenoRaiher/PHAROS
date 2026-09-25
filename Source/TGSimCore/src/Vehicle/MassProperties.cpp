// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Vehicle/MassProperties.h"

// Combines rigid component data into total spacecraft mass, CM, inertia, and I_dot.

#include <algorithm>

namespace tgsim
{
    namespace
    {
        Mat3d SkewSymmetricMatrix(const Vec3d& value)
        {
            Mat3d result = Mat3d::Zero();
            result.m[0][1] = -value.z; result.m[0][2] = value.y;
            result.m[1][0] = value.z; result.m[1][2] = -value.x;
            result.m[2][0] = -value.y; result.m[2][1] = value.x;
            return result;
        }

        double ComponentMassRate(
            const ComponentDefinition& component,
            const std::vector<double>& component_mass_rates_kgps)
        {
            return component.variable_mass_state_index < component_mass_rates_kgps.size()
                ? component_mass_rates_kgps[component.variable_mass_state_index]
                : 0.0;
        }

        double ComponentInertiaScale(
            const ComponentDefinition& component,
            const double current_mass_kg)
        {
            // Every propagated component mass uses the PHAROS proportional-depletion model:
            // I_C(m) = (m / m_0) I_C,0. Fixed-mass components retain I_C,0.
            return component.variable_mass_state_index != kInvalidIndex &&
                component.initial_mass_kg > 0.0
                ? current_mass_kg / component.initial_mass_kg
                : 1.0;
        }
    }

    // Resolve the instantaneous mass used everywhere else for one component. A component
    // without a mass-state index remains rigidly at initial_mass_kg; a variable component
    // reads its propagated state and cannot fall below its configured dry/minimum mass.
    double MassPropertiesModel::ComponentMass(const ComponentDefinition& component, const SpacecraftState& state)
    {
        if (component.variable_mass_state_index == kInvalidIndex ||
            component.variable_mass_state_index >= state.variable_component_masses_kg.size())
        {
            return component.initial_mass_kg;
        }
        return std::max(component.minimum_mass_kg, state.variable_component_masses_kg[component.variable_mass_state_index]);
    }

    MassProperties MassPropertiesModel::ComputeWithoutRate(
        const VehicleSettings& vehicle,
        const SpacecraftState& state,
        const std::vector<double>& articulation_rates) const
    {
        // Assemble geometry, total mass, total CM, and inertia at one state. This is the
        // algebraic mass-property stage used before force evaluation and by the I_dot
        // finite difference below; it does not calculate or propagate any time derivative.
        MassProperties result;
        // Joint coordinates determine every component origin and local-axis orientation.
        result.component_poses = ComponentKinematicsModel().ComputePoses(
            vehicle, state, articulation_rates);
        if (vehicle.components.empty())
        {
            result.mass_kg = state.mass_kg;
            result.inertia_body_kgm2 = Mat3d::Identity();
            return result;
        }

        // First pass: total mass and first moment are required before inertia can be
        // shifted to the total CM.
        for (std::size_t component_index = 0; component_index < vehicle.components.size(); ++component_index)
        {
            const ComponentDefinition& component = vehicle.components[component_index];
            const ComponentPose& pose = result.component_poses[component_index];
            const double mass = ComponentMass(component, state);
            result.mass_kg += mass;
            // First moment: m_total * r_CM = sum(m_i * r_i).
            result.center_of_mass_body_m += mass * ComponentKinematicsModel::PointToBody(pose, component.center_of_mass_component_m);
        }

        if (result.mass_kg > 0.0)
        {
            // Spacecraft center of mass: r_CM = sum(m_i r_i) / sum(m_i).
            result.center_of_mass_body_m /= result.mass_kg;
        }

        // Second pass: rotate each centroidal inertia into B, then shift it from the
        // component CM to the just-computed spacecraft CM.
        for (std::size_t component_index = 0; component_index < vehicle.components.size(); ++component_index)
        {
            const ComponentDefinition& component = vehicle.components[component_index];
            const ComponentPose& pose = result.component_poses[component_index];
            const double mass = ComponentMass(component, state);
            const double scale = ComponentInertiaScale(component, mass);
            // Variable mass: I_C(m)=m/m_initial * I_C,initial. Fixed mass: I_C=I_C,initial.
            // Rotate component inertia into body axes: I_B = R_BC I_C R_BC^T.
            const Mat3d rotated_inertia =
                pose.component_to_body * (component.inertia_centroid_component_kgm2 * scale) *
                pose.component_to_body.Transposed();
            const Vec3d component_center = ComponentKinematicsModel::PointToBody(pose, component.center_of_mass_component_m);
            const Vec3d offset = component_center - result.center_of_mass_body_m;
            // Parallel-axis theorem: I_CM = I_c + m (|d|^2 I_3 - d d^T).
            const Mat3d parallel_axis = mass * (Mat3d::Diagonal({offset.NormSquared(), offset.NormSquared(), offset.NormSquared()}) - OuterProduct(offset, offset));
            result.inertia_body_kgm2 += rotated_inertia + parallel_axis;
        }

        return result;
    }

    MassProperties MassPropertiesModel::Compute(
        const VehicleSettings& vehicle,
        const SpacecraftState& state,
        const std::vector<double>& component_mass_rates_kgps,
        const std::vector<double>& articulation_rates) const
    {
        // Main workflow entry: return instantaneous properties and, when m_dot or eta_dot
        // is available, calculate the body-frame inertia derivative needed by the
        // variable-inertia Euler equation.
        MassProperties result = ComputeWithoutRate(vehicle, state, articulation_rates);
        const bool has_mass_rates = !component_mass_rates_kgps.empty() && !state.variable_component_masses_kg.empty();
        const bool has_articulation_rates = !articulation_rates.empty() && !state.articulation_coordinates.empty();
        if (!has_mass_rates && !has_articulation_rates) return result;

        // TG-1 Eq. (92) needs d_i = r_i-r_CM and d_dot_i. First differentiate
        // r_CM = sum(m_i r_i)/M:
        // r_dot_CM = [sum(m_dot_i r_i + m_i r_dot_i) - M_dot r_CM] / M.
        double total_mass_rate_kgps = 0.0;
        Vec3d first_moment_rate_kgmps;
        Vec3d articulation_first_moment_rate_kgmps;
        std::vector<Vec3d> component_centers_body_m(vehicle.components.size());
        std::vector<Vec3d> component_center_rates_body_mps(vehicle.components.size());
        for (std::size_t index = 0; index < vehicle.components.size(); ++index)
        {
            const ComponentDefinition& component = vehicle.components[index];
            const ComponentPose& pose = result.component_poses[index];
            const double mass = ComponentMass(component, state);
            const double mass_rate = has_mass_rates
                ? ComponentMassRate(component, component_mass_rates_kgps)
                : 0.0;
            const Vec3d center = ComponentKinematicsModel::PointToBody(
                pose, component.center_of_mass_component_m);
            const Vec3d center_rate = has_articulation_rates
                ? ComponentKinematicsModel::PointRateToBody(
                    pose, component.center_of_mass_component_m)
                : Vec3d::Zero();
            component_centers_body_m[index] = center;
            component_center_rates_body_mps[index] = center_rate;
            total_mass_rate_kgps += mass_rate;
            first_moment_rate_kgmps += mass_rate * center + mass * center_rate;
            articulation_first_moment_rate_kgmps += mass * center_rate;
        }
        const Vec3d center_of_mass_rate_body_mps = result.mass_kg > 0.0
            ? (first_moment_rate_kgmps -
                total_mass_rate_kgps * result.center_of_mass_body_m) / result.mass_kg
            : Vec3d::Zero();
        const Vec3d mass_redistribution_rate_body_mps = result.mass_kg > 0.0
            ? (first_moment_rate_kgmps -
                total_mass_rate_kgps * result.center_of_mass_body_m -
                articulation_first_moment_rate_kgmps) / result.mass_kg
            : Vec3d::Zero();
        result.center_of_mass_rate_body_mps = center_of_mass_rate_body_mps;
        result.center_of_mass_mass_redistribution_rate_body_mps =
            mass_redistribution_rate_body_mps;

        // TG-1 Eq. (92), printed p. 24 (PDF p. 26):
        // I_dot_CM^B = sum_i [R_dot I_i R^T + R I_dot_i R^T + R I_i R_dot^T
        //   + m_dot_i(|d_i|^2 E-d_i d_i^T)
        //   + m_i(2 d_i.d_dot_i E-d_dot_i d_i^T-d_i d_dot_i^T)].
        for (std::size_t index = 0; index < vehicle.components.size(); ++index)
        {
            const ComponentDefinition& component = vehicle.components[index];
            const ComponentPose& pose = result.component_poses[index];
            const double mass = ComponentMass(component, state);
            const double mass_rate = has_mass_rates
                ? ComponentMassRate(component, component_mass_rates_kgps)
                : 0.0;
            const double inertia_scale = ComponentInertiaScale(component, mass);
            const Mat3d inertia_component =
                component.inertia_centroid_component_kgm2 * inertia_scale;
            const Mat3d inertia_rate_component =
                component.variable_mass_state_index != kInvalidIndex &&
                component.initial_mass_kg > 0.0
                ? component.inertia_centroid_component_kgm2 *
                    (mass_rate / component.initial_mass_kg)
                : Mat3d::Zero();

            // R_dot_B,C = [omega_C/B^B]x R_B,C. Base angular velocity is absent
            // because this is the derivative of inertia components expressed in B.
            const Mat3d rotation_rate = has_articulation_rates
                ? SkewSymmetricMatrix(pose.angular_velocity_relative_body_radps) *
                    pose.component_to_body
                : Mat3d::Zero();
            result.inertia_rate_body_kgm2ps +=
                rotation_rate * inertia_component * pose.component_to_body.Transposed() +
                pose.component_to_body * inertia_rate_component * pose.component_to_body.Transposed() +
                pose.component_to_body * inertia_component * rotation_rate.Transposed();

            const Vec3d offset =
                component_centers_body_m[index] - result.center_of_mass_body_m;
            const Vec3d offset_rate =
                component_center_rates_body_mps[index] - center_of_mass_rate_body_mps;
            const Mat3d offset_quadratic = Mat3d::Diagonal({
                offset.NormSquared(), offset.NormSquared(), offset.NormSquared()}) -
                OuterProduct(offset, offset);
            const double scalar_rate = 2.0 * Dot(offset, offset_rate);
            const Mat3d offset_quadratic_rate = Mat3d::Diagonal({
                scalar_rate, scalar_rate, scalar_rate}) -
                OuterProduct(offset_rate, offset) - OuterProduct(offset, offset_rate);
            result.inertia_rate_body_kgm2ps +=
                mass_rate * offset_quadratic + mass * offset_quadratic_rate;
        }
        return result;
    }
}
