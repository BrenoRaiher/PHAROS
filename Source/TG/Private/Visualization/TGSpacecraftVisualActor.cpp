// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Visualization/TGSpacecraftVisualActor.h"

#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectGlobals.h"

namespace TGSpacecraftVisualActorPrivate
{
    const FName ParameterSolidColor(TEXT("SolidColor"));
    const FName ParameterBaseColorTint(TEXT("BaseColorTint"));

    const FName ParameterUseBaseColorTexture(
        TEXT("UseBaseColorTexture"));
    const FName ParameterBaseColorTexture(
        TEXT("BaseColorTexture"));

    const FName ParameterUseNormalTexture(
        TEXT("UseNormalTexture"));
    const FName ParameterNormalTexture(
        TEXT("NormalTexture"));

    const FName ParameterUseRoughnessTexture(
        TEXT("UseRoughnessTexture"));
    const FName ParameterRoughnessTexture(
        TEXT("RoughnessTexture"));

    const FName ParameterUseMetallicTexture(
        TEXT("UseMetallicTexture"));
    const FName ParameterMetallicTexture(
        TEXT("MetallicTexture"));

    FString NormalizeRuntimeFilePath(
        const FString& FilePath)
    {
        FString NormalizedPath = FilePath;
        NormalizedPath.TrimStartAndEndInline();

        if (!NormalizedPath.IsEmpty())
        {
            FPaths::NormalizeFilename(
                NormalizedPath);
        }

        return NormalizedPath;
    }

    constexpr int32 MaximumStlTriangleCount = 2000000;

    struct FStlTriangle
    {
        FVector A = FVector::ZeroVector;
        FVector B = FVector::ZeroVector;
        FVector C = FVector::ZeroVector;
    };

    uint32 ReadLittleEndianUint32(
        const uint8* Data)
    {
        return
            static_cast<uint32>(Data[0]) |
            (static_cast<uint32>(Data[1]) << 8) |
            (static_cast<uint32>(Data[2]) << 16) |
            (static_cast<uint32>(Data[3]) << 24);
    }

    float ReadLittleEndianFloat(
        const uint8* Data)
    {
        const uint32 Bits =
            ReadLittleEndianUint32(Data);

        float Value = 0.0f;
        FMemory::Memcpy(
            &Value,
            &Bits,
            sizeof(float));

        return Value;
    }

    bool IsFiniteVector(
        const FVector& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    double GetStlUnitsToCentimeters(
        ETGStlLengthUnit Unit)
    {
        switch (Unit)
        {
            case ETGStlLengthUnit::Millimeters:
                return 0.1;

            case ETGStlLengthUnit::Centimeters:
                return 1.0;

            case ETGStlLengthUnit::Meters:
                return 100.0;
        }

        return 0.0;
    }

    FVector ConvertStlVertexToUnreal(
        const FVector& SourceVertex,
        double UnitsToCentimeters)
    {
        /*
         * Backend/component coordinates are right-handed while Unreal uses the
         * project's Y-reflected visualization convention.
         */
        return FVector(
            SourceVertex.X * UnitsToCentimeters,
            -SourceVertex.Y * UnitsToCentimeters,
            SourceVertex.Z * UnitsToCentimeters);
    }

    bool ParseBinaryStl(
        const TArray<uint8>& Bytes,
        TArray<FStlTriangle>& OutTriangles,
        FText& OutErrorText)
    {
        OutTriangles.Reset();

        if (Bytes.Num() < 84)
        {
            return false;
        }

        const uint32 TriangleCount =
            ReadLittleEndianUint32(
                Bytes.GetData() + 80);

        const uint64 ExpectedSize =
            84ull +
            static_cast<uint64>(TriangleCount) * 50ull;

        if (
            ExpectedSize !=
            static_cast<uint64>(Bytes.Num()))
        {
            return false;
        }

        if (TriangleCount == 0)
        {
            OutErrorText = FText::FromString(
                TEXT("Binary STL contains no triangles."));

            return true;
        }

        if (
            TriangleCount >
            static_cast<uint32>(
                MaximumStlTriangleCount))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "STL contains %u triangles, exceeding the "
                        "preview limit of %d."),
                    TriangleCount,
                    MaximumStlTriangleCount));

            return true;
        }

        OutTriangles.Reserve(
            static_cast<int32>(TriangleCount));

        const uint8* Cursor =
            Bytes.GetData() + 84;

        for (
            uint32 TriangleIndex = 0;
            TriangleIndex < TriangleCount;
            ++TriangleIndex)
        {
            /*
             * 12 bytes facet normal, then three 12-byte vertices, then the
             * two-byte attribute field.
             */
            Cursor += 12;

            FStlTriangle Triangle;

            FVector* Vertices[3] =
            {
                &Triangle.A,
                &Triangle.B,
                &Triangle.C
            };

            for (int32 VertexIndex = 0; VertexIndex < 3; ++VertexIndex)
            {
                const float X =
                    ReadLittleEndianFloat(Cursor + 0);
                const float Y =
                    ReadLittleEndianFloat(Cursor + 4);
                const float Z =
                    ReadLittleEndianFloat(Cursor + 8);

                Cursor += 12;

                *Vertices[VertexIndex] =
                    FVector(
                        static_cast<double>(X),
                        static_cast<double>(Y),
                        static_cast<double>(Z));

                if (!IsFiniteVector(*Vertices[VertexIndex]))
                {
                    OutTriangles.Reset();

                    OutErrorText = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "Binary STL triangle %u contains a "
                                "non-finite vertex."),
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
        TArray<FStlTriangle>& OutTriangles,
        FText& OutErrorText)
    {
        OutTriangles.Reset();

        FString Contents;

        if (!FFileHelper::LoadFileToString(
                Contents,
                *FilePath))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Could not read ASCII STL file: %s"),
                    *FilePath));

            return false;
        }

        TArray<FString> Lines;
        Contents.ParseIntoArrayLines(
            Lines,
            false);

        TArray<FVector> PendingVertices;
        PendingVertices.Reserve(3);

        for (const FString& SourceLine : Lines)
        {
            FString Line = SourceLine;
            Line.TrimStartAndEndInline();

            if (Line.IsEmpty())
            {
                continue;
            }

            TArray<FString> Tokens;
            Line.ParseIntoArrayWS(Tokens);

            if (
                Tokens.Num() != 4 ||
                !Tokens[0].Equals(
                    TEXT("vertex"),
                    ESearchCase::IgnoreCase))
            {
                continue;
            }

            double X = 0.0;
            double Y = 0.0;
            double Z = 0.0;

            if (
                !FDefaultValueHelper::ParseDouble(
                    Tokens[1],
                    X) ||
                !FDefaultValueHelper::ParseDouble(
                    Tokens[2],
                    Y) ||
                !FDefaultValueHelper::ParseDouble(
                    Tokens[3],
                    Z))
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "ASCII STL contains a vertex with an invalid "
                        "numeric coordinate."));

                return false;
            }

            const FVector Vertex(X, Y, Z);

            if (!IsFiniteVector(Vertex))
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "ASCII STL contains a non-finite vertex."));

                return false;
            }

            PendingVertices.Add(Vertex);

            if (PendingVertices.Num() == 3)
            {
                FStlTriangle Triangle;
                Triangle.A = PendingVertices[0];
                Triangle.B = PendingVertices[1];
                Triangle.C = PendingVertices[2];

                OutTriangles.Add(Triangle);
                PendingVertices.Reset();

                if (
                    OutTriangles.Num() >
                    MaximumStlTriangleCount)
                {
                    OutTriangles.Reset();

                    OutErrorText = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "STL exceeds the preview limit of "
                                "%d triangles."),
                            MaximumStlTriangleCount));

                    return false;
                }
            }
        }

        if (!PendingVertices.IsEmpty())
        {
            OutTriangles.Reset();

            OutErrorText = FText::FromString(
                TEXT(
                    "ASCII STL ended with an incomplete triangle."));

            return false;
        }

        if (OutTriangles.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "STL contains no readable triangles."));

            return false;
        }

        return true;
    }

    bool LoadStlTriangles(
        const FString& FilePath,
        TArray<FStlTriangle>& OutTriangles,
        FText& OutErrorText)
    {
        OutTriangles.Reset();
        OutErrorText = FText::GetEmpty();

        TArray<uint8> Bytes;

        if (!FFileHelper::LoadFileToArray(
                Bytes,
                *FilePath))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Could not read STL file: %s"),
                    *FilePath));

            return false;
        }

        FText BinaryParseMessage;

        if (ParseBinaryStl(
                Bytes,
                OutTriangles,
                BinaryParseMessage))
        {
            if (!BinaryParseMessage.IsEmpty())
            {
                OutTriangles.Reset();
                OutErrorText = BinaryParseMessage;
                return false;
            }

            return !OutTriangles.IsEmpty();
        }

        return ParseAsciiStl(
            FilePath,
            OutTriangles,
            OutErrorText);
    }

    FVector2D MakeProjectedUv(
        const FVector& Vertex,
        const FBox& Bounds,
        int32 ProjectionAxis)
    {
        const FVector Extent =
            Bounds.GetSize();

        const auto NormalizeAxis =
            [](double Value, double Minimum, double Span)
            {
                if (FMath::Abs(Span) <= 1.0e-12)
                {
                    return 0.0;
                }

                return (Value - Minimum) / Span;
            };

        switch (ProjectionAxis)
        {
            case 0:
                return FVector2D(
                    NormalizeAxis(
                        Vertex.Y,
                        Bounds.Min.Y,
                        Extent.Y),
                    NormalizeAxis(
                        Vertex.Z,
                        Bounds.Min.Z,
                        Extent.Z));

            case 1:
                return FVector2D(
                    NormalizeAxis(
                        Vertex.X,
                        Bounds.Min.X,
                        Extent.X),
                    NormalizeAxis(
                        Vertex.Z,
                        Bounds.Min.Z,
                        Extent.Z));

            default:
                return FVector2D(
                    NormalizeAxis(
                        Vertex.X,
                        Bounds.Min.X,
                        Extent.X),
                    NormalizeAxis(
                        Vertex.Y,
                        Bounds.Min.Y,
                        Extent.Y));
        }
    }

    void ConfigureGeneratedMeshCollision(
        UPrimitiveComponent* MeshComponent)
    {
        if (MeshComponent == nullptr)
        {
            return;
        }

        MeshComponent->SetCollisionEnabled(
            ECollisionEnabled::QueryOnly);

        MeshComponent->SetCollisionResponseToAllChannels(
            ECR_Ignore);

        MeshComponent->SetCollisionResponseToChannel(
            ECC_Visibility,
            ECR_Block);

        MeshComponent->SetGenerateOverlapEvents(false);
    }
}

ATGSpacecraftVisualActor::ATGSpacecraftVisualActor()
{
    PrimaryActorTick.bCanEverTick = false;

    BodyFrameRoot =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("BodyFrameRoot"));

    SetRootComponent(BodyFrameRoot);

    static ConstructorHelpers::FObjectFinder<UStaticMesh>
        BoxMeshFinder(
            TEXT("/Engine/BasicShapes/Cube.Cube"));

    if (BoxMeshFinder.Succeeded())
    {
        BoxPrimitiveMesh = BoxMeshFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh>
        SphereMeshFinder(
            TEXT("/Engine/BasicShapes/Sphere.Sphere"));

    if (SphereMeshFinder.Succeeded())
    {
        SpherePrimitiveMesh = SphereMeshFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh>
        CylinderMeshFinder(
            TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

    if (CylinderMeshFinder.Succeeded())
    {
        CylinderPrimitiveMesh = CylinderMeshFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        SurfaceMaterialFinder(
            TEXT(
                "/Game/UI/Configuration/ComponentTree/Materials/"
                "M_TGComponentSurface.M_TGComponentSurface"));

    if (SurfaceMaterialFinder.Succeeded())
    {
        ComponentSurfaceMaterial =
            SurfaceMaterialFinder.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        TranslucentSurfaceMaterialFinder(
            TEXT(
                "/Game/UI/Configuration/ComponentTree/Materials/"
                "M_TGComponentSurface_Translucent."
                "M_TGComponentSurface_Translucent"));

    if (TranslucentSurfaceMaterialFinder.Succeeded())
    {
        ComponentTranslucentSurfaceMaterial =
            TranslucentSurfaceMaterialFinder.Object;
    }
}

bool ATGSpacecraftVisualActor::BuildFromScenario(
    const FTGSimulationScenario& Scenario,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    ClearSpacecraftVisuals();

    if (Scenario.Components.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The spacecraft must contain at least "
                "one physical component."));

        return false;
    }

    if (!ValidateComponentIdentifiers(
            Scenario,
            OutErrorText))
    {
        return false;
    }

    ScenarioSnapshot = Scenario;

    for (
        int32 ComponentIndex = 0;
        ComponentIndex < ScenarioSnapshot.Components.Num();
        ++ComponentIndex)
    {
        const FTGComponentConfig& Component =
            ScenarioSnapshot.Components[ComponentIndex];

        USceneComponent* ComponentFrame =
            CreateGeneratedSceneComponent(
                FString::Printf(
                    TEXT("ComponentFrame_%d"),
                    ComponentIndex),
                BodyFrameRoot);

        if (ComponentFrame == nullptr)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Failed to create the visual frame "
                        "for component '%s'."),
                    *Component.Name));

            ClearSpacecraftVisuals();
            return false;
        }

        ComponentFramesById.Add(
            Component.ComponentId,
            ComponentFrame);

        USceneComponent* GeometryRoot =
            CreateGeneratedSceneComponent(
                FString::Printf(
                    TEXT("GeometryRoot_%d"),
                    ComponentIndex),
                ComponentFrame);

        if (GeometryRoot == nullptr)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Failed to create the geometry root "
                        "for component '%s'."),
                    *Component.Name));

            ClearSpacecraftVisuals();
            return false;
        }

        /*
         * The source geometry is authoritative for visual origin,
         * orientation and dimensions.
         *
         * Legacy visual offset/orientation/scale fields remain serialized for
         * backward compatibility, but this actor deliberately does not apply
         * them under the current Component Visual Appearance contract.
         */
        GeometryRoot->SetRelativeLocation(
            FVector::ZeroVector);

        GeometryRoot->SetRelativeRotation(
            FQuat::Identity);

        GeometryRoot->SetRelativeScale3D(
            FVector::OneVector);

        ComponentGeometryRootsById.Add(
            Component.ComponentId,
            GeometryRoot);

        UMeshComponent* MeshComponent = nullptr;

        if (!CreateConfiguredVisualMesh(
                Component,
                ComponentIndex,
                GeometryRoot,
                MeshComponent,
                OutErrorText))
        {
            ClearSpacecraftVisuals();
            return false;
        }

        ComponentMeshesById.Add(
            Component.ComponentId,
            MeshComponent);

        ComponentIdsByPrimitive.Add(
            MeshComponent,
            Component.ComponentId);
    }

    TArray<FTGComponentKinematicPose> InitialPoses;
    TArray<double> AcceptedCoordinates;

    if (!UTGComponentKinematicsLibrary::
            EvaluateComponentTreeKinematics(
                ScenarioSnapshot,
                TArray<double>(),
                InitialPoses,
                AcceptedCoordinates,
                OutErrorText))
    {
        ClearSpacecraftVisuals();
        return false;
    }

    if (!ApplyComponentPoses(
            InitialPoses,
            OutErrorText))
    {
        ClearSpacecraftVisuals();
        return false;
    }

    bHasBuiltSpacecraft = true;
    return true;
}

bool ATGSpacecraftVisualActor::RefreshKinematicsFromScenario(
    const FTGSimulationScenario& Scenario,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (!bHasBuiltSpacecraft)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Build the spacecraft visual actor before "
                "refreshing its scenario kinematics."));

        return false;
    }

    if (Scenario.Components.Num() != ComponentFramesById.Num())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The spacecraft component topology changed. "
                "Rebuild the spacecraft visual actor instead."));

        return false;
    }

    if (!ValidateComponentIdentifiers(
            Scenario,
            OutErrorText))
    {
        return false;
    }

    for (const FTGComponentConfig& Component : Scenario.Components)
    {
        USceneComponent* const* ExistingFrame =
            ComponentFramesById.Find(Component.ComponentId);

        if (ExistingFrame == nullptr || *ExistingFrame == nullptr)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The spacecraft component topology changed. "
                    "Rebuild the spacecraft visual actor instead."));

            return false;
        }
    }

    TArray<FTGComponentKinematicPose> EvaluatedPoses;
    TArray<double> AcceptedCoordinates;

    if (!UTGComponentKinematicsLibrary::
            EvaluateComponentTreeKinematics(
                Scenario,
                TArray<double>(),
                EvaluatedPoses,
                AcceptedCoordinates,
                OutErrorText))
    {
        return false;
    }

    const FTGSimulationScenario PreviousSnapshot = ScenarioSnapshot;
    ScenarioSnapshot = Scenario;

    if (!ApplyComponentPoses(
            EvaluatedPoses,
            OutErrorText))
    {
        ScenarioSnapshot = PreviousSnapshot;
        return false;
    }

    return true;
}

bool ATGSpacecraftVisualActor::
    RefreshVisualAppearanceFromScenario(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (!bHasBuiltSpacecraft)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Build the spacecraft visual actor before "
                "refreshing visual appearance."));

        return false;
    }

    if (!ValidateVisualRefreshTopology(
            Scenario,
            OutErrorText))
    {
        return false;
    }

    /*
     * Visual refresh deliberately leaves the physical component frames in
     * place. Each visual mesh is rebuilt beneath its existing GeometryRoot so
     * primitive/STL source changes never disturb the current articulated pose
     * or camera framing.
     */
    GeneratedMaterialInstances.Reset();
    GeneratedRuntimeTextures.Reset();

    for (
        int32 ComponentIndex = 0;
        ComponentIndex < Scenario.Components.Num();
        ++ComponentIndex)
    {
        const FTGComponentConfig& Component =
            Scenario.Components[ComponentIndex];

        USceneComponent* const* GeometryRootPointer =
            ComponentGeometryRootsById.Find(
                Component.ComponentId);

        UMeshComponent* const* ExistingMeshPointer =
            ComponentMeshesById.Find(
                Component.ComponentId);

        if (
            GeometryRootPointer == nullptr ||
            *GeometryRootPointer == nullptr ||
            !IsValid(*GeometryRootPointer) ||
            ExistingMeshPointer == nullptr ||
            *ExistingMeshPointer == nullptr ||
            !IsValid(*ExistingMeshPointer))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "The generated visual topology for component "
                        "'%s' is incomplete. Rebuild the spacecraft "
                        "visual actor instead."),
                    *Component.Name));

            return false;
        }

        UMeshComponent* ExistingMesh =
            *ExistingMeshPointer;

        UMeshComponent* ReplacementMesh = nullptr;

        if (!CreateConfiguredVisualMesh(
                Component,
                ComponentIndex,
                *GeometryRootPointer,
                ReplacementMesh,
                OutErrorText))
        {
            return false;
        }

        if (
            HighlightedComponentId ==
            Component.ComponentId)
        {
            ReplacementMesh->SetCustomDepthStencilValue(
                FMath::Clamp(
                    HighlightStencilValue,
                    1,
                    255));

            ReplacementMesh->SetRenderCustomDepth(true);
        }

        ComponentIdsByPrimitive.Remove(
            ExistingMesh);

        ComponentMeshesById.Add(
            Component.ComponentId,
            ReplacementMesh);

        ComponentIdsByPrimitive.Add(
            ReplacementMesh,
            Component.ComponentId);

        GeneratedActorComponents.Remove(
            ExistingMesh);

        ExistingMesh->DestroyComponent();
    }

    ScenarioSnapshot = Scenario;
    return true;
}


bool ATGSpacecraftVisualActor::
    SetComponentVisualVisibility(
        FGuid ComponentId,
        bool bVisible,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (!bHasBuiltSpacecraft)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Build the spacecraft visual actor before "
                "changing component visibility."));

        return false;
    }

    if (!ComponentId.IsValid())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Cannot change visibility for an invalid component ID."));

        return false;
    }

    UMeshComponent* const* MeshPointer =
        ComponentMeshesById.Find(
            ComponentId);

    if (
        MeshPointer == nullptr ||
        *MeshPointer == nullptr ||
        !IsValid(*MeshPointer))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "No generated visual mesh exists for the selected component."));

        return false;
    }

    (*MeshPointer)->SetVisibility(
        bVisible,
        true);

    (*MeshPointer)->SetHiddenInGame(
        !bVisible,
        true);

    for (FTGComponentConfig& Component : ScenarioSnapshot.Components)
    {
        if (Component.ComponentId == ComponentId)
        {
            Component.Visual.bVisible = bVisible;
            break;
        }
    }

    return true;
}


bool ATGSpacecraftVisualActor::ApplyJointCoordinates(
    const TArray<double>& RequestedCoordinates,
    TArray<double>& OutAcceptedCoordinates,
    FText& OutErrorText)
{
    OutAcceptedCoordinates.Reset();
    OutErrorText = FText::GetEmpty();

    if (!bHasBuiltSpacecraft)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Build the spacecraft visual actor "
                "before applying joint coordinates."));

        return false;
    }

    TArray<FTGComponentKinematicPose> EvaluatedPoses;

    if (!UTGComponentKinematicsLibrary::
            EvaluateComponentTreeKinematics(
                ScenarioSnapshot,
                RequestedCoordinates,
                EvaluatedPoses,
                OutAcceptedCoordinates,
                OutErrorText))
    {
        return false;
    }

    return ApplyComponentPoses(
        EvaluatedPoses,
        OutErrorText);
}

bool ATGSpacecraftVisualActor::
    ApplyInitialJointCoordinates(
        TArray<double>& OutAcceptedCoordinates,
        FText& OutErrorText)
{
    OutAcceptedCoordinates.Reset();
    OutErrorText = FText::GetEmpty();

    if (!bHasBuiltSpacecraft)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Build the spacecraft visual actor "
                "before restoring initial coordinates."));

        return false;
    }

    TArray<FTGComponentKinematicPose> InitialPoses;

    if (!UTGComponentKinematicsLibrary::
            EvaluateComponentTreeKinematics(
                ScenarioSnapshot,
                TArray<double>(),
                InitialPoses,
                OutAcceptedCoordinates,
                OutErrorText))
    {
        return false;
    }

    return ApplyComponentPoses(
        InitialPoses,
        OutErrorText);
}

bool ATGSpacecraftVisualActor::ApplyComponentPoses(
    const TArray<FTGComponentKinematicPose>& ComponentPoses,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (
        ComponentPoses.Num() !=
        ScenarioSnapshot.Components.Num())
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Expected %d component pose(s), "
                    "but received %d."),
                ScenarioSnapshot.Components.Num(),
                ComponentPoses.Num()));

        return false;
    }

    for (
        int32 PoseIndex = 0;
        PoseIndex < ComponentPoses.Num();
        ++PoseIndex)
    {
        const FTGComponentKinematicPose& Pose =
            ComponentPoses[PoseIndex];

        USceneComponent* const* FramePointer =
            ComponentFramesById.Find(
                Pose.ComponentId);

        if (
            FramePointer == nullptr ||
            *FramePointer == nullptr)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "No generated visual frame exists "
                        "for component pose '%s'."),
                    *Pose.ComponentName));

            return false;
        }

        const FTransform UnrealRelativeTransform =
            UTGComponentKinematicsLibrary::
                ConvertComponentPoseToUnrealTransform(
                    Pose);

        (*FramePointer)->SetRelativeTransform(
            UnrealRelativeTransform);
    }

    return true;
}

void ATGSpacecraftVisualActor::ClearSpacecraftVisuals()
{
    ClearSrpProxyPreview();

    ClearComponentHighlight();

    ComponentIdsByPrimitive.Reset();
    ComponentMeshesById.Reset();
    ComponentGeometryRootsById.Reset();
    ComponentFramesById.Reset();

    GeneratedMaterialInstances.Reset();
    GeneratedRuntimeTextures.Reset();

    for (
        int32 Index =
            GeneratedActorComponents.Num() - 1;
        Index >= 0;
        --Index)
    {
        UActorComponent* GeneratedComponent =
            GeneratedActorComponents[Index];

        if (IsValid(GeneratedComponent))
        {
            GeneratedComponent->DestroyComponent();
        }
    }

    GeneratedActorComponents.Reset();

    ScenarioSnapshot = FTGSimulationScenario();
    bHasBuiltSpacecraft = false;
}

bool ATGSpacecraftVisualActor::
    FindComponentIdFromPrimitive(
        UPrimitiveComponent* PrimitiveComponent,
        FGuid& OutComponentId) const
{
    OutComponentId.Invalidate();

    if (PrimitiveComponent == nullptr)
    {
        return false;
    }

    const FGuid* FoundId =
        ComponentIdsByPrimitive.Find(
            PrimitiveComponent);

    if (FoundId == nullptr)
    {
        return false;
    }

    OutComponentId = *FoundId;
    return true;
}

bool ATGSpacecraftVisualActor::ShowSrpProxyPreview(
    const FTGSimulationScenario& Scenario,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();
    ClearSrpProxyPreview();

    if (!bHasBuiltSpacecraft)
    {
        OutErrorText = FText::FromString(
            TEXT("The spacecraft visual must be built before its SRP proxy can be shown."));
        return false;
    }

    for (const FTGOpticalFacetConfig& Facet
         : Scenario.SolarRadiationPressure.OpticalFacets)
    {
        if (Facet.ComponentId.IsValid())
        {
            SrpFacetsByComponent.FindOrAdd(Facet.ComponentId).Add(Facet);
        }
    }

    for (TPair<FGuid, TArray<FTGOpticalFacetConfig>>& Pair
         : SrpFacetsByComponent)
    {
        USceneComponent* const* Frame = ComponentFramesById.Find(Pair.Key);
        if (Frame == nullptr || !IsValid(*Frame))
        {
            continue;
        }

        Pair.Value.Sort(
            [](const FTGOpticalFacetConfig& Left,
               const FTGOpticalFacetConfig& Right)
            {
                return Left.StableTriangleIndex < Right.StableTriangleIndex;
            });

        UProceduralMeshComponent* HitMesh =
            CreateGeneratedProceduralMeshComponent(
                FString::Printf(TEXT("SrpHitProxy_%s"), *Pair.Key.ToString()),
                *Frame);
        UProceduralMeshComponent* SelectionMesh =
            CreateGeneratedProceduralMeshComponent(
                FString::Printf(TEXT("SrpSelection_%s"), *Pair.Key.ToString()),
                *Frame);

        if (HitMesh == nullptr || SelectionMesh == nullptr)
        {
            OutErrorText = FText::FromString(
                TEXT("Failed to allocate an SRP preview mesh."));
            ClearSrpProxyPreview();
            return false;
        }

        SrpPreviewActorComponents.Add(HitMesh);
        SrpPreviewActorComponents.Add(SelectionMesh);
        SrpComponentIdsByPrimitive.Add(HitMesh, Pair.Key);
        SrpSelectionMeshesByComponent.Add(Pair.Key, SelectionMesh);

        TArray<FVector> Vertices;
        TArray<int32> TriangleIndices;
        TArray<FVector> Normals;
        TArray<FVector2D> Uvs;
        TArray<FLinearColor> Colors;
        TArray<FProcMeshTangent> Tangents;
        TArray<int32>& StableIndices =
            SrpStableIndicesByComponent.FindOrAdd(Pair.Key);

        Vertices.Reserve(Pair.Value.Num() * 3);
        TriangleIndices.Reserve(Pair.Value.Num() * 3);
        Normals.Reserve(Pair.Value.Num() * 3);
        Uvs.Reserve(Pair.Value.Num() * 3);
        Colors.Reserve(Pair.Value.Num() * 3);
        Tangents.Reserve(Pair.Value.Num() * 3);

        for (const FTGOpticalFacetConfig& Facet : Pair.Value)
        {
            FVector A(
                Facet.Vertex0Meters.X * 100.0,
                -Facet.Vertex0Meters.Y * 100.0,
                Facet.Vertex0Meters.Z * 100.0);
            FVector B(
                Facet.Vertex1Meters.X * 100.0,
                -Facet.Vertex1Meters.Y * 100.0,
                Facet.Vertex1Meters.Z * 100.0);
            FVector C(
                Facet.Vertex2Meters.X * 100.0,
                -Facet.Vertex2Meters.Y * 100.0,
                Facet.Vertex2Meters.Z * 100.0);
            const FVector Normal =
                FVector::CrossProduct(C - A, B - A).GetSafeNormal();
            const FVector HitOffset = Normal * 0.12;
            A += HitOffset;
            B += HitOffset;
            C += HitOffset;

            const int32 Base = Vertices.Num();
            Vertices.Append({ A, B, C });
            TriangleIndices.Append({ Base, Base + 1, Base + 2 });
            Normals.Append({ Normal, Normal, Normal });
            Uvs.Append({ FVector2D(0,0), FVector2D(1,0), FVector2D(0,1) });
            Colors.Append({ FLinearColor::White, FLinearColor::White, FLinearColor::White });
            Tangents.Append({ FProcMeshTangent(), FProcMeshTangent(), FProcMeshTangent() });
            StableIndices.Add(Facet.StableTriangleIndex);
        }

        HitMesh->bUseComplexAsSimpleCollision = true;
        HitMesh->CreateMeshSection_LinearColor(
            0,
            Vertices,
            TriangleIndices,
            Normals,
            Uvs,
            Colors,
            Tangents,
            true);
        HitMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
        HitMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
        HitMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
        HitMesh->SetGenerateOverlapEvents(false);
        HitMesh->SetVisibility(false, true);
        HitMesh->SetHiddenInGame(true, true);

        SelectionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SelectionMesh->SetVisibility(false, true);
        SelectionMesh->SetHiddenInGame(true, true);

        if (ComponentSurfaceMaterial != nullptr)
        {
            UMaterialInstanceDynamic* Material =
                UMaterialInstanceDynamic::Create(ComponentSurfaceMaterial, this);
            if (Material != nullptr)
            {
                Material->SetVectorParameterValue(
                    TGSpacecraftVisualActorPrivate::ParameterSolidColor,
                    FLinearColor(0.0f, 0.85f, 1.0f, 1.0f));
                Material->SetVectorParameterValue(
                    TGSpacecraftVisualActorPrivate::ParameterBaseColorTint,
                    FLinearColor(0.0f, 0.85f, 1.0f, 1.0f));
                SrpPreviewMaterials.Add(Material);
                GeneratedMaterialInstances.Add(Material);
                SelectionMesh->SetMaterial(0, Material);
            }
        }
    }

    return true;
}

void ATGSpacecraftVisualActor::ClearSrpProxyPreview()
{
    for (UActorComponent* Component : SrpPreviewActorComponents)
    {
        if (IsValid(Component))
        {
            GeneratedActorComponents.Remove(Component);
            Component->DestroyComponent();
        }
    }
    for (UMaterialInstanceDynamic* Material : SrpPreviewMaterials)
    {
        GeneratedMaterialInstances.Remove(Material);
    }

    SrpPreviewActorComponents.Reset();
    SrpPreviewMaterials.Reset();
    SrpComponentIdsByPrimitive.Reset();
    SrpStableIndicesByComponent.Reset();
    SrpFacetsByComponent.Reset();
    SrpSelectionMeshesByComponent.Reset();
}

void ATGSpacecraftVisualActor::SetSrpProxyHitTestingEnabled(bool bEnabled)
{
    const auto IsComponentVisible =
        [this](const FGuid& ComponentId)
        {
            UMeshComponent* const* Mesh =
                ComponentMeshesById.Find(ComponentId);
            return Mesh == nullptr ||
                !IsValid(*Mesh) ||
                (*Mesh)->IsVisible();
        };

    for (const TPair<const UPrimitiveComponent*, FGuid>& Pair
         : SrpComponentIdsByPrimitive)
    {
        UPrimitiveComponent* Primitive =
            const_cast<UPrimitiveComponent*>(Pair.Key);
        if (IsValid(Primitive))
        {
            Primitive->SetCollisionEnabled(
                bEnabled && IsComponentVisible(Pair.Value)
                    ? ECollisionEnabled::QueryOnly
                    : ECollisionEnabled::NoCollision);
        }
    }


    for (const TPair<FGuid, UProceduralMeshComponent*>& Pair
         : SrpSelectionMeshesByComponent)
    {
        if (IsValid(Pair.Value))
        {
            const bool bShowSelection =
                bEnabled &&
                IsComponentVisible(Pair.Key) &&
                Pair.Value->GetNumSections() > 0;
            Pair.Value->SetHiddenInGame(!bShowSelection, true);
            Pair.Value->SetVisibility(bShowSelection, true);
        }
    }
}

bool ATGSpacecraftVisualActor::FindSrpFacetFromPrimitive(
    UPrimitiveComponent* PrimitiveComponent,
    int32 HitFaceIndex,
    FGuid& OutComponentId,
    int32& OutStableTriangleIndex) const
{
    OutComponentId.Invalidate();
    OutStableTriangleIndex = INDEX_NONE;

    const FGuid* ComponentId =
        SrpComponentIdsByPrimitive.Find(PrimitiveComponent);
    if (ComponentId == nullptr)
    {
        return false;
    }

    const TArray<int32>* StableIndices =
        SrpStableIndicesByComponent.Find(*ComponentId);
    if (StableIndices == nullptr || !StableIndices->IsValidIndex(HitFaceIndex))
    {
        return false;
    }

    OutComponentId = *ComponentId;
    OutStableTriangleIndex = (*StableIndices)[HitFaceIndex];
    return true;
}

bool ATGSpacecraftVisualActor::SetSrpFacetSelectionHighlight(
    FGuid ComponentId,
    const TArray<int32>& StableTriangleIndices,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    for (const TPair<FGuid, UProceduralMeshComponent*>& Pair
         : SrpSelectionMeshesByComponent)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->ClearAllMeshSections();
            Pair.Value->SetVisibility(false, true);
            Pair.Value->SetHiddenInGame(true, true);
        }
    }

    if (StableTriangleIndices.IsEmpty())
    {
        return true;
    }

    UProceduralMeshComponent* const* SelectionMeshPointer =
        SrpSelectionMeshesByComponent.Find(ComponentId);
    const TArray<FTGOpticalFacetConfig>* Facets =
        SrpFacetsByComponent.Find(ComponentId);
    if (SelectionMeshPointer == nullptr || !IsValid(*SelectionMeshPointer) ||
        Facets == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component has no visible SRP proxy."));
        return false;
    }

    TSet<int32> Requested;
    Requested.Reserve(StableTriangleIndices.Num());
    for (const int32 StableTriangleIndex : StableTriangleIndices)
    {
        Requested.Add(StableTriangleIndex);
    }
    TArray<FVector> Vertices;
    TArray<int32> TriangleIndices;
    TArray<FVector> Normals;
    TArray<FVector2D> Uvs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    for (const FTGOpticalFacetConfig& Facet : *Facets)
    {
        if (!Requested.Contains(Facet.StableTriangleIndex))
        {
            continue;
        }

        FVector A(Facet.Vertex0Meters.X * 100.0,
            -Facet.Vertex0Meters.Y * 100.0, Facet.Vertex0Meters.Z * 100.0);
        FVector B(Facet.Vertex1Meters.X * 100.0,
            -Facet.Vertex1Meters.Y * 100.0, Facet.Vertex1Meters.Z * 100.0);
        FVector C(Facet.Vertex2Meters.X * 100.0,
            -Facet.Vertex2Meters.Y * 100.0, Facet.Vertex2Meters.Z * 100.0);
        const FVector Normal =
            FVector::CrossProduct(C - A, B - A).GetSafeNormal();
        const FVector Offset = Normal * 0.25;
        A += Offset;
        B += Offset;
        C += Offset;

        const int32 Base = Vertices.Num();
        Vertices.Append({ A, B, C });
        TriangleIndices.Append({ Base, Base + 1, Base + 2 });
        Normals.Append({ Normal, Normal, Normal });
        Uvs.Append({ FVector2D(0,0), FVector2D(1,0), FVector2D(0,1) });
        Colors.Append({ FLinearColor::White, FLinearColor::White, FLinearColor::White });
        Tangents.Append({ FProcMeshTangent(), FProcMeshTangent(), FProcMeshTangent() });
    }

    if (Vertices.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT("None of the selected SRP faces exists in the generated proxy."));
        return false;
    }

    UProceduralMeshComponent* SelectionMesh = *SelectionMeshPointer;
    SelectionMesh->CreateMeshSection_LinearColor(
        0, Vertices, TriangleIndices, Normals, Uvs, Colors, Tangents, false);
    if (!SrpPreviewMaterials.IsEmpty())
    {
        SelectionMesh->SetMaterial(0, SrpPreviewMaterials[0]);
    }
    SelectionMesh->SetHiddenInGame(false, true);
    SelectionMesh->SetVisibility(true, true);
    return true;
}

bool ATGSpacecraftVisualActor::SetSrpPrimitiveRegionSelectionHighlight(
    const FTGSimulationScenario& Scenario,
    FGuid ComponentId,
    const TArray<ETGSrpLogicalRegion>& Regions,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    for (const TPair<FGuid, UProceduralMeshComponent*>& Pair
         : SrpSelectionMeshesByComponent)
    {
        if (IsValid(Pair.Value))
        {
            Pair.Value->ClearAllMeshSections();
            Pair.Value->SetVisibility(false, true);
            Pair.Value->SetHiddenInGame(true, true);
        }
    }

    if (Regions.IsEmpty())
    {
        return true;
    }

    const FTGComponentConfig* Component =
        Scenario.Components.FindByPredicate(
            [&ComponentId](const FTGComponentConfig& Candidate)
            {
                return Candidate.ComponentId == ComponentId;
            });
    USceneComponent* const* Frame = ComponentFramesById.Find(ComponentId);
    if (Component == nullptr || Frame == nullptr || !IsValid(*Frame) ||
        Component->Visual.GeometrySource !=
            ETGComponentGeometrySource::Primitive)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected primitive region has no current visual frame."));
        return false;
    }

    UProceduralMeshComponent* SelectionMesh = nullptr;
    if (UProceduralMeshComponent* const* Existing =
            SrpSelectionMeshesByComponent.Find(ComponentId))
    {
        SelectionMesh = *Existing;
    }
    if (!IsValid(SelectionMesh))
    {
        SelectionMesh = CreateGeneratedProceduralMeshComponent(
            FString::Printf(
                TEXT("SrpPrimitiveRegionSelection_%s"),
                *ComponentId.ToString()),
            *Frame);
        if (!IsValid(SelectionMesh))
        {
            OutErrorText = FText::FromString(
                TEXT("Could not create the primitive-region selection overlay."));
            return false;
        }
        SrpPreviewActorComponents.Add(SelectionMesh);
        SrpSelectionMeshesByComponent.Add(ComponentId, SelectionMesh);
        SelectionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }

    TSet<ETGSrpLogicalRegion> SelectedRegions;
    SelectedRegions.Reserve(Regions.Num());
    for (const ETGSrpLogicalRegion Region : Regions)
    {
        SelectedRegions.Add(Region);
    }
    TArray<FVector> Vertices;
    TArray<int32> TriangleIndices;
    TArray<FVector> Normals;
    TArray<FVector2D> Uvs;
    TArray<FLinearColor> Colors;
    TArray<FProcMeshTangent> Tangents;

    const auto AddTriangle =
        [&Vertices, &TriangleIndices, &Normals, &Uvs, &Colors, &Tangents](
            const FVector& BackendA,
            const FVector& BackendB,
            const FVector& BackendC)
        {
            FVector A(BackendA.X * 100.0, -BackendA.Y * 100.0, BackendA.Z * 100.0);
            FVector B(BackendB.X * 100.0, -BackendB.Y * 100.0, BackendB.Z * 100.0);
            FVector C(BackendC.X * 100.0, -BackendC.Y * 100.0, BackendC.Z * 100.0);
            const FVector Normal =
                FVector::CrossProduct(C - A, B - A).GetSafeNormal();
            const FVector Offset = Normal * 0.25;
            A += Offset;
            B += Offset;
            C += Offset;
            const int32 Base = Vertices.Num();
            Vertices.Append({ A, B, C });
            TriangleIndices.Append({ Base, Base + 1, Base + 2 });
            Normals.Append({ Normal, Normal, Normal });
            Uvs.Append({ FVector2D(0,0), FVector2D(1,0), FVector2D(0,1) });
            Colors.Append({ FLinearColor::White, FLinearColor::White, FLinearColor::White });
            Tangents.Append({ FProcMeshTangent(), FProcMeshTangent(), FProcMeshTangent() });
        };

    const FTGComponentVisualConfig& Visual = Component->Visual;
    if (Visual.PrimitiveType == ETGPrimitiveGeometryType::Box)
    {
        const FVector H = Visual.BoxDimensionsMeters * 0.5;
        struct FFace
        {
            ETGSrpLogicalRegion Region;
            FVector A;
            FVector B;
            FVector C;
            FVector D;
        };
        const FFace Faces[] =
        {
            { ETGSrpLogicalRegion::BoxPositiveX, {H.X,-H.Y,-H.Z}, {H.X,H.Y,-H.Z}, {H.X,H.Y,H.Z}, {H.X,-H.Y,H.Z} },
            { ETGSrpLogicalRegion::BoxNegativeX, {-H.X,-H.Y,-H.Z}, {-H.X,-H.Y,H.Z}, {-H.X,H.Y,H.Z}, {-H.X,H.Y,-H.Z} },
            { ETGSrpLogicalRegion::BoxPositiveY, {-H.X,H.Y,-H.Z}, {-H.X,H.Y,H.Z}, {H.X,H.Y,H.Z}, {H.X,H.Y,-H.Z} },
            { ETGSrpLogicalRegion::BoxNegativeY, {-H.X,-H.Y,-H.Z}, {H.X,-H.Y,-H.Z}, {H.X,-H.Y,H.Z}, {-H.X,-H.Y,H.Z} },
            { ETGSrpLogicalRegion::BoxPositiveZ, {-H.X,-H.Y,H.Z}, {H.X,-H.Y,H.Z}, {H.X,H.Y,H.Z}, {-H.X,H.Y,H.Z} },
            { ETGSrpLogicalRegion::BoxNegativeZ, {-H.X,-H.Y,-H.Z}, {-H.X,H.Y,-H.Z}, {H.X,H.Y,-H.Z}, {H.X,-H.Y,-H.Z} }
        };
        for (const FFace& Face : Faces)
        {
            if (SelectedRegions.Contains(Face.Region))
            {
                AddTriangle(Face.A, Face.B, Face.C);
                AddTriangle(Face.A, Face.C, Face.D);
            }
        }
    }
    else if (Visual.PrimitiveType == ETGPrimitiveGeometryType::Cylinder)
    {
        constexpr int32 Segments = 32;
        const double Radius = Visual.CylinderRadiusMeters;
        const double HalfLength = Visual.CylinderLengthMeters * 0.5;
        for (int32 Segment = 0; Segment < Segments; ++Segment)
        {
            const double A0 = 2.0 * UE_DOUBLE_PI * Segment / Segments;
            const double A1 = 2.0 * UE_DOUBLE_PI * (Segment + 1) / Segments;
            const FVector P0(Radius * FMath::Cos(A0), Radius * FMath::Sin(A0), -HalfLength);
            const FVector P1(Radius * FMath::Cos(A1), Radius * FMath::Sin(A1), -HalfLength);
            const FVector P2(P1.X, P1.Y, HalfLength);
            const FVector P3(P0.X, P0.Y, HalfLength);
            if (SelectedRegions.Contains(ETGSrpLogicalRegion::CylinderSide))
            {
                AddTriangle(P0, P1, P2);
                AddTriangle(P0, P2, P3);
            }
            if (SelectedRegions.Contains(ETGSrpLogicalRegion::CylinderPositiveCap))
            {
                AddTriangle(FVector(0,0,HalfLength), P3, P2);
            }
            if (SelectedRegions.Contains(ETGSrpLogicalRegion::CylinderNegativeCap))
            {
                AddTriangle(FVector(0,0,-HalfLength), P1, P0);
            }
        }
    }

    if (Vertices.IsEmpty())
    {
        return true;
    }

    SelectionMesh->CreateMeshSection_LinearColor(
        0, Vertices, TriangleIndices, Normals, Uvs, Colors, Tangents, false);
    UMaterialInstanceDynamic* Material =
        SrpPreviewMaterials.IsEmpty()
            ? nullptr
            : SrpPreviewMaterials[0];
    if (Material == nullptr && ComponentSurfaceMaterial != nullptr)
    {
        Material = UMaterialInstanceDynamic::Create(ComponentSurfaceMaterial, this);
        if (Material != nullptr)
        {
            Material->SetVectorParameterValue(
                TGSpacecraftVisualActorPrivate::ParameterSolidColor,
                FLinearColor(0.0f, 0.85f, 1.0f, 1.0f));
            Material->SetVectorParameterValue(
                TGSpacecraftVisualActorPrivate::ParameterBaseColorTint,
                FLinearColor(0.0f, 0.85f, 1.0f, 1.0f));
            SrpPreviewMaterials.Add(Material);
            GeneratedMaterialInstances.Add(Material);
        }
    }
    if (Material != nullptr)
    {
        SelectionMesh->SetMaterial(0, Material);
    }
    SelectionMesh->SetHiddenInGame(false, true);
    SelectionMesh->SetVisibility(true, true);
    return true;
}

bool ATGSpacecraftVisualActor::SetComponentHighlight(
    FGuid ComponentId,
    int32 StencilValue,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (!ComponentId.IsValid())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The component highlight identifier "
                "is invalid."));

        return false;
    }

    const int32 AcceptedStencilValue =
        FMath::Clamp(
            StencilValue,
            1,
            255);

    /*
     * There must be only one highlighted physical component.
     */
    ClearComponentHighlight();

    TArray<UPrimitiveComponent*> PrimitiveComponents;

    GetComponents<UPrimitiveComponent>(
        PrimitiveComponents);

    bool bFoundComponentPrimitive = false;

    for (
        UPrimitiveComponent* PrimitiveComponent :
        PrimitiveComponents)
    {
        if (!IsValid(PrimitiveComponent))
        {
            continue;
        }

        FGuid PrimitiveComponentId;

        if (
            !FindComponentIdFromPrimitive(
                PrimitiveComponent,
                PrimitiveComponentId) ||
            PrimitiveComponentId != ComponentId)
        {
            continue;
        }

        PrimitiveComponent->
            SetCustomDepthStencilValue(
                AcceptedStencilValue);

        PrimitiveComponent->
            SetRenderCustomDepth(true);

        bFoundComponentPrimitive = true;
    }

    if (!bFoundComponentPrimitive)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected component does not have "
                "a generated primitive that can be highlighted."));

        return false;
    }

    HighlightedComponentId = ComponentId;
    HighlightStencilValue = AcceptedStencilValue;

    return true;
}

void ATGSpacecraftVisualActor::
    ClearComponentHighlight()
{
    HighlightedComponentId.Invalidate();
    HighlightStencilValue = 0;

    TArray<UPrimitiveComponent*> PrimitiveComponents;

    GetComponents<UPrimitiveComponent>(
        PrimitiveComponents);

    for (
        UPrimitiveComponent* PrimitiveComponent :
        PrimitiveComponents)
    {
        if (!IsValid(PrimitiveComponent))
        {
            continue;
        }

        FGuid PrimitiveComponentId;

        /*
         * Only touch primitives registered as physical-component
         * geometry. Other primitive components owned by this actor
         * must remain unchanged.
         */
        if (!FindComponentIdFromPrimitive(
                PrimitiveComponent,
                PrimitiveComponentId))
        {
            continue;
        }

        PrimitiveComponent->
            SetRenderCustomDepth(false);

        PrimitiveComponent->
            SetCustomDepthStencilValue(0);
    }
}

bool ATGSpacecraftVisualActor::GetSpacecraftBounds(
    FVector& OutWorldCenter,
    FVector& OutWorldExtent,
    double& OutSphereRadius) const
{
    OutWorldCenter = GetActorLocation();
    OutWorldExtent = FVector::ZeroVector;
    OutSphereRadius = 0.0;

    if (ComponentMeshesById.IsEmpty())
    {
        return false;
    }

    FBox CombinedBounds(
        EForceInit::ForceInit);

    for (
        const TPair<
            FGuid,
            UMeshComponent*>& Pair
        : ComponentMeshesById)
    {
        const UMeshComponent* MeshComponent =
            Pair.Value;

        if (
            MeshComponent == nullptr ||
            !MeshComponent->IsVisible())
        {
            continue;
        }

        CombinedBounds +=
            MeshComponent->Bounds.GetBox();
    }

    if (!CombinedBounds.IsValid)
    {
        return false;
    }

    OutWorldCenter =
        CombinedBounds.GetCenter();

    OutWorldExtent =
        CombinedBounds.GetExtent();

    OutSphereRadius =
        OutWorldExtent.Length();

    return true;
}

bool ATGSpacecraftVisualActor::
    GetComponentWorldTransformById(
        FGuid ComponentId,
        FTransform& OutWorldTransform) const
{
    OutWorldTransform = FTransform::Identity;

    if (!ComponentId.IsValid())
    {
        return false;
    }

    USceneComponent* const* FramePointer =
        ComponentFramesById.Find(ComponentId);

    if (
        FramePointer == nullptr ||
        *FramePointer == nullptr ||
        !IsValid(*FramePointer))
    {
        return false;
    }

    OutWorldTransform =
        (*FramePointer)->GetComponentTransform();

    return true;
}

int32 ATGSpacecraftVisualActor::
    GetGeneratedPhysicalComponentCount() const
{
    return ComponentFramesById.Num();
}

bool ATGSpacecraftVisualActor::
    HasBuiltSpacecraft() const
{
    return bHasBuiltSpacecraft;
}

USceneComponent*
ATGSpacecraftVisualActor::
    CreateGeneratedSceneComponent(
        const FString& BaseName,
        USceneComponent* AttachParent)
{
    if (AttachParent == nullptr)
    {
        return nullptr;
    }

    const FName UniqueName =
        MakeUniqueObjectName(
            this,
            USceneComponent::StaticClass(),
            FName(*BaseName));

    USceneComponent* NewComponent =
        NewObject<USceneComponent>(
            this,
            UniqueName);

    if (NewComponent == nullptr)
    {
        return nullptr;
    }

    NewComponent->SetMobility(
        EComponentMobility::Movable);

    NewComponent->SetupAttachment(
        AttachParent);

    AddInstanceComponent(
        NewComponent);

    GeneratedActorComponents.Add(
        NewComponent);

    NewComponent->RegisterComponent();

    return NewComponent;
}

UStaticMeshComponent*
ATGSpacecraftVisualActor::
    CreateGeneratedStaticMeshComponent(
        const FString& BaseName,
        USceneComponent* AttachParent)
{
    if (AttachParent == nullptr)
    {
        return nullptr;
    }

    const FName UniqueName =
        MakeUniqueObjectName(
            this,
            UStaticMeshComponent::StaticClass(),
            FName(*BaseName));

    UStaticMeshComponent* NewComponent =
        NewObject<UStaticMeshComponent>(
            this,
            UniqueName);

    if (NewComponent == nullptr)
    {
        return nullptr;
    }

    NewComponent->SetMobility(
        EComponentMobility::Movable);

    NewComponent->SetupAttachment(
        AttachParent);

    AddInstanceComponent(
        NewComponent);

    GeneratedActorComponents.Add(
        NewComponent);

    NewComponent->RegisterComponent();

    return NewComponent;
}

UProceduralMeshComponent*
ATGSpacecraftVisualActor::
    CreateGeneratedProceduralMeshComponent(
        const FString& BaseName,
        USceneComponent* AttachParent)
{
    if (AttachParent == nullptr)
    {
        return nullptr;
    }

    const FName UniqueName =
        MakeUniqueObjectName(
            this,
            UProceduralMeshComponent::StaticClass(),
            FName(*BaseName));

    UProceduralMeshComponent* NewComponent =
        NewObject<UProceduralMeshComponent>(
            this,
            UniqueName);

    if (NewComponent == nullptr)
    {
        return nullptr;
    }

    NewComponent->SetMobility(
        EComponentMobility::Movable);

    NewComponent->SetupAttachment(
        AttachParent);

    AddInstanceComponent(
        NewComponent);

    GeneratedActorComponents.Add(
        NewComponent);

    NewComponent->RegisterComponent();

    return NewComponent;
}

bool ATGSpacecraftVisualActor::
    CreateConfiguredVisualMesh(
        const FTGComponentConfig& Component,
        int32 ComponentIndex,
        USceneComponent* GeometryRoot,
        UMeshComponent*& OutMeshComponent,
        FText& OutErrorText)
{
    OutMeshComponent = nullptr;
    OutErrorText = FText::GetEmpty();

    if (GeometryRoot == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Cannot create visual geometry without a geometry root."));

        return false;
    }

    const FTGComponentVisualConfig& Visual =
        Component.Visual;

    if (
        Visual.GeometrySource ==
        ETGComponentGeometrySource::NoGeometry)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Component '%s' uses the legacy No Geometry mode. "
                    "The current Component Visual Appearance editor "
                    "requires Standard Primitive or Custom STL."),
                *Component.Name));

        return false;
    }

    if (
        Visual.GeometrySource ==
        ETGComponentGeometrySource::Primitive)
    {
        UStaticMeshComponent* StaticMeshComponent =
            CreateGeneratedStaticMeshComponent(
                FString::Printf(
                    TEXT("PrimitiveMesh_%d"),
                    ComponentIndex),
                GeometryRoot);

        if (StaticMeshComponent == nullptr)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Failed to create the primitive mesh "
                        "for component '%s'."),
                    *Component.Name));

            return false;
        }

        if (!ConfigurePrimitiveMesh(
                StaticMeshComponent,
                Component,
                OutErrorText))
        {
            GeneratedActorComponents.Remove(
                StaticMeshComponent);

            StaticMeshComponent->DestroyComponent();
            return false;
        }

        TGSpacecraftVisualActorPrivate::
            ConfigureGeneratedMeshCollision(
                StaticMeshComponent);

        OutMeshComponent = StaticMeshComponent;
        return true;
    }

    if (
        Visual.GeometrySource ==
        ETGComponentGeometrySource::CustomStl)
    {
        UProceduralMeshComponent* ProceduralMeshComponent =
            CreateGeneratedProceduralMeshComponent(
                FString::Printf(
                    TEXT("StlMesh_%d"),
                    ComponentIndex),
                GeometryRoot);

        if (ProceduralMeshComponent == nullptr)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Failed to create the STL mesh "
                        "for component '%s'."),
                    *Component.Name));

            return false;
        }

        if (!ConfigureStlMesh(
                ProceduralMeshComponent,
                Component,
                OutErrorText))
        {
            GeneratedActorComponents.Remove(
                ProceduralMeshComponent);

            ProceduralMeshComponent->DestroyComponent();
            return false;
        }

        TGSpacecraftVisualActorPrivate::
            ConfigureGeneratedMeshCollision(
                ProceduralMeshComponent);

        OutMeshComponent = ProceduralMeshComponent;
        return true;
    }

    OutErrorText = FText::FromString(
        FString::Printf(
            TEXT(
                "Component '%s' has an unsupported geometry source."),
            *Component.Name));

    return false;
}

UStaticMesh*
ATGSpacecraftVisualActor::ResolvePrimitiveMesh(
    ETGPrimitiveGeometryType PrimitiveType) const
{
    switch (PrimitiveType)
    {
        case ETGPrimitiveGeometryType::Box:
            return BoxPrimitiveMesh;

        case ETGPrimitiveGeometryType::Sphere:
            return SpherePrimitiveMesh;

        case ETGPrimitiveGeometryType::Cylinder:
            return CylinderPrimitiveMesh;
    }

    return nullptr;
}

FVector
ATGSpacecraftVisualActor::CalculatePrimitiveMeshScale(
    const FTGComponentVisualConfig& VisualConfig) const
{
    /*
     * Unreal's standard Engine shapes have these unscaled dimensions:
     *
     * Cube:
     *   100 cm × 100 cm × 100 cm
     *
     * Sphere:
     *   radius 50 cm
     *
     * Cylinder:
     *   radius 50 cm and length 100 cm along local Z
     *
     * Therefore dimensions expressed in meters map directly to the scale
     * factors below.
     */
    switch (VisualConfig.PrimitiveType)
    {
        case ETGPrimitiveGeometryType::Box:
            return VisualConfig.BoxDimensionsMeters;

        case ETGPrimitiveGeometryType::Sphere:
        {
            const double DiameterMeters =
                2.0 *
                VisualConfig.SphereRadiusMeters;

            return FVector(
                DiameterMeters,
                DiameterMeters,
                DiameterMeters);
        }

        case ETGPrimitiveGeometryType::Cylinder:
        {
            const double DiameterMeters =
                2.0 *
                VisualConfig.CylinderRadiusMeters;

            return FVector(
                DiameterMeters,
                DiameterMeters,
                VisualConfig.CylinderLengthMeters);
        }
    }

    return FVector::OneVector;
}

bool ATGSpacecraftVisualActor::ConfigurePrimitiveMesh(
    UStaticMeshComponent* MeshComponent,
    const FTGComponentConfig& Component,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (MeshComponent == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Cannot configure a null primitive mesh component."));

        return false;
    }

    const FTGComponentVisualConfig& Visual =
        Component.Visual;

    if (
        Visual.GeometrySource !=
        ETGComponentGeometrySource::Primitive)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Component '%s' is not configured as a "
                    "standard primitive."),
                *Component.Name));

        return false;
    }

    UStaticMesh* PrimitiveMesh =
        ResolvePrimitiveMesh(
            Visual.PrimitiveType);

    if (PrimitiveMesh == nullptr)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "The standard primitive mesh required by "
                    "component '%s' could not be loaded."),
                *Component.Name));

        return false;
    }

    MeshComponent->SetStaticMesh(
        PrimitiveMesh);

    MeshComponent->SetRelativeLocation(
        FVector::ZeroVector);

    MeshComponent->SetRelativeRotation(
        FQuat::Identity);

    MeshComponent->SetRelativeScale3D(
        CalculatePrimitiveMeshScale(
            Visual));

    MeshComponent->SetVisibility(
        Visual.bVisible,
        true);

    MeshComponent->SetHiddenInGame(
        !Visual.bVisible,
        true);

    return ApplyWholeComponentSurfaceAppearance(
        MeshComponent,
        Component,
        OutErrorText);
}

bool ATGSpacecraftVisualActor::ConfigureStlMesh(
    UProceduralMeshComponent* MeshComponent,
    const FTGComponentConfig& Component,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (MeshComponent == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Cannot configure a null STL mesh component."));

        return false;
    }

    const FTGComponentVisualConfig& Visual =
        Component.Visual;

    if (
        Visual.GeometrySource !=
        ETGComponentGeometrySource::CustomStl)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Component '%s' is not configured as Custom STL."),
                *Component.Name));

        return false;
    }

    const FString StlFilePath =
        TGSpacecraftVisualActorPrivate::
            NormalizeRuntimeFilePath(
                Visual.StlFilePath);

    if (
        StlFilePath.IsEmpty() ||
        !FPaths::FileExists(StlFilePath))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "The STL file for component '%s' does not exist."),
                *Component.Name));

        return false;
    }

    const double UnitsToCentimeters =
        TGSpacecraftVisualActorPrivate::
            GetStlUnitsToCentimeters(
                Visual.StlLengthUnit);

    if (!(UnitsToCentimeters > 0.0))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Component '%s' has an invalid STL length unit."),
                *Component.Name));

        return false;
    }

    TArray<TGSpacecraftVisualActorPrivate::FStlTriangle>
        SourceTriangles;

    if (!TGSpacecraftVisualActorPrivate::
            LoadStlTriangles(
                StlFilePath,
                SourceTriangles,
                OutErrorText))
    {
        return false;
    }

    TArray<TGSpacecraftVisualActorPrivate::FStlTriangle>
        ConvertedTriangles;

    ConvertedTriangles.Reserve(
        SourceTriangles.Num());

    FBox LocalBounds(
        EForceInit::ForceInit);

    for (
        const TGSpacecraftVisualActorPrivate::FStlTriangle&
            SourceTriangle :
        SourceTriangles)
    {
        TGSpacecraftVisualActorPrivate::FStlTriangle
            Triangle;

        Triangle.A =
            TGSpacecraftVisualActorPrivate::
                ConvertStlVertexToUnreal(
                    SourceTriangle.A,
                    UnitsToCentimeters);

        /*
         * Keep the source vertex order after the Y reflection. Unreal uses
         * that reflected winding as the visible front face. Shading normals
         * are reversed below so they still point outward.
         */
        Triangle.B =
            TGSpacecraftVisualActorPrivate::
                ConvertStlVertexToUnreal(
                    SourceTriangle.B,
                    UnitsToCentimeters);

        Triangle.C =
            TGSpacecraftVisualActorPrivate::
                ConvertStlVertexToUnreal(
                    SourceTriangle.C,
                    UnitsToCentimeters);

        if (
            !TGSpacecraftVisualActorPrivate::
                IsFiniteVector(Triangle.A) ||
            !TGSpacecraftVisualActorPrivate::
                IsFiniteVector(Triangle.B) ||
            !TGSpacecraftVisualActorPrivate::
                IsFiniteVector(Triangle.C))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "STL geometry for component '%s' contains "
                        "non-finite coordinates after unit conversion."),
                    *Component.Name));

            return false;
        }

        const FVector FaceNormal =
            FVector::CrossProduct(
                Triangle.C - Triangle.A,
                Triangle.B - Triangle.A);

        if (
            FaceNormal.SizeSquared() <=
            1.0e-12)
        {
            continue;
        }

        ConvertedTriangles.Add(Triangle);

        LocalBounds += Triangle.A;
        LocalBounds += Triangle.B;
        LocalBounds += Triangle.C;
    }

    if (
        ConvertedTriangles.IsEmpty() ||
        !LocalBounds.IsValid)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "STL geometry for component '%s' contains no "
                    "non-degenerate triangles."),
                *Component.Name));

        return false;
    }

    const int32 VertexCount =
        ConvertedTriangles.Num() * 3;

    TArray<FVector> Vertices;
    TArray<int32> Triangles;
    TArray<FVector> Normals;
    TArray<FVector2D> UV0;
    TArray<FLinearColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    Vertices.Reserve(VertexCount);
    Triangles.Reserve(VertexCount);
    Normals.Reserve(VertexCount);
    UV0.Reserve(VertexCount);
    VertexColors.Reserve(VertexCount);
    Tangents.Reserve(VertexCount);

    for (
        const TGSpacecraftVisualActorPrivate::FStlTriangle&
            Triangle :
        ConvertedTriangles)
    {
        const FVector FaceNormal =
            FVector::CrossProduct(
                Triangle.C - Triangle.A,
                Triangle.B - Triangle.A).
                GetSafeNormal();

        const FVector AbsoluteNormal(
            FMath::Abs(FaceNormal.X),
            FMath::Abs(FaceNormal.Y),
            FMath::Abs(FaceNormal.Z));

        int32 ProjectionAxis = 2;
        FVector TangentX = FVector::ForwardVector;

        if (
            AbsoluteNormal.X >= AbsoluteNormal.Y &&
            AbsoluteNormal.X >= AbsoluteNormal.Z)
        {
            ProjectionAxis = 0;
            TangentX = FVector::YAxisVector;
        }
        else if (
            AbsoluteNormal.Y >= AbsoluteNormal.X &&
            AbsoluteNormal.Y >= AbsoluteNormal.Z)
        {
            ProjectionAxis = 1;
            TangentX = FVector::XAxisVector;
        }

        const FVector TriangleVertices[3] =
        {
            Triangle.A,
            Triangle.B,
            Triangle.C
        };

        const int32 BaseVertexIndex =
            Vertices.Num();

        for (int32 LocalVertexIndex = 0; LocalVertexIndex < 3; ++LocalVertexIndex)
        {
            const FVector& Vertex =
                TriangleVertices[LocalVertexIndex];

            Vertices.Add(Vertex);
            Triangles.Add(
                BaseVertexIndex + LocalVertexIndex);
            Normals.Add(FaceNormal);

            UV0.Add(
                TGSpacecraftVisualActorPrivate::
                    MakeProjectedUv(
                        Vertex,
                        LocalBounds,
                        ProjectionAxis));

            VertexColors.Add(
                FLinearColor::White);

            Tangents.Add(
                FProcMeshTangent(
                    TangentX,
                    false));
        }
    }

    MeshComponent->ClearAllMeshSections();

    MeshComponent->bUseComplexAsSimpleCollision = true;
    MeshComponent->bUseAsyncCooking = true;

    /*
     * STL contains no UV channel. The importer generates component-local
     * box-projected UV0 coordinates so the existing whole-component material
     * can render textures without world-space sliding. A later material-only
     * upgrade can replace this with blended triplanar projection.
     */
    MeshComponent->CreateMeshSection_LinearColor(
        0,
        Vertices,
        Triangles,
        Normals,
        UV0,
        VertexColors,
        Tangents,
        true);

    MeshComponent->SetRelativeLocation(
        FVector::ZeroVector);

    MeshComponent->SetRelativeRotation(
        FQuat::Identity);

    MeshComponent->SetRelativeScale3D(
        FVector::OneVector);

    MeshComponent->SetVisibility(
        Visual.bVisible,
        true);

    MeshComponent->SetHiddenInGame(
        !Visual.bVisible,
        true);

    return ApplyWholeComponentSurfaceAppearance(
        MeshComponent,
        Component,
        OutErrorText);
}

bool ATGSpacecraftVisualActor::
    ApplyWholeComponentSurfaceAppearance(
        UMeshComponent* MeshComponent,
        const FTGComponentConfig& Component,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (MeshComponent == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Cannot apply surface appearance to a null mesh."));

        return false;
    }

    const FTGComponentVisualConfig& Visual =
        Component.Visual;

    const float ActiveOpacity =
        Visual.SurfaceAppearanceMode ==
            ETGComponentSurfaceAppearanceMode::Textured
            ? Visual.BaseColorTint.A
            : Visual.DisplayColor.A;

    const bool bRequiresTranslucentMaterial =
        FMath::Clamp(ActiveOpacity, 0.0f, 1.0f) <
        1.0f - KINDA_SMALL_NUMBER;

    UMaterialInterface* SurfaceMaterial =
        bRequiresTranslucentMaterial
            ? ComponentTranslucentSurfaceMaterial.Get()
            : ComponentSurfaceMaterial.Get();

    if (SurfaceMaterial == nullptr)
    {
        OutErrorText = FText::FromString(
            bRequiresTranslucentMaterial
                ? TEXT(
                    "The translucent component surface material is missing. "
                    "Create /Game/UI/Configuration/ComponentTree/Materials/"
                    "M_TGComponentSurface_Translucent or assign "
                    "ComponentTranslucentSurfaceMaterial on the visual actor.")
                : TEXT(
                    "The reusable component surface material is missing. "
                    "Create /Game/UI/Configuration/ComponentTree/Materials/"
                    "M_TGComponentSurface or assign ComponentSurfaceMaterial "
                    "on the visual actor."));

        return false;
    }

    UMaterialInstanceDynamic* MaterialInstance =
        UMaterialInstanceDynamic::Create(
            SurfaceMaterial,
            this);

    if (MaterialInstance == nullptr)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Failed to create the dynamic material instance "
                    "for component '%s'."),
                *Component.Name));

        return false;
    }

    MaterialInstance->SetVectorParameterValue(
        TGSpacecraftVisualActorPrivate::
            ParameterSolidColor,
        Visual.DisplayColor);

    MaterialInstance->SetVectorParameterValue(
        TGSpacecraftVisualActorPrivate::
            ParameterBaseColorTint,
        Visual.BaseColorTint);

    MaterialInstance->SetScalarParameterValue(
        TGSpacecraftVisualActorPrivate::
            ParameterUseBaseColorTexture,
        0.0f);

    MaterialInstance->SetScalarParameterValue(
        TGSpacecraftVisualActorPrivate::
            ParameterUseNormalTexture,
        0.0f);

    MaterialInstance->SetScalarParameterValue(
        TGSpacecraftVisualActorPrivate::
            ParameterUseRoughnessTexture,
        0.0f);

    MaterialInstance->SetScalarParameterValue(
        TGSpacecraftVisualActorPrivate::
            ParameterUseMetallicTexture,
        0.0f);

    if (
        Visual.SurfaceAppearanceMode ==
        ETGComponentSurfaceAppearanceMode::Textured)
    {
        UTexture2D* BaseColorTexture =
            LoadRuntimeTexture(
                Visual.BaseColorTextureFilePath,
                EGeneratedTextureUsage::BaseColor,
                OutErrorText);

        if (BaseColorTexture == nullptr)
        {
            return false;
        }

        MaterialInstance->SetTextureParameterValue(
            TGSpacecraftVisualActorPrivate::
                ParameterBaseColorTexture,
            BaseColorTexture);

        MaterialInstance->SetScalarParameterValue(
            TGSpacecraftVisualActorPrivate::
                ParameterUseBaseColorTexture,
            1.0f);

        if (!Visual.NormalTextureFilePath.IsEmpty())
        {
            UTexture2D* NormalTexture =
                LoadRuntimeTexture(
                    Visual.NormalTextureFilePath,
                    EGeneratedTextureUsage::Normal,
                    OutErrorText);

            if (NormalTexture == nullptr)
            {
                return false;
            }

            MaterialInstance->SetTextureParameterValue(
                TGSpacecraftVisualActorPrivate::
                    ParameterNormalTexture,
                NormalTexture);

            MaterialInstance->SetScalarParameterValue(
                TGSpacecraftVisualActorPrivate::
                    ParameterUseNormalTexture,
                1.0f);
        }

        if (!Visual.RoughnessTextureFilePath.IsEmpty())
        {
            UTexture2D* RoughnessTexture =
                LoadRuntimeTexture(
                    Visual.RoughnessTextureFilePath,
                    EGeneratedTextureUsage::Mask,
                    OutErrorText);

            if (RoughnessTexture == nullptr)
            {
                return false;
            }

            MaterialInstance->SetTextureParameterValue(
                TGSpacecraftVisualActorPrivate::
                    ParameterRoughnessTexture,
                RoughnessTexture);

            MaterialInstance->SetScalarParameterValue(
                TGSpacecraftVisualActorPrivate::
                    ParameterUseRoughnessTexture,
                1.0f);
        }

        if (!Visual.MetallicTextureFilePath.IsEmpty())
        {
            UTexture2D* MetallicTexture =
                LoadRuntimeTexture(
                    Visual.MetallicTextureFilePath,
                    EGeneratedTextureUsage::Mask,
                    OutErrorText);

            if (MetallicTexture == nullptr)
            {
                return false;
            }

            MaterialInstance->SetTextureParameterValue(
                TGSpacecraftVisualActorPrivate::
                    ParameterMetallicTexture,
                MetallicTexture);

            MaterialInstance->SetScalarParameterValue(
                TGSpacecraftVisualActorPrivate::
                    ParameterUseMetallicTexture,
                1.0f);
        }
    }

    MeshComponent->SetMaterial(
        0,
        MaterialInstance);

    GeneratedMaterialInstances.Add(
        MaterialInstance);

    return true;
}

UTexture2D* ATGSpacecraftVisualActor::LoadRuntimeTexture(
    const FString& FilePath,
    EGeneratedTextureUsage Usage,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    const FString NormalizedPath =
        TGSpacecraftVisualActorPrivate::
            NormalizeRuntimeFilePath(
                FilePath);

    if (NormalizedPath.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "A required runtime texture file path is empty."));

        return nullptr;
    }

    if (!FPaths::FileExists(NormalizedPath))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Runtime texture file does not exist: %s"),
                *NormalizedPath));

        return nullptr;
    }

    UTexture2D* Texture =
        UKismetRenderingLibrary::
            ImportFileAsTexture2D(
                this,
                NormalizedPath);

    if (Texture == nullptr)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Unreal could not import runtime texture file: %s"),
                *NormalizedPath));

        return nullptr;
    }

    switch (Usage)
    {
        case EGeneratedTextureUsage::BaseColor:
            Texture->SRGB = true;
            Texture->CompressionSettings =
                TextureCompressionSettings::TC_Default;
            break;

        case EGeneratedTextureUsage::Normal:
            Texture->SRGB = false;
            Texture->CompressionSettings =
                TextureCompressionSettings::TC_Normalmap;
            break;

        case EGeneratedTextureUsage::Mask:
            Texture->SRGB = false;
            Texture->CompressionSettings =
                TextureCompressionSettings::TC_Masks;
            break;
    }

    Texture->UpdateResource();

    GeneratedRuntimeTextures.Add(
        Texture);

    return Texture;
}

bool ATGSpacecraftVisualActor::
    ValidateVisualRefreshTopology(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText) const
{
    OutErrorText = FText::GetEmpty();

    if (
        Scenario.Components.Num() !=
            ScenarioSnapshot.Components.Num() ||
        Scenario.Components.Num() !=
            ComponentFramesById.Num() ||
        Scenario.Components.Num() !=
            ComponentGeometryRootsById.Num() ||
        Scenario.Components.Num() !=
            ComponentMeshesById.Num())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The spacecraft component topology changed. "
                "Rebuild the spacecraft visual actor instead."));

        return false;
    }

    if (!ValidateComponentIdentifiers(
            Scenario,
            OutErrorText))
    {
        return false;
    }

    for (const FTGComponentConfig& Component : Scenario.Components)
    {
        USceneComponent* const* ExistingFrame =
            ComponentFramesById.Find(
                Component.ComponentId);

        if (
            ExistingFrame == nullptr ||
            *ExistingFrame == nullptr ||
            !IsValid(*ExistingFrame))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The spacecraft component topology changed. "
                    "Rebuild the spacecraft visual actor instead."));

            return false;
        }
    }

    return true;
}

bool ATGSpacecraftVisualActor::
    ValidateComponentIdentifiers(
        const FTGSimulationScenario& Scenario,
        FText& OutErrorText) const
{
    OutErrorText = FText::GetEmpty();

    TSet<FGuid> UsedComponentIds;

    for (
        int32 ComponentIndex = 0;
        ComponentIndex < Scenario.Components.Num();
        ++ComponentIndex)
    {
        const FTGComponentConfig& Component =
            Scenario.Components[ComponentIndex];

        if (!Component.ComponentId.IsValid())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Components[%d] has an invalid "
                        "component identifier. Normalize the "
                        "scenario component tree first."),
                    ComponentIndex));

            return false;
        }

        if (UsedComponentIds.Contains(
                Component.ComponentId))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Components[%d] has a duplicated "
                        "component identifier."),
                    ComponentIndex));

            return false;
        }

        UsedComponentIds.Add(
            Component.ComponentId);
    }

    return true;
}
