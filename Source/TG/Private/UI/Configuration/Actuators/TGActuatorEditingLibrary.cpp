// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Actuators/TGActuatorEditingLibrary.h"
#include "UI/Configuration/Actuators/TGConfigActuatorsWidgetBase.h"

#include "Components/WidgetSwitcher.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace TGActuatorEditingPrivate
{
    constexpr double VectorMagnitudeTolerance = 1.0e-12;
    constexpr double MomentumTolerance = 1.0e-12;
    constexpr double CsvZeroTolerance = 1.0e-12;

    enum class EProfileValueRule : uint8
    {
        NonNegativeThrust,
        PositiveSpecificImpulse
    };

    bool NamesEqual(
        const FString& First,
        const FString& Second)
    {
        return First.Equals(
            Second,
            ESearchCase::IgnoreCase);
    }

    bool IsFiniteVector(const FVector& Value)
    {
        return
            FMath::IsFinite(Value.X) &&
            FMath::IsFinite(Value.Y) &&
            FMath::IsFinite(Value.Z);
    }

    const FTGComponentConfig* FindComponentByName(
        const FTGSimulationScenario& Scenario,
        const FString& Candidate)
    {
        for (const FTGComponentConfig& Component
             : Scenario.Components)
        {
            if (NamesEqual(
                    Component.Name,
                    Candidate))
            {
                return &Component;
            }
        }

        return nullptr;
    }

    FString MakeUniqueDefaultThrusterName(
        const FTGSimulationScenario& Scenario)
    {
        int32 Suffix = 1;

        while (true)
        {
            const FString Candidate =
                FString::Printf(
                    TEXT("Thruster %d"),
                    Suffix);

            bool bAlreadyUsed = false;

            for (const FTGThrusterConfig& Thruster
                 : Scenario.Thrusters)
            {
                if (NamesEqual(
                        Thruster.Name,
                        Candidate))
                {
                    bAlreadyUsed = true;
                    break;
                }
            }

            if (!bAlreadyUsed)
            {
                return Candidate;
            }

            ++Suffix;
        }
    }

    FString MakeUniqueDefaultWheelName(
        const FTGSimulationScenario& Scenario)
    {
        int32 Suffix = 1;

        while (true)
        {
            const FString Candidate =
                FString::Printf(
                    TEXT("Wheel %d"),
                    Suffix);

            bool bAlreadyUsed = false;

            for (const FTGReactionWheelConfig& Wheel
                 : Scenario.ReactionWheels)
            {
                if (NamesEqual(
                        Wheel.Name,
                        Candidate))
                {
                    bAlreadyUsed = true;
                    break;
                }
            }

            if (!bAlreadyUsed)
            {
                return Candidate;
            }

            ++Suffix;
        }
    }

    bool ParseFiniteDouble(
        FString ValueText,
        double& OutValue)
    {
        ValueText.TrimStartAndEndInline();

        if (!LexTryParseString(
                OutValue,
                *ValueText))
        {
            return false;
        }

        return FMath::IsFinite(OutValue);
    }

    bool ValidateProfileCsv(
        const FString& RawPath,
        const EProfileValueRule ValueRule,
        FString& OutNormalizedPath,
        FText& OutErrorText)
    {
        OutNormalizedPath.Reset();
        OutErrorText = FText::GetEmpty();

        FString Path = RawPath;
        Path.TrimStartAndEndInline();

        if (Path.IsEmpty())
        {
            OutErrorText = FText::FromString(
                TEXT("Select a CSV profile file."));
            return false;
        }

        Path = FPaths::ConvertRelativePathToFull(Path);
        FPaths::NormalizeFilename(Path);

        if (!IFileManager::Get().FileExists(*Path))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("CSV profile file does not exist: %s"),
                    *Path));
            return false;
        }

        TArray<FString> Lines;

        if (!FFileHelper::LoadFileToStringArray(
                Lines,
                *Path))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("Could not read CSV profile file: %s"),
                    *Path));
            return false;
        }

        int32 AcceptedSampleCount = 0;
        double PreviousTime = -TNumericLimits<double>::Max();
        double FirstValue = 0.0;
        double LastValue = 0.0;

        for (int32 LineIndex = 0;
             LineIndex < Lines.Num();
             ++LineIndex)
        {
            FString Line = Lines[LineIndex];
            Line.TrimStartAndEndInline();

            if (Line.IsEmpty())
            {
                continue;
            }

            TArray<FString> Columns;
            Line.ParseIntoArray(
                Columns,
                TEXT(","),
                false);

            if (Columns.Num() != 2)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "CSV line %d must contain exactly two "
                            "comma-separated values."),
                        LineIndex + 1));
                return false;
            }

            double TimeSeconds = 0.0;
            double SampleValue = 0.0;

            if (!ParseFiniteDouble(
                    Columns[0],
                    TimeSeconds) ||
                !ParseFiniteDouble(
                    Columns[1],
                    SampleValue))
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "CSV line %d contains an invalid numeric "
                            "value."),
                        LineIndex + 1));
                return false;
            }

            if (TimeSeconds < 0.0)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "CSV line %d has a negative time. Profile "
                            "times are measured from ignition."),
                        LineIndex + 1));
                return false;
            }

            if (AcceptedSampleCount > 0 &&
                TimeSeconds <= PreviousTime)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "CSV line %d must have a time greater than "
                            "the previous sample time."),
                        LineIndex + 1));
                return false;
            }

            if (ValueRule ==
                    EProfileValueRule::NonNegativeThrust &&
                SampleValue < 0.0)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "CSV line %d has negative thrust. Thrust "
                            "samples must be nonnegative."),
                        LineIndex + 1));
                return false;
            }

            if (ValueRule ==
                    EProfileValueRule::PositiveSpecificImpulse &&
                SampleValue <= 0.0)
            {
                OutErrorText = FText::FromString(
                    FString::Printf(
                        TEXT(
                            "CSV line %d has non-positive specific "
                            "impulse."),
                        LineIndex + 1));
                return false;
            }

            if (AcceptedSampleCount == 0)
            {
                FirstValue = SampleValue;
            }

            LastValue = SampleValue;
            PreviousTime = TimeSeconds;
            ++AcceptedSampleCount;
        }

        if (AcceptedSampleCount == 0)
        {
            OutErrorText = FText::FromString(
                TEXT("CSV profile file contains no samples."));
            return false;
        }

        if (ValueRule ==
            EProfileValueRule::NonNegativeThrust)
        {
            if (AcceptedSampleCount < 2)
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "A thrust CSV profile needs at least two "
                        "samples so both endpoints can be zero."));
                return false;
            }

            if (FMath::Abs(FirstValue) > CsvZeroTolerance ||
                FMath::Abs(LastValue) > CsvZeroTolerance)
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "The first and last thrust samples must both "
                        "be zero."));
                return false;
            }
        }

        OutNormalizedPath = MoveTemp(Path);
        return true;
    }

    bool ValidateScalarProfile(
        const FTGScalarProfileConfig& InputProfile,
        const EProfileValueRule ValueRule,
        FTGScalarProfileConfig& OutAcceptedProfile,
        FText& OutErrorText)
    {
        OutAcceptedProfile = InputProfile;
        OutErrorText = FText::GetEmpty();

        if (InputProfile.Source ==
            ETGScalarProfileSource::Constant)
        {
            if (!FMath::IsFinite(InputProfile.ConstantValue))
            {
                OutErrorText = FText::FromString(
                    TEXT("Profile constant must be finite."));
                return false;
            }

            if (ValueRule ==
                    EProfileValueRule::NonNegativeThrust &&
                InputProfile.ConstantValue < 0.0)
            {
                OutErrorText = FText::FromString(
                    TEXT("Constant thrust must be nonnegative."));
                return false;
            }

            if (ValueRule ==
                    EProfileValueRule::PositiveSpecificImpulse &&
                InputProfile.ConstantValue <= 0.0)
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "Constant specific impulse must be greater "
                        "than zero."));
                return false;
            }

            return true;
        }

        FString NormalizedPath;

        if (!ValidateProfileCsv(
                InputProfile.CsvFilePath,
                ValueRule,
                NormalizedPath,
                OutErrorText))
        {
            return false;
        }

        OutAcceptedProfile.CsvFilePath =
            MoveTemp(NormalizedPath);

        return true;
    }

    FDateTime ResolveThrusterTime(
        const FTGSimulationScenario& Scenario,
        const ETGThrusterTimeMode Mode,
        const FDateTime& AbsoluteUtc,
        const double ElapsedSeconds)
    {
        if (Mode == ETGThrusterTimeMode::AbsoluteUtc)
        {
            return AbsoluteUtc;
        }

        return Scenario.ScenarioAndSolver.StartUtc +
            FTimespan::FromSeconds(ElapsedSeconds);
    }
}

bool UTGActuatorEditingLibrary::ValidateThrustProfileCsv(
    const FString& CsvFilePath,
    FString& OutNormalizedPath,
    FText& OutErrorText)
{
    return TGActuatorEditingPrivate::ValidateProfileCsv(
        CsvFilePath,
        TGActuatorEditingPrivate::EProfileValueRule::NonNegativeThrust,
        OutNormalizedPath,
        OutErrorText);
}

bool UTGActuatorEditingLibrary::ValidateSpecificImpulseProfileCsv(
    const FString& CsvFilePath,
    FString& OutNormalizedPath,
    FText& OutErrorText)
{
    return TGActuatorEditingPrivate::ValidateProfileCsv(
        CsvFilePath,
        TGActuatorEditingPrivate::EProfileValueRule::PositiveSpecificImpulse,
        OutNormalizedPath,
        OutErrorText);
}

TArray<FString>
UTGActuatorEditingLibrary::
GetActuatorMountComponentNames(
    const FTGSimulationScenario& Scenario)
{
    TArray<FString> Names;
    Names.Reserve(Scenario.Components.Num());

    for (const FTGComponentConfig& Component
         : Scenario.Components)
    {
        Names.Add(Component.Name);
    }

    return Names;
}

TArray<FString>
UTGActuatorEditingLibrary::
GetVariableMassComponentNames(
    const FTGSimulationScenario& Scenario)
{
    TArray<FString> Names;

    for (const FTGComponentConfig& Component
         : Scenario.Components)
    {
        if (Component.bVariableMass)
        {
            Names.Add(Component.Name);
        }
    }

    return Names;
}

TArray<FString>
UTGActuatorEditingLibrary::
GetThrusterNames(
    const FTGSimulationScenario& Scenario)
{
    TArray<FString> Names;
    Names.Reserve(Scenario.Thrusters.Num());

    for (const FTGThrusterConfig& Thruster
         : Scenario.Thrusters)
    {
        Names.Add(Thruster.Name);
    }

    return Names;
}

bool UTGActuatorEditingLibrary::
IsValidThrusterIndex(
    const FTGSimulationScenario& Scenario,
    const int32 ThrusterIndex)
{
    return Scenario.Thrusters.IsValidIndex(
        ThrusterIndex);
}

bool UTGActuatorEditingLibrary::
AddDefaultThruster(
    FTGSimulationScenario& Scenario,
    int32& OutNewThrusterIndex,
    FText& OutErrorText)
{
    OutNewThrusterIndex = INDEX_NONE;
    OutErrorText = FText::GetEmpty();

    if (Scenario.Components.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Create the Component Tree before adding "
                "thrusters."));
        return false;
    }

    const TArray<FString> VariableMassComponents =
        GetVariableMassComponentNames(Scenario);

    if (VariableMassComponents.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Mark at least one Component Tree component as "
                "Variable Mass before adding a thruster."));
        return false;
    }

    FTGThrusterConfig NewThruster;
    NewThruster.Name =
        TGActuatorEditingPrivate::
            MakeUniqueDefaultThrusterName(Scenario);

    NewThruster.Mode =
        ETGThrusterMode::PrescribedProfile;

    NewThruster.MountComponentName =
        Scenario.Components[0].Name;

    NewThruster.PropellantComponentName =
        VariableMassComponents[0];

    NewThruster.ApplicationPointMeters =
        FVector::ZeroVector;

    NewThruster.Direction =
        FVector::ForwardVector;

    NewThruster.IgnitionTimeMode =
        ETGThrusterTimeMode::ElapsedSimulationTime;

    NewThruster.IgnitionElapsedSeconds = 0.0;
    NewThruster.IgnitionUtc =
        Scenario.ScenarioAndSolver.StartUtc;

    NewThruster.bNeverShutsDown = true;

    NewThruster.ShutdownTimeMode =
        ETGThrusterTimeMode::ElapsedSimulationTime;

    NewThruster.ShutdownElapsedSeconds = 1.0;
    NewThruster.ShutdownUtc =
        Scenario.ScenarioAndSolver.StartUtc +
        FTimespan::FromSeconds(1.0);

    NewThruster.PrescribedThrust.Source =
        ETGScalarProfileSource::Constant;
    NewThruster.PrescribedThrust.ConstantValue = 0.0;

    NewThruster.PrescribedSpecificImpulse.Source =
        ETGScalarProfileSource::Constant;
    NewThruster.PrescribedSpecificImpulse.ConstantValue = 1.0;

    // Hidden while prescribed, but initialized valid so changing mode is atomic.
    NewThruster.MaximumThrustNewtons = 1.0;

    Scenario.Thrusters.Add(NewThruster);
    OutNewThrusterIndex =
        Scenario.Thrusters.Num() - 1;

    return true;
}

bool UTGActuatorEditingLibrary::
GetThrusterAtIndex(
    const FTGSimulationScenario& Scenario,
    const int32 ThrusterIndex,
    FTGThrusterConfig& OutThruster)
{
    if (!Scenario.Thrusters.IsValidIndex(
            ThrusterIndex))
    {
        OutThruster = FTGThrusterConfig{};
        return false;
    }

    OutThruster =
        Scenario.Thrusters[ThrusterIndex];

    return true;
}

bool UTGActuatorEditingLibrary::
ValidateThruster(
    const FTGSimulationScenario& Scenario,
    const FTGThrusterConfig& Thruster,
    FTGThrusterConfig& OutAcceptedThruster,
    FText& OutWarningText,
    FText& OutErrorText)
{
    OutAcceptedThruster = Thruster;
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    OutAcceptedThruster.Name.TrimStartAndEndInline();
    OutAcceptedThruster.MountComponentName.TrimStartAndEndInline();
    OutAcceptedThruster.PropellantComponentName.TrimStartAndEndInline();

    if (OutAcceptedThruster.Name.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT("Thruster name cannot be empty."));
        return false;
    }

    const FTGComponentConfig* MountComponent =
        TGActuatorEditingPrivate::FindComponentByName(
            Scenario,
            OutAcceptedThruster.MountComponentName);

    if (MountComponent == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT("Select an existing mount component."));
        return false;
    }

    OutAcceptedThruster.MountComponentName =
        MountComponent->Name;

    const FTGComponentConfig* PropellantComponent =
        TGActuatorEditingPrivate::FindComponentByName(
            Scenario,
            OutAcceptedThruster.PropellantComponentName);

    if (PropellantComponent == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT("Select an existing propellant component."));
        return false;
    }

    if (!PropellantComponent->bVariableMass)
    {
        OutErrorText = FText::FromString(
            FString::Printf(
                TEXT(
                    "Propellant component '%s' must be marked "
                    "Variable Mass in the Component Tree."),
                *PropellantComponent->Name));
        return false;
    }

    OutAcceptedThruster.PropellantComponentName =
        PropellantComponent->Name;

    if (!TGActuatorEditingPrivate::IsFiniteVector(
            OutAcceptedThruster.ApplicationPointMeters))
    {
        OutErrorText = FText::FromString(
            TEXT("Thruster application point must be finite."));
        return false;
    }

    if (!TGActuatorEditingPrivate::IsFiniteVector(
            OutAcceptedThruster.Direction))
    {
        OutErrorText = FText::FromString(
            TEXT("Thruster direction must be finite."));
        return false;
    }

    const double DirectionMagnitude =
        OutAcceptedThruster.Direction.Size();

    if (!FMath::IsFinite(DirectionMagnitude) ||
        DirectionMagnitude <=
            TGActuatorEditingPrivate::VectorMagnitudeTolerance)
    {
        OutErrorText = FText::FromString(
            TEXT("Thruster direction must be a non-zero vector."));
        return false;
    }

    OutAcceptedThruster.Direction /=
        DirectionMagnitude;

    if (OutAcceptedThruster.IgnitionTimeMode ==
        ETGThrusterTimeMode::ElapsedSimulationTime)
    {
        if (!FMath::IsFinite(
                OutAcceptedThruster.IgnitionElapsedSeconds) ||
            OutAcceptedThruster.IgnitionElapsedSeconds < 0.0)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Ignition elapsed time must be a finite "
                    "nonnegative value."));
            return false;
        }
    }
    else if (OutAcceptedThruster.IgnitionUtc ==
             FDateTime::MinValue())
    {
        OutErrorText = FText::FromString(
            TEXT("Enter a valid ignition UTC time."));
        return false;
    }

    if (!OutAcceptedThruster.bNeverShutsDown)
    {
        if (OutAcceptedThruster.ShutdownTimeMode ==
            ETGThrusterTimeMode::ElapsedSimulationTime)
        {
            if (!FMath::IsFinite(
                    OutAcceptedThruster.ShutdownElapsedSeconds) ||
                OutAcceptedThruster.ShutdownElapsedSeconds < 0.0)
            {
                OutErrorText = FText::FromString(
                    TEXT(
                        "Shutdown elapsed time must be a finite "
                        "nonnegative value."));
                return false;
            }
        }
        else if (OutAcceptedThruster.ShutdownUtc ==
                 FDateTime::MinValue())
        {
            OutErrorText = FText::FromString(
                TEXT("Enter a valid shutdown UTC time."));
            return false;
        }

        const FDateTime IgnitionTime =
            TGActuatorEditingPrivate::ResolveThrusterTime(
                Scenario,
                OutAcceptedThruster.IgnitionTimeMode,
                OutAcceptedThruster.IgnitionUtc,
                OutAcceptedThruster.IgnitionElapsedSeconds);

        const FDateTime ShutdownTime =
            TGActuatorEditingPrivate::ResolveThrusterTime(
                Scenario,
                OutAcceptedThruster.ShutdownTimeMode,
                OutAcceptedThruster.ShutdownUtc,
                OutAcceptedThruster.ShutdownElapsedSeconds);

        if (ShutdownTime <= IgnitionTime)
        {
            OutErrorText = FText::FromString(
                TEXT("Shutdown time must be after ignition time."));
            return false;
        }
    }

    if (OutAcceptedThruster.Mode ==
        ETGThrusterMode::PrescribedProfile)
    {
        FTGScalarProfileConfig AcceptedThrustProfile;
        FText ProfileError;

        if (!TGActuatorEditingPrivate::ValidateScalarProfile(
                OutAcceptedThruster.PrescribedThrust,
                TGActuatorEditingPrivate::
                    EProfileValueRule::NonNegativeThrust,
                AcceptedThrustProfile,
                ProfileError))
        {
            OutErrorText = FText::FromString(
                FString(TEXT("Thrust profile: ")) +
                ProfileError.ToString());
            return false;
        }

        FTGScalarProfileConfig AcceptedIspProfile;

        if (!TGActuatorEditingPrivate::ValidateScalarProfile(
                OutAcceptedThruster.PrescribedSpecificImpulse,
                TGActuatorEditingPrivate::
                    EProfileValueRule::PositiveSpecificImpulse,
                AcceptedIspProfile,
                ProfileError))
        {
            OutErrorText = FText::FromString(
                FString(TEXT("Specific impulse profile: ")) +
                ProfileError.ToString());
            return false;
        }

        OutAcceptedThruster.PrescribedThrust =
            MoveTemp(AcceptedThrustProfile);

        OutAcceptedThruster.PrescribedSpecificImpulse =
            MoveTemp(AcceptedIspProfile);
    }
    else
    {
        if (!FMath::IsFinite(
                OutAcceptedThruster.MaximumThrustNewtons) ||
            OutAcceptedThruster.MaximumThrustNewtons <= 0.0)
        {
            OutErrorText = FText::FromString(
                TEXT(
                    "Maximum commanded thrust must be finite and "
                    "greater than zero."));
            return false;
        }
    }

    return true;
}

bool UTGActuatorEditingLibrary::
ApplyThruster(
    FTGSimulationScenario& Scenario,
    const int32 ThrusterIndex,
    const FTGThrusterConfig& UpdatedThruster,
    FTGThrusterConfig& OutAcceptedThruster,
    FText& OutWarningText,
    FText& OutErrorText)
{
    OutAcceptedThruster = UpdatedThruster;
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    if (!Scenario.Thrusters.IsValidIndex(
            ThrusterIndex))
    {
        OutErrorText = FText::FromString(
            TEXT("The selected thruster no longer exists."));
        return false;
    }

    if (!ValidateThruster(
            Scenario,
            UpdatedThruster,
            OutAcceptedThruster,
            OutWarningText,
            OutErrorText))
    {
        return false;
    }

    for (int32 Index = 0;
         Index < Scenario.Thrusters.Num();
         ++Index)
    {
        if (Index == ThrusterIndex)
        {
            continue;
        }

        if (TGActuatorEditingPrivate::NamesEqual(
                Scenario.Thrusters[Index].Name,
                OutAcceptedThruster.Name))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT("A thruster named '%s' already exists."),
                    *OutAcceptedThruster.Name));
            return false;
        }
    }

    Scenario.Thrusters[ThrusterIndex] =
        OutAcceptedThruster;

    return true;
}

bool UTGActuatorEditingLibrary::
DeleteThruster(
    FTGSimulationScenario& Scenario,
    const int32 ThrusterIndex,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (!Scenario.Thrusters.IsValidIndex(
            ThrusterIndex))
    {
        OutErrorText = FText::FromString(
            TEXT("The selected thruster no longer exists."));
        return false;
    }

    Scenario.Thrusters.RemoveAt(ThrusterIndex);
    return true;
}

TArray<FString>
UTGActuatorEditingLibrary::
GetReactionWheelNames(
    const FTGSimulationScenario& Scenario)
{
    TArray<FString> Names;
    Names.Reserve(Scenario.ReactionWheels.Num());

    for (const FTGReactionWheelConfig& Wheel
         : Scenario.ReactionWheels)
    {
        Names.Add(Wheel.Name);
    }

    return Names;
}

bool UTGActuatorEditingLibrary::
IsValidReactionWheelIndex(
    const FTGSimulationScenario& Scenario,
    const int32 WheelIndex)
{
    return Scenario.ReactionWheels.IsValidIndex(
        WheelIndex);
}

bool UTGActuatorEditingLibrary::
AddDefaultReactionWheel(
    FTGSimulationScenario& Scenario,
    int32& OutNewWheelIndex,
    FText& OutErrorText)
{
    OutNewWheelIndex = INDEX_NONE;
    OutErrorText = FText::GetEmpty();

    if (Scenario.Components.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Create the Component Tree before adding "
                "wheels."));
        return false;
    }

    FTGReactionWheelConfig NewWheel;
    NewWheel.Name =
        TGActuatorEditingPrivate::
            MakeUniqueDefaultWheelName(Scenario);

    NewWheel.MountComponentName =
        Scenario.Components[0].Name;

    NewWheel.Axis = FVector::ForwardVector;
    NewWheel.InitialMomentumNewtonMeterSeconds = 0.0;
    NewWheel.MaximumAbsoluteMomentumNewtonMeterSeconds = 0.0;

    Scenario.ReactionWheels.Add(NewWheel);
    OutNewWheelIndex =
        Scenario.ReactionWheels.Num() - 1;

    return true;
}

bool UTGActuatorEditingLibrary::
GetReactionWheelAtIndex(
    const FTGSimulationScenario& Scenario,
    const int32 WheelIndex,
    FTGReactionWheelConfig& OutWheel)
{
    if (!Scenario.ReactionWheels.IsValidIndex(
            WheelIndex))
    {
        OutWheel = FTGReactionWheelConfig{};
        return false;
    }

    OutWheel =
        Scenario.ReactionWheels[WheelIndex];

    return true;
}

bool UTGActuatorEditingLibrary::
ValidateReactionWheel(
    const FTGSimulationScenario& Scenario,
    const FTGReactionWheelConfig& Wheel,
    FTGReactionWheelConfig& OutAcceptedWheel,
    FText& OutWarningText,
    FText& OutErrorText)
{
    OutAcceptedWheel = Wheel;
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    OutAcceptedWheel.Name.TrimStartAndEndInline();
    OutAcceptedWheel.MountComponentName.TrimStartAndEndInline();

    if (OutAcceptedWheel.Name.IsEmpty())
    {
        OutErrorText = FText::FromString(
            TEXT("Wheel name cannot be empty."));
        return false;
    }

    const FTGComponentConfig* MountComponent =
        TGActuatorEditingPrivate::FindComponentByName(
            Scenario,
            OutAcceptedWheel.MountComponentName);

    if (MountComponent == nullptr)
    {
        OutErrorText = FText::FromString(
            TEXT("Select an existing mount component."));
        return false;
    }

    OutAcceptedWheel.MountComponentName =
        MountComponent->Name;

    if (!TGActuatorEditingPrivate::IsFiniteVector(
            OutAcceptedWheel.Axis))
    {
        OutErrorText = FText::FromString(
            TEXT("Wheel direction must be finite."));
        return false;
    }

    const double AxisMagnitude =
        OutAcceptedWheel.Axis.Size();

    if (!FMath::IsFinite(AxisMagnitude) ||
        AxisMagnitude <=
            TGActuatorEditingPrivate::VectorMagnitudeTolerance)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Wheel direction must be a non-zero "
                "vector."));
        return false;
    }

    OutAcceptedWheel.Axis /=
        AxisMagnitude;

    if (!FMath::IsFinite(
            OutAcceptedWheel.InitialMomentumNewtonMeterSeconds))
    {
        OutErrorText = FText::FromString(
            TEXT("Initial wheel momentum must be finite."));
        return false;
    }

    if (!FMath::IsFinite(
            OutAcceptedWheel.MaximumAbsoluteMomentumNewtonMeterSeconds))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Maximum absolute wheel momentum must be "
                "finite."));
        return false;
    }

    if (OutAcceptedWheel.MaximumAbsoluteMomentumNewtonMeterSeconds <
        0.0)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "Maximum absolute wheel momentum cannot be "
                "negative."));
        return false;
    }

    if (FMath::Abs(
            OutAcceptedWheel.InitialMomentumNewtonMeterSeconds) >
        OutAcceptedWheel.MaximumAbsoluteMomentumNewtonMeterSeconds +
            TGActuatorEditingPrivate::MomentumTolerance)
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The absolute initial wheel momentum cannot "
                "exceed the maximum absolute wheel momentum."));
        return false;
    }

    return true;
}

bool UTGActuatorEditingLibrary::
ApplyReactionWheel(
    FTGSimulationScenario& Scenario,
    const int32 WheelIndex,
    const FTGReactionWheelConfig& UpdatedWheel,
    FTGReactionWheelConfig& OutAcceptedWheel,
    FText& OutWarningText,
    FText& OutErrorText)
{
    OutAcceptedWheel = UpdatedWheel;
    OutWarningText = FText::GetEmpty();
    OutErrorText = FText::GetEmpty();

    if (!Scenario.ReactionWheels.IsValidIndex(
            WheelIndex))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected wheel no longer "
                "exists."));
        return false;
    }

    if (!ValidateReactionWheel(
            Scenario,
            UpdatedWheel,
            OutAcceptedWheel,
            OutWarningText,
            OutErrorText))
    {
        return false;
    }

    for (int32 Index = 0;
         Index < Scenario.ReactionWheels.Num();
         ++Index)
    {
        if (Index == WheelIndex)
        {
            continue;
        }

        if (TGActuatorEditingPrivate::NamesEqual(
                Scenario.ReactionWheels[Index].Name,
                OutAcceptedWheel.Name))
        {
            OutErrorText = FText::FromString(
                FString::Printf(
                    TEXT(
                        "A wheel named '%s' already "
                        "exists."),
                    *OutAcceptedWheel.Name));
            return false;
        }
    }

    Scenario.ReactionWheels[WheelIndex] =
        OutAcceptedWheel;

    return true;
}

bool UTGActuatorEditingLibrary::
DeleteReactionWheel(
    FTGSimulationScenario& Scenario,
    const int32 WheelIndex,
    FText& OutErrorText)
{
    OutErrorText = FText::GetEmpty();

    if (!Scenario.ReactionWheels.IsValidIndex(
            WheelIndex))
    {
        OutErrorText = FText::FromString(
            TEXT(
                "The selected wheel no longer "
                "exists."));
        return false;
    }

    Scenario.ReactionWheels.RemoveAt(WheelIndex);
    return true;
}

bool UTGActuatorEditingLibrary::
RefreshActuatorsPanelInSwitcher(
    UWidgetSwitcher* Switcher)
{
    if (Switcher == nullptr)
    {
        return false;
    }

    const int32 ChildCount =
        Switcher->GetChildrenCount();

    for (int32 ChildIndex = 0;
         ChildIndex < ChildCount;
         ++ChildIndex)
    {
        UTGConfigActuatorsWidgetBase* ActuatorsPanel =
            Cast<UTGConfigActuatorsWidgetBase>(
                Switcher->GetChildAt(ChildIndex));

        if (ActuatorsPanel != nullptr)
        {
            ActuatorsPanel->RefreshFromCurrentDraft();
            return true;
        }
    }

    return false;
}
