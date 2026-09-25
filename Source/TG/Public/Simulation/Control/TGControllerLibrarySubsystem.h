// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "TGControllerLibrarySubsystem.generated.h"

UENUM(BlueprintType)
enum class ETGControllerBuildStatus : uint8
{
    NotBuilt UMETA(DisplayName = "Not Built"),
    Building UMETA(DisplayName = "Building"),
    Ready UMETA(DisplayName = "Ready"),
    Failed UMETA(DisplayName = "Failed"),
    IncompatibleController UMETA(DisplayName = "Incompatible Controller")
};

/** Read-only controller-library row consumed directly by the Controls HUD. */
USTRUCT(BlueprintType)
struct TG_API FTGControllerRecord
{
    GENERATED_BODY()

    /** Opaque, application-generated identity stored by FTGControlConfig. */
    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    FName ControllerId = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    ETGControllerBuildStatus BuildStatus =
        ETGControllerBuildStatus::NotBuilt;

    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    bool bTrusted = false;

    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    bool bHasEditableSource = false;

    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    FDateTime LastSourceModifiedUtc;

    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    FDateTime LastSuccessfulBuildUtc;

    /** Application-managed path shown only as diagnostic information. */
    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    FString ManagedSourceFilePath;

    /** Application-managed path shown only as diagnostic information. */
    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    FString ManagedDllFilePath;

    UPROPERTY(BlueprintReadOnly, Category = "Controller")
    FString LastBuildDiagnostics;
};

USTRUCT(BlueprintType)
struct TG_API FTGControllerToolchainStatus
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Controller Toolchain")
    bool bAvailable = false;

    UPROPERTY(BlueprintReadOnly, Category = "Controller Toolchain")
    bool bBundledWithApplication = false;

    UPROPERTY(BlueprintReadOnly, Category = "Controller Toolchain")
    FString CompilerPath;

    UPROPERTY(BlueprintReadOnly, Category = "Controller Toolchain")
    FString CompilerVersion;

    UPROPERTY(BlueprintReadOnly, Category = "Controller Toolchain")
    FText StatusMessage;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(
    FTGControllerLibraryChanged);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
    FTGControllerBuildFinished,
    FName,
    ControllerId,
    bool,
    bSucceeded,
    FText,
    Message);

/**
 * Non-widget controller management for the packaged application.
 *
 * Widgets only call these operations and bind to the two delegates. Source,
 * builds, registry metadata, compiler invocation, DLL validation, and stable
 * IDs are all owned here.
 */
UCLASS(BlueprintType)
class TG_API UTGControllerLibrarySubsystem final
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Controllers")
    FTGControllerLibraryChanged OnControllerLibraryChanged;

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|Controllers")
    FTGControllerBuildFinished OnControllerBuildFinished;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Controllers")
    TArray<FTGControllerRecord> GetControllers() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Controllers")
    bool FindController(
        FName ControllerId,
        FTGControllerRecord& OutController) const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool CreateControllerFromTemplate(
        const FString& DisplayName,
        bool bTrusted,
        FTGControllerRecord& OutController,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool ImportControllerSource(
        const FString& DisplayName,
        const FString& SourceCppFilePath,
        bool bTrusted,
        FTGControllerRecord& OutController,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool ImportPrebuiltControllerDll(
        const FString& DisplayName,
        const FString& DllFilePath,
        bool bTrusted,
        FTGControllerRecord& OutController,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool RenameController(
        FName ControllerId,
        const FString& NewDisplayName,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool LoadControllerSource(
        FName ControllerId,
        FString& OutSource,
        FText& OutError) const;

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool SaveControllerSource(
        FName ControllerId,
        const FString& Source,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool SetControllerTrusted(
        FName ControllerId,
        bool bTrusted,
        FText& OutError);

    /** Starts a background compiler process and returns immediately. */
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool BuildController(
        FName ControllerId,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    bool DeleteController(
        FName ControllerId,
        FText& OutError);

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    void RefreshControllerLibrary();

    UFUNCTION(BlueprintCallable, Category = "PHAROS|Controllers")
    FTGControllerToolchainStatus InspectControllerToolchain() const;

    UFUNCTION(BlueprintPure, Category = "PHAROS|Controllers")
    bool IsControllerReady(
        FName ControllerId,
        FText& OutReason) const;

    /** C++ adapter hook; the scenario HUD never receives this DLL path. */
    bool ResolveReadyControllerDllPath(
        FName ControllerId,
        FString& OutDllPath,
        FString& OutError) const;

private:
    static constexpr int32 RegistryFormatVersion = 1;
    static constexpr int64 MaximumSourceBytes = 2 * 1024 * 1024;
    static constexpr int32 MaximumDiagnosticsCharacters = 1024 * 1024;

    TArray<FTGControllerRecord> Controllers;

    FString GetControllerRootDirectory() const;
    FString GetControllerDirectory(FName ControllerId) const;
    FString GetRegistryFilePath() const;
    FString ResolveSdkDirectory() const;
    FString ResolveTemplateFilePath() const;

    bool EnsureStorage(FText& OutError) const;
    bool LoadRegistry(FText& OutError);
    bool SaveRegistry(FText& OutError) const;
    bool ValidateDisplayName(
        const FString& DisplayName,
        FName IgnoredControllerId,
        FString& OutTrimmedName,
        FText& OutError) const;
    bool CreateControllerFromSourceText(
        const FString& DisplayName,
        const FString& Source,
        bool bTrusted,
        FTGControllerRecord& OutController,
        FText& OutError);

    FTGControllerRecord* FindMutable(FName ControllerId);
    const FTGControllerRecord* FindInternal(FName ControllerId) const;
    void BroadcastLibraryChanged();
};
