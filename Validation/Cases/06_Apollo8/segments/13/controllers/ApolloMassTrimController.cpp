#include "PHAROSControllerAPI.h"
#include "MassTrimConfig.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <new>

namespace
{
    constexpr double kSpecificImpulseSeconds = 5.0;
    constexpr double kStandardGravityMetersPerSecondSquared = 9.80665;
    constexpr double kMaximumThrustNewtons = 100.0;

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

    void Command(
        const TGControlInput& input,
        TGControlOutput& output,
        const char* name,
        const double throttle)
    {
        for (uint64_t index = 0; index < input.ThrusterCount; ++index)
        {
            if (!Equals(input.Thrusters[index].Name, name) ||
                index >= output.ThrusterCommandCount) continue;
            const TGThrusterStateView& thruster = input.Thrusters[index];
            if (thruster.Mode == TG_THRUSTER_COMMANDED &&
                thruster.FiringWindowOpen != 0 &&
                thruster.PropellantAvailable != 0)
            {
                output.ThrusterCommands[index].Throttle =
                    Clamp(throttle, 0.0, 1.0);
                output.ThrusterCommands[index].SpecificImpulseSeconds =
                    kSpecificImpulseSeconds;
            }
            return;
        }
    }
}

class TGUserController final
{
public:
    double NextDiscontinuityElapsedTime(double) const
    {
        return std::numeric_limits<double>::infinity();
    }

    void ComputeControl(const TGControlInput& input, TGControlOutput& output)
    {
        const double elapsed = Clamp(
            input.ElapsedSimulationTimeSeconds, 0.0, APOLLO_TRIM_DURATION_S);
        const double fraction = APOLLO_TRIM_DURATION_S > 0.0
            ? elapsed / APOLLO_TRIM_DURATION_S : 1.0;
        const double scheduledMass = APOLLO_INITIAL_MASS_KG +
            fraction * (APOLLO_TARGET_FINAL_MASS_KG - APOLLO_INITIAL_MASS_KG);
        const double scheduledMassRate = APOLLO_TRIM_DURATION_S > 0.0
            ? (APOLLO_INITIAL_MASS_KG - APOLLO_TARGET_FINAL_MASS_KG) /
                APOLLO_TRIM_DURATION_S
            : 0.0;
        const double trackingError =
            input.SpacecraftState.TotalMassKilograms - scheduledMass;
        const double requestedMassRate =
            Clamp(scheduledMassRate + trackingError / 10.0, 0.0, 1.0e9);
        const double pairMaximumMassRate = 2.0 * kMaximumThrustNewtons /
            (kSpecificImpulseSeconds * kStandardGravityMetersPerSecondSquared);
        const double throttle = requestedMassRate / pairMaximumMassRate;
        Command(input, output, "Balanced Vent Plus", throttle);
        Command(input, output, "Balanced Vent Minus", throttle);
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
