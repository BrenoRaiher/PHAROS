// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "Simulation/ComponentTree/TGComponentTreeEditingLibrary.h"

namespace TGComponentTreeEditingPrivate
{
    bool NamesEqual(
        const FString& First,
        const FString& Second)
    {
        return First.Equals(Second, ESearchCase::IgnoreCase);
    }

    bool ContainsName(
        const TArray<FString>& Names,
        const FString& Candidate)
    {
        for (const FString& Name : Names)
        {
            if (NamesEqual(Name, Candidate))
            {
                return true;
            }
        }

        return false;
    }

    int32 FindComponentIndexByIdInternal(
        const TArray<FTGComponentConfig>& Components,
        const FGuid& ComponentId)
    {
        if (!ComponentId.IsValid())
        {
            return INDEX_NONE;
        }

        for (int32 Index = 0; Index < Components.Num(); ++Index)
        {
            if (Components[Index].ComponentId == ComponentId)
            {
                return Index;
            }
        }

        return INDEX_NONE;
    }

    int32 FindJointDofIndexByIdInternal(
        const FTGComponentConfig& Component,
        const FGuid& DofId)
    {
        if (!DofId.IsValid())
        {
            return INDEX_NONE;
        }

        for (
            int32 Index = 0;
            Index < Component.DegreesOfFreedom.Num();
            ++Index)
        {
            if (Component.DegreesOfFreedom[Index].DofId == DofId)
            {
                return Index;
            }
        }

        return INDEX_NONE;
    }

    bool IsComponentNameAlreadyUsed(
        const TArray<FTGComponentConfig>& Components,
        const FString& Candidate,
        int32 IgnoredIndex = INDEX_NONE)
    {
        for (int32 Index = 0; Index < Components.Num(); ++Index)
        {
            if (Index == IgnoredIndex)
            {
                continue;
            }

            if (NamesEqual(Components[Index].Name, Candidate))
            {
                return true;
            }
        }

        return false;
    }

    bool IsDofNameAlreadyUsed(
        const FTGComponentConfig& Component,
        const FString& Candidate)
    {
        for (const FTGJointDofConfig& Dof
             : Component.DegreesOfFreedom)
        {
            if (NamesEqual(Dof.Name, Candidate))
            {
                return true;
            }
        }

        return false;
    }

    FString MakeUniqueComponentName(
        const TArray<FTGComponentConfig>& Components)
    {
        for (int32 Number = 1; Number < MAX_int32; ++Number)
        {
            const FString Candidate =
                FString::Printf(TEXT("Component %d"), Number);

            if (!IsComponentNameAlreadyUsed(
                    Components,
                    Candidate))
            {
                return Candidate;
            }
        }

        return TEXT("Component");
    }

    FString MakeUniqueDofName(
        const FTGComponentConfig& Component)
    {
        for (int32 Number = 1; Number < MAX_int32; ++Number)
        {
            const FString Candidate =
                FString::Printf(TEXT("DOF %d"), Number);

            if (!IsDofNameAlreadyUsed(Component, Candidate))
            {
                return Candidate;
            }
        }

        return TEXT("DOF");
    }

    FTGComponentConfig MakeDefaultComponent(
        const FString& Name)
    {
        FTGComponentConfig Component;

        Component.ComponentId = FGuid::NewGuid();
        Component.Name = Name;

        Component.InitialMassKilograms = 1.0;
        Component.MinimumMassKilograms = 1.0;
        Component.bVariableMass = false;

        Component.LocalCenterOfMassMeters =
            FVector::ZeroVector;

        Component.CentroidalInertia
            .IxxKilogramMetersSquared = 1.0;

        Component.CentroidalInertia
            .IyyKilogramMetersSquared = 1.0;

        Component.CentroidalInertia
            .IzzKilogramMetersSquared = 1.0;

        Component.CentroidalInertia
            .IxyKilogramMetersSquared = 0.0;

        Component.CentroidalInertia
            .IxzKilogramMetersSquared = 0.0;

        Component.CentroidalInertia
            .IyzKilogramMetersSquared = 0.0;

        Component.OriginInBodyMeters =
            FVector::ZeroVector;

        Component.ComponentToBodyOrientation =
            FQuat::Identity;

        Component.ParentComponentName.Reset();

        Component.ParentAnchorMeters =
            FVector::ZeroVector;

        Component.ChildAnchorMeters =
            FVector::ZeroVector;

        Component.ChildToParentZeroOrientation =
            FQuat::Identity;

        Component.DegreesOfFreedom.Reset();

        return Component;
    }

    void CollectSubtreeIndices(
        const TArray<FTGComponentConfig>& Components,
        int32 RootIndex,
        TArray<int32>& OutIndices,
        TArray<FString>& OutNames)
    {
        OutIndices.Reset();
        OutNames.Reset();

        if (!Components.IsValidIndex(RootIndex))
        {
            return;
        }

        OutIndices.Add(RootIndex);
        OutNames.Add(Components[RootIndex].Name);

        bool bAddedComponent = true;

        while (bAddedComponent)
        {
            bAddedComponent = false;

            for (
                int32 Index = 0;
                Index < Components.Num();
                ++Index)
            {
                if (OutIndices.Contains(Index))
                {
                    continue;
                }

                const FString& ParentName =
                    Components[Index].ParentComponentName;

                if (ContainsName(OutNames, ParentName))
                {
                    OutIndices.Add(Index);
                    OutNames.Add(Components[Index].Name);
                    bAddedComponent = true;
                }
            }
        }

        OutIndices.Sort();
    }

    bool FindExternalReference(
        const FTGSimulationScenario& Scenario,
        const TArray<FString>& ComponentNames,
        FString& OutReferenceDescription)
    {
        for (const FTGThrusterConfig& Thruster
             : Scenario.Thrusters)
        {
            if (ContainsName(
                    ComponentNames,
                    Thruster.MountComponentName))
            {
                OutReferenceDescription = FString::Printf(
                    TEXT(
                        "Component '%s' is the mount component "
                        "of thruster '%s'."),
                    *Thruster.MountComponentName,
                    *Thruster.Name);

                return true;
            }

            if (ContainsName(
                    ComponentNames,
                    Thruster.PropellantComponentName))
            {
                OutReferenceDescription = FString::Printf(
                    TEXT(
                        "Component '%s' supplies propellant "
                        "to thruster '%s'."),
                    *Thruster.PropellantComponentName,
                    *Thruster.Name);

                return true;
            }
        }

        for (const FTGReactionWheelConfig& Wheel
             : Scenario.ReactionWheels)
        {
            if (ContainsName(
                    ComponentNames,
                    Wheel.MountComponentName))
            {
                OutReferenceDescription = FString::Printf(
                    TEXT(
                        "Component '%s' is the mount component "
                        "of reaction wheel '%s'."),
                    *Wheel.MountComponentName,
                    *Wheel.Name);

                return true;
            }
        }

        OutReferenceDescription.Reset();
        return false;
    }

    void ReplaceNameReference(
        FString& Reference,
        const FString& OldName,
        const FString& NewName)
    {
        if (NamesEqual(Reference, OldName))
        {
            Reference = NewName;
        }
    }
}

void UTGComponentTreeEditingLibrary::
    NormalizeScenarioComponentTree(
        FTGSimulationScenario& Scenario)
{
    if (Scenario.Components.IsEmpty())
    {
        Scenario.Components.Add(
            TGComponentTreeEditingPrivate::
                MakeDefaultComponent(TEXT("Main Body")));
    }

    TSet<FGuid> UsedComponentIds;
    TSet<FGuid> UsedDofIds;

    for (FTGComponentConfig& Component
         : Scenario.Components)
    {
        if (
            !Component.ComponentId.IsValid() ||
            UsedComponentIds.Contains(Component.ComponentId))
        {
            Component.ComponentId = FGuid::NewGuid();
        }

        UsedComponentIds.Add(Component.ComponentId);

        for (FTGJointDofConfig& Dof
             : Component.DegreesOfFreedom)
        {
            if (
                !Dof.DofId.IsValid() ||
                UsedDofIds.Contains(Dof.DofId))
            {
                Dof.DofId = FGuid::NewGuid();
            }

            UsedDofIds.Add(Dof.DofId);
        }
    }
}

bool UTGComponentTreeEditingLibrary::
    AddDefaultChildComponent(
        FTGSimulationScenario& Scenario,
        FGuid ParentComponentId,
        FGuid& OutComponentId,
        FText& OutErrorText)
{
    OutComponentId.Invalidate();
    OutErrorText = FText::GetEmpty();

    NormalizeScenarioComponentTree(Scenario);

    const int32 ParentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                ParentComponentId);

    if (ParentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected parent component no longer exists."));

        return false;
    }

    FTGComponentConfig NewComponent =
        TGComponentTreeEditingPrivate::
            MakeDefaultComponent(
                TGComponentTreeEditingPrivate::
                    MakeUniqueComponentName(
                        Scenario.Components));

    NewComponent.ParentComponentName =
        Scenario.Components[ParentIndex].Name;

    NewComponent.OriginInBodyMeters =
        FVector::ZeroVector;

    NewComponent.ComponentToBodyOrientation =
        FQuat::Identity;

    Scenario.Components.Add(NewComponent);

    OutComponentId = NewComponent.ComponentId;
    return true;
}

bool UTGComponentTreeEditingLibrary::
    RenameComponent(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FString& NewName,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    FString TrimmedName = NewName;
    TrimmedName.TrimStartAndEndInline();

    if (TrimmedName.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT("Component name cannot be empty."));

        return false;
    }

    if (TGComponentTreeEditingPrivate::
            IsComponentNameAlreadyUsed(
                Scenario.Components,
                TrimmedName,
                ComponentIndex))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Another component is already named '%s'."),
                *TrimmedName));

        return false;
    }

    const FString OldName =
        Scenario.Components[ComponentIndex].Name;

    if (TGComponentTreeEditingPrivate::
            NamesEqual(OldName, TrimmedName))
    {
        Scenario.Components[ComponentIndex].Name =
            TrimmedName;

        return true;
    }

    Scenario.Components[ComponentIndex].Name =
        TrimmedName;

    for (FTGComponentConfig& Component
         : Scenario.Components)
    {
        TGComponentTreeEditingPrivate::
            ReplaceNameReference(
                Component.ParentComponentName,
                OldName,
                TrimmedName);
    }

    for (FTGThrusterConfig& Thruster
         : Scenario.Thrusters)
    {
        TGComponentTreeEditingPrivate::
            ReplaceNameReference(
                Thruster.MountComponentName,
                OldName,
                TrimmedName);

        TGComponentTreeEditingPrivate::
            ReplaceNameReference(
                Thruster.PropellantComponentName,
                OldName,
                TrimmedName);
    }

    for (FTGReactionWheelConfig& Wheel
         : Scenario.ReactionWheels)
    {
        TGComponentTreeEditingPrivate::
            ReplaceNameReference(
                Wheel.MountComponentName,
                OldName,
                TrimmedName);
    }

    for (FTGOpticalFacetConfig& Facet
         : Scenario.SolarRadiationPressure.OpticalFacets)
    {
        if (
            Facet.ComponentId == ComponentId ||
            TGComponentTreeEditingPrivate::
                NamesEqual(
                    Facet.ComponentName,
                    OldName))
        {
            Facet.ComponentId = ComponentId;
            Facet.ComponentName = TrimmedName;
        }
    }

    return true;
}

bool UTGComponentTreeEditingLibrary::
    ReparentComponent(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid NewParentComponentId,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                ComponentId);

    const int32 NewParentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                NewParentComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (NewParentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected new parent no longer exists."));

        return false;
    }

    if (ComponentIndex == 0)
    {
        OutErrorText = FText::FromString(
            TEXT("The main component cannot be reparented."));

        return false;
    }

    if (ComponentIndex == NewParentIndex)
    {
        OutErrorText = FText::FromString(
            TEXT("A component cannot be its own parent."));

        return false;
    }

    TArray<int32> SubtreeIndices;
    TArray<FString> SubtreeNames;

    TGComponentTreeEditingPrivate::
        CollectSubtreeIndices(
            Scenario.Components,
            ComponentIndex,
            SubtreeIndices,
            SubtreeNames);

    if (SubtreeIndices.Contains(NewParentIndex))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "A component cannot be parented beneath "
                "one of its own descendants."));

        return false;
    }

    const FString NewParentName =
        Scenario.Components[NewParentIndex].Name;

    if (TGComponentTreeEditingPrivate::
            NamesEqual(
                Scenario.Components[ComponentIndex]
                    .ParentComponentName,
                NewParentName))
    {
        return true;
    }

    TArray<FTGComponentConfig> MovedComponents;
    MovedComponents.Reserve(SubtreeIndices.Num());

    for (const int32 Index : SubtreeIndices)
    {
        MovedComponents.Add(Scenario.Components[Index]);
    }

    for (
        int32 Position = SubtreeIndices.Num() - 1;
        Position >= 0;
        --Position)
    {
        Scenario.Components.RemoveAt(
            SubtreeIndices[Position]);
    }

    int32 MovedRootIndex = INDEX_NONE;

    for (
        int32 Index = 0;
        Index < MovedComponents.Num();
        ++Index)
    {
        if (MovedComponents[Index].ComponentId == ComponentId)
        {
            MovedRootIndex = Index;
            break;
        }
    }

    if (MovedRootIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The component subtree could not be reconstructed."));

        return false;
    }

    MovedComponents[MovedRootIndex]
        .ParentComponentName = NewParentName;

    Scenario.Components.Append(MovedComponents);

    return true;
}

bool UTGComponentTreeEditingLibrary::
    DeleteComponentSubtree(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        int32& OutDeletedComponentCount,
        FText& OutErrorText)
{
    OutDeletedComponentCount = 0;
    OutErrorText = FText::GetEmpty();

    NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (ComponentIndex == 0)
    {
        OutErrorText = FText::FromString(
            TEXT("The main component cannot be deleted."));

        return false;
    }

    TArray<int32> SubtreeIndices;
    TArray<FString> SubtreeNames;

    TGComponentTreeEditingPrivate::
        CollectSubtreeIndices(
            Scenario.Components,
            ComponentIndex,
            SubtreeIndices,
            SubtreeNames);

    FString ReferenceDescription;

    if (TGComponentTreeEditingPrivate::
            FindExternalReference(
                Scenario,
                SubtreeNames,
                ReferenceDescription))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "The component subtree cannot be deleted. %s"),
                *ReferenceDescription));

        return false;
    }

    TSet<FGuid> DeletedComponentIds;
    for (const int32 Index : SubtreeIndices)
    {
        DeletedComponentIds.Add(
            Scenario.Components[Index].ComponentId);
    }

    // Generated SRP facets are an owned cache, not an external reference.
    // Deleting their component removes them automatically.
    Scenario.SolarRadiationPressure.OpticalFacets.RemoveAll(
        [&DeletedComponentIds](const FTGOpticalFacetConfig& Facet)
        {
            return DeletedComponentIds.Contains(Facet.ComponentId);
        });

    OutDeletedComponentCount = SubtreeIndices.Num();

    for (
        int32 Position = SubtreeIndices.Num() - 1;
        Position >= 0;
        --Position)
    {
        Scenario.Components.RemoveAt(
            SubtreeIndices[Position]);
    }

    return true;
}

bool UTGComponentTreeEditingLibrary::
    AddDefaultJointDof(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        ETGJointMotionType MotionType,
        FGuid& OutDofId,
        FText& OutErrorText)
{
    OutDofId.Invalidate();
    OutErrorText = FText::GetEmpty();

    NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (ComponentIndex == 0)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The main component does not have a parent joint."));

        return false;
    }

    FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    FTGJointDofConfig NewDof;

    NewDof.DofId = FGuid::NewGuid();
    NewDof.Name =
        TGComponentTreeEditingPrivate::
            MakeUniqueDofName(Component);

    NewDof.MotionType = MotionType;
    NewDof.Axis = FVector::ForwardVector;
    NewDof.InitialCoordinate = 0.0;
    NewDof.InitialRate = 0.0;

    NewDof.bHasMinimumCoordinate = true;
    NewDof.bHasMaximumCoordinate = true;

    if (MotionType == ETGJointMotionType::Rotation)
    {
        NewDof.MinimumCoordinate = -PI;
        NewDof.MaximumCoordinate = PI;
        NewDof.MaximumAbsoluteRate = 1.0;
        NewDof.MaximumAbsoluteEffort = 0.0;
    }
    else
    {
        NewDof.MinimumCoordinate = -1.0;
        NewDof.MaximumCoordinate = 1.0;
        NewDof.MaximumAbsoluteRate = 1.0;
        NewDof.MaximumAbsoluteEffort = 0.0;
    }

    Component.DegreesOfFreedom.Add(NewDof);

    OutDofId = NewDof.DofId;
    return true;
}

bool UTGComponentTreeEditingLibrary::
    DeleteJointDof(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DofId,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    const int32 DofIndex =
        TGComponentTreeEditingPrivate::
            FindJointDofIndexByIdInternal(
                Component,
                DofId);

    if (DofIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected joint DOF no longer exists."));

        return false;
    }

    Component.DegreesOfFreedom.RemoveAt(DofIndex);
    return true;
}

bool UTGComponentTreeEditingLibrary::
    MoveJointDof(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DofId,
        int32 NewIndex,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditingPrivate::
            FindComponentIndexByIdInternal(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    const int32 OldIndex =
        TGComponentTreeEditingPrivate::
            FindJointDofIndexByIdInternal(
                Component,
                DofId);

    if (OldIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected joint DOF no longer exists."));

        return false;
    }

    if (!Component.DegreesOfFreedom.IsValidIndex(NewIndex))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "The requested DOF index %d is outside "
                    "the valid range 0 through %d."),
                NewIndex,
                Component.DegreesOfFreedom.Num() - 1));

        return false;
    }

    if (OldIndex == NewIndex)
    {
        return true;
    }

    int32 CurrentIndex = OldIndex;

    while (CurrentIndex < NewIndex)
    {
        Component.DegreesOfFreedom.Swap(
            CurrentIndex,
            CurrentIndex + 1);

        ++CurrentIndex;
    }

    while (CurrentIndex > NewIndex)
    {
        Component.DegreesOfFreedom.Swap(
            CurrentIndex,
            CurrentIndex - 1);

        --CurrentIndex;
    }

    return true;
}

int32 UTGComponentTreeEditingLibrary::
    FindComponentIndexById(
        const TArray<FTGComponentConfig>& Components,
        FGuid ComponentId)
{
    return TGComponentTreeEditingPrivate::
        FindComponentIndexByIdInternal(
            Components,
            ComponentId);
}

int32 UTGComponentTreeEditingLibrary::
    FindJointDofIndexById(
        const FTGComponentConfig& Component,
        FGuid DofId)
{
    return TGComponentTreeEditingPrivate::
        FindJointDofIndexByIdInternal(
            Component,
            DofId);
}
