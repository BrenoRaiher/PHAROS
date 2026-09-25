// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Blueprint/WidgetTree.h"
#include "Components/ComboBoxString.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"

namespace TGComboBoxAssetStyleTests
{
    struct FComboBoxTarget
    {
        const TCHAR* ObjectPath;
        const TCHAR* WidgetName;
    };

    constexpr const TCHAR* AtmosphereObjectPath =
        TEXT("/Game/UI/Configuration/WBP_Config_Atmosphere.WBP_Config_Atmosphere");
    constexpr const TCHAR* AtmosphereReferenceName =
        TEXT("COMBO_AtmosphereCentralBody");

    constexpr const TCHAR* AssetPaths[] =
    {
        TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
        TEXT("/Game/UI/Configuration/WBP_Config_Aerodynamics.WBP_Config_Aerodynamics"),
        AtmosphereObjectPath,
        TEXT("/Game/UI/Configuration/WBP_Config_Controls.WBP_Config_Controls"),
        TEXT("/Game/UI/Configuration/WBP_Config_InitialStateFrame.WBP_Config_InitialStateFrame"),
        TEXT("/Game/UI/Configuration/WBP_Config_ScenarioSolver.WBP_Config_ScenarioSolver"),
        TEXT("/Game/UI/Configuration/WBP_Config_SolarRadiationPressure.WBP_Config_SolarRadiationPressure"),
        TEXT("/Game/UI/Configuration/ComponentTree/WBP_ComponentDegreeOfFreedomRow.WBP_ComponentDegreeOfFreedomRow"),
        TEXT("/Game/UI/Configuration/ComponentTree/WBP_ComponentInspector.WBP_ComponentInspector")
    };

    constexpr FComboBoxTarget Targets[] =
    {
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_ThrusterMode")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_ThrusterMountComponent")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_ThrusterPropellantComponent")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_ThrusterIgnitionTimeMode")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_ThrusterShutdownTimeMode")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_ThrusterThrustSource")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_ThrusterIspSource")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Actuators.WBP_Config_Actuators"),
            TEXT("COMBO_WheelMountComponent")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Aerodynamics.WBP_Config_Aerodynamics"),
            TEXT("COMBO_DatabaseInterpolation")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Aerodynamics.WBP_Config_Aerodynamics"),
            TEXT("COMBO_DatabaseExtrapolation")
        },
        {
            AtmosphereObjectPath,
            TEXT("COMBO_AtmosphereCentralBody")
        },
        {
            AtmosphereObjectPath,
            TEXT("COMBO_AtmosphereModel")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Controls.WBP_Config_Controls"),
            TEXT("COMBO_ControlMode")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_Controls.WBP_Config_Controls"),
            TEXT("COMBO_ScenarioController")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_InitialStateFrame.WBP_Config_InitialStateFrame"),
            TEXT("COMBO_ReferenceFrame")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_ScenarioSolver.WBP_Config_ScenarioSolver"),
            TEXT("INPUT_IntegratorKind")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_ScenarioSolver.WBP_Config_ScenarioSolver"),
            TEXT("INPUT_OutputMode")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_ScenarioSolver.WBP_Config_ScenarioSolver"),
            TEXT("INPUT_EndMode")
        },
        {
            TEXT("/Game/UI/Configuration/WBP_Config_SolarRadiationPressure.WBP_Config_SolarRadiationPressure"),
            TEXT("COMBO_ProxyResolutionMode")
        },
        {
            TEXT("/Game/UI/Configuration/ComponentTree/WBP_ComponentDegreeOfFreedomRow.WBP_ComponentDegreeOfFreedomRow"),
            TEXT("INPUT_DegreeOfFreedomType")
        },
        {
            TEXT("/Game/UI/Configuration/ComponentTree/WBP_ComponentInspector.WBP_ComponentInspector"),
            TEXT("COMBO_GeometrySource")
        },
        {
            TEXT("/Game/UI/Configuration/ComponentTree/WBP_ComponentInspector.WBP_ComponentInspector"),
            TEXT("COMBO_PrimitiveType")
        },
        {
            TEXT("/Game/UI/Configuration/ComponentTree/WBP_ComponentInspector.WBP_ComponentInspector"),
            TEXT("COMBO_StlLengthUnit")
        },
        {
            TEXT("/Game/UI/Configuration/ComponentTree/WBP_ComponentInspector.WBP_ComponentInspector"),
            TEXT("COMBO_SurfaceAppearance")
        }
    };

    bool MatchesObjectPath(
        const FComboBoxTarget& Target,
        const TCHAR* ObjectPath)
    {
        return FCString::Strcmp(Target.ObjectPath, ObjectPath) == 0;
    }

    void CompareStyle(
        const UComboBoxString& Actual,
        const UComboBoxString& Reference,
        const FString& Context,
        FAutomationTestBase& Test)
    {
        if (!FComboBoxStyle::StaticStruct()->CompareScriptStruct(
                &Actual.GetWidgetStyle(),
                &Reference.GetWidgetStyle(),
                0))
        {
            Test.AddError(Context + TEXT(": WidgetStyle differs from Atmosphere."));
        }
        if (!FTableRowStyle::StaticStruct()->CompareScriptStruct(
                &Actual.GetItemStyle(),
                &Reference.GetItemStyle(),
                0))
        {
            Test.AddError(Context + TEXT(": ItemStyle differs from Atmosphere."));
        }
        if (!FScrollBarStyle::StaticStruct()->CompareScriptStruct(
                &Actual.GetScrollBarStyle(),
                &Reference.GetScrollBarStyle(),
                0))
        {
            Test.AddError(Context + TEXT(": ScrollBarStyle differs from Atmosphere."));
        }
        if (Actual.GetContentPadding() != Reference.GetContentPadding())
        {
            Test.AddError(Context + TEXT(": ContentPadding differs from Atmosphere."));
        }
        if (Actual.GetMaxListHeight() != Reference.GetMaxListHeight())
        {
            Test.AddError(Context + TEXT(": MaxListHeight differs from Atmosphere."));
        }
        if (Actual.GetFont() != Reference.GetFont())
        {
            Test.AddError(Context + TEXT(": Font differs from Atmosphere."));
        }
        if (Actual.GetForegroundColor() != Reference.GetForegroundColor())
        {
            Test.AddError(Context + TEXT(": ForegroundColor differs from Atmosphere."));
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGComboBoxAssetStyleTest,
    "TG.UI.Theme.ComboBoxAssetParity",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGComboBoxAssetStyleTest::RunTest(const FString& Parameters)
{
    using namespace TGComboBoxAssetStyleTests;
    (void) Parameters;

    UWidgetBlueprint* AtmosphereBlueprint =
        LoadObject<UWidgetBlueprint>(nullptr, AtmosphereObjectPath);
    if (AtmosphereBlueprint == nullptr ||
        AtmosphereBlueprint->WidgetTree == nullptr)
    {
        AddError(TEXT("Could not load the Atmosphere reference Widget Blueprint."));
        return false;
    }

    const UComboBoxString* AtmosphereReference =
        Cast<UComboBoxString>(
            AtmosphereBlueprint->WidgetTree->FindWidget(
                FName(AtmosphereReferenceName)));
    if (AtmosphereReference == nullptr)
    {
        AddError(TEXT("Could not find the Atmosphere reference ComboBox."));
        return false;
    }

    int32 CheckedComboBoxCount = 0;
    for (const TCHAR* ObjectPath : AssetPaths)
    {
        UWidgetBlueprint* Blueprint =
            LoadObject<UWidgetBlueprint>(nullptr, ObjectPath);
        if (Blueprint == nullptr || Blueprint->WidgetTree == nullptr ||
            Blueprint->GetOutermost() == nullptr)
        {
            AddError(FString::Printf(
                TEXT("Could not load ComboBox asset: %s"),
                ObjectPath));
            continue;
        }

        UPackage* Package = Blueprint->GetOutermost();
        const bool bPackageWasDirty = Package->IsDirty();

        TSet<FName> ExpectedNames;
        for (const FComboBoxTarget& Target : Targets)
        {
            if (MatchesObjectPath(Target, ObjectPath))
            {
                ExpectedNames.Add(FName(Target.WidgetName));
            }
        }

        TArray<UWidget*> AllWidgets;
        Blueprint->WidgetTree->GetAllWidgets(AllWidgets);
        TSet<FName> ActualNames;
        for (UWidget* Widget : AllWidgets)
        {
            if (const UComboBoxString* ComboBox =
                    Cast<UComboBoxString>(Widget))
            {
                ActualNames.Add(ComboBox->GetFName());
            }
        }

        for (const FName& ActualName : ActualNames)
        {
            if (!ExpectedNames.Contains(ActualName))
            {
                AddError(FString::Printf(
                    TEXT("Uninventoried ComboBox %s in %s."),
                    *ActualName.ToString(),
                    ObjectPath));
            }
        }
        for (const FName& ExpectedName : ExpectedNames)
        {
            const UComboBoxString* ComboBox =
                Cast<UComboBoxString>(
                    Blueprint->WidgetTree->FindWidget(ExpectedName));
            if (ComboBox == nullptr)
            {
                AddError(FString::Printf(
                    TEXT("Missing ComboBox %s in %s."),
                    *ExpectedName.ToString(),
                    ObjectPath));
                continue;
            }

            CompareStyle(
                *ComboBox,
                *AtmosphereReference,
                FString::Printf(
                    TEXT("%s.%s"),
                    ObjectPath,
                    *ExpectedName.ToString()),
                *this);
            ++CheckedComboBoxCount;
        }

        TestEqual(
            FString::Printf(
                TEXT("ComboBox count for %s"),
                ObjectPath),
            ActualNames.Num(),
            ExpectedNames.Num());
        TestEqual(
            FString::Printf(
                TEXT("Package dirtiness is unchanged for %s"),
                ObjectPath),
            Package->IsDirty(),
            bPackageWasDirty);
    }

    TestEqual(
        TEXT("All active Blueprint ComboBoxes were checked"),
        CheckedComboBoxCount,
        static_cast<int32>(UE_ARRAY_COUNT(Targets)));
    return !HasAnyErrors();
}

#endif
