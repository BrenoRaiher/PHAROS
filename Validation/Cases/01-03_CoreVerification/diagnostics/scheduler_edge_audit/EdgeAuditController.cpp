#include "PHAROSControllerAPI.h"

#include <cstring>
#include <limits>
#include <new>

namespace
{
    constexpr double kStart = 1000000000.0000001;
    constexpr double kStop = 1000000000.0000002;

    bool Equals(const TGStringView view, const char* text)
    {
        const size_t length = std::strlen(text);
        return view.Length == static_cast<uint64_t>(length) &&
            (length == 0 ||
             (view.Data != nullptr &&
              std::memcmp(view.Data, text, length) == 0));
    }
}

class TGUserController final
{
public:
    double NextDiscontinuityElapsedTime(const double current) const
    {
        if (current < kStart)
            return kStart;
        if (current < kStop)
            return kStop;
        return std::numeric_limits<double>::infinity();
    }

    void ComputeControl(
        const TGControlInput& input,
        TGControlOutput& output) const
    {
        const double time = input.ElapsedSimulationTimeSeconds;
        for (uint64_t index = 0;
             index < input.ThrusterCount &&
             index < output.ThrusterCommandCount;
             ++index)
        {
            if (Equals(input.Thrusters[index].Name,
                       "Edge Commanded Thruster") &&
                time >= kStart && time < kStop)
            {
                output.ThrusterCommands[index].Throttle = 1.0;
                output.ThrusterCommands[index].SpecificImpulseSeconds = 1000.0;
            }
        }

        for (uint64_t index = 0;
             index < input.ReactionWheelCount &&
             index < output.ReactionWheelMomentumRateCount;
             ++index)
        {
            if (Equals(input.ReactionWheels[index].Name, "Z Wheel"))
                output.ReactionWheelMomentumRatesNewtonMeters[index] = 0.01;
        }
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
    if (
        input->StructSize != sizeof(TGControlInput) ||
        output->StructSize != sizeof(TGControlOutput))
    {
        return TG_CONTROLLER_RESULT_INVALID_ARGUMENT;
    }
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
    double current_elapsed_simulation_time_seconds)
{
    if (controller == nullptr)
        return std::numeric_limits<double>::infinity();
    try
    {
        return static_cast<TGUserController*>(controller)->
            NextDiscontinuityElapsedTime(
                current_elapsed_simulation_time_seconds);
    }
    catch (...)
    {
        return std::numeric_limits<double>::infinity();
    }
}
