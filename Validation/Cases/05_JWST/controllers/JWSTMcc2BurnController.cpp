#include "MissionControlCommon.h"

#include <limits>
#include <new>

class TGUserController final
{
public:
    double NextDiscontinuityElapsedTime(double) const
    { return std::numeric_limits<double>::infinity(); }

    void ComputeControl(const TGControlInput& input, TGControlOutput& output)
    {
        const TGVec3 deltaV = jwst_control::Mcc2DeltaVVector();
        const TGVec3 direction = jwst_control::Unit(
            deltaV, {1.0, 0.0, 0.0});
        jwst_control::ApplyAttitudePd(
            input, output, jwst_control::PointBodyXAt(input, direction));
        jwst_control::CommandScatBurn(
            input, output, jwst_control::Norm(deltaV));
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
        static_cast<TGUserController*>(controller)->ComputeControl(*input, *output);
    }
    catch (...) { return TG_CONTROLLER_RESULT_USER_EXCEPTION; }
    return TG_CONTROLLER_RESULT_OK;
}
TG_CONTROLLER_EXPORT double TG_CONTROLLER_CALL
TG_NextControllerDiscontinuityElapsedTime(void* controller, double current)
{
    return controller
        ? static_cast<TGUserController*>(controller)->NextDiscontinuityElapsedTime(current)
        : std::numeric_limits<double>::infinity();
}
