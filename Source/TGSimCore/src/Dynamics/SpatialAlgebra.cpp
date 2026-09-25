// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Dynamics/SpatialAlgebra.h"

// Implements the spatial-vector equations used by Featherstone recursions.

#include <cmath>
#include <Eigen/LU>

namespace tgsim
{
    namespace
    {
        void MotionToArray(const SpatialMotion& value, double out[6])
        {
            out[0] = value.angular.x; out[1] = value.angular.y; out[2] = value.angular.z;
            out[3] = value.linear.x; out[4] = value.linear.y; out[5] = value.linear.z;
        }

        void ForceToArray(const SpatialForce& value, double out[6])
        {
            out[0] = value.moment.x; out[1] = value.moment.y; out[2] = value.moment.z;
            out[3] = value.force.x; out[4] = value.force.y; out[5] = value.force.z;
        }

        SpatialForce ArrayToForce(const double value[6])
        {
            return {{value[0], value[1], value[2]}, {value[3], value[4], value[5]}};
        }

        /// Returns [v]x, the 3x3 skew-symmetric matrix satisfying [v]x w = v x w.
        Mat3d SkewSymmetricMatrix(const Vec3d& value)
        {
            Mat3d result = Mat3d::Zero();
            result.m[0][1] = -value.z; result.m[0][2] = value.y;
            result.m[1][0] = value.z; result.m[1][2] = -value.x;
            result.m[2][0] = -value.y; result.m[2][1] = value.x;
            return result;
        }
    }

    SpatialMotion& SpatialMotion::operator+=(const SpatialMotion& other)
    {
        angular += other.angular;
        linear += other.linear;
        return *this;
    }

    SpatialMotion& SpatialMotion::operator-=(const SpatialMotion& other)
    {
        angular -= other.angular;
        linear -= other.linear;
        return *this;
    }

    SpatialMotion& SpatialMotion::operator*=(double scalar)
    {
        angular *= scalar;
        linear *= scalar;
        return *this;
    }

    SpatialForce& SpatialForce::operator+=(const SpatialForce& other)
    {
        moment += other.moment;
        force += other.force;
        return *this;
    }

    SpatialForce& SpatialForce::operator-=(const SpatialForce& other)
    {
        moment -= other.moment;
        force -= other.force;
        return *this;
    }

    SpatialForce& SpatialForce::operator*=(double scalar)
    {
        moment *= scalar;
        force *= scalar;
        return *this;
    }

    SpatialMatrix SpatialMatrix::Identity()
    {
        SpatialMatrix result;
        for (int index = 0; index < 6; ++index) result.m[index][index] = 1.0;
        return result;
    }

    SpatialMatrix SpatialMatrix::Transposed() const
    {
        SpatialMatrix result;
        for (int row = 0; row < 6; ++row)
            for (int column = 0; column < 6; ++column)
                result.m[row][column] = m[column][row];
        return result;
    }

    bool SpatialMatrix::SolveForMotion(
        const SpatialForce& rhs,
        SpatialMotion& solution,
        double relative_rank_threshold) const
    {
        Eigen::Matrix<double, 6, 6> coefficient_matrix;
        Eigen::Matrix<double, 6, 1> right_hand_side;
        double rhs_array[6] = {};
        ForceToArray(rhs, rhs_array);
        for (int row = 0; row < 6; ++row)
        {
            for (int column = 0; column < 6; ++column)
                coefficient_matrix(row, column) = m[row][column];
            right_hand_side(row) = rhs_array[row];
        }

        // A x = b, where A is *this, b is a spatial force [moment; force], and
        // x is the requested spatial motion [angular; linear]. FullPivLU reports
        // rank before solve(), preventing a singular articulated-base inertia from
        // silently producing an invalid acceleration.
        Eigen::FullPivLU<Eigen::Matrix<double, 6, 6>> decomposition(coefficient_matrix);
        decomposition.setThreshold(relative_rank_threshold);
        if (decomposition.rank() < 6) return false;
        const Eigen::Matrix<double, 6, 1> solved = decomposition.solve(right_hand_side);
        if (!solved.allFinite()) return false;
        solution = {
            {solved(0), solved(1), solved(2)},
            {solved(3), solved(4), solved(5)}};
        return true;
    }

    SpatialMatrix& SpatialMatrix::operator+=(const SpatialMatrix& other)
    {
        for (int row = 0; row < 6; ++row)
            for (int column = 0; column < 6; ++column)
                m[row][column] += other.m[row][column];
        return *this;
    }

    SpatialMatrix& SpatialMatrix::operator-=(const SpatialMatrix& other)
    {
        for (int row = 0; row < 6; ++row)
            for (int column = 0; column < 6; ++column)
                m[row][column] -= other.m[row][column];
        return *this;
    }

    SpatialMatrix& SpatialMatrix::operator*=(double scalar)
    {
        for (auto& row : m)
            for (double& element : row)
                element *= scalar;
        return *this;
    }

    SpatialMatrix operator*(const SpatialMatrix& lhs, const SpatialMatrix& rhs)
    {
        SpatialMatrix result;
        for (int row = 0; row < 6; ++row)
            for (int column = 0; column < 6; ++column)
                for (int inner = 0; inner < 6; ++inner)
                    result.m[row][column] += lhs.m[row][inner] * rhs.m[inner][column];
        return result;
    }

    SpatialForce operator*(const SpatialMatrix& matrix, const SpatialMotion& motion)
    {
        double input[6] = {};
        double output[6] = {};
        MotionToArray(motion, input);
        for (int row = 0; row < 6; ++row)
            for (int column = 0; column < 6; ++column)
                output[row] += matrix.m[row][column] * input[column];
        return ArrayToForce(output);
    }

    SpatialMatrix OuterProduct(const SpatialForce& a, const SpatialForce& b)
    {
        double a_values[6] = {};
        double b_values[6] = {};
        ForceToArray(a, a_values);
        ForceToArray(b, b_values);
        SpatialMatrix result;
        for (int row = 0; row < 6; ++row)
            for (int column = 0; column < 6; ++column)
                result.m[row][column] = a_values[row] * b_values[column];
        return result;
    }

    SpatialMatrix SpatialTransform::MotionMatrix() const
    {
        SpatialMatrix result;
        // Featherstone RBDA Eq. (2.25), printed p. 22. With motion ordering
        // [angular; linear], r_PC expressed in P, and R_CP mapping P components
        // into C components: X_C<-P = [R_CP, 0; -R_CP[r_PC]x, R_CP].
        const Mat3d offset_cross = SkewSymmetricMatrix(offset_parent_m);
        const Mat3d lower_left = -1.0 * (parent_to_child_rotation * offset_cross);
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                result.m[row][column] = parent_to_child_rotation.m[row][column];
                result.m[row + 3][column] = lower_left.m[row][column];
                result.m[row + 3][column + 3] = parent_to_child_rotation.m[row][column];
            }
        }
        return result;
    }

    SpatialMotion SpatialTransform::ApplyMotion(const SpatialMotion& parent_motion) const
    {
        // omega_C = R_CP omega_P
        // v_C = R_CP [v_P + omega_P x r_PC].
        return {
            parent_to_child_rotation * parent_motion.angular,
            parent_to_child_rotation * (parent_motion.linear + Cross(parent_motion.angular, offset_parent_m))
        };
    }

    SpatialForce SpatialTransform::ApplyForceToParent(const SpatialForce& child_force) const
    {
        const Mat3d child_to_parent = parent_to_child_rotation.Transposed();
        const Vec3d force_parent = child_to_parent * child_force.force;
        // n_P = R_PC n_C + r_PC x F_P; F_P = R_PC F_C.
        return {
            child_to_parent * child_force.moment + Cross(offset_parent_m, force_parent),
            force_parent
        };
    }

    SpatialMatrix SpatialTransform::ApplyInertiaToParent(const SpatialMatrix& child_inertia) const
    {
        const SpatialMatrix transform = MotionMatrix();
        return transform.Transposed() * child_inertia * transform;
    }

    SpatialTransform ComposeParentToChildTransforms(
        const SpatialTransform& parent_to_middle,
        const SpatialTransform& middle_to_child)
    {
        SpatialTransform result;
        result.parent_to_child_rotation =
            middle_to_child.parent_to_child_rotation * parent_to_middle.parent_to_child_rotation;
        // r_PC = r_PM + R_PM r_MC.
        result.offset_parent_m = parent_to_middle.offset_parent_m +
            parent_to_middle.parent_to_child_rotation.Transposed() * middle_to_child.offset_parent_m;
        return result;
    }

    SpatialMotion CrossMotion(const SpatialMotion& velocity, const SpatialMotion& motion)
    {
        // [w;v] x [w2;v2] = [w x w2; w x v2 + v x w2].
        return {
            Cross(velocity.angular, motion.angular),
            Cross(velocity.angular, motion.linear) + Cross(velocity.linear, motion.angular)
        };
    }

    SpatialForce CrossForce(const SpatialMotion& velocity, const SpatialForce& force)
    {
        // [w;v] x* [n;f] = [w x n + v x f; w x f].
        return {
            Cross(velocity.angular, force.moment) + Cross(velocity.linear, force.force),
            Cross(velocity.angular, force.force)
        };
    }

    double Dot(const SpatialMotion& motion, const SpatialForce& force)
    {
        return Dot(motion.angular, force.moment) + Dot(motion.linear, force.force);
    }

    SpatialMatrix MakeSpatialInertia(
        double mass_kg,
        const Vec3d& center_of_mass_m,
        const Mat3d& inertia_centroid_kgm2)
    {
        SpatialMatrix result;
        const Mat3d center_cross = SkewSymmetricMatrix(center_of_mass_m);
        const Mat3d inertia_at_origin = inertia_centroid_kgm2 + mass_kg * (
            Mat3d::Diagonal({
                center_of_mass_m.NormSquared(),
                center_of_mass_m.NormSquared(),
                center_of_mass_m.NormSquared()}) -
            OuterProduct(center_of_mass_m, center_of_mass_m));
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                result.m[row][column] = inertia_at_origin.m[row][column];
                result.m[row][column + 3] = mass_kg * center_cross.m[row][column];
                result.m[row + 3][column] = -mass_kg * center_cross.m[row][column];
                result.m[row + 3][column + 3] = row == column ? mass_kg : 0.0;
            }
        }
        return result;
    }
}
