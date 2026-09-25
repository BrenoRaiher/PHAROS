// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UI/Configuration/Environment/TGConfigSolarRadiationPressureWidgetBase.h"
#include "UObject/ScriptDelegates.h"
#include "UObject/UnrealType.h"

namespace TGSrpWidgetContractTests
{
    const TCHAR* WidgetSourceRelativePath =
        TEXT(
            "Source/TG/Private/UI/Configuration/Environment/"
            "TGConfigSolarRadiationPressureWidgetBase.cpp");

    bool LoadWidgetSource(
        FAutomationTestBase& Test,
        FString& OutSource)
    {
        const FString SourcePath = FPaths::Combine(
            FPaths::ProjectDir(),
            WidgetSourceRelativePath);
        if (FFileHelper::LoadFileToString(OutSource, *SourcePath))
        {
            return true;
        }

        Test.AddError(
            FString::Printf(
                TEXT("Could not read SRP widget source contract: %s"),
                *SourcePath));
        return false;
    }

    bool ExtractFunctionBody(
        const FString& Source,
        const TCHAR* FunctionName,
        FString& OutBody)
    {
        const FRegexPattern DefinitionPattern(
            FString::Printf(
                TEXT(
                    "UTGConfigSolarRadiationPressureWidgetBase::"
                    "\\s*%s\\s*\\("),
                FunctionName));
        FRegexMatcher DefinitionMatcher(
            DefinitionPattern,
            Source);
        if (!DefinitionMatcher.FindNext())
        {
            return false;
        }

        const int32 OpenBraceIndex = Source.Find(
            TEXT("{"),
            ESearchCase::CaseSensitive,
            ESearchDir::FromStart,
            DefinitionMatcher.GetMatchEnding());
        if (OpenBraceIndex == INDEX_NONE)
        {
            return false;
        }

        int32 Depth = 0;
        for (int32 Index = OpenBraceIndex; Index < Source.Len(); ++Index)
        {
            if (Source[Index] == TEXT('{'))
            {
                ++Depth;
            }
            else if (Source[Index] == TEXT('}'))
            {
                --Depth;
                if (Depth == 0)
                {
                    OutBody = Source.Mid(
                        OpenBraceIndex,
                        Index - OpenBraceIndex + 1);
                    return true;
                }
            }
        }

        return false;
    }

    void TestContainsAll(
        FAutomationTestBase& Test,
        const FString& ContractName,
        const FString& Text,
        const TArray<FString>& RequiredTokens)
    {
        FString CompactText;
        CompactText.Reserve(Text.Len());
        for (const TCHAR Character : Text)
        {
            if (!FChar::IsWhitespace(Character))
            {
                CompactText.AppendChar(Character);
            }
        }

        for (const FString& RequiredToken : RequiredTokens)
        {
            FString CompactToken;
            CompactToken.Reserve(RequiredToken.Len());
            for (const TCHAR Character : RequiredToken)
            {
                if (!FChar::IsWhitespace(Character))
                {
                    CompactToken.AppendChar(Character);
                }
            }

            Test.TestTrue(
                *FString::Printf(
                    TEXT("%s contains '%s'"),
                    *ContractName,
                    *RequiredToken),
                CompactText.Contains(
                    CompactToken,
                    ESearchCase::CaseSensitive));
        }
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpReflectedDelegateHandlerContractTest,
    "TG.UI.SRP.Widget.ReflectedDelegateHandlerContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpReflectedDelegateHandlerContractTest::RunTest(
    const FString& Parameters)
{
    struct FExpectedHandler
    {
        const TCHAR* Name;
        int32 ParameterCount;
    };

    const FExpectedHandler ExpectedHandlers[] =
    {
        { TEXT("HandleEnableSrpChanged"), 1 },
        { TEXT("HandleComputeCelestialEclipsesChanged"), 1 },
        { TEXT("HandleAddOccultingBodyClicked"), 0 },
        { TEXT("HandleRemoveOccultingBodyClicked"), 0 },
        { TEXT("HandleClearOccultingBodiesClicked"), 0 },
        { TEXT("HandleComputeComponentShadowsChanged"), 1 },
        { TEXT("HandleApplyGlobalOpticalPropertiesClicked"), 0 },
        { TEXT("HandleComponentRowSelected"), 1 },
        { TEXT("HandleComponentRowIncludeChanged"), 2 },
        { TEXT("HandleComponentRowVisibilityChanged"), 2 },
        { TEXT("HandlePreviewSrpFacetClicked"), 2 },
        { TEXT("HandlePreviewSrpPrimitiveSurfaceClicked"), 2 },
        { TEXT("HandleClearFaceSelectionClicked"), 0 },
        { TEXT("HandleComponentSelectionChanged"), 2 },
        { TEXT("HandleIncludeComponentChanged"), 1 },
        { TEXT("HandleProxyResolutionModeChanged"), 2 },
        { TEXT("HandleCustomTargetTriangleCountCommitted"), 2 },
        { TEXT("HandleUseGlobalFallbackChanged"), 1 },
        { TEXT("HandleApplyOneOpticalConfigurationChanged"), 1 },
        { TEXT("HandleApplyComponentOpticalPropertiesClicked"), 0 },
        { TEXT("HandleOverrideTargetModeChanged"), 2 },
        { TEXT("HandleOverrideTargetChanged"), 2 },
        { TEXT("HandleSelectOverrideInPreviewClicked"), 0 },
        { TEXT("HandleApplyOverrideOpticalPropertiesClicked"), 0 },
        { TEXT("HandleClearOverrideOpticalPropertiesClicked"), 0 },
        { TEXT("HandleConfirmProxyInvalidationClicked"), 0 },
        { TEXT("HandleCancelProxyInvalidationClicked"), 0 }
    };

    UClass* WidgetClass =
        UTGConfigSolarRadiationPressureWidgetBase::StaticClass();
    UTGConfigSolarRadiationPressureWidgetBase* Widget =
        NewObject<UTGConfigSolarRadiationPressureWidgetBase>(
            GetTransientPackage());
    TestNotNull(TEXT("Transient SRP widget exists"), Widget);

    for (const FExpectedHandler& Expected : ExpectedHandlers)
    {
        const FName HandlerName(Expected.Name);
        UFunction* Function = WidgetClass->FindFunctionByName(HandlerName);
        TestNotNull(Expected.Name, Function);
        if (Function == nullptr)
        {
            continue;
        }

        TestTrue(
            *FString::Printf(
                TEXT("%s is a native reflected function"),
                Expected.Name),
            Function->HasAnyFunctionFlags(FUNC_Native));

        int32 ParameterCount = 0;
        for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
        {
            const FProperty* Property = *PropertyIt;
            if (Property->HasAnyPropertyFlags(CPF_Parm) &&
                !Property->HasAnyPropertyFlags(CPF_ReturnParm))
            {
                ++ParameterCount;
            }
        }
        TestEqual(
            *FString::Printf(
                TEXT("%s reflected parameter count"),
                Expected.Name),
            ParameterCount,
            Expected.ParameterCount);

        if (Widget != nullptr)
        {
            FScriptDelegate Delegate;
            Delegate.BindUFunction(Widget, HandlerName);
            TestTrue(
                *FString::Printf(
                    TEXT("%s binds by its exact reflected name"),
                    Expected.Name),
                Delegate.IsBound());
        }
    }

    TestNull(
        TEXT("Leading-space handler name does not resolve"),
        WidgetClass->FindFunctionByName(
            FName(TEXT(" HandleEnableSrpChanged"))));
    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpDynamicDelegateWhitespaceContractTest,
    "TG.UI.SRP.Widget.DynamicDelegateWhitespaceContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpDynamicDelegateWhitespaceContractTest::RunTest(
    const FString& Parameters)
{
    FString Source;
    if (!TGSrpWidgetContractTests::LoadWidgetSource(*this, Source))
    {
        return false;
    }

    const FRegexPattern SplitHandlerPattern(
        TEXT(
            "&UTGConfigSolarRadiationPressureWidgetBase::"
            "\\s+Handle[A-Za-z0-9_]+"));
    FRegexMatcher SplitHandlerMatcher(SplitHandlerPattern, Source);
    TestFalse(
        TEXT(
            "No dynamic-delegate handler pointer has whitespace after "
            "the scope operator"),
        SplitHandlerMatcher.FindNext());

    const TCHAR* RequiredDynamicBindings[] =
    {
        TEXT("HandleEnableSrpChanged"),
        TEXT("HandleComputeCelestialEclipsesChanged"),
        TEXT("HandleAddOccultingBodyClicked"),
        TEXT("HandleRemoveOccultingBodyClicked"),
        TEXT("HandleClearOccultingBodiesClicked"),
        TEXT("HandleComputeComponentShadowsChanged"),
        TEXT("HandleApplyGlobalOpticalPropertiesClicked"),
        TEXT("HandleComponentRowSelected"),
        TEXT("HandleComponentRowIncludeChanged"),
        TEXT("HandleComponentRowVisibilityChanged"),
        TEXT("HandleComponentSelectionChanged"),
        TEXT("HandleIncludeComponentChanged"),
        TEXT("HandleProxyResolutionModeChanged"),
        TEXT("HandleCustomTargetTriangleCountCommitted"),
        TEXT("HandleUseGlobalFallbackChanged"),
        TEXT("HandleApplyOneOpticalConfigurationChanged"),
        TEXT("HandleApplyComponentOpticalPropertiesClicked"),
        TEXT("HandleOverrideTargetModeChanged"),
        TEXT("HandleOverrideTargetChanged"),
        TEXT("HandleSelectOverrideInPreviewClicked"),
        TEXT("HandleApplyOverrideOpticalPropertiesClicked"),
        TEXT("HandleClearOverrideOpticalPropertiesClicked"),
        TEXT("HandleClearFaceSelectionClicked"),
        TEXT("HandlePreviewSrpFacetClicked"),
        TEXT("HandlePreviewSrpPrimitiveSurfaceClicked"),
        TEXT("HandleConfirmProxyInvalidationClicked"),
        TEXT("HandleCancelProxyInvalidationClicked")
    };
    for (const TCHAR* Handler : RequiredDynamicBindings)
    {
        TestTrue(
            *FString::Printf(
                TEXT("Dynamic binding uses contiguous pointer for %s"),
                Handler),
            Source.Contains(
                FString::Printf(
                    TEXT(
                        "&UTGConfigSolarRadiationPressureWidgetBase::%s"),
                    Handler),
                ESearchCase::CaseSensitive));
    }

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpPendingProxyInputContractTest,
    "TG.UI.SRP.Widget.PendingProxyInputConfirmationContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpPendingProxyInputContractTest::RunTest(
    const FString& Parameters)
{
    FString Source;
    if (!TGSrpWidgetContractTests::LoadWidgetSource(*this, Source))
    {
        return false;
    }

    FString ResolutionBody;
    FString CustomCountBody;
    FString ConfirmBody;
    FString CancelBody;
    TestTrue(
        TEXT("Resolution handler body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("HandleProxyResolutionModeChanged"),
            ResolutionBody));
    TestTrue(
        TEXT("Custom-count handler body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("HandleCustomTargetTriangleCountCommitted"),
            CustomCountBody));
    TestTrue(
        TEXT("Confirmation handler body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("HandleConfirmProxyInvalidationClicked"),
            ConfirmBody));
    TestTrue(
        TEXT("Cancellation handler body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("HandleCancelProxyInvalidationClicked"),
            CancelBody));

    TGSrpWidgetContractTests::TestContainsAll(
        *this,
        TEXT("Resolution handler"),
        ResolutionBody,
        {
            TEXT("TriangleOverrides.IsEmpty"),
            TEXT("ETGSrpPendingProxyInputChange::ResolutionMode"),
            TEXT("PendingProxyResolutionMode = NewMode"),
            TEXT("SetProxyInvalidationConfirmationVisible")
        });
    TGSrpWidgetContractTests::TestContainsAll(
        *this,
        TEXT("Custom-count handler"),
        CustomCountBody,
        {
            TEXT("TriangleOverrides.IsEmpty"),
            TEXT(
                "ETGSrpPendingProxyInputChange::"
                "CustomTargetTriangleCount"),
            TEXT(
                "PendingCustomTargetTriangleCount = "
                "TargetTriangleCount"),
            TEXT("SetProxyInvalidationConfirmationVisible")
        });
    TGSrpWidgetContractTests::TestContainsAll(
        *this,
        TEXT("Confirmation handler"),
        ConfirmBody,
        {
            TEXT("DiscardTriangleOverridesAndGeneratedProxy"),
            TEXT("case ETGSrpPendingProxyInputChange::ResolutionMode"),
            TEXT("ProxyResolutionMode ="),
            TEXT("PendingProxyResolutionMode"),
            TEXT(
                "case ETGSrpPendingProxyInputChange::"
                "CustomTargetTriangleCount"),
            TEXT("CustomTargetTriangleCount ="),
            TEXT("PendingCustomTargetTriangleCount"),
            TEXT("CommitWorkingScenarioToDraft"),
            TEXT("RefreshUiFromWorkingScenario")
        });
    TGSrpWidgetContractTests::TestContainsAll(
        *this,
        TEXT("Cancellation handler"),
        CancelBody,
        {
            TEXT("PendingProxyInvalidationComponentId.Invalidate"),
            TEXT("ETGSrpPendingProxyInputChange::None"),
            TEXT("SetProxyInvalidationConfirmationVisible(false)"),
            TEXT("RefreshUiFromWorkingScenario")
        });
    TestFalse(
        TEXT("Cancel never discards generated proxy data"),
        CancelBody.Contains(
            TEXT("DiscardTriangleOverridesAndGeneratedProxy")));
    TestFalse(
        TEXT("Cancel never commits a pending value"),
        CancelBody.Contains(TEXT("CommitWorkingScenarioToDraft")));

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGSrpPreviewLifecycleContractTest,
    "TG.UI.SRP.Widget.PreviewLifecycleContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGSrpPreviewLifecycleContractTest::RunTest(
    const FString& Parameters)
{
    FString Source;
    if (!TGSrpWidgetContractTests::LoadWidgetSource(*this, Source))
    {
        return false;
    }

    FString ActivateBody;
    FString DeactivateBody;
    FString DestructBody;
    FString ConstructBody;
    FString RefreshIntegrationBody;
    TestTrue(
        TEXT("Preview activation body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("ActivateSolarRadiationPressurePreview"),
            ActivateBody));
    TestTrue(
        TEXT("Preview deactivation body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("DeactivateSolarRadiationPressurePreview"),
            DeactivateBody));
    TestTrue(
        TEXT("NativeDestruct body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("NativeDestruct"),
            DestructBody));
    TestTrue(
        TEXT("NativeConstruct body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("NativeConstruct"),
            ConstructBody));
    TestTrue(
        TEXT("Preview refresh body is extractable"),
        TGSrpWidgetContractTests::ExtractFunctionBody(
            Source,
            TEXT("RefreshPreviewIntegration"),
            RefreshIntegrationBody));

    TestTrue(
        TEXT("The SRP panel supports the keyboard focus requested by Game+UI input mode"),
        ConstructBody.Contains(
            TEXT("SetIsFocusable(true)"),
            ESearchCase::CaseSensitive));

    TGSrpWidgetContractTests::TestContainsAll(
        *this,
        TEXT("Preview activation"),
        ActivateBody,
        {
            TEXT("DeactivateSolarRadiationPressurePreview"),
            TEXT("PullWorkingScenarioFromDraft"),
            TEXT("NormalizeSolarRadiationPressureScenario"),
            TEXT("SetActorHiddenInGame(false)"),
            TEXT("SetPreviewSpacecraftActor"),
            TEXT("BuildFromScenario"),
            TEXT("OnPreviewSrpFacetClicked.AddUniqueDynamic"),
            TEXT("OnPreviewSrpPrimitiveSurfaceClicked.AddUniqueDynamic"),
            TEXT("SetSrpFacetSelectionEnabled(true)"),
            TEXT("Set3DInteractionEnabled(true)"),
            TEXT("bPreviewIntegrationActive = true"),
            TEXT("ShowSrpProxyPreview"),
            TEXT("FrameSpacecraft"),
            TEXT("return true")
        });
    TGSrpWidgetContractTests::TestContainsAll(
        *this,
        TEXT("Preview deactivation"),
        DeactivateBody,
        {
            TEXT("bPreviewIntegrationActive = false"),
            TEXT("OnPreviewSrpFacetClicked.RemoveDynamic"),
            TEXT("OnPreviewSrpPrimitiveSurfaceClicked.RemoveDynamic"),
            TEXT("SetSrpFacetSelectionEnabled(false)"),
            TEXT("Set3DInteractionEnabled(false)"),
            TEXT("ClearPreviewComponentSelection"),
            TEXT("ClearSrpProxyPreview"),
            TEXT("SetActorHiddenInGame(true)"),
            TEXT("PreviewPawn = nullptr"),
            TEXT("PreviewSpacecraftActor = nullptr")
        });

    TestTrue(
        TEXT("Preview actor is unhidden only after a successful build"),
        ActivateBody.Find(TEXT("BuildFromScenario")) != INDEX_NONE &&
        ActivateBody.Find(TEXT("SetActorHiddenInGame(false)")) >
            ActivateBody.Find(TEXT("BuildFromScenario")));
    TestTrue(
        TEXT("Activation has failure rollback paths"),
        ActivateBody.Replace(
            TEXT("DeactivateSolarRadiationPressurePreview"),
            TEXT(""),
            ESearchCase::CaseSensitive).Len() <=
            ActivateBody.Len() -
                (3 * FCString::Strlen(
                    TEXT("DeactivateSolarRadiationPressurePreview"))));

    TestTrue(
        TEXT("Widget destruction performs full preview deactivation"),
        DestructBody.Contains(
            TEXT("DeactivateSolarRadiationPressurePreview"),
            ESearchCase::CaseSensitive));
    TestFalse(
        TEXT("Preview refresh never adopts an arbitrary world pawn"),
        RefreshIntegrationBody.Contains(
            TEXT("TActorIterator"),
            ESearchCase::CaseSensitive));
    TGSrpWidgetContractTests::TestContainsAll(
        *this,
        TEXT("Preview refresh"),
        RefreshIntegrationBody,
        {
            TEXT("IsValid(PreviewPawn)"),
            TEXT("IsValid(PreviewSpacecraftActor)"),
            TEXT("DeactivateSolarRadiationPressurePreview"),
            TEXT("SetPreviewSpacecraftActor"),
            TEXT("SetSrpFacetSelectionEnabled(true)"),
        });

    UFunction* ActivateFunction =
        UTGConfigSolarRadiationPressureWidgetBase::StaticClass()->
            FindFunctionByName(
                FName(TEXT("ActivateSolarRadiationPressurePreview")));
    TestNotNull(
        TEXT("Preview activation is reflected for Blueprint"),
        ActivateFunction);
    if (ActivateFunction != nullptr)
    {
        TestEqual(
            TEXT("Preview activation has two inputs plus its return value"),
            ActivateFunction->NumParms,
            3);
        TestTrue(
            TEXT("Preview activation exposes a Boolean success result"),
            CastField<FBoolProperty>(
                ActivateFunction->GetReturnProperty()) != nullptr);
    }

    UFunction* DeactivateFunction =
        UTGConfigSolarRadiationPressureWidgetBase::StaticClass()->
            FindFunctionByName(
                FName(TEXT("DeactivateSolarRadiationPressurePreview")));
    TestNotNull(
        TEXT("Preview deactivation is reflected for Blueprint"),
        DeactivateFunction);
    if (DeactivateFunction != nullptr)
    {
        TestEqual(
            TEXT("Preview deactivation has no parameters"),
            DeactivateFunction->NumParms,
            0);
        TestNull(
            TEXT("Preview deactivation has no return value"),
            DeactivateFunction->GetReturnProperty());
    }

    return !HasAnyErrors();
}

#endif
