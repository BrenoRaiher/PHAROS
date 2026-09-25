// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "TGSim/Environment/IEphemerisProvider.h"

/// Production TGSimCore ephemeris provider backed by the project's CSPICE kernels.
/// The pure C++ backend owns only the interface; this Unreal-side class owns the
/// FString/FVector conversion and the CSPICE dependency.
class TG_API FTGSpiceEphemerisProvider final : public tgsim::IEphemerisProvider
{
public:
    bool TryGetBodyState(
        const std::string& BodyName,
        double EphemerisTimeTdbSeconds,
        tgsim::BodyState& BodyState) const override;

    bool TryGetBodyFixedToIcrf(
        const std::string& BodyName,
        double EphemerisTimeTdbSeconds,
        tgsim::Mat3d& BodyFixedToIcrf) const override;

    bool TryGetBodyAngularVelocityIcrf(
        const std::string& BodyName,
        double EphemerisTimeTdbSeconds,
        tgsim::Vec3d& AngularVelocityIcrfRadps) const override;

    bool TryGetBodyGravityMetadata(
        const std::string& BodyName,
        tgsim::BodyGravityMetadata& Metadata) const override;
};
