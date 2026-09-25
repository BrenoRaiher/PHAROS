// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/MainMenu/TGLegalDocumentUtils.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    void AddCandidate(
        TArray<FString>& Candidates,
        FString Candidate)
    {
        Candidate = FPaths::ConvertRelativePathToFull(Candidate);
        FPaths::CollapseRelativeDirectories(Candidate);
        FPaths::NormalizeFilename(Candidate);
        Candidates.AddUnique(MoveTemp(Candidate));
    }
}

bool TGLegalDocumentUtils::ResolvePath(
    const FString& FileName,
    FString& OutPath)
{
    OutPath.Reset();
    if (FileName.IsEmpty() ||
        FileName.Contains(TEXT("..")) ||
        !FPaths::IsRelative(FileName))
    {
        return false;
    }

    TArray<FString> Candidates;
    Candidates.Reserve(4);

    const FString BaseDirectory = FPlatformProcess::BaseDir();
    AddCandidate(
        Candidates,
        FPaths::Combine(BaseDirectory, TEXT("Legal"), FileName));
    AddCandidate(
        Candidates,
        FPaths::Combine(
            BaseDirectory,
            TEXT(".."),
            TEXT(".."),
            TEXT(".."),
            FileName));
    AddCandidate(
        Candidates,
        FPaths::Combine(FPaths::LaunchDir(), FileName));
    AddCandidate(
        Candidates,
        FPaths::Combine(
            FPaths::ProjectDir(),
            TEXT("Docs"),
            TEXT("Legal"),
            FileName));

    for (const FString& Candidate : Candidates)
    {
        if (IFileManager::Get().FileExists(*Candidate))
        {
            OutPath = Candidate;
            return true;
        }
    }

    return false;
}

bool TGLegalDocumentUtils::LoadText(
    const FString& FileName,
    FString& OutText,
    FString& OutPath)
{
    OutText.Reset();
    return ResolvePath(FileName, OutPath) &&
        FFileHelper::LoadFileToString(OutText, *OutPath);
}

bool TGLegalDocumentUtils::Open(const FString& FileName)
{
    FString Path;
    if (!ResolvePath(FileName, Path))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PHAROS: Legal document is unavailable: %s"),
            *FileName);
        return false;
    }

    if (!FPlatformProcess::LaunchFileInDefaultExternalApplication(*Path))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("PHAROS: Could not open legal document: %s"),
            *Path);
        return false;
    }

    return true;
}
