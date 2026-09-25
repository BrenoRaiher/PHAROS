#include "PHAROSControllerAPI.h"

#include <cstring>
#include <limits>
#include <new>

namespace
{
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
        if (current < 1.0)
            return 1.0;
        if (current < 11.0)
            return 11.0;
        return std::numeric_limits<double>::infinity();
    }

    void ComputeControl(
        const TGControlInput& input,
        TGControlOutput& output) const
    {
        const double time = input.ElapsedSimulationTimeSeconds;

        for (uint64_t index = 0;
             index < input.JointCount && index < output.JointEffortCount;
             ++index)
        {
            const TGJointStateView& joint = input.Joints[index];
            if (Equals(joint.Name, "Hinge"))
                output.JointEffortsNewtonMetersOrNewtons[index] = 0.4;
            else if (Equals(joint.Name, "Slider"))
                output.JointEffortsNewtonMetersOrNewtons[index] = 1.6;
            else if (Equals(joint.Name, "Nested Hinge"))
                output.JointEffortsNewtonMetersOrNewtons[index] =
                    -1.5 * joint.Coordinate;
            else if (Equals(joint.Name, "Nested Slider"))
                output.JointEffortsNewtonMetersOrNewtons[index] =
                    -4.0 * joint.Coordinate;
        }

        for (uint64_t index = 0;
             index < input.ThrusterCount &&
             index < output.ThrusterCommandCount;
             ++index)
        {
            const TGThrusterStateView& thruster = input.Thrusters[index];
            if (Equals(thruster.Name, "Commanded Main Thruster") &&
                thruster.Mode == TG_THRUSTER_COMMANDED &&
                thruster.FiringWindowOpen != 0 &&
                thruster.PropellantAvailable != 0 &&
                time >= 1.0 && time < 11.0)
            {
                output.ThrusterCommands[index].Throttle = 0.40;
                output.ThrusterCommands[index].SpecificImpulseSeconds = 250.0;
            }
        }

        for (uint64_t index = 0;
             index < input.ReactionWheelCount &&
             index < output.ReactionWheelMomentumRateCount;
             ++index)
        {
            if (!Equals(input.ReactionWheels[index].Name, "Z Wheel"))
                continue;

            output.ReactionWheelMomentumRatesNewtonMeters[index] = 0.01;
        }

        // This scenario has no other actuator channels, so the direct-torque
        // case can be selected without private controller state.
        if (input.ThrusterCount == 0 && input.JointCount == 0 &&
            input.ReactionWheelCount == 0 && time < 1.0)
        {
            output.AdditionalExternalTorqueBodyNewtonMeters =
                {1.0, 0.0, 0.0};
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
