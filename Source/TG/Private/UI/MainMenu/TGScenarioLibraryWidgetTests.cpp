// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"
#include "UI/MainMenu/TGScenarioLibraryEntryWidgetBase.h"
#include "UI/MainMenu/TGScenarioLibraryWidgetBase.h"
#include "Widgets/SWidget.h"

namespace TGScenarioLibraryWidgetTests
{
    bool ValidateWidgetBranch(
        FAutomationTestBase& Test,
        UWidget* Widget,
        TSet<const UWidget*>& ActiveWidgets,
        TSet<const UWidget*>& VisitedWidgets)
    {
        if (Widget == nullptr)
        {
            Test.AddError(TEXT("The Scenario Library widget tree contains a null child."));
            return false;
        }

        if (ActiveWidgets.Contains(Widget))
        {
            Test.AddError(FString::Printf(
                TEXT("The Scenario Library widget tree contains a cycle at '%s'."),
                *Widget->GetName()));
            return false;
        }

        if (VisitedWidgets.Contains(Widget))
        {
            Test.AddError(FString::Printf(
                TEXT("The Scenario Library widget '%s' is attached to more than one parent."),
                *Widget->GetName()));
            return false;
        }

        ActiveWidgets.Add(Widget);
        VisitedWidgets.Add(Widget);

        bool bValid = true;
        if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
        {
            for (int32 ChildIndex = 0;
                 ChildIndex < Panel->GetChildrenCount();
                 ++ChildIndex)
            {
                bValid &= ValidateWidgetBranch(
                    Test,
                    Panel->GetChildAt(ChildIndex),
                    ActiveWidgets,
                    VisitedWidgets);
            }
        }

        ActiveWidgets.Remove(Widget);
        return bValid;
    }

    bool ValidateSlateBranch(
        FAutomationTestBase& Test,
        const TSharedRef<SWidget>& Widget,
        TSet<const SWidget*>& ActiveWidgets,
        TSet<const SWidget*>& VisitedWidgets,
        TArray<FString>& WidgetPath)
    {
        const SWidget* WidgetAddress = &Widget.Get();
        const FString WidgetDescription = FString::Printf(
            TEXT("%s[%s]"),
            *Widget->GetTypeAsString(),
            *Widget->GetTag().ToString());

        if (ActiveWidgets.Contains(WidgetAddress))
        {
            WidgetPath.Add(WidgetDescription);
            Test.AddError(FString::Printf(
                TEXT("The generated Scenario Library Slate tree contains a cycle: %s"),
                *FString::Join(WidgetPath, TEXT(" -> "))));
            WidgetPath.Pop();
            return false;
        }

        if (VisitedWidgets.Contains(WidgetAddress))
        {
            Test.AddError(FString::Printf(
                TEXT("The generated Scenario Library Slate widget '%s' is attached more than once."),
                *WidgetDescription));
            return false;
        }

        ActiveWidgets.Add(WidgetAddress);
        VisitedWidgets.Add(WidgetAddress);
        WidgetPath.Add(WidgetDescription);

        bool bValid = true;
        FChildren* Children = Widget->GetAllChildren();
        if (Children != nullptr)
        {
            for (int32 ChildIndex = 0;
                 ChildIndex < Children->Num();
                 ++ChildIndex)
            {
                bValid &= ValidateSlateBranch(
                    Test,
                    Children->GetChildAt(ChildIndex),
                    ActiveWidgets,
                    VisitedWidgets,
                    WidgetPath);
            }
        }

        WidgetPath.Pop();
        ActiveWidgets.Remove(WidgetAddress);
        return bValid;
    }

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGScenarioLibrarySlatePrepassTest,
    "TG.UI.MainMenu.ScenarioLibrarySlatePrepass",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGScenarioLibrarySlatePrepassTest::RunTest(const FString& Parameters)
{
    UClass* LibraryClass = StaticLoadClass(
        UUserWidget::StaticClass(),
        nullptr,
        TEXT("/Game/UI/WBP_ScenarioLibrary.WBP_ScenarioLibrary_C"));
    TestNotNull(TEXT("Scenario Library generated class"), LibraryClass);
    if (LibraryClass == nullptr)
    {
        return false;
    }

    UWidgetBlueprintGeneratedClass* GeneratedClass =
        Cast<UWidgetBlueprintGeneratedClass>(LibraryClass);
    TestNotNull(TEXT("Scenario Library Widget Blueprint class"), GeneratedClass);
    if (GeneratedClass == nullptr)
    {
        return false;
    }

    UWidgetTree* Tree = GeneratedClass->GetWidgetTreeArchetype();
    TestNotNull(TEXT("Scenario Library widget tree"), Tree);
    TestNotNull(
        TEXT("Scenario Library root widget"),
        Tree != nullptr ? Tree->RootWidget.Get() : nullptr);
    if (Tree == nullptr || Tree->RootWidget == nullptr)
    {
        return false;
    }

    TSet<const UWidget*> ActiveWidgets;
    TSet<const UWidget*> VisitedWidgets;
    TGScenarioLibraryWidgetTests::ValidateWidgetBranch(
        *this,
        Tree->RootWidget,
        ActiveWidgets,
        VisitedWidgets);

    const UListViewBase* RecentList = Cast<UListViewBase>(
        Tree->FindWidget(TEXT("LIST_Recent")));
    const UListViewBase* ScenarioList = Cast<UListViewBase>(
        Tree->FindWidget(TEXT("LIST_Scenarios")));
    TestNotNull(TEXT("Recent list"), RecentList);
    TestNotNull(TEXT("Scenario catalog list"), ScenarioList);

    for (const UListViewBase* List : {RecentList, ScenarioList})
    {
        if (List == nullptr)
        {
            continue;
        }

        const UClass* EntryClass = List->GetEntryWidgetClass();
        TestNotNull(TEXT("Scenario list entry class"), EntryClass);
        if (EntryClass != nullptr)
        {
            TestTrue(
                TEXT("Scenario list entry uses the native row base"),
                EntryClass->IsChildOf(
                    UTGScenarioLibraryEntryWidgetBase::StaticClass()));
            TestFalse(
                TEXT("Scenario list entry must not recursively use the library widget"),
                EntryClass->IsChildOf(
                    UTGScenarioLibraryWidgetBase::StaticClass()));
        }
    }

    UTGScenarioLibraryWidgetBase* Widget =
        NewObject<UTGScenarioLibraryWidgetBase>(
            GetTransientPackage(),
            LibraryClass);
    TestNotNull(TEXT("Scenario Library runtime widget"), Widget);
    if (Widget == nullptr)
    {
        return false;
    }

    TestTrue(TEXT("Scenario Library initialization"), Widget->Initialize());
    const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
    TSet<const SWidget*> ActiveSlateWidgets;
    TSet<const SWidget*> VisitedSlateWidgets;
    TArray<FString> SlateWidgetPath;
    if (!TGScenarioLibraryWidgetTests::ValidateSlateBranch(
            *this,
            SlateWidget,
            ActiveSlateWidgets,
            VisitedSlateWidgets,
            SlateWidgetPath))
    {
        return false;
    }
    SlateWidget->SlatePrepass(1.0f);

    return !HasAnyErrors();
}

#endif
