#include "TGControllerAPI.h"
#include "ControllerConfig.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace
{
    constexpr double kPi = 3.1415926535897932384626433832795;
    constexpr double kMaximumWheelTorqueNewtonMeters = 0.14;
    constexpr double kPointingStiffnessNewtonMetersPerRadian = 0.30;
    constexpr double kRateDampingNewtonMeterSeconds = 180.0;
    constexpr double kMaximumBurnMisalignmentRadians = 0.5 * kPi / 180.0;

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

    TGVec3 Normalize(const TGVec3& vector)
    {
        const double magnitude = std::sqrt(
            vector.X * vector.X + vector.Y * vector.Y + vector.Z * vector.Z);
        if (!(magnitude > 0.0) || !std::isfinite(magnitude))
            return {1.0, 0.0, 0.0};
        return {vector.X / magnitude, vector.Y / magnitude, vector.Z / magnitude};
    }

    const TGCelestialBodyStateView* FindBody(
        const TGControlInput& input, const char* name)
    {
        for (uint64_t index = 0; index < input.CelestialBodyCount; ++index)
            if (Equals(input.CelestialBodies[index].Name, name))
                return &input.CelestialBodies[index];
        return nullptr;
    }

    uint64_t FindThruster(const TGControlInput& input, const char* name)
    {
        for (uint64_t index = 0; index < input.ThrusterCount; ++index)
            if (Equals(input.Thrusters[index].Name, name)) return index;
        return TG_CONTROLLER_INVALID_INDEX;
    }

    uint64_t FindWheel(const TGControlInput& input, const char* name)
    {
        for (uint64_t index = 0; index < input.ReactionWheelCount; ++index)
            if (Equals(input.ReactionWheels[index].Name, name)) return index;
        return TG_CONTROLLER_INVALID_INDEX;
    }

    TGVec3 RotateIcrfToBody(const TGQuat& quaternion, const TGVec3& vector)
    {
        const double qnorm = std::sqrt(
            quaternion.W * quaternion.W + quaternion.X * quaternion.X +
            quaternion.Y * quaternion.Y + quaternion.Z * quaternion.Z);
        const double w = quaternion.W / qnorm;
        const double x = quaternion.X / qnorm;
        const double y = quaternion.Y / qnorm;
        const double z = quaternion.Z / qnorm;

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
            r00 * vector.X + r10 * vector.Y + r20 * vector.Z,
            r01 * vector.X + r11 * vector.Y + r21 * vector.Z,
            r02 * vector.X + r12 * vector.Y + r22 * vector.Z};
    }

    TGVec3 FixedInitialTarget()
    {
        return Normalize({
            CASSINI_INITIAL_TARGET_X,
            CASSINI_INITIAL_TARGET_Y,
            CASSINI_INITIAL_TARGET_Z});
    }

    TGVec3 FixedFinalTarget()
    {
        return Normalize({
            CASSINI_FINAL_TARGET_X,
            CASSINI_FINAL_TARGET_Y,
            CASSINI_FINAL_TARGET_Z});
    }

    TGVec3 SunTarget(const TGControlInput& input)
    {
        const TGCelestialBodyStateView* sun = FindBody(input, "Sun");
        if (sun == nullptr) return {1.0, 0.0, 0.0};
        return Normalize({
            sun->PositionIcrfMeters.X - input.SpacecraftState.PositionIcrfMeters.X,
            sun->PositionIcrfMeters.Y - input.SpacecraftState.PositionIcrfMeters.Y,
            sun->PositionIcrfMeters.Z - input.SpacecraftState.PositionIcrfMeters.Z});
    }

    TGVec3 TargetDirectionIcrf(const TGControlInput& input)
    {
        const double elapsed = input.ElapsedSimulationTimeSeconds;
#if CASSINI_INITIAL_FIXED_ACTIVE
        if (elapsed < CASSINI_INITIAL_FIXED_END_S) return FixedInitialTarget();
#endif
#if CASSINI_FINAL_FIXED_ACTIVE
        if (elapsed >= CASSINI_FINAL_FIXED_START_S) return FixedFinalTarget();
#endif
        return SunTarget(input);
    }

    void CommandWheel(
        const TGControlInput& input,
        TGControlOutput& output,
        const char* name,
        const double spacecraftTorqueNewtonMeters)
    {
        const uint64_t index = FindWheel(input, name);
        if (index == TG_CONTROLLER_INVALID_INDEX ||
            index >= output.ReactionWheelMomentumRateCount) return;
        output.ReactionWheelMomentumRatesNewtonMeters[index] = -Clamp(
            spacecraftTorqueNewtonMeters,
            -kMaximumWheelTorqueNewtonMeters,
            kMaximumWheelTorqueNewtonMeters);
    }

    double AlignmentAngleRadians(const TGControlInput& input)
    {
        const TGVec3 targetBody = RotateIcrfToBody(
            input.SpacecraftState.AttitudeBodyToIcrf,
            TargetDirectionIcrf(input));
        return std::atan2(
            std::sqrt(targetBody.Y * targetBody.Y + targetBody.Z * targetBody.Z),
            Clamp(targetBody.X, -1.0, 1.0));
    }

    void CommandAttitude(const TGControlInput& input, TGControlOutput& output)
    {
        const TGVec3 targetBody = RotateIcrfToBody(
            input.SpacecraftState.AttitudeBodyToIcrf,
            TargetDirectionIcrf(input));
        const double transverse = std::sqrt(
            targetBody.Y * targetBody.Y + targetBody.Z * targetBody.Z);
        const double angle = std::atan2(
            transverse, Clamp(targetBody.X, -1.0, 1.0));

        TGVec3 error{0.0, 0.0, 0.0};
        if (transverse > 1.0e-14)
        {
            error.Y = -targetBody.Z * angle / transverse;
            error.Z = targetBody.Y * angle / transverse;
        }
        else if (targetBody.X < 0.0)
        {
            error.Y = angle;
        }

        const TGVec3 rate = input.SpacecraftState.AngularVelocityBodyRadiansPerSecond;
        const TGVec3 torque{
            -kRateDampingNewtonMeterSeconds * rate.X,
            kPointingStiffnessNewtonMetersPerRadian * error.Y -
                kRateDampingNewtonMeterSeconds * rate.Y,
            kPointingStiffnessNewtonMetersPerRadian * error.Z -
                kRateDampingNewtonMeterSeconds * rate.Z};

        CommandWheel(input, output, "X Reaction Wheel", torque.X);
        CommandWheel(input, output, "Y Reaction Wheel", torque.Y);
        CommandWheel(input, output, "Z Reaction Wheel", torque.Z);
    }

    void CommandBurn(const TGControlInput& input, TGControlOutput& output)
    {
#if CASSINI_BURN_ENABLED
        const double elapsed = input.ElapsedSimulationTimeSeconds;
        if (elapsed < CASSINI_BURN_START_S || elapsed >= CASSINI_BURN_END_S)
            return;
        if (AlignmentAngleRadians(input) > kMaximumBurnMisalignmentRadians)
            return;
        const uint64_t index = FindThruster(input, CASSINI_THRUSTER_NAME);
        if (index == TG_CONTROLLER_INVALID_INDEX ||
            index >= output.ThrusterCommandCount) return;
        const TGThrusterStateView& thruster = input.Thrusters[index];
        if (thruster.Mode != TG_THRUSTER_COMMANDED ||
            thruster.FiringWindowOpen == 0 ||
            thruster.PropellantAvailable == 0) return;
        output.ThrusterCommands[index].Throttle = CASSINI_BURN_THROTTLE;
        output.ThrusterCommands[index].SpecificImpulseSeconds =
            CASSINI_THRUSTER_ISP_S;
#endif
    }

    double NextBoundary(const double current)
    {
        double next = std::numeric_limits<double>::infinity();
        auto consider = [&](const double candidate)
        {
            if (candidate > current && candidate < next) next = candidate;
        };
#if CASSINI_INITIAL_FIXED_ACTIVE
        consider(CASSINI_INITIAL_FIXED_END_S);
#endif
#if CASSINI_FINAL_FIXED_ACTIVE
        consider(CASSINI_FINAL_FIXED_START_S);
#endif
#if CASSINI_BURN_ENABLED
        consider(CASSINI_BURN_START_S);
        consider(CASSINI_BURN_END_S);
#endif
        return next;
    }
}

class TGUserController final
{
public:
    void ComputeControl(const TGControlInput& input, TGControlOutput& output)
    {
        CommandAttitude(input, output);
        CommandBurn(input, output);
        output.AdditionalExternalTorqueBodyNewtonMeters = {0.0, 0.0, 0.0};
    }
};

TG_CONTROLLER_EXPORT uint32_t TG_CONTROLLER_CALL TG_GetControllerApiVersion(void)
{
    return TG_CONTROLLER_API_VERSION;
}

TG_CONTROLLER_EXPORT void* TG_CONTROLLER_CALL TG_CreateController(void)
{
    return new (std::nothrow) TGUserController();
}

TG_CONTROLLER_EXPORT void TG_CONTROLLER_CALL TG_DestroyController(void* controller)
{
    delete static_cast<TGUserController*>(controller);
}

TG_CONTROLLER_EXPORT uint32_t TG_CONTROLLER_CALL TG_ComputeControl(
    void* controller,
    const TGControlInput* input,
    TGControlOutput* output)
{
    if (controller == nullptr || input == nullptr || output == nullptr)
        return TG_CONTROLLER_RESULT_INVALID_ARGUMENT;
    if (input->ApiVersion != TG_CONTROLLER_API_VERSION ||
        output->ApiVersion != TG_CONTROLLER_API_VERSION ||
        input->StructSize != sizeof(TGControlInput) ||
        output->StructSize != sizeof(TGControlOutput))
        return TG_CONTROLLER_RESULT_INVALID_ARGUMENT;
    try
    {
        static_cast<TGUserController*>(controller)->ComputeControl(*input, *output);
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
    const double currentElapsedSimulationTimeSeconds)
{
    if (controller == nullptr) return std::numeric_limits<double>::infinity();
    try
    {
        return NextBoundary(currentElapsedSimulationTimeSeconds);
    }
    catch (...)
    {
        return std::numeric_limits<double>::infinity();
    }
}
