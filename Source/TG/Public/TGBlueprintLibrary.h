// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TGBlueprintLibrary.generated.h"

UCLASS()
class TG_API UTGBlueprintLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Test")
    static FString HelloFromCPP();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Test")
    static double AddTwoNumbers(double A, double B);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Test")
    static FVector TestVectorFromCPP();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|SPICE")
    static bool LoadSPICEKernels(FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|SPICE")
    static bool ConvertUTCToET(const FString& UTCString, double& OutET, FString& OutMessage);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|SPICE")
    static bool GetBodyICRFPositionCm(
        const FString& BodyName,
        double ET,
        FVector& OutPositionCm,
        FString& OutMessage
    );

    UFUNCTION(BlueprintCallable, Category = "PHAROS|SPICE")
    static bool GetBodyRelativeLocationUECm(
        const FString& BodyName,
        double ET,
        const FVector& ObserverICRFCm,
        double MaxVisibleDistanceCm,
        FVector& OutRelativeLocationUECm,
        bool& bShouldBeVisible,
        FString& OutMessage
    );
};