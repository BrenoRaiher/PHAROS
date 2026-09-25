#include "PHAROSControllerAPI.h"
#include "EarthAttitudeRecoveryCommon.h"

#include <limits>
#include <new>

class TGUserController final
{
public:
    void Compute(const TGControlInput& input, TGControlOutput& output) const
    {
        earth_recovery::ComputeControl(true, input, output);
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


TG_CONTROLLER_EXPORT void* TG_CONTROLLER_CALL TG_CreateController(void)
{ return new (std::nothrow) TGUserController(); }
TG_CONTROLLER_EXPORT void TG_CONTROLLER_CALL TG_DestroyController(void* controller)
{ delete static_cast<TGUserController*>(controller); }
TG_CONTROLLER_EXPORT uint32_t TG_CONTROLLER_CALL TG_ComputeControl(
    void* controller, const TGControlInput* input, TGControlOutput* output)
{
    if (!controller || !input || !output ||
        input->StructSize != sizeof(TGControlInput) ||
        output->StructSize != sizeof(TGControlOutput))
        return TG_CONTROLLER_RESULT_INVALID_ARGUMENT;
    try
    {
        static_cast<TGUserController*>(controller)->Compute(*input, *output);
    }
    catch (...) { return TG_CONTROLLER_RESULT_USER_EXCEPTION; }
    return TG_CONTROLLER_RESULT_OK;
}
TG_CONTROLLER_EXPORT double TG_CONTROLLER_CALL
TG_NextControllerDiscontinuityElapsedTime(void*, double current)
{ return earth_recovery::NextDiscontinuity(current); }

