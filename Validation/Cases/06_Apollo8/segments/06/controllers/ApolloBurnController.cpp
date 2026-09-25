#include "PHAROSControllerAPI.h"
#include "BurnConfig.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace
{
    constexpr double kRcsSpecificImpulseSeconds = 289.0;
    constexpr double kSpsSpecificImpulseSeconds = APOLLO_SPS_ISP_SECONDS;
    constexpr double kVentSpecificImpulseSeconds = 5.0;
    constexpr double kStandardGravityMetersPerSecondSquared = 9.80665;
    constexpr double kVentMaximumThrustNewtons = 100.0;
    constexpr double kPairMaximumTorqueNewtonMeters = 1780.0;
    constexpr double kAttitudeRateGain = 40000.0;
    constexpr double kAttitudeAngleToRateGain = 0.4;
    constexpr double kMaximumSlewRateRadiansPerSecond = 0.08;
    constexpr double kMaximumBurnMisalignmentRadians =
        1.0 * 3.14159265358979323846 / 180.0;

    double Clamp(const double value, const double minimum, const double maximum)
    {
        return value < minimum ? minimum : (value > maximum ? maximum : value);
    }

    bool Equals(const TGStringView view, const char* text)
    {
        const size_t length = std::strlen(text);
        return view.Length == static_cast<uint64_t>(length) &&
            (length == 0 || (view.Data != nullptr &&
                std::memcmp(view.Data, text, length) == 0));
    }

    uint64_t FindThruster(const TGControlInput& input, const char* name)
    {
        for (uint64_t index = 0; index < input.ThrusterCount; ++index)
            if (Equals(input.Thrusters[index].Name, name)) return index;
        return TG_CONTROLLER_INVALID_INDEX;
    }

    void CommandThruster(
        const TGControlInput& input,
        TGControlOutput& output,
        const char* name,
        const double throttle,
        const double specificImpulseSeconds)
    {
        const uint64_t index = FindThruster(input, name);
        if (index == TG_CONTROLLER_INVALID_INDEX ||
            index >= output.ThrusterCommandCount) return;
        const TGThrusterStateView& thruster = input.Thrusters[index];
        if (thruster.Mode != TG_THRUSTER_COMMANDED ||
            thruster.FiringWindowOpen == 0 ||
            thruster.PropellantAvailable == 0) return;
        output.ThrusterCommands[index].Throttle = Clamp(throttle, 0.0, 1.0);
        output.ThrusterCommands[index].SpecificImpulseSeconds =
            specificImpulseSeconds;
    }

    TGVec3 Normalize(const TGVec3& vector, const TGVec3& fallback)
    {
        const double norm = std::sqrt(
            vector.X * vector.X + vector.Y * vector.Y + vector.Z * vector.Z);
        if (!(norm > 1.0e-12)) return fallback;
        return {vector.X / norm, vector.Y / norm, vector.Z / norm};
    }

    const TGCelestialBodyStateView* FindBody(
        const TGControlInput& input, const int32_t naifId)
    {
        for (uint64_t index = 0; index < input.CelestialBodyCount; ++index)
            if (input.CelestialBodies[index].NaifId == naifId)
                return &input.CelestialBodies[index];
        return nullptr;
    }

    TGVec3 CircularizationDeltaVelocity(const TGControlInput& input)
    {
        const TGCelestialBodyStateView* body =
            FindBody(input, APOLLO_TARGET_BODY_NAIF_ID);
        if (body == nullptr) return {0.0, 0.0, 0.0};
        const TGVec3 position{
            input.SpacecraftState.PositionIcrfMeters.X - body->PositionIcrfMeters.X,
            input.SpacecraftState.PositionIcrfMeters.Y - body->PositionIcrfMeters.Y,
            input.SpacecraftState.PositionIcrfMeters.Z - body->PositionIcrfMeters.Z};
        const TGVec3 velocity{
            input.SpacecraftState.VelocityIcrfMetersPerSecond.X -
                body->VelocityIcrfMetersPerSecond.X,
            input.SpacecraftState.VelocityIcrfMetersPerSecond.Y -
                body->VelocityIcrfMetersPerSecond.Y,
            input.SpacecraftState.VelocityIcrfMetersPerSecond.Z -
                body->VelocityIcrfMetersPerSecond.Z};
        const double radius = std::sqrt(
            position.X * position.X + position.Y * position.Y + position.Z * position.Z);
        if (!(radius > 1.0)) return {0.0, 0.0, 0.0};
        const TGVec3 radial{position.X / radius, position.Y / radius, position.Z / radius};
        const double radialSpeed =
            velocity.X * radial.X + velocity.Y * radial.Y + velocity.Z * radial.Z;
        const TGVec3 transverse{
            velocity.X - radialSpeed * radial.X,
            velocity.Y - radialSpeed * radial.Y,
            velocity.Z - radialSpeed * radial.Z};
        const TGVec3 tangent = Normalize(transverse, {0.0, 1.0, 0.0});
        const double circularSpeed = std::sqrt(
            body->GravitationalParameterMetersCubedPerSecondSquared / radius);
        return {
            circularSpeed * tangent.X - velocity.X,
            circularSpeed * tangent.Y - velocity.Y,
            circularSpeed * tangent.Z - velocity.Z};
    }

    double VectorNorm(const TGVec3& vector)
    {
        return std::sqrt(
            vector.X * vector.X + vector.Y * vector.Y + vector.Z * vector.Z);
    }

    TGVec3 TargetDirectionIcrf(const TGControlInput& input)
    {
        const TGVec3 fixed = Normalize(
            {APOLLO_TARGET_X, APOLLO_TARGET_Y, APOLLO_TARGET_Z},
            {1.0, 0.0, 0.0});
#if APOLLO_TARGET_MODE == 5
        return Normalize(CircularizationDeltaVelocity(input), fixed);
#elif APOLLO_TARGET_MODE >= 1 && APOLLO_TARGET_MODE <= 4
        const TGCelestialBodyStateView* body =
            FindBody(input, APOLLO_TARGET_BODY_NAIF_ID);
        if (body == nullptr) return fixed;
        TGVec3 relativeVelocity{
            input.SpacecraftState.VelocityIcrfMetersPerSecond.X -
                body->VelocityIcrfMetersPerSecond.X,
            input.SpacecraftState.VelocityIcrfMetersPerSecond.Y -
                body->VelocityIcrfMetersPerSecond.Y,
            input.SpacecraftState.VelocityIcrfMetersPerSecond.Z -
                body->VelocityIcrfMetersPerSecond.Z};
#if APOLLO_TARGET_MODE == 3 || APOLLO_TARGET_MODE == 4
        const TGVec3 relativePosition{
            input.SpacecraftState.PositionIcrfMeters.X -
                body->PositionIcrfMeters.X,
            input.SpacecraftState.PositionIcrfMeters.Y -
                body->PositionIcrfMeters.Y,
            input.SpacecraftState.PositionIcrfMeters.Z -
                body->PositionIcrfMeters.Z};
        const TGVec3 radial = Normalize(relativePosition, {1.0, 0.0, 0.0});
        const double radialSpeed =
            relativeVelocity.X * radial.X +
            relativeVelocity.Y * radial.Y +
            relativeVelocity.Z * radial.Z;
        relativeVelocity.X -= radialSpeed * radial.X;
        relativeVelocity.Y -= radialSpeed * radial.Y;
        relativeVelocity.Z -= radialSpeed * radial.Z;
        const double transverseNorm = std::sqrt(
            relativeVelocity.X * relativeVelocity.X +
            relativeVelocity.Y * relativeVelocity.Y +
            relativeVelocity.Z * relativeVelocity.Z);
        relativeVelocity.X += APOLLO_RADIAL_BIAS * transverseNorm * radial.X;
        relativeVelocity.Y += APOLLO_RADIAL_BIAS * transverseNorm * radial.Y;
        relativeVelocity.Z += APOLLO_RADIAL_BIAS * transverseNorm * radial.Z;
        const TGVec3 orbitNormal = Normalize({
            radial.Y * relativeVelocity.Z - radial.Z * relativeVelocity.Y,
            radial.Z * relativeVelocity.X - radial.X * relativeVelocity.Z,
            radial.X * relativeVelocity.Y - radial.Y * relativeVelocity.X},
            {0.0, 0.0, 1.0});
        relativeVelocity.X += APOLLO_NORMAL_BIAS * transverseNorm * orbitNormal.X;
        relativeVelocity.Y += APOLLO_NORMAL_BIAS * transverseNorm * orbitNormal.Y;
        relativeVelocity.Z += APOLLO_NORMAL_BIAS * transverseNorm * orbitNormal.Z;
#endif
#if APOLLO_TARGET_MODE == 1 || APOLLO_TARGET_MODE == 3
        relativeVelocity.X = -relativeVelocity.X;
        relativeVelocity.Y = -relativeVelocity.Y;
        relativeVelocity.Z = -relativeVelocity.Z;
#endif
        return Normalize(relativeVelocity, fixed);
#else
        return fixed;
#endif
    }

    TGVec3 TargetDirectionBody(
        const TGQuat& quaternion, const TGVec3& targetIcrf)
    {
        const double norm = std::sqrt(
            quaternion.W * quaternion.W + quaternion.X * quaternion.X +
            quaternion.Y * quaternion.Y + quaternion.Z * quaternion.Z);
        const double w = quaternion.W / norm;
        const double x = quaternion.X / norm;
        const double y = quaternion.Y / norm;
        const double z = quaternion.Z / norm;
        const double tx = targetIcrf.X;
        const double ty = targetIcrf.Y;
        const double tz = targetIcrf.Z;

        const double r00 = 1.0 - 2.0 * (y * y + z * z);
        const double r01 = 2.0 * (x * y - w * z);
        const double r02 = 2.0 * (x * z + w * y);
        const double r10 = 2.0 * (x * y + w * z);
        const double r11 = 1.0 - 2.0 * (x * x + z * z);
        const double r12 = 2.0 * (y * z - w * x);
        const double r20 = 2.0 * (x * z - w * y);
        const double r21 = 2.0 * (y * z + w * x);
        const double r22 = 1.0 - 2.0 * (x * x + y * y);

        return {
            r00 * tx + r10 * ty + r20 * tz,
            r01 * tx + r11 * ty + r21 * tz,
            r02 * tx + r12 * ty + r22 * tz};
    }

    double AlignmentAngleRadians(const TGVec3& targetBody)
    {
        return std::atan2(
            std::sqrt(targetBody.Y * targetBody.Y + targetBody.Z * targetBody.Z),
            Clamp(targetBody.X, -1.0, 1.0));
    }

    void CommandTorqueAxis(
        const TGControlInput& input,
        TGControlOutput& output,
        const double torque,
        const char* positiveA,
        const char* positiveB,
        const char* negativeA,
        const char* negativeB)
    {
        const double throttle = Clamp(
            std::abs(torque) / kPairMaximumTorqueNewtonMeters, 0.0, 1.0);
        if (torque >= 0.0)
        {
            CommandThruster(input, output, positiveA, throttle,
                kRcsSpecificImpulseSeconds);
            CommandThruster(input, output, positiveB, throttle,
                kRcsSpecificImpulseSeconds);
        }
        else
        {
            CommandThruster(input, output, negativeA, throttle,
                kRcsSpecificImpulseSeconds);
            CommandThruster(input, output, negativeB, throttle,
                kRcsSpecificImpulseSeconds);
        }
    }
}

class TGUserController final
{
public:
    double NextDiscontinuityElapsedTime(const double current) const
    {
        double next = std::numeric_limits<double>::infinity();
        const double switches[] = {
            APOLLO_BURN_START_S,
            APOLLO_BURN_END_S,
            APOLLO_ULLAGE_START_S,
            APOLLO_ULLAGE_END_S};
        for (const double candidate : switches)
            if (candidate > current && candidate < next) next = candidate;
        return next;
    }

    void ComputeControl(const TGControlInput& input, TGControlOutput& output)
    {
        const TGVec3 targetIcrf = TargetDirectionIcrf(input);
        const TGVec3 targetBody = TargetDirectionBody(
            input.SpacecraftState.AttitudeBodyToIcrf, targetIcrf);
        const double transverse = std::sqrt(
            targetBody.Y * targetBody.Y + targetBody.Z * targetBody.Z);
        const double angle = AlignmentAngleRadians(targetBody);
        TGVec3 attitudeError{0.0, 0.0, 0.0};
        if (transverse > 1.0e-12)
        {
            attitudeError.Y = -targetBody.Z * angle / transverse;
            attitudeError.Z = targetBody.Y * angle / transverse;
        }
        else if (targetBody.X < 0.0)
        {
            attitudeError.Y = angle;
        }

        const double desiredSpeed = Clamp(
            kAttitudeAngleToRateGain * angle,
            0.0,
            kMaximumSlewRateRadiansPerSecond);
        const double inverseAngle = angle > 1.0e-12 ? 1.0 / angle : 0.0;
        const TGVec3 desiredRate{
            0.0,
            attitudeError.Y * inverseAngle * desiredSpeed,
            attitudeError.Z * inverseAngle * desiredSpeed};
        const TGVec3 rate =
            input.SpacecraftState.AngularVelocityBodyRadiansPerSecond;
        const TGVec3 torque{
            kAttitudeRateGain * (desiredRate.X - rate.X),
            kAttitudeRateGain * (desiredRate.Y - rate.Y),
            kAttitudeRateGain * (desiredRate.Z - rate.Z)};

        CommandTorqueAxis(input, output, torque.X,
            "Roll Plus A", "Roll Plus B", "Roll Minus A", "Roll Minus B");
        CommandTorqueAxis(input, output, torque.Y,
            "Pitch Plus A", "Pitch Plus B", "Pitch Minus A", "Pitch Minus B");
        CommandTorqueAxis(input, output, torque.Z,
            "Yaw Plus A", "Yaw Plus B", "Yaw Minus A", "Yaw Minus B");

        const double elapsed = input.ElapsedSimulationTimeSeconds;
        if (elapsed < APOLLO_BURN_START_S &&
            APOLLO_TARGET_PREBURN_MASS_KG > 0.0)
        {
            const double excessMass = input.SpacecraftState.TotalMassKilograms -
                APOLLO_TARGET_PREBURN_MASS_KG;
            if (excessMass > 1.0e-6)
            {
                const double requestedMassRate = excessMass / 5.0;
                const double pairMaximumMassRate =
                    2.0 * kVentMaximumThrustNewtons /
                    (kVentSpecificImpulseSeconds *
                        kStandardGravityMetersPerSecondSquared);
                const double ventThrottle =
                    requestedMassRate / pairMaximumMassRate;
                CommandThruster(input, output, "Balanced Vent Plus",
                    ventThrottle, kVentSpecificImpulseSeconds);
                CommandThruster(input, output, "Balanced Vent Minus",
                    ventThrottle, kVentSpecificImpulseSeconds);
            }
        }

        const bool ullageRequested = APOLLO_ULLAGE_ENABLED != 0 &&
            elapsed >= APOLLO_ULLAGE_START_S &&
            elapsed < APOLLO_ULLAGE_END_S;
        if (ullageRequested && angle <= kMaximumBurnMisalignmentRadians)
        {
            CommandThruster(input, output, "SM RCS Translation Cluster",
                APOLLO_ULLAGE_THROTTLE, kRcsSpecificImpulseSeconds);
        }

        const bool burnRequested = elapsed >= APOLLO_BURN_START_S &&
            elapsed < APOLLO_BURN_END_S;
        if (burnRequested && angle <= kMaximumBurnMisalignmentRadians)
        {
            double burnThrottle = APOLLO_BURN_THROTTLE;
#if APOLLO_TARGET_MODE == 5
            const double remaining = VectorNorm(CircularizationDeltaVelocity(input));
            burnThrottle *= Clamp(
                (remaining - APOLLO_CIRCULARIZATION_STOP_MPS) / 2.0,
                0.0, 1.0);
#endif
#if APOLLO_USE_SPS
            CommandThruster(input, output, "SPS Main Engine",
                burnThrottle, kSpsSpecificImpulseSeconds);
#else
            CommandThruster(input, output, "SM RCS Translation Cluster",
                burnThrottle, kRcsSpecificImpulseSeconds);
#endif
        }

        output.AdditionalExternalTorqueBodyNewtonMeters = {0.0, 0.0, 0.0};
    }
};

PHAROS_CONTROLLER_EXPORT uint32_t PHAROS_CONTROLLER_CALL
PHAROS_DescribeControllerContract(PHAROSControllerContract* Contract)
{
    if (Contract == nullptr ||
        Contract->StructSize != sizeof(PHAROSControllerContract))
    {
        return PHAROS_CONTROLLER_RESULT_INVALID_ARGUMENT;
    }

    *Contract = PHAROSControllerContract{};
    Contract->StructSize = sizeof(PHAROSControllerContract);
    Contract->ControlInputSize = sizeof(PHAROSControlInput);
    Contract->ControlOutputSize = sizeof(PHAROSControlOutput);
    Contract->ThrusterCommandSize = sizeof(PHAROSThrusterCommand);
    Contract->SimulationConfigurationSize =
        sizeof(PHAROSSimulationConfiguration);
    Contract->SpacecraftStateViewSize = sizeof(PHAROSSpacecraftStateView);
    Contract->ComponentStateViewSize = sizeof(PHAROSComponentStateView);
    Contract->JointStateViewSize = sizeof(PHAROSJointStateView);
    Contract->ThrusterStateViewSize = sizeof(PHAROSThrusterStateView);
    Contract->ReactionWheelStateViewSize =
        sizeof(PHAROSReactionWheelStateView);
    Contract->CelestialBodyStateViewSize =
        sizeof(PHAROSCelestialBodyStateView);
    return PHAROS_CONTROLLER_RESULT_OK;
}

TG_CONTROLLER_EXPORT void* TG_CONTROLLER_CALL
TG_CreateController(void)
{
    return new (std::nothrow) TGUserController();
}

TG_CONTROLLER_EXPORT void TG_CONTROLLER_CALL
TG_DestroyController(void* controller)
{
    delete static_cast<TGUserController*>(controller);
}

TG_CONTROLLER_EXPORT uint32_t TG_CONTROLLER_CALL
TG_ComputeControl(
    void* controller,
    const TGControlInput* input,
    TGControlOutput* output)
{
    if (controller == nullptr || input == nullptr || output == nullptr)
        return TG_CONTROLLER_RESULT_INVALID_ARGUMENT;
    if (input->StructSize != sizeof(TGControlInput) ||
        output->StructSize != sizeof(TGControlOutput))
        return TG_CONTROLLER_RESULT_INVALID_ARGUMENT;
    try
    {
        static_cast<TGUserController*>(controller)->ComputeControl(
            *input, *output);
    }
    catch (...)
    {
        return TG_CONTROLLER_RESULT_USER_EXCEPTION;
    }
    return TG_CONTROLLER_RESULT_OK;
}

TG_CONTROLLER_EXPORT double TG_CONTROLLER_CALL
TG_NextControllerDiscontinuityElapsedTime(
    void* controller,
    double currentElapsedSimulationTimeSeconds)
{
    if (controller == nullptr)
        return std::numeric_limits<double>::infinity();
    try
    {
        return static_cast<TGUserController*>(controller)->
            NextDiscontinuityElapsedTime(currentElapsedSimulationTimeSeconds);
    }
    catch (...)
    {
        return std::numeric_limits<double>::infinity();
    }
}
