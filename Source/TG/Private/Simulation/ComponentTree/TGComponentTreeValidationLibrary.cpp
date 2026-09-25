// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/ComponentTree/TGComponentTreeValidationLibrary.h"

#include "Misc/Paths.h"

namespace TGComponentTreeValidationPrivate
{
    constexpr double VectorTolerance = 1.0e-12;
    constexpr double UnitVectorTolerance = 1.0e-6;
    constexpr double UnitQuaternionTolerance = 1.0e-6;

    bool NamesEqual(
        const FString& First,
        const FString& Second)
    {
        return First.Equals(Second, ESearchCase::IgnoreCase);
    }

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

    bool IsFiniteColor(const FLinearColor& Value)
    {
        return
            IsFinite(Value.R) &&
            IsFinite(Value.G) &&
            IsFinite(Value.B) &&
            IsFinite(Value.A);
    }

    bool IsSupportedTextureFilePath(const FString& FilePath)
    {
        const FString Extension =
            FPaths::GetExtension(FilePath, false).ToLower();

        return
            Extension == TEXT("png") ||
            Extension == TEXT("jpg") ||
            Extension == TEXT("jpeg");
    }

    void AddIssue(
        FTGComponentTreeValidationReport& Report,
        ETGComponentTreeIssueSeverity Severity,
        const FString& Path,
        const FText& Message,
        const FGuid& ComponentId = FGuid(),
        const FGuid& DofId = FGuid())
    {
        FTGComponentTreeValidationIssue Issue;

        Issue.Severity = Severity;
        Issue.Path = Path;
        Issue.ComponentId = ComponentId;
        Issue.DofId = DofId;
        Issue.Message = Message;

        Report.Issues.Add(Issue);

        if (Severity == ETGComponentTreeIssueSeverity::Error)
        {
            ++Report.ErrorCount;
        }
        else
        {
            ++Report.WarningCount;
        }
    }

    bool ComputePrincipalMoments(
        const FTGSymmetricInertia& Inertia,
        FVector& OutPrincipalMoments)
    {
        const double A00 =
            Inertia.IxxKilogramMetersSquared;

        const double A11 =
            Inertia.IyyKilogramMetersSquared;

        const double A22 =
            Inertia.IzzKilogramMetersSquared;

        const double A01 =
            Inertia.IxyKilogramMetersSquared;

        const double A02 =
            Inertia.IxzKilogramMetersSquared;

        const double A12 =
            Inertia.IyzKilogramMetersSquared;

        if (
            !IsFinite(A00) ||
            !IsFinite(A11) ||
            !IsFinite(A22) ||
            !IsFinite(A01) ||
            !IsFinite(A02) ||
            !IsFinite(A12))
        {
            OutPrincipalMoments = FVector::ZeroVector;
            return false;
        }

        const double OffDiagonalSquareSum =
            A01 * A01 +
            A02 * A02 +
            A12 * A12;

        TArray<double> Eigenvalues;
        Eigenvalues.Reserve(3);

        if (OffDiagonalSquareSum <= 1.0e-30)
        {
            Eigenvalues.Add(A00);
            Eigenvalues.Add(A11);
            Eigenvalues.Add(A22);
        }
        else
        {
            const double MeanTrace =
                (A00 + A11 + A22) / 3.0;

            const double Centered00 = A00 - MeanTrace;
            const double Centered11 = A11 - MeanTrace;
            const double Centered22 = A22 - MeanTrace;

            const double P2 =
                Centered00 * Centered00 +
                Centered11 * Centered11 +
                Centered22 * Centered22 +
                2.0 * OffDiagonalSquareSum;

            const double P =
                FMath::Sqrt(FMath::Max(P2 / 6.0, 0.0));

            if (P <= 1.0e-15)
            {
                Eigenvalues.Add(MeanTrace);
                Eigenvalues.Add(MeanTrace);
                Eigenvalues.Add(MeanTrace);
            }
            else
            {
                const double B00 = Centered00 / P;
                const double B11 = Centered11 / P;
                const double B22 = Centered22 / P;

                const double B01 = A01 / P;
                const double B02 = A02 / P;
                const double B12 = A12 / P;

                const double DeterminantB =
                    B00 * (B11 * B22 - B12 * B12) -
                    B01 * (B01 * B22 - B12 * B02) +
                    B02 * (B01 * B12 - B11 * B02);

                const double R =
                    FMath::Clamp(
                        DeterminantB / 2.0,
                        -1.0,
                        1.0);

                const double Phi =
                    FMath::Acos(R) / 3.0;

                constexpr double TwoPiOverThree =
                    2.0943951023931954923;

                const double EigenvalueLargest =
                    MeanTrace +
                    2.0 * P * FMath::Cos(Phi);

                const double EigenvalueSmallest =
                    MeanTrace +
                    2.0 * P *
                        FMath::Cos(Phi + TwoPiOverThree);

                const double EigenvalueMiddle =
                    3.0 * MeanTrace -
                    EigenvalueLargest -
                    EigenvalueSmallest;

                Eigenvalues.Add(EigenvalueSmallest);
                Eigenvalues.Add(EigenvalueMiddle);
                Eigenvalues.Add(EigenvalueLargest);
            }
        }

        Eigenvalues.Sort();

        OutPrincipalMoments = FVector(
            Eigenvalues[0],
            Eigenvalues[1],
            Eigenvalues[2]);

        return true;
    }

    void ValidateQuaternion(
        const FQuat& Quaternion,
        const FString& Path,
        const FGuid& ComponentId,
        FTGComponentTreeValidationReport& Report)
    {
        if (!IsFiniteQuaternion(Quaternion))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                Path,
                FText::FromString(
                    TEXT(
                        "Quaternion components must all be finite.")),
                ComponentId);

            return;
        }

        const double SquaredNorm =
            Quaternion.X * Quaternion.X +
            Quaternion.Y * Quaternion.Y +
            Quaternion.Z * Quaternion.Z +
            Quaternion.W * Quaternion.W;

        if (SquaredNorm <= VectorTolerance)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                Path,
                FText::FromString(
                    TEXT("Quaternion cannot be zero.")),
                ComponentId);

            return;
        }

        const double Norm = FMath::Sqrt(SquaredNorm);

        if (
            FMath::Abs(Norm - 1.0) >
            UnitQuaternionTolerance)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Warning,
                Path,
                FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Quaternion norm is %.9g instead of 1. "
                            "Normalize it before simulation."),
                        Norm)),
                ComponentId);
        }
    }

    int32 FindComponentIndexByName(
        const TArray<FTGComponentConfig>& Components,
        const FString& Name)
    {
        for (int32 Index = 0; Index < Components.Num(); ++Index)
        {
            if (NamesEqual(Components[Index].Name, Name))
            {
                return Index;
            }
        }

        return INDEX_NONE;
    }

    void ValidateVisualConfig(
        const FTGComponentConfig& Component,
        int32 ComponentIndex,
        FTGComponentTreeValidationReport& Report)
    {
        const FString BasePath =
            FString::Printf(
                TEXT("Components[%d].Visual"),
                ComponentIndex);

        const FTGComponentVisualConfig& Visual =
            Component.Visual;

        // These fields remain serialized for backwards compatibility, but the
        // Component Visual Appearance contract does not expose them. Source
        // geometry must define the visual origin, orientation, and scale.
        if (Visual.StlRecenterMode != ETGStlRecenterMode::KeepImportedOrigin)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".StlRecenterMode"),
                FText::FromString(
                    TEXT(
                        "STL recentering must remain Keep Imported Origin. "
                        "Adjust the source STL instead.")),
                Component.ComponentId);
        }

        if (Visual.VisualOffsetMeters != FVector::ZeroVector)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".VisualOffsetMeters"),
                FText::FromString(
                    TEXT(
                        "Visual offset must remain zero. Adjust the source "
                        "geometry instead.")),
                Component.ComponentId);
        }

        if (Visual.VisualOrientation != FQuat::Identity)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".VisualOrientation"),
                FText::FromString(
                    TEXT(
                        "Visual orientation must remain identity. Adjust the "
                        "source geometry instead.")),
                Component.ComponentId);
        }

        if (Visual.VisualScale != FVector::OneVector)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".VisualScale"),
                FText::FromString(
                    TEXT(
                        "Visual scale must remain one. Adjust the source "
                        "geometry or choose the correct STL length unit.")),
                Component.ComponentId);
        }

        if (!IsFiniteColor(Visual.DisplayColor))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".DisplayColor"),
                FText::FromString(
                    TEXT("Display color must be finite.")),
                Component.ComponentId);
        }

        if (!IsFiniteColor(Visual.BaseColorTint))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".BaseColorTint"),
                FText::FromString(
                    TEXT("Base-color tint must be finite.")),
                Component.ComponentId);
        }

        switch (Visual.GeometrySource)
        {
            case ETGComponentGeometrySource::NoGeometry:
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".GeometrySource"),
                    FText::FromString(
                        TEXT(
                            "No Geometry is no longer a valid component "
                            "appearance. Choose Standard Primitive or "
                            "Custom STL.")),
                    Component.ComponentId);
                break;
            }

            case ETGComponentGeometrySource::Primitive:
            {
                switch (Visual.PrimitiveType)
                {
                    case ETGPrimitiveGeometryType::Box:
                    {
                        if (
                            !IsFiniteVector(
                                Visual.BoxDimensionsMeters) ||
                            Visual.BoxDimensionsMeters.X <= 0.0 ||
                            Visual.BoxDimensionsMeters.Y <= 0.0 ||
                            Visual.BoxDimensionsMeters.Z <= 0.0)
                        {
                            AddIssue(
                                Report,
                                ETGComponentTreeIssueSeverity::Error,
                                BasePath + TEXT(".BoxDimensionsMeters"),
                                FText::FromString(
                                    TEXT(
                                        "All box dimensions must be finite "
                                        "and greater than zero.")),
                                Component.ComponentId);
                        }

                        break;
                    }

                    case ETGPrimitiveGeometryType::Sphere:
                    {
                        if (
                            !IsFinite(Visual.SphereRadiusMeters) ||
                            Visual.SphereRadiusMeters <= 0.0)
                        {
                            AddIssue(
                                Report,
                                ETGComponentTreeIssueSeverity::Error,
                                BasePath + TEXT(".SphereRadiusMeters"),
                                FText::FromString(
                                    TEXT(
                                        "Sphere radius must be finite and "
                                        "greater than zero.")),
                                Component.ComponentId);
                        }

                        break;
                    }

                    case ETGPrimitiveGeometryType::Cylinder:
                    {
                        if (
                            !IsFinite(Visual.CylinderRadiusMeters) ||
                            Visual.CylinderRadiusMeters <= 0.0)
                        {
                            AddIssue(
                                Report,
                                ETGComponentTreeIssueSeverity::Error,
                                BasePath + TEXT(".CylinderRadiusMeters"),
                                FText::FromString(
                                    TEXT(
                                        "Cylinder radius must be finite and "
                                        "greater than zero.")),
                                Component.ComponentId);
                        }

                        if (
                            !IsFinite(Visual.CylinderLengthMeters) ||
                            Visual.CylinderLengthMeters <= 0.0)
                        {
                            AddIssue(
                                Report,
                                ETGComponentTreeIssueSeverity::Error,
                                BasePath + TEXT(".CylinderLengthMeters"),
                                FText::FromString(
                                    TEXT(
                                        "Cylinder length must be finite and "
                                        "greater than zero.")),
                                Component.ComponentId);
                        }

                        break;
                    }
                }

                break;
            }

            case ETGComponentGeometrySource::CustomStl:
            {
                FString TrimmedPath = Visual.StlFilePath;
                TrimmedPath.TrimStartAndEndInline();

                if (TrimmedPath.IsEmpty())
                {
                    AddIssue(
                        Report,
                        ETGComponentTreeIssueSeverity::Error,
                        BasePath + TEXT(".StlFilePath"),
                        FText::FromString(
                            TEXT(
                                "Custom STL geometry requires a selected "
                                "STL file.")),
                        Component.ComponentId);
                }
                else
                {
                    if (!FPaths::GetExtension(
                            TrimmedPath,
                            false).Equals(
                                TEXT("stl"),
                                ESearchCase::IgnoreCase))
                    {
                        AddIssue(
                            Report,
                            ETGComponentTreeIssueSeverity::Error,
                            BasePath + TEXT(".StlFilePath"),
                            FText::FromString(
                                TEXT(
                                    "Custom geometry file must use the "
                                    ".stl extension.")),
                            Component.ComponentId);
                    }

                    if (!FPaths::FileExists(TrimmedPath))
                    {
                        AddIssue(
                            Report,
                            ETGComponentTreeIssueSeverity::Error,
                            BasePath + TEXT(".StlFilePath"),
                            FText::FromString(
                                TEXT("Selected STL file does not exist.")),
                            Component.ComponentId);
                    }
                }

                break;
            }
        }

        if (
            Visual.SurfaceAppearanceMode ==
            ETGComponentSurfaceAppearanceMode::Textured)
        {
            const auto ValidateTexture =
                [&](const FString& SourcePath,
                    const FString& FieldName,
                    const FString& Label,
                    bool bRequired)
                {
                    FString TrimmedPath = SourcePath;
                    TrimmedPath.TrimStartAndEndInline();

                    if (TrimmedPath.IsEmpty())
                    {
                        if (bRequired)
                        {
                            AddIssue(
                                Report,
                                ETGComponentTreeIssueSeverity::Error,
                                BasePath + TEXT(".") + FieldName,
                                FText::FromString(
                                    Label +
                                    TEXT(
                                        " texture is required in Textured "
                                        "mode.")),
                                Component.ComponentId);
                        }

                        return;
                    }

                    if (!IsSupportedTextureFilePath(TrimmedPath))
                    {
                        AddIssue(
                            Report,
                            ETGComponentTreeIssueSeverity::Error,
                            BasePath + TEXT(".") + FieldName,
                            FText::FromString(
                                Label +
                                TEXT(
                                    " texture must be a PNG, JPG, or JPEG "
                                    "file.")),
                            Component.ComponentId);
                    }

                    if (!FPaths::FileExists(TrimmedPath))
                    {
                        AddIssue(
                            Report,
                            ETGComponentTreeIssueSeverity::Error,
                            BasePath + TEXT(".") + FieldName,
                            FText::FromString(
                                Label + TEXT(" texture file does not exist.")),
                            Component.ComponentId);
                    }
                };

            ValidateTexture(
                Visual.BaseColorTextureFilePath,
                TEXT("BaseColorTextureFilePath"),
                TEXT("Base color"),
                true);

            ValidateTexture(
                Visual.NormalTextureFilePath,
                TEXT("NormalTextureFilePath"),
                TEXT("Normal"),
                false);

            ValidateTexture(
                Visual.RoughnessTextureFilePath,
                TEXT("RoughnessTextureFilePath"),
                TEXT("Roughness"),
                false);

            ValidateTexture(
                Visual.MetallicTextureFilePath,
                TEXT("MetallicTextureFilePath"),
                TEXT("Metallic"),
                false);
        }
        else if (
            Visual.SurfaceAppearanceMode !=
            ETGComponentSurfaceAppearanceMode::SolidColor)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".SurfaceAppearanceMode"),
                FText::FromString(
                    TEXT("Surface appearance mode is invalid.")),
                Component.ComponentId);
        }
    }

    void ValidateJointDof(
        const FTGComponentConfig& Component,
        int32 ComponentIndex,
        const FTGJointDofConfig& Dof,
        int32 DofIndex,
        TSet<FGuid>& UsedDofIds,
        FTGComponentTreeValidationReport& Report)
    {
        const FString BasePath =
            FString::Printf(
                TEXT(
                    "Components[%d].DegreesOfFreedom[%d]"),
                ComponentIndex,
                DofIndex);

        if (!Dof.DofId.IsValid())
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".DofId"),
                FText::FromString(
                    TEXT("Joint DOF identifier is invalid.")),
                Component.ComponentId,
                Dof.DofId);
        }
        else if (UsedDofIds.Contains(Dof.DofId))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".DofId"),
                FText::FromString(
                    TEXT("Joint DOF identifier is duplicated.")),
                Component.ComponentId,
                Dof.DofId);
        }
        else
        {
            UsedDofIds.Add(Dof.DofId);
        }

        FString TrimmedName = Dof.Name;
        TrimmedName.TrimStartAndEndInline();

        if (TrimmedName.IsEmpty())
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".Name"),
                FText::FromString(
                    TEXT("Joint DOF name cannot be empty.")),
                Component.ComponentId,
                Dof.DofId);
        }

        for (int32 EarlierIndex = 0;
             EarlierIndex < DofIndex;
             ++EarlierIndex)
        {
            if (NamesEqual(
                    Component.DegreesOfFreedom[EarlierIndex].Name,
                    Dof.Name))
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".Name"),
                    FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Joint DOF name '%s' is duplicated "
                                "inside this component."),
                            *Dof.Name)),
                    Component.ComponentId,
                    Dof.DofId);

                break;
            }
        }

        if (!IsFiniteVector(Dof.Axis))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".Axis"),
                FText::FromString(
                    TEXT("Joint axis must be finite.")),
                Component.ComponentId,
                Dof.DofId);
        }
        else
        {
            const double AxisLength = Dof.Axis.Length();

            if (AxisLength <= VectorTolerance)
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".Axis"),
                    FText::FromString(
                        TEXT("Joint axis cannot be zero.")),
                    Component.ComponentId,
                    Dof.DofId);
            }
            else if (
                FMath::Abs(AxisLength - 1.0) >
                UnitVectorTolerance)
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Warning,
                    BasePath + TEXT(".Axis"),
                    FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Joint axis length is %.9g. "
                                "Normalize it before simulation."),
                            AxisLength)),
                    Component.ComponentId,
                    Dof.DofId);
            }
        }

        if (!IsFinite(Dof.InitialCoordinate))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".InitialCoordinate"),
                FText::FromString(
                    TEXT("Initial coordinate must be finite.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (!IsFinite(Dof.InitialRate))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".InitialRate"),
                FText::FromString(
                    TEXT("Initial rate must be finite.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (
            !IsFinite(Dof.MaximumAbsoluteRate) ||
            Dof.MaximumAbsoluteRate < 0.0)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".MaximumAbsoluteRate"),
                FText::FromString(
                    TEXT(
                        "Maximum absolute rate must be finite "
                        "and nonnegative.")),
                Component.ComponentId,
                Dof.DofId);
        }
        else if (
            IsFinite(Dof.InitialRate) &&
            FMath::Abs(Dof.InitialRate) >
                Dof.MaximumAbsoluteRate)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".InitialRate"),
                FText::FromString(
                    TEXT(
                        "Initial rate exceeds the maximum "
                        "absolute rate.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (
            !IsFinite(Dof.MaximumAbsoluteEffort) ||
            Dof.MaximumAbsoluteEffort < 0.0)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".MaximumAbsoluteEffort"),
                FText::FromString(
                    TEXT(
                        "Maximum absolute effort must be finite "
                        "and nonnegative.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (
            Dof.bHasMinimumCoordinate &&
            !IsFinite(Dof.MinimumCoordinate))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".MinimumCoordinate"),
                FText::FromString(
                    TEXT("Minimum coordinate must be finite.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (
            Dof.bHasMaximumCoordinate &&
            !IsFinite(Dof.MaximumCoordinate))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".MaximumCoordinate"),
                FText::FromString(
                    TEXT("Maximum coordinate must be finite.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (
            Dof.bHasMinimumCoordinate &&
            Dof.bHasMaximumCoordinate &&
            IsFinite(Dof.MinimumCoordinate) &&
            IsFinite(Dof.MaximumCoordinate) &&
            Dof.MinimumCoordinate > Dof.MaximumCoordinate)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".CoordinateLimits"),
                FText::FromString(
                    TEXT(
                        "Minimum coordinate cannot exceed "
                        "maximum coordinate.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (
            IsFinite(Dof.InitialCoordinate) &&
            Dof.bHasMinimumCoordinate &&
            IsFinite(Dof.MinimumCoordinate) &&
            Dof.InitialCoordinate < Dof.MinimumCoordinate)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".InitialCoordinate"),
                FText::FromString(
                    TEXT(
                        "Initial coordinate is below "
                        "the minimum coordinate.")),
                Component.ComponentId,
                Dof.DofId);
        }

        if (
            IsFinite(Dof.InitialCoordinate) &&
            Dof.bHasMaximumCoordinate &&
            IsFinite(Dof.MaximumCoordinate) &&
            Dof.InitialCoordinate > Dof.MaximumCoordinate)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".InitialCoordinate"),
                FText::FromString(
                    TEXT(
                        "Initial coordinate is above "
                        "the maximum coordinate.")),
                Component.ComponentId,
                Dof.DofId);
        }
    }
}

bool UTGComponentTreeValidationLibrary::
    ValidateSymmetricInertia(
        const FTGSymmetricInertia& Inertia,
        FVector& OutPrincipalMomentsKilogramMetersSquared,
        FText& OutErrorText)
{
    OutPrincipalMomentsKilogramMetersSquared =
        FVector::ZeroVector;

    OutErrorText = FText::GetEmpty();

    if (!TGComponentTreeValidationPrivate::
            ComputePrincipalMoments(
                Inertia,
                OutPrincipalMomentsKilogramMetersSquared))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Every inertia-tensor coefficient must be finite."));

        return false;
    }

    const double LargestMagnitude =
        FMath::Max(
            1.0,
            FMath::Max(
                FMath::Abs(
                    OutPrincipalMomentsKilogramMetersSquared.X),
                FMath::Max(
                    FMath::Abs(
                        OutPrincipalMomentsKilogramMetersSquared.Y),
                    FMath::Abs(
                        OutPrincipalMomentsKilogramMetersSquared.Z))));

    const double NegativeTolerance =
        1.0e-10 * LargestMagnitude;

    if (
        OutPrincipalMomentsKilogramMetersSquared.X <
            -NegativeTolerance ||
        OutPrincipalMomentsKilogramMetersSquared.Y <
            -NegativeTolerance ||
        OutPrincipalMomentsKilogramMetersSquared.Z <
            -NegativeTolerance)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "All principal moments of inertia must "
                "be nonnegative."));

        return false;
    }

    OutPrincipalMomentsKilogramMetersSquared.X = FMath::Max(
        0.0,
        OutPrincipalMomentsKilogramMetersSquared.X);
    OutPrincipalMomentsKilogramMetersSquared.Y = FMath::Max(
        0.0,
        OutPrincipalMomentsKilogramMetersSquared.Y);
    OutPrincipalMomentsKilogramMetersSquared.Z = FMath::Max(
        0.0,
        OutPrincipalMomentsKilogramMetersSquared.Z);

    const double TriangleTolerance =
        1.0e-9 * LargestMagnitude;

    if (
        OutPrincipalMomentsKilogramMetersSquared.Z >
        OutPrincipalMomentsKilogramMetersSquared.X +
        OutPrincipalMomentsKilogramMetersSquared.Y +
        TriangleTolerance)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The principal moments violate the rigid-body "
                "triangle inequality: Imax must not exceed "
                "the sum of the other two principal moments."));

        return false;
    }

    return true;
}

FTGComponentTreeValidationReport
UTGComponentTreeValidationLibrary::
    ValidateScenarioComponentTree(
        const FTGSimulationScenario& Scenario)
{
    using namespace TGComponentTreeValidationPrivate;

    FTGComponentTreeValidationReport Report;

    if (Scenario.Components.IsEmpty())
    {
        AddIssue(
            Report,
            ETGComponentTreeIssueSeverity::Error,
            TEXT("Components"),
            FText::FromString(
                TEXT(
                    "The spacecraft must contain at least "
                    "one physical component.")));

        Report.bIsValid = false;
        Report.Summary = FText::FromString(
            TEXT("Component tree contains 1 error."));

        return Report;
    }

    TSet<FGuid> UsedComponentIds;
    TSet<FGuid> UsedDofIds;
    double TotalInitialMassKilograms = 0.0;
    double MinimumReachableMassKilograms = 0.0;
    bool bCanValidateAggregateMass = true;

    for (
        int32 ComponentIndex = 0;
        ComponentIndex < Scenario.Components.Num();
        ++ComponentIndex)
    {
        const FTGComponentConfig& Component =
            Scenario.Components[ComponentIndex];

        const FString BasePath =
            FString::Printf(
                TEXT("Components[%d]"),
                ComponentIndex);

        if (!Component.ComponentId.IsValid())
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".ComponentId"),
                FText::FromString(
                    TEXT("Component identifier is invalid.")),
                Component.ComponentId);
        }
        else if (UsedComponentIds.Contains(Component.ComponentId))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".ComponentId"),
                FText::FromString(
                    TEXT("Component identifier is duplicated.")),
                Component.ComponentId);
        }
        else
        {
            UsedComponentIds.Add(Component.ComponentId);
        }

        FString TrimmedName = Component.Name;
        TrimmedName.TrimStartAndEndInline();

        if (TrimmedName.IsEmpty())
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".Name"),
                FText::FromString(
                    TEXT("Component name cannot be empty.")),
                Component.ComponentId);
        }

        for (int32 EarlierIndex = 0;
             EarlierIndex < ComponentIndex;
             ++EarlierIndex)
        {
            if (NamesEqual(
                    Scenario.Components[EarlierIndex].Name,
                    Component.Name))
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".Name"),
                    FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Component name '%s' is duplicated."),
                            *Component.Name)),
                    Component.ComponentId);

                break;
            }
        }

        const bool bInitialMassIsValid =
            IsFinite(Component.InitialMassKilograms) &&
            Component.InitialMassKilograms >= 0.0;

        if (!bInitialMassIsValid)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".InitialMassKilograms"),
                FText::FromString(
                    TEXT(
                        "Initial mass must be finite and "
                        "nonnegative.")),
                Component.ComponentId);
        }

        const bool bMinimumMassIsValid =
            IsFinite(Component.MinimumMassKilograms) &&
            Component.MinimumMassKilograms >= 0.0;

        if (!bMinimumMassIsValid)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".MinimumMassKilograms"),
                FText::FromString(
                    TEXT(
                        "Minimum mass must be finite "
                        "and nonnegative.")),
                Component.ComponentId);
        }
        const bool bMassRangeIsValid =
            bInitialMassIsValid &&
            bMinimumMassIsValid &&
            Component.MinimumMassKilograms <=
                Component.InitialMassKilograms;

        if (
            bInitialMassIsValid &&
            bMinimumMassIsValid &&
            Component.MinimumMassKilograms >
                Component.InitialMassKilograms)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".MinimumMassKilograms"),
                FText::FromString(
                    TEXT(
                        "Minimum mass cannot exceed "
                        "initial mass.")),
                Component.ComponentId);
        }

        if (bMassRangeIsValid)
        {
            TotalInitialMassKilograms +=
                Component.InitialMassKilograms;

            MinimumReachableMassKilograms +=
                Component.bVariableMass
                ? Component.MinimumMassKilograms
                : Component.InitialMassKilograms;
        }
        else
        {
            bCanValidateAggregateMass = false;
        }

        if (
            !Component.bVariableMass &&
            IsFinite(Component.InitialMassKilograms) &&
            IsFinite(Component.MinimumMassKilograms) &&
            !FMath::IsNearlyEqual(
                Component.InitialMassKilograms,
                Component.MinimumMassKilograms))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Warning,
                BasePath + TEXT(".MinimumMassKilograms"),
                FText::FromString(
                    TEXT(
                        "Minimum mass differs from initial mass "
                        "although Variable Mass is disabled.")),
                Component.ComponentId);
        }

        if (!IsFiniteVector(Component.LocalCenterOfMassMeters))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".LocalCenterOfMassMeters"),
                FText::FromString(
                    TEXT(
                        "Local center of mass must be finite.")),
                Component.ComponentId);
        }

        FVector PrincipalMoments;
        FText InertiaError;

        if (!ValidateSymmetricInertia(
                Component.CentroidalInertia,
                PrincipalMoments,
                InertiaError))
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                BasePath + TEXT(".CentroidalInertia"),
                InertiaError,
                Component.ComponentId);
        }

        if (ComponentIndex == 0)
        {
            FString ParentName =
                Component.ParentComponentName;

            ParentName.TrimStartAndEndInline();

            if (!ParentName.IsEmpty())
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".ParentComponentName"),
                    FText::FromString(
                        TEXT(
                            "The main component cannot "
                            "have a parent.")),
                    Component.ComponentId);
            }

            if (!Component.DegreesOfFreedom.IsEmpty())
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".DegreesOfFreedom"),
                    FText::FromString(
                        TEXT(
                            "The main component cannot have "
                            "parent-joint degrees of freedom.")),
                    Component.ComponentId);
            }

            if (!IsFiniteVector(Component.OriginInBodyMeters))
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".OriginInBodyMeters"),
                    FText::FromString(
                        TEXT(
                            "Main-component origin must be finite.")),
                    Component.ComponentId);
            }

            ValidateQuaternion(
                Component.ComponentToBodyOrientation,
                BasePath +
                    TEXT(".ComponentToBodyOrientation"),
                Component.ComponentId,
                Report);
        }
        else
        {
            FString ParentName =
                Component.ParentComponentName;

            ParentName.TrimStartAndEndInline();

            if (ParentName.IsEmpty())
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".ParentComponentName"),
                    FText::FromString(
                        TEXT(
                            "Every child component requires "
                            "a parent component.")),
                    Component.ComponentId);
            }
            else
            {
                const int32 ParentIndex =
                    FindComponentIndexByName(
                        Scenario.Components,
                        ParentName);

                if (ParentIndex == INDEX_NONE)
                {
                    AddIssue(
                        Report,
                        ETGComponentTreeIssueSeverity::Error,
                        BasePath +
                            TEXT(".ParentComponentName"),
                        FText::FromString(
                            FString::Printf(
                                TEXT(
                                    "Parent component '%s' "
                                    "does not exist."),
                                *ParentName)),
                        Component.ComponentId);
                }
                else if (ParentIndex >= ComponentIndex)
                {
                    AddIssue(
                        Report,
                        ETGComponentTreeIssueSeverity::Error,
                        BasePath +
                            TEXT(".ParentComponentName"),
                        FText::FromString(
                            TEXT(
                                "A child must reference a parent "
                                "appearing earlier in the component "
                                "array.")),
                        Component.ComponentId);
                }
            }

            if (!IsFiniteVector(Component.ParentAnchorMeters))
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".ParentAnchorMeters"),
                    FText::FromString(
                        TEXT("Parent anchor must be finite.")),
                    Component.ComponentId);
            }

            if (!IsFiniteVector(Component.ChildAnchorMeters))
            {
                AddIssue(
                    Report,
                    ETGComponentTreeIssueSeverity::Error,
                    BasePath + TEXT(".ChildAnchorMeters"),
                    FText::FromString(
                        TEXT("Child anchor must be finite.")),
                    Component.ComponentId);
            }

            ValidateQuaternion(
                Component.ChildToParentZeroOrientation,
                BasePath +
                    TEXT(".ChildToParentZeroOrientation"),
                Component.ComponentId,
                Report);

            for (
                int32 DofIndex = 0;
                DofIndex <
                    Component.DegreesOfFreedom.Num();
                ++DofIndex)
            {
                ValidateJointDof(
                    Component,
                    ComponentIndex,
                    Component.DegreesOfFreedom[DofIndex],
                    DofIndex,
                    UsedDofIds,
                    Report);
            }
        }

        ValidateVisualConfig(
            Component,
            ComponentIndex,
            Report);
    }

    if (bCanValidateAggregateMass)
    {
        if (
            !IsFinite(TotalInitialMassKilograms) ||
            TotalInitialMassKilograms <= 0.0)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                TEXT("Components.TotalInitialMassKilograms"),
                FText::FromString(
                    TEXT(
                        "Total initial spacecraft mass must be finite "
                        "and greater than zero.")));
        }

        if (
            !IsFinite(MinimumReachableMassKilograms) ||
            MinimumReachableMassKilograms <= 0.0)
        {
            AddIssue(
                Report,
                ETGComponentTreeIssueSeverity::Error,
                TEXT("Components.MinimumReachableMassKilograms"),
                FText::FromString(
                    TEXT(
                        "Minimum reachable spacecraft mass must be finite "
                        "and greater than zero.")));
        }
    }

    Report.bIsValid = Report.ErrorCount == 0;

    Report.Summary = FText::FromString(
        FString::Printf(
            TEXT(
                "Component tree: %d error(s), %d warning(s)."),
            Report.ErrorCount,
            Report.WarningCount));

    return Report;
}

FText UTGComponentTreeValidationLibrary::
    FormatComponentTreeValidationReportForHud(
        const FTGComponentTreeValidationReport& Report,
        int32 MaximumIssues)
{
    const int32 SafeMaximum =
        FMath::Max(MaximumIssues, 0);

    FString Formatted =
        Report.Summary.ToString();

    const int32 IssueCountToShow =
        FMath::Min(
            SafeMaximum,
            Report.Issues.Num());

    for (int32 Index = 0;
         Index < IssueCountToShow;
         ++Index)
    {
        const FTGComponentTreeValidationIssue& Issue =
            Report.Issues[Index];

        const TCHAR* SeverityPrefix =
            Issue.Severity ==
                ETGComponentTreeIssueSeverity::Error
            ? TEXT("[ERROR]")
            : TEXT("[WARNING]");

        Formatted += FString::Printf(
            TEXT("\n%s %s: %s"),
            SeverityPrefix,
            *Issue.Path,
            *Issue.Message.ToString());
    }

    if (IssueCountToShow < Report.Issues.Num())
    {
        Formatted += FString::Printf(
            TEXT(
                "\n... %d additional issue(s) not shown."),
            Report.Issues.Num() - IssueCountToShow);
    }

    return FText::FromString(Formatted);
}
