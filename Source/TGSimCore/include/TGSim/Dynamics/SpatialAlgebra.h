// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Six-dimensional spatial-vector primitives used by the multibody solver.
// Motion vectors are ordered [angular; linear] and force vectors [moment; force].

#include "TGSim/Core/Types.h"

#include <cstddef>

namespace tgsim
{
    struct SpatialMotion
    {
        Vec3d angular;
        Vec3d linear;

        SpatialMotion& operator+=(const SpatialMotion& other);
        SpatialMotion& operator-=(const SpatialMotion& other);
        SpatialMotion& operator*=(double scalar);
        friend SpatialMotion operator+(SpatialMotion lhs, const SpatialMotion& rhs) { return lhs += rhs; }
        friend SpatialMotion operator-(SpatialMotion lhs, const SpatialMotion& rhs) { return lhs -= rhs; }
        friend SpatialMotion operator-(const SpatialMotion& value) { return {-value.angular, -value.linear}; }
        friend SpatialMotion operator*(SpatialMotion value, double scalar) { return value *= scalar; }
        friend SpatialMotion operator*(double scalar, SpatialMotion value) { return value *= scalar; }
    };

    struct SpatialForce
    {
        Vec3d moment;
        Vec3d force;

        SpatialForce& operator+=(const SpatialForce& other);
        SpatialForce& operator-=(const SpatialForce& other);
        SpatialForce& operator*=(double scalar);
        friend SpatialForce operator+(SpatialForce lhs, const SpatialForce& rhs) { return lhs += rhs; }
        friend SpatialForce operator-(SpatialForce lhs, const SpatialForce& rhs) { return lhs -= rhs; }
        friend SpatialForce operator-(const SpatialForce& value) { return {-value.moment, -value.force}; }
        friend SpatialForce operator*(SpatialForce value, double scalar) { return value *= scalar; }
        friend SpatialForce operator*(double scalar, SpatialForce value) { return value *= scalar; }
    };

    /// General 6x6 matrix. Spatial inertias are represented by this same type.
    struct SpatialMatrix
    {
        double m[6][6] = {};

        static SpatialMatrix Zero() { return {}; }
        static SpatialMatrix Identity();

        SpatialMatrix Transposed() const;
        /// Solves (*this) x = rhs for the unknown spatial motion x. Thus the
        /// SpatialMatrix object on which this method is called is the matrix A.
        /// Eigen FullPivLU performs the rank-revealing linear solve.
        bool SolveForMotion(
            const SpatialForce& rhs,
            SpatialMotion& solution,
            double relative_rank_threshold = 1.0e-12) const;

        SpatialMatrix& operator+=(const SpatialMatrix& other);
        SpatialMatrix& operator-=(const SpatialMatrix& other);
        SpatialMatrix& operator*=(double scalar);
        friend SpatialMatrix operator+(SpatialMatrix lhs, const SpatialMatrix& rhs) { return lhs += rhs; }
        friend SpatialMatrix operator-(SpatialMatrix lhs, const SpatialMatrix& rhs) { return lhs -= rhs; }
        friend SpatialMatrix operator*(SpatialMatrix value, double scalar) { return value *= scalar; }
        friend SpatialMatrix operator*(double scalar, SpatialMatrix value) { return value *= scalar; }
    };

    SpatialMatrix operator*(const SpatialMatrix& lhs, const SpatialMatrix& rhs);
    SpatialForce operator*(const SpatialMatrix& matrix, const SpatialMotion& motion);
    /// Returns a*b^T in spatial coordinate order [angular; linear].
    SpatialMatrix OuterProduct(const SpatialForce& a, const SpatialForce& b);

    /// X maps a parent-frame motion vector into child-frame coordinates.
    /// offset_parent_m points from the parent origin to the child origin in parent axes.
    struct SpatialTransform
    {
        Mat3d parent_to_child_rotation = Mat3d::Identity();
        Vec3d offset_parent_m;

        static SpatialTransform Identity() { return {}; }

        /// v_child = X_child_parent * v_parent.
        SpatialMotion ApplyMotion(const SpatialMotion& parent_motion) const;
        /// f_parent = X_child_parent^T * f_child.
        SpatialForce ApplyForceToParent(const SpatialForce& child_force) const;
        /// I_parent = X_child_parent^T I_child X_child_parent.
        SpatialMatrix ApplyInertiaToParent(const SpatialMatrix& child_inertia) const;
        /// Returns the explicit Featherstone motion transform (RBDA Eq. 2.25):
        /// X_C<-P = [ R_CP, 0; -R_CP [r_PC]x, R_CP ].
        SpatialMatrix MotionMatrix() const;
    };

    /// Returns the parent-to-child transform obtained by applying parent-to-middle
    /// first and middle-to-child second.
    SpatialTransform ComposeParentToChildTransforms(
        const SpatialTransform& parent_to_middle,
        const SpatialTransform& middle_to_child);

    /// Spatial motion cross product: result = velocity x motion.
    SpatialMotion CrossMotion(const SpatialMotion& velocity, const SpatialMotion& motion);
    /// Spatial force cross product: result = velocity x* force.
    SpatialForce CrossForce(const SpatialMotion& velocity, const SpatialForce& force);
    /// Power pairing S^T f = angular dot moment + linear dot force.
    double Dot(const SpatialMotion& motion, const SpatialForce& force);

    /// Spatial inertia about a frame origin for mass m, local CM c, and centroidal inertia I_CM.
    /// I_spatial = [I_CM + m(|c|^2 E-cc^T), m[c]x; -m[c]x, mE].
    SpatialMatrix MakeSpatialInertia(
        double mass_kg,
        const Vec3d& center_of_mass_m,
        const Mat3d& inertia_centroid_kgm2);
}
