// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Visualization/TGVisualizationConstellationRowWidget.h"

#include "Components/Border.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "UI/Theme/TGUiTheme.h"

namespace TGVisualizationConstellationRowPrivate
{
    FString NormalizeConstellationId(FString Value)
    {
        int32 ScopeSeparator = INDEX_NONE;
        if (Value.FindLastChar(TEXT(':'), ScopeSeparator))
        {
            Value = Value.Mid(ScopeSeparator + 1);
        }
        Value.ReplaceInline(TEXT("_"), TEXT(""));
        Value.ReplaceInline(TEXT(" "), TEXT(""));
        return Value.ToLower();
    }

    bool TryReadConstellationId(const AActor& Actor, FString& OutId)
    {
        for (TFieldIterator<FProperty> PropertyIt(
                 Actor.GetClass(),
                 EFieldIteratorFlags::IncludeSuper);
             PropertyIt;
             ++PropertyIt)
        {
            FProperty* Property = *PropertyIt;
            if (!Property->GetName().StartsWith(TEXT("ConstellationId")))
            {
                continue;
            }
            if (const FEnumProperty* EnumProperty =
                    CastField<FEnumProperty>(Property))
            {
                const void* Value = Property->ContainerPtrToValuePtr<void>(
                    &Actor);
                const int64 EnumValue = EnumProperty->GetUnderlyingProperty()
                    ->GetSignedIntPropertyValue(Value);
                OutId = EnumProperty->GetEnum()->GetNameStringByValue(
                    EnumValue);
                return true;
            }
            if (const FByteProperty* ByteProperty =
                    CastField<FByteProperty>(Property))
            {
                const uint8 Value = ByteProperty->GetPropertyValue_InContainer(
                    &Actor);
                OutId = ByteProperty->Enum != nullptr
                    ? ByteProperty->Enum->GetNameStringByValue(Value)
                    : FString::FromInt(Value);
                return true;
            }
            if (const FNameProperty* NameProperty =
                    CastField<FNameProperty>(Property))
            {
                OutId = NameProperty->GetPropertyValue_InContainer(&Actor)
                    .ToString();
                return true;
            }
            if (const FStrProperty* StringProperty =
                    CastField<FStrProperty>(Property))
            {
                OutId = StringProperty->GetPropertyValue_InContainer(&Actor);
                return true;
            }
        }
        return false;
    }

    bool InvokeSingleBooleanFunction(
        UObject& Target,
        const FName FunctionName,
        const bool bValue)
    {
        UFunction* Function = Target.FindFunction(FunctionName);
        if (Function == nullptr)
        {
            return false;
        }

        FStructOnScope Parameters(Function);
        uint8* ParameterMemory = Parameters.GetStructMemory();
        bool bSetBooleanParameter = false;
        for (TFieldIterator<FProperty> PropertyIt(Function); PropertyIt; ++PropertyIt)
        {
            FProperty* Property = *PropertyIt;
            if (!Property->HasAnyPropertyFlags(CPF_Parm) ||
                Property->HasAnyPropertyFlags(CPF_ReturnParm))
            {
                continue;
            }
            if (FBoolProperty* BooleanProperty =
                    CastField<FBoolProperty>(Property))
            {
                BooleanProperty->SetPropertyValue_InContainer(
                    ParameterMemory,
                    bValue);
                bSetBooleanParameter = true;
            }
        }
        if (!bSetBooleanParameter)
        {
            return false;
        }

        Target.ProcessEvent(Function, ParameterMemory);
        return true;
    }

    bool IsMatchingConstellationShell(
        const AActor& Actor,
        const FString& ConstellationId)
    {
        if (!Actor.GetClass()->GetName().Contains(TEXT("ConstellationShell")))
        {
            return false;
        }
        FString ActorConstellationId;
        return TryReadConstellationId(Actor, ActorConstellationId) &&
            NormalizeConstellationId(ActorConstellationId) ==
                NormalizeConstellationId(ConstellationId);
    }
}

void UTGVisualizationConstellationRowWidget::InitializeConstellationRow(
    const FString& InConstellationId,
    const FString& InConstellationName)
{
    ConstellationId = InConstellationId;
    ConstellationName = InConstellationName;

    RefreshDesignerWidgets();
}

const FString&
UTGVisualizationConstellationRowWidget::GetConstellationName() const
{
    return ConstellationName;
}

void UTGVisualizationConstellationRowWidget::NativeConstruct()
{
    Super::NativeConstruct();
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    if (BORDER_ConstellationRow != nullptr)
    {
        BORDER_ConstellationRow->SetBrush(TGUiTheme::MakeRoundedBrush(
            Palette.Surface,
            4.0f,
            Palette.Border,
            1.0f));
        BORDER_ConstellationRow->SetPadding(FMargin(12.0f, 8.0f));
    }
    TGUiTheme::ApplyCompactCheckBoxStyle(*CHECK_Outline);
    TGUiTheme::ApplyTextStyle(
        *TXT_ConstellationName,
        ETGUiTextStyle::FieldLabel,
        Palette.TextPrimary);
    if (TXT_OutlineLabel != nullptr)
    {
        TGUiTheme::ApplyTextStyle(
            *TXT_OutlineLabel,
            ETGUiTextStyle::Caption,
            Palette.TextSecondary);
    }
    if (UHorizontalBoxSlot* CheckSlot =
            Cast<UHorizontalBoxSlot>(CHECK_Outline->Slot))
    {
        CheckSlot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
    }
    CHECK_Outline->OnCheckStateChanged.RemoveDynamic(
        this,
        &UTGVisualizationConstellationRowWidget::
            HandleOutlineCheckStateChanged);
    CHECK_Outline->OnCheckStateChanged.AddUniqueDynamic(
        this,
        &UTGVisualizationConstellationRowWidget::
            HandleOutlineCheckStateChanged);
    RefreshDesignerWidgets();
}

void UTGVisualizationConstellationRowWidget::NativeDestruct()
{
    CHECK_Outline->OnCheckStateChanged.RemoveDynamic(
        this,
        &UTGVisualizationConstellationRowWidget::
            HandleOutlineCheckStateChanged);
    Super::NativeDestruct();
}

void UTGVisualizationConstellationRowWidget::RefreshDesignerWidgets()
{
    bRefreshingDesignerWidgets = true;
    if (TXT_ConstellationName != nullptr)
    {
        TXT_ConstellationName->SetText(FText::FromString(ConstellationName));
    }
    if (CHECK_Outline != nullptr)
    {
        CHECK_Outline->SetIsChecked(bOutlineVisible);
    }
    bRefreshingDesignerWidgets = false;
}

void UTGVisualizationConstellationRowWidget::ApplyOutlineVisibility(
    const bool bVisible)
{
    UWorld* World = GetWorld();
    if (World == nullptr)
    {
        return;
    }
    for (TActorIterator<AActor> ActorIt(World); ActorIt; ++ActorIt)
    {
        if (TGVisualizationConstellationRowPrivate::
                IsMatchingConstellationShell(**ActorIt, ConstellationId))
        {
            TGVisualizationConstellationRowPrivate::
                InvokeSingleBooleanFunction(
                    **ActorIt,
                    TEXT("SetConstellationOutlineVisible"),
                    bVisible);
        }
    }
}

void UTGVisualizationConstellationRowWidget::
    HandleOutlineCheckStateChanged(const bool bChecked)
{
    if (bRefreshingDesignerWidgets)
    {
        return;
    }
    bOutlineVisible = bChecked;
    ApplyOutlineVisibility(bChecked);
}
