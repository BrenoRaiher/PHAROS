// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGSim/Integrators/IntegratorUtilities.h"

#include "TGSim/Vehicle/MassProperties.h"

#include <algorithm>

namespace tgsim
{
    void RestoreIntegratedStateInvariants(
        const SimulationConfig& config,
        SpacecraftState& state)
    {
        state.attitude_body_to_icrf = state.attitude_body_to_icrf.Normalized();

        for (const ComponentDefinition& component : config.vehicle.components)
        {
            if (component.variable_mass_state_index < state.variable_component_masses_kg.size())
            {
                state.variable_component_masses_kg[component.variable_mass_state_index] = std::max(
                    component.minimum_mass_kg,
                    state.variable_component_masses_kg[component.variable_mass_state_index]);
            }

        }

        if (!config.vehicle.components.empty())
            state.mass_kg = MassPropertiesModel().Compute(config.vehicle, state).mass_kg;
    }
}
