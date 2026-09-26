// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if defined(TGSIM_STANDALONE_SCENARIO_TEST)

#include "TGSim/Scenario/CelestialCatalog.h"
#include "TGSim/Scenario/ScenarioCompiler.h"
#include "TGSim/Scenario/ScenarioFile.h"
#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <iostream>

namespace
{
    tgsim::scenario::ScenarioDocument MakeDocument()
    {
        using namespace tgsim::scenario;
        ScenarioDocument document;
        document.generator = "TGScenarioRoundTripTest";
        document.scenario.name = "Round trip";
        document.scenario.start_utc = "2030-01-01T00:00:00Z";
        document.scenario.end_mode = EndMode::Duration;
        document.scenario.duration_seconds = 10.0;
        document.scenario.maximum_integrator_step_seconds = 0.5;
        document.scenario.maximum_wall_clock_runtime_seconds = 42.5;
        document.initial_state.authoring_frame = "Earth";

        Component child;
        child.id = "22222222-2222-2222-2222-222222222222";
        child.name = "Panel";
        child.parent_component_name = "Bus";
        child.initial_mass_kg = 0.0;
        child.minimum_mass_kg = 0.0;
        child.centroidal_inertia = {1.0, 1.0, 1.0, 0.0, 0.0, 0.0};
        child.parent_anchor_m = {1.0, 0.0, 0.0};
        JointDof hinge;
        hinge.id = "33333333-3333-3333-3333-333333333333";
        hinge.name = "Panel hinge";
        hinge.axis = tgsim::Vec3d::UnitY();
        hinge.initial_coordinate = 0.52359877559829887308;
        hinge.initial_rate = 0.26179938779914943654;
        hinge.minimum_coordinate = -1.57079632679489661923;
        hinge.maximum_coordinate = 1.57079632679489661923;
        hinge.maximum_absolute_rate = 0.78539816339744830962;
        hinge.maximum_absolute_effort = 0.5;
        child.degrees_of_freedom.push_back(hinge);
        child.visual.geometry_source = GeometrySource::CustomStl;
        child.visual.stl_file_path = "assets/panel.stl";
        child.visual.surface_appearance = SurfaceAppearance::Textured;
        child.visual.base_color_texture_file_path = "assets/panel_albedo.png";
        child.visual.display_color = {0.1, 0.2, 0.3, 0.4};
        child.srp.proxy_resolution = SrpProxyResolution::Custom;
        child.srp.custom_target_triangle_count = 32;
        child.srp.generated_geometry_signature = "panel-cache-v1";
        child.srp.logical_region_overrides.push_back(
            {SrpLogicalRegion::PositiveX, {0.2, 0.3, 0.5}});
        child.srp.triangle_overrides.push_back({7, {0.4, 0.1, 0.5}});

        Component bus;
        bus.id = "11111111-1111-1111-1111-111111111111";
        bus.name = "Bus";
        bus.initial_mass_kg = 100.0;
        bus.minimum_mass_kg = 20.0;
        bus.variable_mass = true;
        bus.centroidal_inertia = {10.0, 11.0, 12.0, 0.1, 0.2, 0.3};

        // Child first is intentional: ScenarioCompiler must topologically sort it.
        document.components = {child, bus};

        Thruster thruster;
        thruster.name = "Main engine";
        thruster.mount_component_name = "Bus";
        thruster.propellant_component_name = "Bus";
        thruster.ignition_time_mode = ThrusterTimeMode::Elapsed;
        thruster.ignition_elapsed_seconds = 1.0e-7;
        thruster.never_shuts_down = false;
        thruster.shutdown_time_mode = ThrusterTimeMode::Elapsed;
        thruster.shutdown_elapsed_seconds = 2.0e-7;
        thruster.prescribed_thrust.constant_value = 10.0;
        thruster.prescribed_specific_impulse.constant_value = 250.0;
        document.thrusters.push_back(thruster);

        ReactionWheel wheel;
        wheel.name = "Pitch wheel";
        wheel.mount_component_name = "Bus";
        wheel.axis_component = tgsim::Vec3d::UnitY();
        wheel.initial_momentum_nms = 0.2;
        wheel.maximum_absolute_momentum_nms = 2.0;
        document.reaction_wheels.push_back(wheel);

        document.control.unreal_controller_id = "managed-controller-id";
        document.control.controller_dll_path = "controllers/controller.dll";

        CelestialBody earth;
        earth.catalog_key = "Earth";
        earth.gravity_enabled = false;
        earth.harmonic_model_csv_file_path = "gravity/earth.csv";
        earth.maximum_harmonic_degree = 8;
        document.celestial_bodies.push_back(earth);

        document.solar_radiation_pressure.enabled = false;
        document.solar_radiation_pressure.occulting_body_names = {"Earth", "Moon"};
        OpticalFacet facet;
        facet.name = "Panel triangle";
        facet.component_id = child.id;
        facet.component_name = child.name;
        facet.stable_triangle_index = 7;
        facet.vertex0_component_m = {0.0, 0.0, 0.0};
        facet.vertex1_component_m = {1.0, 0.0, 0.0};
        facet.vertex2_component_m = {0.0, 1.0, 0.0};
        facet.optical_properties = {0.2, 0.3, 0.5};
        document.solar_radiation_pressure.optical_facets.push_back(facet);

        document.atmosphere.enabled = false;
        document.atmosphere.general_profile_csv_path = "environment/atmosphere.csv";
        document.atmosphere.chp_coefficient_csv_path = "environment/chp.csv";

        document.aerodynamics.enabled = false;
        document.aerodynamics.reference_area_m2 = 4.0;
        document.aerodynamics.database.enabled = true;
        document.aerodynamics.database.csv_file_path = "aero/database.csv";
        document.aerodynamics.database.maximum_normalized_neighbor_distance = 2.5;
        AerodynamicDatabaseRow aerodynamic_row;
        aerodynamic_row.speed_ratio = 4.0;
        aerodynamic_row.knudsen_number = 10.0;
        aerodynamic_row.articulation_coordinates = {0.25};
        aerodynamic_row.force_coefficients_body = {1.0, 2.0, 3.0};
        aerodynamic_row.moment_coefficients_body = {4.0, 5.0, 6.0};
        document.aerodynamics.database.rows.push_back(aerodynamic_row);
        return document;
    }
}

int main()
{
    using namespace tgsim::scenario;

    assert(GetCelestialCatalog().size() == 44);
    assert(FindCelestialCatalogEntry("earth") != nullptr);
    assert(FindCelestialCatalogEntry("phoebe") != nullptr);
    assert(FindCelestialCatalogEntry("not-a-body") == nullptr);

    const ScenarioDocument source = MakeDocument();
    const std::string text = SerializeScenarioText(source);
    assert(text.find("TGSCN") != std::string::npos);
    assert(text.find("initial_coordinate = 30") != std::string::npos);
    assert(text.find("minimum_coordinate = -90") != std::string::npos);
    assert(text.find("maximum_coordinate = 90") != std::string::npos);
    assert(text.find("assets/panel.stl") != std::string::npos);
    assert(text.find("authoring_frame") != std::string::npos);
    assert(text.find("scale_inertia_with_mass") == std::string::npos);

    ScenarioDocument parsed;
    Diagnostics diagnostics;
    assert(ParseScenarioText(text, parsed, diagnostics));
    assert(!HasErrors(diagnostics));
    assert(parsed.components.size() == 2);
    assert(parsed.components[0].initial_mass_kg == 0.0);
    assert(parsed.components[0].minimum_mass_kg == 0.0);
    assert(parsed.scenario.maximum_wall_clock_runtime_seconds == 42.5);
    assert(parsed.initial_state.authoring_frame == "Earth");
    assert(parsed.components[0].degrees_of_freedom.size() == 1);
    assert(std::abs(
        parsed.components[0].degrees_of_freedom[0].initial_coordinate -
        0.52359877559829887308) < 1.0e-12);
    assert(std::abs(
        parsed.components[0].degrees_of_freedom[0].initial_rate -
        0.26179938779914943654) < 1.0e-12);
    assert(std::abs(
        *parsed.components[0].degrees_of_freedom[0].minimum_coordinate +
        1.57079632679489661923) < 1.0e-12);
    assert(std::abs(
        *parsed.components[0].degrees_of_freedom[0].maximum_coordinate -
        1.57079632679489661923) < 1.0e-12);
    assert(std::abs(
        parsed.components[0].degrees_of_freedom[0].maximum_absolute_rate -
        0.78539816339744830962) < 1.0e-12);
    assert(parsed.components[0].visual.stl_file_path == "assets/panel.stl");
    assert(parsed.components[0].visual.base_color_texture_file_path ==
        "assets/panel_albedo.png");
    assert(parsed.components[0].srp.logical_region_overrides.size() == 1);
    assert(parsed.components[0].srp.triangle_overrides.size() == 1);
    assert(parsed.thrusters.size() == 1);
    assert(parsed.reaction_wheels.size() == 1);
    assert(parsed.control.unreal_controller_id == "managed-controller-id");
    assert(parsed.control.controller_dll_path == "controllers/controller.dll");
    assert(parsed.celestial_bodies.size() == 1);
    assert(parsed.celestial_bodies[0].maximum_harmonic_degree == 8);
    assert(parsed.solar_radiation_pressure.optical_facets.size() == 1);
    assert(parsed.atmosphere.general_profile_csv_path ==
        "environment/atmosphere.csv");
    assert(parsed.aerodynamics.database.rows.size() == 1);
    assert(parsed.aerodynamics.database.maximum_normalized_neighbor_distance == 2.5);

    const std::filesystem::path resolved = ResolveReferencedPath(
        std::filesystem::path("C:/mission/cases/case.tgscn"),
        "../assets/panel.stl");
    assert(resolved == std::filesystem::path("C:/mission/assets/panel.stl"));

    ScenarioCompilerServices services;
    services.utc_to_ephemeris_time = [](
        const std::string&, double& ephemeris_time, std::string&)
    {
        ephemeris_time = 0.0;
        return true;
    };
    tgsim::SimulationRequest request;
    parsed.celestial_bodies.clear();
    diagnostics.clear();
    assert(CompileScenario(
        parsed, std::filesystem::path("C:/mission/case.tgscn"),
        services, request, diagnostics));
    assert(!HasErrors(diagnostics));
    assert(request.vehicle.components.size() == 2);
    assert(request.vehicle.components[0].name == "Bus");
    assert(request.vehicle.components[1].name == "Panel");
    assert(request.vehicle.components[1].initial_mass_kg == 0.0);
    assert(request.vehicle.components[1].minimum_mass_kg == 0.0);
    assert(request.vehicle.components[1].parent_component_index == 0);
    assert(request.vehicle.components[1].articulation_to_parent.dofs.size() == 1);
    assert(std::abs(
        request.vehicle.components[1].articulation_to_parent.dofs[0].
            initial_coordinate -
        0.52359877559829887308) < 1.0e-12);
    assert(std::abs(
        request.vehicle.components[1].articulation_to_parent.dofs[0].
            initial_rate -
        0.26179938779914943654) < 1.0e-12);
    assert(request.vehicle.components[0].variable_mass_state_index == 0);
    assert(request.vehicle.thrusters[0].component_index == 0);
    assert(request.vehicle.thrusters[0].propellant_component_index == 0);
    assert(request.vehicle.thrusters[0].ignition_elapsed_time_seconds ==
        1.0e-7);
    assert(request.vehicle.thrusters[0].shutdown_elapsed_time_seconds ==
        2.0e-7);
    assert(request.requested_duration_seconds ==
        parsed.scenario.duration_seconds);

    ScenarioDocument zero_floor = parsed;
    zero_floor.components[1].minimum_mass_kg = 0.0;
    diagnostics.clear();
    assert(!CompileScenario(
        zero_floor, std::filesystem::path("C:/mission/case.tgscn"),
        services, request, diagnostics));
    assert(std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [](const Diagnostic& diagnostic)
        {
            return diagnostic.code == "SCN-SPACECRAFT-MINIMUM-MASS";
        }));

    ScenarioDocument all_massless = zero_floor;
    all_massless.components[1].initial_mass_kg = 0.0;
    diagnostics.clear();
    assert(!CompileScenario(
        all_massless, std::filesystem::path("C:/mission/case.tgscn"),
        services, request, diagnostics));
    assert(std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [](const Diagnostic& diagnostic)
        {
            return diagnostic.code == "SCN-SPACECRAFT-INITIAL-MASS";
        }));

    // Variable mass is now the sole inertia-scaling switch.
    tgsim::VehicleSettings scaling_vehicle;
    tgsim::ComponentDefinition scaling_component;
    scaling_component.name = "Scaling tank";
    scaling_component.initial_mass_kg = 100.0;
    scaling_component.minimum_mass_kg = 20.0;
    scaling_component.variable_mass_state_index = 0;
    scaling_component.inertia_centroid_component_kgm2 =
        tgsim::Mat3d::Diagonal({10.0, 20.0, 30.0});
    scaling_vehicle.components.push_back(scaling_component);
    tgsim::SpacecraftState scaling_state;
    scaling_state.variable_component_masses_kg = {50.0};
    const tgsim::MassProperties scaled =
        tgsim::MassPropertiesModel().Compute(scaling_vehicle, scaling_state);
    assert(std::abs(scaled.inertia_body_kgm2.m[0][0] - 5.0) < 1.0e-12);
    assert(std::abs(scaled.inertia_body_kgm2.m[1][1] - 10.0) < 1.0e-12);
    assert(std::abs(scaled.inertia_body_kgm2.m[2][2] - 15.0) < 1.0e-12);

    // Unsupported schema fields are rejected.
    std::string unsupported_text = text;
    const std::size_t field_marker = unsupported_text.find("variable_mass = false");
    assert(field_marker != std::string::npos);
    unsupported_text.insert(
        field_marker + std::string("variable_mass = false").size(),
        "\nscale_inertia_with_mass = false");
    ScenarioDocument unsupported_document;
    diagnostics.clear();
    assert(!ParseScenarioText(unsupported_text, unsupported_document, diagnostics));
    assert(HasErrors(diagnostics));
    assert(std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [](const Diagnostic& diagnostic)
        {
            return diagnostic.code == "TGSCN-UNKNOWN-KEY";
        }));

    ScenarioDocument malformed;
    diagnostics.clear();
    assert(!ParseScenarioText("format = [", malformed, diagnostics));
    assert(HasErrors(diagnostics));
    assert(!diagnostics.front().message.empty());

    std::string misspelled = text;
    misspelled.insert(
        0,
        "unexpected_header = true # intentional unknown key\n");
    diagnostics.clear();
    assert(!ParseScenarioText(misspelled, malformed, diagnostics));
    assert(std::any_of(
        diagnostics.begin(), diagnostics.end(),
        [](const Diagnostic& diagnostic)
        {
            return diagnostic.code == "TGSCN-UNKNOWN-KEY";
        }));

    std::cout << "TGSCN round-trip and compiler tests passed.\n";
    return 0;
}

#endif
