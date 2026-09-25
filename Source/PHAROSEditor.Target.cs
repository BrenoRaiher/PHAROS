// Copyright (c) 2026 Breno Raiher.

using UnrealBuildTool;
using System.Collections.Generic;

public class PHAROSEditorTarget : TargetRules
{
    public PHAROSEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("TGSimCore");
        ExtraModuleNames.Add("TG");
    }
}
