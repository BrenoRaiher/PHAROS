// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Foundational math and propagated-state types used by the entire backend.
// This file has no dependency on Unreal Engine or on any other TGSim header.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace tgsim
{
    constexpr double kPi = 3.1415926535897932384626433832795;
    constexpr double kStandardGravityMps2 = 9.80665;
    constexpr double kSpeedOfLightMps = 299792458.0;
    constexpr double kAstronomicalUnitM = 149597870700.0;

    struct Vec3d
    {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        static constexpr Vec3d Zero() { return {}; }
        static constexpr Vec3d UnitX() { return {1.0, 0.0, 0.0}; }
        static constexpr Vec3d UnitY() { return {0.0, 1.0, 0.0}; }
        static constexpr Vec3d UnitZ() { return {0.0, 0.0, 1.0}; }

        /// Returns x^2 + y^2 + z^2 without taking a square root.
        double NormSquared() const { return x * x + y * y + z * z; }
        /// Returns the Euclidean magnitude |v|.
        double Norm() const { return std::sqrt(NormSquared()); }
        /// Returns a unit vector in this direction, or zero if its magnitude is too small.
        Vec3d Normalized(double epsilon = 1.0e-15) const
        {
            const double norm = Norm();
            return norm > epsilon ? *this / norm : Vec3d::Zero();
        }

        Vec3d& operator+=(const Vec3d& other)
        {
            x += other.x; y += other.y; z += other.z;
            return *this;
        }
        Vec3d& operator-=(const Vec3d& other)
        {
            x -= other.x; y -= other.y; z -= other.z;
            return *this;
        }
        Vec3d& operator*=(double scalar)
        {
            x *= scalar; y *= scalar; z *= scalar;
            return *this;
        }
        Vec3d& operator/=(double scalar)
        {
            x /= scalar; y /= scalar; z /= scalar;
            return *this;
        }

        friend Vec3d operator+(Vec3d lhs, const Vec3d& rhs) { return lhs += rhs; }
        friend Vec3d operator-(Vec3d lhs, const Vec3d& rhs) { return lhs -= rhs; }
        friend Vec3d operator-(const Vec3d& value) { return {-value.x, -value.y, -value.z}; }
        friend Vec3d operator*(Vec3d value, double scalar) { return value *= scalar; }
        friend Vec3d operator*(double scalar, Vec3d value) { return value *= scalar; }
        friend Vec3d operator/(Vec3d value, double scalar) { return value /= scalar; }
    };

    inline double Dot(const Vec3d& a, const Vec3d& b)
    {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    }

    inline Vec3d Cross(const Vec3d& a, const Vec3d& b)
    {
        return {
            a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x
        };
    }

    struct Mat3d
    {
        double m[3][3] = {
            {1.0, 0.0, 0.0},
            {0.0, 1.0, 0.0},
            {0.0, 0.0, 1.0}
        };

        static Mat3d Identity() { return {}; }
        static Mat3d Zero()
        {
            Mat3d value;
            for (auto& row : value.m)
            {
                for (double& element : row) element = 0.0;
            }
            return value;
        }
        static Mat3d Diagonal(const Vec3d& diagonal)
        {
            Mat3d value = Zero();
            value.m[0][0] = diagonal.x;
            value.m[1][1] = diagonal.y;
            value.m[2][2] = diagonal.z;
            return value;
        }

        /// Returns A^T. Rotation-matrix transpose also represents the inverse rotation.
        Mat3d Transposed() const;
        /// Returns det(A), used to test whether inertia and transformation matrices are invertible.
        double Determinant() const;
        /// Returns A^-1, or the zero matrix when |det(A)| <= epsilon.
        Mat3d Inverse(double epsilon = 1.0e-18) const;

        Mat3d& operator+=(const Mat3d& other);
        Mat3d& operator-=(const Mat3d& other);
        Mat3d& operator*=(double scalar);
        friend Mat3d operator+(Mat3d lhs, const Mat3d& rhs) { return lhs += rhs; }
        friend Mat3d operator-(Mat3d lhs, const Mat3d& rhs) { return lhs -= rhs; }
        friend Mat3d operator*(Mat3d value, double scalar) { return value *= scalar; }
        friend Mat3d operator*(double scalar, Mat3d value) { return value *= scalar; }
    };

    /// Applies a matrix transformation to a vector.
    Vec3d operator*(const Mat3d& matrix, const Vec3d& vector);
    /// Composes two matrix transformations; rhs is applied first.
    Mat3d operator*(const Mat3d& lhs, const Mat3d& rhs);
    /// Returns a b^T, used by the parallel-axis theorem.
    Mat3d OuterProduct(const Vec3d& a, const Vec3d& b);
    /// Returns the right-handed Rodrigues rotation about axis by angle_rad.
    Mat3d RotationAroundAxis(const Vec3d& axis, double angle_rad);

    struct Quatd
    {
        double w = 1.0;
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;

        static constexpr Quatd Identity() { return {}; }
        double NormSquared() const { return w * w + x * x + y * y + z * z; }
        double Norm() const { return std::sqrt(NormSquared()); }
        /// Returns a unit quaternion, or identity when the norm is too small.
        Quatd Normalized(double epsilon = 1.0e-15) const;
        Quatd Conjugate() const { return {w, -x, -y, -z}; }
        /// Returns R_I_B, the matrix that maps body components into ICRF components.
        Mat3d ToRotationMatrix() const;
        /// Rotates a body-frame vector into ICRF using this body-to-ICRF attitude.
        Vec3d Rotate(const Vec3d& body_vector) const;
        /// Rotates an ICRF vector into body axes using the inverse attitude.
        Vec3d InverseRotate(const Vec3d& inertial_vector) const;

        Quatd& operator+=(const Quatd& other);
        Quatd& operator*=(double scalar);
        friend Quatd operator+(Quatd lhs, const Quatd& rhs) { return lhs += rhs; }
        friend Quatd operator*(Quatd value, double scalar) { return value *= scalar; }
        friend Quatd operator*(double scalar, Quatd value) { return value *= scalar; }
    };

    /// Hamilton quaternion product.
    Quatd operator*(const Quatd& lhs, const Quatd& rhs);
    /// Evaluates q_dot = 0.5 q (x) [0, omega_B] for a body-to-ICRF quaternion.
    Quatd QuaternionRateBodyToInertial(const Quatd& attitude_body_to_icrf, const Vec3d& omega_body_radps);

    /// Complete state propagated by the general 6-DOF dynamics model.
    /// Translational quantities are in ICRF; rotational quantities are in body axes.
    struct SpacecraftState
    {
        // SPICE ET, numerically TDB seconds past J2000. UTC conversion belongs at
        // the UI/serialization boundary because UTC is discontinuous at leap seconds.
        double ephemeris_time_tdb_seconds = 0.0;
        // Small independent integration clock measured from this run's start.
        // Propagating this value avoids losing sub-millisecond increments when ET
        // is hundreds of millions of seconds from J2000. Absolute ET is derived as
        // start_ET + elapsed_time whenever SPICE or telemetry needs it.
        double elapsed_time_seconds = 0.0;
        Vec3d position_icrf_m; // Geometric total-spacecraft center of mass.
        Vec3d velocity_icrf_mps; // Time derivative of position_icrf_m.
        Quatd attitude_body_to_icrf;
        Vec3d angular_velocity_body_radps;
        double mass_kg = 0.0;
        std::vector<double> variable_component_masses_kg; // One entry per variable-mass state index.
        // Generalized articulation coordinates eta: radians for rotation DOFs, meters for translation DOFs.
        std::vector<double> articulation_coordinates;
        // eta_dot: rad/s for rotation DOFs, m/s for translation DOFs.
        std::vector<double> articulation_rates;
        std::vector<double> internal_angular_momenta_nms; // Signed momentum along each wheel axis.
    };

    /// Time derivative of SpacecraftState returned by a dynamics model.
    struct StateDerivative
    {
        Vec3d position_rate_mps;
        Vec3d velocity_rate_mps2;
        Quatd attitude_rate{0.0, 0.0, 0.0, 0.0};
        Vec3d angular_acceleration_body_radps2;
        double mass_rate_kgps = 0.0;
        std::vector<double> variable_component_mass_rates_kgps;
        // d(eta)/dt. This normally equals the current articulation_rates vector.
        std::vector<double> articulation_coordinate_rates;
        // d(eta_dot)/dt: rad/s^2 for rotation DOFs, m/s^2 for translation DOFs.
        std::vector<double> articulation_accelerations;
        std::vector<double> internal_angular_momentum_rates_nm; // d(h)/dt has torque units N m.
    };

    /// Diagnostic breakdown of all forces and torques at one state and time.
    /// Forces use ICRF axes; torques use spacecraft body axes.
    struct ForceTorqueSample
    {
        Vec3d gravity_force_icrf_n;
        Vec3d thrust_force_icrf_n;
        Vec3d solar_radiation_force_icrf_n;
        Vec3d aerodynamic_force_icrf_n;

        Vec3d gravity_torque_body_nm;
        Vec3d thrust_torque_body_nm;
        Vec3d solar_radiation_torque_body_nm;
        Vec3d aerodynamic_torque_body_nm;
        Vec3d control_torque_body_nm;

        Vec3d force_icrf_n;
        Vec3d torque_body_nm;
        double mass_rate_kgps = 0.0; // Negative while propellant is consumed.
        double visible_sun_fraction = 1.0;
        bool aerodynamics_outside_validity = false;
        double aerodynamic_dynamic_pressure_pa = 0.0;
        double aerodynamic_molecular_speed_ratio = 0.0;
        double aerodynamic_knudsen_number = 0.0;
        Vec3d aerodynamic_force_coefficients_body;
        Vec3d aerodynamic_moment_coefficients_body_about_cm;
        bool aerodynamic_database_used = false;
        bool aerodynamic_fallback_used = false;
    };

    /// External wrench applied to one articulated component. Force uses ICRF axes;
    /// torque is about that component's origin and uses component-local axes.
    struct ComponentLoad
    {
        Vec3d force_icrf_n;
        Vec3d torque_about_component_origin_component_nm;

        ComponentLoad& operator+=(const ComponentLoad& other)
        {
            force_icrf_n += other.force_icrf_n;
            torque_about_component_origin_component_nm +=
                other.torque_about_component_origin_component_nm;
            return *this;
        }
    };

    /// One complete right-hand-side evaluation: state derivative plus telemetry.
    struct DynamicsEvaluation
    {
        StateDerivative derivative;
        ForceTorqueSample applied_force_torque;
        Vec3d center_of_mass_body_m;
        Mat3d inertia_body_kgm2;
        // Linear acceleration of the main-body frame origin from the free-base solve.
        Vec3d base_origin_acceleration_body_mps2;
        Vec3d total_linear_momentum_icrf_kgmps;
        Vec3d total_angular_momentum_about_cm_icrf_kgm2ps;
        // Known actuator inputs after effort-limit clamping.
        std::vector<double> applied_joint_efforts;
        // Applied thrust magnitude for every configured thruster.
        std::vector<double> thruster_thrusts_n;
        // Stop/constraint reaction. Normally zero away from a coordinate or rate limit.
        std::vector<double> joint_constraint_efforts;
        // Instantaneous component transforms used by Unreal playback. Vector index
        // equals the corresponding VehicleSettings::components index.
        std::vector<Vec3d> component_origins_body_m;
        std::vector<Mat3d> component_to_body_rotations;
        bool multibody_solve_succeeded = false;
    };
}
