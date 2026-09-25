// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Visualization/TGSimulationPlaybackActor.h"

#include "Visualization/TGScreenSpaceLineBatchComponent.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/LineBatchComponent.h"
#include "Components/SceneComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/LightComponent.h"
#include "Components/MeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "ContentStreaming.h"
#include "Engine/DirectionalLight.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "RenderingThread.h"
#include "Simulation/ComponentTree/TGComponentKinematicsLibrary.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "Simulation/TGScenarioFileLibrary.h"
#include "Simulation/TGSimulationRunSubsystem.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "SpiceBridge.h"
#include "Styling/CoreStyle.h"
#include "UI/Theme/TGUiTheme.h"
#include "UI/Visualization/TGVisualizationHudWidget.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/ConstructorHelpers.h"
#include "Visualization/TGSpacecraftOrbitCameraActor.h"
#include "Visualization/TGSpacecraftVisualActor.h"
#include "Widgets/Images/SThrobber.h"
#include "Widgets/Layout/SBorder.h"

namespace TGSimulationPlaybackPrivate
{
    const FString ComponentPrefix = TEXT("component_");
    const FString BodyPrefix = TEXT("body_");
    const FName IgnoreCelestialRadiusScalingTag =
        TEXT("TGIgnoreForBodyRadiusScaling");

    void AddDefaultBodyBinding(
        TArray<FTGCelestialPlaybackBinding>& Bindings,
        const TCHAR* Key,
        const TCHAR* ClassPath)
    {
        FTGCelestialPlaybackBinding Binding;
        Binding.BodyColumnKey = Key;
        Binding.ActorClass = TSoftClassPtr<AActor>(
            FSoftObjectPath(ClassPath));
        Bindings.Add(MoveTemp(Binding));
    }

    FString ExtractKey(
        const FString& Column,
        const FString& Prefix,
        const FString& Suffix)
    {
        if (!Column.StartsWith(Prefix) || !Column.EndsWith(Suffix))
        {
            return FString{};
        }

        const int32 KeyLength =
            Column.Len() - Prefix.Len() - Suffix.Len();
        return KeyLength > 0
            ? Column.Mid(Prefix.Len(), KeyLength)
            : FString{};
    }

    double ApparentDiskOverlapArea(
        const double RadiusA,
        const double RadiusB,
        const double Separation)
    {
        if (Separation >= RadiusA + RadiusB)
        {
            return 0.0;
        }
        if (Separation <= FMath::Abs(RadiusA - RadiusB))
        {
            const double SmallerRadius = FMath::Min(RadiusA, RadiusB);
            return UE_DOUBLE_PI * SmallerRadius * SmallerRadius;
        }

        const double SeparationSquared = Separation * Separation;
        const double RadiusASquared = RadiusA * RadiusA;
        const double RadiusBSquared = RadiusB * RadiusB;
        const double AngleA = FMath::Acos(FMath::Clamp(
            (SeparationSquared + RadiusASquared - RadiusBSquared) /
                (2.0 * Separation * RadiusA),
            -1.0,
            1.0));
        const double AngleB = FMath::Acos(FMath::Clamp(
            (SeparationSquared + RadiusBSquared - RadiusASquared) /
                (2.0 * Separation * RadiusB),
            -1.0,
            1.0));
        const double Radical = FMath::Max(
            0.0,
            (-Separation + RadiusA + RadiusB) *
                (Separation + RadiusA - RadiusB) *
                (Separation - RadiusA + RadiusB) *
                (Separation + RadiusA + RadiusB));
        return RadiusASquared * AngleA + RadiusBSquared * AngleB -
            0.5 * FMath::Sqrt(Radical);
    }

    double ApparentDiameterPixels(
        const double RadiusMeters,
        const double DistanceMeters,
        const double ViewportHeightPixels,
        const double VerticalFovRadians,
        const double Magnification)
    {
        if (RadiusMeters <= 0.0 || DistanceMeters <= 0.0)
        {
            return DistanceMeters <= 0.0
                ? TNumericLimits<double>::Max()
                : 0.0;
        }

        const double AngularDiameter = 2.0 * FMath::Asin(FMath::Clamp(
            RadiusMeters * FMath::Max(0.0, Magnification) / DistanceMeters,
            0.0,
            1.0));
        return AngularDiameter * ViewportHeightPixels /
            FMath::Max(VerticalFovRadians, UE_DOUBLE_SMALL_NUMBER);
    }

    double ResolveVisualReferenceRadiusMeters(
        const FTGCelestialCatalogEntry& Entry,
        const double SpiceReferenceRadiusMeters)
    {
        if (SpiceReferenceRadiusMeters > 0.0)
        {
            return SpiceReferenceRadiusMeters;
        }

        // The loaded PCK has no Pallas RADII entry. Its Blueprint is authored
        // with triaxial proportions; this equatorial reference radius lets the
        // standard apparent-size and uniform-scale paths retain those ratios.
        return Entry.CatalogKey == TEXT("Pallas") ? 275000.0 : 0.0;
    }

    FBox CalculateCelestialRadiusReferenceBounds(AActor& BodyActor)
    {
        FBox RadiusBounds(ForceInit);
        TArray<UPrimitiveComponent*> Primitives;
        BodyActor.GetComponents<UPrimitiveComponent>(Primitives);

        const FTransform ActorTransform = BodyActor.GetActorTransform();
        for (UPrimitiveComponent* Primitive : Primitives)
        {
            if (!IsValid(Primitive) ||
                Primitive->ComponentHasTag(
                    IgnoreCelestialRadiusScalingTag))
            {
                continue;
            }

            // CalcBounds expects the component-to-output transform. Supplying
            // component-to-actor gives a box in actor-local coordinates, which
            // keeps authored triaxial mesh ratios but excludes current actor
            // scale and translation.
            const FTransform ComponentToActor =
                Primitive->GetComponentTransform().GetRelativeTransform(
                    ActorTransform);
            RadiusBounds += Primitive->CalcBounds(
                ComponentToActor).GetBox();
        }

        return RadiusBounds.IsValid
            ? RadiusBounds
            : BodyActor.CalculateComponentsBoundingBoxInLocalSpace(
                true,
                true);
    }

}

bool ATGSimulationPlaybackActor::FColumnVector::IsComplete() const
{
    return X != INDEX_NONE && Y != INDEX_NONE && Z != INDEX_NONE;
}

bool ATGSimulationPlaybackActor::FColumnRotation::IsComplete() const
{
    for (const int32 Value : Values)
    {
        if (Value == INDEX_NONE)
        {
            return false;
        }
    }
    return true;
}

bool ATGSimulationPlaybackActor::FColumnRotation::IsEmpty() const
{
    for (const int32 Value : Values)
    {
        if (Value != INDEX_NONE)
        {
            return false;
        }
    }
    return true;
}

ATGSimulationPlaybackActor::ATGSimulationPlaybackActor()
{
    PrimaryActorTick.bCanEverTick = true;

    PlaybackRoot = CreateDefaultSubobject<USceneComponent>(
        TEXT("PlaybackRoot"));
    SetRootComponent(PlaybackRoot);

    TrajectoryLineBatch =
        CreateDefaultSubobject<UTGScreenSpaceLineBatchComponent>(
        TEXT("LocalTrajectory"));
    TrajectoryLineBatch->SetupAttachment(PlaybackRoot);
    TrajectoryLineBatch->SetHiddenInGame(false);
    TrajectoryLineBatch->SetCastShadow(false);
    TrajectoryLineBatch->SetReceivesDecals(false);

    BodyReticleLineBatch =
        CreateDefaultSubobject<UTGScreenSpaceLineBatchComponent>(
            TEXT("BodyReticles"));
    BodyReticleLineBatch->SetupAttachment(PlaybackRoot);
    BodyReticleLineBatch->SetHiddenInGame(false);
    BodyReticleLineBatch->SetCastShadow(false);
    BodyReticleLineBatch->SetReceivesDecals(false);

    SpacecraftActorClass = ATGSpacecraftVisualActor::StaticClass();
    VisualizationHudClass =
        TSoftClassPtr<UTGVisualizationHudWidget>(FSoftObjectPath(
            TEXT("/Game/UI/Visualization/WBP_VisualizationHUD."
                 "WBP_VisualizationHUD_C")));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded())
    {
        GenericCelestialSphereMesh = SphereMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowCylinderMesh(
        TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (ArrowCylinderMesh.Succeeded())
    {
        VectorArrowCylinderMesh = ArrowCylinderMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UStaticMesh> ArrowConeMesh(
        TEXT("/Engine/BasicShapes/Cone.Cone"));
    if (ArrowConeMesh.Succeeded())
    {
        VectorArrowConeMesh = ArrowConeMesh.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface>
        ArrowMaterial(
            TEXT("/Engine/EngineDebugMaterials/"
                 "LevelColorationUnlitMaterial."
                 "LevelColorationUnlitMaterial"));
    if (ArrowMaterial.Succeeded())
    {
        VectorArrowMaterial = ArrowMaterial.Object;
    }

    static ConstructorHelpers::FObjectFinder<UMaterialInterface> SphereMaterial(
        TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (SphereMaterial.Succeeded())
    {
        GenericCelestialMaterial = SphereMaterial.Object;
    }

    using namespace TGSimulationPlaybackPrivate;
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("sun"),
        TEXT("/Game/Blueprints/BP_Sun.BP_Sun_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("mercury"),
        TEXT("/Game/Blueprints/BP_Mercury.BP_Mercury_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("venus"),
        TEXT("/Game/Blueprints/BP_Venus.BP_Venus_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("earth"),
        TEXT("/Game/Blueprints/BP_Earth.BP_Earth_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("moon"),
        TEXT("/Game/Blueprints/BP_Moon.BP_Moon_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("mars"),
        TEXT("/Game/Blueprints/BP_Mars.BP_Mars_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("phobos"),
        TEXT("/Game/Blueprints/BP_Phobos.BP_Phobos_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("deimos"),
        TEXT("/Game/Blueprints/BP_Deimos.BP_Deimos_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("jupiter"),
        TEXT("/Game/Blueprints/BP_Jupiter.BP_Jupiter_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("io"),
        TEXT("/Game/Blueprints/BP_Io.BP_Io_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("europa"),
        TEXT("/Game/Blueprints/BP_Europa.BP_Europa_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("ganymede"),
        TEXT("/Game/Blueprints/BP_Ganymede.BP_Ganymede_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("callisto"),
        TEXT("/Game/Blueprints/BP_Callisto.BP_Callisto_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("saturn"),
        TEXT("/Game/Blueprints/BP_Saturn.BP_Saturn_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("mimas"),
        TEXT("/Game/Blueprints/BP_Mimas.BP_Mimas_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("enceladus"),
        TEXT("/Game/Blueprints/BP_Enceladus.BP_Enceladus_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("tethys"),
        TEXT("/Game/Blueprints/BP_Tethys.BP_Tethys_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("dione"),
        TEXT("/Game/Blueprints/BP_Dione.BP_Dione_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("rhea"),
        TEXT("/Game/Blueprints/BP_Rhea.BP_Rhea_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("titan"),
        TEXT("/Game/Blueprints/BP_Titan.BP_Titan_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("iapetus"),
        TEXT("/Game/Blueprints/BP_Iapetus.BP_Iapetus_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("phoebe"),
        TEXT("/Game/Blueprints/BP_Phoebe.BP_Phoebe_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("uranus"),
        TEXT("/Game/Blueprints/BP_Uranus.BP_Uranus_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("miranda"),
        TEXT("/Game/Blueprints/BP_Miranda.BP_Miranda_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("ariel"),
        TEXT("/Game/Blueprints/BP_Ariel.BP_Ariel_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("umbriel"),
        TEXT("/Game/Blueprints/BP_Umbriel.BP_Umbriel_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("titania"),
        TEXT("/Game/Blueprints/BP_Titania.BP_Titania_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("oberon"),
        TEXT("/Game/Blueprints/BP_Oberon.BP_Oberon_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("neptune"),
        TEXT("/Game/Blueprints/BP_Neptune.BP_Neptune_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("triton"),
        TEXT("/Game/Blueprints/BP_Triton.BP_Triton_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("pluto"),
        TEXT("/Game/Blueprints/BP_Pluto.BP_Pluto_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("charon"),
        TEXT("/Game/Blueprints/BP_Charon.BP_Charon_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("ceres"),
        TEXT("/Game/Blueprints/BP_Ceres.BP_Ceres_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("pallas"),
        TEXT("/Game/Blueprints/BP_Pallas.BP_Pallas_C"));
    AddDefaultBodyBinding(
        CelestialBodyBindings, TEXT("vesta"),
        TEXT("/Game/Blueprints/BP_Vesta.BP_Vesta_C"));
}

void ATGSimulationPlaybackActor::BeginPlay()
{
    Super::BeginPlay();

    if (bInitializeFromLatestCompletedRunOnBeginPlay)
    {
        UGameInstance* GameInstance = GetGameInstance();
        const UTGSimulationRunSubsystem* RunSubsystem =
            GameInstance != nullptr
                ? GameInstance->GetSubsystem<UTGSimulationRunSubsystem>()
                : nullptr;
        if (RunSubsystem != nullptr && RunSubsystem->HasCompletedRun())
        {
            FText ErrorText;
            if (!InitializeFromLatestCompletedRun(ErrorText))
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("PHAROS completed-run playback initialization failed: %s"),
                    *ErrorText.ToString());
            }
            return;
        }
    }

    if (!bInitializeOnBeginPlay || StartupResultCsvFile.FilePath.IsEmpty())
    {
        return;
    }

    FText ErrorText;
    const bool bInitialized = StartupScenarioFile.FilePath.IsEmpty()
        ? InitializeFromCurrentScenario(
            StartupResultCsvFile.FilePath, ErrorText)
        : InitializeFromFiles(
            StartupScenarioFile.FilePath,
            StartupResultCsvFile.FilePath,
            ErrorText);

    if (!bInitialized)
    {
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PHAROS playback initialization failed: %s"),
            *ErrorText.ToString());
    }
}

void ATGSimulationPlaybackActor::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    HideVisualizationWarmupOverlay();
    DestroyPresentation();
    ClearVisualizationArrows();
    DestroySpawnedActors();
    Super::EndPlay(EndPlayReason);
}

void ATGSimulationPlaybackActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    UpdateVisualizationWarmupOverlay();

    if (APlayerController* PlayerController =
            GetWorld() != nullptr
                ? GetWorld()->GetFirstPlayerController()
                : nullptr)
    {
        if (PlayerController->WasInputKeyJustPressed(EKeys::Escape))
        {
            ToggleVisualizationMenu();
        }
    }

    // Visibility-fill direction follows the orbit camera even while playback
    // is paused and no simulation frame needs to be advanced.
    UpdateVisualizationOptionLights();

    if (!bPlaybackRunning || !bResultLoaded || DeltaSeconds <= 0.0f)
    {
        return;
    }

    const double Duration = GetDurationSeconds();
    if (Duration <= 0.0)
    {
        bPlaybackRunning = false;
        return;
    }

    double RequestedTime = CurrentElapsedSeconds +
        static_cast<double>(DeltaSeconds) *
        PlaybackRateSimulationSecondsPerRealSecond;

    bool bReachedEnd = false;
    if (RequestedTime > Duration)
    {
        if (bLoop)
        {
            RequestedTime = FMath::Fmod(RequestedTime, Duration);
        }
        else
        {
            RequestedTime = Duration;
            bPlaybackRunning = false;
            bReachedEnd = true;
        }
    }

    FText ErrorText;
    if (!SeekToElapsedSeconds(RequestedTime, ErrorText))
    {
        bPlaybackRunning = false;
        UE_LOG(
            LogTemp,
            Error,
            TEXT("PHAROS playback stopped: %s"),
            *ErrorText.ToString());
        return;
    }

    if (bReachedEnd)
    {
        OnPlaybackFinished.Broadcast();
    }
}

bool ATGSimulationPlaybackActor::InitializeFromCurrentScenario(
    const FString& ResultCsvFilePath,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationSubsystem* SimulationSubsystem =
        GameInstance != nullptr
            ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
            : nullptr;

    if (SimulationSubsystem == nullptr ||
        !SimulationSubsystem->HasCurrentScenarioDraft())
    {
        OutErrorText = FText::FromString(
            TEXT("No current HUD scenario is available for playback."));
        return false;
    }

    return InitializeFromScenario(
        SimulationSubsystem->GetCurrentScenarioDraft(),
        ResultCsvFilePath,
        OutErrorText);
}

bool ATGSimulationPlaybackActor::InitializeFromLatestCompletedRun(
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();
    UGameInstance* GameInstance = GetGameInstance();
    const UTGSimulationRunSubsystem* RunSubsystem =
        GameInstance != nullptr
            ? GameInstance->GetSubsystem<UTGSimulationRunSubsystem>()
            : nullptr;
    if (RunSubsystem == nullptr || !RunSubsystem->HasCompletedRun())
    {
        OutErrorText = FText::FromString(
            TEXT("No completed simulation run is available for playback."));
        return false;
    }

    const FTGCompletedSimulationRun Run =
        RunSubsystem->GetLatestCompletedRun();
    return InitializeFromFiles(
        Run.ScenarioFilePath,
        Run.ResultCsvFilePath,
        OutErrorText);
}

bool ATGSimulationPlaybackActor::InitializeFromFiles(
    const FString& ScenarioFilePath,
    const FString& ResultCsvFilePath,
    FText& OutErrorText)
{
    FTGSimulationScenario Scenario;
    if (!UTGScenarioFileLibrary::ImportScenarioFromTgscn(
            this,
            ResolveInputPath(ScenarioFilePath),
            Scenario,
            OutErrorText))
    {
        return false;
    }

    return InitializeFromScenario(
        Scenario,
        ResultCsvFilePath,
        OutErrorText);
}

bool ATGSimulationPlaybackActor::InitializeFromScenario(
    const FTGSimulationScenario& Scenario,
    const FString& ResultCsvFilePath,
    FText& OutErrorText)
{
    ClearPlayback();
    // Every newly opened result starts in real time. The timeline can change
    // this only after playback has been initialized.
    SetPlaybackRate(1.0);
    ShowVisualizationWarmupOverlay();
    // Playback owns the spacecraft-centered display frame. Its origin is not
    // a level-design placement choice: the propagated spacecraft CM is always
    // rendered at Unreal world (0, 0, 0).
    SetActorLocationAndRotation(
        FVector::ZeroVector,
        FRotator::ZeroRotator);
    SetActorScale3D(FVector::OneVector);
    ScenarioSnapshot = Scenario;

    if (!LoadResultCsv(ResultCsvFilePath, OutErrorText) ||
        !BuildVisualActors(OutErrorText))
    {
        HideVisualizationWarmupOverlay();
        ClearPlayback();
        return false;
    }

    CurrentElapsedSeconds = 0.0;
    bPlaybackRunning = bAutoPlay;
    if (!ApplyCurrentFrame(OutErrorText))
    {
        HideVisualizationWarmupOverlay();
        ClearPlayback();
        return false;
    }

    WarmInitialVisualizationResources();
    ScheduleVisualizationReveal();
    return true;
}

void ATGSimulationPlaybackActor::WarmInitialVisualizationResources()
{
    ReleaseVisualizationWarmupTextureResidency();

    TArray<UMeshComponent*> InitialMeshes;
    auto CollectActorMeshes = [&InitialMeshes](AActor* Actor)
    {
        if (!IsValid(Actor) || Actor->IsHidden())
        {
            return;
        }

        TArray<UMeshComponent*> ActorMeshes;
        Actor->GetComponents<UMeshComponent>(ActorMeshes);
        for (UMeshComponent* Mesh : ActorMeshes)
        {
            if (IsValid(Mesh))
            {
                InitialMeshes.AddUnique(Mesh);
            }
        }
    };

    CollectActorMeshes(SpacecraftActor);
    for (const TPair<FString, TObjectPtr<AActor>>& Pair :
         SpawnedCelestialActors)
    {
        CollectActorMeshes(Pair.Value);
    }

    // Dynamic playback actors are created after Unreal's normal map-load
    // texture-streaming prime. Temporarily force only the actors visible in
    // the initial frame so their real material textures are requested now.
    for (UMeshComponent* Mesh : InitialMeshes)
    {
        Mesh->SetTextureForceResidentFlag(true);
        VisualizationWarmupForcedMeshes.Add(Mesh);
    }

    FlushAsyncLoading();
    if (UWorld* World = GetWorld())
    {
        World->BlockTillLevelStreamingCompleted();
    }

    IStreamingManager::Get().NotifyLevelChange();
    const int32 RemainingRequests =
        IStreamingManager::Get().StreamAllResources(30.0f);
    FlushRenderingCommands();

    if (RemainingRequests > 0)
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Visualization resource warm-up ended with %d streaming requests still pending."),
            RemainingRequests);
    }
}

void ATGSimulationPlaybackActor::ShowVisualizationWarmupOverlay()
{
    HideVisualizationWarmupOverlay();

    UWorld* World = GetWorld();
    UGameViewportClient* Viewport = World != nullptr
        ? World->GetGameViewport()
        : nullptr;
    if (Viewport == nullptr)
    {
        return;
    }

    VisualizationWarmupOverlay =
        SNew(SBorder)
        .BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
        .BorderBackgroundColor(TGUiTheme::GetPalette().Backdrop)
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        [
            SNew(SThrobber)
            .NumPieces(5)
        ];
    VisualizationWarmupViewport = Viewport;
    Viewport->AddViewportWidgetContent(
        VisualizationWarmupOverlay.ToSharedRef(),
        MAX_int32);
}

void ATGSimulationPlaybackActor::ScheduleVisualizationReveal()
{
    if (!VisualizationWarmupOverlay.IsValid())
    {
        ReleaseVisualizationWarmupTextureResidency();
        return;
    }

    // Tick occurs before rendering. Waiting through three complete ticks keeps
    // the overlay over three genuinely rendered world frames, rather than
    // merely sleeping for an arbitrary map-load interval.
    VisualizationWarmupFramesRemaining = 3;
    VisualizationWarmupEarliestRemovalSeconds =
        FPlatformTime::Seconds() + 0.20;
}

void ATGSimulationPlaybackActor::UpdateVisualizationWarmupOverlay()
{
    if (!VisualizationWarmupOverlay.IsValid())
    {
        return;
    }
    if (VisualizationWarmupFramesRemaining > 0)
    {
        --VisualizationWarmupFramesRemaining;
        return;
    }
    if (FPlatformTime::Seconds() <
        VisualizationWarmupEarliestRemovalSeconds)
    {
        return;
    }

    IStreamingManager::Get().BlockTillAllRequestsFinished(
        1.0f,
        false);
    FlushRenderingCommands();
    HideVisualizationWarmupOverlay();
}

void ATGSimulationPlaybackActor::HideVisualizationWarmupOverlay()
{
    UGameViewportClient* Viewport = VisualizationWarmupViewport.Get();
    if (Viewport != nullptr && VisualizationWarmupOverlay.IsValid())
    {
        Viewport->RemoveViewportWidgetContent(
            VisualizationWarmupOverlay.ToSharedRef());
    }
    VisualizationWarmupOverlay.Reset();
    VisualizationWarmupViewport.Reset();
    VisualizationWarmupFramesRemaining = 0;
    VisualizationWarmupEarliestRemovalSeconds = 0.0;
    ReleaseVisualizationWarmupTextureResidency();
}

void ATGSimulationPlaybackActor::
    ReleaseVisualizationWarmupTextureResidency()
{
    for (const TWeakObjectPtr<UMeshComponent>& WeakMesh :
         VisualizationWarmupForcedMeshes)
    {
        if (UMeshComponent* Mesh = WeakMesh.Get())
        {
            Mesh->SetTextureForceResidentFlag(false);
        }
    }
    VisualizationWarmupForcedMeshes.Reset();
}

void ATGSimulationPlaybackActor::Play()
{
    if (bResultLoaded)
    {
        bPlaybackRunning = true;
    }
}

void ATGSimulationPlaybackActor::Pause()
{
    bPlaybackRunning = false;
}

void ATGSimulationPlaybackActor::TogglePlayPause()
{
    if (bPlaybackRunning)
    {
        Pause();
    }
    else
    {
        Play();
    }
}

void ATGSimulationPlaybackActor::SetPlaybackRate(
    double SimulationSecondsPerRealSecond)
{
    PlaybackRateSimulationSecondsPerRealSecond = FMath::Max(
        0.0,
        SimulationSecondsPerRealSecond);
}

bool ATGSimulationPlaybackActor::SeekToElapsedSeconds(
    double RequestedElapsedSeconds,
    FText& OutErrorText)
{
    if (!bResultLoaded)
    {
        OutErrorText = FText::FromString(
            TEXT("Load a simulation result before seeking."));
        return false;
    }

    CurrentElapsedSeconds = FMath::Clamp(
        RequestedElapsedSeconds,
        0.0,
        GetDurationSeconds());
    return ApplyCurrentFrame(OutErrorText);
}

bool ATGSimulationPlaybackActor::SeekToNormalizedTime(
    double RequestedNormalizedTime,
    FText& OutErrorText)
{
    return SeekToElapsedSeconds(
        FMath::Clamp(RequestedNormalizedTime, 0.0, 1.0) *
            GetDurationSeconds(),
        OutErrorText);
}

bool ATGSimulationPlaybackActor::StepBySimulationSeconds(
    double DeltaSimulationSeconds,
    FText& OutErrorText)
{
    return SeekToElapsedSeconds(
        CurrentElapsedSeconds + DeltaSimulationSeconds,
        OutErrorText);
}

bool ATGSimulationPlaybackActor::RefreshCurrentFrame(
    FText& OutErrorText)
{
    return ApplyCurrentFrame(OutErrorText);
}

void ATGSimulationPlaybackActor::ClearPlayback()
{
    HideVisualizationWarmupOverlay();
    bPlaybackRunning = false;
    bResultLoaded = false;
    CurrentElapsedSeconds = 0.0;

    DestroyPresentation();
    ClearVisualizationArrows();
    DestroySpawnedActors();
    if (TrajectoryLineBatch != nullptr)
    {
        TrajectoryLineBatch->Flush();
    }
    if (BodyReticleLineBatch != nullptr)
    {
        BodyReticleLineBatch->Flush();
    }
    ScenarioSnapshot = FTGSimulationScenario{};
    ColumnNames.Reset();
    ResultCsvColumnCount = 0;
    NumericRows.Reset();
    ColumnIndices.Reset();
    ComponentTracks.Reset();
    CelestialTracks.Reset();
    ForceTracks.Reset();
    PhysicalVectorTracks.Reset();
    VisualizationArrowInfos.Reset();
    SolarSystemOverviewPaths.Reset();
    LocalTrajectoryDisplayStartNormalized = 0.0;
    LocalTrajectoryDisplayEndNormalized = 1.0;
    SpacecraftPosition = FColumnVector{};
    SpacecraftVelocity = FColumnVector{};
    SpacecraftAngularVelocityBody = FColumnVector{};
    SpacecraftCenterOfMassBody = FColumnVector{};
    SpacecraftQuaternionW = INDEX_NONE;
    SpacecraftQuaternionX = INDEX_NONE;
    SpacecraftQuaternionY = INDEX_NONE;
    SpacecraftQuaternionZ = INDEX_NONE;
    VisibleSunFractionColumn = INDEX_NONE;
    MassColumn = INDEX_NONE;
    MassRateColumn = INDEX_NONE;
    TimeColumn = INDEX_NONE;
}

bool ATGSimulationPlaybackActor::IsResultLoaded() const
{
    return bResultLoaded;
}

bool ATGSimulationPlaybackActor::IsPlaying() const
{
    return bPlaybackRunning;
}

double ATGSimulationPlaybackActor::GetStartEphemerisTimeTdbSeconds() const
{
    return bResultLoaded ? NumericRows[0][TimeColumn] : 0.0;
}

double ATGSimulationPlaybackActor::GetEndEphemerisTimeTdbSeconds() const
{
    return bResultLoaded ? NumericRows.Last()[TimeColumn] : 0.0;
}

double ATGSimulationPlaybackActor::GetDurationSeconds() const
{
    return bResultLoaded
        ? GetEndEphemerisTimeTdbSeconds() -
            GetStartEphemerisTimeTdbSeconds()
        : 0.0;
}

double ATGSimulationPlaybackActor::GetElapsedSeconds() const
{
    return CurrentElapsedSeconds;
}

double ATGSimulationPlaybackActor::GetNormalizedTime() const
{
    const double Duration = GetDurationSeconds();
    return Duration > 0.0 ? CurrentElapsedSeconds / Duration : 0.0;
}

double ATGSimulationPlaybackActor::
    GetCurrentEphemerisTimeTdbSeconds() const
{
    return bResultLoaded
        ? GetStartEphemerisTimeTdbSeconds() + CurrentElapsedSeconds
        : 0.0;
}

void ATGSimulationPlaybackActor::GetResultNumericColumnNames(
    TArray<FString>& OutColumnNames) const
{
    OutColumnNames.Reset();
    if (!bResultLoaded)
    {
        return;
    }

    const int32 AuthoredColumnCount = FMath::Clamp(
        ResultCsvColumnCount,
        0,
        ColumnNames.Num());
    OutColumnNames.Append(ColumnNames.GetData(), AuthoredColumnCount);
}

bool ATGSimulationPlaybackActor::BuildResultNumericPlotSamples(
    const FString& XColumnName,
    const FString& YColumnName,
    const FString& ZColumnName,
    const double StartNormalized,
    const double EndNormalized,
    TArray<FVector>& OutSamples,
    FVector& OutMinimum,
    FVector& OutMaximum) const
{
    OutSamples.Reset();
    OutMinimum = FVector::ZeroVector;
    OutMaximum = FVector::ZeroVector;
    if (!bResultLoaded || NumericRows.IsEmpty())
    {
        return false;
    }

    const int32* XColumn = ColumnIndices.Find(XColumnName);
    const int32* YColumn = ColumnIndices.Find(YColumnName);
    const int32* ZColumn = ZColumnName.IsEmpty()
        ? nullptr
        : ColumnIndices.Find(ZColumnName);
    if (XColumn == nullptr || YColumn == nullptr ||
        (!ZColumnName.IsEmpty() && ZColumn == nullptr))
    {
        return false;
    }

    const double LowerNormalized = FMath::Clamp(
        FMath::Min(StartNormalized, EndNormalized),
        0.0,
        1.0);
    const double UpperNormalized = FMath::Clamp(
        FMath::Max(StartNormalized, EndNormalized),
        0.0,
        1.0);
    const double StartTime = FMath::Lerp(
        GetStartEphemerisTimeTdbSeconds(),
        GetEndEphemerisTimeTdbSeconds(),
        LowerNormalized);
    const double EndTime = FMath::Lerp(
        GetStartEphemerisTimeTdbSeconds(),
        GetEndEphemerisTimeTdbSeconds(),
        UpperNormalized);

    auto EvaluateAtTime = [this, XColumn, YColumn, ZColumn](
        const double Time,
        FVector& OutValue) -> bool
    {
        int32 LowerIndex = INDEX_NONE;
        int32 UpperIndex = INDEX_NONE;
        double Alpha = 0.0;
        if (!FindInterpolationRows(Time, LowerIndex, UpperIndex, Alpha))
        {
            return false;
        }
        OutValue = FVector(
            FMath::Lerp(
                NumericRows[LowerIndex][*XColumn],
                NumericRows[UpperIndex][*XColumn],
                Alpha),
            FMath::Lerp(
                NumericRows[LowerIndex][*YColumn],
                NumericRows[UpperIndex][*YColumn],
                Alpha),
            ZColumn != nullptr
                ? FMath::Lerp(
                    NumericRows[LowerIndex][*ZColumn],
                    NumericRows[UpperIndex][*ZColumn],
                    Alpha)
                : 0.0);
        return true;
    };

    FVector StartValue;
    if (!EvaluateAtTime(StartTime, StartValue))
    {
        return false;
    }
    OutSamples.Add(StartValue);
    OutMinimum = StartValue;
    OutMaximum = StartValue;
    if (FMath::IsNearlyEqual(StartTime, EndTime))
    {
        return true;
    }

    // Keep plotting bounded for very large result files while retaining both
    // exact interval endpoints and uniformly sampling the intervening rows.
    constexpr int32 MaximumPlotSamples = 4096;
    int32 InteriorRowCount = 0;
    for (const TArray<double>& Row : NumericRows)
    {
        const double Time = Row[TimeColumn];
        if (Time > StartTime && Time < EndTime)
        {
            ++InteriorRowCount;
            const FVector Value(
                Row[*XColumn],
                Row[*YColumn],
                ZColumn != nullptr ? Row[*ZColumn] : 0.0);
            OutMinimum = OutMinimum.ComponentMin(Value);
            OutMaximum = OutMaximum.ComponentMax(Value);
        }
    }
    const int32 InteriorBudget = MaximumPlotSamples - 2;
    const int32 RowStride = FMath::Max(
        1,
        FMath::CeilToInt(
            static_cast<double>(InteriorRowCount) /
            static_cast<double>(InteriorBudget)));
    int32 InteriorIndex = 0;
    for (const TArray<double>& Row : NumericRows)
    {
        const double Time = Row[TimeColumn];
        if (Time <= StartTime || Time >= EndTime)
        {
            continue;
        }
        if ((InteriorIndex++ % RowStride) != 0)
        {
            continue;
        }
        OutSamples.Add(FVector(
            Row[*XColumn],
            Row[*YColumn],
            ZColumn != nullptr ? Row[*ZColumn] : 0.0));
    }

    FVector EndValue;
    if (!EvaluateAtTime(EndTime, EndValue))
    {
        OutSamples.Reset();
        return false;
    }
    OutSamples.Add(EndValue);
    OutMinimum = OutMinimum.ComponentMin(EndValue);
    OutMaximum = OutMaximum.ComponentMax(EndValue);
    return true;
}

bool ATGSimulationPlaybackActor::EvaluateResultNumericPlotPoint(
    const FString& XColumnName,
    const FString& YColumnName,
    const FString& ZColumnName,
    FVector& OutPoint) const
{
    OutPoint = FVector::ZeroVector;
    if (!bResultLoaded || NumericRows.IsEmpty())
    {
        return false;
    }

    const int32* XColumn = ColumnIndices.Find(XColumnName);
    const int32* YColumn = ColumnIndices.Find(YColumnName);
    const int32* ZColumn = ZColumnName.IsEmpty()
        ? nullptr
        : ColumnIndices.Find(ZColumnName);
    if (XColumn == nullptr || YColumn == nullptr ||
        (!ZColumnName.IsEmpty() && ZColumn == nullptr))
    {
        return false;
    }

    int32 LowerIndex = INDEX_NONE;
    int32 UpperIndex = INDEX_NONE;
    double Alpha = 0.0;
    if (!FindInterpolationRows(
            GetCurrentEphemerisTimeTdbSeconds(),
            LowerIndex,
            UpperIndex,
            Alpha))
    {
        return false;
    }

    OutPoint = FVector(
        FMath::Lerp(
            NumericRows[LowerIndex][*XColumn],
            NumericRows[UpperIndex][*XColumn],
            Alpha),
        FMath::Lerp(
            NumericRows[LowerIndex][*YColumn],
            NumericRows[UpperIndex][*YColumn],
            Alpha),
        ZColumn != nullptr
            ? FMath::Lerp(
                NumericRows[LowerIndex][*ZColumn],
                NumericRows[UpperIndex][*ZColumn],
                Alpha)
            : 0.0);
    return true;
}

bool ATGSimulationPlaybackActor::GetCurrentUtc(
    FString& OutUtc,
    FText& OutErrorText) const
{
    OutUtc.Reset();
    OutErrorText = FText::GetEmpty();
    if (!bResultLoaded)
    {
        OutErrorText = FText::FromString(
            TEXT("Load a simulation result before requesting UTC."));
        return false;
    }

    FString Message;
    if (!FSpiceBridge::ConvertETToUTC(
            GetCurrentEphemerisTimeTdbSeconds(),
            OutUtc,
            Message))
    {
        OutErrorText = FText::FromString(Message);
        return false;
    }
    return true;
}

TArray<FString> ATGSimulationPlaybackActor::
    GetLoadedCelestialBodyKeys() const
{
    TArray<FString> Keys;
    Keys.Reserve(CelestialTracks.Num());
    for (const FCelestialTrack& Track : CelestialTracks)
    {
        Keys.Add(Track.Key);
    }
    return Keys;
}

bool ATGSimulationPlaybackActor::LoadResultCsv(
    const FString& ResultCsvFilePath,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();
    const FString ResolvedPath = ResolveInputPath(ResultCsvFilePath);

    FString Contents;
    if (!FFileHelper::LoadFileToString(Contents, *ResolvedPath))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT("Could not read simulation result CSV: %s"),
                *ResolvedPath));
        return false;
    }

    TArray<FString> Lines;
    Contents.ParseIntoArrayLines(Lines, false);
    while (!Lines.IsEmpty() && Lines.Last().TrimStartAndEnd().IsEmpty())
    {
        Lines.Pop();
    }

    if (Lines.Num() < 2 || !ParseCsvLine(Lines[0], ColumnNames))
    {
        OutErrorText = FText::FromString(
            TEXT("The result CSV must contain a header and at least one data row."));
        return false;
    }

    if (!ColumnNames.IsEmpty())
    {
        ColumnNames[0].RemoveFromStart(FString::Chr(0xFEFF));
    }

    for (int32 Column = 0; Column < ColumnNames.Num(); ++Column)
    {
        ColumnNames[Column].TrimStartAndEndInline();
        if (ColumnNames[Column].IsEmpty() ||
            ColumnIndices.Contains(ColumnNames[Column]))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("Result CSV column %d is empty or duplicated."),
                    Column + 1));
            return false;
        }
        ColumnIndices.Add(ColumnNames[Column], Column);
    }
    ResultCsvColumnCount = ColumnNames.Num();

    NumericRows.Reserve(Lines.Num() - 1);
    for (int32 LineIndex = 1; LineIndex < Lines.Num(); ++LineIndex)
    {
        if (Lines[LineIndex].TrimStartAndEnd().IsEmpty())
        {
            continue;
        }

        TArray<FString> Fields;
        if (!ParseCsvLine(Lines[LineIndex], Fields) ||
            Fields.Num() != ColumnNames.Num())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("Result CSV row %d has %d columns; expected %d."),
                    LineIndex + 1,
                    Fields.Num(),
                    ColumnNames.Num()));
            return false;
        }

        TArray<double> Row;
        Row.SetNumUninitialized(Fields.Num());
        for (int32 Column = 0; Column < Fields.Num(); ++Column)
        {
            Fields[Column].TrimStartAndEndInline();
            if (!LexTryParseString(Row[Column], *Fields[Column]) ||
                !FMath::IsFinite(Row[Column]))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("Result CSV row %d, column '%s' is not a finite number."),
                        LineIndex + 1,
                        *ColumnNames[Column]));
                return false;
            }
        }
        NumericRows.Add(MoveTemp(Row));
    }

    if (NumericRows.IsEmpty() || !BuildTrackSchema(OutErrorText))
    {
        if (OutErrorText.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT("The result CSV contains no numeric rows."));
        }
        return false;
    }

    const int32* ElapsedTimeColumn =
        ColumnIndices.Find(TEXT("elapsed_time_seconds"));
    for (int32 RowIndex = 1; RowIndex < NumericRows.Num(); ++RowIndex)
    {
        const double PreviousTime =
            NumericRows[RowIndex - 1][TimeColumn];
        const double CurrentTime = NumericRows[RowIndex][TimeColumn];
        if (CurrentTime > PreviousTime)
        {
            continue;
        }

        const bool bLegacyDuplicateTerminalEt =
            CurrentTime == PreviousTime &&
            RowIndex == NumericRows.Num() - 1 &&
            ElapsedTimeColumn != nullptr &&
            NumericRows[RowIndex][*ElapsedTimeColumn] >
                NumericRows[RowIndex - 1][*ElapsedTimeColumn];
        if (bLegacyDuplicateTerminalEt)
        {
            // Older runners could append a nanosecond-scale final tail whose
            // elapsed time was distinct but whose large absolute ET rounded
            // to the preceding value. Keep the true final state and discard
            // only the superseded fixed-grid row.
            NumericRows.RemoveAt(RowIndex - 1);
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("PHAROS playback coalesced a legacy duplicate terminal ET in '%s'."),
                *ResolvedPath);
            break;
        }

        if (CurrentTime <= PreviousTime)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("Result CSV ephemeris time must increase strictly; row %d does not."),
                    RowIndex + 2));
            return false;
        }
    }

    AppendVisibleCatalogCelestialTracks();

    bResultLoaded = true;
    return true;
}

void ATGSimulationPlaybackActor::AppendVisibleCatalogCelestialTracks()
{
    if (NumericRows.IsEmpty() || TimeColumn == INDEX_NONE ||
        !SpacecraftPosition.IsComplete())
    {
        return;
    }

    FString SpiceMessage;
    if (!FSpiceBridge::LoadKernels(SpiceMessage))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PHAROS playback could not load visual-only SPICE tracks: %s"),
            *SpiceMessage);
        return;
    }

    TSet<FString> ExistingBodyKeys;
    for (const FCelestialTrack& Track : CelestialTracks)
    {
        ExistingBodyKeys.Add(NormalizeColumnKey(Track.Key));
    }

    const double ReferenceViewHeight = FMath::Max(
        1.0,
        CelestialVisibilityReferenceViewportHeightPixels);
    const double ReferenceVerticalFov = FMath::DegreesToRadians(FMath::Clamp(
        CelestialVisibilityReferenceVerticalFovDegrees,
        1.0,
        170.0));

    // Probe a bounded set before generating full tracks. A deliberately lower
    // threshold keeps the precheck conservative while avoiding millions of
    // unnecessary SPICE calls for bodies that remain sub-pixel throughout.
    constexpr int32 MaximumVisibilityProbeCount = 2048;
    constexpr double VisibilityProbeThresholdScale = 0.25;
    const int32 ProbeStride = FMath::Max(
        1,
        FMath::CeilToInt(
            static_cast<double>(NumericRows.Num()) /
            static_cast<double>(MaximumVisibilityProbeCount)));

    for (const FTGCelestialCatalogEntry& Entry :
         UTGCelestialCatalogLibrary::GetCelestialCatalog())
    {
        if (Entry.SourceRole == ETGCelestialSourceRole::SystemBarycenter)
        {
            continue;
        }

        const FString BodyKey = NormalizeColumnKey(
            Entry.CatalogKey.ToString());
        const FString SpiceKey = NormalizeColumnKey(Entry.SpiceTarget);
        if (ExistingBodyKeys.Contains(BodyKey) ||
            ExistingBodyKeys.Contains(SpiceKey))
        {
            continue;
        }

        double IgnoredGravitationalParameter = 0.0;
        double SpiceReferenceRadius = 0.0;
        if (!FSpiceBridge::GetBodyGravityMetadataSI(
                Entry.SpiceTarget,
                IgnoredGravitationalParameter,
                SpiceReferenceRadius,
                SpiceMessage))
        {
            continue;
        }
        const double ReferenceRadius =
            TGSimulationPlaybackPrivate::ResolveVisualReferenceRadiusMeters(
                Entry,
                SpiceReferenceRadius);
        if (ReferenceRadius <= 0.0)
        {
            continue;
        }

        const FTGCelestialPlaybackBinding* Binding = FindBodyBinding(BodyKey);
        bool bMayBecomeVisible = false;
        auto ProbeRow = [&](const int32 RowIndex)
        {
            FVector BodyPosition;
            FVector IgnoredVelocity;
            if (!FSpiceBridge::GetBodyICRFStateSI(
                    Entry.SpiceTarget,
                    NumericRows[RowIndex][TimeColumn],
                    BodyPosition,
                    IgnoredVelocity,
                    SpiceMessage))
            {
                return;
            }

            const FVector SpacecraftPositionIcrf(
                NumericRows[RowIndex][SpacecraftPosition.X],
                NumericRows[RowIndex][SpacecraftPosition.Y],
                NumericRows[RowIndex][SpacecraftPosition.Z]);
            const double Distance =
                (BodyPosition - SpacecraftPositionIcrf).Length();
            if (Binding != nullptr &&
                Binding->MaximumVisibleDistanceMeters > 0.0 &&
                Distance > Binding->MaximumVisibleDistanceMeters)
            {
                return;
            }

            bMayBecomeVisible =
                TGSimulationPlaybackPrivate::ApparentDiameterPixels(
                    ReferenceRadius,
                    Distance,
                    ReferenceViewHeight,
                    ReferenceVerticalFov,
                    CelestialBodyVisualMagnification) >=
                MinimumCelestialApparentDiameterPixels *
                    VisibilityProbeThresholdScale;
        };

        for (int32 RowIndex = 0;
             RowIndex < NumericRows.Num() && !bMayBecomeVisible;
             RowIndex += ProbeStride)
        {
            ProbeRow(RowIndex);
        }
        if (!bMayBecomeVisible && NumericRows.Num() > 1 &&
            (NumericRows.Num() - 1) % ProbeStride != 0)
        {
            ProbeRow(NumericRows.Num() - 1);
        }
        if (!bMayBecomeVisible)
        {
            continue;
        }

        constexpr int32 ValuesPerBodyTrack = 16;
        TArray<TArray<double>> GeneratedRows;
        GeneratedRows.SetNum(NumericRows.Num());
        bool bCompleteTrack = true;
        for (int32 RowIndex = 0;
             RowIndex < NumericRows.Num();
             ++RowIndex)
        {
            FVector BodyPosition;
            FVector BodyVelocity;
            if (!FSpiceBridge::GetBodyICRFStateSI(
                    Entry.SpiceTarget,
                    NumericRows[RowIndex][TimeColumn],
                    BodyPosition,
                    BodyVelocity,
                    SpiceMessage))
            {
                bCompleteTrack = false;
                break;
            }

            tgsim::Mat3d BodyFixedToIcrf = tgsim::Mat3d::Identity();
            FSpiceBridge::GetBodyFixedToICRF(
                Entry.SpiceTarget,
                NumericRows[RowIndex][TimeColumn],
                BodyFixedToIcrf,
                SpiceMessage);

            TArray<double>& Values = GeneratedRows[RowIndex];
            Values.Reserve(ValuesPerBodyTrack);
            Values.Add(BodyPosition.X);
            Values.Add(BodyPosition.Y);
            Values.Add(BodyPosition.Z);
            Values.Add(BodyVelocity.X);
            Values.Add(BodyVelocity.Y);
            Values.Add(BodyVelocity.Z);
            Values.Add(ReferenceRadius);
            for (int32 MatrixRow = 0; MatrixRow < 3; ++MatrixRow)
            {
                for (int32 MatrixColumn = 0;
                     MatrixColumn < 3;
                     ++MatrixColumn)
                {
                    Values.Add(BodyFixedToIcrf.m[MatrixRow][MatrixColumn]);
                }
            }
        }
        if (!bCompleteTrack)
        {
            UE_LOG(
                LogTemp,
                Verbose,
                TEXT("PHAROS playback skipped visual-only body '%s': %s"),
                *Entry.DisplayName.ToString(),
                *SpiceMessage);
            continue;
        }

        const int32 FirstColumn = ColumnNames.Num();
        const FString Prefix =
            TGSimulationPlaybackPrivate::BodyPrefix + BodyKey + TEXT("_");
        TArray<FString> GeneratedColumnNames = {
            Prefix + TEXT("position_icrf_x_m"),
            Prefix + TEXT("position_icrf_y_m"),
            Prefix + TEXT("position_icrf_z_m"),
            Prefix + TEXT("velocity_icrf_x_mps"),
            Prefix + TEXT("velocity_icrf_y_mps"),
            Prefix + TEXT("velocity_icrf_z_mps"),
            Prefix + TEXT("reference_radius_m")};
        for (int32 MatrixIndex = 0; MatrixIndex < 9; ++MatrixIndex)
        {
            GeneratedColumnNames.Add(
                Prefix + TEXT("rotation_body_fixed_to_icrf_") +
                FString::Printf(
                    TEXT("%d%d"),
                    MatrixIndex / 3,
                    MatrixIndex % 3));
        }

        for (const FString& ColumnName : GeneratedColumnNames)
        {
            ColumnIndices.Add(ColumnName, ColumnNames.Num());
            ColumnNames.Add(ColumnName);
        }
        for (int32 RowIndex = 0;
             RowIndex < NumericRows.Num();
             ++RowIndex)
        {
            NumericRows[RowIndex].Append(GeneratedRows[RowIndex]);
        }

        FCelestialTrack Track;
        Track.Key = BodyKey;
        Track.Position.X = FirstColumn;
        Track.Position.Y = FirstColumn + 1;
        Track.Position.Z = FirstColumn + 2;
        Track.Velocity.X = FirstColumn + 3;
        Track.Velocity.Y = FirstColumn + 4;
        Track.Velocity.Z = FirstColumn + 5;
        Track.ReferenceRadius = FirstColumn + 6;
        for (int32 MatrixIndex = 0; MatrixIndex < 9; ++MatrixIndex)
        {
            Track.Rotation.Values[MatrixIndex] =
                FirstColumn + 7 + MatrixIndex;
        }
        CelestialTracks.Add(MoveTemp(Track));
        ExistingBodyKeys.Add(BodyKey);

        UE_LOG(
            LogTemp,
            Log,
            TEXT("PHAROS playback added visual-only SPICE track for '%s'."),
            *Entry.DisplayName.ToString());
    }
}

bool ATGSimulationPlaybackActor::BuildTrackSchema(
    FText& OutErrorText)
{
    auto RequireColumn = [this, &OutErrorText](
        const TCHAR* Name,
        int32& OutColumn) -> bool
    {
        const int32* Found = ColumnIndices.Find(Name);
        if (Found == nullptr)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("Result CSV is missing required column '%s'."),
                    Name));
            return false;
        }
        OutColumn = *Found;
        return true;
    };

    if (!RequireColumn(
            TEXT("ephemeris_time_tdb_seconds_past_j2000"),
            TimeColumn) ||
        !RequireColumn(TEXT("position_icrf_x_m"), SpacecraftPosition.X) ||
        !RequireColumn(TEXT("position_icrf_y_m"), SpacecraftPosition.Y) ||
        !RequireColumn(TEXT("position_icrf_z_m"), SpacecraftPosition.Z) ||
        !RequireColumn(TEXT("velocity_icrf_x_mps"), SpacecraftVelocity.X) ||
        !RequireColumn(TEXT("velocity_icrf_y_mps"), SpacecraftVelocity.Y) ||
        !RequireColumn(TEXT("velocity_icrf_z_mps"), SpacecraftVelocity.Z) ||
        !RequireColumn(
            TEXT("quaternion_body_to_icrf_w"),
            SpacecraftQuaternionW) ||
        !RequireColumn(
            TEXT("quaternion_body_to_icrf_x"),
            SpacecraftQuaternionX) ||
        !RequireColumn(
            TEXT("quaternion_body_to_icrf_y"),
            SpacecraftQuaternionY) ||
        !RequireColumn(
            TEXT("quaternion_body_to_icrf_z"),
            SpacecraftQuaternionZ) ||
        !RequireColumn(
            TEXT("angular_velocity_body_x_radps"),
            SpacecraftAngularVelocityBody.X) ||
        !RequireColumn(
            TEXT("angular_velocity_body_y_radps"),
            SpacecraftAngularVelocityBody.Y) ||
        !RequireColumn(
            TEXT("angular_velocity_body_z_radps"),
            SpacecraftAngularVelocityBody.Z) ||
        !RequireColumn(TEXT("mass_kg"), MassColumn) ||
        !RequireColumn(
            TEXT("center_of_mass_body_x_m"),
            SpacecraftCenterOfMassBody.X) ||
        !RequireColumn(
            TEXT("center_of_mass_body_y_m"),
            SpacecraftCenterOfMassBody.Y) ||
        !RequireColumn(
            TEXT("center_of_mass_body_z_m"),
            SpacecraftCenterOfMassBody.Z) ||
        !RequireColumn(TEXT("mass_rate_kgps"), MassRateColumn) ||
        !RequireColumn(
            TEXT("visible_sun_fraction"),
            VisibleSunFractionColumn))
    {
        return false;
    }

    using namespace TGSimulationPlaybackPrivate;
    TMap<FString, int32> ComponentTrackIndices;
    TMap<FString, int32> BodyTrackIndices;
    TSet<FString> ForceTrackKeys;
    TSet<FString> TorqueTrackKeys;

    for (const FString& Column : ColumnNames)
    {
        const FString ComponentKey = ExtractKey(
            Column,
            ComponentPrefix,
            TEXT("_origin_body_x_m"));
        if (!ComponentKey.IsEmpty() &&
            !ComponentTrackIndices.Contains(ComponentKey))
        {
            FComponentTrack Track;
            Track.Key = ComponentKey;
            ComponentTrackIndices.Add(
                ComponentKey,
                ComponentTracks.Add(MoveTemp(Track)));
        }

        const FString BodyKey = ExtractKey(
            Column,
            BodyPrefix,
            TEXT("_position_icrf_x_m"));
        if (!BodyKey.IsEmpty() && !BodyTrackIndices.Contains(BodyKey))
        {
            FCelestialTrack Track;
            Track.Key = BodyKey;
            BodyTrackIndices.Add(
                BodyKey,
                CelestialTracks.Add(MoveTemp(Track)));
        }

        const FString ForceKey = ExtractKey(
            Column,
            TEXT("force_"),
            TEXT("_icrf_x_n"));
        if (!ForceKey.IsEmpty())
        {
            ForceTrackKeys.Add(ForceKey);
        }

        const FString TorqueKey = ExtractKey(
            Column,
            TEXT("torque_"),
            TEXT("_body_x_nm"));
        if (!TorqueKey.IsEmpty())
        {
            TorqueTrackKeys.Add(TorqueKey);
        }
    }

    auto FindColumn = [this](const FString& Name) -> int32
    {
        const int32* Found = ColumnIndices.Find(Name);
        return Found != nullptr ? *Found : INDEX_NONE;
    };

    for (FComponentTrack& Track : ComponentTracks)
    {
        const FString Prefix = ComponentPrefix + Track.Key + TEXT("_");
        Track.Origin.X = FindColumn(Prefix + TEXT("origin_body_x_m"));
        Track.Origin.Y = FindColumn(Prefix + TEXT("origin_body_y_m"));
        Track.Origin.Z = FindColumn(Prefix + TEXT("origin_body_z_m"));
        for (int32 Index = 0; Index < 9; ++Index)
        {
            Track.Rotation.Values[Index] = FindColumn(
                Prefix + TEXT("rotation_component_to_body_") +
                FString::Printf(TEXT("%d%d"), Index / 3, Index % 3));
        }

        if (!Track.Origin.IsComplete() || !Track.Rotation.IsComplete())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("Component result track '%s' is incomplete."),
                    *Track.Key));
            return false;
        }
    }

    for (FCelestialTrack& Track : CelestialTracks)
    {
        const FString Prefix = BodyPrefix + Track.Key + TEXT("_");
        Track.Position.X = FindColumn(Prefix + TEXT("position_icrf_x_m"));
        Track.Position.Y = FindColumn(Prefix + TEXT("position_icrf_y_m"));
        Track.Position.Z = FindColumn(Prefix + TEXT("position_icrf_z_m"));
        Track.Velocity.X = FindColumn(Prefix + TEXT("velocity_icrf_x_mps"));
        Track.Velocity.Y = FindColumn(Prefix + TEXT("velocity_icrf_y_mps"));
        Track.Velocity.Z = FindColumn(Prefix + TEXT("velocity_icrf_z_mps"));
        Track.ReferenceRadius = FindColumn(Prefix + TEXT("reference_radius_m"));
        for (int32 Index = 0; Index < 9; ++Index)
        {
            Track.Rotation.Values[Index] = FindColumn(
                Prefix + TEXT("rotation_body_fixed_to_icrf_") +
                FString::Printf(TEXT("%d%d"), Index / 3, Index % 3));
        }

        if (!Track.Position.IsComplete() ||
            (!Track.Rotation.IsEmpty() && !Track.Rotation.IsComplete()))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("Celestial result track '%s' is incomplete."),
                    *Track.Key));
            return false;
        }
    }


    for (const FString& Key : ForceTrackKeys)
    {
        FForceTrack Track;
        Track.Key = Key;
        Track.DisplayName = Key.Replace(TEXT("_"), TEXT(" "));
        if (!Track.DisplayName.IsEmpty())
        {
            Track.DisplayName[0] = FChar::ToUpper(Track.DisplayName[0]);
        }
        const FString Prefix = TEXT("force_") + Key + TEXT("_icrf_");
        Track.ForceIcrf.X = FindColumn(Prefix + TEXT("x_n"));
        Track.ForceIcrf.Y = FindColumn(Prefix + TEXT("y_n"));
        Track.ForceIcrf.Z = FindColumn(Prefix + TEXT("z_n"));
        if (Track.ForceIcrf.IsComplete())
        {
            ForceTracks.Add(MoveTemp(Track));
        }
    }
    ForceTracks.Sort([](const FForceTrack& Left, const FForceTrack& Right)
    {
        return Left.Key < Right.Key;
    });

    TArray<FString> SortedTorqueKeys = TorqueTrackKeys.Array();
    SortedTorqueKeys.Sort();
    for (const FString& Key : SortedTorqueKeys)
    {
        FPhysicalVectorTrack Track;
        Track.Key = Key;
        Track.ArrowId = FName(*(TEXT("torque_") + Key));
        Track.DisplayName = Key == TEXT("srp")
            ? TEXT("Solar Radiation Pressure Torque (SRP)")
            : Key.Replace(TEXT("_"), TEXT(" ")) + TEXT(" Torque");
        if (!Track.DisplayName.IsEmpty() && Key != TEXT("srp"))
        {
            Track.DisplayName[0] = FChar::ToUpper(Track.DisplayName[0]);
        }
        Track.Unit = TEXT("N m");
        Track.Quantity = ETGVisualizationArrowQuantity::Torque;
        Track.bComponentsInBodyFrame = true;
        const FString Prefix = TEXT("torque_") + Key + TEXT("_body_");
        Track.Components.X = FindColumn(Prefix + TEXT("x_nm"));
        Track.Components.Y = FindColumn(Prefix + TEXT("y_nm"));
        Track.Components.Z = FindColumn(Prefix + TEXT("z_nm"));
        if (Track.Components.IsComplete())
        {
            PhysicalVectorTracks.Add(MoveTemp(Track));
        }
    }

    auto AddPhysicalVectorTrack = [this, &FindColumn](
        const TCHAR* Key,
        const TCHAR* ArrowId,
        const TCHAR* DisplayName,
        const TCHAR* Unit,
        const ETGVisualizationArrowQuantity Quantity,
        const TCHAR* XColumn,
        const TCHAR* YColumn,
        const TCHAR* ZColumn,
        const bool bComponentsInBodyFrame)
    {
        FPhysicalVectorTrack Track;
        Track.Key = Key;
        Track.ArrowId = FName(ArrowId);
        Track.DisplayName = DisplayName;
        Track.Unit = Unit;
        Track.Quantity = Quantity;
        Track.bComponentsInBodyFrame = bComponentsInBodyFrame;
        Track.Components.X = FindColumn(XColumn);
        Track.Components.Y = FindColumn(YColumn);
        Track.Components.Z = FindColumn(ZColumn);
        if (Track.Components.IsComplete())
        {
            PhysicalVectorTracks.Add(MoveTemp(Track));
        }
    };

    AddPhysicalVectorTrack(
        TEXT("velocity"),
        TEXT("velocity_icrf"),
        TEXT("Linear Velocity (ICRF)"),
        TEXT("m/s"),
        ETGVisualizationArrowQuantity::LinearVelocity,
        TEXT("velocity_icrf_x_mps"),
        TEXT("velocity_icrf_y_mps"),
        TEXT("velocity_icrf_z_mps"),
        false);
    AddPhysicalVectorTrack(
        TEXT("linear_momentum"),
        TEXT("linear_momentum_icrf"),
        TEXT("Linear Momentum (ICRF)"),
        TEXT("kg m/s"),
        ETGVisualizationArrowQuantity::LinearMomentum,
        TEXT("linear_momentum_icrf_x_kgmps"),
        TEXT("linear_momentum_icrf_y_kgmps"),
        TEXT("linear_momentum_icrf_z_kgmps"),
        false);
    AddPhysicalVectorTrack(
        TEXT("angular_momentum_about_cm"),
        TEXT("angular_momentum_about_cm_icrf"),
        TEXT("Angular Momentum about CM (ICRF)"),
        TEXT("kg m^2/s"),
        ETGVisualizationArrowQuantity::AngularMomentum,
        TEXT("angular_momentum_about_cm_icrf_x_kgm2ps"),
        TEXT("angular_momentum_about_cm_icrf_y_kgm2ps"),
        TEXT("angular_momentum_about_cm_icrf_z_kgm2ps"),
        false);
    AddPhysicalVectorTrack(
        TEXT("angular_velocity"),
        TEXT("angular_velocity_icrf"),
        TEXT("Angular Velocity (ICRF)"),
        TEXT("rad/s"),
        ETGVisualizationArrowQuantity::AngularVelocity,
        TEXT("angular_velocity_body_x_radps"),
        TEXT("angular_velocity_body_y_radps"),
        TEXT("angular_velocity_body_z_radps"),
        true);

    return true;
}

bool ATGSimulationPlaybackActor::BuildVisualActors(
    FText& OutErrorText)
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT("Playback has no valid Unreal world."));
        return false;
    }

    bHasTranslucentSpacecraftSurface = false;
    if (!ScenarioSnapshot.Components.IsEmpty())
    {
        bHasTranslucentSpacecraftSurface =
            ScenarioSnapshot.Components.ContainsByPredicate(
                [](const FTGComponentConfig& Component)
                {
                    const FTGComponentVisualConfig& Visual =
                        Component.Visual;
                    const float Opacity =
                        Visual.SurfaceAppearanceMode ==
                                ETGComponentSurfaceAppearanceMode::Textured
                            ? Visual.BaseColorTint.A
                            : Visual.DisplayColor.A;
                    return FMath::Clamp(Opacity, 0.0f, 1.0f) <
                        1.0f - KINDA_SMALL_NUMBER;
                });

        UClass* VisualClass = SpacecraftActorClass != nullptr
            ? SpacecraftActorClass.Get()
            : ATGSpacecraftVisualActor::StaticClass();
        SpacecraftActor = World->SpawnActor<ATGSpacecraftVisualActor>(
            VisualClass,
            GetActorTransform());

        if (SpacecraftActor == nullptr ||
            !SpacecraftActor->BuildFromScenario(
                ScenarioSnapshot,
                OutErrorText))
        {
            if (OutErrorText.IsEmpty())
            {
                OutErrorText = FText::FromString(
                    TEXT("Failed to create the spacecraft visual actor."));
            }
            return false;
        }

        /*
         * Isolate the spacecraft on lighting channel 1. The level's original
         * Sun light remains on channel 0 for planets and other scene visuals;
         * a runtime copy below lights only these generated spacecraft meshes.
         * Consequently, sky shells and real-scale celestial meshes cannot cast
         * renderer shadows onto the spacecraft.
         */
        TArray<UPrimitiveComponent*> SpacecraftPrimitives;
        SpacecraftActor->GetComponents<UPrimitiveComponent>(
            SpacecraftPrimitives);
        for (UPrimitiveComponent* Primitive : SpacecraftPrimitives)
        {
            if (IsValid(Primitive))
            {
                Primitive->SetLightingChannels(false, true, false);
                /*
                 * A centimeter-scale spacecraft and a planet thousands of
                 * kilometers away cannot share a useful shadow-map precision
                 * range. Letting this mesh cast into the world produces a
                 * magnified black disk on the planet. Surface normals still
                 * provide directional light/shade; eclipse attenuation is
                 * evaluated analytically from the recorded celestial geometry.
                 */
                Primitive->SetCastShadow(false);
            }
        }

        if (SunDirectionalLight != nullptr)
        {
            UDirectionalLightComponent* const CelestialSunComponent =
                Cast<UDirectionalLightComponent>(
                    SunDirectionalLight->GetLightComponent());
            if (CelestialSunComponent != nullptr)
            {
                CelestialSunComponent->SetAtmosphereSunLight(true);
                CelestialSunComponent->SetAtmosphereSunLightIndex(0);
                CelestialSunComponent->SetLightingChannels(
                    true,
                    false,
                    false);
                CelestialSunComponent->SetCastShadows(false);
                // The level Sun normally owns scene-wide forward rendering.
                // A translucent spacecraft temporarily gives its isolated Sun
                // higher priority below so lighting-channel filtering cannot
                // leave that surface black.
                CelestialSunComponent->SetForwardShadingPriority(1);
            }

            FActorSpawnParameters SpawnParameters;
            SpawnParameters.Owner = this;
            SpawnParameters.SpawnCollisionHandlingOverride =
                ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

            SpacecraftSunDirectionalLight =
                World->SpawnActor<ADirectionalLight>(
                    ADirectionalLight::StaticClass(),
                    SunDirectionalLight->GetActorTransform(),
                    SpawnParameters);

            if (SpacecraftSunDirectionalLight == nullptr)
            {
                OutErrorText = FText::FromString(
                    TEXT("Failed to create the spacecraft-only Sun light."));
                return false;
            }

            UDirectionalLightComponent* const SpacecraftSunComponent =
                Cast<UDirectionalLightComponent>(
                    SpacecraftSunDirectionalLight->GetLightComponent());
            if (SpacecraftSunComponent == nullptr)
            {
                OutErrorText = FText::FromString(
                    TEXT("The spacecraft-only Sun has no light component."));
                return false;
            }

            SpacecraftSunComponent->SetMobility(
                EComponentMobility::Movable);
            SpacecraftSunComponent->SetAtmosphereSunLight(false);
            SpacecraftSunComponent->SetLightingChannels(
                false,
                true,
                false);
            // Analytic visible-Sun fraction below replaces renderer shadows.
            SpacecraftSunComponent->SetCastShadows(false);
            SpacecraftSunComponent->SetForwardShadingPriority(
                bHasTranslucentSpacecraftSurface ? 2 : 0);
        }

        TSet<FString> ComponentKeys;
        for (const FTGComponentConfig& Component : ScenarioSnapshot.Components)
        {
            const FString Key = NormalizeColumnKey(Component.Name);
            if (ComponentKeys.Contains(Key))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("Components '%s' collide after result-column name normalization."),
                        *Component.Name));
                return false;
            }
            ComponentKeys.Add(Key);

            const bool bHasTrack = ComponentTracks.ContainsByPredicate(
                [&Key](const FComponentTrack& Track)
                {
                    return Track.Key == Key;
                });
            if (!bHasTrack)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("Result CSV has no component pose track for '%s' (key '%s')."),
                        *Component.Name,
                        *Key));
                return false;
            }
        }
    }

    SpawnCelestialActors();
    BuildOverviewPaths();
    BuildVisualizationArrows();
    BuildPresentation();
    CreateVisualizationOptionLights();
    UpdateVisualizationOptionLights();
    return true;
}

void ATGSimulationPlaybackActor::SpawnCelestialActors()
{
    for (const FCelestialTrack& Track : CelestialTracks)
    {
        const FTGCelestialPlaybackBinding* Binding =
            FindBodyBinding(Track.Key);
        if (!ShouldEverSpawnCelestialTrack(Track, Binding))
        {
            continue;
        }

        AActor* BodyActor = SpawnCelestialActor(Track, Binding);
        if (BodyActor == nullptr)
        {
            UE_LOG(
                LogTemp,
                Warning,
                TEXT("No visual actor could be created for result body '%s'."),
                *Track.Key);
            continue;
        }
        SpawnedCelestialActors.Add(Track.Key, BodyActor);
    }
}

void ATGSimulationPlaybackActor::DestroySpawnedActors()
{
    RestoreBodyEmissiveMaterials();

    if (BodiesEmissiveDirectionalLight != nullptr)
    {
        BodiesEmissiveDirectionalLight->Destroy();
        BodiesEmissiveDirectionalLight = nullptr;
    }
    if (SpacecraftEmissiveDirectionalLight != nullptr)
    {
        SpacecraftEmissiveDirectionalLight->Destroy();
        SpacecraftEmissiveDirectionalLight = nullptr;
    }
    if (SpacecraftSunDirectionalLight != nullptr)
    {
        SpacecraftSunDirectionalLight->Destroy();
        SpacecraftSunDirectionalLight = nullptr;
    }

    if (SpacecraftActor != nullptr)
    {
        SpacecraftActor->Destroy();
        SpacecraftActor = nullptr;
    }
    bHasTranslucentSpacecraftSurface = false;

    for (TPair<FString, TObjectPtr<AActor>>& Pair :
        SpawnedCelestialActors)
    {
        if (Pair.Value != nullptr)
        {
            Pair.Value->Destroy();
        }
    }
    SpawnedCelestialActors.Reset();
}

void ATGSimulationPlaybackActor::CreateVisualizationOptionLights()
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }

    auto SpawnVisibilityFill = [this, World](
        const FName ActorName,
        const bool bChannelOne,
        const bool bChannelTwo) -> ADirectionalLight*
    {
        FActorSpawnParameters Parameters;
        Parameters.Name = ActorName;
        Parameters.Owner = this;
        Parameters.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        ADirectionalLight* Light = World->SpawnActor<ADirectionalLight>(
            ADirectionalLight::StaticClass(),
            GetActorTransform(),
            Parameters);
        UDirectionalLightComponent* Component = Light != nullptr
            ? Cast<UDirectionalLightComponent>(Light->GetLightComponent())
            : nullptr;
        if (Component == nullptr)
        {
            if (Light != nullptr)
            {
                Light->Destroy();
            }
            return nullptr;
        }

        Component->SetMobility(EComponentMobility::Movable);
        Component->SetAtmosphereSunLight(false);
        Component->SetLightingChannels(
            !bChannelOne && !bChannelTwo,
            bChannelOne,
            bChannelTwo);
        Component->SetCastShadows(false);
        Component->SetForwardShadingPriority(0);
        Component->SetVolumetricScatteringIntensity(0.0f);
        Component->SetVisibility(false);
        return Light;
    };

    BodiesEmissiveDirectionalLight = SpawnVisibilityFill(
        TEXT("TG_BodiesEmissiveVisibilityFill"),
        false,
        true);
    SpacecraftEmissiveDirectionalLight = SpawnVisibilityFill(
        TEXT("TG_SpacecraftEmissiveVisibilityFill"),
        true,
        false);
}

void ATGSimulationPlaybackActor::UpdateVisualizationOptionLights()
{
    FVector CameraLocation = FVector::ZeroVector;
    bool bHasCameraLocation = false;
    if (IsValid(OrbitCameraActor))
    {
        CameraLocation = OrbitCameraActor->GetActorLocation();
        bHasCameraLocation = true;
    }
    else if (APlayerController* PlayerController =
                 GetWorld() != nullptr
                     ? GetWorld()->GetFirstPlayerController()
                     : nullptr)
    {
        FRotator IgnoredRotation;
        PlayerController->GetPlayerViewPoint(
            CameraLocation,
            IgnoredRotation);
        bHasCameraLocation = true;
    }

    const FVector CameraToSpacecraft =
        GetActorLocation() - CameraLocation;
    const FRotator SpacecraftFillRotation =
        bHasCameraLocation && !CameraToSpacecraft.IsNearlyZero()
            ? CameraToSpacecraft.Rotation()
            : FRotator::ZeroRotator;

    FVector BodyFillTarget = GetActorLocation();
    double LargestApparentRadius = -1.0;
    if (bHasCameraLocation)
    {
        for (const TPair<FString, TObjectPtr<AActor>>& Pair :
             SpawnedCelestialActors)
        {
            AActor* BodyActor = Pair.Value;
            if (!IsValid(BodyActor) || BodyActor->IsHidden())
            {
                continue;
            }

            FVector BoundsOrigin = BodyActor->GetActorLocation();
            FVector BoundsExtent = FVector::ZeroVector;
            BodyActor->GetActorBounds(
                false,
                BoundsOrigin,
                BoundsExtent);
            const double Distance = FVector::Distance(
                CameraLocation,
                BoundsOrigin);
            const double ApparentRadius = Distance > UE_DOUBLE_SMALL_NUMBER
                ? BoundsExtent.GetMax() / Distance
                : TNumericLimits<double>::Max();
            if (ApparentRadius > LargestApparentRadius)
            {
                LargestApparentRadius = ApparentRadius;
                BodyFillTarget = BoundsOrigin;
            }
        }
    }

    const FVector CameraToBody = BodyFillTarget - CameraLocation;
    const FRotator BodyFillRotation =
        bHasCameraLocation && !CameraToBody.IsNearlyZero()
            ? CameraToBody.Rotation()
            : SpacecraftFillRotation;

    float ReferenceIntensity = 1.0f;
    if (SunDirectionalLight != nullptr)
    {
        if (const UDirectionalLightComponent* SunComponent =
                Cast<UDirectionalLightComponent>(
                    SunDirectionalLight->GetLightComponent()))
        {
            ReferenceIntensity = FMath::Max(
                SunComponent->Intensity,
                0.01f);
        }
    }

    auto UpdateFill = [ReferenceIntensity](
        ADirectionalLight* Light,
        const FRotator& FillRotation,
        const bool bEnabled,
        const int32 ForwardShadingPriority)
    {
        if (Light == nullptr)
        {
            return;
        }
        Light->SetActorRotation(FillRotation);
        if (UDirectionalLightComponent* Component =
                Cast<UDirectionalLightComponent>(
                    Light->GetLightComponent()))
        {
            Component->SetIntensity(ReferenceIntensity);
            Component->SetForwardShadingPriority(
                bEnabled ? ForwardShadingPriority : 0);
            Component->SetVisibility(bEnabled);
        }
    };

    UpdateFill(
        BodiesEmissiveDirectionalLight,
        BodyFillRotation,
        bBodiesEmissive,
        0);
    UpdateFill(
        SpacecraftEmissiveDirectionalLight,
        SpacecraftFillRotation,
        bSpacecraftEmissive,
        bHasTranslucentSpacecraftSurface ? 3 : 0);

    UpdateBodyEmissiveMaterials();
}

void ATGSimulationPlaybackActor::UpdateBodyEmissiveMaterials()
{
    if (!bBodiesEmissive)
    {
        RestoreBodyEmissiveMaterials();
        return;
    }
    if (!BodyEmissiveMaterialOverrides.IsEmpty())
    {
        return;
    }

    static const FName NightSurfaceFillParameter(
        TEXT("TG Night Surface Fill"));
    for (const TPair<FString, TObjectPtr<AActor>>& Pair :
         SpawnedCelestialActors)
    {
        AActor* BodyActor = Pair.Value;
        if (!IsValid(BodyActor))
        {
            continue;
        }

        TArray<UMeshComponent*> MeshComponents;
        BodyActor->GetComponents<UMeshComponent>(MeshComponents);
        for (UMeshComponent* MeshComponent : MeshComponents)
        {
            if (!IsValid(MeshComponent))
            {
                continue;
            }

            const int32 MaterialCount = MeshComponent->GetNumMaterials();
            for (int32 MaterialIndex = 0;
                 MaterialIndex < MaterialCount;
                 ++MaterialIndex)
            {
                UMaterialInterface* OriginalMaterial =
                    MeshComponent->GetMaterial(MaterialIndex);
                if (OriginalMaterial == nullptr)
                {
                    continue;
                }

                UMaterialInstanceDynamic* DynamicMaterial =
                    UMaterialInstanceDynamic::Create(
                        OriginalMaterial,
                        MeshComponent);
                if (DynamicMaterial == nullptr)
                {
                    continue;
                }

                DynamicMaterial->SetScalarParameterValue(
                    NightSurfaceFillParameter,
                    1.0f);
                MeshComponent->SetMaterial(
                    MaterialIndex,
                    DynamicMaterial);

                FBodyEmissiveMaterialOverride Override;
                Override.MeshComponent = MeshComponent;
                Override.OriginalMaterial = OriginalMaterial;
                Override.MaterialIndex = MaterialIndex;
                BodyEmissiveMaterialOverrides.Add(MoveTemp(Override));
                BodyEmissiveDynamicMaterials.Add(DynamicMaterial);
            }
        }
    }
}

void ATGSimulationPlaybackActor::RestoreBodyEmissiveMaterials()
{
    for (const FBodyEmissiveMaterialOverride& Override :
         BodyEmissiveMaterialOverrides)
    {
        UMeshComponent* MeshComponent = Override.MeshComponent.Get();
        UMaterialInterface* OriginalMaterial =
            Override.OriginalMaterial.Get();
        if (IsValid(MeshComponent) &&
            OriginalMaterial != nullptr &&
            Override.MaterialIndex >= 0)
        {
            MeshComponent->SetMaterial(
                Override.MaterialIndex,
                OriginalMaterial);
        }
    }
    BodyEmissiveMaterialOverrides.Reset();
    BodyEmissiveDynamicMaterials.Reset();
}

void ATGSimulationPlaybackActor::SetBodiesEmissive(const bool bEnabled)
{
    bBodiesEmissive = bEnabled;
    UpdateVisualizationOptionLights();
}

bool ATGSimulationPlaybackActor::AreBodiesEmissive() const
{
    return bBodiesEmissive;
}

void ATGSimulationPlaybackActor::SetSpacecraftEmissive(
    const bool bEnabled)
{
    bSpacecraftEmissive = bEnabled;
    UpdateVisualizationOptionLights();
}

bool ATGSimulationPlaybackActor::IsSpacecraftEmissive() const
{
    return bSpacecraftEmissive;
}

void ATGSimulationPlaybackActor::SetVisualizationCameraFovDegrees(
    const double FieldOfViewDegrees)
{
    VisualizationCameraFovDegrees = FMath::Clamp(
        FieldOfViewDegrees,
        20.0,
        120.0);
    if (IsValid(OrbitCameraActor))
    {
        OrbitCameraActor->SetFieldOfViewDegrees(
            VisualizationCameraFovDegrees);
    }
}

double ATGSimulationPlaybackActor::
    GetVisualizationCameraFovDegrees() const
{
    return IsValid(OrbitCameraActor)
        ? OrbitCameraActor->GetFieldOfViewDegrees()
        : VisualizationCameraFovDegrees;
}

void ATGSimulationPlaybackActor::SetVisualizationOrbitSensitivity(
    const double SensitivityDegreesPerPixel)
{
    VisualizationOrbitSensitivityDegreesPerPixel = FMath::Clamp(
        SensitivityDegreesPerPixel,
        0.05,
        2.0);
    if (IsValid(OrbitCameraActor))
    {
        OrbitCameraActor->SetOrbitSensitivityDegreesPerPixel(
            VisualizationOrbitSensitivityDegreesPerPixel);
    }
}

double ATGSimulationPlaybackActor::
    GetVisualizationOrbitSensitivity() const
{
    return IsValid(OrbitCameraActor)
        ? OrbitCameraActor->GetOrbitSensitivityDegreesPerPixel()
        : VisualizationOrbitSensitivityDegreesPerPixel;
}

void ATGSimulationPlaybackActor::SetLocalTrajectoryDisplayRangeNormalized(
    const double StartNormalized,
    const double EndNormalized)
{
    const double NewStart = FMath::Clamp(
        FMath::Min(StartNormalized, EndNormalized),
        0.0,
        1.0);
    const double NewEnd = FMath::Clamp(
        FMath::Max(StartNormalized, EndNormalized),
        0.0,
        1.0);
    if (
        FMath::IsNearlyEqual(
            LocalTrajectoryDisplayStartNormalized,
            NewStart) &&
        FMath::IsNearlyEqual(
            LocalTrajectoryDisplayEndNormalized,
            NewEnd))
    {
        return;
    }

    LocalTrajectoryDisplayStartNormalized = NewStart;
    LocalTrajectoryDisplayEndNormalized = NewEnd;

    // Refresh only this overlay while a handle is dragged. Reapplying the
    // complete playback frame would needlessly update every body and widget.
    if (bResultLoaded)
    {
        UpdateLocalTrajectory();
    }
}

double ATGSimulationPlaybackActor::
    GetLocalTrajectoryDisplayStartNormalized() const
{
    return LocalTrajectoryDisplayStartNormalized;
}

double ATGSimulationPlaybackActor::
    GetLocalTrajectoryDisplayEndNormalized() const
{
    return LocalTrajectoryDisplayEndNormalized;
}

void ATGSimulationPlaybackActor::SetLocalTrajectoryColorMode(
    const ETGTrajectoryColorMode Mode)
{
    LocalTrajectoryColorMode = Mode;
    LocalTrajectoryColor = Mode == ETGTrajectoryColorMode::Red
        ? FLinearColor::Red
        : FLinearColor::White;

    if (bResultLoaded)
    {
        UpdateLocalTrajectory();
    }
}

ETGTrajectoryColorMode
ATGSimulationPlaybackActor::GetLocalTrajectoryColorMode() const
{
    return LocalTrajectoryColorMode;
}

void ATGSimulationPlaybackActor::SetLocalTrajectoryFrameMode(
    const ETGTrajectoryFrameMode Mode)
{
    LocalTrajectoryFrameMode = Mode;
    bShowLocalTrajectory = Mode != ETGTrajectoryFrameMode::Hidden;

    if (bResultLoaded)
    {
        UpdateLocalTrajectory();
    }
}

ETGTrajectoryFrameMode
ATGSimulationPlaybackActor::GetLocalTrajectoryFrameMode() const
{
    return bShowLocalTrajectory
        ? LocalTrajectoryFrameMode
        : ETGTrajectoryFrameMode::Hidden;
}

bool ATGSimulationPlaybackActor::ApplyCurrentFrame(
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();
    if (!bResultLoaded)
    {
        OutErrorText = FText::FromString(
            TEXT("No simulation result is loaded."));
        return false;
    }

    const double EphemerisTime =
        GetStartEphemerisTimeTdbSeconds() + CurrentElapsedSeconds;
    int32 LowerIndex = INDEX_NONE;
    int32 UpperIndex = INDEX_NONE;
    double Alpha = 0.0;
    if (!FindInterpolationRows(
            EphemerisTime,
            LowerIndex,
            UpperIndex,
            Alpha))
    {
        OutErrorText = FText::FromString(
            TEXT("Could not bracket the requested playback time."));
        return false;
    }

    const FVector SpacecraftPositionIcrf = InterpolateVector(
        SpacecraftPosition,
        LowerIndex,
        UpperIndex,
        Alpha);

    // Visualization uses the spacecraft-centered translating frame:
    // r_visual = C * (r_ICRF - r_spacecraft_ICRF) * scale.
    // C = diag(1, -1, 1) converts the backend frame handedness to Unreal.
    const FVector ReferencePositionIcrf = SpacecraftPositionIcrf;

    // The CSV fraction remains a robust fallback, but directly interpolating
    // 180-second samples would turn a few-second penumbra into a long fade.
    double VisibleSunFraction = FMath::Clamp(
        FMath::Lerp(
            NumericRows[LowerIndex][VisibleSunFractionColumn],
            NumericRows[UpperIndex][VisibleSunFractionColumn],
            Alpha),
        0.0,
        1.0);
    const FCelestialTrack* SunTrack = CelestialTracks.FindByPredicate(
        [](const FCelestialTrack& Track)
        {
            return Track.Key == TEXT("sun");
        });
    if (SunTrack != nullptr)
    {
        double GeometryVisibleSunFraction = 1.0;
        if (TryComputeVisualSunFraction(
                *SunTrack,
                SpacecraftPositionIcrf,
                LowerIndex,
                UpperIndex,
                Alpha,
                GeometryVisibleSunFraction))
        {
            VisibleSunFraction = GeometryVisibleSunFraction;
        }
    }

    auto ToWorldLocation = [this, &ReferencePositionIcrf](
        const FVector& PositionIcrf) -> FVector
    {
        const FVector Relative = PositionIcrf - ReferencePositionIcrf;
        return FVector(
            Relative.X,
            -Relative.Y,
            Relative.Z) * WorldUnitsPerMeter;
    };

    if (SpacecraftActor != nullptr)
    {
        const FQuat BackendAttitude = InterpolateQuaternion(
            SpacecraftQuaternionW,
            SpacecraftQuaternionX,
            SpacecraftQuaternionY,
            SpacecraftQuaternionZ,
            LowerIndex,
            UpperIndex,
            Alpha);
        const FQuat UnrealAttitude =
            UTGComponentKinematicsLibrary::
                ConvertBackendOrientationToUnreal(BackendAttitude);

        // Component poses use the native 100 UE units/m conversion. This actor
        // scale first restores the orbital scale, then applies visual magnification.
        const double ActorScale =
            (WorldUnitsPerMeter / 100.0) *
            SpacecraftVisualMagnification;
        SpacecraftActor->SetActorScale3D(
            FVector(FMath::Max(0.0, ActorScale)));

        // The propagated translational state is the instantaneous CM, while
        // the generated visual actor is rooted at body-frame origin O_B.
        // Offset O_B so that the current CM, not merely the actor root, is the
        // spacecraft-centered world's exact origin.
        const FVector CenterOfMassBodyMeters = InterpolateVector(
            SpacecraftCenterOfMassBody,
            LowerIndex,
            UpperIndex,
            Alpha);
        const FVector CenterOfMassOffsetWorld =
            UnrealAttitude.RotateVector(
                UTGComponentKinematicsLibrary::
                    ConvertBackendPositionMetersToUnrealCentimeters(
                        CenterOfMassBodyMeters) * ActorScale);
        SpacecraftActor->SetActorLocationAndRotation(
            ToWorldLocation(SpacecraftPositionIcrf) -
                CenterOfMassOffsetWorld,
            UnrealAttitude);

        TArray<FTGComponentKinematicPose> ComponentPoses;
        ComponentPoses.Reserve(ScenarioSnapshot.Components.Num());
        for (const FTGComponentConfig& Component : ScenarioSnapshot.Components)
        {
            const FString Key = NormalizeColumnKey(Component.Name);
            const FComponentTrack* Track = ComponentTracks.FindByPredicate(
                [&Key](const FComponentTrack& Candidate)
                {
                    return Candidate.Key == Key;
                });
            if (Track == nullptr)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("Component track '%s' disappeared during playback."),
                        *Key));
                return false;
            }

            FTGComponentKinematicPose Pose;
            Pose.ComponentId = Component.ComponentId;
            Pose.ComponentName = Component.Name;
            Pose.OriginInBodyMeters = InterpolateVector(
                Track->Origin,
                LowerIndex,
                UpperIndex,
                Alpha);
            Pose.ComponentToBodyOrientation = InterpolateRotationMatrix(
                Track->Rotation,
                LowerIndex,
                UpperIndex,
                Alpha);
            ComponentPoses.Add(MoveTemp(Pose));
        }

        if (!SpacecraftActor->ApplyComponentPoses(
                ComponentPoses,
                OutErrorText))
        {
            return false;
        }
    }

    for (const FCelestialTrack& Track : CelestialTracks)
    {
        TObjectPtr<AActor>* BodyActorPointer =
            SpawnedCelestialActors.Find(Track.Key);
        if (BodyActorPointer == nullptr || *BodyActorPointer == nullptr)
        {
            continue;
        }

        AActor* BodyActor = *BodyActorPointer;
        const FVector Position = InterpolateVector(
            Track.Position,
            LowerIndex,
            UpperIndex,
            Alpha);
        const FVector BodyWorldLocation = ToWorldLocation(Position);
        if (Track.Rotation.IsComplete())
        {
            const FQuat BackendOrientation = InterpolateRotationMatrix(
                Track.Rotation,
                LowerIndex,
                UpperIndex,
                Alpha);
            BodyActor->SetActorRotation(
                UTGComponentKinematicsLibrary::
                    ConvertBackendOrientationToUnreal(
                        BackendOrientation));
        }

        ApplyCelestialBodyRadius(
            *BodyActor,
            Track,
            LowerIndex,
            UpperIndex,
            Alpha,
            BodyWorldLocation);

        const FTGCelestialPlaybackBinding* Binding =
            FindBodyBinding(Track.Key);
        BodyActor->SetActorHiddenInGame(!IsCelestialTrackVisibleAtFrame(
            Track,
            Binding,
            SpacecraftPositionIcrf,
            LowerIndex,
            UpperIndex,
            Alpha));
    }

    if (
        SunTrack != nullptr &&
        bOrientSunDirectionalLight &&
        SunDirectionalLight != nullptr)
    {
        const FVector SpacecraftToSunIcrf = InterpolateVector(
            SunTrack->Position,
            LowerIndex,
            UpperIndex,
            Alpha) - SpacecraftPositionIcrf;
        const FVector SunToSpacecraftWorld =
            -ConvertIcrfVectorToUnreal(SpacecraftToSunIcrf);
        if (!SunToSpacecraftWorld.IsNearlyZero())
        {
            ULightComponent* const LightComponent =
                SunDirectionalLight->GetLightComponent();
            if (
                LightComponent != nullptr &&
                LightComponent->GetMobility() != EComponentMobility::Movable)
            {
                LightComponent->SetMobility(EComponentMobility::Movable);
            }

            const FRotator SunlightRotation =
                SunToSpacecraftWorld.Rotation();
            SunDirectionalLight->SetActorRotation(SunlightRotation);

            if (SpacecraftSunDirectionalLight != nullptr)
            {
                SpacecraftSunDirectionalLight->SetActorRotation(
                    SunlightRotation);
                UDirectionalLightComponent* const CelestialSunComponent =
                    Cast<UDirectionalLightComponent>(
                        SunDirectionalLight->GetLightComponent());
                UDirectionalLightComponent* const SpacecraftSunComponent =
                    Cast<UDirectionalLightComponent>(
                        SpacecraftSunDirectionalLight->GetLightComponent());
                if (
                    CelestialSunComponent != nullptr &&
                    SpacecraftSunComponent != nullptr)
                {
                    // Visual illumination follows Sun direction and eclipse
                    // fraction only; it deliberately has no 1/r^2 falloff.
                    SpacecraftSunComponent->SetIntensity(
                        CelestialSunComponent->Intensity *
                        static_cast<float>(VisibleSunFraction));
                    SpacecraftSunComponent->SetLightColor(
                        CelestialSunComponent->LightColor);
                    SpacecraftSunComponent->SetUseTemperature(
                        CelestialSunComponent->bUseTemperature);
                    SpacecraftSunComponent->SetTemperature(
                        CelestialSunComponent->Temperature);
                    SpacecraftSunComponent->SetLightSourceAngle(
                        CelestialSunComponent->LightSourceAngle);
                }
            }
        }
    }

    UpdateVisualizationOverlays(
        LowerIndex,
        UpperIndex,
        Alpha,
        SpacecraftPositionIcrf);

    OnPlaybackTimeChanged.Broadcast(
        CurrentElapsedSeconds,
        EphemerisTime,
        GetNormalizedTime());
    return true;
}

bool ATGSimulationPlaybackActor::FindInterpolationRows(
    double EphemerisTime,
    int32& OutLowerIndex,
    int32& OutUpperIndex,
    double& OutAlpha) const
{
    if (NumericRows.IsEmpty() || TimeColumn == INDEX_NONE)
    {
        return false;
    }

    if (EphemerisTime <= NumericRows[0][TimeColumn])
    {
        OutLowerIndex = 0;
        OutUpperIndex = 0;
        OutAlpha = 0.0;
        return true;
    }

    if (EphemerisTime >= NumericRows.Last()[TimeColumn])
    {
        OutLowerIndex = NumericRows.Num() - 1;
        OutUpperIndex = OutLowerIndex;
        OutAlpha = 0.0;
        return true;
    }

    int32 Low = 0;
    int32 High = NumericRows.Num() - 1;
    while (Low + 1 < High)
    {
        const int32 Middle = Low + (High - Low) / 2;
        if (NumericRows[Middle][TimeColumn] <= EphemerisTime)
        {
            Low = Middle;
        }
        else
        {
            High = Middle;
        }
    }

    OutLowerIndex = Low;
    OutUpperIndex = High;
    const double LowerTime = NumericRows[Low][TimeColumn];
    const double UpperTime = NumericRows[High][TimeColumn];
    OutAlpha = (EphemerisTime - LowerTime) /
        (UpperTime - LowerTime);
    return true;
}

FVector ATGSimulationPlaybackActor::InterpolateVector(
    const FColumnVector& Columns,
    int32 LowerIndex,
    int32 UpperIndex,
    double Alpha) const
{
    const FVector Lower(
        NumericRows[LowerIndex][Columns.X],
        NumericRows[LowerIndex][Columns.Y],
        NumericRows[LowerIndex][Columns.Z]);
    const FVector Upper(
        NumericRows[UpperIndex][Columns.X],
        NumericRows[UpperIndex][Columns.Y],
        NumericRows[UpperIndex][Columns.Z]);
    return FMath::Lerp(Lower, Upper, Alpha);
}

FQuat ATGSimulationPlaybackActor::InterpolateQuaternion(
    int32 WColumn,
    int32 XColumn,
    int32 YColumn,
    int32 ZColumn,
    int32 LowerIndex,
    int32 UpperIndex,
    double Alpha) const
{
    FQuat Lower(
        NumericRows[LowerIndex][XColumn],
        NumericRows[LowerIndex][YColumn],
        NumericRows[LowerIndex][ZColumn],
        NumericRows[LowerIndex][WColumn]);
    FQuat Upper(
        NumericRows[UpperIndex][XColumn],
        NumericRows[UpperIndex][YColumn],
        NumericRows[UpperIndex][ZColumn],
        NumericRows[UpperIndex][WColumn]);
    Lower.Normalize();
    Upper.Normalize();
    if ((Lower | Upper) < 0.0)
    {
        Upper = Upper * -1.0;
    }
    return FQuat::Slerp(Lower, Upper, Alpha).GetNormalized();
}

FQuat ATGSimulationPlaybackActor::InterpolateRotationMatrix(
    const FColumnRotation& Columns,
    int32 LowerIndex,
    int32 UpperIndex,
    double Alpha) const
{
    FQuat Lower = QuaternionFromMatrixValues(
        NumericRows[LowerIndex],
        Columns);
    FQuat Upper = QuaternionFromMatrixValues(
        NumericRows[UpperIndex],
        Columns);
    if ((Lower | Upper) < 0.0)
    {
        Upper = Upper * -1.0;
    }
    return FQuat::Slerp(Lower, Upper, Alpha).GetNormalized();
}

bool ATGSimulationPlaybackActor::TryComputeVisualSunFraction(
    const FCelestialTrack& SunTrack,
    const FVector& SpacecraftPositionIcrf,
    const int32 LowerIndex,
    const int32 UpperIndex,
    const double Alpha,
    double& OutVisibleSunFraction) const
{
    OutVisibleSunFraction = 1.0;
    if (!ScenarioSnapshot.SolarRadiationPressure.bComputeEclipse)
    {
        return true;
    }
    if (
        SunTrack.ReferenceRadius == INDEX_NONE ||
        !NumericRows.IsValidIndex(LowerIndex) ||
        !NumericRows.IsValidIndex(UpperIndex))
    {
        return false;
    }

    const FVector SpacecraftToSun = InterpolateVector(
        SunTrack.Position,
        LowerIndex,
        UpperIndex,
        Alpha) - SpacecraftPositionIcrf;
    const double SunDistance = SpacecraftToSun.Length();
    const double SunRadius = FMath::Lerp(
        NumericRows[LowerIndex][SunTrack.ReferenceRadius],
        NumericRows[UpperIndex][SunTrack.ReferenceRadius],
        Alpha);
    if (SunRadius <= 0.0 || SunDistance <= SunRadius)
    {
        return false;
    }

    const FVector SunDirection = SpacecraftToSun / SunDistance;
    const double SunAngularRadius = FMath::Asin(FMath::Clamp(
        SunRadius / SunDistance,
        0.0,
        1.0));
    const double ApparentSunArea =
        UE_DOUBLE_PI * SunAngularRadius * SunAngularRadius;
    if (ApparentSunArea <= UE_DOUBLE_SMALL_NUMBER)
    {
        return false;
    }

    const TArray<FString>& ConfiguredOcculters =
        ScenarioSnapshot.SolarRadiationPressure.OccultingBodyNames;
    double MaximumOccultedFraction = 0.0;
    for (const FCelestialTrack& BodyTrack : CelestialTracks)
    {
        if (
            BodyTrack.Key == SunTrack.Key ||
            BodyTrack.ReferenceRadius == INDEX_NONE)
        {
            continue;
        }

        if (!ConfiguredOcculters.IsEmpty())
        {
            const bool bConfigured = ConfiguredOcculters.ContainsByPredicate(
                [&BodyTrack](const FString& BodyName)
                {
                    return NormalizeColumnKey(BodyName) == BodyTrack.Key;
                });
            if (!bConfigured)
            {
                continue;
            }
        }

        const double BodyRadius = FMath::Lerp(
            NumericRows[LowerIndex][BodyTrack.ReferenceRadius],
            NumericRows[UpperIndex][BodyTrack.ReferenceRadius],
            Alpha);
        if (BodyRadius <= 0.0)
        {
            continue;
        }

        const FVector SpacecraftToBody = InterpolateVector(
            BodyTrack.Position,
            LowerIndex,
            UpperIndex,
            Alpha) - SpacecraftPositionIcrf;
        const double BodyDistance = SpacecraftToBody.Length();
        // Match TGSimCore: an occulting disk must be outside the spacecraft
        // and geometrically between it and the Sun.
        if (BodyDistance <= BodyRadius || BodyDistance >= SunDistance)
        {
            continue;
        }

        const double BodyAngularRadius = FMath::Asin(FMath::Clamp(
            BodyRadius / BodyDistance,
            0.0,
            1.0));
        const double CenterSeparation = FMath::Acos(FMath::Clamp(
            FVector::DotProduct(
                SunDirection,
                SpacecraftToBody / BodyDistance),
            -1.0,
            1.0));
        const double OccultedArea =
            TGSimulationPlaybackPrivate::ApparentDiskOverlapArea(
                SunAngularRadius,
                BodyAngularRadius,
                CenterSeparation);
        MaximumOccultedFraction = FMath::Max(
            MaximumOccultedFraction,
            OccultedArea / ApparentSunArea);
    }

    OutVisibleSunFraction = FMath::Clamp(
        1.0 - MaximumOccultedFraction,
        0.0,
        1.0);
    return true;
}

const FTGCelestialPlaybackBinding*
ATGSimulationPlaybackActor::FindBodyBinding(
    const FString& Key) const
{
    const FString Normalized = NormalizeColumnKey(Key);
    return CelestialBodyBindings.FindByPredicate(
        [&Normalized](const FTGCelestialPlaybackBinding& Binding)
        {
            return NormalizeColumnKey(Binding.BodyColumnKey) == Normalized;
        });
}

void ATGSimulationPlaybackActor::ApplyCelestialBodyRadius(
    AActor& BodyActor,
    const FCelestialTrack& Track,
    const int32 LowerIndex,
    const int32 UpperIndex,
    const double Alpha,
    const FVector& TargetCenterWorld) const
{
    if (!bScaleCelestialBodiesFromResultRadii ||
        Track.ReferenceRadius == INDEX_NONE ||
        !NumericRows.IsValidIndex(LowerIndex) ||
        !NumericRows.IsValidIndex(UpperIndex))
    {
        BodyActor.SetActorLocation(TargetCenterWorld);
        return;
    }

    const double ReferenceRadiusMeters = FMath::Lerp(
        NumericRows[LowerIndex][Track.ReferenceRadius],
        NumericRows[UpperIndex][Track.ReferenceRadius],
        Alpha);
    const double TargetRadiusWorldUnits =
        ReferenceRadiusMeters *
        WorldUnitsPerMeter *
        CelestialBodyVisualMagnification;
    if (TargetRadiusWorldUnits <= 0.0)
    {
        BodyActor.SetActorLocation(TargetCenterWorld);

        // A bound body Blueprint may carry an authored physical size when the
        // loaded SPICE PCK has no RADII entry (currently Pallas). Preserve that
        // construction-script scale. Only hide the unit engine sphere used for
        // an unbound body's generic fallback visual.
        const FTGCelestialPlaybackBinding* Binding =
            FindBodyBinding(Track.Key);
        const UClass* BoundActorClass =
            Binding != nullptr
                ? Binding->ActorClass.Get()
                : nullptr;
        if (BoundActorClass == nullptr ||
            !BodyActor.IsA(BoundActorClass))
        {
            BodyActor.SetActorScale3D(FVector::ZeroVector);
        }
        return;
    }

    // Work entirely in actor-local space. This deliberately ignores any
    // BeginPlay scale applied by a legacy celestial Blueprint, so the CSV
    // reference radius remains the sole authority for physical body size.
    const FBox LocalBounds =
        TGSimulationPlaybackPrivate::
            CalculateCelestialRadiusReferenceBounds(BodyActor);
    if (!LocalBounds.IsValid)
    {
        BodyActor.SetActorLocation(TargetCenterWorld);
        return;
    }

    const double LocalRadiusWorldUnits =
        LocalBounds.GetExtent().GetMax();
    if (LocalRadiusWorldUnits <= UE_DOUBLE_SMALL_NUMBER)
    {
        BodyActor.SetActorLocation(TargetCenterWorld);
        return;
    }

    const double UniformScale =
        TargetRadiusWorldUnits /
        LocalRadiusWorldUnits;
    BodyActor.SetActorScale3D(
        FVector(UniformScale));

    // A celestial visual's mesh origin is not assumed to coincide with its
    // actor origin. Rotate and scale the local bounds-center offset, then place
    // that actual visual center exactly at the backend-provided body center.
    const FVector BoundsCenterOffsetWorld =
        BodyActor.GetActorQuat().RotateVector(
            LocalBounds.GetCenter() * UniformScale);
    BodyActor.SetActorLocation(
        TargetCenterWorld - BoundsCenterOffsetWorld);
}

FString ATGSimulationPlaybackActor::NormalizeColumnKey(
    const FString& Value)
{
    FString Result = Value;
    for (TCHAR& Character : Result)
    {
        if (Character >= TEXT('A') && Character <= TEXT('Z'))
        {
            Character = Character - TEXT('A') + TEXT('a');
        }
        else if (!(
            (Character >= TEXT('a') && Character <= TEXT('z')) ||
            (Character >= TEXT('0') && Character <= TEXT('9'))))
        {
            Character = TEXT('_');
        }
    }
    return Result;
}

FString ATGSimulationPlaybackActor::ResolveInputPath(
    const FString& Value)
{
    FString Result = Value;
    Result.TrimStartAndEndInline();
    if (FPaths::IsRelative(Result))
    {
        Result = FPaths::Combine(FPaths::ProjectDir(), Result);
    }
    FPaths::NormalizeFilename(Result);
    return FPaths::ConvertRelativePathToFull(Result);
}

bool ATGSimulationPlaybackActor::ParseCsvLine(
    const FString& Line,
    TArray<FString>& OutFields)
{
    OutFields.Reset();
    FString Current;
    bool bInsideQuotes = false;

    for (int32 Index = 0; Index < Line.Len(); ++Index)
    {
        const TCHAR Character = Line[Index];
        if (Character == TEXT('"'))
        {
            if (bInsideQuotes && Index + 1 < Line.Len() &&
                Line[Index + 1] == TEXT('"'))
            {
                Current.AppendChar(TEXT('"'));
                ++Index;
            }
            else
            {
                bInsideQuotes = !bInsideQuotes;
            }
        }
        else if (Character == TEXT(',') && !bInsideQuotes)
        {
            OutFields.Add(MoveTemp(Current));
            Current.Reset();
        }
        else
        {
            Current.AppendChar(Character);
        }
    }

    if (bInsideQuotes)
    {
        OutFields.Reset();
        return false;
    }

    OutFields.Add(MoveTemp(Current));
    return true;
}

FQuat ATGSimulationPlaybackActor::QuaternionFromMatrixValues(
    const TArray<double>& Row,
    const FColumnRotation& Columns)
{
    double M[3][3];
    for (int32 Index = 0; Index < 9; ++Index)
    {
        M[Index / 3][Index % 3] = Row[Columns.Values[Index]];
    }

    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    double W = 1.0;
    const double Trace = M[0][0] + M[1][1] + M[2][2];

    if (Trace > 0.0)
    {
        const double S = 2.0 * FMath::Sqrt(Trace + 1.0);
        W = 0.25 * S;
        X = (M[2][1] - M[1][2]) / S;
        Y = (M[0][2] - M[2][0]) / S;
        Z = (M[1][0] - M[0][1]) / S;
    }
    else if (M[0][0] > M[1][1] && M[0][0] > M[2][2])
    {
        const double S = 2.0 * FMath::Sqrt(
            1.0 + M[0][0] - M[1][1] - M[2][2]);
        W = (M[2][1] - M[1][2]) / S;
        X = 0.25 * S;
        Y = (M[0][1] + M[1][0]) / S;
        Z = (M[0][2] + M[2][0]) / S;
    }
    else if (M[1][1] > M[2][2])
    {
        const double S = 2.0 * FMath::Sqrt(
            1.0 + M[1][1] - M[0][0] - M[2][2]);
        W = (M[0][2] - M[2][0]) / S;
        X = (M[0][1] + M[1][0]) / S;
        Y = 0.25 * S;
        Z = (M[1][2] + M[2][1]) / S;
    }
    else
    {
        const double S = 2.0 * FMath::Sqrt(
            1.0 + M[2][2] - M[0][0] - M[1][1]);
        W = (M[1][0] - M[0][1]) / S;
        X = (M[0][2] + M[2][0]) / S;
        Y = (M[1][2] + M[2][1]) / S;
        Z = 0.25 * S;
    }

    FQuat Result(X, Y, Z, W);
    Result.Normalize();
    return Result.ContainsNaN() ? FQuat::Identity : Result;
}
