#include "PHAROSControllerAPI.h"

#include <limits>
#include <new>

/*
 * This class and its ComputeControl body belong entirely to the user. Private
 * helper functions and controller memory may also be declared in this file.
 */
class PHAROSUserController final
{
public:
    double NextDiscontinuityElapsedTime(
        double CurrentElapsedSimulationTimeSeconds) const
    {
        /*
         * Return the next known hard switch time here. For example, a command
         * using `ElapsedSimulationTimeSeconds < 10.0` should return 10.0 while
         * CurrentElapsedSimulationTimeSeconds is below 10.0. Continuous control
         * laws should keep the default positive-infinity result.
         */
        (void)CurrentElapsedSimulationTimeSeconds;
        return std::numeric_limits<double>::infinity();
    }

    void ComputeControl(
        const PHAROSControlInput& Input,
        PHAROSControlOutput& Output)
    {
        /*
         * Example command, intentionally disabled:
         *
         * if (Output.ThrusterCommandCount > 0)
         * {
         *     Output.ThrusterCommands[0].Throttle = 0.25;
         *     Output.ThrusterCommands[0].SpecificImpulseSeconds = 300.0;
         *     Output.ThrusterCommands[0].MassFlowDerivativeProvided = 1;
         *     Output.ThrusterCommands[0].
         *         MassFlowDerivativeKilogramsPerSecondSquared = AnalyticQDot;
         * }
         *
         * Inputs and outputs use SI units. Commands left untouched remain zero.
         * MassFlowDerivativeKilogramsPerSecondSquared is q_dot, where q is
         * the actual outward propellant discharge T/(g0*Isp) after command
         * limiting. It is not a throttle derivative or an additional thrust
         * command. Leave MassFlowDerivativeProvided equal to zero when no
         * analytic total derivative is available; PHAROS then omits only the
         * corresponding geometric-center-of-mass m_ddot correction.
         *
         * If the command depends on state, supply its total time derivative,
         * including state dependence. Compute every output from this current
         * Input; the host clears all command fields before every evaluation.
         */

        // Test payloads for the host's optional derivative transport.
        if (Output.ThrusterCommandCount == 0) return;
        auto& c = Output.ThrusterCommands[0];
        c.Throttle = 0.5;
        c.SpecificImpulseSeconds = 1.0;
        const int mode = static_cast<int>(Input.ElapsedSimulationTimeSeconds);
        switch (mode)
        {
        case 0: c.MassFlowDerivativeProvided = 1;
                c.MassFlowDerivativeKilogramsPerSecondSquared = 0.2; break;
        case 1: c.MassFlowDerivativeKilogramsPerSecondSquared = 999.0; break;
        case 2: c.MassFlowDerivativeProvided = 1; break;
        case 3: c.MassFlowDerivativeProvided = 1;
                c.MassFlowDerivativeKilogramsPerSecondSquared = -0.2; break;
        case 4: c.MassFlowDerivativeProvided = 1;
                c.MassFlowDerivativeKilogramsPerSecondSquared =
                    std::numeric_limits<double>::quiet_NaN(); break;
        case 5: c.MassFlowDerivativeProvided = 2;
                c.MassFlowDerivativeKilogramsPerSecondSquared = 0.2; break;
        case 6: c.MassFlowDerivativeProvided = 1;
                c.MassFlowDerivativeKilogramsPerSecondSquared =
                    std::numeric_limits<double>::infinity(); break;
        case 7: throw 1;
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

PHAROS_CONTROLLER_EXPORT void* PHAROS_CONTROLLER_CALL
PHAROS_CreateController(void)
{
    return new (std::nothrow) PHAROSUserController();
}

PHAROS_CONTROLLER_EXPORT void PHAROS_CONTROLLER_CALL
PHAROS_DestroyController(void* Controller)
{
    delete static_cast<PHAROSUserController*>(Controller);
}

PHAROS_CONTROLLER_EXPORT uint32_t PHAROS_CONTROLLER_CALL
PHAROS_ComputeControl(
    void* Controller,
    const PHAROSControlInput* Input,
    PHAROSControlOutput* Output)
{
    if (Controller == nullptr || Input == nullptr || Output == nullptr)
    {
        return PHAROS_CONTROLLER_RESULT_INVALID_ARGUMENT;
    }

    if (Input->StructSize != sizeof(PHAROSControlInput) ||
        Output->StructSize != sizeof(PHAROSControlOutput))
    {
        return PHAROS_CONTROLLER_RESULT_INVALID_ARGUMENT;
    }

    try
    {
        static_cast<PHAROSUserController*>(Controller)->ComputeControl(
            *Input,
            *Output);
    }
    catch (...)
    {
        return PHAROS_CONTROLLER_RESULT_USER_EXCEPTION;
    }

    return PHAROS_CONTROLLER_RESULT_OK;
}

PHAROS_CONTROLLER_EXPORT double PHAROS_CONTROLLER_CALL
PHAROS_NextControllerDiscontinuityElapsedTime(
    void* Controller,
    double CurrentElapsedSimulationTimeSeconds)
{
    if (Controller == nullptr)
        return std::numeric_limits<double>::infinity();

    try
    {
        return static_cast<PHAROSUserController*>(Controller)->
            NextDiscontinuityElapsedTime(
                CurrentElapsedSimulationTimeSeconds);
    }
    catch (...)
    {
        return std::numeric_limits<double>::infinity();
    }
}
