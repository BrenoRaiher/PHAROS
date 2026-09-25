// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class TG : ModuleRules
{
    public TG(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"UMG",
			"Slate",
            "SlateCore",
            "TGSimCore",
            "ProceduralMeshComponent"
		});

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Json",
            "MoviePlayer",
            "RenderCore"
        });

        if (Target.bBuildEditor)
        {
            // Editor-only Blueprint inspection and automation support.
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "BlueprintGraph",
                "UnrealEd",
                "UMGEditor"
            });
        }

        

        string SourcePath = Path.GetFullPath(
            Path.Combine(ModuleDirectory, ".."));

        string ProjectRoot = Path.GetFullPath(
            Path.Combine(ModuleDirectory, "..", ".."));

        string LegalDocumentsPath = Path.Combine(
            ProjectRoot,
            "Docs",
            "Legal");

        string[] LegalSourceFiles =
        {
            Path.Combine(LegalDocumentsPath, "EULA.txt"),
            Path.Combine(LegalDocumentsPath, "THIRD_PARTY_NOTICES.txt"),
            Path.Combine(ProjectRoot, "LICENSE")
        };

        string[] LegalOutputNames =
        {
            "EULA.txt",
            "THIRD_PARTY_NOTICES.txt",
            "PHAROS_LICENSE.txt"
        };

        for (int LegalFileIndex = 0;
             LegalFileIndex < LegalSourceFiles.Length;
             ++LegalFileIndex)
        {
            string LegalSourceFile = LegalSourceFiles[LegalFileIndex];
            if (!File.Exists(LegalSourceFile))
            {
                throw new BuildException(
                    $"Required PHAROS legal file is missing: {LegalSourceFile}");
            }

            ExternalDependencies.Add(LegalSourceFile);
            RuntimeDependencies.Add(
                Path.Combine(
                    "$(TargetOutputDir)",
                    "Legal",
                    LegalOutputNames[LegalFileIndex]),
                LegalSourceFile,
                StagedFileType.NonUFS);
        }

        string LegalLicensesPath = Path.Combine(
            LegalDocumentsPath,
            "Licenses");
        if (!Directory.Exists(LegalLicensesPath))
        {
            throw new BuildException(
                $"Required PHAROS license directory is missing: {LegalLicensesPath}");
        }

        foreach (string LicenseFile in Directory.GetFiles(
            LegalLicensesPath,
            "*",
            SearchOption.AllDirectories))
        {
            string RelativeLicenseFile = Path.GetRelativePath(
                LegalLicensesPath,
                LicenseFile);
            ExternalDependencies.Add(LicenseFile);
            RuntimeDependencies.Add(
                Path.Combine(
                    "$(TargetOutputDir)",
                    "Legal",
                    "Licenses",
                    RelativeLicenseFile),
                LicenseFile,
                StagedFileType.NonUFS);
        }

        string LegalSourcesPath = Path.Combine(LegalDocumentsPath, "Sources");
        if (!File.Exists(Path.Combine(LegalSourcesPath, "Eigen-3.4.0.zip")))
        {
            throw new BuildException("Required Eigen source bundle is missing.");
        }
        foreach (string SourceFile in Directory.GetFiles(
            LegalSourcesPath, "*", SearchOption.AllDirectories))
        {
            ExternalDependencies.Add(SourceFile);
            RuntimeDependencies.Add(
                Path.Combine("$(TargetOutputDir)", "Legal", "Sources",
                    Path.GetRelativePath(LegalSourcesPath, SourceFile)),
                SourceFile, StagedFileType.NonUFS);
        }

        string ReleaseDocumentsPath = Path.Combine(
            ProjectRoot,
            "Docs",
            "Release");
        string[] ReleaseSourceFiles =
        {
            Path.Combine(ReleaseDocumentsPath, "README.md"),
            Path.Combine(ReleaseDocumentsPath, "CHANGELOG.md"),
            Path.Combine(ReleaseDocumentsPath, "RELEASE_NOTES.md"),
            Path.Combine(ReleaseDocumentsPath, "COMPATIBILITY.md"),
            Path.Combine(ReleaseDocumentsPath, "KNOWN_LIMITATIONS.md"),
            Path.Combine(ReleaseDocumentsPath, "SUPPORT.md"),
            Path.Combine(ReleaseDocumentsPath, "PRIVACY.md")
        };
        string[] ReleaseOutputNames =
        {
            "README.md",
            "CHANGELOG.md",
            "RELEASE_NOTES.md",
            "COMPATIBILITY.md",
            "KNOWN_LIMITATIONS.md",
            "SUPPORT.md",
            "PRIVACY.md"
        };

        for (int ReleaseFileIndex = 0;
             ReleaseFileIndex < ReleaseSourceFiles.Length;
             ++ReleaseFileIndex)
        {
            string ReleaseSourceFile =
                ReleaseSourceFiles[ReleaseFileIndex];
            if (!File.Exists(ReleaseSourceFile))
            {
                throw new BuildException(
                    $"Required PHAROS release document is missing: {ReleaseSourceFile}");
            }

            ExternalDependencies.Add(ReleaseSourceFile);
            RuntimeDependencies.Add(
                Path.Combine(
                    "$(TargetOutputDir)",
                    "Documentation",
                    ReleaseOutputNames[ReleaseFileIndex]),
                ReleaseSourceFile,
                StagedFileType.NonUFS);
        }

        string ControllerSdkPath = Path.Combine(
            ProjectRoot,
            "ControllerSDK");

        string ScenarioRunnerBuildScript = Path.Combine(
            ProjectRoot,
            "Tools",
            "TGScenarioRunner",
            "BuildStandaloneRunner.bat");

        ExternalDependencies.Add(ScenarioRunnerBuildScript);

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string ScenarioRunnerExecutable = Path.Combine(
                ProjectRoot,
                "Tools",
                "TGScenarioRunner",
                "bin",
                "PHAROSScenarioRunner.exe");

            // BuildStandaloneRunner.bat creates this generic helper. Once it
            // exists, UBT stages the same binary beside the packaged app.
            if (File.Exists(ScenarioRunnerExecutable))
            {
                RuntimeDependencies.Add(
                    Path.Combine(
                        "$(TargetOutputDir)",
                        "PHAROSScenarioRunner.exe"),
                    ScenarioRunnerExecutable,
                    StagedFileType.NonUFS);
            }
        }

        PrivateIncludePaths.Add(ControllerSdkPath);

        string[] ControllerSdkFiles =
        {
            "PHAROSControllerAPI.h",
            "TGControllerAPI.h",
            "ControllerTemplate.cpp",
            "README.md"
        };

        foreach (string ControllerSdkFile in ControllerSdkFiles)
        {
            // The SDK lives outside Source so user controllers can consume it
            // without depending on Unreal. Register it explicitly so UBT and
            // generated IDE projects still track changes to the public contract.
            ExternalDependencies.Add(Path.Combine(
                ControllerSdkPath,
                ControllerSdkFile));

            RuntimeDependencies.Add(
                Path.Combine(
                    "$(TargetOutputDir)",
                    "ControllerSDK",
                    ControllerSdkFile),
                Path.Combine(
                    ControllerSdkPath,
                    ControllerSdkFile),
                StagedFileType.NonUFS);
        }

        string CSPICEPath = Path.Combine(
            SourcePath,
            "ThirdParty",
            "CSPICE");

        PublicSystemIncludePaths.Add(
            Path.Combine(CSPICEPath, "Include"));

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicAdditionalLibraries.Add(
                Path.Combine(
                    CSPICEPath,
                    "Lib",
                    "Win64",
                    "cspice.lib"));

            // Required by CSPICE/libf2c old C runtime names.
            // These are Windows toolchain libraries, so UBT should resolve them
            // through the platform system-library paths rather than track files.
            PublicSystemLibraries.Add("oldnames.lib");
            PublicSystemLibraries.Add("legacy_stdio_definitions.lib");

            // Required by the reusable native Windows file-selection dialog.
            PublicSystemLibraries.Add("Comdlg32.lib");

            // A release package may vendor LLVM-MinGW here. Iterating only
            // when the directory exists keeps source/editor builds lightweight
            // while staging the complete self-contained compiler for shipping.
            string ControllerToolchainPath = Path.Combine(
                ProjectRoot,
                "Build",
                "ControllerToolchain",
                "Win64");

            if (Directory.Exists(ControllerToolchainPath))
            {
                foreach (string ToolchainFile in Directory.GetFiles(
                    ControllerToolchainPath,
                    "*",
                    SearchOption.AllDirectories))
                {
                    string RelativeToolchainFile = Path.GetRelativePath(
                        ControllerToolchainPath,
                        ToolchainFile);

                    RuntimeDependencies.Add(
                        Path.Combine(
                            "$(TargetOutputDir)",
                            "ControllerToolchain",
                            RelativeToolchainFile),
                        ToolchainFile,
                        StagedFileType.NonUFS);
                }
            }
        }

        string[] SpiceKernelFiles =
        {
            "naif0012.tls",
            "pck00011.tpc",
            "gm_de440.tpc",
            "codes_300ast_20100725.tf",
            "de442.bsp",
            "mar099s.bsp",
            "jup230-short.bsp",
            "jup348.bsp",
            "sat252s.bsp",
            "ura111.bsp",
            "nep076.bsp",
            "plu058.bsp",
            "codes_300ast_20100725.bsp"
        };

        foreach (string KernelFile in SpiceKernelFiles)
        {
            RuntimeDependencies.Add(
                Path.Combine(
                    ProjectRoot,
                    "Content",
                    "SPICEKernels",
                    KernelFile));
        }
    }
}
