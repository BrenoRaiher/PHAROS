// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Core/Types.h"

// Implements backend-native 3D linear algebra and quaternion kinematics.

namespace tgsim
{
    Mat3d Mat3d::Transposed() const
    {
        Mat3d result = Zero();
        for (int row = 0; row < 3; ++row)
        {
            for (int column = 0; column < 3; ++column)
            {
                result.m[row][column] = m[column][row];
            }
        }
        return result;
    }

    double Mat3d::Determinant() const
    {
        // det(A) = a(ei - fh) - b(di - fg) + c(dh - eg)
        return
            m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) -
            m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
            m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
    }

    Mat3d Mat3d::Inverse(double epsilon) const
    {
        const double determinant = Determinant();
        if (std::abs(determinant) <= epsilon)
        {
            return Mat3d::Zero();
        }

        // A^-1 = adj(A) / det(A)
        Mat3d result = Zero();
        result.m[0][0] =  (m[1][1] * m[2][2] - m[1][2] * m[2][1]);
        result.m[0][1] = -(m[0][1] * m[2][2] - m[0][2] * m[2][1]);
        result.m[0][2] =  (m[0][1] * m[1][2] - m[0][2] * m[1][1]);
        result.m[1][0] = -(m[1][0] * m[2][2] - m[1][2] * m[2][0]);
        result.m[1][1] =  (m[0][0] * m[2][2] - m[0][2] * m[2][0]);
        result.m[1][2] = -(m[0][0] * m[1][2] - m[0][2] * m[1][0]);
        result.m[2][0] =  (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
        result.m[2][1] = -(m[0][0] * m[2][1] - m[0][1] * m[2][0]);
        result.m[2][2] =  (m[0][0] * m[1][1] - m[0][1] * m[1][0]);
        return result * (1.0 / determinant);
    }

    Mat3d& Mat3d::operator+=(const Mat3d& other)
    {
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                m[row][column] += other.m[row][column];
        return *this;
    }

    Mat3d& Mat3d::operator-=(const Mat3d& other)
    {
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                m[row][column] -= other.m[row][column];
        return *this;
    }

    Mat3d& Mat3d::operator*=(double scalar)
    {
        for (auto& row : m)
            for (double& element : row)
                element *= scalar;
        return *this;
    }

    Vec3d operator*(const Mat3d& matrix, const Vec3d& vector)
    {
        return {
            matrix.m[0][0] * vector.x + matrix.m[0][1] * vector.y + matrix.m[0][2] * vector.z,
            matrix.m[1][0] * vector.x + matrix.m[1][1] * vector.y + matrix.m[1][2] * vector.z,
            matrix.m[2][0] * vector.x + matrix.m[2][1] * vector.y + matrix.m[2][2] * vector.z
        };
    }

    Mat3d operator*(const Mat3d& lhs, const Mat3d& rhs)
    {
        Mat3d result = Mat3d::Zero();
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                for (int inner = 0; inner < 3; ++inner)
                    result.m[row][column] += lhs.m[row][inner] * rhs.m[inner][column];
        return result;
    }

    Mat3d OuterProduct(const Vec3d& a, const Vec3d& b)
    {
        Mat3d result = Mat3d::Zero();
        const double av[3] = {a.x, a.y, a.z};
        const double bv[3] = {b.x, b.y, b.z};
        for (int row = 0; row < 3; ++row)
            for (int column = 0; column < 3; ++column)
                result.m[row][column] = av[row] * bv[column];
        return result;
    }

    Mat3d RotationAroundAxis(const Vec3d& axis, double angle_rad)
    {
        // Rodrigues: R = cos(theta) I + (1-cos(theta)) uu^T + sin(theta) [u]x
        const Vec3d u = axis.Normalized();
        const double c = std::cos(angle_rad);
        const double s = std::sin(angle_rad);
        const double one_minus_c = 1.0 - c;
        Mat3d result = Mat3d::Zero();
        result.m[0][0] = c + u.x * u.x * one_minus_c;
        result.m[0][1] = u.x * u.y * one_minus_c - u.z * s;
        result.m[0][2] = u.x * u.z * one_minus_c + u.y * s;
        result.m[1][0] = u.y * u.x * one_minus_c + u.z * s;
        result.m[1][1] = c + u.y * u.y * one_minus_c;
        result.m[1][2] = u.y * u.z * one_minus_c - u.x * s;
        result.m[2][0] = u.z * u.x * one_minus_c - u.y * s;
        result.m[2][1] = u.z * u.y * one_minus_c + u.x * s;
        result.m[2][2] = c + u.z * u.z * one_minus_c;
        return result;
    }

    Quatd Quatd::Normalized(double epsilon) const
    {
        // TG-1 Eq. (15): q^T q = 1. Numerical integration does not preserve
        // this nonlinear constraint exactly, so accepted states are projected to S^3.
        const double norm = Norm();
        return norm > epsilon ? Quatd{w / norm, x / norm, y / norm, z / norm} : Quatd::Identity();
    }

    Mat3d Quatd::ToRotationMatrix() const
    {
        // TG-1 Eq. (16), scalar-first Hamilton convention:
        // R_I_B(q) = (q0^2-qv^T qv) I3 + 2 qv qv^T + 2 q0 [qv]x.
        // Expanding that equation component by component gives the assignments below.
        const Quatd q = Normalized();
        Mat3d result = Mat3d::Zero();
        result.m[0][0] = 1.0 - 2.0 * (q.y * q.y + q.z * q.z);
        result.m[0][1] = 2.0 * (q.x * q.y - q.w * q.z);
        result.m[0][2] = 2.0 * (q.x * q.z + q.w * q.y);
        result.m[1][0] = 2.0 * (q.x * q.y + q.w * q.z);
        result.m[1][1] = 1.0 - 2.0 * (q.x * q.x + q.z * q.z);
        result.m[1][2] = 2.0 * (q.y * q.z - q.w * q.x);
        result.m[2][0] = 2.0 * (q.x * q.z - q.w * q.y);
        result.m[2][1] = 2.0 * (q.y * q.z + q.w * q.x);
        result.m[2][2] = 1.0 - 2.0 * (q.x * q.x + q.y * q.y);
        return result;
    }

    Vec3d Quatd::Rotate(const Vec3d& body_vector) const
    {
        // TG-1 Eq. (10): a^I = R_I_B a^B.
        return ToRotationMatrix() * body_vector;
    }

    Vec3d Quatd::InverseRotate(const Vec3d& inertial_vector) const
    {
        // TG-1 Eqs. (11)-(12): a^B = R_I_B^T a^I because R is orthogonal.
        return ToRotationMatrix().Transposed() * inertial_vector;
    }

    Quatd& Quatd::operator+=(const Quatd& other)
    {
        w += other.w; x += other.x; y += other.y; z += other.z;
        return *this;
    }

    Quatd& Quatd::operator*=(double scalar)
    {
        w *= scalar; x *= scalar; y *= scalar; z *= scalar;
        return *this;
    }

    Quatd operator*(const Quatd& lhs, const Quatd& rhs)
    {
        // Hamilton product used by TG-1 Eq. (19):
        // [a0,av] (x) [b0,bv] = [a0b0-av.bv, a0bv+b0av+av x bv].
        return {
            lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z,
            lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
            lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
            lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w
        };
    }

    Quatd QuaternionRateBodyToInertial(const Quatd& attitude_body_to_icrf, const Vec3d& omega_body_radps)
    {
        // TG-1 Eq. (19): q_dot = 1/2 q (x) [0, omega_body], for q mapping B to I
        // and angular velocity components expressed in B.
        return 0.5 * (attitude_body_to_icrf * Quatd{0.0, omega_body_radps.x, omega_body_radps.y, omega_body_radps.z});
    }
}
