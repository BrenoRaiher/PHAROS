// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/ComponentTree/TGComponentKinematicsLibrary.h"

namespace TGComponentKinematicsPrivate
{
    constexpr double AxisNormErrorThreshold = 1.0e-12;
    constexpr double QuaternionSquaredNormErrorThreshold = 1.0e-12;

    bool IsFinite(double Value)
    {
        return FMath::IsFinite(Value);
    }

    bool IsFiniteVector(const FVector& Value)
    {
        return
            IsFinite(Value.X) &&
            IsFinite(Value.Y) &&
            IsFinite(Value.Z);
    }

    bool IsFiniteQuaternion(const FQuat& Value)
    {
        return
            IsFinite(Value.X) &&
            IsFinite(Value.Y) &&
            IsFinite(Value.Z) &&
            IsFinite(Value.W);
    }

    FVector BackendCross(
        const FVector& First,
        const FVector& Second)
    {
        return FVector(
            First.Y * Second.Z - First.Z * Second.Y,
            First.Z * Second.X - First.X * Second.Z,
            First.X * Second.Y - First.Y * Second.X);
    }

    /**
     * Backend-convention quaternion.
     *
     * Storage is explicit scalar-first internally:
     *   W, X, Y, Z
     *
     * Products are Hamilton products and rotations are active,
     * right-handed rotations.
     */
    struct FBackendQuaternion
    {
        double W = 1.0;
        double X = 0.0;
        double Y = 0.0;
        double Z = 0.0;

        static FBackendQuaternion Identity()
        {
            return FBackendQuaternion();
        }

        static bool FromStoredFQuat(
            const FQuat& Stored,
            FBackendQuaternion& OutQuaternion)
        {
            if (!IsFiniteQuaternion(Stored))
            {
                OutQuaternion = Identity();
                return false;
            }

            const double SquaredNorm =
                Stored.X * Stored.X +
                Stored.Y * Stored.Y +
                Stored.Z * Stored.Z +
                Stored.W * Stored.W;

            if (
                !IsFinite(SquaredNorm) ||
                SquaredNorm <=
                    QuaternionSquaredNormErrorThreshold)
            {
                OutQuaternion = Identity();
                return false;
            }

            const double InverseNorm =
                1.0 / FMath::Sqrt(SquaredNorm);

            OutQuaternion.W = Stored.W * InverseNorm;
            OutQuaternion.X = Stored.X * InverseNorm;
            OutQuaternion.Y = Stored.Y * InverseNorm;
            OutQuaternion.Z = Stored.Z * InverseNorm;

            return true;
        }

        static FBackendQuaternion FromNormalizedAxisAngle(
            const FVector& NormalizedAxis,
            double AngleRadians)
        {
            const double HalfAngle = 0.5 * AngleRadians;
            const double SinHalfAngle = FMath::Sin(HalfAngle);

            FBackendQuaternion Result;

            Result.W = FMath::Cos(HalfAngle);
            Result.X = NormalizedAxis.X * SinHalfAngle;
            Result.Y = NormalizedAxis.Y * SinHalfAngle;
            Result.Z = NormalizedAxis.Z * SinHalfAngle;

            return Result;
        }

        FBackendQuaternion operator*(
            const FBackendQuaternion& Right) const
        {
            FBackendQuaternion Result;

            Result.W =
                W * Right.W -
                X * Right.X -
                Y * Right.Y -
                Z * Right.Z;

            Result.X =
                W * Right.X +
                X * Right.W +
                Y * Right.Z -
                Z * Right.Y;

            Result.Y =
                W * Right.Y -
                X * Right.Z +
                Y * Right.W +
                Z * Right.X;

            Result.Z =
                W * Right.Z +
                X * Right.Y -
                Y * Right.X +
                Z * Right.W;

            return Result;
        }

        bool Normalize()
        {
            const double SquaredNorm =
                W * W +
                X * X +
                Y * Y +
                Z * Z;

            if (
                !IsFinite(SquaredNorm) ||
                SquaredNorm <=
                    QuaternionSquaredNormErrorThreshold)
            {
                *this = Identity();
                return false;
            }

            const double InverseNorm =
                1.0 / FMath::Sqrt(SquaredNorm);

            W *= InverseNorm;
            X *= InverseNorm;
            Y *= InverseNorm;
            Z *= InverseNorm;

            return true;
        }

        FVector RotateVector(
            const FVector& Vector) const
        {
            const FVector QuaternionVector(X, Y, Z);

            const double VectorDotQuaternion =
                FVector::DotProduct(
                    QuaternionVector,
                    Vector);

            const double QuaternionVectorSquared =
                FVector::DotProduct(
                    QuaternionVector,
                    QuaternionVector);

            return
                2.0 *
                    VectorDotQuaternion *
                    QuaternionVector +
                (W * W - QuaternionVectorSquared) *
                    Vector +
                2.0 *
                    W *
                    BackendCross(
                        QuaternionVector,
                        Vector);
        }

        FQuat ToStoredFQuat() const
        {
            return FQuat(X, Y, Z, W);
        }
    };

    bool NamesEqual(
        const FString& First,
        const FString& Second)
    {
        return First.Equals(
            Second,
            ESearchCase::IgnoreCase);
    }

    FString Trimmed(const FString& Value)
    {
        FString Result = Value;
        Result.TrimStartAndEndInline();
        return Result;
    }

    int32 FindEarlierParentIndex(
        const TArray<FTGComponentConfig>& Components,
        int32 ChildIndex,
        const FString& ParentName)
    {
        for (int32 Index = 0; Index < ChildIndex; ++Index)
        {
            if (NamesEqual(
                    Trimmed(Components[Index].Name),
                    Trimmed(ParentName)))
            {
                return Index;
            }
        }

        return INDEX_NONE;
    }

    bool ValidateComponentNames(
        const TArray<FTGComponentConfig>& Components,
        FText& OutErrorText)
    {
        for (
            int32 ComponentIndex = 0;
            ComponentIndex < Components.Num();
            ++ComponentIndex)
        {
            const FString ComponentName =
                Trimmed(Components[ComponentIndex].Name);

            if (ComponentName.IsEmpty())
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Components[%d] has an empty name."),
                        ComponentIndex));

                return false;
            }

            for (
                int32 EarlierIndex = 0;
                EarlierIndex < ComponentIndex;
                ++EarlierIndex)
            {
                if (NamesEqual(
                        ComponentName,
                        Trimmed(
                            Components[EarlierIndex].Name)))
                {
                    OutErrorText = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Component name '%s' is duplicated."),
                            *ComponentName));

                    return false;
                }
            }
        }

        return true;
    }

    bool NormalizeAxis(
        const FVector& Axis,
        FVector& OutNormalizedAxis)
    {
        OutNormalizedAxis = FVector::ZeroVector;

        if (!IsFiniteVector(Axis))
        {
            return false;
        }

        const double SquaredNorm =
            Axis.X * Axis.X +
            Axis.Y * Axis.Y +
            Axis.Z * Axis.Z;

        if (
            !IsFinite(SquaredNorm) ||
            SquaredNorm <=
                AxisNormErrorThreshold *
                AxisNormErrorThreshold)
        {
            return false;
        }

        OutNormalizedAxis =
            Axis / FMath::Sqrt(SquaredNorm);

        return true;
    }

    bool ResolveAcceptedCoordinate(
        const FTGJointDofConfig& Dof,
        double RequestedCoordinate,
        double& OutAcceptedCoordinate,
        FText& OutErrorText,
        int32 ComponentIndex,
        int32 DofIndex)
    {
        OutAcceptedCoordinate = 0.0;

        if (!IsFinite(RequestedCoordinate))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Components[%d].DegreesOfFreedom[%d] "
                        "has a non-finite requested coordinate."),
                    ComponentIndex,
                    DofIndex));

            return false;
        }

        if (
            Dof.bHasMinimumCoordinate &&
            !IsFinite(Dof.MinimumCoordinate))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Components[%d].DegreesOfFreedom[%d] "
                        "has a non-finite minimum coordinate."),
                    ComponentIndex,
                    DofIndex));

            return false;
        }

        if (
            Dof.bHasMaximumCoordinate &&
            !IsFinite(Dof.MaximumCoordinate))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Components[%d].DegreesOfFreedom[%d] "
                        "has a non-finite maximum coordinate."),
                    ComponentIndex,
                    DofIndex));

            return false;
        }

        if (
            Dof.bHasMinimumCoordinate &&
            Dof.bHasMaximumCoordinate &&
            Dof.MinimumCoordinate >
                Dof.MaximumCoordinate)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Components[%d].DegreesOfFreedom[%d] "
                        "has a minimum coordinate greater than "
                        "its maximum coordinate."),
                    ComponentIndex,
                    DofIndex));

            return false;
        }

        OutAcceptedCoordinate = RequestedCoordinate;

        if (
            Dof.bHasMinimumCoordinate &&
            OutAcceptedCoordinate <
                Dof.MinimumCoordinate)
        {
            OutAcceptedCoordinate =
                Dof.MinimumCoordinate;
        }

        if (
            Dof.bHasMaximumCoordinate &&
            OutAcceptedCoordinate >
                Dof.MaximumCoordinate)
        {
            OutAcceptedCoordinate =
                Dof.MaximumCoordinate;
        }

        return true;
    }
}

int32 UTGComponentKinematicsLibrary::
    GetArticulationCoordinateCount(
        const FTGSimulationScenario& Scenario)
{
    int32 CoordinateCount = 0;

    for (const FTGComponentConfig& Component
         : Scenario.Components)
    {
        CoordinateCount +=
            Component.DegreesOfFreedom.Num();
    }

    return CoordinateCount;
}

bool UTGComponentKinematicsLibrary::
    EvaluateComponentTreeKinematics(
        const FTGSimulationScenario& Scenario,
        const TArray<double>& RequestedArticulationCoordinates,
        TArray<FTGComponentKinematicPose>& OutComponentPoses,
        TArray<double>& OutAcceptedArticulationCoordinates,
        FText& OutErrorText)
{
    using namespace TGComponentKinematicsPrivate;

    OutComponentPoses.Reset();
    OutAcceptedArticulationCoordinates.Reset();
    OutErrorText = FText::GetEmpty();

    if (Scenario.Components.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The spacecraft must contain at least "
                "one physical component."));

        return false;
    }

    if (!ValidateComponentNames(
            Scenario.Components,
            OutErrorText))
    {
        return false;
    }

    const int32 ExpectedCoordinateCount =
        GetArticulationCoordinateCount(Scenario);

    const bool bUseRequestedCoordinates =
        !RequestedArticulationCoordinates.IsEmpty();

    if (
        bUseRequestedCoordinates &&
        RequestedArticulationCoordinates.Num() !=
            ExpectedCoordinateCount)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Expected %d articulation coordinate(s), "
                    "but received %d."),
                ExpectedCoordinateCount,
                RequestedArticulationCoordinates.Num()));

        return false;
    }

    if (!Scenario.Components[0]
            .ParentComponentName
            .TrimStartAndEnd()
            .IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Component zero is the main component "
                "and cannot have a parent."));

        return false;
    }

    if (!Scenario.Components[0]
            .DegreesOfFreedom
            .IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Component zero cannot have parent-joint "
                "degrees of freedom."));

        return false;
    }

    OutComponentPoses.Reserve(
        Scenario.Components.Num());

    OutAcceptedArticulationCoordinates.Reserve(
        ExpectedCoordinateCount);

    int32 FlatCoordinateIndex = 0;

    for (
        int32 ComponentIndex = 0;
        ComponentIndex < Scenario.Components.Num();
        ++ComponentIndex)
    {
        const FTGComponentConfig& Component =
            Scenario.Components[ComponentIndex];

        FTGComponentKinematicPose Pose;

        Pose.ComponentId = Component.ComponentId;
        Pose.ComponentName = Component.Name;

        if (ComponentIndex == 0)
        {
            if (!IsFiniteVector(
                    Component.OriginInBodyMeters))
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "The main component origin in B "
                        "must be finite."));

                OutComponentPoses.Reset();
                OutAcceptedArticulationCoordinates.Reset();
                return false;
            }

            FBackendQuaternion RootOrientation;

            if (!FBackendQuaternion::FromStoredFQuat(
                    Component.ComponentToBodyOrientation,
                    RootOrientation))
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "The main component-to-B orientation "
                        "must be finite and nonzero."));

                OutComponentPoses.Reset();
                OutAcceptedArticulationCoordinates.Reset();
                return false;
            }

            Pose.OriginInBodyMeters =
                Component.OriginInBodyMeters;

            Pose.ComponentToBodyOrientation =
                RootOrientation.ToStoredFQuat();

            OutComponentPoses.Add(Pose);
            continue;
        }

        const int32 ParentIndex =
            FindEarlierParentIndex(
                Scenario.Components,
                ComponentIndex,
                Component.ParentComponentName);

        if (ParentIndex == INDEX_NONE)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' must reference an "
                        "existing parent appearing earlier "
                        "in the component array."),
                    *Component.Name));

            OutComponentPoses.Reset();
            OutAcceptedArticulationCoordinates.Reset();
            return false;
        }

        if (
            !IsFiniteVector(
                Component.ParentAnchorMeters) ||
            !IsFiniteVector(
                Component.ChildAnchorMeters))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "The anchor vectors of component '%s' "
                        "must be finite."),
                    *Component.Name));

            OutComponentPoses.Reset();
            OutAcceptedArticulationCoordinates.Reset();
            return false;
        }

        FBackendQuaternion ZeroOrientation;

        if (!FBackendQuaternion::FromStoredFQuat(
                Component.ChildToParentZeroOrientation,
                ZeroOrientation))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "The child-to-parent zero orientation "
                        "of component '%s' must be finite "
                        "and nonzero."),
                    *Component.Name));

            OutComponentPoses.Reset();
            OutAcceptedArticulationCoordinates.Reset();
            return false;
        }

        FBackendQuaternion JointOrientation =
            FBackendQuaternion::Identity();

        FVector JointTranslationParent =
            FVector::ZeroVector;

        for (
            int32 DofIndex = 0;
            DofIndex <
                Component.DegreesOfFreedom.Num();
            ++DofIndex)
        {
            const FTGJointDofConfig& Dof =
                Component.DegreesOfFreedom[DofIndex];

            FVector NormalizedAxis;

            if (!NormalizeAxis(
                    Dof.Axis,
                    NormalizedAxis))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "The axis of component '%s', "
                            "DOF %d must be finite and nonzero."),
                        *Component.Name,
                        DofIndex));

                OutComponentPoses.Reset();
                OutAcceptedArticulationCoordinates.Reset();
                return false;
            }

            const double RequestedCoordinate =
                bUseRequestedCoordinates
                ? RequestedArticulationCoordinates[
                    FlatCoordinateIndex]
                : Dof.InitialCoordinate;

            double AcceptedCoordinate = 0.0;

            if (!ResolveAcceptedCoordinate(
                    Dof,
                    RequestedCoordinate,
                    AcceptedCoordinate,
                    OutErrorText,
                    ComponentIndex,
                    DofIndex))
            {
                OutComponentPoses.Reset();
                OutAcceptedArticulationCoordinates.Reset();
                return false;
            }

            OutAcceptedArticulationCoordinates.Add(
                AcceptedCoordinate);

            if (
                Dof.MotionType ==
                ETGJointMotionType::Rotation)
            {
                const FBackendQuaternion DofRotation =
                    FBackendQuaternion::
                        FromNormalizedAxisAngle(
                            NormalizedAxis,
                            AcceptedCoordinate);

                JointOrientation =
                    JointOrientation * DofRotation;

                if (!JointOrientation.Normalize())
                {
                    OutErrorText = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "The accumulated joint "
                                "orientation of component '%s' "
                                "became invalid."),
                            *Component.Name));

                    OutComponentPoses.Reset();
                    OutAcceptedArticulationCoordinates.Reset();
                    return false;
                }
            }
            else
            {
                const FVector AxisInParent =
                    JointOrientation.RotateVector(
                        NormalizedAxis);

                JointTranslationParent +=
                    AxisInParent *
                    AcceptedCoordinate;
            }

            ++FlatCoordinateIndex;
        }

        FBackendQuaternion ParentToChildOrientation =
            JointOrientation * ZeroOrientation;

        if (!ParentToChildOrientation.Normalize())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "The parent-to-child orientation "
                        "of component '%s' became invalid."),
                    *Component.Name));

            OutComponentPoses.Reset();
            OutAcceptedArticulationCoordinates.Reset();
            return false;
        }

        const FVector ParentToChildOriginParent =
            Component.ParentAnchorMeters +
            JointTranslationParent -
            ParentToChildOrientation.RotateVector(
                Component.ChildAnchorMeters);

        const FTGComponentKinematicPose& ParentPose =
            OutComponentPoses[ParentIndex];

        FBackendQuaternion ParentToBodyOrientation;

        if (!FBackendQuaternion::FromStoredFQuat(
                ParentPose.ComponentToBodyOrientation,
                ParentToBodyOrientation))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "The evaluated parent orientation "
                        "for component '%s' is invalid."),
                    *Component.Name));

            OutComponentPoses.Reset();
            OutAcceptedArticulationCoordinates.Reset();
            return false;
        }

        FBackendQuaternion ComponentToBodyOrientation =
            ParentToBodyOrientation *
            ParentToChildOrientation;

        if (!ComponentToBodyOrientation.Normalize())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "The body-relative orientation "
                        "of component '%s' became invalid."),
                    *Component.Name));

            OutComponentPoses.Reset();
            OutAcceptedArticulationCoordinates.Reset();
            return false;
        }

        Pose.OriginInBodyMeters =
            ParentPose.OriginInBodyMeters +
            ParentToBodyOrientation.RotateVector(
                ParentToChildOriginParent);

        Pose.ComponentToBodyOrientation =
            ComponentToBodyOrientation.ToStoredFQuat();

        OutComponentPoses.Add(Pose);
    }

    if (FlatCoordinateIndex != ExpectedCoordinateCount)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The articulation-coordinate traversal "
                "did not match the expected flattened size."));

        OutComponentPoses.Reset();
        OutAcceptedArticulationCoordinates.Reset();
        return false;
    }

    return true;
}

FVector UTGComponentKinematicsLibrary::
    ConvertBackendPositionMetersToUnrealCentimeters(
        FVector BackendPositionMeters)
{
    return FVector(
        100.0 * BackendPositionMeters.X,
        -100.0 * BackendPositionMeters.Y,
        100.0 * BackendPositionMeters.Z);
}

FVector UTGComponentKinematicsLibrary::
    ConvertBackendDirectionToUnreal(
        FVector BackendDirection)
{
    return FVector(
        BackendDirection.X,
        -BackendDirection.Y,
        BackendDirection.Z);
}

FQuat UTGComponentKinematicsLibrary::
    ConvertBackendOrientationToUnreal(
        FQuat BackendOrientation)
{
    using namespace TGComponentKinematicsPrivate;

    FBackendQuaternion NormalizedBackendOrientation;

    if (!FBackendQuaternion::FromStoredFQuat(
            BackendOrientation,
            NormalizedBackendOrientation))
    {
        return FQuat::Identity;
    }

    FQuat UnrealOrientation(
        -NormalizedBackendOrientation.X,
        NormalizedBackendOrientation.Y,
        -NormalizedBackendOrientation.Z,
        NormalizedBackendOrientation.W);

    UnrealOrientation.Normalize();
    return UnrealOrientation;
}

FTransform UTGComponentKinematicsLibrary::
    ConvertComponentPoseToUnrealTransform(
        const FTGComponentKinematicPose& BackendPose)
{
    return FTransform(
        ConvertBackendOrientationToUnreal(
            BackendPose.ComponentToBodyOrientation),
        ConvertBackendPositionMetersToUnrealCentimeters(
            BackendPose.OriginInBodyMeters),
        FVector::OneVector);
}