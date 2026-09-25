// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/ComponentTree/TGComponentTreeItemObject.h"

bool UTGComponentTreeItemObject::HasChildren() const
{
    return !Children.IsEmpty();
}

TArray<UObject*>
UTGComponentTreeItemObject::GetChildrenAsObjects() const
{
    TArray<UObject*> Result;
    Result.Reserve(Children.Num());

    for (UTGComponentTreeItemObject* Child : Children)
    {
        if (IsValid(Child))
        {
            Result.Add(Child);
        }
    }

    return Result;
}

TArray<UObject*>
UTGComponentTreeItemObject::GetAncestorsAsObjects() const
{
    TArray<UObject*> ReversedAncestors;

    const UTGComponentTreeItemObject* CurrentParent =
        ParentItem;

    while (IsValid(CurrentParent))
    {
        ReversedAncestors.Add(
            const_cast<UTGComponentTreeItemObject*>(
                CurrentParent));

        CurrentParent =
            CurrentParent->ParentItem;
    }

    TArray<UObject*> OrderedAncestors;
    OrderedAncestors.Reserve(
        ReversedAncestors.Num());

    for (
        int32 Index = ReversedAncestors.Num() - 1;
        Index >= 0;
        --Index)
    {
        OrderedAncestors.Add(
            ReversedAncestors[Index]);
    }

    return OrderedAncestors;
}

FGuid UTGComponentTreeItemObject::GetComponentId() const
{
    return ComponentId;
}