// Copyright (c) 2026 Breno Raiher.

using UnrealBuildTool;
using System.Collections.Generic;
using System.IO;

public class PHAROSTarget : TargetRules
{
    public PHAROSTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        BuildVersion = "1.0";
        if (Platform == UnrealTargetPlatform.Win64 &&
            Configuration == UnrealTargetConfiguration.Shipping &&
            ProjectFile != null)
        {
            // Remap diagnostic source paths without changing the engine ABI.
            bOverrideBuildEnvironment = true;
            string ProjectRoot = Path.GetDirectoryName(ProjectFile.FullName)!;
            AdditionalCompilerArguments = $"/pathmap:\"{ProjectRoot}=PHAROS\"";
        }
        ExtraModuleNames.Add("TGSimCore");
        ExtraModuleNames.Add("TG");
    }
}
