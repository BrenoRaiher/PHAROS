// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Simulation/TGSimulationScenarioTypes.h"
#include "Types/SlateEnums.h"
#include "TGConfigInitialStateFrameWidgetBase.generated.h"

class UComboBoxString;
class UTextBlock;
class UTGSimulationSubsystem;

/**
 * Frontend-only initial-state authoring adapter.
 *
 * The user may edit a state in ICRF or in a selected SPICE rotating
 * body-fixed frame. Every accepted edit is immediately transformed back into
 * the canonical ICRF/body representation stored by FTGSimulationScenario.
 * Only the selected authoring-frame catalog key is persisted as presentation
 * metadata; TGSimCore still receives the canonical state.
 */
UCLASS(Blueprintable)
class TG_API UTGConfigInitialStateFrameWidgetBase : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "PHAROS|Initial State|Reference Frame")
    void RefreshFromCurrentDraft();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativeTick(
        const FGeometry& MyGeometry,
        float InDeltaTime) override;

private:
    UFUNCTION()
    void HandleReferenceFrameSelectionChanged(
        FString SelectedItem,
        ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleFramePositionCommitted(FVector NewBackendValue);

    UFUNCTION()
    void HandleFrameVelocityCommitted(FVector NewBackendValue);

    UFUNCTION()
    void HandleFrameAttitudeCommitted(FQuat NewBackendValue);

    UFUNCTION()
    void HandleFrameAngularVelocityCommitted(FVector NewBackendValue);

    UTGSimulationSubsystem* GetSimulationSubsystem() const;

    void PopulateReferenceFrameOptions();
    void RestoreReferenceFrameSelection(
        const FTGSimulationScenario& Scenario);
    bool BindInputDispatchers(FText& OutError);
    void UnbindInputDispatchers();

    bool ConvertCanonicalToAuthored(
        const FTGSimulationScenario& Scenario,
        FTGInitialSpacecraftState& OutAuthoredState,
        FText& OutError) const;

    bool ConvertAuthoredToCanonical(
        const FTGSimulationScenario& Scenario,
        const FTGInitialSpacecraftState& InAuthoredState,
        FTGInitialSpacecraftState& OutCanonicalState,
        FText& OutError) const;

    bool CommitAuthoredState();
    void RefreshInputWidgets();

    bool SetVectorWidgetValue(
        UUserWidget* VectorWidget,
        const FVector& Value,
        FText& OutError) const;

    bool SetQuaternionWidgetValue(
        UUserWidget* QuaternionWidget,
        const FQuat& Value,
        FText& OutError) const;

    bool BindWidgetDispatcher(
        UUserWidget* SourceWidget,
        FName DispatcherName,
        FName HandlerName,
        FText& OutError);

    void UnbindWidgetDispatcher(
        UUserWidget* SourceWidget,
        FName DispatcherName,
        FName HandlerName);

    FString GetSelectedFrameDisplayName() const;
    void ShowError(const FText& Error);
    void ClearError();
    void UpdateDescription();

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> COMBO_ReferenceFrame;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> VECTOR_Position;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> VECTOR_Velocity;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> QUAT_Attitude;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UUserWidget> VECTOR_AngularVelocity;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_ReferenceFrameDescription;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_ReferenceFrameError;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TranslationHeading;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_TranslationDescription;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_AttitudeHeading;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_AttitudeDescription;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_AngularVelocityLabel;

    UPROPERTY(Transient)
    FTGInitialSpacecraftState AuthoredState;

    TMap<FString, FName> FrameCatalogKeyByOption;
    TMap<FName, FString> FrameOptionByCatalogKey;
    FName SelectedFrameCatalogKey = NAME_None;
    FString LastAppliedFrameOption;
    uint64 LastObservedDraftRevision = 0;
    bool bRefreshing = false;
};
