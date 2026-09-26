// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"

#include "HAL/FileManager.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Simulation/ComponentTree/TGComponentTreeEditingLibrary.h"
#include "Simulation/TGCelestialCatalogLibrary.h"

namespace TGSolarRadiationPressureEditingPrivate
{
    constexpr double OpticalSumTolerance = 1.0e-6;
    constexpr double MinimumFacetCrossMagnitude = 1.0e-18;
    constexpr int32 MaximumEditableTriangleCount = 32;
    constexpr int32 PerformanceWarningTriangleCount = 2000;
    constexpr int32 MaximumGeneratedTriangleCount = 200000;
    constexpr int32 MaximumStlTriangleCount = 2000000;

    struct FSourceTriangle
    {
        FVector A = FVector::ZeroVector;
        FVector B = FVector::ZeroVector;
        FVector C = FVector::ZeroVector;
    };

    uint32 ReadLittleEndianUint32(const uint8* Data)
    {
        return
            static_cast<uint32>(Data[0]) |
            (static_cast<uint32>(Data[1]) << 8) |
            (static_cast<uint32>(Data[2]) << 16) |
            (static_cast<uint32>(Data[3]) << 24);
    }

    float ReadLittleEndianFloat(const uint8* Data)
    {
        const uint32 Bits = ReadLittleEndianUint32(Data);
        float Value = 0.0f;
        FMemory::Memcpy(&Value, &Bits, sizeof(float));
        return Value;
    }

    bool NamesEqual(const FString& First, const FString& Second)
    {
        return First.Equals(Second, ESearchCase::IgnoreCase);
    }

    bool IsFiniteVector(const FVector& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    bool ParseBinaryStl(
        const TArray<uint8>& Bytes,
        TArray<FSourceTriangle>& OutTriangles,
        FText& OutError)
    {
        OutTriangles.Reset();
        if (Bytes.Num() < 84)
        {
            return false;
        }

        const uint32 TriangleCount =
            ReadLittleEndianUint32(Bytes.GetData() + 80);
        const uint64 ExpectedSize =
            84ull + static_cast<uint64>(TriangleCount) * 50ull;
        if (ExpectedSize != static_cast<uint64>(Bytes.Num()))
        {
            return false;
        }
        if (TriangleCount == 0)
        {
            OutError = FText::FromString(TEXT("The STL file contains no triangles."));
            return true;
        }
        if (TriangleCount > static_cast<uint32>(MaximumStlTriangleCount))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("The STL file contains %u triangles; the supported limit is %d."),
                TriangleCount,
                MaximumStlTriangleCount));
            return true;
        }

        OutTriangles.Reserve(static_cast<int32>(TriangleCount));
        const uint8* Cursor = Bytes.GetData() + 84;
        for (uint32 TriangleIndex = 0;
             TriangleIndex < TriangleCount;
             ++TriangleIndex)
        {
            Cursor += 12;
            FSourceTriangle Triangle;
            FVector* Vertices[] = {&Triangle.A, &Triangle.B, &Triangle.C};
            for (FVector* Vertex : Vertices)
            {
                *Vertex = FVector(
                    static_cast<double>(ReadLittleEndianFloat(Cursor)),
                    static_cast<double>(ReadLittleEndianFloat(Cursor + 4)),
                    static_cast<double>(ReadLittleEndianFloat(Cursor + 8)));
                Cursor += 12;
                if (!IsFiniteVector(*Vertex))
                {
                    OutTriangles.Reset();
                    OutError = FText::FromString(FString::Printf(
                        TEXT("STL triangle %u contains a non-finite vertex."),
                        TriangleIndex));
                    return true;
                }
            }
            Cursor += 2;
            OutTriangles.Add(Triangle);
        }
        return true;
    }

    bool ParseAsciiStl(
        const FString& FilePath,
        TArray<FSourceTriangle>& OutTriangles,
        FText& OutError)
    {
        OutTriangles.Reset();
        FString Contents;
        if (!FFileHelper::LoadFileToString(Contents, *FilePath))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("The STL file could not be read: %s"),
                *FilePath));
            return false;
        }

        TArray<FString> Lines;
        Contents.ParseIntoArrayLines(Lines, false);
        TArray<FVector> PendingVertices;
        PendingVertices.Reserve(3);
        for (const FString& SourceLine : Lines)
        {
            FString Line = SourceLine;
            Line.TrimStartAndEndInline();
            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);
            if (Tokens.Num() != 4 ||
                !Tokens[0].Equals(TEXT("vertex"), ESearchCase::IgnoreCase))
            {
                continue;
            }

            double X = 0.0;
            double Y = 0.0;
            double Z = 0.0;
            if (!FDefaultValueHelper::ParseDouble(Tokens[1], X) ||
                !FDefaultValueHelper::ParseDouble(Tokens[2], Y) ||
                !FDefaultValueHelper::ParseDouble(Tokens[3], Z) ||
                !IsFiniteVector(FVector(X, Y, Z)))
            {
                OutError = FText::FromString(
                    TEXT("The STL file contains an invalid vertex coordinate."));
                return false;
            }

            PendingVertices.Add(FVector(X, Y, Z));
            if (PendingVertices.Num() == 3)
            {
                OutTriangles.Add({
                    PendingVertices[0],
                    PendingVertices[1],
                    PendingVertices[2]});
                PendingVertices.Reset();
                if (OutTriangles.Num() > MaximumStlTriangleCount)
                {
                    OutTriangles.Reset();
                    OutError = FText::FromString(FString::Printf(
                        TEXT("The STL file exceeds the supported limit of %d triangles."),
                        MaximumStlTriangleCount));
                    return false;
                }
            }
        }

        if (!PendingVertices.IsEmpty())
        {
            OutTriangles.Reset();
            OutError = FText::FromString(
                TEXT("The STL file ends with an incomplete triangle."));
            return false;
        }
        if (OutTriangles.IsEmpty())
        {
            OutError = FText::FromString(
                TEXT("The STL file contains no readable triangles."));
            return false;
        }
        return true;
    }

    bool LoadStlTriangles(
        const FString& FilePath,
        TArray<FSourceTriangle>& OutTriangles,
        FText& OutError)
    {
        OutTriangles.Reset();
        OutError = FText::GetEmpty();
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *FilePath))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("The STL file could not be read: %s"),
                *FilePath));
            return false;
        }

        FText BinaryMessage;
        if (ParseBinaryStl(Bytes, OutTriangles, BinaryMessage))
        {
            if (!BinaryMessage.IsEmpty())
            {
                OutTriangles.Reset();
                OutError = BinaryMessage;
                return false;
            }
            return !OutTriangles.IsEmpty();
        }
        return ParseAsciiStl(FilePath, OutTriangles, OutError);
    }

    double StlUnitsToMeters(ETGStlLengthUnit Unit)
    {
        switch (Unit)
        {
            case ETGStlLengthUnit::Millimeters: return 0.001;
            case ETGStlLengthUnit::Centimeters: return 0.01;
            case ETGStlLengthUnit::Meters: return 1.0;
        }
        return 0.0;
    }

    bool ValidateVisualTransform(
        const FTGComponentVisualConfig& Visual,
        FText& OutError)
    {
        if (!IsFiniteVector(Visual.VisualOffsetMeters) ||
            !IsFiniteVector(Visual.VisualScale) ||
            Visual.VisualOrientation.ContainsNaN() ||
            !FMath::IsFinite(Visual.VisualOrientation.X) ||
            !FMath::IsFinite(Visual.VisualOrientation.Y) ||
            !FMath::IsFinite(Visual.VisualOrientation.Z) ||
            !FMath::IsFinite(Visual.VisualOrientation.W))
        {
            OutError = FText::FromString(
                TEXT("The component visual transform contains a non-finite value."));
            return false;
        }
        if (FMath::IsNearlyZero(Visual.VisualScale.X) ||
            FMath::IsNearlyZero(Visual.VisualScale.Y) ||
            FMath::IsNearlyZero(Visual.VisualScale.Z))
        {
            OutError = FText::FromString(
                TEXT("The component visual scale must be nonzero on every axis."));
            return false;
        }
        return true;
    }

    FVector TransformGeometryVertex(
        const FTGComponentVisualConfig& Visual,
        const FVector& Vertex)
    {
        const FVector Scaled(
            Vertex.X * Visual.VisualScale.X,
            Vertex.Y * Visual.VisualScale.Y,
            Vertex.Z * Visual.VisualScale.Z);
        FQuat Orientation = Visual.VisualOrientation;
        Orientation.Normalize();
        return Visual.VisualOffsetMeters + Orientation.RotateVector(Scaled);
    }

    bool AppendGeneratedTriangle(
        const FTGComponentConfig& Component,
        const FVector& A,
        const FVector& B,
        const FVector& C,
        ETGSrpLogicalRegion Region,
        TArray<FTGOpticalFacetConfig>& OutTriangles,
        FText& OutError)
    {
        FVector TransformedA = TransformGeometryVertex(Component.Visual, A);
        FVector TransformedB = TransformGeometryVertex(Component.Visual, B);
        FVector TransformedC = TransformGeometryVertex(Component.Visual, C);
        if (Component.Visual.VisualScale.X *
                Component.Visual.VisualScale.Y *
                Component.Visual.VisualScale.Z < 0.0)
        {
            Swap(TransformedB, TransformedC);
        }

        const double CrossMagnitude = FVector::CrossProduct(
            TransformedB - TransformedA,
            TransformedC - TransformedA).Size();
        if (!FMath::IsFinite(CrossMagnitude))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' produces a non-finite SRP triangle."),
                *Component.Name));
            return false;
        }
        if (CrossMagnitude <= MinimumFacetCrossMagnitude)
        {
            return true;
        }
        if (OutTriangles.Num() >= MaximumGeneratedTriangleCount)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' exceeds the supported SRP geometry limit of %d triangles."),
                *Component.Name,
                MaximumGeneratedTriangleCount));
            return false;
        }

        FTGOpticalFacetConfig Facet;
        Facet.StableTriangleIndex = OutTriangles.Num();
        Facet.LogicalRegion = Region;
        Facet.Vertex0Meters = TransformedA;
        Facet.Vertex1Meters = TransformedB;
        Facet.Vertex2Meters = TransformedC;
        OutTriangles.Add(MoveTemp(Facet));
        return true;
    }

    bool AppendQuad(
        const FTGComponentConfig& Component,
        const FVector& A,
        const FVector& B,
        const FVector& C,
        const FVector& D,
        ETGSrpLogicalRegion Region,
        TArray<FTGOpticalFacetConfig>& OutTriangles,
        FText& OutError)
    {
        return
            AppendGeneratedTriangle(
                Component, A, B, C, Region, OutTriangles, OutError) &&
            AppendGeneratedTriangle(
                Component, A, C, D, Region, OutTriangles, OutError);
    }

    bool BuildBoxProxy(
        const FTGComponentConfig& Component,
        TArray<FTGOpticalFacetConfig>& OutTriangles,
        FText& OutError)
    {
        const FVector Dimensions = Component.Visual.BoxDimensionsMeters;
        if (!IsFiniteVector(Dimensions) ||
            Dimensions.X <= 0.0 ||
            Dimensions.Y <= 0.0 ||
            Dimensions.Z <= 0.0)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' requires positive finite box dimensions."),
                *Component.Name));
            return false;
        }
        const double X = 0.5 * Dimensions.X;
        const double Y = 0.5 * Dimensions.Y;
        const double Z = 0.5 * Dimensions.Z;
        return
            AppendQuad(Component, { X,-Y,-Z}, { X, Y,-Z}, { X, Y, Z}, { X,-Y, Z}, ETGSrpLogicalRegion::BoxPositiveX, OutTriangles, OutError) &&
            AppendQuad(Component, {-X, Y,-Z}, {-X,-Y,-Z}, {-X,-Y, Z}, {-X, Y, Z}, ETGSrpLogicalRegion::BoxNegativeX, OutTriangles, OutError) &&
            AppendQuad(Component, {-X, Y,-Z}, {-X, Y, Z}, { X, Y, Z}, { X, Y,-Z}, ETGSrpLogicalRegion::BoxPositiveY, OutTriangles, OutError) &&
            AppendQuad(Component, {-X,-Y, Z}, {-X,-Y,-Z}, { X,-Y,-Z}, { X,-Y, Z}, ETGSrpLogicalRegion::BoxNegativeY, OutTriangles, OutError) &&
            AppendQuad(Component, {-X,-Y, Z}, { X,-Y, Z}, { X, Y, Z}, {-X, Y, Z}, ETGSrpLogicalRegion::BoxPositiveZ, OutTriangles, OutError) &&
            AppendQuad(Component, {-X, Y,-Z}, { X, Y,-Z}, { X,-Y,-Z}, {-X,-Y,-Z}, ETGSrpLogicalRegion::BoxNegativeZ, OutTriangles, OutError);
    }

    int32 RequestedPrimitiveTriangleCount(const FTGComponentSrpConfig& Config)
    {
        return Config.ProxyResolutionMode ==
                ETGSrpProxyResolutionMode::CustomTargetTriangleCount
            ? Config.CustomTargetTriangleCount
            : 200;
    }

    bool BuildSphereProxy(
        const FTGComponentConfig& Component,
        TArray<FTGOpticalFacetConfig>& OutTriangles,
        FText& OutError)
    {
        const double Radius = Component.Visual.SphereRadiusMeters;
        if (!FMath::IsFinite(Radius) || Radius <= 0.0)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' requires a positive finite sphere radius."),
                *Component.Name));
            return false;
        }
        const int32 Target = RequestedPrimitiveTriangleCount(
            Component.SolarRadiationPressure);
        if (Target <= 0 || Target > MaximumGeneratedTriangleCount)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' requests an unsupported SRP triangle count."),
                *Component.Name));
            return false;
        }

        const int32 LongitudeSegments = FMath::Max(
            3,
            FMath::RoundToInt(FMath::Sqrt(static_cast<double>(Target))));
        const int32 LatitudeBands = FMath::Max(
            2,
            FMath::RoundToInt(
                static_cast<double>(Target) /
                (2.0 * LongitudeSegments)) + 1);

        TArray<TArray<FVector>> Rings;
        Rings.SetNum(LatitudeBands - 1);
        for (int32 Latitude = 1; Latitude < LatitudeBands; ++Latitude)
        {
            const double Theta = UE_PI * Latitude / LatitudeBands;
            TArray<FVector>& Ring = Rings[Latitude - 1];
            Ring.Reserve(LongitudeSegments);
            for (int32 Longitude = 0;
                 Longitude < LongitudeSegments;
                 ++Longitude)
            {
                const double Phi = 2.0 * UE_PI * Longitude /
                    LongitudeSegments;
                Ring.Add(FVector(
                    Radius * FMath::Sin(Theta) * FMath::Cos(Phi),
                    Radius * FMath::Sin(Theta) * FMath::Sin(Phi),
                    Radius * FMath::Cos(Theta)));
            }
        }

        const FVector Top(0.0, 0.0, Radius);
        const FVector Bottom(0.0, 0.0, -Radius);
        for (int32 Longitude = 0;
             Longitude < LongitudeSegments;
             ++Longitude)
        {
            const int32 Next = (Longitude + 1) % LongitudeSegments;
            if (!AppendGeneratedTriangle(
                    Component,
                    Top,
                    Rings[0][Longitude],
                    Rings[0][Next],
                    ETGSrpLogicalRegion::None,
                    OutTriangles,
                    OutError))
            {
                return false;
            }
            for (int32 RingIndex = 0;
                 RingIndex + 1 < Rings.Num();
                 ++RingIndex)
            {
                if (!AppendQuad(
                        Component,
                        Rings[RingIndex][Longitude],
                        Rings[RingIndex + 1][Longitude],
                        Rings[RingIndex + 1][Next],
                        Rings[RingIndex][Next],
                        ETGSrpLogicalRegion::None,
                        OutTriangles,
                        OutError))
                {
                    return false;
                }
            }
            if (!AppendGeneratedTriangle(
                    Component,
                    Bottom,
                    Rings.Last()[Next],
                    Rings.Last()[Longitude],
                    ETGSrpLogicalRegion::None,
                    OutTriangles,
                    OutError))
            {
                return false;
            }
        }
        return true;
    }

    bool BuildCylinderProxy(
        const FTGComponentConfig& Component,
        TArray<FTGOpticalFacetConfig>& OutTriangles,
        FText& OutError)
    {
        const double Radius = Component.Visual.CylinderRadiusMeters;
        const double Length = Component.Visual.CylinderLengthMeters;
        if (!FMath::IsFinite(Radius) || Radius <= 0.0 ||
            !FMath::IsFinite(Length) || Length <= 0.0)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' requires a positive finite cylinder radius and length."),
                *Component.Name));
            return false;
        }
        const int32 Target = RequestedPrimitiveTriangleCount(
            Component.SolarRadiationPressure);
        if (Target <= 0 || Target > MaximumGeneratedTriangleCount)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' requests an unsupported SRP triangle count."),
                *Component.Name));
            return false;
        }
        const int32 Segments = FMath::Max(3, FMath::RoundToInt(Target / 4.0));
        const double HalfLength = 0.5 * Length;
        for (int32 Segment = 0; Segment < Segments; ++Segment)
        {
            const int32 Next = (Segment + 1) % Segments;
            const double Phi0 = 2.0 * UE_PI * Segment / Segments;
            const double Phi1 = 2.0 * UE_PI * Next / Segments;
            const FVector Lower0(
                Radius * FMath::Cos(Phi0),
                Radius * FMath::Sin(Phi0),
                -HalfLength);
            const FVector Lower1(
                Radius * FMath::Cos(Phi1),
                Radius * FMath::Sin(Phi1),
                -HalfLength);
            const FVector Upper0(Lower0.X, Lower0.Y, HalfLength);
            const FVector Upper1(Lower1.X, Lower1.Y, HalfLength);
            if (!AppendQuad(
                    Component,
                    Lower0,
                    Lower1,
                    Upper1,
                    Upper0,
                    ETGSrpLogicalRegion::CylinderSide,
                    OutTriangles,
                    OutError) ||
                !AppendGeneratedTriangle(
                    Component,
                    FVector(0.0, 0.0, HalfLength),
                    Upper0,
                    Upper1,
                    ETGSrpLogicalRegion::CylinderPositiveCap,
                    OutTriangles,
                    OutError) ||
                !AppendGeneratedTriangle(
                    Component,
                    FVector(0.0, 0.0, -HalfLength),
                    Lower1,
                    Lower0,
                    ETGSrpLogicalRegion::CylinderNegativeCap,
                    OutTriangles,
                    OutError))
            {
                return false;
            }
        }
        return true;
    }

    bool BuildStlProxy(
        const FTGComponentConfig& Component,
        TArray<FTGOpticalFacetConfig>& OutTriangles,
        FText& OutWarning,
        FText& OutError)
    {
        FString FilePath = Component.Visual.StlFilePath.TrimStartAndEnd();
        FPaths::NormalizeFilename(FilePath);
        if (FilePath.IsEmpty() || !FPaths::FileExists(FilePath))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("The STL file for component '%s' does not exist."),
                *Component.Name));
            return false;
        }
        const double Units = StlUnitsToMeters(Component.Visual.StlLengthUnit);
        if (!(Units > 0.0))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' has an invalid STL length unit."),
                *Component.Name));
            return false;
        }

        TArray<FSourceTriangle> SourceTriangles;
        if (!LoadStlTriangles(FilePath, SourceTriangles, OutError))
        {
            return false;
        }
        for (const FSourceTriangle& Source : SourceTriangles)
        {
            if (!AppendGeneratedTriangle(
                    Component,
                    Units * Source.A,
                    Units * Source.B,
                    Units * Source.C,
                    ETGSrpLogicalRegion::None,
                    OutTriangles,
                    OutError))
            {
                return false;
            }
        }
        if (OutTriangles.IsEmpty())
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("The STL file for component '%s' contains no non-degenerate triangles."),
                *Component.Name));
            return false;
        }

        const FTGComponentSrpConfig& Config =
            Component.SolarRadiationPressure;
        if (Config.ProxyResolutionMode ==
                ETGSrpProxyResolutionMode::CustomTargetTriangleCount &&
            Config.CustomTargetTriangleCount != OutTriangles.Num())
        {
            OutWarning = FText::FromString(FString::Printf(
                TEXT("Component '%s' preserves all %d STL triangles; its requested target of %d is not applied because removing arbitrary STL triangles would open the SRP surface."),
                *Component.Name,
                OutTriangles.Num(),
                Config.CustomTargetTriangleCount));
        }
        return true;
    }

    bool BuildComponentProxy(
        const FTGComponentConfig& Component,
        TArray<FTGOpticalFacetConfig>& OutTriangles,
        FText& OutWarning,
        FText& OutError)
    {
        OutTriangles.Reset();
        OutWarning = FText::GetEmpty();
        OutError = FText::GetEmpty();
        if (!ValidateVisualTransform(Component.Visual, OutError))
        {
            return false;
        }
        if (Component.Visual.GeometrySource ==
            ETGComponentGeometrySource::NoGeometry)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Component '%s' has no surface geometry for solar radiation pressure."),
                *Component.Name));
            return false;
        }
        if (Component.Visual.GeometrySource ==
            ETGComponentGeometrySource::CustomStl)
        {
            return BuildStlProxy(
                Component, OutTriangles, OutWarning, OutError);
        }
        switch (Component.Visual.PrimitiveType)
        {
            case ETGPrimitiveGeometryType::Box:
                return BuildBoxProxy(Component, OutTriangles, OutError);
            case ETGPrimitiveGeometryType::Sphere:
                return BuildSphereProxy(Component, OutTriangles, OutError);
            case ETGPrimitiveGeometryType::Cylinder:
                return BuildCylinderProxy(Component, OutTriangles, OutError);
        }
        OutError = FText::FromString(FString::Printf(
            TEXT("Component '%s' uses an unsupported surface geometry."),
            *Component.Name));
        return false;
    }

    FTGComponentConfig* FindComponent(
        FTGSimulationScenario& Scenario,
        const FGuid& ComponentId)
    {
        return Scenario.Components.FindByPredicate(
            [&ComponentId](const FTGComponentConfig& Component)
            {
                return Component.ComponentId == ComponentId;
            });
    }

    const FTGComponentConfig* FindComponent(
        const FTGSimulationScenario& Scenario,
        const FGuid& ComponentId)
    {
        return Scenario.Components.FindByPredicate(
            [&ComponentId](const FTGComponentConfig& Component)
            {
                return Component.ComponentId == ComponentId;
            });
    }

    FTGComponentConfig* FindComponentByName(
        FTGSimulationScenario& Scenario,
        const FString& ComponentName)
    {
        return Scenario.Components.FindByPredicate(
            [&ComponentName](const FTGComponentConfig& Component)
            {
                return NamesEqual(Component.Name, ComponentName);
            });
    }

    bool IsFacetLogicalRegionSupported(
        const FTGComponentConfig& Component,
        ETGSrpLogicalRegion Region)
    {
        if (
            Component.Visual.GeometrySource !=
            ETGComponentGeometrySource::Primitive)
        {
            return Region == ETGSrpLogicalRegion::None;
        }

        switch (Component.Visual.PrimitiveType)
        {
            case ETGPrimitiveGeometryType::Box:
                return
                    Region >= ETGSrpLogicalRegion::BoxPositiveX &&
                    Region <= ETGSrpLogicalRegion::BoxNegativeZ;

            case ETGPrimitiveGeometryType::Cylinder:
                return
                    Region >= ETGSrpLogicalRegion::CylinderSide &&
                    Region <= ETGSrpLogicalRegion::CylinderNegativeCap;

            case ETGPrimitiveGeometryType::Sphere:
                return Region == ETGSrpLogicalRegion::None;
        }

        return false;
    }

    bool IsLogicalRegionOverrideSupported(
        const FTGComponentConfig& Component,
        ETGSrpLogicalRegion Region)
    {
        return
            Region != ETGSrpLogicalRegion::None &&
            IsFacetLogicalRegionSupported(Component, Region);
    }

    const FTGSrpLogicalRegionOverride* FindRegionOverride(
        const FTGComponentSrpConfig& Config,
        ETGSrpLogicalRegion Region)
    {
        return Config.LogicalRegionOverrides.FindByPredicate(
            [Region](const FTGSrpLogicalRegionOverride& Override)
            {
                return Override.Region == Region;
            });
    }

    const FTGSrpTriangleOverride* FindTriangleOverride(
        const FTGComponentSrpConfig& Config,
        int32 StableTriangleIndex)
    {
        return Config.TriangleOverrides.FindByPredicate(
            [StableTriangleIndex](const FTGSrpTriangleOverride& Override)
            {
                return Override.ProxyTriangleIndex == StableTriangleIndex;
            });
    }

    FTGSrpOpticalProperties ResolveOpticalProperties(
        const FTGSimulationScenario& Scenario,
        const FTGComponentConfig& Component,
        const FTGOpticalFacetConfig& Facet)
    {
        const FTGComponentSrpConfig& Config =
            Component.SolarRadiationPressure;

        FTGSrpOpticalProperties Result =
            Config.bUseGlobalFallbackOpticalProperties
                ? Scenario.SolarRadiationPressure
                    .GlobalFallbackOpticalProperties
                : Config.ComponentOpticalProperties;

        const bool bSphereUsesWholeComponentOnly =
            Component.Visual.GeometrySource ==
                ETGComponentGeometrySource::Primitive &&
            Component.Visual.PrimitiveType ==
                ETGPrimitiveGeometryType::Sphere;

        if (
            !Config.bApplyOneOpticalConfigurationToEntireComponent &&
            !bSphereUsesWholeComponentOnly)
        {
            if (
                const FTGSrpLogicalRegionOverride* RegionOverride =
                    FindRegionOverride(Config, Facet.LogicalRegion))
            {
                Result = RegionOverride->OpticalProperties;
            }

            if (
                Config.GeneratedTriangleCount <=
                    MaximumEditableTriangleCount)
            {
                if (
                    const FTGSrpTriangleOverride* TriangleOverride =
                        FindTriangleOverride(
                            Config,
                            Facet.StableTriangleIndex))
                {
                    Result = TriangleOverride->OpticalProperties;
                }
            }
        }

        return Result;
    }

    bool ResolveComponentOpticalPropertiesInternal(
        FTGSimulationScenario& Scenario,
        FTGComponentConfig& Component,
        FText& OutError)
    {
        OutError = FText::GetEmpty();

        FString InvalidReason;
        const FTGComponentSrpConfig& Config =
            Component.SolarRadiationPressure;

        const FTGSrpOpticalProperties& BaseProperties =
            Config.bUseGlobalFallbackOpticalProperties
                ? Scenario.SolarRadiationPressure
                    .GlobalFallbackOpticalProperties
                : Config.ComponentOpticalProperties;

        if (!UTGSolarRadiationPressureEditingLibrary::
                IsValidOpticalProperties(
                    BaseProperties,
                    InvalidReason))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT("Component '%s' cannot resolve SRP optics: %s"),
                    *Component.Name,
                    *InvalidReason));

            return false;
        }

        if (!Config.bApplyOneOpticalConfigurationToEntireComponent)
        {
            for (const FTGSrpLogicalRegionOverride& Override
                 : Config.LogicalRegionOverrides)
            {
                if (!IsLogicalRegionOverrideSupported(
                        Component,
                        Override.Region))
                {
                    OutError = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Component '%s' has an override for a logical "
                                "region its geometry does not support."),
                            *Component.Name));
                    return false;
                }

                if (!UTGSolarRadiationPressureEditingLibrary::
                        IsValidOpticalProperties(
                            Override.OpticalProperties,
                            InvalidReason))
                {
                    OutError = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Component '%s' has an invalid logical-region "
                                "override: %s"),
                            *Component.Name,
                            *InvalidReason));

                    return false;
                }
            }

            const bool bSphereUsesWholeComponentOnly =
                Component.Visual.GeometrySource ==
                    ETGComponentGeometrySource::Primitive &&
                Component.Visual.PrimitiveType ==
                    ETGPrimitiveGeometryType::Sphere;

            if (
                bSphereUsesWholeComponentOnly &&
                !Config.TriangleOverrides.IsEmpty())
            {
                OutError = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Component '%s' is a sphere and cannot use "
                            "triangle overrides."),
                        *Component.Name));
                return false;
            }

            if (
                Config.GeneratedTriangleCount > MaximumEditableTriangleCount &&
                !Config.TriangleOverrides.IsEmpty())
            {
                OutError = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Component '%s' has more than 32 generated "
                            "triangles and cannot use triangle overrides."),
                        *Component.Name));
                return false;
            }

            for (const FTGSrpTriangleOverride& Override
                 : Config.TriangleOverrides)
            {
                if (!UTGSolarRadiationPressureEditingLibrary::
                        IsValidOpticalProperties(
                            Override.OpticalProperties,
                            InvalidReason))
                {
                    OutError = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Component '%s' has an invalid triangle "
                                "override: %s"),
                            *Component.Name,
                            *InvalidReason));

                    return false;
                }
            }
        }

        for (FTGOpticalFacetConfig& Facet
             : Scenario.SolarRadiationPressure.OpticalFacets)
        {
            if (Facet.ComponentId != Component.ComponentId)
            {
                continue;
            }

            const FTGSrpOpticalProperties Resolved =
                ResolveOpticalProperties(
                    Scenario,
                    Component,
                    Facet);

            Facet.AbsorptionFraction =
                Resolved.AbsorptionFraction;
            Facet.SpecularReflectionFraction =
                Resolved.SpecularReflectionFraction;
            Facet.DiffuseReflectionFraction =
                Resolved.DiffuseReflectionFraction;
        }

        return true;
    }

    FString JoinMessages(const TArray<FString>& Messages)
    {
        return FString::Join(Messages, TEXT("\n"));
    }
}

void UTGSolarRadiationPressureEditingLibrary::
    NormalizeOpticalPropertyWeights(
        FTGSrpOpticalProperties& Properties)
{
    double* Values[] =
    {
        &Properties.AbsorptionFraction,
        &Properties.SpecularReflectionFraction,
        &Properties.DiffuseReflectionFraction
    };

    double Sum = 0.0;
    for (double* Value : Values)
    {
        *Value = FMath::IsFinite(*Value)
            ? FMath::Max(0.0, *Value)
            : 0.0;
        Sum += *Value;
    }

    if (Sum <= UE_DOUBLE_SMALL_NUMBER)
    {
        Properties.AbsorptionFraction = 1.0;
        Properties.SpecularReflectionFraction = 0.0;
        Properties.DiffuseReflectionFraction = 0.0;
        return;
    }

    for (double* Value : Values)
    {
        *Value /= Sum;
    }
}

void UTGSolarRadiationPressureEditingLibrary::
    NormalizeSolarRadiationPressureScenario(
        FTGSimulationScenario& Scenario)
{
    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    FTGSolarRadiationPressureConfig& Srp =
        Scenario.SolarRadiationPressure;

    // These two values are deliberately fixed and never edited by the HUD.
    Srp.SunBodyName = TEXT("Sun");
    Srp.PressureAtOneAstronomicalUnitPascals = 4.5391e-6;

    // Generated triangles are a rebuildable runtime cache. Keeping them while
    // SRP is disabled can make saved scenarios hundreds of megabytes larger.
    if (!Srp.bEnabled)
    {
        Srp.OpticalFacets.Empty();
    }

    // Preserve an explicitly authored/imported occulter subset. An empty list
    // retains its established meaning of every physical non-Sun catalog body.

    // Resolve generated facets identified only by a component name.
    for (FTGOpticalFacetConfig& Facet : Srp.OpticalFacets)
    {
        if (!Facet.ComponentId.IsValid())
        {
            if (
                FTGComponentConfig* Component =
                    TGSolarRadiationPressureEditingPrivate::
                        FindComponentByName(
                            Scenario,
                            Facet.ComponentName))
            {
                Facet.ComponentId = Component->ComponentId;
            }
        }
    }

    Srp.OpticalFacets.RemoveAll(
        [&Scenario](const FTGOpticalFacetConfig& Facet)
        {
            return
                TGSolarRadiationPressureEditingPrivate::
                    FindComponent(
                        Scenario,
                        Facet.ComponentId) == nullptr;
        });

    for (FTGComponentConfig& Component : Scenario.Components)
    {
        FTGComponentSrpConfig& Config =
            Component.SolarRadiationPressure;

        if (!Config.bIncludedInProxy)
        {
            Srp.OpticalFacets.RemoveAll(
                [&Component](const FTGOpticalFacetConfig& Facet)
                {
                    return Facet.ComponentId == Component.ComponentId;
                });

            Config.GeneratedTriangleCount = 0;
            Config.bProxyGenerationRequired = false;
            Config.LastProxyGenerationMessage =
                TEXT("Component is excluded from the SRP proxy.");

            continue;
        }

        int32 GeneratedCount = 0;
        for (FTGOpticalFacetConfig& Facet : Srp.OpticalFacets)
        {
            if (Facet.ComponentId == Component.ComponentId)
            {
                Facet.ComponentName = Component.Name;
                ++GeneratedCount;
            }
        }

        Config.GeneratedTriangleCount = GeneratedCount;

        const FString CurrentSignature =
            BuildComponentProxyGeometrySignature(Component);

        if (GeneratedCount == 0)
        {
            Config.bProxyGenerationRequired = true;
            Config.LastProxyGenerationMessage =
                TEXT(
                    "Surface geometry will be prepared automatically.");
        }
        else if (
            Config.GeneratedGeometrySignature.IsEmpty() ||
            Config.GeneratedGeometrySignature != CurrentSignature)
        {
            Config.bProxyGenerationRequired = true;
            Config.LastProxyGenerationMessage =
                TEXT(
                    "The component geometry or requested proxy resolution "
                    "changed; its SRP surface geometry will be rebuilt.");
        }
        else
        {
            Config.bProxyGenerationRequired = false;
            Config.LastProxyGenerationMessage =
                FString::Printf(
                    TEXT("Surface geometry ready: %d triangles."),
                    GeneratedCount);
        }

        FText ResolveError;
        TGSolarRadiationPressureEditingPrivate::
            ResolveComponentOpticalPropertiesInternal(
                Scenario,
                Component,
                ResolveError);
    }
}

TArray<FString> UTGSolarRadiationPressureEditingLibrary::
    GetAvailableOccultingBodyNames()
{
    TArray<FString> Result;

    for (const FTGCelestialCatalogEntry& Entry
         : UTGCelestialCatalogLibrary::GetCelestialCatalog())
    {
        if (
            Entry.CatalogKey == FName(TEXT("Sun")) ||
            Entry.SourceRole ==
                ETGCelestialSourceRole::SystemBarycenter)
        {
            continue;
        }

        Result.Add(Entry.CatalogKey.ToString());
    }

    return Result;
}

TArray<FString> UTGSolarRadiationPressureEditingLibrary::
    GetLogicalRegionNames(const FTGComponentConfig& Component)
{
    TArray<FString> Result;

    if (
        Component.Visual.GeometrySource !=
        ETGComponentGeometrySource::Primitive)
    {
        return Result;
    }

    if (
        Component.Visual.PrimitiveType ==
        ETGPrimitiveGeometryType::Box)
    {
        for (
            ETGSrpLogicalRegion Region :
            {
                ETGSrpLogicalRegion::BoxPositiveX,
                ETGSrpLogicalRegion::BoxNegativeX,
                ETGSrpLogicalRegion::BoxPositiveY,
                ETGSrpLogicalRegion::BoxNegativeY,
                ETGSrpLogicalRegion::BoxPositiveZ,
                ETGSrpLogicalRegion::BoxNegativeZ
            })
        {
            Result.Add(GetLogicalRegionDisplayName(Region));
        }
    }
    else if (
        Component.Visual.PrimitiveType ==
        ETGPrimitiveGeometryType::Cylinder)
    {
        for (
            ETGSrpLogicalRegion Region :
            {
                ETGSrpLogicalRegion::CylinderSide,
                ETGSrpLogicalRegion::CylinderPositiveCap,
                ETGSrpLogicalRegion::CylinderNegativeCap
            })
        {
            Result.Add(GetLogicalRegionDisplayName(Region));
        }
    }

    return Result;
}

FString UTGSolarRadiationPressureEditingLibrary::
    GetLogicalRegionDisplayName(ETGSrpLogicalRegion Region)
{
    switch (Region)
    {
        case ETGSrpLogicalRegion::BoxPositiveX:
            return TEXT("Box +X");
        case ETGSrpLogicalRegion::BoxNegativeX:
            return TEXT("Box -X");
        case ETGSrpLogicalRegion::BoxPositiveY:
            return TEXT("Box +Y");
        case ETGSrpLogicalRegion::BoxNegativeY:
            return TEXT("Box -Y");
        case ETGSrpLogicalRegion::BoxPositiveZ:
            return TEXT("Box +Z");
        case ETGSrpLogicalRegion::BoxNegativeZ:
            return TEXT("Box -Z");
        case ETGSrpLogicalRegion::CylinderSide:
            return TEXT("Cylinder Side");
        case ETGSrpLogicalRegion::CylinderPositiveCap:
            return TEXT("Cylinder +Z Cap");
        case ETGSrpLogicalRegion::CylinderNegativeCap:
            return TEXT("Cylinder -Z Cap");
        case ETGSrpLogicalRegion::None:
            break;
    }

    return TEXT("None");
}

bool UTGSolarRadiationPressureEditingLibrary::
    TryParseLogicalRegionDisplayName(
        const FString& DisplayName,
        ETGSrpLogicalRegion& OutRegion)
{
    for (
        ETGSrpLogicalRegion Region :
        {
            ETGSrpLogicalRegion::BoxPositiveX,
            ETGSrpLogicalRegion::BoxNegativeX,
            ETGSrpLogicalRegion::BoxPositiveY,
            ETGSrpLogicalRegion::BoxNegativeY,
            ETGSrpLogicalRegion::BoxPositiveZ,
            ETGSrpLogicalRegion::BoxNegativeZ,
            ETGSrpLogicalRegion::CylinderSide,
            ETGSrpLogicalRegion::CylinderPositiveCap,
            ETGSrpLogicalRegion::CylinderNegativeCap
        })
    {
        if (
            DisplayName.Equals(
                GetLogicalRegionDisplayName(Region),
                ESearchCase::IgnoreCase))
        {
            OutRegion = Region;
            return true;
        }
    }

    OutRegion = ETGSrpLogicalRegion::None;
    return false;
}

FString UTGSolarRadiationPressureEditingLibrary::
    BuildComponentProxyGeometrySignature(
        const FTGComponentConfig& Component)
{
    const FTGComponentVisualConfig& Visual = Component.Visual;
    const FTGComponentSrpConfig& Srp = Component.SolarRadiationPressure;

    FString StlPath = Visual.StlFilePath;
    StlPath.TrimStartAndEndInline();
    if (!StlPath.IsEmpty())
    {
        StlPath = FPaths::ConvertRelativePathToFull(StlPath);
        FPaths::NormalizeFilename(StlPath);
    }

    const int64 StlFileSize =
        StlPath.IsEmpty()
            ? -1
            : IFileManager::Get().FileSize(*StlPath);

    const int64 StlTimestampTicks =
        StlPath.IsEmpty()
            ? 0
            : IFileManager::Get()
                .GetTimeStamp(*StlPath)
                .GetTicks();

    FString StlContentHash = TEXT("not-applicable");
    if (
        Visual.GeometrySource ==
            ETGComponentGeometrySource::CustomStl &&
        !StlPath.IsEmpty())
    {
        const FMD5Hash FileHash = FMD5Hash::HashFile(*StlPath);
        StlContentHash = FileHash.IsValid()
            ? LexToString(FileHash)
            : TEXT("missing-or-unreadable");
    }

    const FString Canonical = FString::Printf(
        TEXT(
            "source=%d;primitive=%d;box=%.17g,%.17g,%.17g;"
            "sphere=%.17g;cylinder=%.17g,%.17g;stl=%s;"
            "stl_unit=%d;stl_recenter=%d;stl_size=%lld;stl_time=%lld;"
            "stl_hash=%s;"
            "offset=%.17g,%.17g,%.17g;orientation=%.17g,%.17g,%.17g,%.17g;"
            "scale=%.17g,%.17g,%.17g;resolution=%d;target=%d"),
        static_cast<int32>(Visual.GeometrySource),
        static_cast<int32>(Visual.PrimitiveType),
        Visual.BoxDimensionsMeters.X,
        Visual.BoxDimensionsMeters.Y,
        Visual.BoxDimensionsMeters.Z,
        Visual.SphereRadiusMeters,
        Visual.CylinderRadiusMeters,
        Visual.CylinderLengthMeters,
        *StlPath,
        static_cast<int32>(Visual.StlLengthUnit),
        static_cast<int32>(Visual.StlRecenterMode),
        StlFileSize,
        StlTimestampTicks,
        *StlContentHash,
        Visual.VisualOffsetMeters.X,
        Visual.VisualOffsetMeters.Y,
        Visual.VisualOffsetMeters.Z,
        Visual.VisualOrientation.X,
        Visual.VisualOrientation.Y,
        Visual.VisualOrientation.Z,
        Visual.VisualOrientation.W,
        Visual.VisualScale.X,
        Visual.VisualScale.Y,
        Visual.VisualScale.Z,
        static_cast<int32>(Srp.ProxyResolutionMode),
        Srp.CustomTargetTriangleCount);

    return FMD5::HashAnsiString(*Canonical);
}

int32 UTGSolarRadiationPressureEditingLibrary::
    GetTotalGeneratedTriangleCount(
        const FTGSimulationScenario& Scenario)
{
    int32 Total = 0;

    for (const FTGComponentConfig& Component : Scenario.Components)
    {
        if (Component.SolarRadiationPressure.bIncludedInProxy)
        {
            Total += FMath::Max(
                0,
                Component.SolarRadiationPressure.GeneratedTriangleCount);
        }
    }

    return Total;
}

bool UTGSolarRadiationPressureEditingLibrary::
    ValidateSolarRadiationPressureScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutWarning,
        FText& OutError)
{
    return ValidateSolarRadiationPressureScenarioInternal(
        Scenario,
        true,
        OutWarning,
        OutError);
}

bool UTGSolarRadiationPressureEditingLibrary::
    ValidateSolarRadiationPressureAuthoring(
        const FTGSimulationScenario& Scenario,
        FText& OutWarning,
        FText& OutError)
{
    return ValidateSolarRadiationPressureScenarioInternal(
        Scenario,
        false,
        OutWarning,
        OutError);
}

bool UTGSolarRadiationPressureEditingLibrary::
    PrepareSolarRadiationPressureGeometry(
        FTGSimulationScenario& Scenario,
        FText& OutSummary,
        FText& OutWarning,
        FText& OutError)
{
    OutSummary = FText::GetEmpty();
    OutWarning = FText::GetEmpty();
    OutError = FText::GetEmpty();
    FTGSimulationScenario PreparedScenario = Scenario;
    NormalizeSolarRadiationPressureScenario(PreparedScenario);

    if (!PreparedScenario.SolarRadiationPressure.bEnabled)
    {
        Scenario = MoveTemp(PreparedScenario);
        return true;
    }

    TArray<FString> Summaries;
    TArray<FString> Warnings;
    for (int32 ComponentIndex = 0;
         ComponentIndex < PreparedScenario.Components.Num();
         ++ComponentIndex)
    {
        FTGComponentConfig& Component =
            PreparedScenario.Components[ComponentIndex];
        FTGComponentSrpConfig& Config =
            Component.SolarRadiationPressure;
        if (!Config.bIncludedInProxy)
        {
            continue;
        }

        const bool bHasCurrentGeometry =
            !Config.bProxyGenerationRequired &&
            Config.GeneratedTriangleCount > 0 &&
            Config.GeneratedGeometrySignature ==
                BuildComponentProxyGeometrySignature(Component);
        if (bHasCurrentGeometry)
        {
            FText ResolveError;
            if (!ResolveComponentOpticalProperties(
                    PreparedScenario,
                    Component.ComponentId,
                    ResolveError))
            {
                OutError = ResolveError;
                return false;
            }
            continue;
        }

        TArray<FTGOpticalFacetConfig> GeneratedTriangles;
        FText ComponentWarning;
        FText ComponentError;
        if (!TGSolarRadiationPressureEditingPrivate::BuildComponentProxy(
                Component,
                GeneratedTriangles,
                ComponentWarning,
                ComponentError))
        {
            OutError = ComponentError;
            return false;
        }

        const FGuid ComponentId = Component.ComponentId;
        const FString Signature =
            BuildComponentProxyGeometrySignature(Component);
        FText ComponentSummary;
        if (!ApplyGeneratedComponentProxy(
                PreparedScenario,
                ComponentId,
                GeneratedTriangles,
                Signature,
                ComponentSummary,
                ComponentError))
        {
            OutError = ComponentError;
            return false;
        }
        if (!ComponentSummary.IsEmpty())
        {
            Summaries.Add(ComponentSummary.ToString());
        }
        if (!ComponentWarning.IsEmpty())
        {
            Warnings.Add(ComponentWarning.ToString());
        }
    }

    NormalizeSolarRadiationPressureScenario(PreparedScenario);
    if (!Summaries.IsEmpty())
    {
        OutSummary = FText::FromString(
            TGSolarRadiationPressureEditingPrivate::JoinMessages(Summaries));
    }
    if (!Warnings.IsEmpty())
    {
        OutWarning = FText::FromString(
            TGSolarRadiationPressureEditingPrivate::JoinMessages(Warnings));
    }
    Scenario = MoveTemp(PreparedScenario);
    return true;
}

bool UTGSolarRadiationPressureEditingLibrary::
    ValidateSolarRadiationPressureScenarioInternal(
        const FTGSimulationScenario& Scenario,
        bool bRequireGeneratedProxy,
        FText& OutWarning,
        FText& OutError)
{
    OutWarning = FText::GetEmpty();
    OutError = FText::GetEmpty();

    FTGSimulationScenario NormalizedScenario = Scenario;
    NormalizeSolarRadiationPressureScenario(NormalizedScenario);

    const FTGSolarRadiationPressureConfig& Srp =
        NormalizedScenario.SolarRadiationPressure;

    if (!Srp.bEnabled)
    {
        return true;
    }

    TArray<FString> Warnings;
    TArray<FString> Errors;
    FString InvalidReason;

    const TArray<FString> SupportedOcculters =
        GetAvailableOccultingBodyNames();

    if (Srp.bComputeEclipse)
    {
        for (const FString& BodyName : Srp.OccultingBodyNames)
        {
            const bool bSupported =
                SupportedOcculters.ContainsByPredicate(
                    [&BodyName](const FString& Candidate)
                    {
                        return
                            Candidate.Equals(
                                BodyName,
                                ESearchCase::IgnoreCase);
                    });

            if (!bSupported)
            {
                Errors.Add(
                    FString::Printf(
                        TEXT(
                            "Occulting body '%s' is not a supported physical "
                            "non-Sun catalog body."),
                        *BodyName));
            }
        }
    }

    int32 IncludedComponentCount = 0;
    TArray<FString> FallbackComponents;

    for (const FTGComponentConfig& Component
         : NormalizedScenario.Components)
    {
        const FTGComponentSrpConfig& Config =
            Component.SolarRadiationPressure;

        if (!Config.bIncludedInProxy)
        {
            continue;
        }

        ++IncludedComponentCount;

        if (
            Component.Visual.GeometrySource ==
            ETGComponentGeometrySource::NoGeometry)
        {
            Errors.Add(
                FString::Printf(
                    TEXT(
                        "Component '%s' has no source geometry for its SRP "
                        "proxy."),
                    *Component.Name));
        }

        if (
            Config.ProxyResolutionMode ==
                ETGSrpProxyResolutionMode::CustomTargetTriangleCount &&
            Config.CustomTargetTriangleCount <= 0)
        {
            Errors.Add(
                FString::Printf(
                    TEXT(
                        "Component '%s' custom target triangle count must "
                        "be positive."),
                    *Component.Name));
        }

        if (Config.bUseGlobalFallbackOpticalProperties)
        {
            FallbackComponents.Add(Component.Name);
        }
        else if (!IsValidOpticalProperties(
                     Config.ComponentOpticalProperties,
                     InvalidReason))
        {
            Errors.Add(
                FString::Printf(
                    TEXT("Component '%s' optical properties: %s"),
                    *Component.Name,
                    *InvalidReason));
        }

        if (!Config.bApplyOneOpticalConfigurationToEntireComponent)
        {
            TSet<ETGSrpLogicalRegion> SeenRegions;
            for (const FTGSrpLogicalRegionOverride& Override
                 : Config.LogicalRegionOverrides)
            {
                if (!TGSolarRadiationPressureEditingPrivate::
                        IsLogicalRegionOverrideSupported(
                            Component,
                            Override.Region))
                {
                    Errors.Add(
                        FString::Printf(
                            TEXT(
                                "Component '%s' has an override for a logical "
                                "region its geometry does not support."),
                            *Component.Name));
                }

                if (SeenRegions.Contains(Override.Region))
                {
                    Errors.Add(
                        FString::Printf(
                            TEXT(
                                "Component '%s' has duplicate logical-region "
                                "overrides."),
                            *Component.Name));
                }
                SeenRegions.Add(Override.Region);

                if (!IsValidOpticalProperties(
                        Override.OpticalProperties,
                        InvalidReason))
                {
                    Errors.Add(
                        FString::Printf(
                            TEXT(
                                "Component '%s' logical-region override: %s"),
                            *Component.Name,
                            *InvalidReason));
                }
            }

            TSet<int32> SeenTriangleIndices;
            for (const FTGSrpTriangleOverride& Override
                 : Config.TriangleOverrides)
            {
                if (SeenTriangleIndices.Contains(Override.ProxyTriangleIndex))
                {
                    Errors.Add(
                        FString::Printf(
                            TEXT(
                                "Component '%s' has duplicate triangle override "
                                "index %d."),
                            *Component.Name,
                            Override.ProxyTriangleIndex));
                }
                SeenTriangleIndices.Add(Override.ProxyTriangleIndex);

                if (Override.ProxyTriangleIndex < 0)
                {
                    Errors.Add(
                        FString::Printf(
                            TEXT(
                                "Component '%s' triangle override index %d "
                                "cannot be negative."),
                            *Component.Name,
                            Override.ProxyTriangleIndex));
                }
                else if (!Config.bProxyGenerationRequired
                    && Config.GeneratedTriangleCount > 0
                    && Override.ProxyTriangleIndex
                        >= Config.GeneratedTriangleCount)
                {
                    Errors.Add(
                        FString::Printf(
                            TEXT(
                                "Component '%s' triangle override index %d is "
                                "outside the generated proxy."),
                            *Component.Name,
                            Override.ProxyTriangleIndex));
                }

                if (!IsValidOpticalProperties(
                        Override.OpticalProperties,
                        InvalidReason))
                {
                    Errors.Add(
                        FString::Printf(
                            TEXT("Component '%s' triangle override: %s"),
                            *Component.Name,
                            *InvalidReason));
                }
            }

            const bool bSphereUsesWholeComponentOnly =
                Component.Visual.GeometrySource ==
                    ETGComponentGeometrySource::Primitive &&
                Component.Visual.PrimitiveType ==
                    ETGPrimitiveGeometryType::Sphere;

            if (
                bSphereUsesWholeComponentOnly &&
                !Config.TriangleOverrides.IsEmpty())
            {
                Errors.Add(
                    FString::Printf(
                        TEXT(
                            "Component '%s' is a sphere and cannot use "
                            "triangle overrides."),
                        *Component.Name));
            }

            if (
                Config.GeneratedTriangleCount >
                    TGSolarRadiationPressureEditingPrivate::
                        MaximumEditableTriangleCount &&
                !Config.TriangleOverrides.IsEmpty())
            {
                Warnings.Add(
                    FString::Printf(
                        TEXT(
                            "Component '%s' has more than 32 generated triangles; "
                            "triangle overrides are disabled and ignored."),
                        *Component.Name));
            }
        }
        else if (!Config.LogicalRegionOverrides.IsEmpty()
            || !Config.TriangleOverrides.IsEmpty())
        {
            Warnings.Add(
                FString::Printf(
                    TEXT(
                        "Component '%s' applies one optical configuration; "
                        "stored region and triangle overrides are ignored."),
                    *Component.Name));
        }

        if (
            bRequireGeneratedProxy &&
            Srp.bEnabled &&
            Config.bProxyGenerationRequired)
        {
            Errors.Add(
                FString::Printf(
                    TEXT(
                        "Surface geometry has not been prepared for "
                        "component '%s'."),
                    *Component.Name));
        }

        if (
            bRequireGeneratedProxy &&
            Srp.bEnabled &&
            Config.GeneratedTriangleCount <= 0)
        {
            Errors.Add(
                FString::Printf(
                    TEXT(
                        "Surface geometry for component '%s' contains no "
                        "triangles."),
                    *Component.Name));
        }
    }

    if (!FallbackComponents.IsEmpty())
    {
        if (!IsValidOpticalProperties(
                Srp.GlobalFallbackOpticalProperties,
                InvalidReason))
        {
            Errors.Add(
                FString::Printf(
                    TEXT("Global fallback optical properties: %s"),
                    *InvalidReason));
        }

        Warnings.Add(
            FString::Printf(
                TEXT("Using global fallback optics: %s."),
                *FString::Join(FallbackComponents, TEXT(", "))));
    }

    if (Srp.bEnabled && IncludedComponentCount == 0)
    {
        Errors.Add(
            TEXT(
                "SRP is enabled, but no component is included in the SRP "
                "proxy."));
    }

    if (
        bRequireGeneratedProxy &&
        Srp.bEnabled &&
        Srp.OpticalFacets.IsEmpty())
    {
        Errors.Add(
            TEXT(
                "SRP is enabled, but no optical surface triangles are "
                "available."));
    }

    if (bRequireGeneratedProxy)
    {
        TSet<FString> FacetIdentityKeys;
        for (const FTGOpticalFacetConfig& Facet : Srp.OpticalFacets)
        {
            const FTGComponentConfig* Component =
                TGSolarRadiationPressureEditingPrivate::
                    FindComponent(
                        NormalizedScenario,
                        Facet.ComponentId);

            if (Component == nullptr)
            {
                Errors.Add(
                    FString::Printf(
                        TEXT("Generated facet '%s' references no component."),
                        *Facet.Name));
                continue;
            }

            if (Component->SolarRadiationPressure.bProxyGenerationRequired)
            {
                // Stale triangles are retained only as a preview/cache until
                // replacement. They are never eligible for the final native
                // request and should not add secondary facet errors.
                continue;
            }

        if (
            !TGSolarRadiationPressureEditingPrivate::
                IsFiniteVector(Facet.Vertex0Meters) ||
            !TGSolarRadiationPressureEditingPrivate::
                IsFiniteVector(Facet.Vertex1Meters) ||
            !TGSolarRadiationPressureEditingPrivate::
                IsFiniteVector(Facet.Vertex2Meters))
        {
            Errors.Add(
                FString::Printf(
                    TEXT("Generated facet '%s' has a non-finite vertex."),
                    *Facet.Name));
        }

        const double CrossMagnitude =
            FVector::CrossProduct(
                Facet.Vertex1Meters - Facet.Vertex0Meters,
                Facet.Vertex2Meters - Facet.Vertex0Meters)
            .Size();

        if (
            !FMath::IsFinite(CrossMagnitude) ||
            CrossMagnitude <=
                TGSolarRadiationPressureEditingPrivate::
                    MinimumFacetCrossMagnitude)
        {
            Errors.Add(
                FString::Printf(
                    TEXT("Generated facet '%s' is degenerate."),
                    *Facet.Name));
        }

        FTGSrpOpticalProperties FacetProperties;
        FacetProperties.AbsorptionFraction = Facet.AbsorptionFraction;
        FacetProperties.SpecularReflectionFraction =
            Facet.SpecularReflectionFraction;
        FacetProperties.DiffuseReflectionFraction =
            Facet.DiffuseReflectionFraction;

        if (!IsValidOpticalProperties(
                FacetProperties,
                InvalidReason))
        {
            Errors.Add(
                FString::Printf(
                    TEXT("Generated facet '%s' optical properties: %s"),
                    *Facet.Name,
                    *InvalidReason));
        }

        const FString IdentityKey = FString::Printf(
            TEXT("%s:%d"),
            *Facet.ComponentId.ToString(),
            Facet.StableTriangleIndex);

        if (FacetIdentityKeys.Contains(IdentityKey))
        {
            Errors.Add(
                FString::Printf(
                    TEXT(
                        "Component '%s' has duplicate stable triangle "
                        "index %d."),
                    *Component->Name,
                    Facet.StableTriangleIndex));
        }
            FacetIdentityKeys.Add(IdentityKey);
        }
    }

    const int32 TotalTriangleCount =
        GetTotalGeneratedTriangleCount(NormalizedScenario);

    if (
        TotalTriangleCount >
        TGSolarRadiationPressureEditingPrivate::
            PerformanceWarningTriangleCount)
    {
        Warnings.Add(
            FString::Printf(
                TEXT(
                    "The SRP proxy contains %d triangles, above the soft "
                    "performance threshold of 2,000."),
                TotalTriangleCount));
    }

    if (!Warnings.IsEmpty())
    {
        OutWarning = FText::FromString(
            TGSolarRadiationPressureEditingPrivate::
                JoinMessages(Warnings));
    }

    if (!Errors.IsEmpty())
    {
        OutError = FText::FromString(
            TGSolarRadiationPressureEditingPrivate::
                JoinMessages(Errors));
        return false;
    }

    return true;
}

bool UTGSolarRadiationPressureEditingLibrary::
    MarkComponentProxyGenerationRequired(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FString& Reason,
        FText& OutError)
{
    OutError = FText::GetEmpty();
    NormalizeSolarRadiationPressureScenario(Scenario);

    FTGComponentConfig* Component =
        TGSolarRadiationPressureEditingPrivate::
            FindComponent(Scenario, ComponentId);

    if (Component == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The selected SRP component no longer exists."));
        return false;
    }

    FTGComponentSrpConfig& Config =
        Component->SolarRadiationPressure;

    Config.bProxyGenerationRequired = Config.bIncludedInProxy;
    Config.LastProxyGenerationMessage =
        Reason.IsEmpty()
            ? TEXT("Surface geometry will be prepared automatically.")
            : Reason;

    return true;
}

bool UTGSolarRadiationPressureEditingLibrary::
    DiscardTriangleOverridesAndGeneratedProxy(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FText& OutError)
{
    OutError = FText::GetEmpty();
    NormalizeSolarRadiationPressureScenario(Scenario);

    FTGComponentConfig* Component =
        TGSolarRadiationPressureEditingPrivate::
            FindComponent(Scenario, ComponentId);

    if (Component == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The selected SRP component no longer exists."));
        return false;
    }

    FTGComponentSrpConfig& Config =
        Component->SolarRadiationPressure;

    Config.TriangleOverrides.Reset();
    Config.LogicalRegionOverrides.RemoveAll(
        [Component](const FTGSrpLogicalRegionOverride& Override)
        {
            return
                !TGSolarRadiationPressureEditingPrivate::
                    IsLogicalRegionOverrideSupported(
                        *Component,
                        Override.Region);
        });
    Config.GeneratedGeometrySignature.Reset();
    Config.GeneratedTriangleCount = 0;
    Config.bProxyGenerationRequired = Config.bIncludedInProxy;
    Config.LastProxyGenerationMessage =
        TEXT(
            "The previous surface geometry was discarded and will be "
            "rebuilt from the current component geometry.");

    Scenario.SolarRadiationPressure.OpticalFacets.RemoveAll(
        [&ComponentId](const FTGOpticalFacetConfig& Facet)
        {
            return Facet.ComponentId == ComponentId;
        });

    return true;
}

bool UTGSolarRadiationPressureEditingLibrary::
    ApplyGeneratedComponentProxy(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const TArray<FTGOpticalFacetConfig>& GeneratedTriangles,
        const FString& GeometrySignature,
        FText& OutSummary,
        FText& OutError)
{
    OutSummary = FText::GetEmpty();
    OutError = FText::GetEmpty();
    NormalizeSolarRadiationPressureScenario(Scenario);

    FTGComponentConfig* Component =
        TGSolarRadiationPressureEditingPrivate::
            FindComponent(Scenario, ComponentId);

    if (Component == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The generated SRP proxy references no current component."));
        return false;
    }

    FTGComponentSrpConfig& Config =
        Component->SolarRadiationPressure;

    if (!Config.bIncludedInProxy)
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT("Component '%s' is excluded from the SRP proxy."),
                *Component->Name));
        return false;
    }

    if (GeneratedTriangles.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("Surface generation produced no SRP triangles."));
        return false;
    }

    const FString CurrentSignature =
        BuildComponentProxyGeometrySignature(*Component);

    const FString SuppliedSignature =
        GeometrySignature.IsEmpty()
            ? CurrentSignature
            : GeometrySignature;

    if (SuppliedSignature != CurrentSignature)
    {
        OutError = FText::FromString(
            TEXT(
                "The generated proxy signature does not match the current "
                "component geometry and resolution."));
        return false;
    }

    if (!Config.TriangleOverrides.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT(
                "Existing triangle overrides cannot be mapped safely to the "
                "new surface geometry. Discard those overrides before "
                "regenerating the surface."));
        return false;
    }

    TArray<FTGOpticalFacetConfig> ValidatedTriangles;
    ValidatedTriangles.Reserve(GeneratedTriangles.Num());
    TSet<int32> StableIndices;

    for (
        int32 ArrayIndex = 0;
        ArrayIndex < GeneratedTriangles.Num();
        ++ArrayIndex)
    {
        FTGOpticalFacetConfig Facet = GeneratedTriangles[ArrayIndex];
        Facet.ComponentId = ComponentId;
        Facet.ComponentName = Component->Name;

        if (Facet.StableTriangleIndex == INDEX_NONE)
        {
            Facet.StableTriangleIndex = ArrayIndex;
        }

        if (Facet.StableTriangleIndex != ArrayIndex)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Generated surface triangles must use contiguous "
                        "indices in array order for component '%s': expected "
                        "%d, received %d."),
                    *Component->Name,
                    ArrayIndex,
                    Facet.StableTriangleIndex));
            return false;
        }
        StableIndices.Add(Facet.StableTriangleIndex);

        if (
            !TGSolarRadiationPressureEditingPrivate::
                IsFiniteVector(Facet.Vertex0Meters) ||
            !TGSolarRadiationPressureEditingPrivate::
                IsFiniteVector(Facet.Vertex1Meters) ||
            !TGSolarRadiationPressureEditingPrivate::
                IsFiniteVector(Facet.Vertex2Meters))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Generated triangle %d for component '%s' contains "
                        "a non-finite vertex."),
                    Facet.StableTriangleIndex,
                    *Component->Name));
            return false;
        }

        const double CrossMagnitude =
            FVector::CrossProduct(
                Facet.Vertex1Meters - Facet.Vertex0Meters,
                Facet.Vertex2Meters - Facet.Vertex0Meters)
            .Size();

        if (
            !FMath::IsFinite(CrossMagnitude) ||
            CrossMagnitude <=
                TGSolarRadiationPressureEditingPrivate::
                    MinimumFacetCrossMagnitude)
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Generated triangle %d for component '%s' is "
                        "degenerate."),
                    Facet.StableTriangleIndex,
                    *Component->Name));
            return false;
        }

        if (
            !TGSolarRadiationPressureEditingPrivate::
                IsFacetLogicalRegionSupported(
                    *Component,
                    Facet.LogicalRegion))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Generated triangle %d uses an unsupported logical "
                        "region for component '%s'."),
                    Facet.StableTriangleIndex,
                    *Component->Name));
            return false;
        }

        if (Facet.Name.IsEmpty())
        {
            Facet.Name = FString::Printf(
                TEXT("%s SRP Triangle %d"),
                *Component->Name,
                Facet.StableTriangleIndex);
        }

        ValidatedTriangles.Add(MoveTemp(Facet));
    }

    FString InvalidReason;
    const FTGSrpOpticalProperties& BaseProperties =
        Config.bUseGlobalFallbackOpticalProperties
            ? Scenario.SolarRadiationPressure
                .GlobalFallbackOpticalProperties
            : Config.ComponentOpticalProperties;

    if (!IsValidOpticalProperties(BaseProperties, InvalidReason))
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT("Component '%s' cannot resolve SRP optics: %s"),
                *Component->Name,
                *InvalidReason));
        return false;
    }

    for (const FTGSrpLogicalRegionOverride& Override
         : Config.LogicalRegionOverrides)
    {
        if (!TGSolarRadiationPressureEditingPrivate::
                IsLogicalRegionOverrideSupported(
                    *Component,
                    Override.Region))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' has an incompatible logical-region "
                        "override. Confirm regeneration so it can be "
                        "discarded first."),
                    *Component->Name));
            return false;
        }

        if (!IsValidOpticalProperties(
                Override.OpticalProperties,
                InvalidReason))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' has an invalid logical-region "
                        "override: %s"),
                    *Component->Name,
                    *InvalidReason));
            return false;
        }
    }

    const bool bSphereUsesWholeComponentOnly =
        Component->Visual.GeometrySource ==
            ETGComponentGeometrySource::Primitive &&
        Component->Visual.PrimitiveType ==
            ETGPrimitiveGeometryType::Sphere;

    if (
        !Config.TriangleOverrides.IsEmpty() &&
        (bSphereUsesWholeComponentOnly ||
         GeneratedTriangles.Num() >
             TGSolarRadiationPressureEditingPrivate::
                 MaximumEditableTriangleCount))
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT(
                    "Component '%s' cannot preserve its triangle overrides "
                    "for this generated proxy. Confirm their discard first."),
                *Component->Name));
        return false;
    }

    for (const FTGSrpTriangleOverride& Override
         : Config.TriangleOverrides)
    {
        if (!StableIndices.Contains(Override.ProxyTriangleIndex))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' triangle override index %d does "
                        "not exist in the generated proxy."),
                    *Component->Name,
                    Override.ProxyTriangleIndex));
            return false;
        }

        if (!IsValidOpticalProperties(
                Override.OpticalProperties,
                InvalidReason))
        {
            OutError = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' has an invalid triangle override: "
                        "%s"),
                    *Component->Name,
                    *InvalidReason));
            return false;
        }
    }

    Scenario.SolarRadiationPressure.OpticalFacets.RemoveAll(
        [&ComponentId](const FTGOpticalFacetConfig& Facet)
        {
            return Facet.ComponentId == ComponentId;
        });

    Scenario.SolarRadiationPressure.OpticalFacets.Append(
        MoveTemp(ValidatedTriangles));

    Config.GeneratedGeometrySignature = SuppliedSignature;
    Config.GeneratedTriangleCount = GeneratedTriangles.Num();
    Config.bProxyGenerationRequired = false;
    Config.LastProxyGenerationMessage =
        FString::Printf(
            TEXT("Surface geometry ready: %d triangles."),
            GeneratedTriangles.Num());

    if (!TGSolarRadiationPressureEditingPrivate::
            ResolveComponentOpticalPropertiesInternal(
                Scenario,
                *Component,
                OutError))
    {
        return false;
    }

    OutSummary = FText::FromString(
        Config.LastProxyGenerationMessage);
    return true;
}

bool UTGSolarRadiationPressureEditingLibrary::
    ResolveComponentOpticalProperties(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FText& OutError)
{
    NormalizeSolarRadiationPressureScenario(Scenario);

    FTGComponentConfig* Component =
        TGSolarRadiationPressureEditingPrivate::
            FindComponent(Scenario, ComponentId);

    if (Component == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The selected SRP component no longer exists."));
        return false;
    }

    return TGSolarRadiationPressureEditingPrivate::
        ResolveComponentOpticalPropertiesInternal(
            Scenario,
            *Component,
            OutError);
}

bool UTGSolarRadiationPressureEditingLibrary::
    IsValidOpticalProperties(
        const FTGSrpOpticalProperties& Properties,
        FString& OutReason)
{
    OutReason.Reset();

    const double Values[] =
    {
        Properties.AbsorptionFraction,
        Properties.SpecularReflectionFraction,
        Properties.DiffuseReflectionFraction
    };

    for (const double Value : Values)
    {
        if (!FMath::IsFinite(Value))
        {
            OutReason = TEXT("all fractions must be finite.");
            return false;
        }

        if (Value < 0.0 || Value > 1.0)
        {
            OutReason = TEXT("every fraction must lie in [0, 1].");
            return false;
        }
    }

    const double Sum =
        Properties.AbsorptionFraction +
        Properties.SpecularReflectionFraction +
        Properties.DiffuseReflectionFraction;

    if (
        FMath::Abs(Sum - 1.0) >
        TGSolarRadiationPressureEditingPrivate::
            OpticalSumTolerance)
    {
        OutReason = FString::Printf(
            TEXT("fractions must sum to 1 (current sum %.9g)."),
            Sum);
        return false;
    }

    return true;
}
