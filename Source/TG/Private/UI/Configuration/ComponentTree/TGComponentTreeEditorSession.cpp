// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/ComponentTree/TGComponentTreeEditorSession.h"

#include "Simulation/ComponentTree/TGComponentTreeEditingLibrary.h"
#include "Simulation/ComponentTree/TGComponentTreeValidationLibrary.h"
#include "Simulation/ComponentTree/TGComponentKinematicsLibrary.h"
#include "Misc/Paths.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"

namespace TGComponentTreeEditorSessionPrivate
{
    constexpr int32 MaxRotationalDegreesOfFreedomPerConnection = 3;
    constexpr int32 MaxTranslationalDegreesOfFreedomPerConnection = 3;
    constexpr int32 MaxTotalDegreesOfFreedomPerConnection = 6;

    FString NormalizeName(const FString& Name)
    {
        FString Result = Name;
        Result.TrimStartAndEndInline();
        Result.ToLowerInline();
        return Result;
    }

    int32 FindComponentIndexById(
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

    struct FHierarchyMaps
    {
        TMap<FGuid, int32> IndexById;
        TMap<FString, FGuid> IdByNormalizedName;
        TMap<FGuid, TArray<FGuid>> ChildrenByParentId;
    };

    bool BuildHierarchyMaps(
        const TArray<FTGComponentConfig>& Components,
        FHierarchyMaps& OutMaps,
        FText& OutErrorText)
    {
        OutMaps = FHierarchyMaps();
        OutErrorText = FText::GetEmpty();

        if (Components.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The spacecraft must contain at least one "
                    "physical component."));

            return false;
        }

        for (int32 Index = 0; Index < Components.Num(); ++Index)
        {
            const FTGComponentConfig& Component = Components[Index];

            FString TrimmedName = Component.Name;
            TrimmedName.TrimStartAndEndInline();

            if (TrimmedName.IsEmpty())
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("Components[%d] has an empty name."),
                        Index));

                return false;
            }

            if (!Component.ComponentId.IsValid())
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Component '%s' has an invalid identifier."),
                        *TrimmedName));

                return false;
            }

            if (OutMaps.IndexById.Contains(Component.ComponentId))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Component '%s' has a duplicate identifier."),
                        *TrimmedName));

                return false;
            }

            const FString NormalizedName = NormalizeName(TrimmedName);

            if (OutMaps.IdByNormalizedName.Contains(NormalizedName))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("Component name '%s' is duplicated."),
                        *TrimmedName));

                return false;
            }

            OutMaps.IndexById.Add(Component.ComponentId, Index);
            OutMaps.IdByNormalizedName.Add(
                NormalizedName,
                Component.ComponentId);

            OutMaps.ChildrenByParentId.FindOrAdd(
                Component.ComponentId);
        }

        FString RootParentName = Components[0].ParentComponentName;
        RootParentName.TrimStartAndEndInline();

        if (!RootParentName.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Component zero is the main component and "
                    "cannot have a parent."));

            return false;
        }

        for (int32 Index = 1; Index < Components.Num(); ++Index)
        {
            const FTGComponentConfig& Component = Components[Index];

            FString ParentName = Component.ParentComponentName;
            ParentName.TrimStartAndEndInline();

            if (ParentName.IsEmpty())
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("Component '%s' does not have a parent."),
                        *Component.Name));

                return false;
            }

            const FGuid* ParentId =
                OutMaps.IdByNormalizedName.Find(
                    NormalizeName(ParentName));

            if (ParentId == nullptr)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Parent component '%s' referenced by '%s' "
                            "does not exist."),
                        *ParentName,
                        *Component.Name));

                return false;
            }

            const int32* ParentIndex =
                OutMaps.IndexById.Find(*ParentId);

            if (ParentIndex == nullptr || *ParentIndex >= Index)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Component '%s' must reference a parent "
                            "appearing earlier in the component array."),
                        *Component.Name));

                return false;
            }

            OutMaps.ChildrenByParentId
                .FindOrAdd(*ParentId)
                .Add(Component.ComponentId);
        }

        return true;
    }

    bool CollectSubtreeIdsPreorder(
        const FGuid& RootComponentId,
        const FHierarchyMaps& Maps,
        TArray<FGuid>& OutIds,
        FText& OutErrorText)
    {
        OutIds.Reset();
        OutErrorText = FText::GetEmpty();

        if (!Maps.IndexById.Contains(RootComponentId))
        {
            OutErrorText = FText::FromString(
                TEXT("The selected component no longer exists."));

            return false;
        }

        TSet<FGuid> Visiting;
        TSet<FGuid> Added;

        TFunction<bool(const FGuid&)> AppendSubtree;

        AppendSubtree =
            [&](const FGuid& ComponentId) -> bool
            {
                if (Visiting.Contains(ComponentId))
                {
                    OutErrorText = FText::FromString(
                        TEXT(
                            "The component hierarchy contains a cycle."));

                    return false;
                }

                if (Added.Contains(ComponentId))
                {
                    OutErrorText = FText::FromString(
                        TEXT(
                            "The component hierarchy contains a component "
                            "more than once."));

                    return false;
                }

                Visiting.Add(ComponentId);
                Added.Add(ComponentId);
                OutIds.Add(ComponentId);

                const TArray<FGuid>* Children =
                    Maps.ChildrenByParentId.Find(ComponentId);

                if (Children != nullptr)
                {
                    for (const FGuid& ChildId : *Children)
                    {
                        if (!AppendSubtree(ChildId))
                        {
                            return false;
                        }
                    }
                }

                Visiting.Remove(ComponentId);
                return true;
            };

        return AppendSubtree(RootComponentId);
    }

    bool FlattenHierarchyPreorder(
        const TArray<FTGComponentConfig>& Components,
        const FHierarchyMaps& Maps,
        TArray<FTGComponentConfig>& OutComponents,
        FText& OutErrorText)
    {
        OutComponents.Reset();
        OutComponents.Reserve(Components.Num());
        OutErrorText = FText::GetEmpty();

        if (Components.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The spacecraft must contain at least one "
                    "physical component."));

            return false;
        }

        TSet<FGuid> Visiting;
        TSet<FGuid> Added;

        TFunction<bool(const FGuid&)> AppendSubtree;

        AppendSubtree =
            [&](const FGuid& ComponentId) -> bool
            {
                if (Visiting.Contains(ComponentId))
                {
                    OutErrorText = FText::FromString(
                        TEXT(
                            "The component hierarchy contains a cycle."));

                    return false;
                }

                if (Added.Contains(ComponentId))
                {
                    OutErrorText = FText::FromString(
                        TEXT(
                            "The component hierarchy contains a component "
                            "more than once."));

                    return false;
                }

                const int32* ComponentIndex =
                    Maps.IndexById.Find(ComponentId);

                if (ComponentIndex == nullptr ||
                    !Components.IsValidIndex(*ComponentIndex))
                {
                    OutErrorText = FText::FromString(
                        TEXT(
                            "The component hierarchy references an "
                            "unknown component."));

                    return false;
                }

                Visiting.Add(ComponentId);
                Added.Add(ComponentId);
                OutComponents.Add(Components[*ComponentIndex]);

                const TArray<FGuid>* Children =
                    Maps.ChildrenByParentId.Find(ComponentId);

                if (Children != nullptr)
                {
                    for (const FGuid& ChildId : *Children)
                    {
                        if (!AppendSubtree(ChildId))
                        {
                            return false;
                        }
                    }
                }

                Visiting.Remove(ComponentId);
                return true;
            };

        if (!AppendSubtree(Components[0].ComponentId))
        {
            OutComponents.Reset();
            return false;
        }

        if (OutComponents.Num() != Components.Num())
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The component hierarchy is disconnected from "
                    "the main component."));

            OutComponents.Reset();
            return false;
        }

        return true;
    }

    bool HaveSameComponentOrder(
        const TArray<FTGComponentConfig>& First,
        const TArray<FTGComponentConfig>& Second)
    {
        if (First.Num() != Second.Num())
        {
            return false;
        }

        for (int32 Index = 0; Index < First.Num(); ++Index)
        {
            if (First[Index].ComponentId != Second[Index].ComponentId)
            {
                return false;
            }
        }

        return true;
    }

    bool MoveSubtreeInternal(
        FTGSimulationScenario& Scenario,
        const FGuid& ComponentId,
        const FGuid& TargetComponentId,
        ETGComponentTreeDropZone DropZone,
        bool& OutChanged,
        FText& OutErrorText)
    {
        OutChanged = false;
        OutErrorText = FText::GetEmpty();

        FTGSimulationScenario WorkingScenario = Scenario;

        FHierarchyMaps Maps;

        if (!BuildHierarchyMaps(
                WorkingScenario.Components,
                Maps,
                OutErrorText))
        {
            return false;
        }

        const int32* ComponentIndexPointer =
            Maps.IndexById.Find(ComponentId);

        if (ComponentIndexPointer == nullptr)
        {
            OutErrorText = FText::FromString(
                TEXT("The selected component no longer exists."));

            return false;
        }

        const int32* TargetIndexPointer =
            Maps.IndexById.Find(TargetComponentId);

        if (TargetIndexPointer == nullptr)
        {
            OutErrorText = FText::FromString(
                TEXT("The drop target no longer exists."));

            return false;
        }

        const int32 ComponentIndex = *ComponentIndexPointer;
        const int32 TargetIndex = *TargetIndexPointer;

        if (ComponentIndex == 0)
        {
            OutErrorText = FText::FromString(
                TEXT("The main component cannot be moved."));

            return false;
        }

        TArray<FGuid> SubtreeIds;

        if (!CollectSubtreeIdsPreorder(
                ComponentId,
                Maps,
                SubtreeIds,
                OutErrorText))
        {
            return false;
        }

        TSet<FGuid> SubtreeIdSet;
        SubtreeIdSet.Reserve(SubtreeIds.Num());

        for (const FGuid& SubtreeId : SubtreeIds)
        {
            SubtreeIdSet.Add(SubtreeId);
        }

        if (SubtreeIdSet.Contains(TargetComponentId))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "A component cannot be dropped onto or beside "
                    "itself or one of its descendants."));

            return false;
        }

        const FTGComponentConfig& MovingComponent =
            WorkingScenario.Components[ComponentIndex];

        const FGuid* OldParentIdPointer =
            Maps.IdByNormalizedName.Find(
                NormalizeName(
                    MovingComponent.ParentComponentName));

        if (OldParentIdPointer == nullptr)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The selected component does not have a valid "
                    "current parent."));

            return false;
        }

        const FGuid OldParentId = *OldParentIdPointer;

        TArray<FGuid>* OldSiblings =
            Maps.ChildrenByParentId.Find(OldParentId);

        if (OldSiblings == nullptr ||
            OldSiblings->RemoveSingle(ComponentId) != 1)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The selected component could not be removed "
                    "from its current sibling list."));

            return false;
        }

        FGuid DestinationParentId;
        int32 DestinationInsertIndex = INDEX_NONE;

        if (DropZone == ETGComponentTreeDropZone::Onto)
        {
            DestinationParentId = TargetComponentId;

            TArray<FGuid>& DestinationChildren =
                Maps.ChildrenByParentId.FindOrAdd(
                    DestinationParentId);

            DestinationInsertIndex = DestinationChildren.Num();
        }
        else
        {
            if (TargetIndex == 0)
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "A component cannot be placed before or after "
                        "the main component. Drop onto it instead."));

                return false;
            }

            const FTGComponentConfig& TargetComponent =
                WorkingScenario.Components[TargetIndex];

            const FGuid* DestinationParentIdPointer =
                Maps.IdByNormalizedName.Find(
                    NormalizeName(
                        TargetComponent.ParentComponentName));

            if (DestinationParentIdPointer == nullptr)
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "The drop target does not have a valid parent."));

                return false;
            }

            DestinationParentId = *DestinationParentIdPointer;

            TArray<FGuid>& DestinationSiblings =
                Maps.ChildrenByParentId.FindOrAdd(
                    DestinationParentId);

            const int32 TargetSiblingIndex =
                DestinationSiblings.IndexOfByKey(
                    TargetComponentId);

            if (TargetSiblingIndex == INDEX_NONE)
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "The drop target could not be found in its "
                        "sibling list."));

                return false;
            }

            DestinationInsertIndex = TargetSiblingIndex;

            if (DropZone == ETGComponentTreeDropZone::After)
            {
                ++DestinationInsertIndex;
            }
        }

        if (SubtreeIdSet.Contains(DestinationParentId))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "A component cannot be parented beneath one of "
                    "its own descendants."));

            return false;
        }

        TArray<FGuid>& DestinationChildren =
            Maps.ChildrenByParentId.FindOrAdd(
                DestinationParentId);

        DestinationInsertIndex = FMath::Clamp(
            DestinationInsertIndex,
            0,
            DestinationChildren.Num());

        DestinationChildren.Insert(
            ComponentId,
            DestinationInsertIndex);

        const int32* DestinationParentIndex =
            Maps.IndexById.Find(DestinationParentId);

        if (DestinationParentIndex == nullptr ||
            !WorkingScenario.Components.IsValidIndex(
                *DestinationParentIndex))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The destination parent could not be resolved."));

            return false;
        }

        WorkingScenario.Components[ComponentIndex]
            .ParentComponentName =
                WorkingScenario.Components[
                    *DestinationParentIndex]
                    .Name;

        TArray<FTGComponentConfig> FlattenedComponents;

        if (!FlattenHierarchyPreorder(
                WorkingScenario.Components,
                Maps,
                FlattenedComponents,
                OutErrorText))
        {
            return false;
        }

        const bool bParentChanged =
            OldParentId != DestinationParentId;

        const bool bOrderChanged =
            !HaveSameComponentOrder(
                Scenario.Components,
                FlattenedComponents);

        OutChanged = bParentChanged || bOrderChanged;

        if (OutChanged)
        {
            WorkingScenario.Components =
                MoveTemp(FlattenedComponents);

            Scenario = MoveTemp(WorkingScenario);
        }

        return true;
    }

    bool IsComponentNameUsed(
        const TArray<FTGComponentConfig>& Components,
        const FString& Candidate)
    {
        const FString NormalizedCandidate =
            NormalizeName(Candidate);

        for (const FTGComponentConfig& Component : Components)
        {
            if (NormalizeName(Component.Name) == NormalizedCandidate)
            {
                return true;
            }
        }

        return false;
    }

    FString MakeUniqueCopiedComponentName(
        const TArray<FTGComponentConfig>& Components,
        const FString& OriginalName)
    {
        FString TrimmedOriginalName = OriginalName;
        TrimmedOriginalName.TrimStartAndEndInline();

        if (TrimmedOriginalName.IsEmpty())
        {
            TrimmedOriginalName = TEXT("Component");
        }

        FString Candidate =
            FString::Printf(
                TEXT("%s Copy"),
                *TrimmedOriginalName);

        if (!IsComponentNameUsed(Components, Candidate))
        {
            return Candidate;
        }

        for (int32 Number = 2; Number < MAX_int32; ++Number)
        {
            Candidate = FString::Printf(
                TEXT("%s Copy %d"),
                *TrimmedOriginalName,
                Number);

            if (!IsComponentNameUsed(Components, Candidate))
            {
                return Candidate;
            }
        }

        return FString::Printf(
            TEXT("%s Copy %s"),
            *TrimmedOriginalName,
            *FGuid::NewGuid().ToString(
                EGuidFormats::Digits));
    }

    bool DuplicateClipboardSubtree(
        FTGSimulationScenario& Scenario,
        const TArray<FTGComponentConfig>& CopiedComponents,
        const FGuid& DestinationParentId,
        FGuid& OutPastedRootComponentId,
        FText& OutErrorText)
    {
        OutPastedRootComponentId.Invalidate();
        OutErrorText = FText::GetEmpty();

        if (CopiedComponents.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT("The copied component subtree is empty."));

            return false;
        }

        FTGSimulationScenario WorkingScenario = Scenario;

        FHierarchyMaps ExistingMaps;

        if (!BuildHierarchyMaps(
                WorkingScenario.Components,
                ExistingMaps,
                OutErrorText))
        {
            return false;
        }

        const int32* DestinationParentIndex =
            ExistingMaps.IndexById.Find(DestinationParentId);

        if (DestinationParentIndex == nullptr ||
            !WorkingScenario.Components.IsValidIndex(
                *DestinationParentIndex))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "The selected paste destination no longer exists."));

            return false;
        }

        const FString DestinationParentName =
            WorkingScenario.Components[*DestinationParentIndex].Name;

        TMap<FString, FString> NewNameByOldNormalizedName;
        TArray<FTGComponentConfig> DuplicatedComponents;
        DuplicatedComponents.Reserve(CopiedComponents.Num());

        for (const FTGComponentConfig& SourceComponent
             : CopiedComponents)
        {
            FTGComponentConfig DuplicatedComponent =
                SourceComponent;

            const FString NewName =
                MakeUniqueCopiedComponentName(
                    WorkingScenario.Components,
                    SourceComponent.Name);

            DuplicatedComponent.ComponentId = FGuid::NewGuid();
            DuplicatedComponent.Name = NewName;

            for (FTGJointDofConfig& Dof
                 : DuplicatedComponent.DegreesOfFreedom)
            {
                Dof.DofId = FGuid::NewGuid();
            }

            NewNameByOldNormalizedName.Add(
                NormalizeName(SourceComponent.Name),
                NewName);

            WorkingScenario.Components.Add(
                DuplicatedComponent);

            DuplicatedComponents.Add(
                DuplicatedComponent);
        }

        for (int32 Index = 0;
             Index < DuplicatedComponents.Num();
             ++Index)
        {
            FTGComponentConfig& DuplicatedComponent =
                DuplicatedComponents[Index];

            if (Index == 0)
            {
                DuplicatedComponent.ParentComponentName =
                    DestinationParentName;

                OutPastedRootComponentId =
                    DuplicatedComponent.ComponentId;
            }
            else
            {
                const FString SourceParentKey =
                    NormalizeName(
                        CopiedComponents[Index]
                            .ParentComponentName);

                const FString* NewParentName =
                    NewNameByOldNormalizedName.Find(
                        SourceParentKey);

                if (NewParentName == nullptr)
                {
                    OutErrorText = FText::FromString(
                        FString::Printf(
                            TEXT(
                                "The copied parent of component '%s' "
                                "could not be resolved."),
                            *CopiedComponents[Index].Name));

                    OutPastedRootComponentId.Invalidate();
                    return false;
                }

                DuplicatedComponent.ParentComponentName =
                    *NewParentName;
            }

            const int32 WorkingIndex =
                WorkingScenario.Components.Num() -
                DuplicatedComponents.Num() +
                Index;

            WorkingScenario.Components[WorkingIndex] =
                DuplicatedComponent;
        }

        FHierarchyMaps UpdatedMaps;

        if (!BuildHierarchyMaps(
                WorkingScenario.Components,
                UpdatedMaps,
                OutErrorText))
        {
            OutPastedRootComponentId.Invalidate();
            return false;
        }

        TArray<FTGComponentConfig> FlattenedComponents;

        if (!FlattenHierarchyPreorder(
                WorkingScenario.Components,
                UpdatedMaps,
                FlattenedComponents,
                OutErrorText))
        {
            OutPastedRootComponentId.Invalidate();
            return false;
        }

        WorkingScenario.Components =
            MoveTemp(FlattenedComponents);

        Scenario = MoveTemp(WorkingScenario);
        return true;
    }

    bool IsFiniteVector(const FVector& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    bool IsFiniteQuaternion(const FQuat& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z) &&
            FMath::IsFinite(Value.W);
    }

    FTGComponentRootTransformEdit MakeRootTransformEdit(
        const FTGComponentConfig& Component)
    {
        FTGComponentRootTransformEdit Transform;

        Transform.OriginInBodyMeters =
            Component.OriginInBodyMeters;

        Transform.ComponentToBodyOrientation =
            Component.ComponentToBodyOrientation;

        return Transform;
    }

    bool HaveSameRootTransform(
        const FTGComponentConfig& Component,
        const FTGComponentRootTransformEdit& Transform)
    {
        if (
            Component.OriginInBodyMeters.X !=
                Transform.OriginInBodyMeters.X ||
            Component.OriginInBodyMeters.Y !=
                Transform.OriginInBodyMeters.Y ||
            Component.OriginInBodyMeters.Z !=
                Transform.OriginInBodyMeters.Z)
        {
            return false;
        }

        const FQuat& ExistingOrientation =
            Component.ComponentToBodyOrientation;

        const FQuat& ProposedOrientation =
            Transform.ComponentToBodyOrientation;

        constexpr double OrientationTolerance = 1.0e-12;

        const bool bSameComponents =
            FMath::Abs(
                ExistingOrientation.X - ProposedOrientation.X) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Y - ProposedOrientation.Y) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Z - ProposedOrientation.Z) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.W - ProposedOrientation.W) <=
                    OrientationTolerance;

        const bool bSameRotationOppositeSign =
            FMath::Abs(
                ExistingOrientation.X + ProposedOrientation.X) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Y + ProposedOrientation.Y) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Z + ProposedOrientation.Z) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.W + ProposedOrientation.W) <=
                    OrientationTolerance;

        return bSameComponents || bSameRotationOppositeSign;
    }

    void ApplyRootTransform(
        FTGComponentConfig& Component,
        const FTGComponentRootTransformEdit& Transform)
    {
        Component.OriginInBodyMeters =
            Transform.OriginInBodyMeters;

        Component.ComponentToBodyOrientation =
            Transform.ComponentToBodyOrientation;
    }

    bool ValidateRootTransform(
        const FTGComponentRootTransformEdit& Transform,
        FTGComponentRootTransformEdit& OutValidatedTransform,
        FText& OutWarningText,
        FText& OutErrorText)
    {
        OutValidatedTransform = Transform;
        OutWarningText = FText::GetEmpty();
        OutErrorText = FText::GetEmpty();

        if (!IsFiniteVector(Transform.OriginInBodyMeters))
        {
            OutErrorText = FText::FromString(
                TEXT("Origin in B must be finite."));

            return false;
        }

        if (!IsFiniteQuaternion(
                Transform.ComponentToBodyOrientation))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Component-to-B orientation must contain "
                    "only finite values."));

            return false;
        }

        const double SquaredNorm =
            Transform.ComponentToBodyOrientation.SizeSquared();

        if (SquaredNorm <= 1.0e-12)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Component-to-B orientation must be a "
                    "nonzero quaternion."));

            return false;
        }

        const double Norm = FMath::Sqrt(SquaredNorm);

        if (FMath::Abs(Norm - 1.0) > 1.0e-6)
        {
            OutWarningText = FText::FromString(
                TEXT(
                    "Component-to-B orientation is not unit length. "
                    "It is stored as entered and will be normalized "
                    "once by the backend conversion layer."));
        }

        return true;
    }

    int32 FindEarlierParentIndexByName(
        const TArray<FTGComponentConfig>& Components,
        int32 ChildIndex)
    {
        if (!Components.IsValidIndex(ChildIndex) || ChildIndex <= 0)
        {
            return INDEX_NONE;
        }

        const FString ParentKey =
            NormalizeName(
                Components[ChildIndex].ParentComponentName);

        if (ParentKey.IsEmpty())
        {
            return INDEX_NONE;
        }

        for (int32 Index = 0; Index < ChildIndex; ++Index)
        {
            if (NormalizeName(Components[Index].Name) == ParentKey)
            {
                return Index;
            }
        }

        return INDEX_NONE;
    }

    FTGComponentChildConnectionEdit MakeChildConnectionEdit(
        const FTGComponentConfig& Component)
    {
        FTGComponentChildConnectionEdit Connection;

        Connection.ParentComponentName =
            Component.ParentComponentName;

        Connection.ParentAnchorMeters =
            Component.ParentAnchorMeters;

        Connection.ChildAnchorMeters =
            Component.ChildAnchorMeters;

        Connection.ChildToParentZeroOrientation =
            Component.ChildToParentZeroOrientation;

        return Connection;
    }

    bool HaveSameChildConnection(
        const FTGComponentConfig& Component,
        const FTGComponentChildConnectionEdit& Connection)
    {
        if (
            Component.ParentAnchorMeters.X !=
                Connection.ParentAnchorMeters.X ||
            Component.ParentAnchorMeters.Y !=
                Connection.ParentAnchorMeters.Y ||
            Component.ParentAnchorMeters.Z !=
                Connection.ParentAnchorMeters.Z ||
            Component.ChildAnchorMeters.X !=
                Connection.ChildAnchorMeters.X ||
            Component.ChildAnchorMeters.Y !=
                Connection.ChildAnchorMeters.Y ||
            Component.ChildAnchorMeters.Z !=
                Connection.ChildAnchorMeters.Z)
        {
            return false;
        }

        const FQuat& ExistingOrientation =
            Component.ChildToParentZeroOrientation;

        const FQuat& ProposedOrientation =
            Connection.ChildToParentZeroOrientation;

        constexpr double OrientationTolerance = 1.0e-12;

        const bool bSameComponents =
            FMath::Abs(
                ExistingOrientation.X - ProposedOrientation.X) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Y - ProposedOrientation.Y) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Z - ProposedOrientation.Z) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.W - ProposedOrientation.W) <=
                    OrientationTolerance;

        const bool bSameRotationOppositeSign =
            FMath::Abs(
                ExistingOrientation.X + ProposedOrientation.X) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Y + ProposedOrientation.Y) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.Z + ProposedOrientation.Z) <=
                    OrientationTolerance &&
            FMath::Abs(
                ExistingOrientation.W + ProposedOrientation.W) <=
                    OrientationTolerance;

        return bSameComponents || bSameRotationOppositeSign;
    }

    void ApplyChildConnection(
        FTGComponentConfig& Component,
        const FTGComponentChildConnectionEdit& Connection)
    {
        Component.ParentAnchorMeters =
            Connection.ParentAnchorMeters;

        Component.ChildAnchorMeters =
            Connection.ChildAnchorMeters;

        Component.ChildToParentZeroOrientation =
            Connection.ChildToParentZeroOrientation;
    }

    bool ValidateChildConnection(
        const FTGComponentChildConnectionEdit& Connection,
        FTGComponentChildConnectionEdit& OutValidatedConnection,
        FText& OutWarningText,
        FText& OutErrorText)
    {
        OutValidatedConnection = Connection;
        OutWarningText = FText::GetEmpty();
        OutErrorText = FText::GetEmpty();

        if (!IsFiniteVector(Connection.ParentAnchorMeters))
        {
            OutErrorText = FText::FromString(
                TEXT("Parent anchor must be finite."));

            return false;
        }

        if (!IsFiniteVector(Connection.ChildAnchorMeters))
        {
            OutErrorText = FText::FromString(
                TEXT("Child anchor must be finite."));

            return false;
        }

        if (!IsFiniteQuaternion(
                Connection.ChildToParentZeroOrientation))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Child-to-parent zero orientation must contain "
                    "only finite values."));

            return false;
        }

        const double SquaredNorm =
            Connection.ChildToParentZeroOrientation.SizeSquared();

        if (SquaredNorm <= 1.0e-12)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Child-to-parent zero orientation must be a "
                    "nonzero quaternion."));

            return false;
        }

        const double Norm = FMath::Sqrt(SquaredNorm);

        if (FMath::Abs(Norm - 1.0) > 1.0e-6)
        {
            OutWarningText = FText::FromString(
                TEXT(
                    "Child-to-parent zero orientation is not unit "
                    "length. It is stored as entered and will be "
                    "normalized once by the backend conversion layer."));
        }

        return true;
    }


    bool IsFiniteColor(const FLinearColor& Value)
    {
        return
            FMath::IsFinite(Value.R) &&
            FMath::IsFinite(Value.G) &&
            FMath::IsFinite(Value.B) &&
            FMath::IsFinite(Value.A);
    }

    FString NormalizeFilePath(const FString& Path)
    {
        FString Result = Path;
        Result.TrimStartAndEndInline();

        if (!Result.IsEmpty())
        {
            FPaths::NormalizeFilename(Result);
        }

        return Result;
    }

    bool IsSupportedTextureExtension(const FString& FilePath)
    {
        const FString Extension =
            FPaths::GetExtension(FilePath, false).ToLower();

        return
            Extension == TEXT("png") ||
            Extension == TEXT("jpg") ||
            Extension == TEXT("jpeg");
    }

    bool ValidateTextureFilePath(
        const FString& SourcePath,
        bool bRequired,
        const TCHAR* FieldLabel,
        FString& OutNormalizedPath,
        FText& OutErrorText)
    {
        OutNormalizedPath = NormalizeFilePath(SourcePath);

        if (OutNormalizedPath.IsEmpty())
        {
            if (bRequired)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("%s texture file is required in Textured mode."),
                        FieldLabel));

                return false;
            }

            return true;
        }

        if (!IsSupportedTextureExtension(OutNormalizedPath))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "%s texture must be a PNG, JPG, or JPEG file."),
                    FieldLabel));

            return false;
        }

        if (!FPaths::FileExists(OutNormalizedPath))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("%s texture file does not exist: %s"),
                    FieldLabel,
                    *OutNormalizedPath));

            return false;
        }

        return true;
    }

    FTGComponentVisualAppearanceEdit MakeVisualAppearanceEdit(
        const FTGComponentConfig& Component)
    {
        const FTGComponentVisualConfig& Visual = Component.Visual;

        FTGComponentVisualAppearanceEdit Appearance;
        Appearance.GeometrySource = Visual.GeometrySource;
        Appearance.PrimitiveType = Visual.PrimitiveType;
        Appearance.BoxDimensionsMeters = Visual.BoxDimensionsMeters;
        Appearance.SphereRadiusMeters = Visual.SphereRadiusMeters;
        Appearance.CylinderRadiusMeters = Visual.CylinderRadiusMeters;
        Appearance.CylinderLengthMeters = Visual.CylinderLengthMeters;
        Appearance.StlFilePath = Visual.StlFilePath;
        Appearance.StlLengthUnit = Visual.StlLengthUnit;
        Appearance.SurfaceAppearanceMode = Visual.SurfaceAppearanceMode;
        Appearance.SolidColor = Visual.DisplayColor;
        Appearance.BaseColorTint = Visual.BaseColorTint;
        Appearance.BaseColorTextureFilePath =
            Visual.BaseColorTextureFilePath;
        Appearance.NormalTextureFilePath =
            Visual.NormalTextureFilePath;
        Appearance.RoughnessTextureFilePath =
            Visual.RoughnessTextureFilePath;
        Appearance.MetallicTextureFilePath =
            Visual.MetallicTextureFilePath;
        Appearance.bVisible = Visual.bVisible;

        return Appearance;
    }

    bool HaveSameVisualAppearance(
        const FTGComponentConfig& Component,
        const FTGComponentVisualAppearanceEdit& Appearance)
    {
        const FTGComponentVisualConfig& Visual = Component.Visual;

        return
            Visual.GeometrySource == Appearance.GeometrySource &&
            Visual.PrimitiveType == Appearance.PrimitiveType &&
            Visual.BoxDimensionsMeters.X == Appearance.BoxDimensionsMeters.X &&
            Visual.BoxDimensionsMeters.Y == Appearance.BoxDimensionsMeters.Y &&
            Visual.BoxDimensionsMeters.Z == Appearance.BoxDimensionsMeters.Z &&
            Visual.SphereRadiusMeters == Appearance.SphereRadiusMeters &&
            Visual.CylinderRadiusMeters == Appearance.CylinderRadiusMeters &&
            Visual.CylinderLengthMeters == Appearance.CylinderLengthMeters &&
            Visual.StlFilePath == Appearance.StlFilePath &&
            Visual.StlLengthUnit == Appearance.StlLengthUnit &&
            Visual.StlRecenterMode == ETGStlRecenterMode::KeepImportedOrigin &&
            Visual.VisualOffsetMeters == FVector::ZeroVector &&
            Visual.VisualOrientation == FQuat::Identity &&
            Visual.VisualScale == FVector::OneVector &&
            Visual.SurfaceAppearanceMode == Appearance.SurfaceAppearanceMode &&
            Visual.DisplayColor == Appearance.SolidColor &&
            Visual.BaseColorTint == Appearance.BaseColorTint &&
            Visual.BaseColorTextureFilePath ==
                Appearance.BaseColorTextureFilePath &&
            Visual.NormalTextureFilePath ==
                Appearance.NormalTextureFilePath &&
            Visual.RoughnessTextureFilePath ==
                Appearance.RoughnessTextureFilePath &&
            Visual.MetallicTextureFilePath ==
                Appearance.MetallicTextureFilePath &&
            Visual.bVisible == Appearance.bVisible;
    }

    void ApplyVisualAppearance(
        FTGComponentConfig& Component,
        const FTGComponentVisualAppearanceEdit& Appearance)
    {
        FTGComponentVisualConfig& Visual = Component.Visual;

        Visual.GeometrySource = Appearance.GeometrySource;
        Visual.PrimitiveType = Appearance.PrimitiveType;
        Visual.BoxDimensionsMeters = Appearance.BoxDimensionsMeters;
        Visual.SphereRadiusMeters = Appearance.SphereRadiusMeters;
        Visual.CylinderRadiusMeters = Appearance.CylinderRadiusMeters;
        Visual.CylinderLengthMeters = Appearance.CylinderLengthMeters;
        Visual.StlFilePath = Appearance.StlFilePath;
        Visual.StlLengthUnit = Appearance.StlLengthUnit;

        // The imported geometry itself defines its origin, orientation and
        // dimensions. Only STL length-unit conversion is permitted here.
        Visual.StlRecenterMode = ETGStlRecenterMode::KeepImportedOrigin;
        Visual.VisualOffsetMeters = FVector::ZeroVector;
        Visual.VisualOrientation = FQuat::Identity;
        Visual.VisualScale = FVector::OneVector;

        Visual.SurfaceAppearanceMode = Appearance.SurfaceAppearanceMode;
        Visual.DisplayColor = Appearance.SolidColor;
        Visual.BaseColorTint = Appearance.BaseColorTint;
        Visual.BaseColorTextureFilePath =
            Appearance.BaseColorTextureFilePath;
        Visual.NormalTextureFilePath =
            Appearance.NormalTextureFilePath;
        Visual.RoughnessTextureFilePath =
            Appearance.RoughnessTextureFilePath;
        Visual.MetallicTextureFilePath =
            Appearance.MetallicTextureFilePath;
        Visual.bVisible = Appearance.bVisible;
    }

    bool ValidateVisualAppearance(
        const FTGComponentVisualAppearanceEdit& Appearance,
        FTGComponentVisualAppearanceEdit& OutValidatedAppearance,
        FText& OutWarningText,
        FText& OutErrorText)
    {
        OutValidatedAppearance = Appearance;
        OutWarningText = FText::GetEmpty();
        OutErrorText = FText::GetEmpty();

        if (
            Appearance.GeometrySource !=
                ETGComponentGeometrySource::Primitive &&
            Appearance.GeometrySource !=
                ETGComponentGeometrySource::CustomStl)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Geometry source must be Standard Primitive or "
                    "Custom STL. No Geometry is not supported."));

            return false;
        }

        if (Appearance.GeometrySource == ETGComponentGeometrySource::Primitive)
        {
            switch (Appearance.PrimitiveType)
            {
                case ETGPrimitiveGeometryType::Box:
                    if (
                        !IsFiniteVector(Appearance.BoxDimensionsMeters) ||
                        Appearance.BoxDimensionsMeters.X <= 0.0 ||
                        Appearance.BoxDimensionsMeters.Y <= 0.0 ||
                        Appearance.BoxDimensionsMeters.Z <= 0.0)
                    {
                        OutErrorText = FText::FromString(
                            TEXT(
                                "All box dimensions must be finite and "
                                "greater than zero."));

                        return false;
                    }
                    break;

                case ETGPrimitiveGeometryType::Sphere:
                    if (
                        !FMath::IsFinite(Appearance.SphereRadiusMeters) ||
                        Appearance.SphereRadiusMeters <= 0.0)
                    {
                        OutErrorText = FText::FromString(
                            TEXT(
                                "Sphere radius must be finite and greater "
                                "than zero."));

                        return false;
                    }
                    break;

                case ETGPrimitiveGeometryType::Cylinder:
                    if (
                        !FMath::IsFinite(Appearance.CylinderRadiusMeters) ||
                        Appearance.CylinderRadiusMeters <= 0.0)
                    {
                        OutErrorText = FText::FromString(
                            TEXT(
                                "Cylinder radius must be finite and greater "
                                "than zero."));

                        return false;
                    }

                    if (
                        !FMath::IsFinite(Appearance.CylinderLengthMeters) ||
                        Appearance.CylinderLengthMeters <= 0.0)
                    {
                        OutErrorText = FText::FromString(
                            TEXT(
                                "Cylinder length must be finite and greater "
                                "than zero."));

                        return false;
                    }
                    break;

                default:
                    OutErrorText = FText::FromString(
                        TEXT("Primitive geometry type is invalid."));
                    return false;
            }
        }
        else
        {
            OutValidatedAppearance.StlFilePath =
                NormalizeFilePath(Appearance.StlFilePath);

            if (OutValidatedAppearance.StlFilePath.IsEmpty())
            {
                OutErrorText = FText::FromString(
                    TEXT("Custom STL geometry requires a selected STL file."));

                return false;
            }

            if (!FPaths::GetExtension(
                    OutValidatedAppearance.StlFilePath,
                    false).Equals(TEXT("stl"), ESearchCase::IgnoreCase))
            {
                OutErrorText = FText::FromString(
                    TEXT("Custom geometry file must use the .stl extension."));

                return false;
            }

            if (!FPaths::FileExists(OutValidatedAppearance.StlFilePath))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT("STL file does not exist: %s"),
                        *OutValidatedAppearance.StlFilePath));

                return false;
            }

            if (
                Appearance.StlLengthUnit != ETGStlLengthUnit::Millimeters &&
                Appearance.StlLengthUnit != ETGStlLengthUnit::Centimeters &&
                Appearance.StlLengthUnit != ETGStlLengthUnit::Meters)
            {
                OutErrorText = FText::FromString(
                    TEXT("STL length unit is invalid."));

                return false;
            }
        }

        if (
            Appearance.SurfaceAppearanceMode !=
                ETGComponentSurfaceAppearanceMode::SolidColor &&
            Appearance.SurfaceAppearanceMode !=
                ETGComponentSurfaceAppearanceMode::Textured)
        {
            OutErrorText = FText::FromString(
                TEXT("Surface appearance mode is invalid."));

            return false;
        }

        if (!IsFiniteColor(Appearance.SolidColor))
        {
            OutErrorText = FText::FromString(
                TEXT("Solid color must contain only finite values."));

            return false;
        }

        if (!IsFiniteColor(Appearance.BaseColorTint))
        {
            OutErrorText = FText::FromString(
                TEXT("Base-color tint must contain only finite values."));

            return false;
        }

        if (
            Appearance.SurfaceAppearanceMode ==
            ETGComponentSurfaceAppearanceMode::Textured)
        {
            if (!ValidateTextureFilePath(
                    Appearance.BaseColorTextureFilePath,
                    true,
                    TEXT("Base color"),
                    OutValidatedAppearance.BaseColorTextureFilePath,
                    OutErrorText))
            {
                return false;
            }

            if (!ValidateTextureFilePath(
                    Appearance.NormalTextureFilePath,
                    false,
                    TEXT("Normal"),
                    OutValidatedAppearance.NormalTextureFilePath,
                    OutErrorText))
            {
                return false;
            }

            if (!ValidateTextureFilePath(
                    Appearance.RoughnessTextureFilePath,
                    false,
                    TEXT("Roughness"),
                    OutValidatedAppearance.RoughnessTextureFilePath,
                    OutErrorText))
            {
                return false;
            }

            if (!ValidateTextureFilePath(
                    Appearance.MetallicTextureFilePath,
                    false,
                    TEXT("Metallic"),
                    OutValidatedAppearance.MetallicTextureFilePath,
                    OutErrorText))
            {
                return false;
            }
        }
        else
        {
            // Keep dormant texture selections available if the user switches
            // back to Textured mode later, but normalize nonempty paths.
            OutValidatedAppearance.BaseColorTextureFilePath =
                NormalizeFilePath(Appearance.BaseColorTextureFilePath);
            OutValidatedAppearance.NormalTextureFilePath =
                NormalizeFilePath(Appearance.NormalTextureFilePath);
            OutValidatedAppearance.RoughnessTextureFilePath =
                NormalizeFilePath(Appearance.RoughnessTextureFilePath);
            OutValidatedAppearance.MetallicTextureFilePath =
                NormalizeFilePath(Appearance.MetallicTextureFilePath);
        }

        return true;
    }

    int32 FindDegreeOfFreedomIndexById(
        const FTGComponentConfig& Component,
        const FGuid& DegreeOfFreedomId)
    {
        if (!DegreeOfFreedomId.IsValid())
        {
            return INDEX_NONE;
        }

        for (
            int32 Index = 0;
            Index < Component.DegreesOfFreedom.Num();
            ++Index)
        {
            if (
                Component.DegreesOfFreedom[Index].DofId ==
                DegreeOfFreedomId)
            {
                return Index;
            }
        }

        return INDEX_NONE;
    }

    int32 GetFlatCoordinateIndex(
        const TArray<FTGComponentConfig>& Components,
        int32 ComponentIndex,
        int32 DegreeOfFreedomIndex)
    {
        if (
            !Components.IsValidIndex(ComponentIndex) ||
            !Components[ComponentIndex]
                .DegreesOfFreedom
                .IsValidIndex(DegreeOfFreedomIndex))
        {
            return INDEX_NONE;
        }

        int32 FlatIndex = 0;

        for (int32 Index = 0; Index < ComponentIndex; ++Index)
        {
            FlatIndex +=
                Components[Index].DegreesOfFreedom.Num();
        }

        return FlatIndex + DegreeOfFreedomIndex;
    }

    void AppendWarning(
        FText& InOutWarningText,
        const FString& Warning)
    {
        if (Warning.IsEmpty())
        {
            return;
        }

        if (InOutWarningText.IsEmpty())
        {
            InOutWarningText = FText::FromString(Warning);
            return;
        }

        InOutWarningText = FText::FromString(
            InOutWarningText.ToString() +
            TEXT("\n") +
            Warning);
    }

    double CanonicalCoordinateToDisplay(
        const ETGJointMotionType MotionType,
        const double Value)
    {
        return MotionType == ETGJointMotionType::Rotation
            ? FMath::RadiansToDegrees(Value)
            : Value;
    }

    double DisplayCoordinateToCanonical(
        const ETGJointMotionType MotionType,
        const double Value)
    {
        return MotionType == ETGJointMotionType::Rotation
            ? FMath::DegreesToRadians(Value)
            : Value;
    }

    FTGComponentDegreeOfFreedomEdit
        MakeCanonicalDegreeOfFreedomEdit(
            const FTGComponentDegreeOfFreedomEdit& DisplayEdit)
    {
        FTGComponentDegreeOfFreedomEdit CanonicalEdit =
            DisplayEdit;

        CanonicalEdit.InitialCoordinate =
            DisplayCoordinateToCanonical(
                DisplayEdit.MotionType,
                DisplayEdit.InitialCoordinate);
        CanonicalEdit.InitialRate =
            DisplayCoordinateToCanonical(
                DisplayEdit.MotionType,
                DisplayEdit.InitialRate);
        CanonicalEdit.MinimumCoordinate =
            DisplayCoordinateToCanonical(
                DisplayEdit.MotionType,
                DisplayEdit.MinimumCoordinate);
        CanonicalEdit.MaximumCoordinate =
            DisplayCoordinateToCanonical(
                DisplayEdit.MotionType,
                DisplayEdit.MaximumCoordinate);
        CanonicalEdit.MaximumAbsoluteRate =
            DisplayCoordinateToCanonical(
                DisplayEdit.MotionType,
                DisplayEdit.MaximumAbsoluteRate);

        return CanonicalEdit;
    }

    FTGComponentDegreeOfFreedomEdit
        MakeDegreeOfFreedomEdit(
            const FTGJointDofConfig& DegreeOfFreedom,
            int32 ArrayIndex,
            int32 FlatCoordinateIndex)
    {
        FTGComponentDegreeOfFreedomEdit Edit;

        Edit.DegreeOfFreedomId =
            DegreeOfFreedom.DofId;

        Edit.ArrayIndex = ArrayIndex;
        Edit.FlatCoordinateIndex =
            FlatCoordinateIndex;

        Edit.Name = DegreeOfFreedom.Name;
        Edit.MotionType =
            DegreeOfFreedom.MotionType;
        Edit.Axis = DegreeOfFreedom.Axis;

        Edit.InitialCoordinate =
            CanonicalCoordinateToDisplay(
                DegreeOfFreedom.MotionType,
                DegreeOfFreedom.InitialCoordinate);
        Edit.InitialRate =
            CanonicalCoordinateToDisplay(
                DegreeOfFreedom.MotionType,
                DegreeOfFreedom.InitialRate);

        Edit.bHasMinimumCoordinate =
            DegreeOfFreedom.bHasMinimumCoordinate;
        Edit.MinimumCoordinate =
            CanonicalCoordinateToDisplay(
                DegreeOfFreedom.MotionType,
                DegreeOfFreedom.MinimumCoordinate);

        Edit.bHasMaximumCoordinate =
            DegreeOfFreedom.bHasMaximumCoordinate;
        Edit.MaximumCoordinate =
            CanonicalCoordinateToDisplay(
                DegreeOfFreedom.MotionType,
                DegreeOfFreedom.MaximumCoordinate);

        Edit.MaximumAbsoluteRate =
            CanonicalCoordinateToDisplay(
                DegreeOfFreedom.MotionType,
                DegreeOfFreedom.MaximumAbsoluteRate);
        Edit.MaximumAbsoluteEffort =
            DegreeOfFreedom.MaximumAbsoluteEffort;

        return Edit;
    }

    bool HaveSameDegreeOfFreedom(
        const FTGJointDofConfig& Existing,
        const FTGComponentDegreeOfFreedomEdit& Proposed)
    {
        return
            Existing.Name == Proposed.Name &&
            Existing.MotionType == Proposed.MotionType &&
            Existing.Axis.X == Proposed.Axis.X &&
            Existing.Axis.Y == Proposed.Axis.Y &&
            Existing.Axis.Z == Proposed.Axis.Z &&
            Existing.InitialCoordinate ==
                Proposed.InitialCoordinate &&
            Existing.InitialRate ==
                Proposed.InitialRate &&
            Existing.bHasMinimumCoordinate ==
                Proposed.bHasMinimumCoordinate &&
            Existing.MinimumCoordinate ==
                Proposed.MinimumCoordinate &&
            Existing.bHasMaximumCoordinate ==
                Proposed.bHasMaximumCoordinate &&
            Existing.MaximumCoordinate ==
                Proposed.MaximumCoordinate &&
            Existing.MaximumAbsoluteRate ==
                Proposed.MaximumAbsoluteRate &&
            Existing.MaximumAbsoluteEffort ==
                Proposed.MaximumAbsoluteEffort;
    }

    void ApplyDegreeOfFreedom(
        FTGJointDofConfig& Target,
        const FTGComponentDegreeOfFreedomEdit& Source)
    {
        Target.Name = Source.Name;
        Target.MotionType = Source.MotionType;
        Target.Axis = Source.Axis;

        Target.InitialCoordinate =
            Source.InitialCoordinate;
        Target.InitialRate =
            Source.InitialRate;

        Target.bHasMinimumCoordinate =
            Source.bHasMinimumCoordinate;
        Target.MinimumCoordinate =
            Source.MinimumCoordinate;

        Target.bHasMaximumCoordinate =
            Source.bHasMaximumCoordinate;
        Target.MaximumCoordinate =
            Source.MaximumCoordinate;

        Target.MaximumAbsoluteRate =
            Source.MaximumAbsoluteRate;
        Target.MaximumAbsoluteEffort =
            Source.MaximumAbsoluteEffort;
    }

    bool ValidateDegreeOfFreedom(
        const FTGComponentConfig& Component,
        const FTGJointDofConfig& Existing,
        int32 ExistingArrayIndex,
        int32 ExistingFlatCoordinateIndex,
        const FTGComponentDegreeOfFreedomEdit& Proposed,
        FTGComponentDegreeOfFreedomEdit& OutValidated,
        FText& OutWarningText,
        FText& OutErrorText)
    {
        OutValidated = Proposed;
        OutValidated.DegreeOfFreedomId =
            Existing.DofId;
        OutValidated.ArrayIndex =
            ExistingArrayIndex;
        OutValidated.FlatCoordinateIndex =
            ExistingFlatCoordinateIndex;

        OutWarningText = FText::GetEmpty();
        OutErrorText = FText::GetEmpty();

        FString TrimmedName = Proposed.Name;
        TrimmedName.TrimStartAndEndInline();

        if (TrimmedName.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Degree-of-freedom name cannot be empty."));

            return false;
        }

        for (
            const FTGJointDofConfig& Other :
            Component.DegreesOfFreedom)
        {
            if (
                Other.DofId != Existing.DofId &&
                NormalizeName(Other.Name) ==
                    NormalizeName(TrimmedName))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "Degree-of-freedom name '%s' is "
                            "already used by this component."),
                        *TrimmedName));

                return false;
            }
        }

        OutValidated.Name = TrimmedName;

        if (
            Proposed.MotionType !=
                ETGJointMotionType::Rotation &&
            Proposed.MotionType !=
                ETGJointMotionType::Translation)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Degree-of-freedom motion type is invalid."));

            return false;
        }

        if (
            Component.DegreesOfFreedom.Num() >
            MaxTotalDegreesOfFreedomPerConnection)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "A rigid parent-child connection can contain at most "
                    "six independent degrees of freedom. Delete excess "
                    "degrees of freedom before editing this connection."));

            return false;
        }

        int32 OtherMatchingMotionTypeCount = 0;

        for (
            const FTGJointDofConfig& Other :
            Component.DegreesOfFreedom)
        {
            if (
                Other.DofId != Existing.DofId &&
                Other.MotionType == Proposed.MotionType)
            {
                ++OtherMatchingMotionTypeCount;
            }
        }

        const int32 MaximumMatchingMotionTypeCount =
            Proposed.MotionType == ETGJointMotionType::Rotation
                ? MaxRotationalDegreesOfFreedomPerConnection
                : MaxTranslationalDegreesOfFreedomPerConnection;

        if (
            OtherMatchingMotionTypeCount >=
            MaximumMatchingMotionTypeCount)
        {
            OutErrorText = FText::FromString(
                Proposed.MotionType == ETGJointMotionType::Rotation
                    ? TEXT(
                        "A rigid parent-child connection can contain at most "
                        "three independent rotational degrees of freedom.")
                    : TEXT(
                        "A rigid parent-child connection can contain at most "
                        "three independent translational degrees of freedom."));

            return false;
        }

        if (!IsFiniteVector(Proposed.Axis))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Degree-of-freedom axis must be finite."));

            return false;
        }

        const double AxisLength = Proposed.Axis.Length();

        if (
            !FMath::IsFinite(AxisLength) ||
            AxisLength <= 1.0e-12)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Degree-of-freedom axis must be nonzero."));

            return false;
        }

        if (FMath::Abs(AxisLength - 1.0) > 1.0e-6)
        {
            AppendWarning(
                OutWarningText,
                FString::Printf(
                    TEXT(
                        "Degree-of-freedom axis length is %.9g. "
                        "It is stored as entered and normalized "
                        "once by the kinematics/conversion layer."),
                    AxisLength));
        }

        if (!FMath::IsFinite(Proposed.InitialCoordinate))
        {
            OutErrorText = FText::FromString(
                TEXT("Initial coordinate must be finite."));

            return false;
        }

        if (!FMath::IsFinite(Proposed.InitialRate))
        {
            OutErrorText = FText::FromString(
                TEXT("Initial rate must be finite."));

            return false;
        }

        if (
            !FMath::IsFinite(
                Proposed.MaximumAbsoluteRate) ||
            Proposed.MaximumAbsoluteRate < 0.0)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Maximum absolute rate must be finite "
                    "and nonnegative."));

            return false;
        }

        if (
            FMath::Abs(Proposed.InitialRate) >
            Proposed.MaximumAbsoluteRate)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Initial rate cannot exceed the maximum "
                    "absolute rate."));

            return false;
        }

        if (
            !FMath::IsFinite(
                Proposed.MaximumAbsoluteEffort) ||
            Proposed.MaximumAbsoluteEffort < 0.0)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Maximum absolute effort must be finite "
                    "and nonnegative."));

            return false;
        }

        if (
            Proposed.bHasMinimumCoordinate &&
            !FMath::IsFinite(
                Proposed.MinimumCoordinate))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Enabled minimum coordinate must be finite."));

            return false;
        }

        if (
            Proposed.bHasMaximumCoordinate &&
            !FMath::IsFinite(
                Proposed.MaximumCoordinate))
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Enabled maximum coordinate must be finite."));

            return false;
        }

        if (
            Proposed.bHasMinimumCoordinate &&
            Proposed.bHasMaximumCoordinate &&
            Proposed.MinimumCoordinate >
                Proposed.MaximumCoordinate)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Minimum coordinate cannot exceed "
                    "maximum coordinate."));

            return false;
        }

        if (
            Proposed.bHasMinimumCoordinate &&
            Proposed.InitialCoordinate <
                Proposed.MinimumCoordinate)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Initial coordinate cannot be below "
                    "the enabled minimum coordinate."));

            return false;
        }

        if (
            Proposed.bHasMaximumCoordinate &&
            Proposed.InitialCoordinate >
                Proposed.MaximumCoordinate)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Initial coordinate cannot exceed "
                    "the enabled maximum coordinate."));

            return false;
        }

        if (Existing.MotionType != Proposed.MotionType)
        {
            AppendWarning(
                OutWarningText,
                TEXT(
                    "Degree-of-freedom motion type changed. "
                    "Review coordinate, rate, limit, and effort "
                    "values because their units and physical "
                    "meaning changed."));
        }

        return true;
    }

    FTGComponentPhysicalPropertiesEdit MakePhysicalPropertiesEdit(
        const FTGComponentConfig& Component)
    {
        FTGComponentPhysicalPropertiesEdit Properties;

        Properties.InitialMassKilograms =
            Component.InitialMassKilograms;

        Properties.MinimumMassKilograms =
            Component.MinimumMassKilograms;

        Properties.bVariableMass =
            Component.bVariableMass;

        Properties.LocalCenterOfMassMeters =
            Component.LocalCenterOfMassMeters;

        Properties.CentroidalInertia =
            Component.CentroidalInertia;

        return Properties;
    }

    bool HaveSamePhysicalProperties(
        const FTGComponentConfig& Component,
        const FTGComponentPhysicalPropertiesEdit& Properties)
    {
        const FTGSymmetricInertia& Existing =
            Component.CentroidalInertia;

        const FTGSymmetricInertia& Proposed =
            Properties.CentroidalInertia;

        return
            Component.InitialMassKilograms ==
                Properties.InitialMassKilograms &&
            Component.MinimumMassKilograms ==
                Properties.MinimumMassKilograms &&
            Component.bVariableMass ==
                Properties.bVariableMass &&
            Component.LocalCenterOfMassMeters.X ==
                Properties.LocalCenterOfMassMeters.X &&
            Component.LocalCenterOfMassMeters.Y ==
                Properties.LocalCenterOfMassMeters.Y &&
            Component.LocalCenterOfMassMeters.Z ==
                Properties.LocalCenterOfMassMeters.Z &&
            Existing.IxxKilogramMetersSquared ==
                Proposed.IxxKilogramMetersSquared &&
            Existing.IyyKilogramMetersSquared ==
                Proposed.IyyKilogramMetersSquared &&
            Existing.IzzKilogramMetersSquared ==
                Proposed.IzzKilogramMetersSquared &&
            Existing.IxyKilogramMetersSquared ==
                Proposed.IxyKilogramMetersSquared &&
            Existing.IxzKilogramMetersSquared ==
                Proposed.IxzKilogramMetersSquared &&
            Existing.IyzKilogramMetersSquared ==
                Proposed.IyzKilogramMetersSquared;
    }

    void ApplyPhysicalProperties(
        FTGComponentConfig& Component,
        const FTGComponentPhysicalPropertiesEdit& Properties)
    {
        Component.InitialMassKilograms =
            Properties.InitialMassKilograms;

        Component.MinimumMassKilograms =
            Properties.MinimumMassKilograms;

        Component.bVariableMass =
            Properties.bVariableMass;

        Component.LocalCenterOfMassMeters =
            Properties.LocalCenterOfMassMeters;

        Component.CentroidalInertia =
            Properties.CentroidalInertia;
    }

    bool ValidatePhysicalProperties(
        const FTGComponentPhysicalPropertiesEdit& Properties,
        FText& OutWarningText,
        FText& OutErrorText)
    {
        OutWarningText = FText::GetEmpty();
        OutErrorText = FText::GetEmpty();

        if (
            !FMath::IsFinite(Properties.InitialMassKilograms) ||
            Properties.InitialMassKilograms < 0.0)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Initial mass must be finite and "
                    "nonnegative."));

            return false;
        }

        if (
            !FMath::IsFinite(Properties.MinimumMassKilograms) ||
            Properties.MinimumMassKilograms < 0.0)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Minimum mass must be finite "
                    "and nonnegative."));

            return false;
        }

        if (
            Properties.MinimumMassKilograms >
                Properties.InitialMassKilograms)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Minimum mass cannot exceed "
                    "initial mass."));

            return false;
        }

        if (!IsFiniteVector(Properties.LocalCenterOfMassMeters))
        {
            OutErrorText = FText::FromString(
                TEXT("Local center of mass must be finite."));

            return false;
        }

        FVector PrincipalMoments;

        if (!UTGComponentTreeValidationLibrary::
                ValidateSymmetricInertia(
                    Properties.CentroidalInertia,
                    PrincipalMoments,
                    OutErrorText))
        {
            return false;
        }

        TArray<FString> Warnings;

        if (
            !Properties.bVariableMass &&
            !FMath::IsNearlyEqual(
                Properties.InitialMassKilograms,
                Properties.MinimumMassKilograms))
        {
            Warnings.Add(
                TEXT(
                    "Minimum mass differs from initial mass "
                    "although Variable Mass is disabled."));
        }

        if (!Warnings.IsEmpty())
        {
            OutWarningText = FText::FromString(
                FString::Join(Warnings, TEXT("\n")));
        }

        return true;
    }
}

void UTGComponentTreeEditorSession::ResetSession()
{
    UndoStack.Reset();
    RedoStack.Reset();
    Clipboard.Reset();
}

bool UTGComponentTreeEditorSession::CanUndo() const
{
    return !UndoStack.IsEmpty();
}

bool UTGComponentTreeEditorSession::CanRedo() const
{
    return !RedoStack.IsEmpty();
}

bool UTGComponentTreeEditorSession::HasClipboard() const
{
    return Clipboard.Mode !=
        ETGComponentTreeClipboardMode::Empty;
}

ETGComponentTreeClipboardMode
UTGComponentTreeEditorSession::GetClipboardMode() const
{
    return Clipboard.Mode;
}

bool UTGComponentTreeEditorSession::
    AddDefaultChildComponent(
        FTGSimulationScenario& Scenario,
        FGuid ParentComponentId,
        FGuid CurrentSelectionId,
        FGuid& OutComponentId,
        FText& OutErrorText)
{
    OutComponentId.Invalidate();
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const FTGSimulationScenario BeforeScenario = Scenario;

    if (!UTGComponentTreeEditingLibrary::
            AddDefaultChildComponent(
                Scenario,
                ParentComponentId,
                OutComponentId,
                OutErrorText))
    {
        return false;
    }

    RecordHistory(
        BeforeScenario,
        Scenario,
        CurrentSelectionId,
        OutComponentId,
        TEXT("Add component"));

    return true;
}

bool UTGComponentTreeEditorSession::RenameComponent(
    FTGSimulationScenario& Scenario,
    FGuid ComponentId,
    const FString& NewName,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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

    if (Scenario.Components[ComponentIndex].Name == TrimmedName)
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;

    if (!UTGComponentTreeEditingLibrary::RenameComponent(
            Scenario,
            ComponentId,
            NewName,
            OutErrorText))
    {
        return false;
    }

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Rename component"));

    return true;
}


bool UTGComponentTreeEditorSession::
    GetComponentPhysicalProperties(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FTGComponentPhysicalPropertiesEdit& OutProperties,
        FText& OutErrorText) const
{
    OutProperties = FTGComponentPhysicalPropertiesEdit();
    OutErrorText = FText::GetEmpty();

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    OutProperties =
        TGComponentTreeEditorSessionPrivate::
            MakePhysicalPropertiesEdit(
                Scenario.Components[ComponentIndex]);

    return true;
}

bool UTGComponentTreeEditorSession::
    ApplyComponentPhysicalProperties(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FTGComponentPhysicalPropertiesEdit& Properties,
        FText& OutWarningText,
        FText& OutErrorText)
{
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    if (!TGComponentTreeEditorSessionPrivate::
            ValidatePhysicalProperties(
                Properties,
                OutWarningText,
                OutErrorText))
    {
        return false;
    }

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    double TotalInitialMassKilograms = 0.0;
    double MinimumReachableMassKilograms = 0.0;
    bool bCanValidateAggregateMass = true;

    for (int32 Index = 0; Index < Scenario.Components.Num(); ++Index)
    {
        const FTGComponentConfig& Existing = Scenario.Components[Index];
        const double InitialMassKilograms = Index == ComponentIndex
            ? Properties.InitialMassKilograms
            : Existing.InitialMassKilograms;
        const double MinimumMassKilograms = Index == ComponentIndex
            ? Properties.MinimumMassKilograms
            : Existing.MinimumMassKilograms;
        const bool bVariableMass = Index == ComponentIndex
            ? Properties.bVariableMass
            : Existing.bVariableMass;

        if (
            !FMath::IsFinite(InitialMassKilograms) ||
            !FMath::IsFinite(MinimumMassKilograms) ||
            InitialMassKilograms < 0.0 ||
            MinimumMassKilograms < 0.0 ||
            MinimumMassKilograms > InitialMassKilograms)
        {
            bCanValidateAggregateMass = false;
            break;
        }

        TotalInitialMassKilograms += InitialMassKilograms;
        MinimumReachableMassKilograms += bVariableMass
            ? MinimumMassKilograms
            : InitialMassKilograms;
    }

    if (
        bCanValidateAggregateMass &&
        (!FMath::IsFinite(TotalInitialMassKilograms) ||
         TotalInitialMassKilograms <= 0.0))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Total initial spacecraft mass must be finite "
                "and greater than zero."));
        return false;
    }

    if (
        bCanValidateAggregateMass &&
        (!FMath::IsFinite(MinimumReachableMassKilograms) ||
         MinimumReachableMassKilograms <= 0.0))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Minimum reachable spacecraft mass must be finite "
                "and greater than zero."));
        return false;
    }

    if (TGComponentTreeEditorSessionPrivate::
            HaveSamePhysicalProperties(
                Scenario.Components[ComponentIndex],
                Properties))
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;

    TGComponentTreeEditorSessionPrivate::
        ApplyPhysicalProperties(
            Scenario.Components[ComponentIndex],
            Properties);

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Edit component physical properties"));

    return true;
}

bool UTGComponentTreeEditorSession::
    GetComponentRootTransform(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FTGComponentRootTransformEdit& OutTransform,
        FText& OutErrorText) const
{
    OutTransform = FTGComponentRootTransformEdit();
    OutErrorText = FText::GetEmpty();

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (ComponentIndex != 0)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Root transform fields belong only to "
                "component zero."));

        return false;
    }

    OutTransform =
        TGComponentTreeEditorSessionPrivate::
            MakeRootTransformEdit(
                Scenario.Components[ComponentIndex]);

    return true;
}

bool UTGComponentTreeEditorSession::
    ApplyComponentRootTransform(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FTGComponentRootTransformEdit& Transform,
        FText& OutWarningText,
        FText& OutErrorText)
{
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    FTGComponentRootTransformEdit ValidatedTransform;

    if (!TGComponentTreeEditorSessionPrivate::
            ValidateRootTransform(
                Transform,
                ValidatedTransform,
                OutWarningText,
                OutErrorText))
    {
        return false;
    }

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (ComponentIndex != 0)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Root transform fields belong only to "
                "component zero."));

        return false;
    }

    if (TGComponentTreeEditorSessionPrivate::
            HaveSameRootTransform(
                Scenario.Components[ComponentIndex],
                ValidatedTransform))
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;

    TGComponentTreeEditorSessionPrivate::
        ApplyRootTransform(
            Scenario.Components[ComponentIndex],
            ValidatedTransform);

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Edit main-component transform"));

    return true;
}

bool UTGComponentTreeEditorSession::
    GetComponentChildConnection(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FTGComponentChildConnectionEdit& OutConnection,
        FText& OutErrorText) const
{
    OutConnection = FTGComponentChildConnectionEdit();
    OutErrorText = FText::GetEmpty();

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "Child connection fields belong only to "
                "non-root components."));

        return false;
    }

    if (TGComponentTreeEditorSessionPrivate::
            FindEarlierParentIndexByName(
                Scenario.Components,
                ComponentIndex) == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected component must reference an existing "
                "parent appearing earlier in the component array."));

        return false;
    }

    OutConnection =
        TGComponentTreeEditorSessionPrivate::
            MakeChildConnectionEdit(
                Scenario.Components[ComponentIndex]);

    return true;
}

bool UTGComponentTreeEditorSession::
    ApplyComponentChildConnection(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FTGComponentChildConnectionEdit& Connection,
        FText& OutWarningText,
        FText& OutErrorText)
{
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    FTGComponentChildConnectionEdit ValidatedConnection;

    if (!TGComponentTreeEditorSessionPrivate::
            ValidateChildConnection(
                Connection,
                ValidatedConnection,
                OutWarningText,
                OutErrorText))
    {
        return false;
    }

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "Child connection fields belong only to "
                "non-root components."));

        return false;
    }

    if (TGComponentTreeEditorSessionPrivate::
            FindEarlierParentIndexByName(
                Scenario.Components,
                ComponentIndex) == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected component must reference an existing "
                "parent appearing earlier in the component array."));

        return false;
    }

    if (TGComponentTreeEditorSessionPrivate::
            HaveSameChildConnection(
                Scenario.Components[ComponentIndex],
                ValidatedConnection))
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;

    TGComponentTreeEditorSessionPrivate::
        ApplyChildConnection(
            Scenario.Components[ComponentIndex],
            ValidatedConnection);

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Edit child component connection"));

    return true;
}


bool UTGComponentTreeEditorSession::GetComponentVisualAppearance(
    const FTGSimulationScenario& Scenario,
    FGuid ComponentId,
    FTGComponentVisualAppearanceEdit& OutAppearance,
    FText& OutErrorText) const
{
    OutAppearance = FTGComponentVisualAppearanceEdit();
    OutErrorText = FText::GetEmpty();

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
                Scenario.Components,
                ComponentId);

    if (ComponentIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (
        Scenario.Components[ComponentIndex].Visual.GeometrySource ==
        ETGComponentGeometrySource::NoGeometry)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "This component uses No Geometry mode. "
                "Choose Standard Primitive or Custom STL before using "
                "the Component Visual Appearance editor."));

        return false;
    }

    OutAppearance =
        TGComponentTreeEditorSessionPrivate::
            MakeVisualAppearanceEdit(
                Scenario.Components[ComponentIndex]);

    return true;
}

bool UTGComponentTreeEditorSession::ApplyComponentVisualAppearance(
    FTGSimulationScenario& Scenario,
    FGuid ComponentId,
    const FTGComponentVisualAppearanceEdit& Appearance,
    FText& OutWarningText,
    FText& OutErrorText)
{
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    FTGComponentVisualAppearanceEdit ValidatedAppearance;

    if (!TGComponentTreeEditorSessionPrivate::
            ValidateVisualAppearance(
                Appearance,
                ValidatedAppearance,
                OutWarningText,
                OutErrorText))
    {
        return false;
    }

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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

    const FTGComponentVisualConfig& ExistingVisual =
        Component.Visual;

    const bool bHasVisualTransform =
        ExistingVisual.StlRecenterMode !=
            ETGStlRecenterMode::KeepImportedOrigin ||
        ExistingVisual.VisualOffsetMeters != FVector::ZeroVector ||
        ExistingVisual.VisualOrientation != FQuat::Identity ||
        ExistingVisual.VisualScale != FVector::OneVector;

    if (bHasVisualTransform)
    {
        TGComponentTreeEditorSessionPrivate::AppendWarning(
            OutWarningText,
            TEXT(
                "Visual recentering/offset/orientation/scale was "
                "reset. The source geometry now defines the component's "
                "visual origin, orientation and scale."));
    }

    if (TGComponentTreeEditorSessionPrivate::
            HaveSameVisualAppearance(
                Component,
                ValidatedAppearance))
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;

    const FString PreviousSrpGeometrySignature =
        UTGSolarRadiationPressureEditingLibrary::
            BuildComponentProxyGeometrySignature(Component);

    TGComponentTreeEditorSessionPrivate::
        ApplyVisualAppearance(
            Component,
            ValidatedAppearance);

    const FString CurrentSrpGeometrySignature =
        UTGSolarRadiationPressureEditingLibrary::
            BuildComponentProxyGeometrySignature(Component);

    if (PreviousSrpGeometrySignature != CurrentSrpGeometrySignature)
    {
        FTGComponentSrpConfig& SrpConfig =
            Component.SolarRadiationPressure;
        SrpConfig.bProxyGenerationRequired =
            SrpConfig.bIncludedInProxy;
        SrpConfig.LastProxyGenerationMessage =
            SrpConfig.bIncludedInProxy
                ? TEXT(
                    "Source geometry changed in Component Tree; SRP proxy "
                    "regeneration is required.")
                : TEXT("Component is excluded from the SRP proxy.");
    }

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Edit component visual appearance"));

    return true;
}

bool UTGComponentTreeEditorSession::
    ApplyComponentVisualVisibility(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        bool bVisible,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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

    if (Component.Visual.bVisible == bVisible)
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;

    Component.Visual.bVisible = bVisible;

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Change component visual visibility"));

    return true;
}


bool UTGComponentTreeEditorSession::
    GetComponentOrderedDegreesOfFreedom(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        TArray<FTGComponentDegreeOfFreedomEdit>&
            OutDegreesOfFreedom,
        FText& OutErrorText) const
{
    OutDegreesOfFreedom.Reset();
    OutErrorText = FText::GetEmpty();

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "The main component does not have "
                "parent-joint degrees of freedom."));

        return false;
    }

    if (TGComponentTreeEditorSessionPrivate::
            FindEarlierParentIndexByName(
                Scenario.Components,
                ComponentIndex) == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected component must reference an existing "
                "parent appearing earlier in the component array."));

        return false;
    }

    const FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    TSet<FGuid> UsedIdentifiers;

    OutDegreesOfFreedom.Reserve(
        Component.DegreesOfFreedom.Num());

    for (
        int32 ArrayIndex = 0;
        ArrayIndex < Component.DegreesOfFreedom.Num();
        ++ArrayIndex)
    {
        const FTGJointDofConfig& DegreeOfFreedom =
            Component.DegreesOfFreedom[ArrayIndex];

        if (
            !DegreeOfFreedom.DofId.IsValid() ||
            UsedIdentifiers.Contains(
                DegreeOfFreedom.DofId))
        {
            OutDegreesOfFreedom.Reset();

            OutErrorText = FText::FromString(
                TEXT(
                    "The selected component contains an invalid "
                    "or duplicated degree-of-freedom identifier."));

            return false;
        }

        UsedIdentifiers.Add(
            DegreeOfFreedom.DofId);

        const int32 FlatCoordinateIndex =
            TGComponentTreeEditorSessionPrivate::
                GetFlatCoordinateIndex(
                    Scenario.Components,
                    ComponentIndex,
                    ArrayIndex);

        OutDegreesOfFreedom.Add(
            TGComponentTreeEditorSessionPrivate::
                MakeDegreeOfFreedomEdit(
                    DegreeOfFreedom,
                    ArrayIndex,
                    FlatCoordinateIndex));
    }

    return true;
}

bool UTGComponentTreeEditorSession::
    AddDefaultComponentDegreeOfFreedom(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        ETGJointMotionType MotionType,
        FGuid& OutDegreeOfFreedomId,
        FText& OutErrorText)
{
    OutDegreeOfFreedomId.Invalidate();
    OutErrorText = FText::GetEmpty();

    if (
        MotionType != ETGJointMotionType::Rotation &&
        MotionType != ETGJointMotionType::Translation)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Degree-of-freedom motion type is invalid."));

        return false;
    }

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "The main component does not have "
                "parent-joint degrees of freedom."));

        return false;
    }

    const FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    if (
        Component.DegreesOfFreedom.Num() >=
        TGComponentTreeEditorSessionPrivate::
            MaxTotalDegreesOfFreedomPerConnection)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "A rigid parent-child connection can contain at most "
                "six independent degrees of freedom."));

        return false;
    }

    int32 MatchingMotionTypeCount = 0;

    for (
        const FTGJointDofConfig& DegreeOfFreedom :
        Component.DegreesOfFreedom)
    {
        if (DegreeOfFreedom.MotionType == MotionType)
        {
            ++MatchingMotionTypeCount;
        }
    }

    const int32 MaximumMatchingMotionTypeCount =
        MotionType == ETGJointMotionType::Rotation
            ? TGComponentTreeEditorSessionPrivate::
                MaxRotationalDegreesOfFreedomPerConnection
            : TGComponentTreeEditorSessionPrivate::
                MaxTranslationalDegreesOfFreedomPerConnection;

    if (
        MatchingMotionTypeCount >=
        MaximumMatchingMotionTypeCount)
    {
        OutErrorText = FText::FromString(
            MotionType == ETGJointMotionType::Rotation
                ? TEXT(
                    "A rigid parent-child connection can contain at most "
                    "three independent rotational degrees of freedom.")
                : TEXT(
                    "A rigid parent-child connection can contain at most "
                    "three independent translational degrees of freedom."));

        return false;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;
    FTGSimulationScenario WorkingScenario = Scenario;

    if (!UTGComponentTreeEditingLibrary::
            AddDefaultJointDof(
                WorkingScenario,
                ComponentId,
                MotionType,
                OutDegreeOfFreedomId,
                OutErrorText))
    {
        OutDegreeOfFreedomId.Invalidate();
        return false;
    }

    Scenario = MoveTemp(WorkingScenario);

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        MotionType == ETGJointMotionType::Rotation
            ? TEXT("Add rotational degree of freedom")
            : TEXT("Add translational degree of freedom"));

    return true;
}

bool UTGComponentTreeEditorSession::
    ApplyComponentDegreeOfFreedom(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        const FTGComponentDegreeOfFreedomEdit&
            DegreeOfFreedom,
        FText& OutWarningText,
        FText& OutErrorText)
{
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "The main component does not have "
                "parent-joint degrees of freedom."));

        return false;
    }

    FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    const int32 DegreeOfFreedomIndex =
        TGComponentTreeEditorSessionPrivate::
            FindDegreeOfFreedomIndexById(
                Component,
                DegreeOfFreedomId);

    if (DegreeOfFreedomIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected degree of freedom no longer exists."));

        return false;
    }

    const int32 FlatCoordinateIndex =
        TGComponentTreeEditorSessionPrivate::
            GetFlatCoordinateIndex(
                Scenario.Components,
                ComponentIndex,
                DegreeOfFreedomIndex);

    FTGComponentDegreeOfFreedomEdit
        ValidatedDegreeOfFreedom;

    const FTGComponentDegreeOfFreedomEdit
        CanonicalDegreeOfFreedom =
            TGComponentTreeEditorSessionPrivate::
                MakeCanonicalDegreeOfFreedomEdit(
                    DegreeOfFreedom);

    if (!TGComponentTreeEditorSessionPrivate::
            ValidateDegreeOfFreedom(
                Component,
                Component.DegreesOfFreedom[
                    DegreeOfFreedomIndex],
                DegreeOfFreedomIndex,
                FlatCoordinateIndex,
                CanonicalDegreeOfFreedom,
                ValidatedDegreeOfFreedom,
                OutWarningText,
                OutErrorText))
    {
        return false;
    }

    if (TGComponentTreeEditorSessionPrivate::
            HaveSameDegreeOfFreedom(
                Component.DegreesOfFreedom[
                    DegreeOfFreedomIndex],
                ValidatedDegreeOfFreedom))
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;

    TGComponentTreeEditorSessionPrivate::
        ApplyDegreeOfFreedom(
            Component.DegreesOfFreedom[
                DegreeOfFreedomIndex],
            ValidatedDegreeOfFreedom);

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Edit component degree of freedom"));

    return true;
}

bool UTGComponentTreeEditorSession::
    DeleteComponentDegreeOfFreedom(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "The main component does not have "
                "parent-joint degrees of freedom."));

        return false;
    }

    const FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    if (TGComponentTreeEditorSessionPrivate::
            FindDegreeOfFreedomIndexById(
                Component,
                DegreeOfFreedomId) == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected degree of freedom no longer exists."));

        return false;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;
    FTGSimulationScenario WorkingScenario = Scenario;

    if (!UTGComponentTreeEditingLibrary::
            DeleteJointDof(
                WorkingScenario,
                ComponentId,
                DegreeOfFreedomId,
                OutErrorText))
    {
        return false;
    }

    Scenario = MoveTemp(WorkingScenario);

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Delete component degree of freedom"));

    return true;
}

bool UTGComponentTreeEditorSession::
    MoveComponentDegreeOfFreedom(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        int32 NewArrayIndex,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "The main component does not have "
                "parent-joint degrees of freedom."));

        return false;
    }

    const FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    const int32 ExistingIndex =
        TGComponentTreeEditorSessionPrivate::
            FindDegreeOfFreedomIndexById(
                Component,
                DegreeOfFreedomId);

    if (ExistingIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected degree of freedom no longer exists."));

        return false;
    }

    if (!Component.DegreesOfFreedom.IsValidIndex(
            NewArrayIndex))
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "The requested DOF index %d is outside "
                    "the valid range 0 through %d."),
                NewArrayIndex,
                Component.DegreesOfFreedom.Num() - 1));

        return false;
    }

    if (ExistingIndex == NewArrayIndex)
    {
        return true;
    }

    const FTGSimulationScenario BeforeScenario = Scenario;
    FTGSimulationScenario WorkingScenario = Scenario;

    if (!UTGComponentTreeEditingLibrary::
            MoveJointDof(
                WorkingScenario,
                ComponentId,
                DegreeOfFreedomId,
                NewArrayIndex,
                OutErrorText))
    {
        return false;
    }

    Scenario = MoveTemp(WorkingScenario);

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        ComponentId,
        TEXT("Reorder component degrees of freedom"));

    return true;
}

bool UTGComponentTreeEditorSession::
    BuildInitialJointPreviewCoordinates(
        const FTGSimulationScenario& Scenario,
        TArray<double>& OutAcceptedCoordinates,
        FText& OutErrorText) const
{
    OutAcceptedCoordinates.Reset();
    OutErrorText = FText::GetEmpty();

    TArray<FTGComponentKinematicPose> ComponentPoses;

    return UTGComponentKinematicsLibrary::
        EvaluateComponentTreeKinematics(
            Scenario,
            TArray<double>(),
            ComponentPoses,
            OutAcceptedCoordinates,
            OutErrorText);
}

bool UTGComponentTreeEditorSession::
    BuildJointDofPreviewCoordinates(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        const TArray<double>& CurrentCoordinates,
        double RequestedCoordinate,
        TArray<double>& OutAcceptedCoordinates,
        double& OutAcceptedCoordinate,
        FText& OutWarningText,
        FText& OutErrorText) const
{
    OutAcceptedCoordinates.Reset();
    OutAcceptedCoordinate = 0.0;
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    if (!FMath::IsFinite(RequestedCoordinate))
    {
        OutErrorText = FText::FromString(
            TEXT("Preview coordinate must be finite."));

        return false;
    }

    const int32 ComponentIndex =
        TGComponentTreeEditorSessionPrivate::
            FindComponentIndexById(
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
                "The main component does not have "
                "parent-joint degrees of freedom."));

        return false;
    }

    const FTGComponentConfig& Component =
        Scenario.Components[ComponentIndex];

    const int32 DegreeOfFreedomIndex =
        TGComponentTreeEditorSessionPrivate::
            FindDegreeOfFreedomIndexById(
                Component,
                DegreeOfFreedomId);

    if (DegreeOfFreedomIndex == INDEX_NONE)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected degree of freedom no longer exists."));

        return false;
    }

    const int32 FlatCoordinateIndex =
        TGComponentTreeEditorSessionPrivate::
            GetFlatCoordinateIndex(
                Scenario.Components,
                ComponentIndex,
                DegreeOfFreedomIndex);

    const int32 ExpectedCoordinateCount =
        UTGComponentKinematicsLibrary::
            GetArticulationCoordinateCount(Scenario);

    TArray<double> BaseCoordinates;
    TArray<FTGComponentKinematicPose> ComponentPoses;

    if (CurrentCoordinates.IsEmpty())
    {
        if (!UTGComponentKinematicsLibrary::
                EvaluateComponentTreeKinematics(
                    Scenario,
                    TArray<double>(),
                    ComponentPoses,
                    BaseCoordinates,
                    OutErrorText))
        {
            return false;
        }
    }
    else
    {
        if (
            CurrentCoordinates.Num() !=
                ExpectedCoordinateCount)
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "Expected %d preview coordinate(s), "
                        "but received %d."),
                    ExpectedCoordinateCount,
                    CurrentCoordinates.Num()));

            return false;
        }

        if (!UTGComponentKinematicsLibrary::
                EvaluateComponentTreeKinematics(
                    Scenario,
                    CurrentCoordinates,
                    ComponentPoses,
                    BaseCoordinates,
                    OutErrorText))
        {
            return false;
        }
    }

    if (!BaseCoordinates.IsValidIndex(
            FlatCoordinateIndex))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected degree of freedom does not have "
                "a valid flattened preview-coordinate index."));

        return false;
    }

    const ETGJointMotionType MotionType =
        Component.DegreesOfFreedom[
            DegreeOfFreedomIndex].MotionType;

    const double CanonicalRequestedCoordinate =
        TGComponentTreeEditorSessionPrivate::
            DisplayCoordinateToCanonical(
                MotionType,
                RequestedCoordinate);

    BaseCoordinates[FlatCoordinateIndex] =
        CanonicalRequestedCoordinate;

    ComponentPoses.Reset();

    if (!UTGComponentKinematicsLibrary::
            EvaluateComponentTreeKinematics(
                Scenario,
                BaseCoordinates,
                ComponentPoses,
                OutAcceptedCoordinates,
                OutErrorText))
    {
        return false;
    }

    if (!OutAcceptedCoordinates.IsValidIndex(
            FlatCoordinateIndex))
    {
        OutAcceptedCoordinates.Reset();

        OutErrorText = FText::FromString(
            TEXT(
                "The evaluated preview-coordinate array does not "
                "contain the selected degree of freedom."));

        return false;
    }

    const double CanonicalAcceptedCoordinate =
        OutAcceptedCoordinates[FlatCoordinateIndex];

    OutAcceptedCoordinate =
        TGComponentTreeEditorSessionPrivate::
            CanonicalCoordinateToDisplay(
                MotionType,
                CanonicalAcceptedCoordinate);

    if (!FMath::IsNearlyEqual(
            CanonicalRequestedCoordinate,
            CanonicalAcceptedCoordinate,
            1.0e-12))
    {
        OutWarningText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Preview coordinate was clamped from %.9g "
                    "to %.9g by the configured coordinate limits."),
                RequestedCoordinate,
                OutAcceptedCoordinate));
    }

    return true;
}

bool UTGComponentTreeEditorSession::
    DeleteComponentSubtree(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        int32& OutDeletedComponentCount,
        FGuid& OutPreferredComponentId,
        FText& OutErrorText)
{
    OutDeletedComponentCount = 0;
    OutPreferredComponentId.Invalidate();
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    TGComponentTreeEditorSessionPrivate::FHierarchyMaps Maps;

    if (!TGComponentTreeEditorSessionPrivate::
            BuildHierarchyMaps(
                Scenario.Components,
                Maps,
                OutErrorText))
    {
        return false;
    }

    const int32* ComponentIndex =
        Maps.IndexById.Find(ComponentId);

    if (ComponentIndex == nullptr ||
        !Scenario.Components.IsValidIndex(*ComponentIndex))
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (*ComponentIndex == 0)
    {
        OutErrorText = FText::FromString(
            TEXT("The main component cannot be deleted."));

        return false;
    }

    const FGuid* ParentId =
        Maps.IdByNormalizedName.Find(
            TGComponentTreeEditorSessionPrivate::NormalizeName(
                Scenario.Components[*ComponentIndex]
                    .ParentComponentName));

    if (ParentId == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected component does not have a valid parent."));

        return false;
    }

    OutPreferredComponentId = *ParentId;

    const FTGSimulationScenario BeforeScenario = Scenario;

    if (!UTGComponentTreeEditingLibrary::
            DeleteComponentSubtree(
                Scenario,
                ComponentId,
                OutDeletedComponentCount,
                OutErrorText))
    {
        OutPreferredComponentId.Invalidate();
        return false;
    }

    RecordHistory(
        BeforeScenario,
        Scenario,
        ComponentId,
        OutPreferredComponentId,
        TEXT("Delete component subtree"));

    return true;
}

bool UTGComponentTreeEditorSession::
    MoveComponentSubtree(
        FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid TargetComponentId,
        ETGComponentTreeDropZone DropZone,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const FTGSimulationScenario BeforeScenario = Scenario;

    bool bChanged = false;

    if (!TGComponentTreeEditorSessionPrivate::
            MoveSubtreeInternal(
                Scenario,
                ComponentId,
                TargetComponentId,
                DropZone,
                bChanged,
                OutErrorText))
    {
        return false;
    }

    if (bChanged)
    {
        RecordHistory(
            BeforeScenario,
            Scenario,
            ComponentId,
            ComponentId,
            TEXT("Move component subtree"));
    }

    return true;
}

bool UTGComponentTreeEditorSession::
    CopyComponentSubtree(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    FTGSimulationScenario NormalizedScenario = Scenario;

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(
            NormalizedScenario);

    TGComponentTreeEditorSessionPrivate::FHierarchyMaps Maps;

    if (!TGComponentTreeEditorSessionPrivate::
            BuildHierarchyMaps(
                NormalizedScenario.Components,
                Maps,
                OutErrorText))
    {
        return false;
    }

    TArray<FGuid> SubtreeIds;

    if (!TGComponentTreeEditorSessionPrivate::
            CollectSubtreeIdsPreorder(
                ComponentId,
                Maps,
                SubtreeIds,
                OutErrorText))
    {
        return false;
    }

    Clipboard.Reset();
    Clipboard.Mode = ETGComponentTreeClipboardMode::Copy;
    Clipboard.SourceRootComponentId = ComponentId;
    Clipboard.CopiedComponents.Reserve(SubtreeIds.Num());

    for (const FGuid& SubtreeId : SubtreeIds)
    {
        const int32* ComponentIndex =
            Maps.IndexById.Find(SubtreeId);

        if (ComponentIndex == nullptr ||
            !NormalizedScenario.Components.IsValidIndex(
                *ComponentIndex))
        {
            Clipboard.Reset();

            OutErrorText = FText::FromString(
                TEXT(
                    "The copied component subtree could not be "
                    "reconstructed."));

            return false;
        }

        Clipboard.CopiedComponents.Add(
            NormalizedScenario.Components[*ComponentIndex]);
    }

    return true;
}

bool UTGComponentTreeEditorSession::CutComponentSubtree(
    const FTGSimulationScenario& Scenario,
    FGuid ComponentId,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    FTGSimulationScenario NormalizedScenario = Scenario;

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(
            NormalizedScenario);

    TGComponentTreeEditorSessionPrivate::FHierarchyMaps Maps;

    if (!TGComponentTreeEditorSessionPrivate::
            BuildHierarchyMaps(
                NormalizedScenario.Components,
                Maps,
                OutErrorText))
    {
        return false;
    }

    const int32* ComponentIndex =
        Maps.IndexById.Find(ComponentId);

    if (ComponentIndex == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT("The selected component no longer exists."));

        return false;
    }

    if (*ComponentIndex == 0)
    {
        OutErrorText = FText::FromString(
            TEXT("The main component cannot be cut."));

        return false;
    }

    Clipboard.Reset();
    Clipboard.Mode = ETGComponentTreeClipboardMode::Cut;
    Clipboard.SourceRootComponentId = ComponentId;

    return true;
}

bool UTGComponentTreeEditorSession::
    PasteComponentSubtree(
        FTGSimulationScenario& Scenario,
        FGuid DestinationParentId,
        FGuid CurrentSelectionId,
        FGuid& OutPastedRootComponentId,
        FText& OutErrorText)
{
    OutPastedRootComponentId.Invalidate();
    OutErrorText = FText::GetEmpty();

    if (Clipboard.Mode ==
        ETGComponentTreeClipboardMode::Empty)
    {
        OutErrorText = FText::FromString(
            TEXT("The component-tree clipboard is empty."));

        return false;
    }

    UTGComponentTreeEditingLibrary::
        NormalizeScenarioComponentTree(Scenario);

    const FTGSimulationScenario BeforeScenario = Scenario;

    if (Clipboard.Mode ==
        ETGComponentTreeClipboardMode::Copy)
    {
        if (!TGComponentTreeEditorSessionPrivate::
                DuplicateClipboardSubtree(
                    Scenario,
                    Clipboard.CopiedComponents,
                    DestinationParentId,
                    OutPastedRootComponentId,
                    OutErrorText))
        {
            return false;
        }

        RecordHistory(
            BeforeScenario,
            Scenario,
            CurrentSelectionId,
            OutPastedRootComponentId,
            TEXT("Paste copied component subtree"));

        return true;
    }

    if (Clipboard.Mode ==
        ETGComponentTreeClipboardMode::Cut)
    {
        const FGuid CutComponentId =
            Clipboard.SourceRootComponentId;

        bool bChanged = false;

        if (!TGComponentTreeEditorSessionPrivate::
                MoveSubtreeInternal(
                    Scenario,
                    CutComponentId,
                    DestinationParentId,
                    ETGComponentTreeDropZone::Onto,
                    bChanged,
                    OutErrorText))
        {
            return false;
        }

        OutPastedRootComponentId = CutComponentId;
        Clipboard.Reset();

        if (bChanged)
        {
            RecordHistory(
                BeforeScenario,
                Scenario,
                CurrentSelectionId,
                OutPastedRootComponentId,
                TEXT("Paste cut component subtree"));
        }

        return true;
    }

    OutErrorText = FText::FromString(
        TEXT("The component-tree clipboard mode is invalid."));

    return false;
}

bool UTGComponentTreeEditorSession::Undo(
    FTGSimulationScenario& Scenario,
    FGuid& OutPreferredComponentId,
    FText& OutEditDescription,
    FText& OutErrorText)
{
    OutPreferredComponentId.Invalidate();
    OutEditDescription = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    if (UndoStack.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT("There is no component-tree edit to undo."));

        return false;
    }

    FHistoryEntry Entry = UndoStack.Pop();

    Scenario = Entry.BeforeScenario;
    OutPreferredComponentId = Entry.BeforeSelectionId;
    OutEditDescription = FText::FromString(Entry.Description);

    RedoStack.Add(MoveTemp(Entry));
    return true;
}

bool UTGComponentTreeEditorSession::Redo(
    FTGSimulationScenario& Scenario,
    FGuid& OutPreferredComponentId,
    FText& OutEditDescription,
    FText& OutErrorText)
{
    OutPreferredComponentId.Invalidate();
    OutEditDescription = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    if (RedoStack.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT("There is no component-tree edit to redo."));

        return false;
    }

    FHistoryEntry Entry = RedoStack.Pop();

    Scenario = Entry.AfterScenario;
    OutPreferredComponentId = Entry.AfterSelectionId;
    OutEditDescription = FText::FromString(Entry.Description);

    UndoStack.Add(MoveTemp(Entry));
    return true;
}

void UTGComponentTreeEditorSession::RecordHistory(
    const FTGSimulationScenario& BeforeScenario,
    const FTGSimulationScenario& AfterScenario,
    FGuid BeforeSelectionId,
    FGuid AfterSelectionId,
    const FString& Description)
{
    FHistoryEntry Entry;

    Entry.BeforeScenario = BeforeScenario;
    Entry.AfterScenario = AfterScenario;
    Entry.BeforeSelectionId = BeforeSelectionId;
    Entry.AfterSelectionId = AfterSelectionId;
    Entry.Description = Description;

    UndoStack.Add(MoveTemp(Entry));
    RedoStack.Reset();

    if (MaximumHistoryEntries <= 0)
    {
        UndoStack.Reset();
        return;
    }

    const int32 ExcessEntryCount =
        UndoStack.Num() - MaximumHistoryEntries;

    if (ExcessEntryCount > 0)
    {
        UndoStack.RemoveAt(
            0,
            ExcessEntryCount,
            EAllowShrinking::No);
    }
}
