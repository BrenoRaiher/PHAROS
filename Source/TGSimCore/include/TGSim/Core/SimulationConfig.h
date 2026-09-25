// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Normalized internal configuration built from SimulationRequest before propagation.

#include "TGSim/Core/SimulationRequest.h"

namespace tgsim
{
    /// Time integration and result-sampling settings used by SimulationEngine.
    struct SolverSettings
    {
        IntegratorKind integrator_kind = IntegratorKind::FixedStepRK4;
        double start_ephemeris_time_tdb_seconds = 0.0;
        double final_ephemeris_time_tdb_seconds = 0.0;
        double requested_duration_seconds =
            std::numeric_limits<double>::quiet_NaN();
        double maximum_integrator_step_seconds = 1.0;
        double initial_integrator_step_seconds = 0.1;
        double absolute_tolerance = 1.0e-9;
        double relative_tolerance = 1.0e-9;
        double output_step_seconds = 1.0;
        OutputMode output_mode = OutputMode::EveryIntegratorStep;
        std::size_t maximum_integration_steps = 10000000;
        std::size_t maximum_output_samples = 1000000;
    };

    /// Immutable configuration shared by every dynamics and integrator evaluation in one run.
    struct SimulationConfig
    {
        std::string scenario_name;
        SimulationKind simulation_kind = SimulationKind::Spacecraft6Dof;
        MassFlowConvention mass_flow_convention = MassFlowConvention::ThrustIncludesExhaustMomentum;
        SolverSettings solver;
        SpacecraftState initial_state;
        VehicleSettings vehicle;
        GravitySettings gravity;
        SolarRadiationSettings solar_radiation;
        AtmosphereSettings atmosphere;
        AerodynamicsSettings aerodynamics;
        ControlSettings control;
    };
}
