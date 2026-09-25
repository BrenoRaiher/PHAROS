
#include "StandaloneControllerAdapter.h"
#include "TGSim/Dynamics/General6DofDynamics.h"
#include "TGSim/Simulation/SimulationConfigBuilder.h"
#include "TGSim/Validation/BackendValidationSuite.h"
#include <cmath>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    using namespace tgsim;
    if (argc != 3) return 2;
    int passed = 0, failed = 0;
    auto check = [&](bool ok, const std::string& name) {
        std::cout << (ok ? "PASS " : "FAIL ") << name << '\n';
        ok ? ++passed : ++failed;
    };
    auto near = [](double a, double b) { return std::abs(a-b) < 2e-12; };
    std::string error;
    auto controller = StandaloneControllerAdapter::Create("CurrentFixture.dll", error);
    check(controller != nullptr, "Current contract accepted: " + error);
    if (!controller) return 1;
    for (const auto& path : {
        std::string("MismatchFixture.dll"), std::string(argv[1]), std::string(argv[2])}) {
        error.clear();
        auto bad = StandaloneControllerAdapter::Create(path, error);
        check(!bad && !error.empty(), "Incompatible DLL rejected: " + path + " | " + error);
    }

    SimulationRequest request;
    request.scenario_name = "DLL-to-CM verification";
    request.initial_state.variable_component_masses_kg = {5.0};
    ComponentDefinition hub;
    hub.name = "Hub"; hub.initial_mass_kg = hub.minimum_mass_kg = 10.0;
    hub.inertia_centroid_component_kgm2 = Mat3d::Diagonal({2.0, 2.0, 2.0});
    request.vehicle.components.push_back(hub);
    ComponentDefinition tank;
    tank.name = "Offset tank"; tank.initial_mass_kg = 5.0; tank.minimum_mass_kg = 1.0;
    tank.variable_mass_state_index = 0; tank.parent_component_index = 0;
    tank.inertia_centroid_component_kgm2 = Mat3d::Diagonal({1.0, 1.0, 1.0});
    tank.articulation_to_parent.parent_anchor_component_m = {3.0, 0.0, 0.0};
    request.vehicle.components.push_back(tank);
    ThrusterDefinition thruster;
    thruster.name = "Root nozzle"; thruster.component_index = 0;
    thruster.propellant_component_index = 1;
    thruster.direction_component = Vec3d::UnitX();
    thruster.mode = ThrusterMode::Commanded;
    thruster.maximum_thrust_n = 2.0 * kStandardGravityMps2;
    request.vehicle.thrusters.push_back(thruster);
    request.control.controller = controller;
    const auto config = SimulationConfigBuilder().Build(request);
    const General6DofDynamics dynamics(config);

    const std::vector<ComponentControlState> components;
    const std::vector<ThrusterControlState> thrusters;
    const std::vector<JointControlState> joints;
    const std::vector<ReactionWheelControlState> wheels;
    const std::vector<CelestialBodyControlState> bodies;
    // Nonmonotonic repetition checks that no previous stage's q-dot is reused.
    for (int mode : {0, 1, 2, 3, 4, 5, 6, 0, 1, 3, 2, 7, 0}) {
        const bool supplied = mode == 0 || mode == 2 || mode == 3;
        const double qdot = mode == 0 ? 0.2 : mode == 3 ? -0.2 : 0.0;
        const ControlInput input{double(mode), double(mode), config.initial_state,
            config, {}, Mat3d::Identity(), components, thrusters, joints, wheels, bodies};
        ControlCommandWriter writer(1, 0, 0);
        controller->ComputeControl(input, writer);
        const auto& command = writer.Command();
        const auto& rate = command.thruster_mass_flow_derivatives_kgps2[0];
        check(rate.has_value() == supplied && (!supplied || near(rate.value(), qdot)),
            "DLL optional-value/reset mode " + std::to_string(mode));
        check(near(command.thruster_throttles[0], mode == 7 ? 0.0 : 0.5),
            "DLL ordinary command mode " + std::to_string(mode));
        const auto e = dynamics.ComputeDynamics(double(mode), config.initial_state, config);
        // Independent geometric-CM identity for a fixed 10 kg hub and an
        // offset 5 kg tank: a_C = F/M - qdot*m_h*L/M^2 - 2*q^2*m_h*L/M^3.
        const double expected = mode == 7 ? 0.0 :
            kStandardGravityMps2 / 15.0 - qdot * 30.0 / 225.0 - 60.0 / 3375.0;
        check(near(e.derivative.velocity_rate_mps2.x, expected) &&
              near(e.derivative.velocity_rate_mps2.y, 0.0) &&
              near(e.derivative.velocity_rate_mps2.z, 0.0),
            "DLL rate reaches actual CM acceleration mode " + std::to_string(mode));
        check(near(e.derivative.mass_rate_kgps, mode == 7 ? 0.0 : -1.0),
            "Actual mass consumption preserved mode " + std::to_string(mode));
    }
    std::cout << "Controller integration: " << passed << " passed, " << failed << " failures.\n";
    return failed ? 1 : 0;
}
