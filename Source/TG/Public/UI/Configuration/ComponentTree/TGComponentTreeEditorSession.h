// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "TGComponentTreeEditorSession.generated.h"

/**
 * Interprets where a dragged component is dropped relative to a tree row.
 */
UENUM(BlueprintType)
enum class ETGComponentTreeDropZone : uint8
{
    Before UMETA(DisplayName = "Before"),
    Onto UMETA(DisplayName = "Onto"),
    After UMETA(DisplayName = "After")
};

/**
 * Current clipboard operation owned by the component-tree editor session.
 */
UENUM(BlueprintType)
enum class ETGComponentTreeClipboardMode : uint8
{
    Empty UMETA(DisplayName = "Empty"),
    Copy UMETA(DisplayName = "Copy"),
    Cut UMETA(DisplayName = "Cut")
};


/**
 * Editable physical properties for one rigid spacecraft component.
 *
 * This UI-facing value object deliberately excludes identity, hierarchy,
 * connection, joint and visual fields so an inspector edit cannot overwrite
 * unrelated component data.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentPhysicalPropertiesEdit
{
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mass",
        meta = (ClampMin = "0.0"))
    double InitialMassKilograms = 1.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Mass",
        meta = (ClampMin = "0.0"))
    double MinimumMassKilograms = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass")
    bool bVariableMass = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Properties")
    FVector LocalCenterOfMassMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mass Properties")
    FTGSymmetricInertia CentroidalInertia;
};

/**
 * Editable transform fields owned exclusively by component zero.
 *
 * OriginInBodyMeters is the main-component origin measured from the B origin
 * and expressed in B. ComponentToBodyOrientation actively rotates vectors
 * from the main-component frame into B.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentRootTransformEdit
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Component")
    FVector OriginInBodyMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Main Component")
    FQuat ComponentToBodyOrientation = FQuat::Identity;
};

/**
 * Editable connection fields owned exclusively by non-root components.
 *
 * ParentComponentName is read-only hierarchy context for the inspector. The
 * apply operation deliberately preserves the existing parent relation and the
 * ordered DegreesOfFreedom array.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentChildConnectionEdit
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Connection")
    FString ParentComponentName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    FVector ParentAnchorMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    FVector ChildAnchorMeters = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Connection")
    FQuat ChildToParentZeroOrientation = FQuat::Identity;
};


/**
 * Editable visual geometry and whole-component surface appearance.
 *
 * This UI-facing value object deliberately excludes visual offset, visual
 * orientation, visual scale and STL recentering. The editor always preserves
 * the imported STL origin/orientation and applies only the selected STL unit.
 * Materials are configured for the entire rigid component, never per face.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentVisualAppearanceEdit
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry")
    ETGComponentGeometrySource GeometrySource =
        ETGComponentGeometrySource::Primitive;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Primitive")
    ETGPrimitiveGeometryType PrimitiveType =
        ETGPrimitiveGeometryType::Box;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Primitive")
    FVector BoxDimensionsMeters = FVector(1.0, 1.0, 1.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Primitive")
    double SphereRadiusMeters = 0.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Primitive")
    double CylinderRadiusMeters = 0.5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|Primitive")
    double CylinderLengthMeters = 1.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|STL")
    FString StlFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Geometry|STL")
    ETGStlLengthUnit StlLengthUnit =
        ETGStlLengthUnit::Millimeters;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    ETGComponentSurfaceAppearanceMode SurfaceAppearanceMode =
        ETGComponentSurfaceAppearanceMode::SolidColor;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FLinearColor SolidColor =
        FLinearColor(0.18f, 0.55f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance")
    FLinearColor BaseColorTint = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Textures")
    FString BaseColorTextureFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Textures")
    FString NormalTextureFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Textures")
    FString RoughnessTextureFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance|Textures")
    FString MetallicTextureFilePath;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Display")
    bool bVisible = true;
};


/**
 * Editable representation of one ordered joint degree of freedom.
 *
 * DegreeOfFreedomId is stable editor identity. ArrayIndex and
 * FlatCoordinateIndex are read-only context calculated from the authoritative
 * component/DOF ordering. The apply operation modifies only the configuration
 * fields and preserves the existing DOF identity and array position.
 */
USTRUCT(BlueprintType)
struct TG_API FTGComponentDegreeOfFreedomEdit
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Identity")
    FGuid DegreeOfFreedomId;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ordering")
    int32 ArrayIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ordering")
    int32 FlatCoordinateIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Identity")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
    ETGJointMotionType MotionType =
        ETGJointMotionType::Rotation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion")
    FVector Axis = FVector::ForwardVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial State")
    double InitialCoordinate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Initial State")
    double InitialRate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    bool bHasMinimumCoordinate = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MinimumCoordinate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    bool bHasMaximumCoordinate = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MaximumCoordinate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MaximumAbsoluteRate = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Limits")
    double MaximumAbsoluteEffort = 0.0;
};

/**
 * Stateful editing session used by the spacecraft component-tree widget.
 *
 * All mutating tree operations pass through this object so that drag/drop,
 * toolbar actions and keyboard shortcuts share the same validation, history
 * and clipboard behavior.
 *
 * Undo and redo intentionally store complete scenario snapshots. The scenario
 * structure contains value types only, making snapshots simple and reliable
 * while the HUD is still under active development.
 */
UCLASS(BlueprintType)
class TG_API UTGComponentTreeEditorSession : public UObject
{
    GENERATED_BODY()

public:
    /** Clears undo, redo and clipboard state. */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Reset Component Tree Editor Session"))
    void ResetSession();

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Component Tree Can Undo"))
    bool CanUndo() const;

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Component Tree Can Redo"))
    bool CanRedo() const;

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Component Tree Has Clipboard"))
    bool HasClipboard() const;

    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Get Component Tree Clipboard Mode"))
    ETGComponentTreeClipboardMode GetClipboardMode() const;

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Session Add Default Child Component"))
    bool AddDefaultChildComponent(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ParentComponentId,
        FGuid CurrentSelectionId,
        FGuid& OutComponentId,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Session Rename Component"))
    bool RenameComponent(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FString& NewName,
        FText& OutErrorText);


    /** Reads only the physical-property fields of one component. */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Get Component Physical Properties"))
    bool GetComponentPhysicalProperties(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FTGComponentPhysicalPropertiesEdit& OutProperties,
        FText& OutErrorText) const;

    /**
     * Validates and atomically commits all physical-property fields.
     *
     * Warnings never block the edit. Invalid values leave Scenario and the
     * undo/redo history unchanged.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Apply Component Physical Properties"))
    bool ApplyComponentPhysicalProperties(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FTGComponentPhysicalPropertiesEdit& Properties,
        FText& OutWarningText,
        FText& OutErrorText);

    /** Reads the root-only transform fields of component zero. */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Get Component Root Transform"))
    bool GetComponentRootTransform(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FTGComponentRootTransformEdit& OutTransform,
        FText& OutErrorText) const;

    /**
     * Validates and atomically commits the root-only transform fields.
     *
     * A finite, nonzero quaternion is stored exactly as entered. A non-unit
     * input is accepted with a warning; the backend conversion layer performs
     * the required one-time normalization before quaternion-to-matrix use.
     * Invalid values leave Scenario and the undo/redo history unchanged.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Apply Component Root Transform"))
    bool ApplyComponentRootTransform(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FTGComponentRootTransformEdit& Transform,
        FText& OutWarningText,
        FText& OutErrorText);

    /** Reads the connection fields of one non-root component. */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Get Component Child Connection"))
    bool GetComponentChildConnection(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FTGComponentChildConnectionEdit& OutConnection,
        FText& OutErrorText) const;

    /**
     * Validates and atomically commits only the editable child-connection
     * fields. ParentComponentName and DegreesOfFreedom are preserved.
     *
     * A finite, nonzero quaternion is stored exactly as entered. A non-unit
     * input is accepted with a warning; the backend conversion layer performs
     * the required one-time normalization before quaternion-to-matrix use.
     * Invalid values leave Scenario and the undo/redo history unchanged.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Apply Component Child Connection"))
    bool ApplyComponentChildConnection(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FTGComponentChildConnectionEdit& Connection,
        FText& OutWarningText,
        FText& OutErrorText);


    /** Reads the editable visual-appearance fields of one component. */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Get Component Visual Appearance"))
    bool GetComponentVisualAppearance(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FTGComponentVisualAppearanceEdit& OutAppearance,
        FText& OutErrorText) const;

    /**
     * Validates and atomically commits geometry and whole-component surface
     * appearance. No Geometry is rejected. STL recentering is fixed to
     * KeepImportedOrigin and legacy visual offset/orientation/scale are reset
     * to zero/identity/one so the source STL remains authoritative.
     *
     * Textured mode requires a PNG/JPG/JPEG base-color image; normal,
     * roughness and metallic maps are optional and use the same formats.
     * Invalid edits leave Scenario and undo/redo history unchanged.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Apply Component Visual Appearance"))
    bool ApplyComponentVisualAppearance(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        const FTGComponentVisualAppearanceEdit& Appearance,
        FText& OutWarningText,
        FText& OutErrorText);


    /**
     * Commits only the component's persistent visibility flag.
     *
     * This intentionally bypasses validation of the inspector's other
     * in-progress visual fields so visibility remains editable while the user
     * is assembling an incomplete Textured appearance (for example before a
     * required base-color texture has been selected).
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Apply Component Visual Visibility"))
    bool ApplyComponentVisualVisibility(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        bool bVisible,
        FText& OutErrorText);


    /**
     * Reads the selected non-root component's complete ordered DOF array.
     *
     * ArrayIndex is local to the component. FlatCoordinateIndex follows the
     * authoritative component-array order and then DOF-array order.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName =
            "Get Component Ordered Degrees Of Freedom"))
    bool GetComponentOrderedDegreesOfFreedom(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        TArray<FTGComponentDegreeOfFreedomEdit>&
            OutDegreesOfFreedom,
        FText& OutErrorText) const;

    /**
     * Appends one authoritative default DOF to a non-root component.
     *
     * A rigid parent-child connection is limited to six independent DOFs:
     * at most three rotational and at most three translational.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName =
            "Add Default Component Degree Of Freedom"))
    bool AddDefaultComponentDegreeOfFreedom(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        ETGJointMotionType MotionType,
        FGuid& OutDegreeOfFreedomId,
        FText& OutErrorText);

    /**
     * Validates and atomically commits one existing ordered DOF.
     *
     * A rigid parent-child connection is limited to six independent DOFs:
     * at most three rotational and at most three translational.
     * Non-unit axes are stored exactly as entered and accepted with a warning.
     * The kinematics/conversion layer performs the required normalization
     * before use. Invalid edits leave Scenario and history unchanged.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName =
            "Apply Component Degree Of Freedom"))
    bool ApplyComponentDegreeOfFreedom(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        const FTGComponentDegreeOfFreedomEdit&
            DegreeOfFreedom,
        FText& OutWarningText,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName =
            "Delete Component Degree Of Freedom"))
    bool DeleteComponentDegreeOfFreedom(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        FText& OutErrorText);

    /**
     * Moves one existing DOF to an explicit local array index.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName =
            "Move Component Degree Of Freedom"))
    bool MoveComponentDegreeOfFreedom(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        int32 NewArrayIndex,
        FText& OutErrorText);

    /**
     * Builds the flattened, limit-projected initial-coordinate array used by
     * the scalar DOF preview widgets.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName =
            "Build Initial Joint Preview Coordinates"))
    bool BuildInitialJointPreviewCoordinates(
        const FTGSimulationScenario& Scenario,
        TArray<double>& OutAcceptedCoordinates,
        FText& OutErrorText) const;

    /**
     * Replaces one DOF coordinate in the current flattened preview array and
     * evaluates the complete ordered chain with backend-equivalent limits.
     *
     * Pass an empty CurrentCoordinates array to start from accepted initial
     * coordinates. OutAcceptedCoordinates is ready for Apply Joint Coordinates.
     * This operation is temporary, creates no history, and does not modify the
     * scenario.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName =
            "Build Joint DOF Preview Coordinates"))
    bool BuildJointDofPreviewCoordinates(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid DegreeOfFreedomId,
        const TArray<double>& CurrentCoordinates,
        double RequestedCoordinate,
        TArray<double>& OutAcceptedCoordinates,
        double& OutAcceptedCoordinate,
        FText& OutWarningText,
        FText& OutErrorText) const;

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Session Delete Component Subtree"))
    bool DeleteComponentSubtree(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        int32& OutDeletedComponentCount,
        FGuid& OutPreferredComponentId,
        FText& OutErrorText);

    /**
     * Moves one complete subtree using normal tree-editor drop semantics.
     *
     * Before/After reorder siblings. Onto makes the dragged component the
     * last child of the target component.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Session Move Component Subtree"))
    bool MoveComponentSubtree(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FGuid TargetComponentId,
        ETGComponentTreeDropZone DropZone,
        FText& OutErrorText);

    /** Copies a complete subtree into the session clipboard. */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Copy Component Subtree"))
    bool CopyComponentSubtree(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FText& OutErrorText);

    /**
     * Marks a complete subtree for moving on the next paste.
     *
     * The scenario is not changed until Paste Component Subtree succeeds.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Cut Component Subtree"))
    bool CutComponentSubtree(
        const FTGSimulationScenario& Scenario,
        FGuid ComponentId,
        FText& OutErrorText);

    /**
     * Pastes the clipboard subtree as the last child of DestinationParentId.
     *
     * Copy creates new component and DOF GUIDs and unique component names.
     * Cut moves the existing subtree and then clears the clipboard.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Paste Component Subtree"))
    bool PasteComponentSubtree(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid DestinationParentId,
        FGuid CurrentSelectionId,
        FGuid& OutPastedRootComponentId,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Undo Component Tree Edit"))
    bool Undo(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid& OutPreferredComponentId,
        FText& OutEditDescription,
        FText& OutErrorText);

    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|Component Tree|Editor Session",
        meta = (DisplayName = "Redo Component Tree Edit"))
    bool Redo(
        UPARAM(ref) FTGSimulationScenario& Scenario,
        FGuid& OutPreferredComponentId,
        FText& OutEditDescription,
        FText& OutErrorText);

private:
    struct FHistoryEntry
    {
        FTGSimulationScenario BeforeScenario;
        FTGSimulationScenario AfterScenario;

        FGuid BeforeSelectionId;
        FGuid AfterSelectionId;

        FString Description;
    };

    struct FClipboardState
    {
        ETGComponentTreeClipboardMode Mode =
            ETGComponentTreeClipboardMode::Empty;

        FGuid SourceRootComponentId;
        TArray<FTGComponentConfig> CopiedComponents;

        void Reset()
        {
            Mode = ETGComponentTreeClipboardMode::Empty;
            SourceRootComponentId.Invalidate();
            CopiedComponents.Reset();
        }
    };

    TArray<FHistoryEntry> UndoStack;
    TArray<FHistoryEntry> RedoStack;

    FClipboardState Clipboard;

    int32 MaximumHistoryEntries = 50;

    void RecordHistory(
        const FTGSimulationScenario& BeforeScenario,
        const FTGSimulationScenario& AfterScenario,
        FGuid BeforeSelectionId,
        FGuid AfterSelectionId,
        const FString& Description);
};
