// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/Control/TGControllerLibrarySubsystem.h"

#include "Simulation/Control/TGDynamicControllerAdapter.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformMisc.h"
#include "HAL/PlatformProcess.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    constexpr TCHAR ControllerDirectoryName[] = TEXT("Controllers");
    constexpr TCHAR RegistryFileName[] = TEXT("ControllerRegistry.json");
    constexpr TCHAR ManagedSourceFileName[] = TEXT("Controller.cpp");

    FString NormalizeFilePath(const FString& Path)
    {
        FString Result = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeFilename(Result);
        return Result;
    }

    FString NormalizeDirectoryPath(const FString& Path)
    {
        FString Result = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeDirectoryName(Result);
        return Result;
    }

    bool IsPathInsideDirectory(
        const FString& CandidatePath,
        const FString& RootDirectory)
    {
        const FString Candidate = NormalizeFilePath(CandidatePath);
        const FString Root = NormalizeDirectoryPath(RootDirectory);
        const FString RootPrefix = Root + TEXT("/");

        return Candidate.StartsWith(
            RootPrefix,
            ESearchCase::IgnoreCase);
    }

    bool IsValidControllerId(const FName ControllerId)
    {
        if (ControllerId.IsNone())
        {
            return false;
        }

        const FString Text = ControllerId.ToString();
        constexpr TCHAR Prefix[] = TEXT("TGController_");

        if (!Text.StartsWith(Prefix, ESearchCase::CaseSensitive))
        {
            return false;
        }

        FGuid ParsedGuid;
        return FGuid::ParseExact(
            Text.RightChop(UE_ARRAY_COUNT(Prefix) - 1),
            EGuidFormats::Digits,
            ParsedGuid);
    }

    FName MakeControllerId()
    {
        return FName(*FString::Printf(
            TEXT("TGController_%s"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)));
    }

    FString QuoteCommandLineArgument(FString Value)
    {
        Value.ReplaceInline(TEXT("\""), TEXT("\\\""));
        return FString::Printf(TEXT("\"%s\""), *Value);
    }

    bool FindExecutableOnPath(
        const FString& ExecutableName,
        FString& OutPath)
    {
#if PLATFORM_WINDOWS
        int32 ReturnCode = INDEX_NONE;
        FString StandardOutput;
        FString StandardError;

        const bool bLaunched = FPlatformProcess::ExecProcess(
            TEXT("where.exe"),
            *QuoteCommandLineArgument(ExecutableName),
            &ReturnCode,
            &StandardOutput,
            &StandardError);

        if (!bLaunched || ReturnCode != 0)
        {
            return false;
        }

        TArray<FString> Lines;
        StandardOutput.ParseIntoArrayLines(Lines, true);

        for (FString Line : Lines)
        {
            Line.TrimStartAndEndInline();

            if (!Line.IsEmpty() && FPaths::FileExists(Line))
            {
                OutPath = NormalizeFilePath(Line);
                return true;
            }
        }
#endif

        return false;
    }

    bool ResolveCompiler(
        FString& OutCompilerPath,
        bool& OutBundled)
    {
        OutCompilerPath.Reset();
        OutBundled = false;

#if !PLATFORM_WINDOWS
        return false;
#else
        const TArray<FString> BundledCandidates = {
            FPaths::Combine(
                FPlatformProcess::BaseDir(),
                TEXT("ControllerToolchain/bin/"
                     "x86_64-w64-mingw32-clang++.exe")),
            FPaths::Combine(
                FPlatformProcess::BaseDir(),
                TEXT("ControllerToolchain/bin/clang++.exe")),
            FPaths::Combine(
                FPaths::ProjectDir(),
                TEXT("Build/ControllerToolchain/Win64/bin/"
                     "x86_64-w64-mingw32-clang++.exe")),
            FPaths::Combine(
                FPaths::ProjectDir(),
                TEXT("Build/ControllerToolchain/Win64/bin/clang++.exe"))};

        for (const FString& Candidate : BundledCandidates)
        {
            if (FPaths::FileExists(Candidate))
            {
                OutCompilerPath = NormalizeFilePath(Candidate);
                OutBundled = true;
                return true;
            }
        }

        FString EnvironmentCompiler =
            FPlatformMisc::GetEnvironmentVariable(
                TEXT("PHAROS_CONTROLLER_COMPILER"));

        if (EnvironmentCompiler.IsEmpty())
        {
            EnvironmentCompiler =
                FPlatformMisc::GetEnvironmentVariable(
                    TEXT("TG_CONTROLLER_COMPILER"));
        }

        if (!EnvironmentCompiler.IsEmpty() &&
            FPaths::FileExists(EnvironmentCompiler))
        {
            OutCompilerPath =
                NormalizeFilePath(EnvironmentCompiler);
            return true;
        }

        const TArray<FString> PathCandidates = {
            TEXT("x86_64-w64-mingw32-clang++.exe"),
            TEXT("clang++.exe"),
            TEXT("g++.exe")};

        for (const FString& ExecutableName : PathCandidates)
        {
            if (FindExecutableOnPath(
                    ExecutableName,
                    OutCompilerPath))
            {
                return true;
            }
        }

        return false;
#endif
    }

    FString FirstNonemptyLine(const FString& Text)
    {
        TArray<FString> Lines;
        Text.ParseIntoArrayLines(Lines, true);

        for (FString Line : Lines)
        {
            Line.TrimStartAndEndInline();

            if (!Line.IsEmpty())
            {
                return Line;
            }
        }

        return {};
    }

    FString ClampDiagnostics(FString Diagnostics)
    {
        constexpr int32 MaximumCharacters = 1024 * 1024;

        if (Diagnostics.Len() <= MaximumCharacters)
        {
            return Diagnostics;
        }

        Diagnostics.LeftInline(MaximumCharacters);
        Diagnostics += TEXT(
            "\n\n[Diagnostics truncated by PHAROS after 1 MiB.]\n");
        return Diagnostics;
    }

    FString BuildCompilerArguments(
        const FString& SdkDirectory,
        const FString& SourcePath,
        const FString& OutputDllPath)
    {
        return FString::Printf(
            TEXT(
                "-std=c++17 -O2 -DNDEBUG "
                "-DTG_CONTROLLER_BUILD=1 -shared -static "
                "-static-libgcc -static-libstdc++ "
                "-Wl,--no-undefined -I%s %s -o %s"),
            *QuoteCommandLineArgument(SdkDirectory),
            *QuoteCommandLineArgument(SourcePath),
            *QuoteCommandLineArgument(OutputDllPath));
    }

    FString StatusToString(
        const ETGControllerBuildStatus Status)
    {
        switch (Status)
        {
            case ETGControllerBuildStatus::Building:
                return TEXT("Building");
            case ETGControllerBuildStatus::Ready:
                return TEXT("Ready");
            case ETGControllerBuildStatus::Failed:
                return TEXT("Failed");
            case ETGControllerBuildStatus::IncompatibleController:
                return TEXT("IncompatibleController");
            case ETGControllerBuildStatus::NotBuilt:
            default:
                return TEXT("NotBuilt");
        }
    }

    ETGControllerBuildStatus StatusFromString(
        const FString& Status)
    {
        if (Status == TEXT("Building"))
        {
            return ETGControllerBuildStatus::Building;
        }
        if (Status == TEXT("Ready"))
        {
            return ETGControllerBuildStatus::Ready;
        }
        if (Status == TEXT("Failed"))
        {
            return ETGControllerBuildStatus::Failed;
        }
        if (Status == TEXT("IncompatibleController"))
        {
            return ETGControllerBuildStatus::IncompatibleController;
        }

        return ETGControllerBuildStatus::NotBuilt;
    }

    FString DateTimeToJson(const FDateTime& Value)
    {
        return Value.GetTicks() > 0
            ? Value.ToIso8601()
            : FString{};
    }

    FDateTime DateTimeFromJson(const FString& Value)
    {
        FDateTime Result;
        return FDateTime::ParseIso8601(*Value, Result)
            ? Result
            : FDateTime{};
    }
}

void UTGControllerLibrarySubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FText Error;
    EnsureStorage(Error);
    LoadRegistry(Error);
    RefreshControllerLibrary();
}

void UTGControllerLibrarySubsystem::Deinitialize()
{
    FText IgnoredError;
    SaveRegistry(IgnoredError);
    Controllers.Reset();

    Super::Deinitialize();
}

TArray<FTGControllerRecord>
UTGControllerLibrarySubsystem::GetControllers() const
{
    TArray<FTGControllerRecord> Result = Controllers;

    Result.Sort(
        [](const FTGControllerRecord& A,
           const FTGControllerRecord& B)
        {
            return A.DisplayName.Compare(
                       B.DisplayName,
                       ESearchCase::IgnoreCase) < 0;
        });

    return Result;
}

bool UTGControllerLibrarySubsystem::FindController(
    const FName ControllerId,
    FTGControllerRecord& OutController) const
{
    const FTGControllerRecord* Found =
        FindInternal(ControllerId);

    if (Found == nullptr)
    {
        OutController = FTGControllerRecord{};
        return false;
    }

    OutController = *Found;
    return true;
}

bool UTGControllerLibrarySubsystem::CreateControllerFromTemplate(
    const FString& DisplayName,
    const bool bTrusted,
    FTGControllerRecord& OutController,
    FText& OutError)
{
    const FString TemplatePath = ResolveTemplateFilePath();
    FString Source;

    if (TemplatePath.IsEmpty() ||
        !FFileHelper::LoadFileToString(Source, *TemplatePath))
    {
        OutController = FTGControllerRecord{};
        OutError = FText::FromString(
            TEXT("The packaged ControllerTemplate.cpp could not be read."));
        return false;
    }

    return CreateControllerFromSourceText(
        DisplayName,
        Source,
        bTrusted,
        OutController,
        OutError);
}

bool UTGControllerLibrarySubsystem::ImportControllerSource(
    const FString& DisplayName,
    const FString& SourceCppFilePath,
    const bool bTrusted,
    FTGControllerRecord& OutController,
    FText& OutError)
{
    OutController = FTGControllerRecord{};
    const FString SourcePath =
        NormalizeFilePath(SourceCppFilePath);

    if (!FPaths::FileExists(SourcePath) ||
        !FPaths::GetExtension(SourcePath, false).Equals(
            TEXT("cpp"),
            ESearchCase::IgnoreCase))
    {
        OutError = FText::FromString(
            TEXT("Select one existing .cpp controller source file."));
        return false;
    }

    const int64 FileSize =
        IFileManager::Get().FileSize(*SourcePath);

    if (FileSize < 0 || FileSize > MaximumSourceBytes)
    {
        OutError = FText::FromString(
            TEXT("Controller source must not exceed 2 MiB."));
        return false;
    }

    FString Source;

    if (!FFileHelper::LoadFileToString(Source, *SourcePath))
    {
        OutError = FText::FromString(
            TEXT("The selected controller source could not be read."));
        return false;
    }

    return CreateControllerFromSourceText(
        DisplayName,
        Source,
        bTrusted,
        OutController,
        OutError);
}

bool UTGControllerLibrarySubsystem::ImportPrebuiltControllerDll(
    const FString& DisplayName,
    const FString& DllFilePath,
    const bool bTrusted,
    FTGControllerRecord& OutController,
    FText& OutError)
{
    OutController = FTGControllerRecord{};

    if (!bTrusted)
    {
        OutError = FText::FromString(
            TEXT("Confirm that the native controller DLL is trusted before importing it."));
        return false;
    }

    FString TrimmedName;

    if (!ValidateDisplayName(
            DisplayName,
            NAME_None,
            TrimmedName,
            OutError))
    {
        return false;
    }

    const FString SourceDllPath =
        NormalizeFilePath(DllFilePath);

    if (!FPaths::FileExists(SourceDllPath) ||
        !FPaths::GetExtension(SourceDllPath, false).Equals(
            TEXT("dll"),
            ESearchCase::IgnoreCase))
    {
        OutError = FText::FromString(
            TEXT("Select one existing controller .dll file."));
        return false;
    }

    FString ProbeError;

    if (!FTGDynamicControllerAdapter::ProbeDll(
            SourceDllPath,
            ProbeError))
    {
        OutError = FText::FromString(ProbeError);
        return false;
    }

    if (!EnsureStorage(OutError))
    {
        return false;
    }

    const FName ControllerId = MakeControllerId();
    const FString ControllerDirectory =
        GetControllerDirectory(ControllerId);
    const FString ImportDirectory = FPaths::Combine(
        ControllerDirectory,
        TEXT("Builds"),
        FString::Printf(
            TEXT("Imported_%s"),
            *FGuid::NewGuid().ToString(EGuidFormats::Digits)));

    if (!IFileManager::Get().MakeDirectory(
            *ImportDirectory,
            true))
    {
        OutError = FText::FromString(
            TEXT("The managed controller directory could not be created."));
        return false;
    }

    const FString ManagedDllPath =
        FPaths::Combine(ImportDirectory, TEXT("Controller.dll"));

    if (IFileManager::Get().Copy(
            *ManagedDllPath,
            *SourceDllPath,
            true,
            true) != COPY_OK)
    {
        OutError = FText::FromString(
            TEXT("The controller DLL could not be copied into managed storage."));
        return false;
    }

    FTGControllerRecord Record;
    Record.ControllerId = ControllerId;
    Record.DisplayName = MoveTemp(TrimmedName);
    Record.BuildStatus = ETGControllerBuildStatus::Ready;
    Record.bTrusted = true;
    Record.bHasEditableSource = false;
    Record.LastSuccessfulBuildUtc = FDateTime::UtcNow();
    Record.ManagedDllFilePath = NormalizeFilePath(ManagedDllPath);
    Record.LastBuildDiagnostics =
        TEXT("Prebuilt controller DLL imported and validated successfully.");

    Controllers.Add(Record);

    if (!SaveRegistry(OutError))
    {
        Controllers.Pop();
        return false;
    }

    OutController = Record;
    BroadcastLibraryChanged();
    return true;
}

bool UTGControllerLibrarySubsystem::RenameController(
    const FName ControllerId,
    const FString& NewDisplayName,
    FText& OutError)
{
    FTGControllerRecord* Record = FindMutable(ControllerId);

    if (Record == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The selected controller no longer exists."));
        return false;
    }

    FString TrimmedName;

    if (!ValidateDisplayName(
            NewDisplayName,
            ControllerId,
            TrimmedName,
            OutError))
    {
        return false;
    }

    Record->DisplayName = MoveTemp(TrimmedName);

    if (!SaveRegistry(OutError))
    {
        return false;
    }

    BroadcastLibraryChanged();
    return true;
}

bool UTGControllerLibrarySubsystem::LoadControllerSource(
    const FName ControllerId,
    FString& OutSource,
    FText& OutError) const
{
    OutSource.Reset();
    const FTGControllerRecord* Record =
        FindInternal(ControllerId);

    if (Record == nullptr ||
        !Record->bHasEditableSource ||
        Record->ManagedSourceFilePath.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("The selected controller has no editable source."));
        return false;
    }

    if (!FFileHelper::LoadFileToString(
            OutSource,
            *Record->ManagedSourceFilePath))
    {
        OutError = FText::FromString(
            TEXT("The managed controller source could not be read."));
        return false;
    }

    OutError = FText::GetEmpty();
    return true;
}

bool UTGControllerLibrarySubsystem::SaveControllerSource(
    const FName ControllerId,
    const FString& Source,
    FText& OutError)
{
    FTGControllerRecord* Record = FindMutable(ControllerId);

    if (Record == nullptr ||
        !Record->bHasEditableSource ||
        Record->ManagedSourceFilePath.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("The selected controller has no editable source."));
        return false;
    }

    const FTCHARToUTF8 Utf8Source(*Source);

    if (Utf8Source.Length() > MaximumSourceBytes)
    {
        OutError = FText::FromString(
            TEXT("Controller source must not exceed 2 MiB."));
        return false;
    }

    const FString ControllerDirectory =
        GetControllerDirectory(ControllerId);

    if (!IsPathInsideDirectory(
            Record->ManagedSourceFilePath,
            ControllerDirectory))
    {
        OutError = FText::FromString(
            TEXT("The managed source path is outside its controller directory."));
        return false;
    }

    if (!FFileHelper::SaveStringToFile(
            Source,
            *Record->ManagedSourceFilePath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FText::FromString(
            TEXT("The controller source could not be saved."));
        return false;
    }

    Record->LastSourceModifiedUtc = FDateTime::UtcNow();
    Record->BuildStatus = ETGControllerBuildStatus::NotBuilt;
    Record->ManagedDllFilePath.Reset();
    Record->LastBuildDiagnostics =
        TEXT("Source changed. Build the controller before selecting it.");

    if (!SaveRegistry(OutError))
    {
        return false;
    }

    BroadcastLibraryChanged();
    return true;
}

bool UTGControllerLibrarySubsystem::SetControllerTrusted(
    const FName ControllerId,
    const bool bTrusted,
    FText& OutError)
{
    FTGControllerRecord* Record = FindMutable(ControllerId);

    if (Record == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The selected controller no longer exists."));
        return false;
    }

    Record->bTrusted = bTrusted;

    if (!SaveRegistry(OutError))
    {
        return false;
    }

    BroadcastLibraryChanged();
    return true;
}

bool UTGControllerLibrarySubsystem::BuildController(
    const FName ControllerId,
    FText& OutError)
{
#if !PLATFORM_WINDOWS
    OutError = FText::FromString(
        TEXT("Controller DLL builds are currently supported only on Win64."));
    return false;
#else
    FTGControllerRecord* Record = FindMutable(ControllerId);

    if (Record == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The selected controller no longer exists."));
        return false;
    }

    if (Record->BuildStatus == ETGControllerBuildStatus::Building)
    {
        OutError = FText::FromString(
            TEXT("This controller is already being built."));
        return false;
    }

    const FString ControllerDirectory =
        GetControllerDirectory(ControllerId);

    if (FTGDynamicControllerAdapter::IsAnyDllLoadedBelow(
            ControllerDirectory))
    {
        OutError = FText::FromString(
            TEXT("Stop the simulation using this controller before rebuilding it."));
        return false;
    }

    if (!Record->bTrusted)
    {
        OutError = FText::FromString(
            TEXT("Confirm that this native controller source is trusted before building it."));
        return false;
    }

    if (!Record->bHasEditableSource ||
        Record->ManagedSourceFilePath.IsEmpty() ||
        !FPaths::FileExists(Record->ManagedSourceFilePath))
    {
        OutError = FText::FromString(
            TEXT("The selected controller has no buildable source."));
        return false;
    }

    const FString SdkDirectory = ResolveSdkDirectory();

    if (SdkDirectory.IsEmpty() ||
        !FPaths::FileExists(
            FPaths::Combine(
                SdkDirectory,
                TEXT("PHAROSControllerAPI.h"))))
    {
        OutError = FText::FromString(
            TEXT("The packaged PHAROS Controller SDK is missing."));
        return false;
    }

    FString CompilerPath;
    bool bBundledCompiler = false;

    if (!ResolveCompiler(CompilerPath, bBundledCompiler))
    {
        OutError = FText::FromString(
            TEXT(
                "No PHAROS controller compiler is available. The packaged "
                "ControllerToolchain is missing."));
        return false;
    }

    const FString BuildId = FString::Printf(
        TEXT("%lld_%s"),
        FDateTime::UtcNow().ToUnixTimestamp(),
        *FGuid::NewGuid().ToString(EGuidFormats::Digits));

    const FString OutputDirectory = FPaths::Combine(
        ControllerDirectory,
        TEXT("Builds"),
        BuildId);

    if (!IFileManager::Get().MakeDirectory(
            *OutputDirectory,
            true))
    {
        OutError = FText::FromString(
            TEXT("The controller build directory could not be created."));
        return false;
    }

    const FString OutputDllPath =
        NormalizeFilePath(
            FPaths::Combine(
                OutputDirectory,
                TEXT("Controller.dll")));

    const FString SourcePath = Record->ManagedSourceFilePath;
    const FString Arguments = BuildCompilerArguments(
        SdkDirectory,
        SourcePath,
        OutputDllPath);

    Record->BuildStatus = ETGControllerBuildStatus::Building;
    Record->ManagedDllFilePath.Reset();
    Record->LastBuildDiagnostics = FString::Printf(
        TEXT("Building with %s..."),
        bBundledCompiler
            ? TEXT("the bundled compiler")
            : TEXT("a development compiler"));

    if (!SaveRegistry(OutError))
    {
        Record->BuildStatus = ETGControllerBuildStatus::Failed;
        return false;
    }

    BroadcastLibraryChanged();
    OutError = FText::GetEmpty();

    const TWeakObjectPtr<UTGControllerLibrarySubsystem> WeakThis(this);

    Async(
        EAsyncExecution::ThreadPool,
        [WeakThis,
         ControllerId,
         CompilerPath,
         Arguments,
         OutputDllPath]()
        {
            int32 ReturnCode = INDEX_NONE;
            FString StandardOutput;
            FString StandardError;

            const bool bLaunched = FPlatformProcess::ExecProcess(
                *CompilerPath,
                *Arguments,
                &ReturnCode,
                &StandardOutput,
                &StandardError);

            FString Diagnostics;

            if (!StandardOutput.IsEmpty())
            {
                Diagnostics += StandardOutput;
            }

            if (!StandardError.IsEmpty())
            {
                if (!Diagnostics.IsEmpty() &&
                    !Diagnostics.EndsWith(TEXT("\n")))
                {
                    Diagnostics += TEXT("\n");
                }

                Diagnostics += StandardError;
            }

            Diagnostics = ClampDiagnostics(MoveTemp(Diagnostics));

            AsyncTask(
                ENamedThreads::GameThread,
                [WeakThis,
                 ControllerId,
                 OutputDllPath,
                 bLaunched,
                 ReturnCode,
                 Diagnostics = MoveTemp(Diagnostics)]() mutable
                {
                    UTGControllerLibrarySubsystem* Subsystem =
                        WeakThis.Get();

                    if (Subsystem == nullptr)
                    {
                        return;
                    }

                    FTGControllerRecord* FinishedRecord =
                        Subsystem->FindMutable(ControllerId);

                    if (FinishedRecord == nullptr)
                    {
                        return;
                    }

                    bool bSucceeded = false;
                    FString CompletionMessage;

                    if (!bLaunched)
                    {
                        CompletionMessage =
                            TEXT("The controller compiler could not be started.");
                        FinishedRecord->BuildStatus =
                            ETGControllerBuildStatus::Failed;
                    }
                    else if (ReturnCode != 0 ||
                             !FPaths::FileExists(OutputDllPath))
                    {
                        CompletionMessage = FString::Printf(
                            TEXT("Controller compilation failed with exit code %d."),
                            ReturnCode);
                        FinishedRecord->BuildStatus =
                            ETGControllerBuildStatus::Failed;
                    }
                    else
                    {
                        FString ProbeError;

                        if (FTGDynamicControllerAdapter::ProbeDll(
                                OutputDllPath,
                                ProbeError))
                        {
                            FinishedRecord->BuildStatus =
                                ETGControllerBuildStatus::Ready;
                            FinishedRecord->ManagedDllFilePath =
                                OutputDllPath;
                            FinishedRecord->LastSuccessfulBuildUtc =
                                FDateTime::UtcNow();
                            CompletionMessage =
                                TEXT("Controller built and validated successfully.");
                            bSucceeded = true;
                        }
                        else
                        {
                            FinishedRecord->BuildStatus =
                                ProbeError.Contains(TEXT("Controller API"))
                                    ? ETGControllerBuildStatus::IncompatibleController
                                    : ETGControllerBuildStatus::Failed;
                            CompletionMessage = ProbeError;
                        }
                    }

                    if (Diagnostics.IsEmpty())
                    {
                        FinishedRecord->LastBuildDiagnostics =
                            CompletionMessage;
                    }
                    else
                    {
                        FinishedRecord->LastBuildDiagnostics =
                            CompletionMessage + TEXT("\n\n") + Diagnostics;
                    }

                    FText SaveError;
                    Subsystem->SaveRegistry(SaveError);
                    Subsystem->BroadcastLibraryChanged();
                    Subsystem->OnControllerBuildFinished.Broadcast(
                        ControllerId,
                        bSucceeded,
                        FText::FromString(CompletionMessage));
                });
        });

    return true;
#endif
}

bool UTGControllerLibrarySubsystem::DeleteController(
    const FName ControllerId,
    FText& OutError)
{
    const int32 RecordIndex = Controllers.IndexOfByPredicate(
        [ControllerId](const FTGControllerRecord& Record)
        {
            return Record.ControllerId == ControllerId;
        });

    if (RecordIndex == INDEX_NONE)
    {
        OutError = FText::FromString(
            TEXT("The selected controller no longer exists."));
        return false;
    }

    if (Controllers[RecordIndex].BuildStatus ==
        ETGControllerBuildStatus::Building)
    {
        OutError = FText::FromString(
            TEXT("Wait for the controller build to finish before deleting it."));
        return false;
    }

    const FString RootDirectory =
        GetControllerRootDirectory();
    const FString ControllerDirectory =
        GetControllerDirectory(ControllerId);

    if (!IsPathInsideDirectory(
            FPaths::Combine(ControllerDirectory, TEXT("placeholder")),
            RootDirectory))
    {
        OutError = FText::FromString(
            TEXT("Refusing to delete an invalid controller directory."));
        return false;
    }

    if (FTGDynamicControllerAdapter::IsAnyDllLoadedBelow(
            ControllerDirectory))
    {
        OutError = FText::FromString(
            TEXT("Stop the simulation using this controller before deleting it."));
        return false;
    }

    if (IFileManager::Get().DirectoryExists(*ControllerDirectory) &&
        !IFileManager::Get().DeleteDirectory(
            *ControllerDirectory,
            false,
            true))
    {
        OutError = FText::FromString(
            TEXT("The managed controller directory could not be deleted."));
        return false;
    }

    Controllers.RemoveAt(RecordIndex);

    if (!SaveRegistry(OutError))
    {
        return false;
    }

    BroadcastLibraryChanged();
    return true;
}

void UTGControllerLibrarySubsystem::RefreshControllerLibrary()
{
    for (FTGControllerRecord& Record : Controllers)
    {
        const FString ControllerDirectory =
            GetControllerDirectory(Record.ControllerId);

        const FString ExpectedSourcePath =
            FPaths::Combine(
                ControllerDirectory,
                ManagedSourceFileName);

        if (Record.bHasEditableSource)
        {
            Record.ManagedSourceFilePath =
                NormalizeFilePath(ExpectedSourcePath);

            if (!FPaths::FileExists(
                    Record.ManagedSourceFilePath))
            {
                Record.BuildStatus =
                    ETGControllerBuildStatus::Failed;
                Record.LastBuildDiagnostics =
                    TEXT("The managed controller source is missing.");
            }
        }
        else
        {
            Record.ManagedSourceFilePath.Reset();
        }

        if (Record.BuildStatus ==
            ETGControllerBuildStatus::Building)
        {
            Record.BuildStatus =
                ETGControllerBuildStatus::Failed;
            Record.LastBuildDiagnostics =
                TEXT("The previous controller build was interrupted.");
        }

        if (!Record.ManagedDllFilePath.IsEmpty())
        {
            Record.ManagedDllFilePath =
                NormalizeFilePath(Record.ManagedDllFilePath);

            if (!IsPathInsideDirectory(
                    Record.ManagedDllFilePath,
                    ControllerDirectory) ||
                !FPaths::FileExists(Record.ManagedDllFilePath))
            {
                Record.BuildStatus =
                    ETGControllerBuildStatus::Failed;
                Record.ManagedDllFilePath.Reset();
                Record.LastBuildDiagnostics =
                    TEXT("The managed controller DLL is missing or invalid.");
            }
            else if (Record.BuildStatus ==
                         ETGControllerBuildStatus::Ready ||
                     Record.BuildStatus ==
                         ETGControllerBuildStatus::IncompatibleController)
            {
                FString ProbeError;

                if (FTGDynamicControllerAdapter::ProbeDll(
                        Record.ManagedDllFilePath,
                        ProbeError))
                {
                    Record.BuildStatus =
                        ETGControllerBuildStatus::Ready;
                }
                else
                {
                    Record.BuildStatus =
                        ProbeError.Contains(TEXT("Controller API"))
                            ? ETGControllerBuildStatus::IncompatibleController
                            : ETGControllerBuildStatus::Failed;
                    Record.LastBuildDiagnostics = ProbeError;
                }
            }
        }
    }

    FText IgnoredError;
    SaveRegistry(IgnoredError);
    BroadcastLibraryChanged();
}

FTGControllerToolchainStatus
UTGControllerLibrarySubsystem::InspectControllerToolchain() const
{
    FTGControllerToolchainStatus Result;
    FString CompilerPath;
    bool bBundled = false;

    if (!ResolveCompiler(CompilerPath, bBundled))
    {
        Result.StatusMessage = FText::FromString(
            TEXT(
                "No controller compiler was found. Install the packaged "
                "ControllerToolchain before distributing PHAROS."));
        return Result;
    }

    Result.bAvailable = true;
    Result.bBundledWithApplication = bBundled;
    Result.CompilerPath = CompilerPath;

    int32 ReturnCode = INDEX_NONE;
    FString StandardOutput;
    FString StandardError;

    if (FPlatformProcess::ExecProcess(
            *CompilerPath,
            TEXT("--version"),
            &ReturnCode,
            &StandardOutput,
            &StandardError) &&
        ReturnCode == 0)
    {
        Result.CompilerVersion =
            FirstNonemptyLine(StandardOutput);
    }

    Result.StatusMessage = FText::FromString(
        bBundled
            ? TEXT("The bundled controller compiler is ready.")
            : TEXT(
                "A development compiler is available. Packaged releases "
                "must include ControllerToolchain."));

    return Result;
}

bool UTGControllerLibrarySubsystem::IsControllerReady(
    const FName ControllerId,
    FText& OutReason) const
{
    FString DllPath;
    FString Error;
    const bool bReady = ResolveReadyControllerDllPath(
        ControllerId,
        DllPath,
        Error);

    OutReason = bReady
        ? FText::GetEmpty()
        : FText::FromString(Error);

    return bReady;
}

bool UTGControllerLibrarySubsystem::ResolveReadyControllerDllPath(
    const FName ControllerId,
    FString& OutDllPath,
    FString& OutError) const
{
    OutDllPath.Reset();
    OutError.Reset();

    const FTGControllerRecord* Record =
        FindInternal(ControllerId);

    if (Record == nullptr)
    {
        OutError = TEXT("The selected controller is not registered.");
        return false;
    }

    if (!Record->bTrusted)
    {
        OutError = TEXT("The selected controller has not been trusted.");
        return false;
    }

    if (Record->BuildStatus != ETGControllerBuildStatus::Ready)
    {
        OutError = TEXT("The selected controller is not in the Ready state.");
        return false;
    }

    if (Record->ManagedDllFilePath.IsEmpty() ||
        !FPaths::FileExists(Record->ManagedDllFilePath))
    {
        OutError = TEXT("The selected controller DLL is missing.");
        return false;
    }

    if (!FTGDynamicControllerAdapter::ProbeDll(
            Record->ManagedDllFilePath,
            OutError))
    {
        return false;
    }

    OutDllPath = Record->ManagedDllFilePath;
    return true;
}

FString UTGControllerLibrarySubsystem::GetControllerRootDirectory() const
{
    return NormalizeDirectoryPath(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            ControllerDirectoryName));
}

FString UTGControllerLibrarySubsystem::GetControllerDirectory(
    const FName ControllerId) const
{
    return NormalizeDirectoryPath(
        FPaths::Combine(
            GetControllerRootDirectory(),
            ControllerId.ToString()));
}

FString UTGControllerLibrarySubsystem::GetRegistryFilePath() const
{
    return NormalizeFilePath(
        FPaths::Combine(
            GetControllerRootDirectory(),
            RegistryFileName));
}

FString UTGControllerLibrarySubsystem::ResolveSdkDirectory() const
{
    const TArray<FString> Candidates = {
        FPaths::Combine(
            FPlatformProcess::BaseDir(),
            TEXT("ControllerSDK")),
        FPaths::Combine(
            FPaths::ProjectDir(),
            TEXT("ControllerSDK"))};

    for (const FString& Candidate : Candidates)
    {
        const FString PublicHeaderPath = FPaths::Combine(
            Candidate,
            TEXT("PHAROSControllerAPI.h"));
        const FString InternalHeaderPath = FPaths::Combine(
            Candidate,
            TEXT("TGControllerAPI.h"));

        if (FPaths::FileExists(PublicHeaderPath) ||
            FPaths::FileExists(InternalHeaderPath))
        {
            return NormalizeDirectoryPath(Candidate);
        }
    }

    return {};
}

FString UTGControllerLibrarySubsystem::ResolveTemplateFilePath() const
{
    const FString SdkDirectory = ResolveSdkDirectory();

    if (SdkDirectory.IsEmpty())
    {
        return {};
    }

    const FString TemplatePath = FPaths::Combine(
        SdkDirectory,
        TEXT("ControllerTemplate.cpp"));

    return FPaths::FileExists(TemplatePath)
        ? NormalizeFilePath(TemplatePath)
        : FString{};
}

bool UTGControllerLibrarySubsystem::EnsureStorage(
    FText& OutError) const
{
    const FString RootDirectory =
        GetControllerRootDirectory();

    if (!IFileManager::Get().MakeDirectory(
            *RootDirectory,
            true))
    {
        OutError = FText::FromString(
            TEXT("The writable controller-library directory could not be created."));
        return false;
    }

    OutError = FText::GetEmpty();
    return true;
}

bool UTGControllerLibrarySubsystem::LoadRegistry(FText& OutError)
{
    Controllers.Reset();

    if (!EnsureStorage(OutError))
    {
        return false;
    }

    const FString RegistryPath = GetRegistryFilePath();

    if (!FPaths::FileExists(RegistryPath))
    {
        OutError = FText::GetEmpty();
        return true;
    }

    FString JsonText;

    if (!FFileHelper::LoadFileToString(JsonText, *RegistryPath))
    {
        OutError = FText::FromString(
            TEXT("The controller registry could not be read."));
        return false;
    }

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonText);

    if (!FJsonSerializer::Deserialize(Reader, RootObject) ||
        !RootObject.IsValid())
    {
        OutError = FText::FromString(
            TEXT("The controller registry JSON is invalid."));
        return false;
    }

    double FormatVersion = 0.0;

    if (!RootObject->TryGetNumberField(
            TEXT("formatVersion"),
            FormatVersion) ||
        static_cast<int32>(FormatVersion) != RegistryFormatVersion)
    {
        OutError = FText::FromString(
            TEXT("The controller registry format is unsupported."));
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* JsonControllers = nullptr;

    if (!RootObject->TryGetArrayField(
            TEXT("controllers"),
            JsonControllers) ||
        JsonControllers == nullptr)
    {
        OutError = FText::GetEmpty();
        return true;
    }

    const FString RootDirectory =
        GetControllerRootDirectory();

    for (const TSharedPtr<FJsonValue>& JsonValue : *JsonControllers)
    {
        const TSharedPtr<FJsonObject> JsonRecord =
            JsonValue.IsValid()
                ? JsonValue->AsObject()
                : nullptr;

        if (!JsonRecord.IsValid())
        {
            continue;
        }

        FString IdText;
        FString DisplayName;

        if (!JsonRecord->TryGetStringField(TEXT("id"), IdText) ||
            !JsonRecord->TryGetStringField(
                TEXT("displayName"),
                DisplayName))
        {
            continue;
        }

        const FName ControllerId(*IdText);

        if (!IsValidControllerId(ControllerId) ||
            FindInternal(ControllerId) != nullptr)
        {
            continue;
        }

        FTGControllerRecord Record;
        Record.ControllerId = ControllerId;
        Record.DisplayName = DisplayName;

        FString Status;
        JsonRecord->TryGetStringField(TEXT("status"), Status);
        Record.BuildStatus = StatusFromString(Status);

        JsonRecord->TryGetBoolField(
            TEXT("trusted"),
            Record.bTrusted);
        JsonRecord->TryGetBoolField(
            TEXT("hasEditableSource"),
            Record.bHasEditableSource);

        FString SourceModified;
        FString LastBuild;
        FString DllRelativePath;

        JsonRecord->TryGetStringField(
            TEXT("lastSourceModifiedUtc"),
            SourceModified);
        JsonRecord->TryGetStringField(
            TEXT("lastSuccessfulBuildUtc"),
            LastBuild);
        JsonRecord->TryGetStringField(
            TEXT("dllRelativePath"),
            DllRelativePath);
        JsonRecord->TryGetStringField(
            TEXT("diagnostics"),
            Record.LastBuildDiagnostics);

        Record.LastSourceModifiedUtc =
            DateTimeFromJson(SourceModified);
        Record.LastSuccessfulBuildUtc =
            DateTimeFromJson(LastBuild);

        const FString ControllerDirectory =
            GetControllerDirectory(ControllerId);

        if (Record.bHasEditableSource)
        {
            Record.ManagedSourceFilePath = NormalizeFilePath(
                FPaths::Combine(
                    ControllerDirectory,
                    ManagedSourceFileName));
        }

        if (!DllRelativePath.IsEmpty())
        {
            const FString CandidateDllPath = NormalizeFilePath(
                FPaths::Combine(
                    RootDirectory,
                    DllRelativePath));

            if (IsPathInsideDirectory(
                    CandidateDllPath,
                    ControllerDirectory))
            {
                Record.ManagedDllFilePath = CandidateDllPath;
            }
        }

        Controllers.Add(MoveTemp(Record));
    }

    OutError = FText::GetEmpty();
    return true;
}

bool UTGControllerLibrarySubsystem::SaveRegistry(
    FText& OutError) const
{
    if (!EnsureStorage(OutError))
    {
        return false;
    }

    const FString RootDirectory =
        GetControllerRootDirectory();
    const TSharedRef<FJsonObject> RootObject =
        MakeShared<FJsonObject>();

    RootObject->SetNumberField(
        TEXT("formatVersion"),
        RegistryFormatVersion);

    TArray<TSharedPtr<FJsonValue>> JsonControllers;
    JsonControllers.Reserve(Controllers.Num());

    for (const FTGControllerRecord& Record : Controllers)
    {
        const TSharedRef<FJsonObject> JsonRecord =
            MakeShared<FJsonObject>();

        JsonRecord->SetStringField(
            TEXT("id"),
            Record.ControllerId.ToString());
        JsonRecord->SetStringField(
            TEXT("displayName"),
            Record.DisplayName);
        JsonRecord->SetStringField(
            TEXT("status"),
            StatusToString(Record.BuildStatus));
        JsonRecord->SetBoolField(
            TEXT("trusted"),
            Record.bTrusted);
        JsonRecord->SetBoolField(
            TEXT("hasEditableSource"),
            Record.bHasEditableSource);
        JsonRecord->SetStringField(
            TEXT("lastSourceModifiedUtc"),
            DateTimeToJson(Record.LastSourceModifiedUtc));
        JsonRecord->SetStringField(
            TEXT("lastSuccessfulBuildUtc"),
            DateTimeToJson(Record.LastSuccessfulBuildUtc));
        JsonRecord->SetStringField(
            TEXT("diagnostics"),
            Record.LastBuildDiagnostics);

        FString RelativeDllPath;

        if (!Record.ManagedDllFilePath.IsEmpty() &&
            IsPathInsideDirectory(
                Record.ManagedDllFilePath,
                RootDirectory))
        {
            RelativeDllPath = Record.ManagedDllFilePath;
            FPaths::MakePathRelativeTo(
                RelativeDllPath,
                *(RootDirectory + TEXT("/")));
            FPaths::NormalizeFilename(RelativeDllPath);
        }

        JsonRecord->SetStringField(
            TEXT("dllRelativePath"),
            RelativeDllPath);

        JsonControllers.Add(
            MakeShared<FJsonValueObject>(JsonRecord));
    }

    RootObject->SetArrayField(
        TEXT("controllers"),
        MoveTemp(JsonControllers));

    FString JsonText;
    const TSharedRef<
        TJsonWriter<
            TCHAR,
            TPrettyJsonPrintPolicy<TCHAR>>> Writer =
        TJsonWriterFactory<
            TCHAR,
            TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonText);

    if (!FJsonSerializer::Serialize(RootObject, Writer) ||
        !FFileHelper::SaveStringToFile(
            JsonText,
            *GetRegistryFilePath(),
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FText::FromString(
            TEXT("The controller registry could not be saved."));
        return false;
    }

    OutError = FText::GetEmpty();
    return true;
}

bool UTGControllerLibrarySubsystem::ValidateDisplayName(
    const FString& DisplayName,
    const FName IgnoredControllerId,
    FString& OutTrimmedName,
    FText& OutError) const
{
    OutTrimmedName = DisplayName;
    OutTrimmedName.TrimStartAndEndInline();

    if (OutTrimmedName.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("Enter a controller name."));
        return false;
    }

    if (OutTrimmedName.Len() > 128)
    {
        OutError = FText::FromString(
            TEXT("Controller names must not exceed 128 characters."));
        return false;
    }

    for (const FTGControllerRecord& Existing : Controllers)
    {
        if (Existing.ControllerId != IgnoredControllerId &&
            Existing.DisplayName.Equals(
                OutTrimmedName,
                ESearchCase::IgnoreCase))
        {
            OutError = FText::FromString(
                TEXT("Another controller already uses this name."));
            return false;
        }
    }

    OutError = FText::GetEmpty();
    return true;
}

bool UTGControllerLibrarySubsystem::CreateControllerFromSourceText(
    const FString& DisplayName,
    const FString& Source,
    const bool bTrusted,
    FTGControllerRecord& OutController,
    FText& OutError)
{
    OutController = FTGControllerRecord{};
    FString TrimmedName;

    if (!ValidateDisplayName(
            DisplayName,
            NAME_None,
            TrimmedName,
            OutError))
    {
        return false;
    }

    const FTCHARToUTF8 Utf8Source(*Source);

    if (Utf8Source.Length() <= 0 ||
        Utf8Source.Length() > MaximumSourceBytes)
    {
        OutError = FText::FromString(
            TEXT("Controller source must be nonempty and no larger than 2 MiB."));
        return false;
    }

    if (!EnsureStorage(OutError))
    {
        return false;
    }

    const FName ControllerId = MakeControllerId();
    const FString ControllerDirectory =
        GetControllerDirectory(ControllerId);

    if (!IFileManager::Get().MakeDirectory(
            *ControllerDirectory,
            true))
    {
        OutError = FText::FromString(
            TEXT("The managed controller directory could not be created."));
        return false;
    }

    const FString ManagedSourcePath = NormalizeFilePath(
        FPaths::Combine(
            ControllerDirectory,
            ManagedSourceFileName));

    if (!FFileHelper::SaveStringToFile(
            Source,
            *ManagedSourcePath,
            FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        OutError = FText::FromString(
            TEXT("The managed controller source could not be created."));
        return false;
    }

    FTGControllerRecord Record;
    Record.ControllerId = ControllerId;
    Record.DisplayName = MoveTemp(TrimmedName);
    Record.BuildStatus = ETGControllerBuildStatus::NotBuilt;
    Record.bTrusted = bTrusted;
    Record.bHasEditableSource = true;
    Record.LastSourceModifiedUtc = FDateTime::UtcNow();
    Record.ManagedSourceFilePath = ManagedSourcePath;
    Record.LastBuildDiagnostics =
        TEXT("Source is ready. Build the controller before selecting it.");

    Controllers.Add(Record);

    if (!SaveRegistry(OutError))
    {
        Controllers.Pop();
        return false;
    }

    OutController = Record;
    BroadcastLibraryChanged();
    return true;
}

FTGControllerRecord*
UTGControllerLibrarySubsystem::FindMutable(
    const FName ControllerId)
{
    return Controllers.FindByPredicate(
        [ControllerId](const FTGControllerRecord& Record)
        {
            return Record.ControllerId == ControllerId;
        });
}

const FTGControllerRecord*
UTGControllerLibrarySubsystem::FindInternal(
    const FName ControllerId) const
{
    return Controllers.FindByPredicate(
        [ControllerId](const FTGControllerRecord& Record)
        {
            return Record.ControllerId == ControllerId;
        });
}

void UTGControllerLibrarySubsystem::BroadcastLibraryChanged()
{
    OnControllerLibraryChanged.Broadcast();
}
