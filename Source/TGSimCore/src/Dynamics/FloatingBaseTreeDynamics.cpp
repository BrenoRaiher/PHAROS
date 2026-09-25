// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Dynamics/FloatingBaseTreeDynamics.h"

// Expands compound joints into one-node-per-DOF and runs Featherstone's
// floating-base Articulated-Body Algorithm (forward dynamics).

#include <algorithm>
#include <cmath>
#include <vector>

namespace tgsim
{
    namespace
    {
        // Internal ABA record for one expanded-tree node. A node is either a massless
        // scalar joint (has_dof=true) or a fixed physical component carrying inertia and
        // external load (component_index valid). Compound joints become serial scalar nodes.
        struct TreeNode
        {
            // Tree topology and current parent-to-node spatial coordinate transform X_i,lambda.
            std::size_t parent = kInvalidIndex;
            SpatialTransform parent_to_node;
            // S_i maps scalar eta_dot/eta_ddot into a six-dimensional joint motion.
            SpatialMotion motion_subspace;
            bool has_dof = false;
            // Maps expanded joint/component nodes back to public state/config indices.
            std::size_t articulation_state_index = kInvalidIndex;
            std::size_t component_index = kInvalidIndex;
            // Scalar joint state/input used only when has_dof is true.
            double coordinate = 0.0;
            double coordinate_rate = 0.0;
            double applied_effort = 0.0;
            ArticulationLimits limits;

            // Per-node physical quantities before descendant contributions are accumulated.
            SpatialMatrix inertia;
            SpatialMatrix inertia_rate;
            // Pass-1 kinematics: v_i, v_Ji=S_i*eta_dot_i, and c_i.
            SpatialMotion velocity;
            SpatialMotion joint_velocity;
            SpatialMotion bias_acceleration;
            // Known applied wrench and non-rigid-body bias additions such as wheel momentum.
            SpatialForce external_wrench;
            SpatialForce additional_bias_force;
            SpatialForce nozzle_carrier_flux;

            // Pass-2 ABA quantities IA_i, pA_i, U_i=IA_i*S_i, d_i, and u_i.
            SpatialMatrix articulated_inertia;
            SpatialForce articulated_bias_force;
            SpatialForce articulated_inertia_times_subspace;
            double articulated_scalar_inertia = 0.0;
            double generalized_bias_force = 0.0;

            // Pass-3 solution a_i and later diagnostic inverse-force accumulation.
            SpatialMotion acceleration;
            SpatialForce subtree_force;
        };

        // Read an optional vector entry without forcing every caller to pre-size every
        // command/rate array. Missing entries mean the supplied fallback, normally zero.
        double ValueAt(const std::vector<double>& values, std::size_t index, double fallback = 0.0)
        {
            return index < values.size() ? values[index] : fallback;
        }

        double CoordinateForDof(
            const ComponentDefinition& component,
            const ArticulationDof& dof,
            std::size_t local_index,
            const SpacecraftState& state)
        {
            // Convert a component-local DOF number into its flattened eta-state index and
            // return the admissible coordinate used to construct this evaluation's X_J.
            if (component.articulation_state_offset == kInvalidIndex)
                return dof.initial_coordinate;
            const std::size_t state_index = component.articulation_state_offset + local_index;
            return std::clamp(
                ValueAt(state.articulation_coordinates, state_index, dof.initial_coordinate),
                dof.limits.minimum_coordinate,
                dof.limits.maximum_coordinate);
        }

        SpatialTransform JointTransform(const ArticulationDof& dof, double coordinate)
        {
            // This is Featherstone's jcalc position output X_J(q) for the supported
            // one-DOF primitives. Fixed installation geometry X_T is composed later while
            // SolveForwardDynamics() expands the component tree.
            SpatialTransform transform;
            const Vec3d axis = dof.axis_joint.Normalized();
            if (dof.motion == ArticulationMotion::Rotation)
            {
                // R_parent_child = Rot(axis,q), so E_child_parent = Rot(axis,q)^T.
                transform.parent_to_child_rotation = RotationAroundAxis(axis, coordinate).Transposed();
            }
            else
            {
                // Prismatic joint: r_parent,child = axis*q in the current joint frame.
                transform.offset_parent_m = axis * coordinate;
            }
            return transform;
        }

        SpatialMotion MotionSubspace(const ArticulationDof& dof)
        {
            // Return jcalc's S_i: the unit six-dimensional motion produced by one unit of
            // generalized rate along this joint's normalized axis.
            const Vec3d axis = dof.axis_joint.Normalized();
            // Revolute S=[axis;0], prismatic S=[0;axis].
            return dof.motion == ArticulationMotion::Rotation
                ? SpatialMotion{axis, Vec3d::Zero()}
                : SpatialMotion{Vec3d::Zero(), axis};
        }

        void AddComponentInertia(
            TreeNode& node,
            const ComponentDefinition& component,
            const SpacecraftState& state,
            const std::vector<double>& mass_rates)
        {
            // Attach one physical component's spatial inertia I_i and optional I_dot_i to
            // its fixed expanded-tree node. Massless scalar joint nodes never call this.
            const double mass = MassPropertiesModel::ComponentMass(component, state);
            // A propagated component mass always preserves component shape by scaling
            // centroidal inertia linearly: I_C(m)=(m/m_initial) I_C,initial.
            const double inertia_scale =
                component.variable_mass_state_index != kInvalidIndex &&
                component.initial_mass_kg > 0.0
                ? mass / component.initial_mass_kg
                : 1.0;
            // MakeSpatialInertia shifts I_C to this component node's origin and forms the
            // coupled 6x6 matrix acting on [angular; linear] spatial acceleration.
            node.inertia = MakeSpatialInertia(
                mass,
                component.center_of_mass_component_m,
                inertia_scale * component.inertia_centroid_component_kgm2);

            if (component.variable_mass_state_index == kInvalidIndex) return;
            // The same spatial-inertia construction applied to m_dot and I_C_dot gives the
            // matrix derivative used in the bias term I_dot_i*v_i.
            const double mass_rate = ValueAt(mass_rates, component.variable_mass_state_index);
            const Mat3d inertia_rate = component.initial_mass_kg > 0.0
                ? (mass_rate / component.initial_mass_kg) * component.inertia_centroid_component_kgm2
                : Mat3d::Zero();
            node.inertia_rate = MakeSpatialInertia(
                mass_rate,
                component.center_of_mass_component_m,
                inertia_rate);
        }

        std::size_t AddNode(std::vector<TreeNode>& nodes, TreeNode node)
        {
            // Append in parent-before-child order and return the stable index used by all
            // subsequent ABA passes and component/DOF lookup arrays.
            nodes.push_back(node);
            return nodes.size() - 1;
        }

        void RunVelocityPass(std::vector<TreeNode>& nodes, const SpatialMotion& base_velocity)
        {
            // Featherstone ABA Pass 1 kinematics. Starting from known base velocity v_0,
            // visit parents before children to evaluate v_Ji, v_i, and c_i for every node.
            nodes[0].velocity = base_velocity;
            nodes[0].bias_acceleration = {};
            for (std::size_t index = 1; index < nodes.size(); ++index)
            {
                TreeNode& node = nodes[index];
                const TreeNode& parent = nodes[node.parent];
                // v_Ji = S_i*eta_dot_i for an active scalar joint; fixed component edges
                // have no relative motion and therefore v_Ji=0.
                node.joint_velocity = node.has_dof
                    ? node.coordinate_rate * node.motion_subspace
                    : SpatialMotion{};
                // v_i = X_i,lambda*v_lambda + v_Ji.
                node.velocity = node.parent_to_node.ApplyMotion(parent.velocity) + node.joint_velocity;
                // For the implemented fixed-axis joints c_Ji=0, hence the full
                // c_i = c_Ji + v_i x v_Ji reduces to the expression below.
                node.bias_acceleration = CrossMotion(node.velocity, node.joint_velocity);
            }
        }

        bool RunArticulatedBodyAlgorithm(
            std::vector<TreeNode>& nodes,
            const std::vector<bool>& acceleration_constrained,
            SpatialMotion& base_acceleration,
            std::vector<double>& joint_accelerations)
        {
            // Execute Pass 2, the floating-root solve, and Pass 3 for the already expanded
            // tree and velocity pass. acceleration_constrained selects joint coordinates
            // whose eta_ddot is prescribed as zero by an active position/rate stop.

            // Initialize each subtree with its node's own inertia and bias force. The reverse
            // loop below will add reduced descendant quantities to these values.
            for (TreeNode& node : nodes)
            {
                node.articulated_inertia = node.inertia;
                // h_i=I_i*v_i is formed separately because pA_i contains v_i x* h_i.
                const SpatialForce momentum = node.inertia * node.velocity;
                // pA_i = v_i x* I_i v_i + I_dot_i v_i + Phi_carrier,i
                //          + p_additional,i - f_external_i.
                // additional_bias_force contains wheel h_dot + omega x h terms.
                node.articulated_bias_force = CrossForce(node.velocity, momentum) +
                    node.inertia_rate * node.velocity + node.nozzle_carrier_flux +
                    node.additional_bias_force - node.external_wrench;
                node.articulated_inertia_times_subspace = {};
                node.articulated_scalar_inertia = 0.0;
                node.generalized_bias_force = 0.0;
            }

            // ABA Pass 2, leaves to root: eliminate every free scalar eta_ddot_i, then
            // transform the resulting articulated inertia/bias pair into the parent frame.
            for (std::size_t index = nodes.size(); index-- > 1;)
            {
                TreeNode& node = nodes[index];
                // Ia and pa are the quantities transmitted across this edge after accounting
                // for whether its scalar acceleration is free or prescribed.
                SpatialMatrix reduced_inertia = node.articulated_inertia;
                SpatialForce reduced_bias;
                const bool constrained = node.has_dof &&
                    node.articulation_state_index < acceleration_constrained.size() &&
                    acceleration_constrained[node.articulation_state_index];

                if (node.has_dof && !constrained)
                {
                    // The three joint-space quantities are evaluated in separate statements:
                    // U_i=IA_i*S_i, d_i=S_i^T*U_i, u_i=tau_i-S_i^T*pA_i.
                    node.articulated_inertia_times_subspace =
                        node.articulated_inertia * node.motion_subspace;
                    node.articulated_scalar_inertia = Dot(
                        node.motion_subspace,
                        node.articulated_inertia_times_subspace);
                    // d_i must be invertible to eliminate this DOF. Scale the singularity
                    // threshold so the test remains meaningful for differently sized models.
                    const double inertia_scale = std::max(1.0, std::abs(node.articulated_scalar_inertia));
                    if (!std::isfinite(node.articulated_scalar_inertia) ||
                        std::abs(node.articulated_scalar_inertia) <= 1.0e-14 * inertia_scale)
                    {
                        return false;
                    }
                    // u_i = tau_i - S_i^T*pA_i.
                    node.generalized_bias_force = node.applied_effort -
                        Dot(node.motion_subspace, node.articulated_bias_force);
                    // Ia_i = IA_i - U_i U_i^T/d_i.
                    reduced_inertia -= OuterProduct(
                        node.articulated_inertia_times_subspace,
                        node.articulated_inertia_times_subspace) *
                        (1.0 / node.articulated_scalar_inertia);
                    // pa_i = pA_i + Ia_i c_i + U_i u_i/d_i.
                    reduced_bias = node.articulated_bias_force +
                        reduced_inertia * node.bias_acceleration +
                        (node.generalized_bias_force / node.articulated_scalar_inertia) *
                            node.articulated_inertia_times_subspace;
                }
                else
                {
                    // A physical fixed node has no generalized coordinate, while an active
                    // stop prescribes eta_ddot=0. Neither case eliminates a free acceleration,
                    // so Ia_i=IA_i and pa_i=pA_i+IA_i*c_i.
                    reduced_bias = node.articulated_bias_force +
                        reduced_inertia * node.bias_acceleration;
                }

                // Accumulate the reduced child equation in parent coordinates:
                // IA_parent += X_i,parent^* Ia_i X_i,parent,
                // pA_parent += X_i,parent^* pa_i.
                TreeNode& parent = nodes[node.parent];
                parent.articulated_inertia +=
                    node.parent_to_node.ApplyInertiaToParent(reduced_inertia);
                parent.articulated_bias_force +=
                    node.parent_to_node.ApplyForceToParent(reduced_bias);
            }

            // Floating-root solve. With no fixed support reaction, the six root equations are
            // IA_0*a_0 + pA_0 = 0, hence a_0 = IA_0^-1*(-pA_0). The massless root is
            // nonsingular now because all physical descendant inertias were accumulated into it.
            if (!nodes[0].articulated_inertia.SolveForMotion(
                -nodes[0].articulated_bias_force,
                base_acceleration))
            {
                return false;
            }

            // ABA Pass 3, root to leaves: recover each free eta_ddot and then its node
            // acceleration from the solved parent acceleration.
            joint_accelerations.assign(joint_accelerations.size(), 0.0);
            nodes[0].acceleration = base_acceleration;
            // Final outward ABA pass recovers q_ddot and each component acceleration.
            for (std::size_t index = 1; index < nodes.size(); ++index)
            {
                TreeNode& node = nodes[index];
                // a'_i = X_i,parent*a_parent + c_i is the acceleration before adding the
                // unknown joint contribution S_i*eta_ddot_i.
                node.acceleration = node.parent_to_node.ApplyMotion(nodes[node.parent].acceleration) +
                    node.bias_acceleration;
                const bool constrained = node.has_dof &&
                    node.articulation_state_index < acceleration_constrained.size() &&
                    acceleration_constrained[node.articulation_state_index];
                if (node.has_dof && !constrained)
                {
                    // eta_ddot_i = d_i^-1 (u_i-U_i^T*a'_i). The variable named
                    // node.acceleration still contains a'_i until the following addition.
                    const double acceleration = (
                        node.generalized_bias_force -
                        Dot(node.acceleration, node.articulated_inertia_times_subspace)) /
                        node.articulated_scalar_inertia;
                    // Complete a_i = a'_i + S_i*eta_ddot_i.
                    node.acceleration += acceleration * node.motion_subspace;
                    if (node.articulation_state_index < joint_accelerations.size())
                        joint_accelerations[node.articulation_state_index] = acceleration;
                }
            }
            return true;
        }

        bool ViolatesStop(const TreeNode& node, double acceleration)
        {
            // Decide whether a freely solved eta_ddot points farther through an active
            // coordinate or speed boundary. SolveForwardDynamics() locks such a DOF at eta_ddot=0
            // and rerun ABA so the base and all other joints include the stop reaction.
            constexpr double coordinate_tolerance = 1.0e-12;
            constexpr double rate_tolerance = 1.0e-12;
            const bool at_lower_coordinate =
                node.coordinate <= node.limits.minimum_coordinate + coordinate_tolerance;
            const bool at_upper_coordinate =
                node.coordinate >= node.limits.maximum_coordinate - coordinate_tolerance;
            if (at_lower_coordinate && node.coordinate_rate <= rate_tolerance && acceleration < 0.0)
                return true;
            if (at_upper_coordinate && node.coordinate_rate >= -rate_tolerance && acceleration > 0.0)
                return true;

            if (std::isfinite(node.limits.maximum_absolute_rate))
            {
                if (node.coordinate_rate <= -node.limits.maximum_absolute_rate + rate_tolerance &&
                    acceleration < 0.0)
                    return true;
                if (node.coordinate_rate >= node.limits.maximum_absolute_rate - rate_tolerance &&
                    acceleration > 0.0)
                    return true;
            }
            return false;
        }
    }

    FloatingBaseTreeEvaluation FloatingBaseTreeDynamics::SolveForwardDynamics(
        const SpacecraftState& state,
        const SimulationConfig& config,
        const FloatingBaseTreeInput& input,
        bool enforce_joint_acceleration_limits) const
    {
        // Main multibody workflow for one General6DofDynamics/RK stage:
        // 1) expand components/DOFs into ABA nodes; 2) choose a co-moving velocity frame;
        // 3) attach loads and wheel bias; 4) solve ABA with active stops; 5) recover
        // reactions, momenta, and total-CM acceleration. No state is integrated here.
        FloatingBaseTreeEvaluation result;
        const std::size_t articulation_count = state.articulation_coordinates.size();
        result.joint_accelerations.assign(articulation_count, 0.0);
        result.joint_constraint_efforts.assign(articulation_count, 0.0);
        if (config.vehicle.components.empty()) return result;

        // Node 0 is the massless six-DOF handle coincident with report frame B at O_B.
        // The physical bus is a fixed child; its inertia reaches node 0 during Pass 2.
        std::vector<TreeNode> nodes(1);
        std::vector<std::size_t> component_nodes(config.vehicle.components.size(), kInvalidIndex);
        std::vector<std::size_t> dof_nodes(articulation_count, kInvalidIndex);

        // Expand the physical component hierarchy in parent-before-child order. component_nodes
        // maps each configured body to the fixed node that carries its inertia and loads;
        // dof_nodes maps each flattened eta index to its massless active node.
        for (std::size_t component_index = 0; component_index < config.vehicle.components.size(); ++component_index)
        {
            const ComponentDefinition& component = config.vehicle.components[component_index];
            if (component.parent_component_index == kInvalidIndex)
            {
                // Parentless main component: fixed transform from base frame B to the
                // component frame, followed by the component's physical spatial inertia.
                TreeNode component_node;
                component_node.parent = 0;
                component_node.parent_to_node.parent_to_child_rotation = component.component_to_body.Transposed();
                component_node.parent_to_node.offset_parent_m = component.origin_body_m;
                component_node.component_index = component_index;
                AddComponentInertia(component_node, component, state, input.variable_component_mass_rates_kgps);
                component_nodes[component_index] = AddNode(nodes, component_node);
                continue;
            }
            if (component.parent_component_index >= component_index ||
                component_nodes[component.parent_component_index] == kInvalidIndex)
            {
                return result;
            }

            std::size_t parent_node = component_nodes[component.parent_component_index];
            const ArticulationDefinition& joint = component.articulation_to_parent;
            if (joint.dofs.empty())
            {
                // Rigid parent-child attachment. With X_J=identity, this transform is only
                // the fixed zero-configuration geometry X_T between component origins/axes.
                TreeNode component_node;
                component_node.parent = parent_node;
                component_node.parent_to_node.parent_to_child_rotation =
                    joint.child_to_parent_at_zero.Transposed();
                component_node.parent_to_node.offset_parent_m = joint.parent_anchor_component_m -
                    joint.child_to_parent_at_zero * joint.child_anchor_component_m;
                component_node.component_index = component_index;
                AddComponentInertia(component_node, component, state, input.variable_component_mass_rates_kgps);
                component_nodes[component_index] = AddNode(nodes, component_node);
                continue;
            }

            // Expand an n-DOF articulation into n serial, massless one-DOF nodes so each
            // node has scalar d_i and uses Featherstone's standard one-DOF ABA equations.
            for (std::size_t local_index = 0; local_index < joint.dofs.size(); ++local_index)
            {
                const ArticulationDof& dof = joint.dofs[local_index];
                const std::size_t state_index = component.articulation_state_offset + local_index;
                TreeNode dof_node;
                dof_node.parent = parent_node;
                dof_node.coordinate = CoordinateForDof(component, dof, local_index, state);
                dof_node.parent_to_node = JointTransform(dof, dof_node.coordinate);
                if (local_index == 0)
                {
                    // The first scalar node also carries the fixed parent-origin-to-joint-anchor
                    // placement. In Featherstone notation this composes X_J(q)*X_T.
                    SpatialTransform parent_to_anchor;
                    parent_to_anchor.offset_parent_m = joint.parent_anchor_component_m;
                    dof_node.parent_to_node = ComposeParentToChildTransforms(
                        parent_to_anchor, dof_node.parent_to_node);
                }
                dof_node.motion_subspace = MotionSubspace(dof);
                dof_node.has_dof = true;
                dof_node.coordinate_rate = ValueAt(input.articulation_rates, state_index);
                dof_node.applied_effort = ValueAt(input.applied_joint_efforts, state_index);
                dof_node.limits = dof.limits;
                dof_node.articulation_state_index = state_index;
                parent_node = AddNode(nodes, dof_node);
                if (state_index < dof_nodes.size()) dof_nodes[state_index] = parent_node;
            }

            // After the final scalar joint, append the physical child component. This fixed
            // edge applies the zero-angle child-axis alignment and child-anchor offset.
            TreeNode component_node;
            component_node.parent = parent_node;
            component_node.parent_to_node.parent_to_child_rotation =
                joint.child_to_parent_at_zero.Transposed();
            component_node.parent_to_node.offset_parent_m =
                -(joint.child_to_parent_at_zero * joint.child_anchor_component_m);
            component_node.component_index = component_index;
            AddComponentInertia(component_node, component, state, input.variable_component_mass_rates_kgps);
            component_nodes[component_index] = AddNode(nodes, component_node);
        }

        // Run internal dynamics in an instantaneously co-moving inertial frame.
        // A uniform ICRF translation cannot affect attitude or articulation, and
        // removing it avoids cancellation between very large barycentric momenta.
        // First trial sets v_O_B=0 while retaining omega_B and all eta_dot. The resulting
        // mass-weighted component velocity is the CM velocity relative to O_B.
        SpatialMotion base_velocity{state.angular_velocity_body_radps, Vec3d::Zero()};
        RunVelocityPass(nodes, base_velocity);
        Vec3d relative_center_of_mass_velocity_body;
        double total_mass = 0.0;
        for (std::size_t component_index = 0; component_index < config.vehicle.components.size(); ++component_index)
        {
            const ComponentDefinition& component = config.vehicle.components[component_index];
            const double mass = MassPropertiesModel::ComponentMass(component, state);
            const TreeNode& node = nodes[component_nodes[component_index]];
            // For component-local CM offset c_i:
            // v_Ci = v_Oi + omega_i x c_i.
            const Vec3d velocity_at_component_cm = node.velocity.linear +
                Cross(node.velocity.angular, component.center_of_mass_component_m);
            const Mat3d component_to_body = component_index < input.component_poses.size()
                ? input.component_poses[component_index].component_to_body
                : Mat3d::Identity();
            relative_center_of_mass_velocity_body += mass *
                (component_to_body * velocity_at_component_cm);
            total_mass += mass;
        }
        if (total_mass <= 0.0) return result;
        // v_CM,rel^B = (1/M) sum_i m_i R_BCi v_Ci^Ci. Choosing
        // v_O_B^B=-v_CM,rel^B makes the total CM instantaneously stationary in the
        // Galilean-boosted frame; accelerations are unchanged by this constant boost.
        relative_center_of_mass_velocity_body /= total_mass;
        base_velocity.linear = -relative_center_of_mass_velocity_body;
        RunVelocityPass(nodes, base_velocity);

        // Effective thrust already carries relative exhaust momentum and pressure.
        // Retain only the outward carrier-momentum flux q*[r_N x v_N; v_N], using
        // the same boosted observer and local frame as I_dot*v at each mount node.
        for (std::size_t thruster_index = 0;
            thruster_index < config.vehicle.thrusters.size();
            ++thruster_index)
        {
            const double mass_flow = ValueAt(
                input.thruster_mass_flows_kgps, thruster_index);
            if (!std::isfinite(mass_flow) || mass_flow <= 0.0) continue;

            const ThrusterDefinition& thruster =
                config.vehicle.thrusters[thruster_index];
            if (thruster.component_index >= component_nodes.size()) continue;
            const std::size_t mount_node_index =
                component_nodes[thruster.component_index];
            if (mount_node_index == kInvalidIndex) continue;

            TreeNode& mount = nodes[mount_node_index];
            const Vec3d nozzle_velocity_component_mps = mount.velocity.linear +
                Cross(
                    mount.velocity.angular,
                    thruster.application_point_component_m);
            const SpatialForce carrier_flux{
                mass_flow * Cross(
                    thruster.application_point_component_m,
                    nozzle_velocity_component_mps),
                mass_flow * nozzle_velocity_component_mps};
            mount.nozzle_carrier_flux += carrier_flux;

            const ComponentPose pose =
                thruster.component_index < input.component_poses.size()
                    ? input.component_poses[thruster.component_index]
                    : ComponentPose{};
            const Vec3d flux_force_body =
                pose.component_to_body * carrier_flux.force;
            result.nozzle_carrier_linear_momentum_flux_solver_body_n +=
                flux_force_body;
            result.nozzle_carrier_angular_momentum_flux_about_cm_solver_body_nm +=
                pose.component_to_body * carrier_flux.moment +
                Cross(
                    pose.origin_body_m - input.center_of_mass_body_m,
                    flux_force_body);
        }

        // Momentum telemetry is assembled from the same component velocities/inertias used
        // by ABA, so conservation tests inspect the actual solved multibody representation.
        Vec3d linear_momentum_body;
        Vec3d angular_momentum_about_base_body;
        for (std::size_t component_index = 0; component_index < config.vehicle.components.size(); ++component_index)
        {
            const TreeNode& node = nodes[component_nodes[component_index]];
            const SpatialForce component_momentum = node.inertia * node.velocity;
            const ComponentPose pose = component_index < input.component_poses.size()
                ? input.component_poses[component_index]
                : ComponentPose{};
            const Vec3d component_linear_momentum_body =
                pose.component_to_body * component_momentum.force;
            linear_momentum_body += component_linear_momentum_body;
            // H_O_B^B += R_BCi*H_Oi^Ci + r_Oi/O_B^B x P_i^B.
            angular_momentum_about_base_body +=
                pose.component_to_body * component_momentum.moment +
                Cross(pose.origin_body_m, component_linear_momentum_body);
        }
        // Wheel rotor momentum is stored separately from mount rigid-body inertia and must
        // therefore be added explicitly to total spacecraft angular momentum.
        for (std::size_t wheel_index = 0; wheel_index < config.vehicle.reaction_wheels.size(); ++wheel_index)
        {
            const ReactionWheelDefinition& wheel = config.vehicle.reaction_wheels[wheel_index];
            if (wheel.component_index >= input.component_poses.size()) continue;
            angular_momentum_about_base_body +=
                ValueAt(state.internal_angular_momenta_nms, wheel_index) *
                ComponentKinematicsModel::DirectionToBody(
                    input.component_poses[wheel.component_index], wheel.axis_component).Normalized();
        }
        // The state velocity is the derivative of the geometric CM. Changing mass
        // weights move that point at beta relative to the lumped material average, so:
        // P^I = M*(v_CM^I-R_IB*beta^B) + R_IB*P_rel^B.
        // Angular momentum is shifted from O_B to CM by
        // H_CM^B = H_O_B^B - r_CM/O_B^B x P_rel^B before rotation to ICRF.
        result.total_linear_momentum_icrf_kgmps =
            total_mass * (
                state.velocity_icrf_mps -
                state.attitude_body_to_icrf.Rotate(
                    input.center_of_mass_mass_redistribution_rate_body_mps)) +
            state.attitude_body_to_icrf.Rotate(linear_momentum_body);
        result.total_angular_momentum_about_cm_icrf_kgm2ps =
            state.attitude_body_to_icrf.Rotate(
                angular_momentum_about_base_body -
                Cross(input.center_of_mass_body_m, linear_momentum_body));

        // Store external wrenches at their physical tree nodes. The equivalent root torque
        // arrives about CM, so shift it to O_B:
        // M_O_B^B = M_CM^B + r_CM/O_B^B x F_root^B.
        const Vec3d root_force_body =
            state.attitude_body_to_icrf.InverseRotate(input.external_force_icrf_n);
        nodes[0].external_wrench = {
            input.external_torque_about_cm_body_nm +
                Cross(input.center_of_mass_body_m, root_force_body),
            root_force_body};
        for (std::size_t component_index = 0;
            component_index < input.component_external_loads.size() &&
            component_index < component_nodes.size();
            ++component_index)
        {
            const ComponentLoad& load = input.component_external_loads[component_index];
            const ComponentPose pose = component_index < input.component_poses.size()
                ? input.component_poses[component_index]
                : ComponentPose{};
            // ComponentLoad force arrives in ICRF while its moment is already about the
            // component origin in component axes. Rotate only the force into that node's axes.
            const Vec3d force_body =
                state.attitude_body_to_icrf.InverseRotate(load.force_icrf_n);
            nodes[component_nodes[component_index]].external_wrench += {
                load.torque_about_component_origin_component_nm,
                pose.component_to_body.Transposed() * force_body};
        }

        // A wheel contributes h_dot*e + omega_mount x (h*e) to its mount-body angular
        // momentum derivative. It is added to pA so the mount/bus receives the opposite
        // reaction while h_dot is propagated separately as the wheel state derivative.
        for (std::size_t wheel_index = 0; wheel_index < config.vehicle.reaction_wheels.size(); ++wheel_index)
        {
            const ReactionWheelDefinition& wheel = config.vehicle.reaction_wheels[wheel_index];
            if (wheel.component_index >= component_nodes.size()) continue;
            TreeNode& mount = nodes[component_nodes[wheel.component_index]];
            const Vec3d axis = wheel.axis_component.Normalized();
            const double momentum = ValueAt(state.internal_angular_momenta_nms, wheel_index);
            const double momentum_rate = ValueAt(input.wheel_momentum_rates_nm, wheel_index);
            mount.additional_bias_force.moment += momentum_rate * axis +
                Cross(mount.velocity.angular, momentum * axis);
        }

        // Active-set loop: solve freely, then lock only accelerations that would drive
        // a coordinate/rate farther through its stop and rerun ABA.
        std::vector<bool> acceleration_constrained(articulation_count, false);
        for (std::size_t iteration = 0; iteration <= articulation_count; ++iteration)
        {
            if (!RunArticulatedBodyAlgorithm(
                nodes,
                acceleration_constrained,
                result.base_acceleration_body,
                result.joint_accelerations))
            {
                return result;
            }

            if (!enforce_joint_acceleration_limits) break;

            // A newly active stop changes the coupled base and all joint accelerations, so
            // collect violations and repeat the complete Pass-2/root/Pass-3 solve.
            bool added_constraint = false;
            for (std::size_t state_index = 0; state_index < dof_nodes.size(); ++state_index)
            {
                if (acceleration_constrained[state_index] || dof_nodes[state_index] == kInvalidIndex)
                    continue;
                const TreeNode& node = nodes[dof_nodes[state_index]];
                if (ViolatesStop(node, result.joint_accelerations[state_index]))
                {
                    acceleration_constrained[state_index] = true;
                    added_constraint = true;
                }
            }
            if (!added_constraint) break;
            if (iteration == articulation_count) return result;
        }

        // Inverse force recovery is diagnostic only: it finds the stop reaction after
        // forward dynamics has already solved base and joint accelerations.
        for (TreeNode& node : nodes)
        {
            // Reconstruct the net parent-on-subtree wrench from the solved motion:
            // f_i = I_i*a_i + v_i x* I_i*v_i + I_dot_i*v_i
            //       + Phi_carrier,i + p_wheel_i - f_ext_i.
            const SpatialForce momentum = node.inertia * node.velocity;
            node.subtree_force = node.inertia * node.acceleration +
                CrossForce(node.velocity, momentum) + node.inertia_rate * node.velocity +
                node.nozzle_carrier_flux + node.additional_bias_force - node.external_wrench;
        }
        for (std::size_t index = nodes.size(); index-- > 1;)
        {
            TreeNode& node = nodes[index];
            if (node.has_dof && node.articulation_state_index < articulation_count &&
                acceleration_constrained[node.articulation_state_index])
            {
                // tau_applied + tau_stop = S^T f_subtree.
                result.joint_constraint_efforts[node.articulation_state_index] =
                    Dot(node.motion_subspace, node.subtree_force) - node.applied_effort;
            }
            // Sum child reactions toward the root using the dual force transform.
            nodes[node.parent].subtree_force +=
                node.parent_to_node.ApplyForceToParent(node.subtree_force);
        }

        // Featherstone's linear acceleration coordinate is spatial, not the ordinary
        // inertial acceleration of the frame origin. Convert it with the velocity from
        // the same Galilean-boosted frame used by ABA.
        result.base_origin_acceleration_body_mps2 =
            result.base_acceleration_body.linear +
            Cross(base_velocity.angular, base_velocity.linear);

        // Reconstruct a mass-weighted average of physical component-centroid
        // accelerations. This is a diagnostic quantity; changing mass weights require
        // additional terms before it can be identified with geometric-CM acceleration.
        Vec3d mass_weighted_component_acceleration_body;
        for (std::size_t component_index = 0; component_index < config.vehicle.components.size(); ++component_index)
        {
            const ComponentDefinition& component = config.vehicle.components[component_index];
            const TreeNode& node = nodes[component_nodes[component_index]];
            const double mass = MassPropertiesModel::ComponentMass(component, state);
            // For a point fixed in this component at c_i:
            // a_Ci = a_spatial,i.linear + omega_i x v_Oi + alpha_i x c_i
            //       + omega_i x (omega_i x c_i).
            // Joint-relative acceleration is already contained in the solved node acceleration.
            const Vec3d component_cm_acceleration = node.acceleration.linear +
                Cross(node.velocity.angular, node.velocity.linear) +
                Cross(node.acceleration.angular, component.center_of_mass_component_m) +
                Cross(node.velocity.angular,
                    Cross(node.velocity.angular, component.center_of_mass_component_m));
            const Mat3d component_to_body = component_index < input.component_poses.size()
                ? input.component_poses[component_index].component_to_body
                : Mat3d::Identity();
            mass_weighted_component_acceleration_body += mass *
                (component_to_body * component_cm_acceleration);
        }
        result.mass_weighted_component_acceleration_icrf_mps2 =
            state.attitude_body_to_icrf.Rotate(
                mass_weighted_component_acceleration_body / total_mass);
        result.solved = true;
        return result;
    }
}
