#pragma once

#include "TGControllerAPI.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace earth_recovery
{
    struct Vec3
    {
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;
    };

    struct DisturbanceEvent
    {
        double StartSeconds;
        double YawRadians;
        double PitchRadians;
        double Throttle;
        double DurationSeconds;
    };

    constexpr double kSpecificImpulseSeconds = 220.0;
    constexpr double kGimbalLeadSeconds = 60.0;
    constexpr double kRecoveryHoldoffSeconds = 5.0;
    constexpr double kWheelMaximumTorqueNewtonMeters = 1.5;
    constexpr double kThrusterMaximumTorqueNewtonMeters = 3.0;
    constexpr double kAttitudeThrusterPairMaximumTorqueNewtonMeters = 4.5;

    // Generated once with Python random.Random(202612). The seed and the
    // resulting values are retained beside the scenarios for reproducibility.
    constexpr std::array<DisturbanceEvent, 5> kEvents{{
        {850.0, -0.647258325, 0.471389488, 0.804061222, 1.518065892},
        {1850.0, -0.011206874, 0.510018163, 0.653585449, 1.702822175},
        {2850.0, 0.222230751, -0.023297373, 0.846204807, 1.347288914},
        {3850.0, -0.168840199, -0.012544623, 0.791735791, 1.052048291},
        {4850.0, 0.942685556, 0.135039171, 0.706362793, 1.367501570},
    }};

    inline Vec3 operator+(const Vec3& a, const Vec3& b)
    {
        return {a.X + b.X, a.Y + b.Y, a.Z + b.Z};
    }

    inline Vec3 operator-(const Vec3& a, const Vec3& b)
    {
        return {a.X - b.X, a.Y - b.Y, a.Z - b.Z};
    }

    inline Vec3 operator*(const Vec3& value, const double scale)
    {
        return {value.X * scale, value.Y * scale, value.Z * scale};
    }

    inline double Dot(const Vec3& a, const Vec3& b)
    {
        return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
    }

    inline Vec3 Cross(const Vec3& a, const Vec3& b)
    {
        return {
            a.Y * b.Z - a.Z * b.Y,
            a.Z * b.X - a.X * b.Z,
            a.X * b.Y - a.Y * b.X};
    }

    inline double Norm(const Vec3& value)
    {
        return std::sqrt(Dot(value, value));
    }

    inline Vec3 Normalize(const Vec3& value, const Vec3& fallback)
    {
        const double magnitude = Norm(value);
        return std::isfinite(magnitude) && magnitude > 1.0e-15
            ? value * (1.0 / magnitude)
            : fallback;
    }

    inline Vec3 ClampMagnitude(const Vec3& value, const double maximum)
    {
        const double magnitude = Norm(value);
        return !std::isfinite(magnitude) || magnitude <= maximum
            ? value
            : value * (maximum / magnitude);
    }

    inline double Clamp(const double value, const double lower, const double upper)
    {
        return std::max(lower, std::min(value, upper));
    }

    inline Vec3 FromApi(const TGVec3 value)
    {
        return {value.X, value.Y, value.Z};
    }

    inline bool Equals(const TGStringView view, const char* text)
    {
        const std::size_t length = std::strlen(text);
        return view.Length == static_cast<std::uint64_t>(length) &&
            (length == 0 ||
             (view.Data != nullptr && std::memcmp(view.Data, text, length) == 0));
    }

    inline Vec3 RotateBodyToIcrf(const TGQuat& quaternion, const Vec3& body)
    {
        const Vec3 vectorPart{quaternion.X, quaternion.Y, quaternion.Z};
        return body + Cross(
            vectorPart,
            Cross(vectorPart, body) + body * quaternion.W) * 2.0;
    }

    inline Vec3 RotateIcrfToBody(const TGQuat& quaternion, const Vec3& icrf)
    {
        return RotateBodyToIcrf(
            {quaternion.W, -quaternion.X, -quaternion.Y, -quaternion.Z}, icrf);
    }

    inline Vec3 Multiply(const TGMat3& matrix, const Vec3& value)
    {
        return {
            matrix.M[0] * value.X + matrix.M[1] * value.Y + matrix.M[2] * value.Z,
            matrix.M[3] * value.X + matrix.M[4] * value.Y + matrix.M[5] * value.Z,
            matrix.M[6] * value.X + matrix.M[7] * value.Y + matrix.M[8] * value.Z};
    }

    inline const TGCelestialBodyStateView* FindBodyByNaifId(
        const TGControlInput& input,
        const std::int32_t naifId)
    {
        for (std::uint64_t index = 0; index < input.CelestialBodyCount; ++index)
        {
            if (input.CelestialBodies[index].NaifId == naifId)
                return &input.CelestialBodies[index];
        }
        return nullptr;
    }

    inline std::uint64_t FindThruster(const TGControlInput& input, const char* name)
    {
        for (std::uint64_t index = 0; index < input.ThrusterCount; ++index)
        {
            if (Equals(input.Thrusters[index].Name, name)) return index;
        }
        return TG_CONTROLLER_INVALID_INDEX;
    }

    inline std::uint64_t FindJoint(const TGControlInput& input, const char* name)
    {
        for (std::uint64_t index = 0; index < input.JointCount; ++index)
        {
            if (Equals(input.Joints[index].Name, name)) return index;
        }
        return TG_CONTROLLER_INVALID_INDEX;
    }

    inline bool RecoveryEnabled(const double elapsedSeconds)
    {
        for (const DisturbanceEvent& event : kEvents)
        {
            if (elapsedSeconds >= event.StartSeconds &&
                elapsedSeconds < event.StartSeconds + event.DurationSeconds +
                    kRecoveryHoldoffSeconds)
            {
                return false;
            }
        }
        return true;
    }

    inline const DisturbanceEvent* ActiveGimbalEvent(const double elapsedSeconds)
    {
        for (const DisturbanceEvent& event : kEvents)
        {
            if (elapsedSeconds >= event.StartSeconds - kGimbalLeadSeconds &&
                elapsedSeconds < event.StartSeconds + event.DurationSeconds)
            {
                return &event;
            }
        }
        return nullptr;
    }

    inline const DisturbanceEvent* ActiveFiringEvent(const double elapsedSeconds)
    {
        for (const DisturbanceEvent& event : kEvents)
        {
            if (elapsedSeconds >= event.StartSeconds &&
                elapsedSeconds < event.StartSeconds + event.DurationSeconds)
            {
                return &event;
            }
        }
        return nullptr;
    }

    inline void CommandGimbal(
        const TGControlInput& input,
        TGControlOutput& output)
    {
        const DisturbanceEvent* event =
            ActiveGimbalEvent(input.ElapsedSimulationTimeSeconds);
        const double yawTarget = event != nullptr ? event->YawRadians : 0.0;
        const double pitchTarget = event != nullptr ? event->PitchRadians : 0.0;
        const std::array<std::pair<const char*, double>, 2> targets{{
            {"Disturbance Yaw", yawTarget},
            {"Disturbance Pitch", pitchTarget},
        }};

        for (const auto& target : targets)
        {
            const std::uint64_t index = FindJoint(input, target.first);
            if (index >= input.JointCount || index >= output.JointEffortCount)
                continue;
            const TGJointStateView& joint = input.Joints[index];
            const double effort = 10.0 * (target.second - joint.Coordinate) -
                4.0 * joint.Rate;
            output.JointEffortsNewtonMetersOrNewtons[index] = Clamp(
                effort,
                -joint.MaximumAbsoluteEffort,
                joint.MaximumAbsoluteEffort);
        }
    }

    inline void CommandDisturbanceThruster(
        const TGControlInput& input,
        TGControlOutput& output)
    {
        const DisturbanceEvent* event =
            ActiveFiringEvent(input.ElapsedSimulationTimeSeconds);
        if (event == nullptr) return;
        const std::uint64_t index = FindThruster(input, "Gimballed Disturbance Thruster");
        if (index >= input.ThrusterCount || index >= output.ThrusterCommandCount)
            return;
        const TGThrusterStateView& thruster = input.Thrusters[index];
        if (thruster.FiringWindowOpen == 0 || thruster.PropellantAvailable == 0)
            return;
        output.ThrusterCommands[index].Throttle = event->Throttle;
        output.ThrusterCommands[index].SpecificImpulseSeconds =
            kSpecificImpulseSeconds;
    }

    inline Vec3 RequestedSunPointingTorque(
        const TGControlInput& input,
        const double maximumTorque)
    {
        const TGCelestialBodyStateView* sun = FindBodyByNaifId(input, 10);
        const TGCelestialBodyStateView* earth = FindBodyByNaifId(input, 399);
        if (sun == nullptr || earth == nullptr) return {};

        const Vec3 position = FromApi(input.SpacecraftState.PositionIcrfMeters);
        const Vec3 velocity = FromApi(
            input.SpacecraftState.VelocityIcrfMetersPerSecond);
        const Vec3 sunDirection = Normalize(
            FromApi(sun->PositionIcrfMeters) - position,
            {1.0, 0.0, 0.0});
        const Vec3 relativePosition = position - FromApi(earth->PositionIcrfMeters);
        const Vec3 relativeVelocity = velocity -
            FromApi(earth->VelocityIcrfMetersPerSecond);
        const Vec3 orbitNormal = Normalize(
            Cross(relativePosition, relativeVelocity),
            {0.0, 0.0, 1.0});

        Vec3 desiredZ = orbitNormal - sunDirection * Dot(orbitNormal, sunDirection);
        if (Norm(desiredZ) <= 1.0e-12)
        {
            const Vec3 reference = std::abs(sunDirection.Z) < 0.9
                ? Vec3{0.0, 0.0, 1.0}
                : Vec3{0.0, 1.0, 0.0};
            desiredZ = reference - sunDirection * Dot(reference, sunDirection);
        }
        desiredZ = Normalize(desiredZ, {0.0, 0.0, 1.0});
        const Vec3 desiredY = Normalize(
            Cross(desiredZ, sunDirection),
            {0.0, 1.0, 0.0});
        desiredZ = Normalize(Cross(sunDirection, desiredY), desiredZ);

        const TGQuat attitude = input.SpacecraftState.AttitudeBodyToIcrf;
        const Vec3 currentX = RotateBodyToIcrf(attitude, {1.0, 0.0, 0.0});
        const Vec3 currentY = RotateBodyToIcrf(attitude, {0.0, 1.0, 0.0});
        const Vec3 currentZ = RotateBodyToIcrf(attitude, {0.0, 0.0, 1.0});
        const Vec3 errorIcrf = (
            Cross(currentX, sunDirection) +
            Cross(currentY, desiredY) +
            Cross(currentZ, desiredZ)) * 0.5;
        const Vec3 errorBody = RotateIcrfToBody(attitude, errorIcrf);
        const Vec3 omega = FromApi(
            input.SpacecraftState.AngularVelocityBodyRadiansPerSecond);
        return ClampMagnitude(errorBody * 1.5 - omega * 35.0, maximumTorque);
    }

    inline void CommandFourWheelRecovery(
        const TGControlInput& input,
        TGControlOutput& output,
        const Vec3& requestedTorque)
    {
        for (std::uint64_t index = 0;
             index < input.ReactionWheelCount &&
             index < output.ReactionWheelMomentumRateCount;
             ++index)
        {
            const TGReactionWheelStateView& wheel = input.ReactionWheels[index];
            if (wheel.MountComponentIndex >= input.ComponentCount) continue;
            const Vec3 axisBody = Normalize(
                Multiply(
                    input.Components[wheel.MountComponentIndex].CurrentComponentToBody,
                    FromApi(wheel.AxisComponent)),
                {1.0, 0.0, 0.0});
            // The tetrahedral axes satisfy A*A^T = (4/3)I, so this is the
            // minimum-norm redundant allocation h_dot = -A^T(AA^T)^-1 tau.
            double momentumRate = -0.75 * Dot(requestedTorque, axisBody);
            if ((wheel.SaturatedPositive != 0 && momentumRate > 0.0) ||
                (wheel.SaturatedNegative != 0 && momentumRate < 0.0))
            {
                momentumRate = 0.0;
            }
            output.ReactionWheelMomentumRatesNewtonMeters[index] = momentumRate;
        }
    }

    inline void CommandThruster(
        const TGControlInput& input,
        TGControlOutput& output,
        const char* name,
        const double throttle)
    {
        if (!(throttle > 0.0)) return;
        const std::uint64_t index = FindThruster(input, name);
        if (index >= input.ThrusterCount || index >= output.ThrusterCommandCount)
            return;
        const TGThrusterStateView& state = input.Thrusters[index];
        if (state.FiringWindowOpen == 0 || state.PropellantAvailable == 0) return;
        output.ThrusterCommands[index].Throttle = Clamp(throttle, 0.0, 1.0);
        output.ThrusterCommands[index].SpecificImpulseSeconds =
            kSpecificImpulseSeconds;
    }

    inline void CommandAxisThrusterPair(
        const TGControlInput& input,
        TGControlOutput& output,
        const char axis,
        const double requestedTorque)
    {
        if (std::abs(requestedTorque) < 5.0e-3) return;
        const double throttle = std::abs(requestedTorque) /
            kAttitudeThrusterPairMaximumTorqueNewtonMeters;
        const bool positive = requestedTorque > 0.0;
        const char* first = nullptr;
        const char* second = nullptr;
        if (axis == 'X')
        {
            first = positive ? "Roll Plus A" : "Roll Minus A";
            second = positive ? "Roll Plus B" : "Roll Minus B";
        }
        else if (axis == 'Y')
        {
            first = positive ? "Pitch Plus A" : "Pitch Minus A";
            second = positive ? "Pitch Plus B" : "Pitch Minus B";
        }
        else
        {
            first = positive ? "Yaw Plus A" : "Yaw Minus A";
            second = positive ? "Yaw Plus B" : "Yaw Minus B";
        }
        CommandThruster(input, output, first, throttle);
        CommandThruster(input, output, second, throttle);
    }

    inline void CommandThrusterRecovery(
        const TGControlInput& input,
        TGControlOutput& output,
        const Vec3& requestedTorque)
    {
        CommandAxisThrusterPair(input, output, 'X', requestedTorque.X);
        CommandAxisThrusterPair(input, output, 'Y', requestedTorque.Y);
        CommandAxisThrusterPair(input, output, 'Z', requestedTorque.Z);
    }

    inline void ComputeControl(
        const bool useReactionWheels,
        const TGControlInput& input,
        TGControlOutput& output)
    {
        CommandGimbal(input, output);
        CommandDisturbanceThruster(input, output);
        if (!RecoveryEnabled(input.ElapsedSimulationTimeSeconds)) return;
        const Vec3 torque = RequestedSunPointingTorque(
            input,
            useReactionWheels
                ? kWheelMaximumTorqueNewtonMeters
                : kThrusterMaximumTorqueNewtonMeters);
        if (useReactionWheels)
            CommandFourWheelRecovery(input, output, torque);
        else
            CommandThrusterRecovery(input, output, torque);
    }

    inline double NextDiscontinuity(const double currentSeconds)
    {
        double next = std::numeric_limits<double>::infinity();
        for (const DisturbanceEvent& event : kEvents)
        {
            const std::array<double, 4> boundaries{{
                event.StartSeconds - kGimbalLeadSeconds,
                event.StartSeconds,
                event.StartSeconds + event.DurationSeconds,
                event.StartSeconds + event.DurationSeconds +
                    kRecoveryHoldoffSeconds,
            }};
            for (const double boundary : boundaries)
            {
                if (boundary > currentSeconds && boundary < next) next = boundary;
            }
        }
        return next;
    }
}

