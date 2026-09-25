// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"

#include <memory>

namespace tgsim
{
    class IDynamicsModel;
    class IIntegrator;

    struct ScenarioModules
    {
        std::unique_ptr<IDynamicsModel> dynamics_model;
        std::unique_ptr<IIntegrator> integrator;
    };

    /// Creates the concrete dynamics model and integrator selected by a configuration.
    class ScenarioFactory
    {
    public:
        /// Returns owned dynamics/integrator instances. A missing selection is returned as nullptr.
        ScenarioModules CreateModules(const SimulationConfig& config) const;
    };
}
