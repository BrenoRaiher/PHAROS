using System.IO;
using UnrealBuildTool;

public class TGSimCore : ModuleRules
{
    public TGSimCore(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "Boost",
            "Eigen",
            "nanoflann"
        });

        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

        // ScenarioFile.cpp uses the vendored, header-only toml++ parser. Keep
        // this include private to the module so TGSimCore's public data types do
        // not expose TOML as part of their API.
        PrivateIncludePaths.Add(Path.Combine(
            ModuleDirectory,
            "..",
            "ThirdParty",
            "tomlplusplus"));

        ExternalDependencies.Add(Path.Combine(
            ModuleDirectory,
            "..",
            "ThirdParty",
            "tomlplusplus",
            "toml.hpp"));
        ExternalDependencies.Add(Path.Combine(
            ModuleDirectory,
            "..",
            "ThirdParty",
            "tomlplusplus",
            "LICENSE"));

        // tests/TGSimCoreValidationMain.cpp is guarded by
        // TGSIM_STANDALONE_VALIDATION, so UBT displays it but does not link main().
    }
}
