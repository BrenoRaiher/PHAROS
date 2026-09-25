// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/ComponentTree/TGComponentTreeViewModelLibrary.h"

namespace TGComponentTreeViewModelPrivate
{
    FString NormalizeName(const FString& Name)
    {
        FString Result = Name;
        Result.TrimStartAndEndInline();
        Result.ToLowerInline();

        return Result;
    }
}

bool UTGComponentTreeViewModelLibrary::
    BuildComponentTreeItems(
        UObject* ItemOuter,
        const FTGSimulationScenario& Scenario,
        TArray<UObject*>& OutRootItems,
        TArray<UTGComponentTreeItemObject*>& OutAllItems,
        FText& OutErrorText)
{
    using namespace TGComponentTreeViewModelPrivate;

    OutRootItems.Reset();
    OutAllItems.Reset();
    OutErrorText = FText::GetEmpty();

    if (!IsValid(ItemOuter))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "A valid owner is required when creating "
                "component-tree items."));

        return false;
    }

    if (Scenario.Components.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The spacecraft must contain at least "
                "one physical component."));

        return false;
    }

    OutAllItems.Reserve(
        Scenario.Components.Num());

    TMap<FString, int32> ComponentIndexByName;

    /*
     * First pass:
     * Validate names and create every item.
     */
    for (
        int32 ComponentIndex = 0;
        ComponentIndex < Scenario.Components.Num();
        ++ComponentIndex)
    {
        const FTGComponentConfig& Component =
            Scenario.Components[ComponentIndex];

        FString TrimmedName = Component.Name;
        TrimmedName.TrimStartAndEndInline();

        if (TrimmedName.IsEmpty())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Components[%d] has an empty name."),
                    ComponentIndex));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        const FString NormalizedName =
            NormalizeName(TrimmedName);

        if (ComponentIndexByName.Contains(
                NormalizedName))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component name '%s' is duplicated."),
                    *TrimmedName));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        if (!Component.ComponentId.IsValid())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' has an invalid identifier. "
                        "Normalize the scenario component tree first."),
                    *TrimmedName));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        UTGComponentTreeItemObject* NewItem =
            NewObject<UTGComponentTreeItemObject>(
                ItemOuter);

        if (!IsValid(NewItem))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Could not create the tree item "
                        "for component '%s'."),
                    *TrimmedName));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        NewItem->ComponentId =
            Component.ComponentId;

        NewItem->ComponentName =
            TrimmedName;

        NewItem->ComponentIndex =
            ComponentIndex;

        NewItem->Depth = 0;
        NewItem->bIsRoot = ComponentIndex == 0;
        NewItem->ParentItem = nullptr;
        NewItem->Children.Reset();

        ComponentIndexByName.Add(
            NormalizedName,
            ComponentIndex);

        OutAllItems.Add(NewItem);
    }

    /*
     * Component zero is the unique root.
     */
    {
        const FTGComponentConfig& RootComponent =
            Scenario.Components[0];

        FString RootParentName =
            RootComponent.ParentComponentName;

        RootParentName.TrimStartAndEndInline();

        if (!RootParentName.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Component zero is the main component "
                    "and cannot have a parent."));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        OutRootItems.Add(
            OutAllItems[0]);
    }

    /*
     * Second pass:
     * Connect every child to an earlier parent.
     */
    for (
        int32 ComponentIndex = 1;
        ComponentIndex < Scenario.Components.Num();
        ++ComponentIndex)
    {
        const FTGComponentConfig& Component =
            Scenario.Components[ComponentIndex];

        FString ParentName =
            Component.ParentComponentName;

        ParentName.TrimStartAndEndInline();

        if (ParentName.IsEmpty())
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' does not have a parent."),
                    *Component.Name));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        const int32* ParentIndexPointer =
            ComponentIndexByName.Find(
                NormalizeName(ParentName));

        if (ParentIndexPointer == nullptr)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Parent component '%s' referenced by "
                        "'%s' does not exist."),
                    *ParentName,
                    *Component.Name));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        const int32 ParentIndex =
            *ParentIndexPointer;

        if (ParentIndex >= ComponentIndex)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Component '%s' must reference a parent "
                        "appearing earlier in the component array."),
                    *Component.Name));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        UTGComponentTreeItemObject* ParentItem =
            OutAllItems[ParentIndex];

        UTGComponentTreeItemObject* ChildItem =
            OutAllItems[ComponentIndex];

        if (
            !IsValid(ParentItem) ||
            !IsValid(ChildItem))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The component-tree item hierarchy "
                    "could not be constructed."));

            OutRootItems.Reset();
            OutAllItems.Reset();
            return false;
        }

        ChildItem->ParentItem = ParentItem;
        ChildItem->Depth = ParentItem->Depth + 1;
        ChildItem->bIsRoot = false;

        ParentItem->Children.Add(ChildItem);
    }

    return true;
}

bool UTGComponentTreeViewModelLibrary::
    FindComponentTreeItemById(
        const TArray<UTGComponentTreeItemObject*>& Items,
        FGuid ComponentId,
        UTGComponentTreeItemObject*& OutItem)
{
    OutItem = nullptr;

    if (!ComponentId.IsValid())
    {
        return false;
    }

    for (UTGComponentTreeItemObject* Item : Items)
    {
        if (
            IsValid(Item) &&
            Item->ComponentId == ComponentId)
        {
            OutItem = Item;
            return true;
        }
    }

    return false;
}

bool UTGComponentTreeViewModelLibrary::
    FindComponentTreeItemByIndex(
        const TArray<UTGComponentTreeItemObject*>& Items,
        int32 ComponentIndex,
        UTGComponentTreeItemObject*& OutItem)
{
    OutItem = nullptr;

    for (UTGComponentTreeItemObject* Item : Items)
    {
        if (
            IsValid(Item) &&
            Item->ComponentIndex == ComponentIndex)
        {
            OutItem = Item;
            return true;
        }
    }

    return false;
}