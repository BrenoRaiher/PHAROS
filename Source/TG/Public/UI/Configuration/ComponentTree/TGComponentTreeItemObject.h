// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "TGComponentTreeItemObject.generated.h"

/**
 * Temporary view-model object representing one physical component inside
 * the UMG Tree View.
 *
 * This object does not own or replace the component configuration. Its stable
 * ComponentId connects the displayed entry back to FTGComponentConfig.
 */
UCLASS(BlueprintType)
class TG_API UTGComponentTreeItemObject : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Component Tree Item")
    FGuid ComponentId;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Component Tree Item")
    FString ComponentName;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Component Tree Item")
    int32 ComponentIndex = INDEX_NONE;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Component Tree Item")
    int32 Depth = 0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Component Tree Item")
    bool bIsRoot = false;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Component Tree Item")
    TObjectPtr<UTGComponentTreeItemObject> ParentItem;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "PHAROS|Component Tree Item")
    TArray<TObjectPtr<UTGComponentTreeItemObject>> Children;

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree Item",
        meta = (DisplayName = "Component Tree Item Has Children"))
    bool HasChildren() const;

    /**
     * Returns the child items using the generic UObject array expected by
     * UTreeView's Get Item Children event.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree Item",
        meta = (DisplayName = "Get Component Tree Item Children"))
    TArray<UObject*> GetChildrenAsObjects() const;

    /**
     * Returns ancestors from the root down to the immediate parent.
     *
     * The component-tree widget will use this when a 3D click selects a deeply
     * nested component whose parent entries are currently collapsed.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree Item",
        meta = (DisplayName = "Get Component Tree Item Ancestors"))
    TArray<UObject*> GetAncestorsAsObjects() const;
	
	UFUNCTION(
		BlueprintPure,
		Category = "PHAROS|Component Tree Item",
		meta = (DisplayName = "Get Component Tree Item ID"))
	FGuid GetComponentId() const;
};