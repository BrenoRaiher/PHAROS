// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "TGSim/Core/Types.h"

namespace FSpiceBridge
{
    bool LoadKernels(FString& OutMessage);

    bool ConvertUTCToET(const FString& UTCString, double& OutET, FString& OutMessage);

    bool ConvertETToUTC(double ET, FString& OutUTCString, FString& OutMessage);

    /// Returns the body's geometric J2000/ICRF barycentric state in backend SI units.
    bool GetBodyICRFStateSI(
        const FString& BodyName,
        double ET,
        FVector& OutPositionM,
        FVector& OutVelocityMps,
        FString& OutMessage
    );

    /// Returns the SPICE body-fixed-to-J2000 rotation matrix at ET.
    bool GetBodyFixedToICRF(
        const FString& BodyName,
        double ET,
        tgsim::Mat3d& OutBodyFixedToICRF,
        FString& OutMessage
    );

    /// Returns the body-fixed frame angular velocity relative to J2000,
    /// expressed in J2000/ICRF axes [rad/s].
    bool GetBodyAngularVelocityICRF(
        const FString& BodyName,
        double ET,
        tgsim::Vec3d& OutAngularVelocityIcrfRadps,
        FString& OutMessage
    );

    /// Reads SPICE GM and, for a physical body, its first PCK shape radius.
    /// These are not the constants belonging to an uploaded harmonic model.
    /// Barycenters return a valid GM and radius zero for point-mass use.
    bool GetBodyGravityMetadataSI(
        const FString& BodyName,
        double& OutGravitationalParameterM3ps2,
        double& OutReferenceRadiusM,
        FString& OutMessage
    );

    bool GetBodyICRFPositionCm(
        const FString& BodyName,
        double ET,
        FVector& OutPositionCm,
        FString& OutMessage
    );

    bool GetBodyRelativeLocationUECm(
        const FString& BodyName,
        double ET,
        const FVector& ObserverICRFCm,
        double MaxVisibleDistanceCm,
        FVector& OutRelativeLocationUECm,
        bool& bShouldBeVisible,
        FString& OutMessage
    );
}
