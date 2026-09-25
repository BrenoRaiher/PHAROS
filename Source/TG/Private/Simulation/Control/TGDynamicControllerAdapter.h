// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "TGControllerAPI.h"
#include "TGSim/Control/IController.h"

#include <memory>
#include <vector>

/*
 * Owns one loaded user-controller DLL and adapts its stable C ABI to the native
 * TGSimCore IController interface. One adapter instance is created per run, so
 * private state inside a user controller is never shared between simulations.
 */
class FTGDynamicControllerAdapter final : public tgsim::IController
{
public:
    ~FTGDynamicControllerAdapter() override;

    static bool ProbeDll(
        const FString& DllPath,
        FString& OutError);

    static std::shared_ptr<FTGDynamicControllerAdapter> Create(
        const FString& DllPath,
        FString& OutError);

    static bool IsAnyDllLoadedBelow(
        const FString& DirectoryPath);

    void ComputeControl(
        const tgsim::ControlInput& Input,
        tgsim::ControlCommandWriter& Output) const override;

    double NextDiscontinuityElapsedTime(
        double CurrentElapsedTimeSeconds) const override;

private:
    FTGDynamicControllerAdapter() = default;

    bool Load(const FString& DllPath, FString& OutError);
    void Unload();

    void* DllHandle = nullptr;
    void* ControllerInstance = nullptr;
    TGDestroyControllerFunction DestroyController = nullptr;
    TGComputeControlFunction ComputeController = nullptr;
    TGNextControllerDiscontinuityFunction NextDiscontinuity = nullptr;
    FString LoadedDllPath;
    bool bRegisteredAsLoaded = false;

    /* Reused every ODE call to avoid repeated heap allocation after warm-up. */
    mutable std::vector<TGComponentStateView> ComponentViews;
    mutable std::vector<TGThrusterStateView> ThrusterViews;
    mutable std::vector<TGJointStateView> JointViews;
    mutable std::vector<TGReactionWheelStateView> ReactionWheelViews;
    mutable std::vector<TGCelestialBodyStateView> CelestialBodyViews;
    mutable std::vector<TGThrusterCommand> ThrusterCommands;
    mutable std::vector<double> JointEfforts;
    mutable std::vector<double> ReactionWheelMomentumRates;
    mutable bool bRuntimeFailureLogged = false;
    mutable bool bDerivativeFailureLogged = false;
};
