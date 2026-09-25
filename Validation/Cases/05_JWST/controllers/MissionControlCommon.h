#pragma once

#include "PHAROSControllerAPI.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace jwst_control
{
#ifndef JWST_MCC1A_DVX
#define JWST_MCC1A_DVX -0.30765878994689955
#define JWST_MCC1A_DVY 20.101342865417173
#define JWST_MCC1A_DVZ 0.2592813788163473
#define JWST_MCC1B_DVX -0.38400000000000001
#define JWST_MCC1B_DVY 2.75
#define JWST_MCC1B_DVZ 0.062
#define JWST_MCC2_DVX -1.292
#define JWST_MCC2_DVY 0.32100000000000001
#define JWST_MCC2_DVZ 0.042999999999999997
#endif

    constexpr double kIspSeconds = 295.0;
    constexpr double kStandardGravity = 9.80665;
    constexpr double kMaximumScatThrustNewtons = 32.0;
    constexpr double kSlewLeadSeconds = 3600.0;

    inline TGVec3 Add(const TGVec3& a, const TGVec3& b)
    {
        return {a.X + b.X, a.Y + b.Y, a.Z + b.Z};
    }

    inline TGVec3 Subtract(const TGVec3& a, const TGVec3& b)
    {
        return {a.X - b.X, a.Y - b.Y, a.Z - b.Z};
    }

    inline TGVec3 Scale(const TGVec3& v, double scale)
    {
        return {scale * v.X, scale * v.Y, scale * v.Z};
    }

    inline double Dot(const TGVec3& a, const TGVec3& b)
    {
        return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    }

    inline TGVec3 Cross(const TGVec3& a, const TGVec3& b)
    {
        return {
            a.Y * b.Z - a.Z * b.Y,
            a.Z * b.X - a.X * b.Z,
            a.X * b.Y - a.Y * b.X};
    }

    inline double Norm(const TGVec3& v)
    {
        return std::sqrt(Dot(v, v));
    }

    inline TGVec3 Unit(const TGVec3& v, const TGVec3& fallback)
    {
        const double norm = Norm(v);
        return norm > 1.0e-15 ? Scale(v, 1.0 / norm) : fallback;
    }

    inline double Clamp(double value, double lower, double upper)
    {
        return std::max(lower, std::min(value, upper));
    }

    inline bool Equals(const TGStringView& view, const char* text)
    {
        const std::size_t length = std::strlen(text);
        return view.Data != nullptr && view.Length == length &&
            std::memcmp(view.Data, text, length) == 0;
    }

    inline bool Contains(const TGStringView& view, const char* text)
    {
        if (view.Data == nullptr) return false;
        const std::size_t needleLength = std::strlen(text);
        if (needleLength == 0 || needleLength > view.Length) return false;
        for (std::size_t offset = 0;
             offset + needleLength <= view.Length;
             ++offset)
        {
            if (std::memcmp(view.Data + offset, text, needleLength) == 0)
                return true;
        }
        return false;
    }

    inline const TGCelestialBodyStateView* FindBody(
        const TGControlInput& input,
        const char* name)
    {
        for (uint64_t index = 0; index < input.CelestialBodyCount; ++index)
        {
            const TGCelestialBodyStateView& body = input.CelestialBodies[index];
            // NAIF IDs are the unambiguous primary lookup. The textual name
            // remains a fallback because display/catalog names may differ.
            const bool naifMatch =
                (std::strcmp(name, "Sun") == 0 && body.NaifId == 10) ||
                (std::strcmp(name, "Earth") == 0 && body.NaifId == 399);
            if (naifMatch || Equals(body.Name, name))
                return &input.CelestialBodies[index];
        }
        return nullptr;
    }

    inline uint64_t FindWheel(const TGControlInput& input, const char* name)
    {
        for (uint64_t index = 0; index < input.ReactionWheelCount; ++index)
        {
            if (Equals(input.ReactionWheels[index].Name, name)) return index;
        }
        return TG_CONTROLLER_INVALID_INDEX;
    }

    inline uint64_t FindThruster(const TGControlInput& input, const char* name)
    {
        for (uint64_t index = 0; index < input.ThrusterCount; ++index)
        {
            if (Equals(input.Thrusters[index].Name, name)) return index;
        }
        return TG_CONTROLLER_INVALID_INDEX;
    }

    inline TGQuat Multiply(const TGQuat& a, const TGQuat& b)
    {
        return {
            a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z,
            a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
            a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
            a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W};
    }

    inline TGQuat Conjugate(const TGQuat& q)
    {
        return {q.W, -q.X, -q.Y, -q.Z};
    }

    inline TGQuat QuaternionFromAxes(
        const TGVec3& xAxisIcrf,
        const TGVec3& yAxisIcrf,
        const TGVec3& zAxisIcrf)
    {
        // Rotation matrix columns are the body basis vectors expressed in ICRF.
        const double r00 = xAxisIcrf.X;
        const double r01 = yAxisIcrf.X;
        const double r02 = zAxisIcrf.X;
        const double r10 = xAxisIcrf.Y;
        const double r11 = yAxisIcrf.Y;
        const double r12 = zAxisIcrf.Y;
        const double r20 = xAxisIcrf.Z;
        const double r21 = yAxisIcrf.Z;
        const double r22 = zAxisIcrf.Z;

        TGQuat q{};
        const double trace = r00 + r11 + r22;
        if (trace > 0.0)
        {
            const double s = 2.0 * std::sqrt(trace + 1.0);
            q = {0.25 * s, (r21 - r12) / s, (r02 - r20) / s,
                 (r10 - r01) / s};
        }
        else if (r00 > r11 && r00 > r22)
        {
            const double s = 2.0 * std::sqrt(1.0 + r00 - r11 - r22);
            q = {(r21 - r12) / s, 0.25 * s, (r01 + r10) / s,
                 (r02 + r20) / s};
        }
        else if (r11 > r22)
        {
            const double s = 2.0 * std::sqrt(1.0 + r11 - r00 - r22);
            q = {(r02 - r20) / s, (r01 + r10) / s, 0.25 * s,
                 (r12 + r21) / s};
        }
        else
        {
            const double s = 2.0 * std::sqrt(1.0 + r22 - r00 - r11);
            q = {(r10 - r01) / s, (r02 + r20) / s,
                 (r12 + r21) / s, 0.25 * s};
        }
        const double norm = std::sqrt(
            q.W * q.W + q.X * q.X + q.Y * q.Y + q.Z * q.Z);
        return {q.W / norm, q.X / norm, q.Y / norm, q.Z / norm};
    }

    inline TGQuat PointBodyXAt(
        const TGControlInput& input,
        const TGVec3& desiredX)
    {
        const TGCelestialBodyStateView* sun = FindBody(input, "Sun");
        const TGVec3 x = Unit(desiredX, {1.0, 0.0, 0.0});
        TGVec3 zHint = sun != nullptr
            ? Subtract(sun->PositionIcrfMeters,
                       input.SpacecraftState.PositionIcrfMeters)
            : TGVec3{0.0, 0.0, 1.0};
        zHint = Subtract(zHint, Scale(x, Dot(zHint, x)));
        const TGVec3 z = Unit(zHint, {0.0, 0.0, 1.0});
        const TGVec3 y = Unit(Cross(z, x), {0.0, 1.0, 0.0});
        return QuaternionFromAxes(x, y, Unit(Cross(x, y), z));
    }

    inline TGQuat SunPointingTarget(const TGControlInput& input)
    {
        const TGCelestialBodyStateView* sun = FindBody(input, "Sun");
        const TGCelestialBodyStateView* earth = FindBody(input, "Earth");
        const TGVec3 z = Unit(
            sun != nullptr
                ? Subtract(sun->PositionIcrfMeters,
                           input.SpacecraftState.PositionIcrfMeters)
                : TGVec3{0.0, 0.0, 1.0},
            {0.0, 0.0, 1.0});
        TGVec3 xHint = earth != nullptr
            ? Subtract(input.SpacecraftState.VelocityIcrfMetersPerSecond,
                       earth->VelocityIcrfMetersPerSecond)
            : input.SpacecraftState.VelocityIcrfMetersPerSecond;
        xHint = Subtract(xHint, Scale(z, Dot(xHint, z)));
        const TGVec3 x = Unit(xHint, {1.0, 0.0, 0.0});
        const TGVec3 y = Unit(Cross(z, x), {0.0, 1.0, 0.0});
        return QuaternionFromAxes(x, y, Unit(Cross(x, y), z));
    }

    inline TGVec3 EarthRelativeVelocityDirection(const TGControlInput& input)
    {
        const TGCelestialBodyStateView* earth = FindBody(input, "Earth");
        return Unit(
            earth != nullptr
                ? Subtract(input.SpacecraftState.VelocityIcrfMetersPerSecond,
                           earth->VelocityIcrfMetersPerSecond)
                : input.SpacecraftState.VelocityIcrfMetersPerSecond,
            {1.0, 0.0, 0.0});
    }

    inline TGVec3 Mcc1aDeltaVVector()
    {
        return {JWST_MCC1A_DVX, JWST_MCC1A_DVY, JWST_MCC1A_DVZ};
    }

    inline TGVec3 Mcc1bDeltaVVector()
    {
        return {JWST_MCC1B_DVX, JWST_MCC1B_DVY, JWST_MCC1B_DVZ};
    }

    inline TGVec3 Mcc2DeltaVVector()
    {
        return {JWST_MCC2_DVX, JWST_MCC2_DVY, JWST_MCC2_DVZ};
    }

    inline TGVec3 BurnVectorForScenario(const TGControlInput& input)
    {
        if (Contains(input.Configuration.ScenarioName, "MCC1A"))
            return Mcc1aDeltaVVector();
        if (Contains(input.Configuration.ScenarioName, "MCC1B"))
            return Mcc1bDeltaVVector();
        return Mcc2DeltaVVector();
    }

    inline void ApplyAttitudePd(
        const TGControlInput& input,
        TGControlOutput& output,
        const TGQuat& target)
    {
        TGQuat error = Multiply(
            Conjugate(input.SpacecraftState.AttitudeBodyToIcrf), target);
        if (error.W < 0.0)
        {
            error = {-error.W, -error.X, -error.Y, -error.Z};
        }

        // Quaternion PD law in body axes. Wheel momentum rate creates the
        // opposite body torque, hence h_dot = -tau for each aligned wheel.
        constexpr double proportionalGain = 0.12;
        constexpr double derivativeGain = 120.0;
        constexpr double maximumTorque = 0.20;
        const TGVec3 omega =
            input.SpacecraftState.AngularVelocityBodyRadiansPerSecond;
        const TGVec3 torque = {
            Clamp(2.0 * proportionalGain * error.X -
                      derivativeGain * omega.X,
                  -maximumTorque, maximumTorque),
            Clamp(2.0 * proportionalGain * error.Y -
                      derivativeGain * omega.Y,
                  -maximumTorque, maximumTorque),
            Clamp(2.0 * proportionalGain * error.Z -
                      derivativeGain * omega.Z,
                  -maximumTorque, maximumTorque)};

        const uint64_t x = FindWheel(input, "RW_X");
        const uint64_t y = FindWheel(input, "RW_Y");
        const uint64_t z = FindWheel(input, "RW_Z");
        if (x < output.ReactionWheelMomentumRateCount)
            output.ReactionWheelMomentumRatesNewtonMeters[x] = -torque.X;
        if (y < output.ReactionWheelMomentumRateCount)
            output.ReactionWheelMomentumRatesNewtonMeters[y] = -torque.Y;
        if (z < output.ReactionWheelMomentumRateCount)
            output.ReactionWheelMomentumRatesNewtonMeters[z] = -torque.Z;
    }

    inline void CommandScatBurn(
        const TGControlInput& input,
        TGControlOutput& output,
        double targetDeltaVMetersPerSecond)
    {
        const uint64_t thruster = FindThruster(input, "SCAT");
        if (thruster >= output.ThrusterCommandCount) return;
        const TGThrusterStateView& state = input.Thrusters[thruster];
        if (state.Mode != TG_THRUSTER_COMMANDED ||
            state.FiringWindowOpen == 0 ||
            state.PropellantAvailable == 0) return;

        double initialMassKilograms = 0.0;
        for (uint64_t index = 0; index < input.ComponentCount; ++index)
            initialMassKilograms += input.Components[index].InitialMassKilograms;

        const double duration =
            input.Configuration.FinalEphemerisTimeTdbSeconds -
            input.Configuration.StartEphemerisTimeTdbSeconds;
        if (!(duration > 0.0) || !(initialMassKilograms > 0.0)) return;
        const double exhaustVelocity = kIspSeconds * kStandardGravity;
        const double requiredThrust = initialMassKilograms *
            (1.0 - std::exp(-targetDeltaVMetersPerSecond / exhaustVelocity)) *
            exhaustVelocity / duration;
        // Pure time-domain first-order actuator response.  This is the closed
        // form response of tau*y_dot + y = u to a step, so it remains safe
        // under repeated/non-monotonic Runge-Kutta stage evaluations.  The
        // scale factor preserves the requested total impulse over the phase.
        constexpr double timeConstantSeconds = 5.0;
        const double impulseResponseArea = duration - timeConstantSeconds *
            (1.0 - std::exp(-duration / timeConstantSeconds));
        const double steadyThrottle = requiredThrust * duration /
            (kMaximumScatThrustNewtons * impulseResponseArea);
        const double lagResponse = 1.0 - std::exp(
            -std::max(0.0, input.ElapsedSimulationTimeSeconds) /
            timeConstantSeconds);
        output.ThrusterCommands[thruster].Throttle = Clamp(
            steadyThrottle * lagResponse, 0.0, 1.0);
        output.ThrusterCommands[thruster].SpecificImpulseSeconds = kIspSeconds;
        auto& command = output.ThrusterCommands[thruster];
        command.MassFlowDerivativeProvided = 1;
        const double rawThrottle = steadyThrottle * lagResponse;
        const double throttleDerivative = rawThrottle >= 1.0 ? 0.0 :
            steadyThrottle * std::exp(-std::max(0.0,
                input.ElapsedSimulationTimeSeconds) / timeConstantSeconds) /
            timeConstantSeconds;
        command.MassFlowDerivativeKilogramsPerSecondSquared =
            kMaximumScatThrustNewtons * throttleDerivative / exhaustVelocity;

    }
}
