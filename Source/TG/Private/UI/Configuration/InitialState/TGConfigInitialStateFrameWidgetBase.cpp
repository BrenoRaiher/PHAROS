// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/InitialState/TGConfigInitialStateFrameWidgetBase.h"

#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "SpiceBridge.h"
#include "TGSim/Core/Types.h"
#include "UObject/UnrealType.h"

namespace TGInitialStateFramePrivate
{
    const FString IcrfOption = TEXT("ICRF (J2000)");

    struct FResolvedBodyFrame
    {
        tgsim::Vec3d origin_position_icrf_m;
        tgsim::Vec3d origin_velocity_icrf_mps;
        tgsim::Mat3d body_fixed_to_icrf;
        tgsim::Vec3d angular_velocity_icrf_radps;
        tgsim::Quatd body_fixed_to_icrf_quaternion;
    };

    tgsim::Vec3d ToBackend(const FVector& Value)
    {
        return {Value.X, Value.Y, Value.Z};
    }

    FVector ToFrontend(const tgsim::Vec3d& Value)
    {
        return FVector(Value.x, Value.y, Value.z);
    }

    tgsim::Quatd ToBackend(const FQuat& Value)
    {
        return {Value.W, Value.X, Value.Y, Value.Z};
    }

    FQuat ToFrontend(const tgsim::Quatd& Value)
    {
        return FQuat(Value.x, Value.y, Value.z, Value.w);
    }

    bool IsFinite(const FVector& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    bool IsFinite(const FQuat& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z) &&
            FMath::IsFinite(Value.W);
    }

    tgsim::Vec3d TransformVector(
        const tgsim::Mat3d& Matrix,
        const tgsim::Vec3d& Vector)
    {
        return {
            Matrix.m[0][0] * Vector.x +
                Matrix.m[0][1] * Vector.y +
                Matrix.m[0][2] * Vector.z,
            Matrix.m[1][0] * Vector.x +
                Matrix.m[1][1] * Vector.y +
                Matrix.m[1][2] * Vector.z,
            Matrix.m[2][0] * Vector.x +
                Matrix.m[2][1] * Vector.y +
                Matrix.m[2][2] * Vector.z};
    }

    tgsim::Vec3d InverseTransformVector(
        const tgsim::Mat3d& Matrix,
        const tgsim::Vec3d& Vector)
    {
        // Rotation inverse: R^-1 v = R^T v.
        return {
            Matrix.m[0][0] * Vector.x +
                Matrix.m[1][0] * Vector.y +
                Matrix.m[2][0] * Vector.z,
            Matrix.m[0][1] * Vector.x +
                Matrix.m[1][1] * Vector.y +
                Matrix.m[2][1] * Vector.z,
            Matrix.m[0][2] * Vector.x +
                Matrix.m[1][2] * Vector.y +
                Matrix.m[2][2] * Vector.z};
    }

    tgsim::Quatd NormalizeQuaternion(const tgsim::Quatd& Value)
    {
        const double Norm = FMath::Sqrt(
            Value.w * Value.w +
            Value.x * Value.x +
            Value.y * Value.y +
            Value.z * Value.z);
        return Norm > UE_DOUBLE_SMALL_NUMBER
            ? tgsim::Quatd{
                Value.w / Norm,
                Value.x / Norm,
                Value.y / Norm,
                Value.z / Norm}
            : tgsim::Quatd::Identity();
    }

    tgsim::Quatd ConjugateQuaternion(const tgsim::Quatd& Value)
    {
        return {Value.w, -Value.x, -Value.y, -Value.z};
    }

    tgsim::Quatd MultiplyQuaternions(
        const tgsim::Quatd& Left,
        const tgsim::Quatd& Right)
    {
        return {
            Left.w * Right.w - Left.x * Right.x -
                Left.y * Right.y - Left.z * Right.z,
            Left.w * Right.x + Left.x * Right.w +
                Left.y * Right.z - Left.z * Right.y,
            Left.w * Right.y - Left.x * Right.z +
                Left.y * Right.w + Left.z * Right.x,
            Left.w * Right.z + Left.x * Right.y -
                Left.y * Right.x + Left.z * Right.w};
    }

    tgsim::Mat3d QuaternionToRotationMatrix(
        const tgsim::Quatd& Value)
    {
        const tgsim::Quatd Q = NormalizeQuaternion(Value);
        tgsim::Mat3d Result = tgsim::Mat3d::Zero();
        Result.m[0][0] = 1.0 - 2.0 * (Q.y * Q.y + Q.z * Q.z);
        Result.m[0][1] = 2.0 * (Q.x * Q.y - Q.w * Q.z);
        Result.m[0][2] = 2.0 * (Q.x * Q.z + Q.w * Q.y);
        Result.m[1][0] = 2.0 * (Q.x * Q.y + Q.w * Q.z);
        Result.m[1][1] = 1.0 - 2.0 * (Q.x * Q.x + Q.z * Q.z);
        Result.m[1][2] = 2.0 * (Q.y * Q.z - Q.w * Q.x);
        Result.m[2][0] = 2.0 * (Q.x * Q.z - Q.w * Q.y);
        Result.m[2][1] = 2.0 * (Q.y * Q.z + Q.w * Q.x);
        Result.m[2][2] = 1.0 - 2.0 * (Q.x * Q.x + Q.y * Q.y);
        return Result;
    }

    tgsim::Vec3d InverseRotateVector(
        const tgsim::Quatd& Rotation,
        const tgsim::Vec3d& Vector)
    {
        return InverseTransformVector(
            QuaternionToRotationMatrix(Rotation),
            Vector);
    }

    void SetVectorUnitsAndFrame(
        UUserWidget* VectorWidget,
        const FString& Text)
    {
        if (VectorWidget == nullptr)
        {
            return;
        }

        if (UTextBlock* UnitsText = Cast<UTextBlock>(
                VectorWidget->GetWidgetFromName(
                    TEXT("TXT_UnitsAndFrame"))))
        {
            UnitsText->SetText(FText::FromString(Text));
        }
    }

    tgsim::Quatd QuaternionFromRotationMatrix(
        const tgsim::Mat3d& Rotation)
    {
        tgsim::Quatd Result;
        const double Trace =
            Rotation.m[0][0] +
            Rotation.m[1][1] +
            Rotation.m[2][2];

        if (Trace > 0.0)
        {
            const double Scale = 2.0 * FMath::Sqrt(Trace + 1.0);
            Result.w = 0.25 * Scale;
            Result.x = (Rotation.m[2][1] - Rotation.m[1][2]) / Scale;
            Result.y = (Rotation.m[0][2] - Rotation.m[2][0]) / Scale;
            Result.z = (Rotation.m[1][0] - Rotation.m[0][1]) / Scale;
        }
        else if (
            Rotation.m[0][0] > Rotation.m[1][1] &&
            Rotation.m[0][0] > Rotation.m[2][2])
        {
            const double Scale = 2.0 * FMath::Sqrt(
                1.0 + Rotation.m[0][0] -
                Rotation.m[1][1] - Rotation.m[2][2]);
            Result.w = (Rotation.m[2][1] - Rotation.m[1][2]) / Scale;
            Result.x = 0.25 * Scale;
            Result.y = (Rotation.m[0][1] + Rotation.m[1][0]) / Scale;
            Result.z = (Rotation.m[0][2] + Rotation.m[2][0]) / Scale;
        }
        else if (Rotation.m[1][1] > Rotation.m[2][2])
        {
            const double Scale = 2.0 * FMath::Sqrt(
                1.0 + Rotation.m[1][1] -
                Rotation.m[0][0] - Rotation.m[2][2]);
            Result.w = (Rotation.m[0][2] - Rotation.m[2][0]) / Scale;
            Result.x = (Rotation.m[0][1] + Rotation.m[1][0]) / Scale;
            Result.y = 0.25 * Scale;
            Result.z = (Rotation.m[1][2] + Rotation.m[2][1]) / Scale;
        }
        else
        {
            const double Scale = 2.0 * FMath::Sqrt(
                1.0 + Rotation.m[2][2] -
                Rotation.m[0][0] - Rotation.m[1][1]);
            Result.w = (Rotation.m[1][0] - Rotation.m[0][1]) / Scale;
            Result.x = (Rotation.m[0][2] + Rotation.m[2][0]) / Scale;
            Result.y = (Rotation.m[1][2] + Rotation.m[2][1]) / Scale;
            Result.z = 0.25 * Scale;
        }

        return NormalizeQuaternion(Result);
    }

    bool ResolveBodyFrame(
        const FName CatalogKey,
        const FTGSimulationScenario& Scenario,
        FResolvedBodyFrame& OutFrame,
        FText& OutError)
    {
        FTGCelestialCatalogEntry Entry;
        if (!UTGCelestialCatalogLibrary::FindCelestialCatalogEntry(
                CatalogKey,
                Entry) ||
            Entry.SourceRole ==
                ETGCelestialSourceRole::SystemBarycenter)
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("'%s' is not a physical celestial-body frame."),
                *CatalogKey.ToString()));
            return false;
        }

        const FString StartUtc =
            Scenario.ScenarioAndSolver.StartUtc.ToIso8601();
        double EphemerisTime = 0.0;
        FString Diagnostic;
        if (!FSpiceBridge::ConvertUTCToET(
                StartUtc,
                EphemerisTime,
                Diagnostic))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Cannot evaluate the selected frame at Start UTC: %s"),
                *Diagnostic));
            return false;
        }

        FVector PositionIcrf;
        FVector VelocityIcrf;
        if (!FSpiceBridge::GetBodyICRFStateSI(
                Entry.SpiceTarget,
                EphemerisTime,
                PositionIcrf,
                VelocityIcrf,
                Diagnostic))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Cannot obtain the %s origin state: %s"),
                *Entry.DisplayName.ToString(),
                *Diagnostic));
            return false;
        }

        if (!FSpiceBridge::GetBodyFixedToICRF(
                Entry.SpiceTarget,
                EphemerisTime,
                OutFrame.body_fixed_to_icrf,
                Diagnostic))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Cannot obtain the %s body-fixed orientation: %s"),
                *Entry.DisplayName.ToString(),
                *Diagnostic));
            return false;
        }

        if (!FSpiceBridge::GetBodyAngularVelocityICRF(
                Entry.SpiceTarget,
                EphemerisTime,
                OutFrame.angular_velocity_icrf_radps,
                Diagnostic))
        {
            OutError = FText::FromString(FString::Printf(
                TEXT("Cannot obtain the %s frame angular velocity: %s"),
                *Entry.DisplayName.ToString(),
                *Diagnostic));
            return false;
        }

        OutFrame.origin_position_icrf_m = ToBackend(PositionIcrf);
        OutFrame.origin_velocity_icrf_mps = ToBackend(VelocityIcrf);
        OutFrame.body_fixed_to_icrf_quaternion =
            QuaternionFromRotationMatrix(
                OutFrame.body_fixed_to_icrf);
        OutError = FText::GetEmpty();
        return true;
    }
}

void UTGConfigInitialStateFrameWidgetBase::NativeConstruct()
{
    Super::NativeConstruct();

    PopulateReferenceFrameOptions();

    if (COMBO_ReferenceFrame != nullptr)
    {
        COMBO_ReferenceFrame->OnSelectionChanged.AddUniqueDynamic(
            this,
            &UTGConfigInitialStateFrameWidgetBase::
                HandleReferenceFrameSelectionChanged);
    }

    FText BindingError;
    if (!BindInputDispatchers(BindingError))
    {
        ShowError(BindingError);
    }

    RefreshFromCurrentDraft();
}

void UTGConfigInitialStateFrameWidgetBase::NativeDestruct()
{
    if (COMBO_ReferenceFrame != nullptr)
    {
        COMBO_ReferenceFrame->OnSelectionChanged.RemoveDynamic(
            this,
            &UTGConfigInitialStateFrameWidgetBase::
                HandleReferenceFrameSelectionChanged);
    }

    UnbindInputDispatchers();
    Super::NativeDestruct();
}

void UTGConfigInitialStateFrameWidgetBase::NativeTick(
    const FGeometry& MyGeometry,
    const float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bRefreshing)
    {
        return;
    }

    if (COMBO_ReferenceFrame != nullptr)
    {
        const FString CurrentOption =
            COMBO_ReferenceFrame->GetSelectedOption();
        if (!CurrentOption.IsEmpty() &&
            CurrentOption != LastAppliedFrameOption)
        {
            HandleReferenceFrameSelectionChanged(
                CurrentOption,
                ESelectInfo::Direct);
            return;
        }
    }

    const UTGSimulationSubsystem* Subsystem =
        GetSimulationSubsystem();
    if (Subsystem != nullptr &&
        Subsystem->GetCurrentScenarioDraftRevision() !=
            LastObservedDraftRevision)
    {
        RefreshFromCurrentDraft();
    }
}

UTGSimulationSubsystem*
UTGConfigInitialStateFrameWidgetBase::GetSimulationSubsystem() const
{
    UWorld* World = GetWorld();
    UGameInstance* GameInstance =
        World != nullptr
            ? World->GetGameInstance()
            : nullptr;
    return GameInstance != nullptr
        ? GameInstance->GetSubsystem<UTGSimulationSubsystem>()
        : nullptr;
}

void UTGConfigInitialStateFrameWidgetBase::
PopulateReferenceFrameOptions()
{
    if (COMBO_ReferenceFrame == nullptr)
    {
        return;
    }

    TGuardValue<bool> RefreshGuard(bRefreshing, true);
    FrameCatalogKeyByOption.Reset();
    FrameOptionByCatalogKey.Reset();
    COMBO_ReferenceFrame->ClearOptions();
    COMBO_ReferenceFrame->AddOption(
        TGInitialStateFramePrivate::IcrfOption);
    FrameCatalogKeyByOption.Add(
        TGInitialStateFramePrivate::IcrfOption,
        NAME_None);
    FrameOptionByCatalogKey.Add(
        NAME_None,
        TGInitialStateFramePrivate::IcrfOption);

    for (const FTGCelestialCatalogEntry& Entry
         : UTGCelestialCatalogLibrary::GetCelestialCatalog())
    {
        if (Entry.SourceRole ==
            ETGCelestialSourceRole::SystemBarycenter)
        {
            continue;
        }

        const FString Option = FString::Printf(
            TEXT("%s body-fixed"),
            *Entry.DisplayName.ToString());
        COMBO_ReferenceFrame->AddOption(Option);
        FrameCatalogKeyByOption.Add(Option, Entry.CatalogKey);
        FrameOptionByCatalogKey.Add(Entry.CatalogKey, Option);
    }

    COMBO_ReferenceFrame->SetSelectedOption(
        TGInitialStateFramePrivate::IcrfOption);
    SelectedFrameCatalogKey = NAME_None;
    LastAppliedFrameOption =
        TGInitialStateFramePrivate::IcrfOption;
}

void UTGConfigInitialStateFrameWidgetBase::
RestoreReferenceFrameSelection(const FTGSimulationScenario& Scenario)
{
    FName FrameCatalogKey =
        Scenario.InitialState.AuthoringFrameCatalogKey;
    const FString* FrameOption =
        FrameOptionByCatalogKey.Find(FrameCatalogKey);
    if (FrameOption == nullptr)
    {
        FrameCatalogKey = NAME_None;
        FrameOption = FrameOptionByCatalogKey.Find(NAME_None);
    }

    if (FrameOption == nullptr)
    {
        return;
    }

    TGuardValue<bool> RefreshGuard(bRefreshing, true);
    SelectedFrameCatalogKey = FrameCatalogKey;
    LastAppliedFrameOption = *FrameOption;
    if (COMBO_ReferenceFrame != nullptr &&
        COMBO_ReferenceFrame->GetSelectedOption() != *FrameOption)
    {
        COMBO_ReferenceFrame->SetSelectedOption(*FrameOption);
    }
}

void UTGConfigInitialStateFrameWidgetBase::RefreshFromCurrentDraft()
{
    ClearError();

    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        ShowError(FText::FromString(
            TEXT("No current simulation scenario draft is available.")));
        return;
    }

    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    const FTGSimulationScenario Scenario =
        Subsystem->GetCurrentScenarioDraft();
    RestoreReferenceFrameSelection(Scenario);
    UpdateDescription();

    FText Error;
    FTGInitialSpacecraftState ConvertedState;
    if (!ConvertCanonicalToAuthored(
            Scenario,
            ConvertedState,
            Error))
    {
        ShowError(Error);
        return;
    }

    AuthoredState = ConvertedState;
    RefreshInputWidgets();
}

bool UTGConfigInitialStateFrameWidgetBase::ConvertCanonicalToAuthored(
    const FTGSimulationScenario& Scenario,
    FTGInitialSpacecraftState& OutAuthoredState,
    FText& OutError) const
{
    const FTGInitialSpacecraftState& Canonical = Scenario.InitialState;
    if (!TGInitialStateFramePrivate::IsFinite(Canonical.PositionMeters) ||
        !TGInitialStateFramePrivate::IsFinite(
            Canonical.VelocityMetersPerSecond) ||
        !TGInitialStateFramePrivate::IsFinite(
            Canonical.AngularVelocityBodyRadiansPerSecond) ||
        !TGInitialStateFramePrivate::IsFinite(
            Canonical.AttitudeBodyToIcrf) ||
        Canonical.AttitudeBodyToIcrf.SizeSquared() <=
            UE_DOUBLE_SMALL_NUMBER)
    {
        OutError = FText::FromString(
            TEXT("The canonical initial state contains invalid values."));
        return false;
    }

    OutAuthoredState = Canonical;
    OutAuthoredState.AuthoringFrameCatalogKey =
        SelectedFrameCatalogKey;
    if (SelectedFrameCatalogKey.IsNone())
    {
        OutError = FText::GetEmpty();
        return true;
    }

    TGInitialStateFramePrivate::FResolvedBodyFrame Frame;
    if (!TGInitialStateFramePrivate::ResolveBodyFrame(
            SelectedFrameCatalogKey,
            Scenario,
            Frame,
            OutError))
    {
        return false;
    }

    const tgsim::Vec3d PositionIcrf =
        TGInitialStateFramePrivate::ToBackend(
            Canonical.PositionMeters);
    const tgsim::Vec3d VelocityIcrf =
        TGInitialStateFramePrivate::ToBackend(
            Canonical.VelocityMetersPerSecond);
    const tgsim::Vec3d RelativePositionIcrf =
        PositionIcrf - Frame.origin_position_icrf_m;

    // r^F = R_I,F^T (r^I - r_origin^I).
    const tgsim::Vec3d PositionFrame =
        TGInitialStateFramePrivate::InverseTransformVector(
            Frame.body_fixed_to_icrf,
            RelativePositionIcrf);

    // v^F_rel = R_I,F^T [v^I - v_origin^I - omega_F/I^I x r_rel^I].
    const tgsim::Vec3d VelocityFrame =
        TGInitialStateFramePrivate::InverseTransformVector(
            Frame.body_fixed_to_icrf,
            VelocityIcrf - Frame.origin_velocity_icrf_mps -
                tgsim::Cross(
                    Frame.angular_velocity_icrf_radps,
                    RelativePositionIcrf));

    // Preserve the authored quaternion norm. Normalization is an explicit
    // action in WBP_QuaternionInput, not a side effect of frame conversion.
    const tgsim::Quatd BodyToIcrf =
        TGInitialStateFramePrivate::ToBackend(
            Canonical.AttitudeBodyToIcrf);

    // q_F,B = q_I,F* q_I,B, where * on q_I,F denotes conjugation.
    const tgsim::Quatd BodyToFrame =
        TGInitialStateFramePrivate::MultiplyQuaternions(
            TGInitialStateFramePrivate::ConjugateQuaternion(
                Frame.body_fixed_to_icrf_quaternion),
            BodyToIcrf);

    // omega_B/F^B = omega_B/I^B - R_I,B^T omega_F/I^I.
    const tgsim::Vec3d AngularVelocityRelativeBody =
        TGInitialStateFramePrivate::ToBackend(
            Canonical.AngularVelocityBodyRadiansPerSecond) -
        TGInitialStateFramePrivate::InverseRotateVector(
            BodyToIcrf,
            Frame.angular_velocity_icrf_radps);

    OutAuthoredState.PositionMeters =
        TGInitialStateFramePrivate::ToFrontend(PositionFrame);
    OutAuthoredState.VelocityMetersPerSecond =
        TGInitialStateFramePrivate::ToFrontend(VelocityFrame);
    OutAuthoredState.AttitudeBodyToIcrf =
        TGInitialStateFramePrivate::ToFrontend(BodyToFrame);
    OutAuthoredState.AngularVelocityBodyRadiansPerSecond =
        TGInitialStateFramePrivate::ToFrontend(
            AngularVelocityRelativeBody);
    OutError = FText::GetEmpty();
    return true;
}

bool UTGConfigInitialStateFrameWidgetBase::ConvertAuthoredToCanonical(
    const FTGSimulationScenario& Scenario,
    const FTGInitialSpacecraftState& InAuthoredState,
    FTGInitialSpacecraftState& OutCanonicalState,
    FText& OutError) const
{
    if (!TGInitialStateFramePrivate::IsFinite(
            InAuthoredState.PositionMeters) ||
        !TGInitialStateFramePrivate::IsFinite(
            InAuthoredState.VelocityMetersPerSecond) ||
        !TGInitialStateFramePrivate::IsFinite(
            InAuthoredState.AngularVelocityBodyRadiansPerSecond) ||
        !TGInitialStateFramePrivate::IsFinite(
            InAuthoredState.AttitudeBodyToIcrf) ||
        InAuthoredState.AttitudeBodyToIcrf.SizeSquared() <=
            UE_DOUBLE_SMALL_NUMBER)
    {
        OutError = FText::FromString(
            TEXT("Initial-state entries must be finite and the attitude quaternion must be nonzero."));
        return false;
    }

    OutCanonicalState = InAuthoredState;
    OutCanonicalState.AuthoringFrameCatalogKey =
        SelectedFrameCatalogKey;
    if (SelectedFrameCatalogKey.IsNone())
    {
        OutError = FText::GetEmpty();
        return true;
    }

    TGInitialStateFramePrivate::FResolvedBodyFrame Frame;
    if (!TGInitialStateFramePrivate::ResolveBodyFrame(
            SelectedFrameCatalogKey,
            Scenario,
            Frame,
            OutError))
    {
        return false;
    }

    const tgsim::Vec3d PositionFrame =
        TGInitialStateFramePrivate::ToBackend(
            InAuthoredState.PositionMeters);
    const tgsim::Vec3d VelocityFrame =
        TGInitialStateFramePrivate::ToBackend(
            InAuthoredState.VelocityMetersPerSecond);

    // r^I = r_origin^I + R_I,F r^F.
    const tgsim::Vec3d RelativePositionIcrf =
        TGInitialStateFramePrivate::TransformVector(
            Frame.body_fixed_to_icrf,
            PositionFrame);
    const tgsim::Vec3d PositionIcrf =
        Frame.origin_position_icrf_m + RelativePositionIcrf;

    // v^I = v_origin^I + R_I,F v^F_rel + omega_F/I^I x r_rel^I.
    const tgsim::Vec3d VelocityIcrf =
        Frame.origin_velocity_icrf_mps +
        TGInitialStateFramePrivate::TransformVector(
            Frame.body_fixed_to_icrf,
            VelocityFrame) +
        tgsim::Cross(
            Frame.angular_velocity_icrf_radps,
            RelativePositionIcrf);

    // Preserve the authored quaternion norm through the frame change.
    const tgsim::Quatd BodyToFrame =
        TGInitialStateFramePrivate::ToBackend(
            InAuthoredState.AttitudeBodyToIcrf);
    const tgsim::Quatd BodyToIcrf =
        TGInitialStateFramePrivate::MultiplyQuaternions(
            Frame.body_fixed_to_icrf_quaternion,
            BodyToFrame);

    // omega_B/I^B = omega_B/F^B + R_I,B^T omega_F/I^I.
    const tgsim::Vec3d AngularVelocityBody =
        TGInitialStateFramePrivate::ToBackend(
            InAuthoredState.AngularVelocityBodyRadiansPerSecond) +
        TGInitialStateFramePrivate::InverseRotateVector(
            BodyToIcrf,
            Frame.angular_velocity_icrf_radps);

    OutCanonicalState.PositionMeters =
        TGInitialStateFramePrivate::ToFrontend(PositionIcrf);
    OutCanonicalState.VelocityMetersPerSecond =
        TGInitialStateFramePrivate::ToFrontend(VelocityIcrf);
    OutCanonicalState.AttitudeBodyToIcrf =
        TGInitialStateFramePrivate::ToFrontend(BodyToIcrf);
    OutCanonicalState.AngularVelocityBodyRadiansPerSecond =
        TGInitialStateFramePrivate::ToFrontend(AngularVelocityBody);
    OutError = FText::GetEmpty();
    return true;
}

void UTGConfigInitialStateFrameWidgetBase::
HandleReferenceFrameSelectionChanged(
    const FString SelectedItem,
    const ESelectInfo::Type SelectionType)
{
    (void)SelectionType;
    if (bRefreshing)
    {
        return;
    }

    const FName* CatalogKey =
        FrameCatalogKeyByOption.Find(SelectedItem);
    if (CatalogKey == nullptr)
    {
        ShowError(FText::FromString(
            TEXT("The selected reference-frame option is not recognized.")));
        return;
    }

    SelectedFrameCatalogKey = *CatalogKey;
    LastAppliedFrameOption = SelectedItem;

    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        ShowError(FText::FromString(
            TEXT("No current simulation scenario draft is available.")));
        return;
    }

    FTGSimulationScenario Scenario =
        Subsystem->GetCurrentScenarioDraft();
    Scenario.InitialState.AuthoringFrameCatalogKey =
        SelectedFrameCatalogKey;
    Subsystem->SetCurrentScenarioDraft(Scenario);
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    RefreshFromCurrentDraft();
}

void UTGConfigInitialStateFrameWidgetBase::
HandleFramePositionCommitted(const FVector NewBackendValue)
{
    if (bRefreshing)
    {
        return;
    }
    AuthoredState.PositionMeters = NewBackendValue;
    CommitAuthoredState();
}

void UTGConfigInitialStateFrameWidgetBase::
HandleFrameVelocityCommitted(const FVector NewBackendValue)
{
    if (bRefreshing)
    {
        return;
    }
    AuthoredState.VelocityMetersPerSecond = NewBackendValue;
    CommitAuthoredState();
}

void UTGConfigInitialStateFrameWidgetBase::
HandleFrameAttitudeCommitted(const FQuat NewBackendValue)
{
    if (bRefreshing)
    {
        return;
    }
    AuthoredState.AttitudeBodyToIcrf = NewBackendValue;
    CommitAuthoredState();
}

void UTGConfigInitialStateFrameWidgetBase::
HandleFrameAngularVelocityCommitted(const FVector NewBackendValue)
{
    if (bRefreshing)
    {
        return;
    }
    AuthoredState.AngularVelocityBodyRadiansPerSecond =
        NewBackendValue;
    CommitAuthoredState();
}

bool UTGConfigInitialStateFrameWidgetBase::CommitAuthoredState()
{
    ClearError();
    UTGSimulationSubsystem* Subsystem = GetSimulationSubsystem();
    if (Subsystem == nullptr || !Subsystem->HasCurrentScenarioDraft())
    {
        ShowError(FText::FromString(
            TEXT("No current simulation scenario draft is available.")));
        return false;
    }

    FTGSimulationScenario Scenario =
        Subsystem->GetCurrentScenarioDraft();
    FTGInitialSpacecraftState CanonicalState;
    FText Error;
    if (!ConvertAuthoredToCanonical(
            Scenario,
            AuthoredState,
            CanonicalState,
            Error))
    {
        ShowError(Error);
        RefreshFromCurrentDraft();
        return false;
    }

    Scenario.InitialState = CanonicalState;
    Subsystem->SetCurrentScenarioDraft(Scenario);
    LastObservedDraftRevision =
        Subsystem->GetCurrentScenarioDraftRevision();
    return true;
}

void UTGConfigInitialStateFrameWidgetBase::RefreshInputWidgets()
{
    TGuardValue<bool> RefreshGuard(bRefreshing, true);
    FText Error;
    if (!SetVectorWidgetValue(
            VECTOR_Position,
            AuthoredState.PositionMeters,
            Error) ||
        !SetVectorWidgetValue(
            VECTOR_Velocity,
            AuthoredState.VelocityMetersPerSecond,
            Error) ||
        !SetQuaternionWidgetValue(
            QUAT_Attitude,
            AuthoredState.AttitudeBodyToIcrf,
            Error) ||
        !SetVectorWidgetValue(
            VECTOR_AngularVelocity,
            AuthoredState.AngularVelocityBodyRadiansPerSecond,
            Error))
    {
        ShowError(Error);
    }
}

bool UTGConfigInitialStateFrameWidgetBase::SetVectorWidgetValue(
    UUserWidget* VectorWidget,
    const FVector& Value,
    FText& OutError) const
{
    if (VectorWidget == nullptr)
    {
        OutError = FText::FromString(
            TEXT("A required WBP_Vector3Input widget is missing."));
        return false;
    }

    UFunction* Function =
        VectorWidget->FindFunction(TEXT("SetVectorValue"));
    if (Function == nullptr)
    {
        OutError = FText::FromString(
            TEXT("WBP_Vector3Input.SetVectorValue was not found."));
        return false;
    }

    struct FParameters
    {
        FVector NewBackendValue;
    } Parameters{Value};
    VectorWidget->ProcessEvent(Function, &Parameters);
    OutError = FText::GetEmpty();
    return true;
}

bool UTGConfigInitialStateFrameWidgetBase::SetQuaternionWidgetValue(
    UUserWidget* QuaternionWidget,
    const FQuat& Value,
    FText& OutError) const
{
    if (QuaternionWidget == nullptr)
    {
        OutError = FText::FromString(
            TEXT("The required WBP_QuaternionInput widget is missing."));
        return false;
    }

    UFunction* Function =
        QuaternionWidget->FindFunction(TEXT("SetQuaternionValue"));
    if (Function == nullptr)
    {
        OutError = FText::FromString(
            TEXT("WBP_QuaternionInput.SetQuaternionValue was not found."));
        return false;
    }

    struct FParameters
    {
        FQuat NewBackendValue;
    } Parameters{Value};
    QuaternionWidget->ProcessEvent(Function, &Parameters);
    OutError = FText::GetEmpty();
    return true;
}

bool UTGConfigInitialStateFrameWidgetBase::BindInputDispatchers(
    FText& OutError)
{
    return
        BindWidgetDispatcher(
            VECTOR_Position,
            TEXT("OnVectorCommitted"),
            GET_FUNCTION_NAME_CHECKED(
                UTGConfigInitialStateFrameWidgetBase,
                HandleFramePositionCommitted),
            OutError) &&
        BindWidgetDispatcher(
            VECTOR_Velocity,
            TEXT("OnVectorCommitted"),
            GET_FUNCTION_NAME_CHECKED(
                UTGConfigInitialStateFrameWidgetBase,
                HandleFrameVelocityCommitted),
            OutError) &&
        BindWidgetDispatcher(
            QUAT_Attitude,
            TEXT("OnQuaternionCommitted"),
            GET_FUNCTION_NAME_CHECKED(
                UTGConfigInitialStateFrameWidgetBase,
                HandleFrameAttitudeCommitted),
            OutError) &&
        BindWidgetDispatcher(
            VECTOR_AngularVelocity,
            TEXT("OnVectorCommitted"),
            GET_FUNCTION_NAME_CHECKED(
                UTGConfigInitialStateFrameWidgetBase,
                HandleFrameAngularVelocityCommitted),
            OutError);
}

void UTGConfigInitialStateFrameWidgetBase::UnbindInputDispatchers()
{
    UnbindWidgetDispatcher(
        VECTOR_Position,
        TEXT("OnVectorCommitted"),
        GET_FUNCTION_NAME_CHECKED(
            UTGConfigInitialStateFrameWidgetBase,
            HandleFramePositionCommitted));
    UnbindWidgetDispatcher(
        VECTOR_Velocity,
        TEXT("OnVectorCommitted"),
        GET_FUNCTION_NAME_CHECKED(
            UTGConfigInitialStateFrameWidgetBase,
            HandleFrameVelocityCommitted));
    UnbindWidgetDispatcher(
        QUAT_Attitude,
        TEXT("OnQuaternionCommitted"),
        GET_FUNCTION_NAME_CHECKED(
            UTGConfigInitialStateFrameWidgetBase,
            HandleFrameAttitudeCommitted));
    UnbindWidgetDispatcher(
        VECTOR_AngularVelocity,
        TEXT("OnVectorCommitted"),
        GET_FUNCTION_NAME_CHECKED(
            UTGConfigInitialStateFrameWidgetBase,
            HandleFrameAngularVelocityCommitted));
}

bool UTGConfigInitialStateFrameWidgetBase::BindWidgetDispatcher(
    UUserWidget* SourceWidget,
    const FName DispatcherName,
    const FName HandlerName,
    FText& OutError)
{
    if (SourceWidget == nullptr)
    {
        OutError = FText::FromString(FString::Printf(
            TEXT("The widget for dispatcher '%s' is missing."),
            *DispatcherName.ToString()));
        return false;
    }

    FMulticastDelegateProperty* DelegateProperty =
        FindFProperty<FMulticastDelegateProperty>(
            SourceWidget->GetClass(),
            DispatcherName);
    if (DelegateProperty == nullptr)
    {
        OutError = FText::FromString(FString::Printf(
            TEXT("Dispatcher '%s' was not found on '%s'."),
            *DispatcherName.ToString(),
            *SourceWidget->GetClass()->GetName()));
        return false;
    }

    void* DelegateValue =
        DelegateProperty->ContainerPtrToValuePtr<void>(SourceWidget);
    FScriptDelegate Handler;
    Handler.BindUFunction(this, HandlerName);
    DelegateProperty->RemoveDelegate(
        Handler,
        SourceWidget,
        DelegateValue);
    DelegateProperty->AddDelegate(
        Handler,
        SourceWidget,
        DelegateValue);
    OutError = FText::GetEmpty();
    return true;
}

void UTGConfigInitialStateFrameWidgetBase::UnbindWidgetDispatcher(
    UUserWidget* SourceWidget,
    const FName DispatcherName,
    const FName HandlerName)
{
    if (SourceWidget == nullptr)
    {
        return;
    }

    FMulticastDelegateProperty* DelegateProperty =
        FindFProperty<FMulticastDelegateProperty>(
            SourceWidget->GetClass(),
            DispatcherName);
    if (DelegateProperty == nullptr)
    {
        return;
    }

    void* DelegateValue =
        DelegateProperty->ContainerPtrToValuePtr<void>(SourceWidget);
    FScriptDelegate Handler;
    Handler.BindUFunction(this, HandlerName);
    DelegateProperty->RemoveDelegate(
        Handler,
        SourceWidget,
        DelegateValue);
}

FString UTGConfigInitialStateFrameWidgetBase::
GetSelectedFrameDisplayName() const
{
    if (SelectedFrameCatalogKey.IsNone())
    {
        return TGInitialStateFramePrivate::IcrfOption;
    }

    FTGCelestialCatalogEntry Entry;
    return UTGCelestialCatalogLibrary::FindCelestialCatalogEntry(
            SelectedFrameCatalogKey,
            Entry)
        ? Entry.DisplayName.ToString()
        : SelectedFrameCatalogKey.ToString();
}

void UTGConfigInitialStateFrameWidgetBase::UpdateDescription()
{
    if (SelectedFrameCatalogKey.IsNone())
    {
        if (TXT_TranslationHeading != nullptr)
        {
            TXT_TranslationHeading->SetText(
                FText::FromString(TEXT("TRANSLATIONAL STATE - ICRF")));
        }
        if (TXT_TranslationDescription != nullptr)
        {
            TXT_TranslationDescription->SetText(FText::FromString(
                TEXT("Provide the spacecraft's position and velocity in ICRF at Start UTC.")));
        }
        if (TXT_AttitudeHeading != nullptr)
        {
            TXT_AttitudeHeading->SetText(
                FText::FromString(
                    TEXT("ATTITUDE - SPACECRAFT TO ICRF")));
        }
        if (TXT_AttitudeDescription != nullptr)
        {
            TXT_AttitudeDescription->SetText(FText::FromString(
                TEXT("Provide the spacecraft's attitude and angular velocity in ICRF at Start UTC.")));
        }
        TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
            VECTOR_Position,
            TEXT("km - ICRF"));
        TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
            VECTOR_Velocity,
            TEXT("km/s - ICRF"));
        TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
            QUAT_Attitude,
            TEXT("Spacecraft to ICRF"));
        TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
            VECTOR_AngularVelocity,
            TEXT("rad/s - Spacecraft relative to ICRF, expressed in spacecraft frame"));
        if (TXT_ReferenceFrameDescription != nullptr)
        {
            TXT_ReferenceFrameDescription->SetText(FText::FromString(
                TEXT("Select the input frame in which the initial state is provided.")));
        }
        if (TXT_AngularVelocityLabel != nullptr)
        {
            TXT_AngularVelocityLabel->SetToolTipText(FText::FromString(
                TEXT("Spacecraft angular velocity relative to the selected reference frame, expressed in the spacecraft frame.")));
        }
        return;
    }

    const FString FrameName = GetSelectedFrameDisplayName();
    const bool bUsesDefiniteArticle =
        FrameName.Equals(TEXT("Earth"), ESearchCase::IgnoreCase) ||
        FrameName.Equals(TEXT("Moon"), ESearchCase::IgnoreCase) ||
        FrameName.Equals(TEXT("Sun"), ESearchCase::IgnoreCase);
    const FString FrameNameInProse = bUsesDefiniteArticle
        ? FString::Printf(TEXT("the %s"), *FrameName)
        : FrameName;
    if (TXT_TranslationHeading != nullptr)
    {
        TXT_TranslationHeading->SetText(FText::FromString(
            FString::Printf(
                TEXT("TRANSLATIONAL STATE - %s BODY-FIXED"),
                *FrameName.ToUpper())));
    }
    if (TXT_TranslationDescription != nullptr)
    {
        TXT_TranslationDescription->SetText(FText::FromString(
            FString::Printf(
                TEXT("Position is relative to the center of %s. Velocity is relative to the rotating %s body-fixed frame."),
                *FrameNameInProse,
                *FrameName)));
    }
    if (TXT_AttitudeHeading != nullptr)
    {
        TXT_AttitudeHeading->SetText(FText::FromString(
            FString::Printf(
                TEXT("ATTITUDE - SPACECRAFT TO %s BODY-FIXED"),
                *FrameName.ToUpper())));
    }
    if (TXT_AttitudeDescription != nullptr)
    {
        TXT_AttitudeDescription->SetText(FText::FromString(
            FString::Printf(
                TEXT("The quaternion maps the spacecraft frame to the %s body-fixed frame. Angular velocity is that of the spacecraft relative to that frame, expressed in the spacecraft frame."),
                *FrameName)));
    }
    TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
        VECTOR_Position,
        FString::Printf(TEXT("km - %s body-fixed"), *FrameName));
    TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
        VECTOR_Velocity,
        FString::Printf(
            TEXT("km/s - relative to rotating %s frame"),
            *FrameName));
    TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
        QUAT_Attitude,
        FString::Printf(
            TEXT("Spacecraft to %s body-fixed"),
            *FrameName));
    TGInitialStateFramePrivate::SetVectorUnitsAndFrame(
        VECTOR_AngularVelocity,
        FString::Printf(
            TEXT("rad/s - Spacecraft relative to %s body-fixed, expressed in spacecraft frame"),
            *FrameName));
    if (TXT_ReferenceFrameDescription != nullptr)
    {
        TXT_ReferenceFrameDescription->SetText(FText::FromString(
            FString::Printf(
                TEXT("Enter the complete initial state in the rotating %s body-fixed frame at Start UTC."),
                *FrameName)));
    }
    if (TXT_AngularVelocityLabel != nullptr)
    {
        TXT_AngularVelocityLabel->SetToolTipText(FText::FromString(
            TEXT("Spacecraft angular velocity relative to the selected reference frame, expressed in the spacecraft frame.")));
    }
}

void UTGConfigInitialStateFrameWidgetBase::ShowError(
    const FText& Error)
{
    if (TXT_ReferenceFrameError == nullptr)
    {
        return;
    }
    TXT_ReferenceFrameError->SetText(Error);
    TXT_ReferenceFrameError->SetVisibility(
        Error.IsEmpty()
            ? ESlateVisibility::Collapsed
            : ESlateVisibility::Visible);
}

void UTGConfigInitialStateFrameWidgetBase::ClearError()
{
    ShowError(FText::GetEmpty());
}
