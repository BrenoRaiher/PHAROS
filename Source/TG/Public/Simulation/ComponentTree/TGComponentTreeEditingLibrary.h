// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGComponentTreeEditingLibrary.generated.h"

/**
 * Safe editing operations for the Blueprint-facing spacecraft component tree.
 *
 * The functions operate on the complete scenario so that component-name
 * references in other scenario sections can be preserved or checked.
 */
UCLASS()
class TG_API UTGComponentTreeEditingLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Ensures that the scenario contains a main component and that every
     * component and joint DOF has a valid, unique editor GUID.
     *
     * This function deliberately does not silently repair physical values,
     * duplicate names, invalid parents, or invalid joint definitions.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Normalize Scenario Component Tree"))
    static void NormalizeScenarioComponentTree(
        UPARAM(ref) FTGSimulationScenario& Scenario);

    /**
     * Adds a new fixed child component beneath the selected parent.
     *
     * The component is appended to the array, guaranteeing that its parent
     * appears earlier in the backend-facing component order.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Add Default Child Component"))
    static bool AddDefaultChildComponent(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ParentComponentId,
        FGuid& OutComponentId,
        FText& OutErrorText);

    /**
     * Renames one component and updates all unambiguous scenario references
     * that store the component's name.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Rename Component"))
    static bool RenameComponent(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FString& NewName,
        FText& OutErrorText);

    /**
     * Changes the parent of a child component.
     *
     * The complete moved subtree is appended after the new parent while its
     * internal ordering is preserved. Reparenting under a descendant is
     * rejected.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Reparent Component"))
    static bool ReparentComponent(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid NewParentComponentId,
        FText& OutErrorText);

    /**
     * Deletes a component and every descendant beneath it.
     *
     * The main component cannot be deleted. Deletion is rejected while any
     * component in the subtree is referenced by an actuator, SRP facet, or
     * aerodynamic configuration.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Delete Component Subtree"))
    static bool DeleteComponentSubtree(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        int32& OutDeletedComponentCount,
        FText& OutErrorText);

    /**
     * Adds one default ordered joint DOF to a child component.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Add Default Joint DOF"))
    static bool AddDefaultJointDof(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        ETGJointMotionType MotionType,
        FGuid& OutDofId,
        FText& OutErrorText);

    /**
     * Deletes one joint DOF using its stable editor identity.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Delete Joint DOF"))
    static bool DeleteJointDof(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DofId,
        FText& OutErrorText);

    /**
     * Moves one joint DOF to an explicit array index.
     *
     * DOF order is physically meaningful and is therefore preserved exactly.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Move Joint DOF"))
    static bool MoveJointDof(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DofId,
        int32 NewIndex,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Find Component Index By ID"))
    static int32 FindComponentIndexById(
        const TArray<FTGComponentConfig>& Components,
        FGuid ComponentId);

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree",
        meta = (DisplayName = "Find Joint DOF Index By ID"))
    static int32 FindJointDofIndexById(
        const FTGComponentConfig& Component,
        FGuid DofId);
};