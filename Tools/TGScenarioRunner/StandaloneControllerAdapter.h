// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGControllerAPI.h"
#include "TGSim/Control/IController.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

/*
 * Loads a controller built against ControllerSDK/TGControllerAPI.h and
 * exposes it through TGSimCore's IController interface. This is deliberately
 * independent of Unreal so the command-line runner and packaged application
 * execute the same controller ABI.
 */
class StandaloneControllerAdapter final : public tgsim::IController
{
public:
    ~StandaloneControllerAdapter() override;

    static std::shared_ptr<StandaloneControllerAdapter> Create(
        const std::filesystem::path& dll_path,
        std::string& error);

    void ComputeControl(
        const tgsim::ControlInput& input,
        tgsim::ControlCommandWriter& output) const override;

    double NextDiscontinuityElapsedTime(
        double current_elapsed_time_seconds) const override;

private:
    StandaloneControllerAdapter() = default;

    bool Load(const std::filesystem::path& dll_path, std::string& error);
    void Unload();

    void* dll_handle_ = nullptr;
    void* controller_instance_ = nullptr;
    TGDestroyControllerFunction destroy_controller_ = nullptr;
    TGComputeControlFunction compute_controller_ = nullptr;
    TGNextControllerDiscontinuityFunction next_discontinuity_ = nullptr;
    std::filesystem::path loaded_dll_path_;

    // Reused at each ODE evaluation to avoid repeated allocation after warm-up.
    mutable std::vector<TGComponentStateView> component_views_;
    mutable std::vector<TGThrusterStateView> thruster_views_;
    mutable std::vector<TGJointStateView> joint_views_;
    mutable std::vector<TGReactionWheelStateView> reaction_wheel_views_;
    mutable std::vector<TGCelestialBodyStateView> celestial_body_views_;
    mutable std::vector<TGThrusterCommand> thruster_commands_;
    mutable std::vector<double> joint_efforts_;
    mutable std::vector<double> reaction_wheel_momentum_rates_;
    mutable bool runtime_failure_reported_ = false;
    mutable bool derivative_failure_reported_ = false;
};
