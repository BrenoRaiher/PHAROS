// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Review/TGBlueprintConfigReviewNavigationBase.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TreeView.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "UI/Configuration/ComponentTree/TGComponentTreeItemObject.h"
#include "UObject/UnrealType.h"

namespace
{
    UWidget* FindWidgetInNestedUserWidgets(
        UUserWidget* Root,
        const FName WidgetName)
    {
        if (Root == nullptr || Root->WidgetTree == nullptr)
        {
            return nullptr;
        }

        TArray<UUserWidget*> NestedWidgets;
        Root->WidgetTree->ForEachWidget(
            [&NestedWidgets](UWidget* Widget)
            {
                if (UUserWidget* Nested = Cast<UUserWidget>(Widget))
                {
                    NestedWidgets.Add(Nested);
                }
            });

        for (UUserWidget* Nested : NestedWidgets)
        {
            if (UWidget* Found = Nested->GetWidgetFromName(WidgetName))
            {
                return Found;
            }
            if (UWidget* Found = FindWidgetInNestedUserWidgets(
                    Nested,
                    WidgetName))
            {
                return Found;
            }
        }

        return nullptr;
    }

    UTGComponentTreeItemObject* FindComponentItemRecursive(
        const TArray<UObject*>& Items,
        const FGuid& ComponentId,
        int32 ComponentIndex)
    {
        for (UObject* Object : Items)
        {
            UTGComponentTreeItemObject* Item =
                Cast<UTGComponentTreeItemObject>(Object);
            if (Item == nullptr)
                continue;
            if ((ComponentId.IsValid() && Item->ComponentId == ComponentId)
                || (!ComponentId.IsValid()
                    && ComponentIndex != INDEX_NONE
                    && Item->ComponentIndex == ComponentIndex))
                return Item;

            TArray<UObject*> Children;
            Children.Reserve(Item->Children.Num());
            for (UTGComponentTreeItemObject* Child : Item->Children)
                Children.Add(Child);
            if (UTGComponentTreeItemObject* Found =
                    FindComponentItemRecursive(
                        Children, ComponentId, ComponentIndex))
                return Found;
        }
        return nullptr;
    }

    FGuid ReadGuidProperty(const UObject* Object, const FName Name)
    {
        if (Object == nullptr)
            return FGuid{};
        const FStructProperty* Property =
            FindFProperty<FStructProperty>(Object->GetClass(), Name);
        if (Property == nullptr || Property->Struct != TBaseStructure<FGuid>::Get())
            return FGuid{};
        return *Property->ContainerPtrToValuePtr<FGuid>(Object);
    }
}

void UTGBlueprintConfigReviewNavigationBase::NativeConstruct()
{
    Super::NativeConstruct();

    // The Scenario/Solver page is Blueprint-authored, but its displayed
    // integrator must always agree with the authoritative scenario draft.
    // Bind after Blueprint construction so this final commit cannot be
    // overwritten by the page's legacy string-to-enum event graph.
    BoundIntegratorKindCombo = Cast<UComboBoxString>(
        GetWidgetFromName(TEXT("INPUT_IntegratorKind")));
    if (BoundIntegratorKindCombo == nullptr)
    {
        return;
    }

    BoundIntegratorKindCombo->OnSelectionChanged.RemoveDynamic(
        this,
        &UTGBlueprintConfigReviewNavigationBase::
            HandleIntegratorKindSelectionChanged);
    BoundIntegratorKindCombo->OnSelectionChanged.AddDynamic(
        this,
        &UTGBlueprintConfigReviewNavigationBase::
            HandleIntegratorKindSelectionChanged);

    UGameInstance* GameInstance = GetGameInstance();
    const UTGSimulationSubsystem* Subsystem = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        return;
    }

    // Imported/saved data is authoritative during construction. This avoids
    // a designer-default RK4 selection replacing a loaded adaptive setup
    // before the Blueprint has refreshed its controls.
    switch (Subsystem->GetCurrentScenarioDraft()
                .ScenarioAndSolver.IntegratorKind)
    {
        case ETGIntegratorKind::FixedStepRK4:
            BoundIntegratorKindCombo->SetSelectedOption(
                TEXT("Fixed-Step RK4"));
            break;

        case ETGIntegratorKind::AdaptiveDormandPrince54:
            BoundIntegratorKindCombo->SetSelectedOption(
                TEXT("Adaptive Dormand-Prince 5(4)"));
            break;

        default:
            CommitIntegratorKindSelection(
                BoundIntegratorKindCombo->GetSelectedOption());
            break;
    }
}

void UTGBlueprintConfigReviewNavigationBase::NativeDestruct()
{
    if (BoundIntegratorKindCombo != nullptr)
    {
        BoundIntegratorKindCombo->OnSelectionChanged.RemoveDynamic(
            this,
            &UTGBlueprintConfigReviewNavigationBase::
                HandleIntegratorKindSelectionChanged);
    }
    BoundIntegratorKindCombo = nullptr;

    Super::NativeDestruct();
}

void UTGBlueprintConfigReviewNavigationBase::
    HandleIntegratorKindSelectionChanged(
        FString SelectedItem,
        const ESelectInfo::Type SelectionType)
{
    (void)SelectionType;
    CommitIntegratorKindSelection(SelectedItem);
}

void UTGBlueprintConfigReviewNavigationBase::CommitIntegratorKindSelection(
    const FString& SelectedItem)
{
    ETGIntegratorKind SelectedKind = ETGIntegratorKind::Unspecified;
    if (SelectedItem.Contains(TEXT("RK4"), ESearchCase::IgnoreCase))
    {
        SelectedKind = ETGIntegratorKind::FixedStepRK4;
    }
    else if (SelectedItem.Contains(
                 TEXT("Dormand"),
                 ESearchCase::IgnoreCase))
    {
        SelectedKind = ETGIntegratorKind::AdaptiveDormandPrince54;
    }
    else
    {
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UTGSimulationSubsystem* Subsystem = GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        return;
    }

    FTGSimulationScenario Scenario =
        Subsystem->GetCurrentScenarioDraft();
    if (Scenario.ScenarioAndSolver.IntegratorKind == SelectedKind)
    {
        return;
    }

    Scenario.ScenarioAndSolver.IntegratorKind = SelectedKind;
    Subsystem->SetCurrentScenarioDraft(Scenario);
}

bool UTGBlueprintConfigReviewNavigationBase::
    NavigateToScenarioReviewIssue_Implementation(
        const FTGScenarioReviewIssue& Issue)
{
    const FString& Path = Issue.Path;

    if (Issue.Section == ETGScenarioReviewSection::ScenarioAndSolver)
    {
        if (Path.Contains(TEXT("ScenarioName"))) return FocusWidgetNamed(TEXT("INPUT_ScenarioName"));
        if (Path.Contains(TEXT("StartUtc"))) return FocusWidgetNamed(TEXT("INPUT_StartUtc"));
        if (Path.Contains(TEXT("EndMode"))) return FocusWidgetNamed(TEXT("INPUT_EndMode"));
        if (Path.Contains(TEXT("FinalUtc"))) return FocusWidgetNamed(TEXT("INPUT_FinalUtc"));
        if (Path.Contains(TEXT("Duration"))) return FocusWidgetNamed(TEXT("INPUT_Duration"));
        if (Path.Contains(TEXT("IntegratorKind"))) return FocusWidgetNamed(TEXT("INPUT_IntegratorKind"));
        if (Path.Contains(TEXT("MaximumIntegratorStep"))) return FocusWidgetNamed(TEXT("INPUT_MaximumIntegratorStep"));
        if (Path.Contains(TEXT("InitialIntegratorStep"))) return FocusWidgetNamed(TEXT("INPUT_InitialIntegratorStep"));
        if (Path.Contains(TEXT("AbsoluteTolerance"))) return FocusWidgetNamed(TEXT("INPUT_AbsoluteTolerance"));
        if (Path.Contains(TEXT("RelativeTolerance"))) return FocusWidgetNamed(TEXT("INPUT_RelativeTolerance"));
        if (Path.Contains(TEXT("OutputMode"))) return FocusWidgetNamed(TEXT("INPUT_OutputMode"));
        if (Path.Contains(TEXT("OutputStep"))) return FocusWidgetNamed(TEXT("INPUT_OutputStep"));
        if (Path.Contains(TEXT("MaximumIntegrationSteps"))) return FocusWidgetNamed(TEXT("INPUT_MaximumIntegrationSteps"));
        if (Path.Contains(TEXT("MaximumOutputSamples"))) return FocusWidgetNamed(TEXT("INPUT_MaximumOutputSamples"));
        if (Path.Contains(TEXT("MaximumWallClockRuntimeSeconds"))) return FocusWidgetNamed(TEXT("INPUT_MaximumWallClockRuntimeSeconds"));
    }
    else if (Issue.Section == ETGScenarioReviewSection::InitialState)
    {
        if (Path.Contains(TEXT("Position"))) return FocusWidgetNamed(TEXT("VECTOR_Position"));
        if (Path.Contains(TEXT("Velocity"))) return FocusWidgetNamed(TEXT("VECTOR_Velocity"));
        if (Path.Contains(TEXT("Attitude"))) return FocusWidgetNamed(TEXT("QUAT_Attitude"));
        if (Path.Contains(TEXT("AngularVelocity"))) return FocusWidgetNamed(TEXT("VECTOR_AngularVelocity"));
    }
    else if (Issue.Section == ETGScenarioReviewSection::ComponentsAndJoints)
    {
        if (NavigateComponentTreeIssue(Issue))
            return true;
    }
    else if (Issue.Section == ETGScenarioReviewSection::Gravity)
    {
        if (NavigateGravityIssue(Issue))
            return true;
    }

    if (NavigateDynamicScenarioReviewIssue(Issue))
    {
        return true;
    }

    // Dynamic-page fallback is explicit: focus its principal navigation area
    // and report false so the host preserves the full diagnostic fallback.
    if (Issue.Section == ETGScenarioReviewSection::ComponentsAndJoints)
        FocusWidgetNamed(TEXT("TREE_Components"));
    else if (Issue.Section == ETGScenarioReviewSection::Gravity)
        FocusWidgetNamed(TEXT("SCROLL_Catalog"));
    else
        SetKeyboardFocus();
    return false;
}

bool UTGBlueprintConfigReviewNavigationBase::FocusWidgetNamed(
    FName WidgetName)
{
    // Prefer a field inside a nested native editor. This lets configuration
    // pages retain their old hidden Blueprint controls while Review focuses
    // the visible replacement widget.
    UWidget* Widget = FindWidgetInNestedUserWidgets(
        this,
        WidgetName);
    if (Widget == nullptr)
    {
        Widget = GetWidgetFromName(WidgetName);
    }
    if (Widget == nullptr)
    {
        return false;
    }
    Widget->SetIsEnabled(true);
    Widget->SetKeyboardFocus();
    return true;
}

bool UTGBlueprintConfigReviewNavigationBase::NavigateComponentTreeIssue(
    const FTGScenarioReviewIssue& Issue)
{
    UTreeView* Tree = Cast<UTreeView>(GetWidgetFromName(TEXT("TREE_Components")));
    if (Tree == nullptr
        || (!Issue.ComponentId.IsValid() && Issue.ArrayIndex == INDEX_NONE))
        return false;

    UTGComponentTreeItemObject* Item = FindComponentItemRecursive(
        Tree->GetListItems(), Issue.ComponentId, Issue.ArrayIndex);
    if (Item == nullptr)
        return false;

    for (UObject* Ancestor : Item->GetAncestorsAsObjects())
        Tree->SetItemExpansion(Ancestor, true);
    Tree->SetItemSelection(Item, true);
    Tree->RequestScrollItemIntoView(Item);

    UUserWidget* Inspector = Cast<UUserWidget>(
        GetWidgetFromName(TEXT("INSPECTOR_Component")));
    if (Inspector == nullptr)
    {
        Tree->SetKeyboardFocus();
        return false;
    }

    const FString& Path = Issue.Path;
    if (Issue.DofId.IsValid())
    {
        UPanelWidget* Rows = Cast<UPanelWidget>(
            Inspector->GetWidgetFromName(TEXT("VBOX_DegreeOfFreedomRows")));
        if (Rows == nullptr)
            return false;

        for (int32 Index = 0; Index < Rows->GetChildrenCount(); ++Index)
        {
            UUserWidget* Row = Cast<UUserWidget>(Rows->GetChildAt(Index));
            if (ReadGuidProperty(Row, TEXT("DegreeOfFreedomId")) != Issue.DofId)
                continue;

            if (UWidget* Details = Row->GetWidgetFromName(
                    TEXT("VBOX_DegreeOfFreedomDetails")))
                Details->SetVisibility(ESlateVisibility::Visible);

            FName FieldName = TEXT("BORDER_DegreeOfFreedomRow");
            if (Path.Contains(TEXT(".Name"))) FieldName = TEXT("INPUT_DegreeOfFreedomName");
            else if (Path.Contains(TEXT("MotionType"))) FieldName = TEXT("INPUT_DegreeOfFreedomType");
            else if (Path.Contains(TEXT(".Axis"))) FieldName = TEXT("VECTOR_DegreeOfFreedomAxis");
            else if (Path.Contains(TEXT("InitialCoordinate"))) FieldName = TEXT("INPUT_InitialCoordinate");
            else if (Path.Contains(TEXT("InitialRate"))) FieldName = TEXT("INPUT_InitialRate");
            else if (Path.Contains(TEXT("MaximumAbsoluteRate"))) FieldName = TEXT("INPUT_MaximumAbsoluteRate");
            else if (Path.Contains(TEXT("MaximumAbsoluteEffort"))) FieldName = TEXT("INPUT_MaximumAbsoluteEffort");
            else if (Path.Contains(TEXT("MinimumCoordinate"))) FieldName = TEXT("INPUT_MinimumCoordinate");
            else if (Path.Contains(TEXT("MaximumCoordinate"))) FieldName = TEXT("INPUT_MaximumCoordinate");

            UWidget* Field = Row->GetWidgetFromName(FieldName);
            if (Field == nullptr)
                Field = Row;
            Field->SetIsEnabled(true);
            Field->SetKeyboardFocus();
            return true;
        }
        return false;
    }

    if (Path.EndsWith(TEXT(".Name"))
        && !Path.Contains(TEXT("DegreesOfFreedom")))
    {
        if (UFunction* RenameFunction =
                FindFunction(TEXT("BeginInlineRenameSelectedComponent")))
        {
            ProcessEvent(RenameFunction, nullptr);
            return true;
        }
    }

    FName FieldName = TEXT("SCROLL_Inspector");
    if (Path.Contains(TEXT("InitialMass"))) FieldName = TEXT("EDT_InitialMassKg");
    else if (Path.Contains(TEXT("MinimumMass"))) FieldName = TEXT("EDT_MinimumMassKg");
    else if (Path.Contains(TEXT("bVariableMass"))) FieldName = TEXT("CHK_VariableMass");
    else if (Path.Contains(TEXT("LocalCenterOfMass"))) FieldName = TEXT("VECTOR_LocalCM");
    else if (Path.Contains(TEXT("CentroidalInertia"))) FieldName = TEXT("EDT_Ixx");
    else if (Path.Contains(TEXT("OriginInBody"))) FieldName = TEXT("VECTOR_RootOrigin");
    else if (Path.Contains(TEXT("ComponentToBodyOrientation"))) FieldName = TEXT("QUAT_RootComponentToBody");
    else if (Path.Contains(TEXT("ParentAnchor"))) FieldName = TEXT("VECTOR_ParentAnchor");
    else if (Path.Contains(TEXT("ChildAnchor"))) FieldName = TEXT("VECTOR_ChildAnchor");
    else if (Path.Contains(TEXT("ChildToParentZeroOrientation"))) FieldName = TEXT("QUAT_ChildToParentZero");
    else if (Path.Contains(TEXT("GeometrySource"))) FieldName = TEXT("COMBO_GeometrySource");
    else if (Path.Contains(TEXT("PrimitiveType"))) FieldName = TEXT("COMBO_PrimitiveType");
    else if (Path.Contains(TEXT("BoxDimensions"))) FieldName = TEXT("VECTOR_BoxDimensions");
    else if (Path.Contains(TEXT("SphereRadius"))) FieldName = TEXT("INPUT_SphereRadius");
    else if (Path.Contains(TEXT("CylinderRadius"))) FieldName = TEXT("INPUT_CylinderRadius");
    else if (Path.Contains(TEXT("CylinderLength"))) FieldName = TEXT("INPUT_CylinderLength");
    else if (Path.Contains(TEXT("StlFilePath"))) FieldName = TEXT("BTN_BrowseStl");
    else if (Path.Contains(TEXT("StlLengthUnit"))) FieldName = TEXT("COMBO_StlLengthUnit");
    else if (Path.Contains(TEXT("SurfaceAppearance"))) FieldName = TEXT("COMBO_SurfaceAppearance");
    else if (Path.Contains(TEXT("DisplayColor"))) FieldName = TEXT("COLOR_SolidColor");
    else if (Path.Contains(TEXT("BaseColorTint"))) FieldName = TEXT("COLOR_BaseColorTint");
    else if (Path.Contains(TEXT("BaseColorTexture"))) FieldName = TEXT("BTN_BrowseBaseColorTexture");
    else if (Path.Contains(TEXT("NormalTexture"))) FieldName = TEXT("BTN_BrowseNormalTexture");
    else if (Path.Contains(TEXT("RoughnessTexture"))) FieldName = TEXT("BTN_BrowseRoughnessTexture");
    else if (Path.Contains(TEXT("MetallicTexture"))) FieldName = TEXT("BTN_BrowseMetallicTexture");
    else if (Path.Contains(TEXT("bVisible"))) FieldName = TEXT("CHECK_Visible");

    UWidget* Field = Inspector->GetWidgetFromName(FieldName);
    if (Field == nullptr)
        return false;
    Field->SetIsEnabled(true);
    Field->SetKeyboardFocus();
    return true;
}

bool UTGBlueprintConfigReviewNavigationBase::NavigateGravityIssue(
    const FTGScenarioReviewIssue& Issue)
{
    const FString& Path = Issue.Path;
    if (Path.Contains(TEXT("bIncludeFirstPostNewtonian")))
        return FocusWidgetNamed(TEXT("CHK_Include1PN"));

    FName CatalogKey = NAME_None;
    if (Issue.ArrayIndex != INDEX_NONE)
    {
        if (const UGameInstance* GameInstance = GetGameInstance())
        {
            if (const UTGSimulationSubsystem* Subsystem =
                    GameInstance->GetSubsystem<UTGSimulationSubsystem>())
            {
                const FTGSimulationScenario Scenario =
                    Subsystem->GetCurrentScenarioDraft();
                if (Scenario.CelestialBodies.IsValidIndex(Issue.ArrayIndex))
                    CatalogKey = Scenario.CelestialBodies[Issue.ArrayIndex].CatalogKey;
            }
        }
    }

    UUserWidget* SourceRow = nullptr;
    if (!CatalogKey.IsNone() && WidgetTree != nullptr)
    {
        TArray<UWidget*> Widgets;
        WidgetTree->GetAllWidgets(Widgets);
        for (UWidget* Widget : Widgets)
        {
            UUserWidget* Candidate = Cast<UUserWidget>(Widget);
            if (Candidate == nullptr)
                continue;
            const FStructProperty* EntryProperty =
                FindFProperty<FStructProperty>(
                    Candidate->GetClass(), TEXT("CatalogEntry"));
            if (EntryProperty == nullptr
                || EntryProperty->Struct != FTGCelestialCatalogEntry::StaticStruct())
                continue;
            const FTGCelestialCatalogEntry* Entry =
                EntryProperty->ContainerPtrToValuePtr<FTGCelestialCatalogEntry>(Candidate);
            if (Entry != nullptr && Entry->CatalogKey == CatalogKey)
            {
                SourceRow = Candidate;
                break;
            }
        }
    }

    if (SourceRow != nullptr)
    {
        if (UButton* SelectButton = Cast<UButton>(
                SourceRow->GetWidgetFromName(TEXT("BTN_SelectSource"))))
            SelectButton->OnClicked.Broadcast();
        if (UScrollBox* CatalogScroll = Cast<UScrollBox>(
                GetWidgetFromName(TEXT("SCROLL_Catalog"))))
            CatalogScroll->ScrollWidgetIntoView(SourceRow, true);

        if (Path.Contains(TEXT("bGravityEnabled")))
        {
            if (UWidget* Enabled = SourceRow->GetWidgetFromName(
                    TEXT("CHK_GravityEnabled")))
            {
                Enabled->SetKeyboardFocus();
                return true;
            }
        }
    }

    if (Path.Contains(TEXT("AutomaticActivationRadius")))
        return FocusWidgetNamed(TEXT("INPUT_AutomaticRadiusKm"));
    if (Path.Contains(TEXT("BarycenterResolutionRadius")))
        return FocusWidgetNamed(TEXT("INPUT_BarycenterResolutionKm"));
    if (Path.Contains(TEXT("MaximumHarmonicDegree")))
        return FocusWidgetNamed(TEXT("INPUT_MaximumHarmonicDegree"));
    if (Path.Contains(TEXT("HarmonicModelCsv")))
        return FocusWidgetNamed(TEXT("BTN_SelectHarmonicCsv"));

    if (SourceRow != nullptr)
    {
        SourceRow->SetKeyboardFocus();
        return true;
    }
    return false;
}
