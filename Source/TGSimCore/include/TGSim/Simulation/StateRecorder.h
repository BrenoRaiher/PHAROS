// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Core/SimulationConfig.h"
#include "TGSim/Core/SimulationResult.h"

namespace tgsim
{
    /// Accumulates typed telemetry and the parallel AVS-style numeric solution matrix.
    class StateRecorder
    {
    public:
        /// Builds stable output-column names, including configured component/wheel states.
        explicit StateRecorder(const SimulationConfig& config);

        /// Appends one output instant: state, load diagnostics, CM, inertia, and numeric row.
        void Record(const SpacecraftState& state, const DynamicsEvaluation& evaluation);

        /// Copies all accumulated rows into the final SimulationResult.
        SimulationResult BuildResult(bool success, const std::string& message) const;

        std::size_t SampleCount() const { return samples_.size(); }

    private:
        GravitySettings gravity_;
        std::size_t thruster_count_ = 0;
        std::vector<TelemetrySample> samples_;
        std::vector<std::string> column_names_;
        std::vector<std::vector<double>> solution_array_;
    };
}
