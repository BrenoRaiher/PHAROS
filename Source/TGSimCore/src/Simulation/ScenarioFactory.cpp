// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Simulation/ScenarioFactory.h"

// Selects and owns the concrete ODE model and integrator used by SimulationEngine.

#include "TGSim/Dynamics/General6DofDynamics.h"
#include "TGSim/Integrators/AdaptiveDormandPrince54.h"
#include "TGSim/Integrators/FixedStepRK4.h"

namespace tgsim
{
    ScenarioModules ScenarioFactory::CreateModules(const SimulationConfig& config) const
    {
        // Called once by SimulationEngine after configuration validation. It converts enum
        // selections into owned polymorphic modules used throughout the propagation loop;
        // it does not evaluate dynamics or take an integration step itself.
        ScenarioModules modules;
        // The current public SimulationKind has only the coupled spacecraft implementation.
        modules.dynamics_model = std::make_unique<General6DofDynamics>(config);
        // Instantiate the requested numerical integration algorithm.
        if (config.solver.integrator_kind == IntegratorKind::FixedStepRK4)
            modules.integrator = std::make_unique<FixedStepRK4>();
        else if (config.solver.integrator_kind == IntegratorKind::AdaptiveDormandPrince54)
            modules.integrator = std::make_unique<AdaptiveDormandPrince54>();
        return modules;
    }
}
