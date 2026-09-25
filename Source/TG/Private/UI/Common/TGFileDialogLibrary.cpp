// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGFileDialogLibrary.h"

#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <commdlg.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
    FString NormalizeFileExtension(FString Extension)
    {
        Extension.TrimStartAndEndInline();

        if (Extension.StartsWith(TEXT("*.")))
        {
            Extension = Extension.RightChop(2);
        }
        else if (Extension.StartsWith(TEXT(".")))
        {
            Extension = Extension.RightChop(1);
        }
        else if (Extension.StartsWith(TEXT("*")))
        {
            Extension = Extension.RightChop(1);
        }

        Extension.TrimStartAndEndInline();
        Extension.ToLowerInline();

        return Extension;
    }

#if PLATFORM_WINDOWS

    void AppendNullTerminatedString(
        TArray<TCHAR>& Destination,
        const FString& Value)
    {
        Destination.Append(*Value, Value.Len());
        Destination.Add(TEXT('\0'));
    }

    void AppendFilterEntry(
        TArray<TCHAR>& FilterBuffer,
        const FString& Description,
        const FString& Pattern)
    {
        AppendNullTerminatedString(FilterBuffer, Description);
        AppendNullTerminatedString(FilterBuffer, Pattern);
    }

#endif
}

bool UTGFileDialogLibrary::OpenSingleFileDialog(
    const FString& DialogTitle,
    const FString& DefaultDirectory,
    const FString& DefaultFileName,
    const FString& FileTypeDescription,
    const TArray<FString>& AllowedExtensions,
    const bool bIncludeAllFiles,
    FString& SelectedFilePath)
{
    SelectedFilePath.Reset();

#if !PLATFORM_WINDOWS

    return false;

#else

    /*
     * Normalize the extensions once so callers may provide:
     *
     * csv
     * .csv
     * *.csv
     */
    TArray<FString> NormalizedExtensions;

    for (const FString& RawExtension : AllowedExtensions)
    {
        const FString NormalizedExtension =
            NormalizeFileExtension(RawExtension);

        if (!NormalizedExtension.IsEmpty())
        {
            NormalizedExtensions.AddUnique(NormalizedExtension);
        }
    }

    /*
     * Build a Windows filter pattern such as:
     *
     * *.csv
     *
     * or:
     *
     * *.txt;*.dat
     */
    FString SupportedPattern = TEXT("*.*");

    if (!NormalizedExtensions.IsEmpty())
    {
        TArray<FString> IndividualPatterns;
        IndividualPatterns.Reserve(NormalizedExtensions.Num());

        for (const FString& Extension : NormalizedExtensions)
        {
            IndividualPatterns.Add(
                FString::Printf(TEXT("*.%s"), *Extension));
        }

        SupportedPattern =
            FString::Join(IndividualPatterns, TEXT(";"));
    }

    FString EffectiveDescription = FileTypeDescription;
    EffectiveDescription.TrimStartAndEndInline();

    if (EffectiveDescription.IsEmpty())
    {
        EffectiveDescription = TEXT("Supported files");
    }

    const FString SupportedFilterLabel = FString::Printf(
        TEXT("%s (%s)"),
        *EffectiveDescription,
        *SupportedPattern);

    /*
     * OPENFILENAME expects pairs of null-terminated strings:
     *
     * Description\0Pattern\0
     *
     * The complete filter list ends with one additional null character.
     */
    TArray<TCHAR> FilterBuffer;

    AppendFilterEntry(
        FilterBuffer,
        SupportedFilterLabel,
        SupportedPattern);

    if (bIncludeAllFiles && SupportedPattern != TEXT("*.*"))
    {
        AppendFilterEntry(
            FilterBuffer,
            TEXT("All files (*.*)"),
            TEXT("*.*"));
    }

    FilterBuffer.Add(TEXT('\0'));

    /*
     * Large buffer to support long Windows paths.
     */
    constexpr int32 FileBufferCharacterCount = 32768;

    TArray<TCHAR> FileBuffer;
    FileBuffer.SetNumZeroed(FileBufferCharacterCount);

    if (!DefaultFileName.IsEmpty())
    {
        FCString::Strncpy(
            FileBuffer.GetData(),
            *DefaultFileName,
            FileBuffer.Num());
    }

    FString InitialDirectory;

    if (!DefaultDirectory.IsEmpty())
    {
        InitialDirectory =
            FPaths::ConvertRelativePathToFull(DefaultDirectory);

        FPaths::NormalizeDirectoryName(InitialDirectory);

        if (!IFileManager::Get().DirectoryExists(*InitialDirectory))
        {
            InitialDirectory.Reset();
        }
    }

    HWND ParentWindowHandle = nullptr;
    const bool bSlateIsAvailable = FSlateApplication::IsInitialized();

    if (bSlateIsAvailable)
    {
        FSlateApplication& SlateApplication =
            FSlateApplication::Get();

        const void* UnrealParentWindowHandle =
            SlateApplication.FindBestParentWindowHandleForDialogs(
                TSharedPtr<SWidget>(),
                ESlateParentWindowSearchMethod::ActiveWindow);

        ParentWindowHandle = reinterpret_cast<HWND>(
            const_cast<void*>(UnrealParentWindowHandle));

        SlateApplication.ExternalModalStart();
    }

    OPENFILENAMEW DialogConfiguration{};
    DialogConfiguration.lStructSize = sizeof(OPENFILENAMEW);
    DialogConfiguration.hwndOwner = ParentWindowHandle;
    DialogConfiguration.lpstrFilter = FilterBuffer.GetData();
    DialogConfiguration.lpstrFile = FileBuffer.GetData();
    DialogConfiguration.nMaxFile =
        static_cast<DWORD>(FileBuffer.Num());

    DialogConfiguration.lpstrInitialDir =
        InitialDirectory.IsEmpty()
            ? nullptr
            : *InitialDirectory;

    DialogConfiguration.lpstrTitle =
        DialogTitle.IsEmpty()
            ? nullptr
            : *DialogTitle;

    DialogConfiguration.lpstrDefExt =
        NormalizedExtensions.IsEmpty()
            ? nullptr
            : *NormalizedExtensions[0];

    DialogConfiguration.Flags =
        OFN_EXPLORER |
        OFN_FILEMUSTEXIST |
        OFN_PATHMUSTEXIST |
        OFN_NOCHANGEDIR |
        OFN_HIDEREADONLY;

    const BOOL bFileWasSelected =
        GetOpenFileNameW(&DialogConfiguration);

    if (bSlateIsAvailable)
    {
        FSlateApplication::Get().ExternalModalStop();
    }

    if (!bFileWasSelected)
    {
        return false;
    }

    FString CandidateFilePath(FileBuffer.GetData());

    CandidateFilePath =
        FPaths::ConvertRelativePathToFull(CandidateFilePath);

    FPaths::NormalizeFilename(CandidateFilePath);

    if (!IFileManager::Get().FileExists(*CandidateFilePath))
    {
        return false;
    }

    /*
     * The Windows filter controls what the dialog displays, but we also
     * validate the returned extension before accepting the path.
     */
    if (!NormalizedExtensions.IsEmpty())
    {
        FString CandidateExtension =
            FPaths::GetExtension(CandidateFilePath, false);

        CandidateExtension.ToLowerInline();

        if (!NormalizedExtensions.Contains(CandidateExtension))
        {
            return false;
        }
    }

    SelectedFilePath = MoveTemp(CandidateFilePath);
    return true;

#endif
}
