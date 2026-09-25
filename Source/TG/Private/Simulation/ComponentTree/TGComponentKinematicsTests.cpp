// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Simulation/ComponentTree/TGComponentKinematicsLibrary.h"

namespace TGComponentKinematicsTestsPrivate
{
    constexpr double PositionTolerance = 1.0e-9;
    constexpr double CoordinateTolerance = 1.0e-12;
    constexpr double QuaternionTolerance = 1.0e-9;
    constexpr double HalfPi = 1.57079632679489661923;

    bool AreVectorsNearlyEqual(
        const FVector& First,
        const FVector& Second,
        double Tolerance = PositionTolerance)
    {
        return
            FMath::Abs(First.X - Second.X) <= Tolerance &&
            FMath::Abs(First.Y - Second.Y) <= Tolerance &&
            FMath::Abs(First.Z - Second.Z) <= Tolerance;
    }

    bool AreQuaternionsEquivalent(
        const FQuat& First,
        const FQuat& Second,
        double Tolerance = QuaternionTolerance)
    {
        FQuat FirstNormalized = First;
        FQuat SecondNormalized = Second;

        FirstNormalized.Normalize();
        SecondNormalized.Normalize();

        const double DotProduct =
            FirstNormalized.X * SecondNormalized.X +
            FirstNormalized.Y * SecondNormalized.Y +
            FirstNormalized.Z * SecondNormalized.Z +
            FirstNormalized.W * SecondNormalized.W;

        /*
         * q and -q represent the same orientation.
         */
        return
            FMath::Abs(FMath::Abs(DotProduct) - 1.0) <=
            Tolerance;
    }

    FTGSimulationScenario MakeScenarioWithRoot()
    {
        FTGSimulationScenario Scenario;

        FTGComponentConfig Root;

        Root.ComponentId = FGuid::NewGuid();
        Root.Name = TEXT("Main Body");

        Root.OriginInBodyMeters = FVector::ZeroVector;
        Root.ComponentToBodyOrientation = FQuat::Identity;

        Root.ParentComponentName.Reset();
        Root.DegreesOfFreedom.Reset();

        Scenario.Components.Add(Root);

        return Scenario;
    }

    FTGComponentConfig MakeDefaultChild()
    {
        FTGComponentConfig Child;

        Child.ComponentId = FGuid::NewGuid();
        Child.Name = TEXT("Child");
        Child.ParentComponentName = TEXT("Main Body");

        Child.ParentAnchorMeters = FVector::ZeroVector;
        Child.ChildAnchorMeters = FVector::ZeroVector;

        Child.ChildToParentZeroOrientation =
            FQuat::Identity;

        Child.DegreesOfFreedom.Reset();

        return Child;
    }

    FTGJointDofConfig MakeRotationalDof(
        const FVector& Axis)
    {
        FTGJointDofConfig Dof;

        Dof.DofId = FGuid::NewGuid();
        Dof.Name = TEXT("Rotation");
        Dof.MotionType = ETGJointMotionType::Rotation;
        Dof.Axis = Axis;

        Dof.InitialCoordinate = 0.0;
        Dof.InitialRate = 0.0;

        Dof.bHasMinimumCoordinate = true;
        Dof.MinimumCoordinate = -3.14159265358979323846;

        Dof.bHasMaximumCoordinate = true;
        Dof.MaximumCoordinate = 3.14159265358979323846;

        Dof.MaximumAbsoluteRate = 1.0;
        Dof.MaximumAbsoluteEffort = 0.0;

        return Dof;
    }

    FTGJointDofConfig MakeTranslationalDof(
        const FVector& Axis,
        double MinimumCoordinate = -10.0,
        double MaximumCoordinate = 10.0)
    {
        FTGJointDofConfig Dof;

        Dof.DofId = FGuid::NewGuid();
        Dof.Name = TEXT("Translation");
        Dof.MotionType = ETGJointMotionType::Translation;
        Dof.Axis = Axis;

        Dof.InitialCoordinate = 0.0;
        Dof.InitialRate = 0.0;

        Dof.bHasMinimumCoordinate = true;
        Dof.MinimumCoordinate = MinimumCoordinate;

        Dof.bHasMaximumCoordinate = true;
        Dof.MaximumCoordinate = MaximumCoordinate;

        Dof.MaximumAbsoluteRate = 1.0;
        Dof.MaximumAbsoluteEffort = 0.0;

        return Dof;
    }

    bool EvaluateScenario(
        FAutomationTestBase& Test,
        const FTGSimulationScenario& Scenario,
        const TArray<double>& RequestedCoordinates,
        TArray<FTGComponentKinematicPose>& OutPoses,
        TArray<double>& OutAcceptedCoordinates)
    {
        FText ErrorText;

        const bool bSucceeded =
            UTGComponentKinematicsLibrary::
                EvaluateComponentTreeKinematics(
                    Scenario,
                    RequestedCoordinates,
                    OutPoses,
                    OutAcceptedCoordinates,
                    ErrorText);

        if (!bSucceeded)
        {
            Test.AddError(
                FString::Printf(
                    TEXT(
                        "Kinematics evaluation failed: %s"),
                    *ErrorText.ToString()));
        }

        return bSucceeded;
    }
}

// ============================================================================
// Contract Example A
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGComponentKinematicsSingleRotationTest,
    "TG.ComponentTree.Kinematics.Contract.SingleRotationalDof",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FTGComponentKinematicsSingleRotationTest::RunTest(
    const FString& Parameters)
{
    using namespace TGComponentKinematicsTestsPrivate;

    FTGSimulationScenario Scenario =
        MakeScenarioWithRoot();

    FTGComponentConfig Child =
        MakeDefaultChild();

    Child.ParentAnchorMeters =
        FVector(1.0, 0.0, 0.0);

    Child.ChildAnchorMeters =
        FVector(0.5, 0.0, 0.0);

    Child.ChildToParentZeroOrientation =
        FQuat::Identity;

    Child.DegreesOfFreedom.Add(
        MakeRotationalDof(FVector::UpVector));

    Scenario.Components.Add(Child);

    TArray<double> RequestedCoordinates;
    RequestedCoordinates.Add(HalfPi);

    TArray<FTGComponentKinematicPose> Poses;
    TArray<double> AcceptedCoordinates;

    if (!EvaluateScenario(
            *this,
            Scenario,
            RequestedCoordinates,
            Poses,
            AcceptedCoordinates))
    {
        return false;
    }

    TestEqual(
        TEXT("Two component poses must be returned"),
        Poses.Num(),
        2);

    TestEqual(
        TEXT("One accepted coordinate must be returned"),
        AcceptedCoordinates.Num(),
        1);

    if (
        Poses.Num() != 2 ||
        AcceptedCoordinates.Num() != 1)
    {
        return false;
    }

    TestTrue(
        TEXT("The requested rotational coordinate is accepted"),
        FMath::Abs(
            AcceptedCoordinates[0] - HalfPi) <=
            CoordinateTolerance);

    const FVector ExpectedOrigin(
        1.0,
        -0.5,
        0.0);

    TestTrue(
        TEXT(
            "The child origin matches contract Example A"),
        AreVectorsNearlyEqual(
            Poses[1].OriginInBodyMeters,
            ExpectedOrigin));

    const double SinHalfAngle =
        FMath::Sin(0.5 * HalfPi);

    const double CosHalfAngle =
        FMath::Cos(0.5 * HalfPi);

    const FQuat ExpectedOrientation(
        0.0,
        0.0,
        SinHalfAngle,
        CosHalfAngle);

    TestTrue(
        TEXT(
            "The child orientation matches Rz(pi/2)"),
        AreQuaternionsEquivalent(
            Poses[1].ComponentToBodyOrientation,
            ExpectedOrientation));

    return true;
}

// ============================================================================
// Contract Example B
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGComponentKinematicsMixedDofTest,
    "TG.ComponentTree.Kinematics.Contract.MixedRotationTranslation",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FTGComponentKinematicsMixedDofTest::RunTest(
    const FString& Parameters)
{
    using namespace TGComponentKinematicsTestsPrivate;

    FTGSimulationScenario Scenario =
        MakeScenarioWithRoot();

    FTGComponentConfig Child =
        MakeDefaultChild();

    Child.ParentAnchorMeters =
        FVector(1.0, 2.0, 0.0);

    Child.ChildAnchorMeters =
        FVector(0.0, 1.0, 0.0);

    /*
     * Zero orientation: rotation about +X by pi/2.
     */
    Child.ChildToParentZeroOrientation =
        FQuat(
            FMath::Sin(0.5 * HalfPi),
            0.0,
            0.0,
            FMath::Cos(0.5 * HalfPi));

    Child.DegreesOfFreedom.Add(
        MakeRotationalDof(FVector::UpVector));

    Child.DegreesOfFreedom.Add(
        MakeTranslationalDof(FVector::ForwardVector));

    Scenario.Components.Add(Child);

    TArray<double> RequestedCoordinates;
    RequestedCoordinates.Add(HalfPi);
    RequestedCoordinates.Add(2.0);

    TArray<FTGComponentKinematicPose> Poses;
    TArray<double> AcceptedCoordinates;

    if (!EvaluateScenario(
            *this,
            Scenario,
            RequestedCoordinates,
            Poses,
            AcceptedCoordinates))
    {
        return false;
    }

    TestEqual(
        TEXT("Two component poses must be returned"),
        Poses.Num(),
        2);

    TestEqual(
        TEXT("Two accepted coordinates must be returned"),
        AcceptedCoordinates.Num(),
        2);

    if (
        Poses.Num() != 2 ||
        AcceptedCoordinates.Num() != 2)
    {
        return false;
    }

    TestTrue(
        TEXT(
            "The rotational coordinate is accepted"),
        FMath::Abs(
            AcceptedCoordinates[0] - HalfPi) <=
            CoordinateTolerance);

    TestTrue(
        TEXT(
            "The translational coordinate is accepted"),
        FMath::Abs(
            AcceptedCoordinates[1] - 2.0) <=
            CoordinateTolerance);

    const FVector ExpectedOrigin(
        1.0,
        4.0,
        -1.0);

    TestTrue(
        TEXT(
            "The child origin matches contract Example B"),
        AreVectorsNearlyEqual(
            Poses[1].OriginInBodyMeters,
            ExpectedOrigin));

    /*
     * Rz(pi/2) * Rx(pi/2)
     * Backend scalar-first quaternion:
     * [w,x,y,z] = [0.5,0.5,0.5,0.5]
     *
     * Stored in FQuat fields:
     * [x,y,z,w] = [0.5,0.5,0.5,0.5]
     */
    const FQuat ExpectedOrientation(
        0.5,
        0.5,
        0.5,
        0.5);

    TestTrue(
        TEXT(
            "The child orientation matches "
            "Rz(pi/2) multiplied by Rx(pi/2)"),
        AreQuaternionsEquivalent(
            Poses[1].ComponentToBodyOrientation,
            ExpectedOrientation));

    return true;
}

// ============================================================================
// Preview-limit clamping
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGComponentKinematicsCoordinateClampingTest,
    "TG.ComponentTree.Kinematics.Preview.CoordinateClamping",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FTGComponentKinematicsCoordinateClampingTest::RunTest(
    const FString& Parameters)
{
    using namespace TGComponentKinematicsTestsPrivate;

    FTGSimulationScenario Scenario =
        MakeScenarioWithRoot();

    FTGComponentConfig Child =
        MakeDefaultChild();

    Child.DegreesOfFreedom.Add(
        MakeTranslationalDof(
            FVector::ForwardVector,
            -1.0,
            1.0));

    Scenario.Components.Add(Child);

    TArray<double> RequestedCoordinates;
    RequestedCoordinates.Add(4.0);

    TArray<FTGComponentKinematicPose> Poses;
    TArray<double> AcceptedCoordinates;

    if (!EvaluateScenario(
            *this,
            Scenario,
            RequestedCoordinates,
            Poses,
            AcceptedCoordinates))
    {
        return false;
    }

    if (
        Poses.Num() != 2 ||
        AcceptedCoordinates.Num() != 1)
    {
        AddError(
            TEXT(
                "The clamping test returned an unexpected "
                "pose or coordinate count."));

        return false;
    }

    TestTrue(
        TEXT(
            "Requested coordinate 4 is clamped to maximum 1"),
        FMath::Abs(
            AcceptedCoordinates[0] - 1.0) <=
            CoordinateTolerance);

    TestTrue(
        TEXT(
            "The clamped translation produces origin (1,0,0)"),
        AreVectorsNearlyEqual(
            Poses[1].OriginInBodyMeters,
            FVector(1.0, 0.0, 0.0)));

    return true;
}

// ============================================================================
// Backend-to-Unreal conversion
// ============================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGComponentKinematicsUnrealConversionTest,
    "TG.ComponentTree.Kinematics.Conversion.BackendToUnreal",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FTGComponentKinematicsUnrealConversionTest::RunTest(
    const FString& Parameters)
{
    using namespace TGComponentKinematicsTestsPrivate;

    const FVector BackendPositionMeters(
        1.0,
        2.0,
        3.0);

    const FVector UnrealPositionCentimeters =
        UTGComponentKinematicsLibrary::
            ConvertBackendPositionMetersToUnrealCentimeters(
                BackendPositionMeters);

    TestTrue(
        TEXT(
            "Position conversion applies meters-to-centimeters "
            "and reflects Y"),
        AreVectorsNearlyEqual(
            UnrealPositionCentimeters,
            FVector(100.0, -200.0, 300.0)));

    const double SinHalfAngle =
        FMath::Sin(0.5 * HalfPi);

    const double CosHalfAngle =
        FMath::Cos(0.5 * HalfPi);

    const FQuat BackendPositiveZRotation(
        0.0,
        0.0,
        SinHalfAngle,
        CosHalfAngle);

    const FQuat UnrealOrientation =
        UTGComponentKinematicsLibrary::
            ConvertBackendOrientationToUnreal(
                BackendPositiveZRotation);

    /*
     * R_UE = C * R_backend * C maps backend +Z rotation
     * to the opposite numeric Z quaternion component.
     */
    const FQuat ExpectedUnrealOrientation(
        0.0,
        0.0,
        -SinHalfAngle,
        CosHalfAngle);

    TestTrue(
        TEXT(
            "Orientation conversion applies C R C"),
        AreQuaternionsEquivalent(
            UnrealOrientation,
            ExpectedUnrealOrientation));

    return true;
}

#endif