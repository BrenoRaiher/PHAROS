// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/ComponentTree/TGComponentTreeValidationLibrary.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGInertialessComponentValidationTest,
    "PHAROS.ComponentTree.Validation.InertialessComponent",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGInertialessComponentValidationTest::RunTest(
    const FString& Parameters)
{
    (void)Parameters;

    FVector PrincipalMoments;
    FText ErrorText;
    const FTGSymmetricInertia ZeroInertia;

    TestTrue(
        TEXT("An inertialess component tensor is valid"),
        UTGComponentTreeValidationLibrary::ValidateSymmetricInertia(
            ZeroInertia,
            PrincipalMoments,
            ErrorText));
    TestEqual(TEXT("Zero I1"), PrincipalMoments.X, 0.0);
    TestEqual(TEXT("Zero I2"), PrincipalMoments.Y, 0.0);
    TestEqual(TEXT("Zero I3"), PrincipalMoments.Z, 0.0);
    TestTrue(TEXT("No error is reported"), ErrorText.IsEmpty());

    FTGSymmetricInertia NegativeInertia;
    NegativeInertia.IxxKilogramMetersSquared = -1.0;

    TestFalse(
        TEXT("A tensor with a negative principal moment is invalid"),
        UTGComponentTreeValidationLibrary::ValidateSymmetricInertia(
            NegativeInertia,
            PrincipalMoments,
            ErrorText));

    return true;
}

#endif
