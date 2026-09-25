// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/TGScenarioFileLibrary.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Simulation/Control/TGControllerLibrarySubsystem.h"
#include "Simulation/TGScenarioDocumentAdapter.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "TGSim/Scenario/ScenarioDiagnostics.h"
#include "TGSim/Scenario/ScenarioFile.h"
#include "UI/Common/TGFileDialogLibrary.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"

#include <filesystem>

namespace
{
    std::string TgscnFileToUtf8(const FString& value)
    {
        return std::string(TCHAR_TO_UTF8(*value));
    }

    FString TgscnFileFromUtf8(const std::string& value)
    {
        return FString(UTF8_TO_TCHAR(value.c_str()));
    }

    UTGControllerLibrarySubsystem* ResolveControllerLibrary(
        const UObject* WorldContextObject)
    {
        const UWorld* World = WorldContextObject != nullptr
            ? WorldContextObject->GetWorld()
            : nullptr;
        UGameInstance* GameInstance = World != nullptr
            ? World->GetGameInstance()
            : nullptr;
        return GameInstance != nullptr
            ? GameInstance->GetSubsystem<UTGControllerLibrarySubsystem>()
            : nullptr;
    }

    std::filesystem::path ToPath(const FString& value)
    {
#if PLATFORM_WINDOWS
        return std::filesystem::path(std::wstring(*value));
#else
        return std::filesystem::u8path(TgscnFileToUtf8(value));
#endif
    }
}

bool UTGScenarioFileLibrary::ExportScenarioToTgscn(
    const UObject* WorldContextObject,
    const FTGSimulationScenario& Scenario,
    const FString& FilePath,
    FText& OutMessage)
{
    OutMessage = FText::GetEmpty();
    FString normalized_path = FilePath.TrimStartAndEnd();
    if (normalized_path.IsEmpty())
    {
        OutMessage = FText::FromString(TEXT("Choose a .tgscn output file."));
        return false;
    }
    if (!FPaths::GetExtension(normalized_path, true).Equals(
            TEXT(".tgscn"), ESearchCase::IgnoreCase))
        normalized_path += TEXT(".tgscn");

    FTGSimulationScenario PreparedScenario = Scenario;
    FText SrpSummary;
    FText SrpWarning;
    FText SrpError;
    if (!UTGSolarRadiationPressureEditingLibrary::
            PrepareSolarRadiationPressureGeometry(
                PreparedScenario,
                SrpSummary,
                SrpWarning,
                SrpError))
    {
        OutMessage = SrpError.IsEmpty()
            ? FText::FromString(
                TEXT("Solar-radiation-pressure geometry could not be prepared."))
            : SrpError;
        return false;
    }

    FString controller_dll_path;
    if (PreparedScenario.Control.Mode == ETGControlMode::CompiledUserController)
    {
        if (!PreparedScenario.Control.ControllerId.IsNone())
        {
            UTGControllerLibrarySubsystem* library =
                ResolveControllerLibrary(WorldContextObject);
            FString error;
            if (library == nullptr ||
                !library->ResolveReadyControllerDllPath(
                    PreparedScenario.Control.ControllerId, controller_dll_path, error))
            {
                OutMessage = FText::FromString(
                    error.IsEmpty()
                        ? TEXT("The selected controller is not ready for export.")
                        : error);
                return false;
            }
        }
        else
        {
            controller_dll_path =
                PreparedScenario.Control.StandaloneControllerDllFilePath;
            if (controller_dll_path.IsEmpty())
            {
                OutMessage = FText::FromString(
                    TEXT("The compiled-controller scenario has no controller DLL to export."));
                return false;
            }
        }
    }

    tgsim::scenario::ScenarioDocument document;
    FTGScenarioDocumentAdapter::ToPortableDocument(
        PreparedScenario, controller_dll_path, document);
    tgsim::scenario::Diagnostics diagnostics;
    if (!tgsim::scenario::SaveScenarioFile(
            ToPath(normalized_path), document, diagnostics))
    {
        OutMessage = FText::FromString(TgscnFileFromUtf8(
            tgsim::scenario::FormatDiagnostics(diagnostics)));
        return false;
    }
    FString Message = FString::Printf(
        TEXT("Scenario exported to %s"),
        *normalized_path);
    if (!SrpWarning.IsEmpty())
    {
        Message += TEXT("\n") + SrpWarning.ToString();
    }
    OutMessage = FText::FromString(Message);
    return true;
}

bool UTGScenarioFileLibrary::ImportScenarioFromTgscn(
    const UObject* WorldContextObject,
    const FString& FilePath,
    FTGSimulationScenario& OutScenario,
    FText& OutMessage)
{
    (void)WorldContextObject;
    OutScenario = FTGSimulationScenario{};
    OutMessage = FText::GetEmpty();
    const FString normalized_path = FilePath.TrimStartAndEnd();
    if (normalized_path.IsEmpty())
    {
        OutMessage = FText::FromString(TEXT("Choose a .tgscn input file."));
        return false;
    }

    tgsim::scenario::ScenarioDocument document;
    tgsim::scenario::Diagnostics diagnostics;
    if (!tgsim::scenario::LoadScenarioFile(
            ToPath(normalized_path), document, diagnostics))
    {
        OutMessage = FText::FromString(TgscnFileFromUtf8(
            tgsim::scenario::FormatDiagnostics(diagnostics)));
        return false;
    }

    FString message;
    if (!FTGScenarioDocumentAdapter::FromPortableDocument(
            document, normalized_path, OutScenario, message))
    {
        OutMessage = FText::FromString(message);
        return false;
    }
    UTGSolarRadiationPressureEditingLibrary::
        NormalizeSolarRadiationPressureScenario(OutScenario);
    OutMessage = FText::FromString(message);
    return true;
}

bool UTGScenarioFileLibrary::ChooseAndImportTgscnAsCurrentDraft(
    const UObject* WorldContextObject,
    FString& OutSelectedFilePath,
    FText& OutMessage)
{
    OutSelectedFilePath.Reset();
    OutMessage = FText::GetEmpty();

    if (!UTGFileDialogLibrary::OpenSingleFileDialog(
            TEXT("Import PHAROS Scenario"),
            FPaths::ProjectSavedDir(),
            FString{},
            TEXT("PHAROS scenario"),
            {TEXT("tgscn")},
            false,
            OutSelectedFilePath))
    {
        OutMessage = FText::FromString(
            TEXT("No PHAROS scenario file was selected."));
        return false;
    }

    FTGSimulationScenario ImportedScenario;
    if (!ImportScenarioFromTgscn(
            WorldContextObject,
            OutSelectedFilePath,
            ImportedScenario,
            OutMessage))
    {
        return false;
    }

    UWorld* World = WorldContextObject != nullptr
        ? WorldContextObject->GetWorld()
        : nullptr;
    UGameInstance* GameInstance = World != nullptr
        ? World->GetGameInstance()
        : nullptr;
    UTGSimulationSubsystem* ScenarioSubsystem = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
    if (ScenarioSubsystem == nullptr)
    {
        OutMessage = FText::FromString(
            TEXT("The simulation scenario subsystem is unavailable."));
        return false;
    }

    ScenarioSubsystem->SetImportedScenarioDraft(ImportedScenario);
    return true;
}
