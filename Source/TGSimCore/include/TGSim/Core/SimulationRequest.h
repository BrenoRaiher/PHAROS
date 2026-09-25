// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

// Public input contract passed to SimulationEngine::Run().

#include "TGSim/Core/ModelTypes.h"

#include <limits>
#include <string>

namespace tgsim
{
    enum class SimulationKind
    {
        Spacecraft6Dof
    };

    enum class IntegratorKind
    {
        FixedStepRK4,
        AdaptiveDormandPrince54
    };

    enum class OutputMode
    {
        EveryIntegratorStep,
        FixedInterval
    };

    enum class MassFlowConvention
    {
        // Standard rocket convention: configured thrust already includes exhaust momentum flux.
        ThrustIncludesExhaustMomentum,
        // Literal derivative of m*v: adds -(m_dot/m)*v to acceleration.
        MomentumDerivative
    };

    /// All caller-provided inputs required to define and run one simulation.
    /// No HUD or Unreal object is needed; values are supplied directly in SI units.
    struct SimulationRequest
    {
        std::string scenario_name = "Untitled Scenario";
        SimulationKind simulation_kind = SimulationKind::Spacecraft6Dof;
        IntegratorKind integrator_kind = IntegratorKind::FixedStepRK4;
        OutputMode output_mode = OutputMode::EveryIntegratorStep;
        MassFlowConvention mass_flow_convention = MassFlowConvention::ThrustIncludesExhaustMomentum;

        SpacecraftState initial_state;
        VehicleSettings vehicle;
        GravitySettings gravity;
        SolarRadiationSettings solar_radiation;
        AtmosphereSettings atmosphere;
        AerodynamicsSettings aerodynamics;
        ControlSettings control;

        // SPICE ET: TDB seconds past the J2000 epoch. UTC is a presentation/input
        // conversion performed by the Unreal/SPICE adapter, never the ODE time scale.
        double start_ephemeris_time_tdb_seconds = 0.0;
        double final_ephemeris_time_tdb_seconds = 0.0;
        // Optional elapsed duration retained independently from the absolute ET
        // endpoints. Scenario compilers should set this for duration-authored runs
        // because (start ET + a short duration) can lose low bits at large epochs.
        // NaN keeps the direct-request behavior of deriving duration as final-start.
        double requested_duration_seconds =
            std::numeric_limits<double>::quiet_NaN();
        double maximum_integrator_step_seconds = 1.0;
        double initial_integrator_step_seconds = 0.1; // Adaptive method initial guess.
        double absolute_tolerance = 1.0e-9;
        double relative_tolerance = 1.0e-9;
        double output_step_seconds = 1.0; // Used only by FixedInterval output mode.
        std::size_t maximum_integration_steps = 10000000;
        // Prevents an accidentally dense HUD request from exhausting memory while
        // both typed telemetry and the numeric solution table are retained.
        std::size_t maximum_output_samples = 1000000;
    };
}
