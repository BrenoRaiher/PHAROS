// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "TGSim/Environment/IEphemerisProvider.h"

#include <filesystem>
#include <string>

class StandaloneSpiceProvider final : public tgsim::IEphemerisProvider
{
public:
    bool LoadKernels(
        const std::filesystem::path& kernel_directory,
        std::string& error);

    bool ConvertUtcToEphemerisTime(
        const std::string& utc,
        double& ephemeris_time_tdb_seconds,
        std::string& error) const;

    bool TryGetBodyState(
        const std::string& body_name,
        double ephemeris_time_tdb_seconds,
        tgsim::BodyState& body_state) const override;

    bool TryGetBodyFixedToIcrf(
        const std::string& body_name,
        double ephemeris_time_tdb_seconds,
        tgsim::Mat3d& body_fixed_to_icrf) const override;

    bool TryGetBodyAngularVelocityIcrf(
        const std::string& body_name,
        double ephemeris_time_tdb_seconds,
        tgsim::Vec3d& angular_velocity_icrf_radps) const override;

    bool TryGetBodyGravityMetadata(
        const std::string& body_name,
        tgsim::BodyGravityMetadata& metadata) const override;

private:
    bool CheckSucceeded(const std::string& operation, std::string& error) const;
    bool TryResolveBodyFixedFrameName(
        const std::string& body_name,
        std::string& frame_name) const;
    bool loaded_ = false;
};
