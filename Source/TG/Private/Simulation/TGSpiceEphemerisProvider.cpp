// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGSpiceEphemerisProvider.h"

#include "SpiceBridge.h"

bool FTGSpiceEphemerisProvider::TryGetBodyState(
    const std::string& BodyName,
    double EphemerisTimeTdbSeconds,
    tgsim::BodyState& BodyState) const
{
    FVector PositionM;
    FVector VelocityMps;
    FString Message;
    if (!FSpiceBridge::GetBodyICRFStateSI(
            UTF8_TO_TCHAR(BodyName.c_str()),
            EphemerisTimeTdbSeconds,
            PositionM,
            VelocityMps,
            Message))
    {
        return false;
    }

    BodyState.position_icrf_m = {PositionM.X, PositionM.Y, PositionM.Z};
    BodyState.velocity_icrf_mps = {VelocityMps.X, VelocityMps.Y, VelocityMps.Z};
    return true;
}

bool FTGSpiceEphemerisProvider::TryGetBodyFixedToIcrf(
    const std::string& BodyName,
    double EphemerisTimeTdbSeconds,
    tgsim::Mat3d& BodyFixedToIcrf) const
{
    FString Message;
    return FSpiceBridge::GetBodyFixedToICRF(
        UTF8_TO_TCHAR(BodyName.c_str()),
        EphemerisTimeTdbSeconds,
        BodyFixedToIcrf,
        Message);
}

bool FTGSpiceEphemerisProvider::TryGetBodyAngularVelocityIcrf(
    const std::string& BodyName,
    double EphemerisTimeTdbSeconds,
    tgsim::Vec3d& AngularVelocityIcrfRadps) const
{
    FString Message;
    return FSpiceBridge::GetBodyAngularVelocityICRF(
        UTF8_TO_TCHAR(BodyName.c_str()),
        EphemerisTimeTdbSeconds,
        AngularVelocityIcrfRadps,
        Message);
}

bool FTGSpiceEphemerisProvider::TryGetBodyGravityMetadata(
    const std::string& BodyName,
    tgsim::BodyGravityMetadata& Metadata) const
{
    FString Message;
    return FSpiceBridge::GetBodyGravityMetadataSI(
        UTF8_TO_TCHAR(BodyName.c_str()),
        Metadata.gravitational_parameter_m3ps2,
        Metadata.reference_radius_m,
        Message);
}
