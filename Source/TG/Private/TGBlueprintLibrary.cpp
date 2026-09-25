// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "TGBlueprintLibrary.h"
#include "SpiceBridge.h"

FString UTGBlueprintLibrary::HelloFromCPP()
{
    return TEXT("Hello from C++ inside Unreal");
}

double UTGBlueprintLibrary::AddTwoNumbers(double A, double B)
{
    return A + B;
}

FVector UTGBlueprintLibrary::TestVectorFromCPP()
{
    return FVector(1000.0, 2000.0, 3000.0);
}

//double UTGBlueprintLibrary::GetScenarioETFake()
//{
//    return 0.0; // placeholder only; later CSPICE ET
//}
//
//FVector UTGBlueprintLibrary::GetObserverICRFPositionFake()
//{
//    const FName Scenario = "EarthMoonVenusClose";
//    const double KmToCm = 1.0e5;
//
//    const FVector Earth_km(
//        -72494230.542965934,
//        -122904814.74042217,
//        -53258426.644901618
//    );
//
//    const FVector Moon_km = Earth_km + FVector(384400.0, 0.0, 0.0);
//    const FVector Venus_km = Earth_km + FVector(2000000.0, 800000.0, 200000.0);
//
//    if (Scenario == "NearEarth")
//    {
//        return KmToCm * (Earth_km + FVector(10000.0, 0.0, 0.0));
//    }
//
//    if (Scenario == "EarthMoon")
//    {
//        // Spacecraft between Earth and Moon, closer to Earth.
//        return KmToCm * (Earth_km + FVector(20000.0, 0.0, 0.0));
//    }
//
//    if (Scenario == "EarthMoonVenusClose")
//    {
//        // Artificial debug case: Earth + Moon + Venus all reasonably testable.
//        return KmToCm * (Earth_km + FVector(20000.0, 0.0, 0.0));
//    }
//
//    if (Scenario == "NearMoon")
//    {
//        return KmToCm * (Moon_km + FVector(10000.0, 0.0, 0.0));
//    }
//
//    if (Scenario == "NearVenus")
//    {
//        return KmToCm * (Venus_km + FVector(10000.0, 0.0, 0.0));
//    }
//
//    return KmToCm * (Earth_km + FVector(10000.0, 0.0, 0.0));
//}
//
//FVector UTGBlueprintLibrary::GetBodyICRFPositionFake(FName BodyName)
//{
//    const FName Scenario = "EarthMoonVenusClose";
//    const double KmToCm = 1.0e5;
//
//    const FVector Earth_km(
//        -72494230.542965934,
//        -122904814.74042217,
//        -53258426.644901618
//    );
//
//    const FVector Moon_km = Earth_km + FVector(384400.0, 0.0, 0.0);
//    const FVector Venus_km = Earth_km + FVector(2000000.0, 800000.0, 200000.0);
//
//    const FVector Sun_km(0.0, 0.0, 0.0);
//
//    const FVector Mars_km(
//        204900500.0,
//        42067830.0,
//        13797840.0
//    );
//
//    const FVector Pluto_km(
//        2933613397.7333899,
//        -3901088578.9745412,
//        -2101299475.9275265
//    );
//
//    if (BodyName == "Sun")
//    {
//        return KmToCm * Sun_km;
//    }
//
//    if (BodyName == "Earth")
//    {
//        return KmToCm * Earth_km;
//    }
//
//    if (BodyName == "Moon")
//    {
//        return KmToCm * Moon_km;
//    }
//
//    if (BodyName == "Venus")
//    {
//        if (Scenario == "EarthMoonVenusClose" || Scenario == "NearVenus")
//        {
//            return KmToCm * Venus_km;
//        }
//
//        return FVector(1.0e25, 1.0e25, 1.0e25);
//    }
//
//    if (BodyName == "Mars")
//    {
//        return KmToCm * Mars_km;
//    }
//
//    if (BodyName == "Pluto")
//    {
//        return KmToCm * Pluto_km;
//    }
//
//    // Hide bodies not implemented in this fake test.
//    return FVector(1.0e25, 1.0e25, 1.0e25);
//}
//
//FVector UTGBlueprintLibrary::GetRelativeBodyLocationUE(
//    FVector BodyICRF_cm,
//    FVector ObserverICRF_cm,
//    double MaxVisibleDistanceCm,
//    bool& bShouldBeVisible
//)
//{
//    const FVector RelativeICRF_cm = BodyICRF_cm - ObserverICRF_cm;
//
//    const FVector RelativeUE_cm(
//        RelativeICRF_cm.X,
//        -RelativeICRF_cm.Y,
//        RelativeICRF_cm.Z
//    );
//
//    bShouldBeVisible = RelativeUE_cm.Length() <= MaxVisibleDistanceCm;
//
//    return RelativeUE_cm;
//}

bool UTGBlueprintLibrary::LoadSPICEKernels(FString& OutMessage)
{
    return FSpiceBridge::LoadKernels(OutMessage);
}

bool UTGBlueprintLibrary::ConvertUTCToET(const FString& UTCString, double& OutET, FString& OutMessage)
{
    return FSpiceBridge::ConvertUTCToET(UTCString, OutET, OutMessage);
}

bool UTGBlueprintLibrary::GetBodyICRFPositionCm(
    const FString& BodyName,
    double ET,
    FVector& OutPositionCm,
    FString& OutMessage
)
{
    return FSpiceBridge::GetBodyICRFPositionCm(
        BodyName,
        ET,
        OutPositionCm,
        OutMessage
    );
}

bool UTGBlueprintLibrary::GetBodyRelativeLocationUECm(
    const FString& BodyName,
    double ET,
    const FVector& ObserverICRFCm,
    double MaxVisibleDistanceCm,
    FVector& OutRelativeLocationUECm,
    bool& bShouldBeVisible,
    FString& OutMessage
)
{
    return FSpiceBridge::GetBodyRelativeLocationUECm(
        BodyName,
        ET,
        ObserverICRFCm,
        MaxVisibleDistanceCm,
        OutRelativeLocationUECm,
        bShouldBeVisible,
        OutMessage
    );
}