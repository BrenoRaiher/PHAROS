// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Review/TGScenarioReviewValidationLibrary.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/PanelWidget.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "Misc/FileHelper.h"
#include "Simulation/ComponentTree/TGComponentTreeValidationLibrary.h"
#include "Simulation/Control/TGControllerLibrarySubsystem.h"
#include "Simulation/TGCelestialCatalogLibrary.h"
#include "Simulation/TGGravityCoverageLibrary.h"
#include "Simulation/TGHarmonicCsvLibrary.h"
#include "Simulation/TGScalarProfileCsvLibrary.h"
#include "Simulation/TGSimulationSubsystem.h"
#include "SpiceBridge.h"
#include "UI/Configuration/Actuators/TGActuatorEditingLibrary.h"
#include "UI/Configuration/Controls/TGControlEditingLibrary.h"
#include "UI/Configuration/Environment/TGEnvironmentEditingLibrary.h"
#include "UI/Configuration/Environment/TGSolarRadiationPressureEditingLibrary.h"
#include "UI/Configuration/Review/TGConfigReviewWidgetBase.h"
#include "UI/Configuration/Review/TGScenarioReviewNavigationTarget.h"

namespace TGScenarioReview
{
    constexpr double UnitTolerance = 1.0e-6;
    constexpr double ZeroTolerance = 1.0e-12;

    template <typename WidgetType>
    WidgetType* FindWidgetDescendantOfType(UWidget* Root)
    {
        if (WidgetType* Match = Cast<WidgetType>(Root))
        {
            return Match;
        }

        if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
        {
            for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
            {
                if (WidgetType* Match = FindWidgetDescendantOfType<WidgetType>(
                        Panel->GetChildAt(Index)))
                {
                    return Match;
                }
            }
        }
        return nullptr;
    }

    UWidget* FindReviewNavigationTarget(UWidget* Root)
    {
        if (Root == nullptr)
        {
            return nullptr;
        }
        if (Root->GetClass()->ImplementsInterface(
                UTGScenarioReviewNavigationTarget::StaticClass()))
        {
            return Root;
        }

        if (UPanelWidget* Panel = Cast<UPanelWidget>(Root))
        {
            for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
            {
                if (UWidget* Match = FindReviewNavigationTarget(
                        Panel->GetChildAt(Index)))
                {
                    return Match;
                }
            }
        }
        return nullptr;
    }

    bool IsFinite(double Value)
    {
        return FMath::IsFinite(Value);
    }

    bool IsFinite(const FVector& Value)
    {
        return IsFinite(Value.X) && IsFinite(Value.Y) && IsFinite(Value.Z);
    }

    bool IsFinite(const FQuat& Value)
    {
        return IsFinite(Value.X) && IsFinite(Value.Y)
            && IsFinite(Value.Z) && IsFinite(Value.W);
    }

    void Add(
        FTGScenarioReviewReport& Report,
        ETGScenarioReviewSeverity Severity,
        const TCHAR* Code,
        const FString& Path,
        const FString& Message,
        ETGScenarioReviewSection Section,
        FGuid ComponentId = FGuid{},
        FGuid DofId = FGuid{},
        int32 ArrayIndex = INDEX_NONE,
        FName ObjectId = NAME_None,
        uint8 LogicalRegion = 0,
        int32 StableTriangleIndex = INDEX_NONE)
    {
        FTGScenarioReviewIssue& Issue = Report.Issues.AddDefaulted_GetRef();
        Issue.Severity = Severity;
        Issue.Code = FName(Code);
        Issue.Path = Path;
        Issue.Message = FText::FromString(Message);
        Issue.Section = Section;
        Issue.ComponentId = ComponentId;
        Issue.DofId = DofId;
        Issue.ArrayIndex = ArrayIndex;
        Issue.ObjectId = ObjectId;
        Issue.LogicalRegion = LogicalRegion;
        Issue.StableTriangleIndex = StableTriangleIndex;

        if (Severity == ETGScenarioReviewSeverity::Error)
        {
            ++Report.ErrorCount;
        }
        else
        {
            ++Report.WarningCount;
        }
    }

    FString EffectiveIssuePath(
        const FString& Prefix,
        const FString& HelperPath)
    {
        if (HelperPath.IsEmpty())
        {
            return Prefix;
        }
        if (HelperPath.StartsWith(TEXT("Components")))
        {
            return HelperPath;
        }
        return Prefix + TEXT(".") + HelperPath;
    }

    int32 ExtractLeadingArrayIndex(
        const FString& Path,
        const TCHAR* Prefix)
    {
        if (!Path.StartsWith(Prefix))
            return INDEX_NONE;
        const int32 Start = FCString::Strlen(Prefix);
        int32 End = INDEX_NONE;
        if (!Path.FindChar(TEXT(']'), End) || End <= Start)
            return INDEX_NONE;
        const FString IndexText = Path.Mid(Start, End - Start);
        return IndexText.IsNumeric() ? FCString::Atoi(*IndexText) : INDEX_NONE;
    }

    const TCHAR* InferComponentCode(const FTGComponentTreeValidationIssue& Issue)
    {
        const FString& Path = Issue.Path;
        const FString Message = Issue.Message.ToString();

        if (Path == TEXT("Components")) return TEXT("CMP-001");
        if (Path == TEXT("Components.TotalInitialMassKilograms"))
            return TEXT("CMP-009");
        if (Path == TEXT("Components.MinimumReachableMassKilograms"))
            return TEXT("CMP-012");
        if (Path.EndsWith(TEXT(".ComponentId")))
            return Message.Contains(TEXT("duplicated")) ? TEXT("CMP-003") : TEXT("CMP-002");
        if (Path.EndsWith(TEXT(".Name")) && !Path.Contains(TEXT("DegreesOfFreedom")))
            return Message.Contains(TEXT("duplicated")) ? TEXT("CMP-005") : TEXT("CMP-004");
        if (Path.EndsWith(TEXT(".InitialMassKilograms"))) return TEXT("CMP-006");
        if (Path.EndsWith(TEXT(".MinimumMassKilograms")))
            return Message.Contains(TEXT("exceed")) ? TEXT("CMP-008") : TEXT("CMP-007");
        if (Path.EndsWith(TEXT(".LocalCenterOfMassMeters"))) return TEXT("CMP-013");
        if (Path.EndsWith(TEXT(".CentroidalInertia")))
        {
            if (Message.Contains(TEXT("triangle"), ESearchCase::IgnoreCase)) return TEXT("CMP-016");
            if (Message.Contains(TEXT("nonnegative"), ESearchCase::IgnoreCase)
                || Message.Contains(TEXT("positive"), ESearchCase::IgnoreCase))
                return TEXT("CMP-015");
            return TEXT("CMP-014");
        }
        if (Path.EndsWith(TEXT(".DofId")))
            return Message.Contains(TEXT("duplicated")) ? TEXT("DOF-002") : TEXT("DOF-001");
        if (Path.Contains(TEXT("DegreesOfFreedom")) && Path.EndsWith(TEXT(".Name")))
            return Message.Contains(TEXT("duplicated")) ? TEXT("DOF-004") : TEXT("DOF-003");
        if (Path.EndsWith(TEXT(".MotionType"))) return TEXT("DOF-005");
        if (Path.EndsWith(TEXT(".Axis")))
            return Issue.Severity == ETGComponentTreeIssueSeverity::Warning ? TEXT("DOF-007") : TEXT("DOF-006");
        if (Path.EndsWith(TEXT(".InitialRate")))
            return Message.Contains(TEXT("exceed")) ? TEXT("DOF-011") : TEXT("DOF-009");
        if (Path.EndsWith(TEXT(".MaximumAbsoluteRate"))) return TEXT("DOF-010");
        if (Path.EndsWith(TEXT(".MaximumAbsoluteEffort"))) return TEXT("DOF-012");
        if (Path.EndsWith(TEXT(".MinimumCoordinate"))) return TEXT("DOF-013");
        if (Path.EndsWith(TEXT(".MaximumCoordinate"))) return TEXT("DOF-014");
        if (Path.EndsWith(TEXT(".CoordinateLimits"))) return TEXT("DOF-015");
        if (Path.EndsWith(TEXT(".InitialCoordinate"))) return TEXT("DOF-016");
        if (Path.EndsWith(TEXT(".ParentAnchorMeters"))) return TEXT("TREE-009");
        if (Path.EndsWith(TEXT(".ChildAnchorMeters"))) return TEXT("TREE-010");
        if (Path.EndsWith(TEXT(".ChildToParentZeroOrientation")))
            return Issue.Severity == ETGComponentTreeIssueSeverity::Warning ? TEXT("TREE-012") : TEXT("TREE-011");
        if (Path.EndsWith(TEXT(".OriginInBodyMeters"))) return TEXT("TREE-003");
        if (Path.EndsWith(TEXT(".ComponentToBodyOrientation")))
            return Issue.Severity == ETGComponentTreeIssueSeverity::Warning ? TEXT("TREE-005") : TEXT("TREE-004");
        if (Path.EndsWith(TEXT(".DegreesOfFreedom"))) return TEXT("TREE-002");
        if (Path.EndsWith(TEXT(".ParentComponentName")))
        {
            if (Message.Contains(TEXT("main"), ESearchCase::IgnoreCase)) return TEXT("TREE-001");
            if (Message.Contains(TEXT("earlier"), ESearchCase::IgnoreCase)) return TEXT("TREE-008");
            if (Message.Contains(TEXT("exist"), ESearchCase::IgnoreCase)) return TEXT("TREE-007");
            return TEXT("TREE-006");
        }
        if (Path.EndsWith(TEXT(".GeometrySource"))) return TEXT("VIS-001");
        if (Path.EndsWith(TEXT(".BoxDimensionsMeters"))) return TEXT("VIS-002");
        if (Path.EndsWith(TEXT(".SphereRadiusMeters"))) return TEXT("VIS-003");
        if (Path.EndsWith(TEXT(".CylinderRadiusMeters"))) return TEXT("VIS-004");
        if (Path.EndsWith(TEXT(".CylinderLengthMeters"))) return TEXT("VIS-005");
        if (Path.EndsWith(TEXT(".StlLengthUnit"))) return TEXT("VIS-010");
        if (Path.EndsWith(TEXT(".StlRecenterMode"))) return TEXT("VIS-011");
        if (Path.EndsWith(TEXT(".VisualOffsetMeters"))) return TEXT("VIS-012");
        if (Path.EndsWith(TEXT(".VisualOrientation"))) return TEXT("VIS-013");
        if (Path.EndsWith(TEXT(".VisualScale"))) return TEXT("VIS-014");
        if (Path.EndsWith(TEXT(".DisplayColor"))) return TEXT("VIS-015");
        if (Path.EndsWith(TEXT(".BaseColorTint"))) return TEXT("VIS-016");
        if (Path.EndsWith(TEXT(".SurfaceAppearanceMode"))) return TEXT("VIS-017");
        if (Path.EndsWith(TEXT(".PrimitiveType"))) return TEXT("VIS-021");
        if (Path.Contains(TEXT("TextureFilePath")))
            return Message.Contains(TEXT("extension"), ESearchCase::IgnoreCase) ? TEXT("VIS-019") : TEXT("VIS-020");
        if (Path.EndsWith(TEXT(".StlFilePath")))
        {
            if (Message.Contains(TEXT("required"), ESearchCase::IgnoreCase)) return TEXT("VIS-006");
            if (Message.Contains(TEXT("extension"), ESearchCase::IgnoreCase)) return TEXT("VIS-007");
            if (Message.Contains(TEXT("triangle"), ESearchCase::IgnoreCase)) return TEXT("VIS-009");
            return TEXT("VIS-008");
        }
        return TEXT("CMP-VALIDATION");
    }

    const TCHAR* InferThrusterCode(const FString& Message)
    {
        if (Message.Contains(TEXT("name"), ESearchCase::IgnoreCase)) return TEXT("THR-001");
        if (Message.Contains(TEXT("mount"), ESearchCase::IgnoreCase)) return TEXT("THR-004");
        if (Message.Contains(TEXT("propellant"), ESearchCase::IgnoreCase))
            return Message.Contains(TEXT("variable"), ESearchCase::IgnoreCase) ? TEXT("THR-006") : TEXT("THR-005");
        if (Message.Contains(TEXT("application point"), ESearchCase::IgnoreCase)) return TEXT("THR-007");
        if (Message.Contains(TEXT("direction"), ESearchCase::IgnoreCase)) return TEXT("THR-008");
        if (Message.Contains(TEXT("ignition"), ESearchCase::IgnoreCase)) return TEXT("THR-011");
        if (Message.Contains(TEXT("shutdown"), ESearchCase::IgnoreCase)) return TEXT("THR-014");
        if (Message.Contains(TEXT("maximum commanded thrust"), ESearchCase::IgnoreCase)) return TEXT("THR-019");
        if (Message.Contains(TEXT("specific impulse"), ESearchCase::IgnoreCase)) return TEXT("PRO-003");
        if (Message.Contains(TEXT("profile"), ESearchCase::IgnoreCase)
            || Message.Contains(TEXT("CSV"), ESearchCase::IgnoreCase)) return TEXT("CSV-PRO-VALIDATION");
        return TEXT("THR-VALIDATION");
    }

    const TCHAR* InferWheelCode(const FString& Message)
    {
        if (Message.Contains(TEXT("name"), ESearchCase::IgnoreCase)) return TEXT("WHL-001");
        if (Message.Contains(TEXT("mount"), ESearchCase::IgnoreCase)) return TEXT("WHL-003");
        if (Message.Contains(TEXT("axis"), ESearchCase::IgnoreCase)) return TEXT("WHL-004");
        if (Message.Contains(TEXT("initial"), ESearchCase::IgnoreCase)) return TEXT("WHL-006");
        if (Message.Contains(TEXT("maximum"), ESearchCase::IgnoreCase)) return TEXT("WHL-007");
        return TEXT("WHL-VALIDATION");
    }

    const TCHAR* InferControllerCode(const FString& Message)
    {
        if (Message.Contains(TEXT("unsupported"), ESearchCase::IgnoreCase)) return TEXT("CTL-001");
        if (Message.Contains(TEXT("Select"), ESearchCase::IgnoreCase)) return TEXT("CTL-002");
        if (Message.Contains(TEXT("unavailable"), ESearchCase::IgnoreCase)) return TEXT("CTL-003");
        if (Message.Contains(TEXT("registered"), ESearchCase::IgnoreCase)) return TEXT("CTL-004");
        if (Message.Contains(TEXT("trusted"), ESearchCase::IgnoreCase)) return TEXT("CTL-005");
        if (Message.Contains(TEXT("Ready"), ESearchCase::IgnoreCase)) return TEXT("CTL-006");
        if (Message.Contains(TEXT("missing"), ESearchCase::IgnoreCase)) return TEXT("CTL-007");
        return TEXT("CTL-008");
    }

    bool ResolveEffectiveFinalUtc(
        const FTGScenarioSolverConfig& Solver,
        FDateTime& OutFinalUtc)
    {
        if (Solver.EndMode == ETGSimulationEndMode::FinalUtc)
        {
            OutFinalUtc = Solver.FinalUtc;
            return OutFinalUtc != FDateTime::MinValue();
        }
        if (Solver.EndMode == ETGSimulationEndMode::Duration
            && IsFinite(Solver.DurationSeconds)
            && Solver.DurationSeconds >= 0.0)
        {
            OutFinalUtc = Solver.StartUtc
                + FTimespan::FromSeconds(Solver.DurationSeconds);
            return true;
        }
        return false;
    }

    double ResolveEffectiveDuration(const FTGScenarioSolverConfig& Solver)
    {
        if (Solver.EndMode == ETGSimulationEndMode::Duration)
        {
            return Solver.DurationSeconds;
        }
        if (Solver.EndMode == ETGSimulationEndMode::FinalUtc
            && Solver.StartUtc != FDateTime::MinValue()
            && Solver.FinalUtc != FDateTime::MinValue())
        {
            return (Solver.FinalUtc - Solver.StartUtc).GetTotalSeconds();
        }
        return -1.0;
    }

    bool FindCatalogByUserName(
        const FString& Name,
        FTGCelestialCatalogEntry& OutEntry)
    {
        for (const FTGCelestialCatalogEntry& Entry :
             UTGCelestialCatalogLibrary::GetCelestialCatalog())
        {
            if (Entry.SpiceTarget.Equals(Name, ESearchCase::IgnoreCase)
                || Entry.DisplayName.ToString().Equals(Name, ESearchCase::IgnoreCase)
                || Entry.CatalogKey.ToString().Equals(Name, ESearchCase::IgnoreCase))
            {
                OutEntry = Entry;
                return true;
            }
        }
        return false;
    }

    bool IsRegionSupported(
        const FTGComponentConfig& Component,
        ETGSrpLogicalRegion Region)
    {
        if (Component.Visual.GeometrySource != ETGComponentGeometrySource::Primitive)
        {
            return false;
        }
        switch (Component.Visual.PrimitiveType)
        {
        case ETGPrimitiveGeometryType::Box:
            return Region >= ETGSrpLogicalRegion::BoxPositiveX
                && Region <= ETGSrpLogicalRegion::BoxNegativeZ;
        case ETGPrimitiveGeometryType::Cylinder:
            return Region == ETGSrpLogicalRegion::CylinderSide
                || Region == ETGSrpLogicalRegion::CylinderPositiveCap
                || Region == ETGSrpLogicalRegion::CylinderNegativeCap;
        default:
            return false;
        }
    }

    void ValidateSolver(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        const FTGScenarioSolverConfig& S = Scenario.ScenarioAndSolver;
        const ETGScenarioReviewSection Section =
            ETGScenarioReviewSection::ScenarioAndSolver;

        if (S.ScenarioName.TrimStartAndEnd().IsEmpty())
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-001"), TEXT("ScenarioAndSolver.ScenarioName"), TEXT("Scenario name cannot be empty."), Section);
        else if (S.ScenarioName != S.ScenarioName.TrimStartAndEnd())
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("GEN-006"), TEXT("ScenarioAndSolver.ScenarioName"), TEXT("Leading or trailing whitespace will be removed."), Section);

        if (S.SimulationKind != ETGSimulationKind::Spacecraft6Dof)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-002"), TEXT("ScenarioAndSolver.SimulationKind"), TEXT("Only Spacecraft 6-DOF simulation is currently supported."), Section);
        if (S.StartUtc == FDateTime::MinValue())
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-003"), TEXT("ScenarioAndSolver.StartUtc"), TEXT("Enter a valid start UTC that SPICE can convert."), Section);
        if (S.EndMode != ETGSimulationEndMode::FinalUtc
            && S.EndMode != ETGSimulationEndMode::Duration)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-004"), TEXT("ScenarioAndSolver.EndMode"), TEXT("Select Final UTC or Duration as the simulation end mode."), Section);
        if (S.EndMode == ETGSimulationEndMode::FinalUtc)
        {
            if (S.FinalUtc == FDateTime::MinValue())
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-005"), TEXT("ScenarioAndSolver.FinalUtc"), TEXT("Enter a valid final UTC that SPICE can convert."), Section);
            else if (S.StartUtc != FDateTime::MinValue() && S.FinalUtc < S.StartUtc)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-006"), TEXT("ScenarioAndSolver.FinalUtc"), TEXT("Final UTC must not precede start UTC."), Section);
        }
        if (S.EndMode == ETGSimulationEndMode::Duration
            && (!IsFinite(S.DurationSeconds) || S.DurationSeconds < 0.0))
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-007"), TEXT("ScenarioAndSolver.DurationSeconds"), TEXT("Duration must be finite and nonnegative."), Section);

        const double Duration = ResolveEffectiveDuration(S);
        if (Duration == 0.0)
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SOL-008"), TEXT("ScenarioAndSolver.DurationSeconds"), TEXT("Simulation duration is zero; only the initial state will be recorded."), Section);

        if (S.IntegratorKind != ETGIntegratorKind::FixedStepRK4
            && S.IntegratorKind != ETGIntegratorKind::AdaptiveDormandPrince54)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-009"), TEXT("ScenarioAndSolver.IntegratorKind"), TEXT("Select Fixed-Step RK4 or Adaptive Dormand-Prince 5(4)."), Section);
        if (!IsFinite(S.MaximumIntegratorStepSeconds) || S.MaximumIntegratorStepSeconds <= 0.0)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-010"), TEXT("ScenarioAndSolver.MaximumIntegratorStepSeconds"), TEXT("Maximum integrator step must be a positive finite number of seconds."), Section);
        if (S.IntegratorKind == ETGIntegratorKind::AdaptiveDormandPrince54)
        {
            if (!IsFinite(S.InitialIntegratorStepSeconds)
                || S.InitialIntegratorStepSeconds <= 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-011"), TEXT("ScenarioAndSolver.InitialIntegratorStepSeconds"), TEXT("Initial adaptive step must be a positive finite number of seconds."), Section);
            else if (IsFinite(S.MaximumIntegratorStepSeconds)
                && S.InitialIntegratorStepSeconds
                    > S.MaximumIntegratorStepSeconds)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SOL-012"), TEXT("ScenarioAndSolver.InitialIntegratorStepSeconds"), TEXT("Initial adaptive step exceeds the maximum integrator step and will be capped to that maximum."), Section);
            if (!IsFinite(S.AbsoluteTolerance) || S.AbsoluteTolerance <= 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-013"), TEXT("ScenarioAndSolver.AbsoluteTolerance"), TEXT("Absolute tolerance must be positive and finite."), Section);
            if (!IsFinite(S.RelativeTolerance) || S.RelativeTolerance <= 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-014"), TEXT("ScenarioAndSolver.RelativeTolerance"), TEXT("Relative tolerance must be positive and finite."), Section);
        }
        if (S.OutputMode != ETGOutputMode::EveryIntegratorStep
            && S.OutputMode != ETGOutputMode::FixedInterval)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-015"), TEXT("ScenarioAndSolver.OutputMode"), TEXT("Select Every Integrator Step or Fixed Interval output."), Section);
        if (S.OutputMode == ETGOutputMode::FixedInterval
            && (!IsFinite(S.OutputStepSeconds) || S.OutputStepSeconds <= 0.0))
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-016"), TEXT("ScenarioAndSolver.OutputStepSeconds"), TEXT("Output interval must be a positive finite number of seconds."), Section);
        if (S.MaximumIntegrationSteps <= 0)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-017"), TEXT("ScenarioAndSolver.MaximumIntegrationSteps"), TEXT("Maximum integration attempts must be greater than zero."), Section);
        if (S.MaximumOutputSamples <= 0)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-018"), TEXT("ScenarioAndSolver.MaximumOutputSamples"), TEXT("Maximum stored samples must be greater than zero."), Section);
        if (!IsFinite(S.MaximumWallClockRuntimeSeconds) ||
            S.MaximumWallClockRuntimeSeconds <= 0.0)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-023"), TEXT("ScenarioAndSolver.MaximumWallClockRuntimeSeconds"), TEXT("Maximum backend runtime must be a positive finite number of real-time seconds."), Section);
        if (S.MassFlowConvention != ETGMassFlowConvention::ThrustIncludesExhaustMomentum)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-019"), TEXT("ScenarioAndSolver.MassFlowConvention"), TEXT("Only Thrust Includes Exhaust Momentum is currently supported."), Section);

        if (Duration > 0.0 && IsFinite(S.MaximumIntegratorStepSeconds)
            && S.MaximumIntegratorStepSeconds > 0.0 && S.MaximumIntegrationSteps > 0)
        {
            const int64 MinimumSteps = FMath::CeilToInt64(Duration / S.MaximumIntegratorStepSeconds);
            if (MinimumSteps > S.MaximumIntegrationSteps)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-020"), TEXT("ScenarioAndSolver.MaximumIntegrationSteps"), TEXT("Maximum integration attempts is below the theoretical minimum required to reach the final time."), Section);
        }
        if (Duration > 0.0 && S.OutputMode == ETGOutputMode::FixedInterval
            && IsFinite(S.OutputStepSeconds) && S.OutputStepSeconds > 0.0)
        {
            const int64 RequiredSamples = 1 + FMath::CeilToInt64(Duration / S.OutputStepSeconds);
            if (S.MaximumOutputSamples > 0 && RequiredSamples > S.MaximumOutputSamples)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SOL-021"), TEXT("ScenarioAndSolver.MaximumOutputSamples"), TEXT("Maximum stored samples is smaller than the number required by the selected duration and output interval."), Section);
            if (S.OutputStepSeconds > Duration)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SOL-022"), TEXT("ScenarioAndSolver.OutputStepSeconds"), TEXT("Output interval exceeds the simulation duration; only boundary samples may be stored."), Section);
        }
    }

    void ValidateInitialState(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        const FTGInitialSpacecraftState& S = Scenario.InitialState;
        const ETGScenarioReviewSection Section = ETGScenarioReviewSection::InitialState;
        if (!IsFinite(S.PositionMeters))
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("STA-001"), TEXT("InitialState.PositionMeters"), TEXT("Initial ICRF position must contain three finite values in meters."), Section);
        if (!IsFinite(S.VelocityMetersPerSecond))
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("STA-002"), TEXT("InitialState.VelocityMetersPerSecond"), TEXT("Initial ICRF velocity must contain three finite values in meters per second."), Section);
        const double QuaternionSquaredNorm = S.AttitudeBodyToIcrf.SizeSquared();
        if (!IsFinite(S.AttitudeBodyToIcrf) || !IsFinite(QuaternionSquaredNorm)
            || QuaternionSquaredNorm <= ZeroTolerance)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("STA-003"), TEXT("InitialState.AttitudeBodyToIcrf"), TEXT("Initial B-to-ICRF attitude quaternion must be finite and nonzero."), Section);
        else
        {
            const double Norm = FMath::Sqrt(QuaternionSquaredNorm);
            if (FMath::Abs(Norm - 1.0) > UnitTolerance)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("STA-004"), TEXT("InitialState.AttitudeBodyToIcrf"), FString::Printf(TEXT("Initial attitude quaternion norm is %.17g; it will be normalized before simulation."), Norm), Section);
        }
        if (!IsFinite(S.AngularVelocityBodyRadiansPerSecond))
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("STA-005"), TEXT("InitialState.AngularVelocityBodyRadiansPerSecond"), TEXT("Initial body angular velocity must contain three finite values in radians per second."), Section);
    }

    void ValidateComponents(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        const FTGComponentTreeValidationReport ComponentReport =
            UTGComponentTreeValidationLibrary::ValidateScenarioComponentTree(Scenario);
        for (const FTGComponentTreeValidationIssue& Source : ComponentReport.Issues)
        {
            int32 ComponentIndex = ExtractLeadingArrayIndex(
                Source.Path,
                TEXT("Components["));
            const FTGComponentConfig* IssueComponent =
                Scenario.Components.IsValidIndex(ComponentIndex)
                    ? &Scenario.Components[ComponentIndex]
                    : nullptr;

            if (IssueComponent == nullptr && Source.ComponentId.IsValid())
            {
                ComponentIndex = Scenario.Components.IndexOfByPredicate(
                    [&Source](const FTGComponentConfig& Component)
                    {
                        return Component.ComponentId == Source.ComponentId;
                    });
                IssueComponent = Scenario.Components.IsValidIndex(ComponentIndex)
                    ? &Scenario.Components[ComponentIndex]
                    : nullptr;
            }

            FString Message = Source.Message.ToString();
            if (IssueComponent != nullptr)
            {
                FString ComponentLabel =
                    IssueComponent->Name.TrimStartAndEnd();
                if (ComponentLabel.IsEmpty())
                {
                    ComponentLabel = FString::Printf(
                        TEXT("Component %d"),
                        ComponentIndex + 1);
                }
                Message += FString::Printf(TEXT(" [%s]"), *ComponentLabel);
            }

            Add(
                Report,
                Source.Severity == ETGComponentTreeIssueSeverity::Error
                    ? ETGScenarioReviewSeverity::Error
                    : ETGScenarioReviewSeverity::Warning,
                InferComponentCode(Source),
                Source.Path,
                Message,
                ETGScenarioReviewSection::ComponentsAndJoints,
                Source.ComponentId,
                Source.DofId,
                ComponentIndex);
        }

        bool bAnyVisible = false;
        for (int32 Index = 0; Index < Scenario.Components.Num(); ++Index)
        {
            const FTGComponentConfig& Component = Scenario.Components[Index];
            const FString Base = FString::Printf(TEXT("Components[%d]"), Index);
            bAnyVisible |= Component.Visual.bVisible;

            if (Component.Name != Component.Name.TrimStartAndEnd())
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("GEN-006"), Base + TEXT(".Name"), TEXT("Leading or trailing whitespace will be removed."), ETGScenarioReviewSection::ComponentsAndJoints, Component.ComponentId, FGuid{}, Index);
            if (Component.bVariableMass
                && FMath::IsNearlyEqual(Component.InitialMassKilograms, Component.MinimumMassKilograms))
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("CMP-011"), Base + TEXT(".MinimumMassKilograms"), TEXT("This variable-mass component has no consumable mass above its minimum."), ETGScenarioReviewSection::ComponentsAndJoints, Component.ComponentId, FGuid{}, Index);
        }
        if (!Scenario.Components.IsEmpty() && !bAnyVisible)
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("VIS-022"), TEXT("Components"), TEXT("All spacecraft components are hidden; result playback will show no spacecraft geometry."), ETGScenarioReviewSection::ComponentsAndJoints);
    }

    void ValidateActuators(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        const auto FindComponent = [&Scenario](const FString& Name)
            -> const FTGComponentConfig*
        {
            return Scenario.Components.FindByPredicate(
                [&Name](const FTGComponentConfig& Component)
                {
                    return Component.Name.Equals(
                        Name.TrimStartAndEnd(), ESearchCase::IgnoreCase);
                });
        };
        const auto ValidateProfile = [&Report](
            const FTGScalarProfileConfig& Profile,
            ETGScalarProfileQuantity Quantity,
            const FString& Path,
            int32 Index)
        {
            if (Profile.Source != ETGScalarProfileSource::Constant
                && Profile.Source != ETGScalarProfileSource::CsvProfile)
            {
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("PRO-001"), Path + TEXT(".Source"), TEXT("Select Constant or CSV Profile."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
                return;
            }
            if (Profile.Source == ETGScalarProfileSource::Constant)
            {
                const bool bValid = IsFinite(Profile.ConstantValue)
                    && (Quantity == ETGScalarProfileQuantity::Thrust
                        ? Profile.ConstantValue >= 0.0
                        : Profile.ConstantValue > 0.0);
                if (!bValid)
                    Add(Report, ETGScenarioReviewSeverity::Error,
                        Quantity == ETGScalarProfileQuantity::Thrust ? TEXT("PRO-002") : TEXT("PRO-003"),
                        Path + TEXT(".ConstantValue"),
                        Quantity == ETGScalarProfileQuantity::Thrust
                            ? TEXT("Constant thrust must be finite and nonnegative.")
                            : TEXT("Constant specific impulse must be finite and greater than zero."),
                        ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
                return;
            }

            const FTGScalarProfileCsvInspection Inspection =
                UTGScalarProfileCsvLibrary::InspectScalarProfileCsv(
                    Profile.CsvFilePath, Quantity);
            for (const FString& Message : Inspection.ErrorMessages)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("CSV-PRO-VALIDATION"), Path + TEXT(".CsvFilePath"), Message, ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
        };

        TSet<FString> ThrusterNames;
        for (int32 Index = 0; Index < Scenario.Thrusters.Num(); ++Index)
        {
            const FTGThrusterConfig& Thruster = Scenario.Thrusters[Index];
            const FString Base = FString::Printf(TEXT("Thrusters[%d] ('%s')"), Index, *Thruster.Name);
            const int32 ErrorCountBefore = Report.ErrorCount;
            const int32 WarningCountBefore = Report.WarningCount;
            const FString CanonicalName = Thruster.Name.TrimStartAndEnd().ToLower();
            if (CanonicalName.IsEmpty())
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-001"), Base + TEXT(".Name"), TEXT("Thruster name cannot be empty."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (Thruster.Name != Thruster.Name.TrimStartAndEnd())
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("GEN-006"), Base + TEXT(".Name"), TEXT("Leading or trailing whitespace will be removed."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!CanonicalName.IsEmpty() && ThrusterNames.Contains(CanonicalName))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-002"), Base + TEXT(".Name"), FString::Printf(TEXT("A thruster named '%s' already exists."), *Thruster.Name), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            ThrusterNames.Add(CanonicalName);

            if (Thruster.Mode != ETGThrusterMode::PrescribedProfile
                && Thruster.Mode != ETGThrusterMode::Commanded)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-003"), Base + TEXT(".Mode"), TEXT("Select Prescribed Profile or Commanded mode."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (FindComponent(Thruster.MountComponentName) == nullptr)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-004"), Base + TEXT(".MountComponentName"), TEXT("Select an existing mount component."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            const FTGComponentConfig* Propellant = FindComponent(
                Thruster.PropellantComponentName);
            if (Propellant == nullptr)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-005"), Base + TEXT(".PropellantComponentName"), TEXT("Select an existing propellant component."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (!Propellant->bVariableMass)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-006"), Base + TEXT(".PropellantComponentName"), FString::Printf(TEXT("Propellant component '%s' must be marked Variable Mass in the Component Tree."), *Propellant->Name), ETGScenarioReviewSection::Actuators, Propellant->ComponentId, FGuid{}, Index);
            if (!IsFinite(Thruster.ApplicationPointMeters))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-007"), Base + TEXT(".ApplicationPointMeters"), TEXT("Thruster application point must contain three finite mount-component values."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!IsFinite(Thruster.Direction)
                || !IsFinite(Thruster.Direction.Size())
                || Thruster.Direction.Size() <= ZeroTolerance)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-008"), Base + TEXT(".Direction"), TEXT("Thruster direction must be a finite nonzero vector."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (FMath::Abs(Thruster.Direction.Size() - 1.0) > UnitTolerance)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("THR-009"), Base + TEXT(".Direction"), FString::Printf(TEXT("Thruster direction norm is %.17g; it will be normalized before storage."), Thruster.Direction.Size()), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);

            if (Thruster.IgnitionTimeMode != ETGThrusterTimeMode::AbsoluteUtc
                && Thruster.IgnitionTimeMode != ETGThrusterTimeMode::ElapsedSimulationTime)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-010"), Base + TEXT(".IgnitionTimeMode"), TEXT("Select Absolute UTC or Elapsed Simulation Time for ignition."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (Thruster.IgnitionTimeMode == ETGThrusterTimeMode::ElapsedSimulationTime
                && (!IsFinite(Thruster.IgnitionElapsedSeconds)
                    || Thruster.IgnitionElapsedSeconds < 0.0))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-011"), Base + TEXT(".IgnitionElapsedSeconds"), TEXT("Ignition elapsed time must be finite and nonnegative."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (Thruster.IgnitionTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                && Thruster.IgnitionUtc == FDateTime::MinValue())
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-012"), Base + TEXT(".IgnitionUtc"), TEXT("Enter a valid ignition UTC time."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);

            if (!Thruster.bNeverShutsDown)
            {
                if (Thruster.ShutdownTimeMode != ETGThrusterTimeMode::AbsoluteUtc
                    && Thruster.ShutdownTimeMode != ETGThrusterTimeMode::ElapsedSimulationTime)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-013"), Base + TEXT(".ShutdownTimeMode"), TEXT("Select Absolute UTC or Elapsed Simulation Time for shutdown."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
                else if (Thruster.ShutdownTimeMode == ETGThrusterTimeMode::ElapsedSimulationTime
                    && (!IsFinite(Thruster.ShutdownElapsedSeconds)
                        || Thruster.ShutdownElapsedSeconds < 0.0))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-014"), Base + TEXT(".ShutdownElapsedSeconds"), TEXT("Shutdown elapsed time must be finite and nonnegative."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
                else if (Thruster.ShutdownTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                    && Thruster.ShutdownUtc == FDateTime::MinValue())
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-015"), Base + TEXT(".ShutdownUtc"), TEXT("Enter a valid shutdown UTC time."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);

                const bool bIgnitionResolvable =
                    (Thruster.IgnitionTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                        && Thruster.IgnitionUtc != FDateTime::MinValue())
                    || (Thruster.IgnitionTimeMode == ETGThrusterTimeMode::ElapsedSimulationTime
                        && IsFinite(Thruster.IgnitionElapsedSeconds)
                        && Thruster.IgnitionElapsedSeconds >= 0.0);
                const bool bShutdownResolvable =
                    (Thruster.ShutdownTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                        && Thruster.ShutdownUtc != FDateTime::MinValue())
                    || (Thruster.ShutdownTimeMode == ETGThrusterTimeMode::ElapsedSimulationTime
                        && IsFinite(Thruster.ShutdownElapsedSeconds)
                        && Thruster.ShutdownElapsedSeconds >= 0.0);
                if (bIgnitionResolvable && bShutdownResolvable)
                {
                    const FDateTime Ignition =
                        Thruster.IgnitionTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                            ? Thruster.IgnitionUtc
                            : Scenario.ScenarioAndSolver.StartUtc
                                + FTimespan::FromSeconds(Thruster.IgnitionElapsedSeconds);
                    const FDateTime Shutdown =
                        Thruster.ShutdownTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                            ? Thruster.ShutdownUtc
                            : Scenario.ScenarioAndSolver.StartUtc
                                + FTimespan::FromSeconds(Thruster.ShutdownElapsedSeconds);
                    if (Shutdown <= Ignition)
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-016"), Base, TEXT("Shutdown time must be after ignition time."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
                    FDateTime FinalUtc;
                    if (ResolveEffectiveFinalUtc(Scenario.ScenarioAndSolver, FinalUtc)
                        && (Shutdown <= Scenario.ScenarioAndSolver.StartUtc
                            || Ignition >= FinalUtc))
                        Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("THR-017"), Base, TEXT("Thruster firing window does not overlap the simulation interval; this thruster will not fire."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
                }
            }
            else
            {
                const bool bIgnitionResolvable =
                    (Thruster.IgnitionTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                        && Thruster.IgnitionUtc != FDateTime::MinValue())
                    || (Thruster.IgnitionTimeMode == ETGThrusterTimeMode::ElapsedSimulationTime
                        && IsFinite(Thruster.IgnitionElapsedSeconds)
                        && Thruster.IgnitionElapsedSeconds >= 0.0);
                if (bIgnitionResolvable)
                {
                    const FDateTime Ignition =
                        Thruster.IgnitionTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                            ? Thruster.IgnitionUtc
                            : Scenario.ScenarioAndSolver.StartUtc
                                + FTimespan::FromSeconds(Thruster.IgnitionElapsedSeconds);
                    FDateTime FinalUtc;
                    if (ResolveEffectiveFinalUtc(Scenario.ScenarioAndSolver, FinalUtc)
                        && Ignition >= FinalUtc)
                        Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("THR-017"), Base, TEXT("Thruster firing window does not overlap the simulation interval; this thruster will not fire."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
                }
            }

            if (Thruster.Mode == ETGThrusterMode::PrescribedProfile)
            {
                ValidateProfile(Thruster.PrescribedThrust, ETGScalarProfileQuantity::Thrust, Base + TEXT(".PrescribedThrust"), Index);
                ValidateProfile(Thruster.PrescribedSpecificImpulse, ETGScalarProfileQuantity::SpecificImpulse, Base + TEXT(".PrescribedSpecificImpulse"), Index);
            }
            else if (Thruster.Mode == ETGThrusterMode::Commanded
                && (!IsFinite(Thruster.MaximumThrustNewtons)
                    || Thruster.MaximumThrustNewtons <= 0.0))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-019"), Base + TEXT(".MaximumThrustNewtons"), TEXT("Maximum commanded thrust must be finite and greater than zero."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);

            FTGThrusterConfig Accepted;
            FText Warning;
            FText Error;
            const bool bValid = UTGActuatorEditingLibrary::ValidateThruster(
                Scenario, Thruster, Accepted, Warning, Error);
            if (!bValid && !Error.IsEmpty()
                && Report.ErrorCount == ErrorCountBefore)
                Add(Report, ETGScenarioReviewSeverity::Error, InferThrusterCode(Error.ToString()), Base, Error.ToString(), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!Warning.IsEmpty()
                && Report.WarningCount == WarningCountBefore)
                Add(Report, ETGScenarioReviewSeverity::Warning, InferThrusterCode(Warning.ToString()), Base, Warning.ToString(), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);

            if (Thruster.Mode == ETGThrusterMode::Commanded
                && Scenario.Control.Mode == ETGControlMode::None)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("THR-020"), Base + TEXT(".Mode"), TEXT("Commanded thruster receives zero throttle because no user controller is selected."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);

            if (Propellant != nullptr && Propellant->bVariableMass
                && FMath::IsNearlyEqual(Propellant->InitialMassKilograms, Propellant->MinimumMassKilograms))
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("THR-018"), Base + TEXT(".PropellantComponentName"), FString::Printf(TEXT("Propellant component '%s' starts at its depletion floor; this thruster cannot fire."), *Propellant->Name), ETGScenarioReviewSection::Actuators, Propellant->ComponentId, FGuid{}, Index);
        }

        TSet<FString> WheelNames;
        for (int32 Index = 0; Index < Scenario.ReactionWheels.Num(); ++Index)
        {
            const FTGReactionWheelConfig& Wheel = Scenario.ReactionWheels[Index];
            const FString Base = FString::Printf(TEXT("ReactionWheels[%d] ('%s')"), Index, *Wheel.Name);
            const int32 ErrorCountBefore = Report.ErrorCount;
            const int32 WarningCountBefore = Report.WarningCount;
            const FString CanonicalName = Wheel.Name.TrimStartAndEnd().ToLower();
            if (CanonicalName.IsEmpty())
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("WHL-001"), Base + TEXT(".Name"), TEXT("Reaction-wheel name cannot be empty."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (Wheel.Name != Wheel.Name.TrimStartAndEnd())
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("GEN-006"), Base + TEXT(".Name"), TEXT("Leading or trailing whitespace will be removed."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!CanonicalName.IsEmpty() && WheelNames.Contains(CanonicalName))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("WHL-002"), Base + TEXT(".Name"), FString::Printf(TEXT("A reaction wheel named '%s' already exists."), *Wheel.Name), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            WheelNames.Add(CanonicalName);

            if (FindComponent(Wheel.MountComponentName) == nullptr)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("WHL-003"), Base + TEXT(".MountComponentName"), TEXT("Select an existing mount component."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!IsFinite(Wheel.Axis) || !IsFinite(Wheel.Axis.Size())
                || Wheel.Axis.Size() <= ZeroTolerance)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("WHL-004"), Base + TEXT(".Axis"), TEXT("Reaction-wheel axis must be a finite nonzero vector."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (FMath::Abs(Wheel.Axis.Size() - 1.0) > UnitTolerance)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("WHL-005"), Base + TEXT(".Axis"), FString::Printf(TEXT("Reaction-wheel axis norm is %.17g; it will be normalized before storage."), Wheel.Axis.Size()), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!IsFinite(Wheel.InitialMomentumNewtonMeterSeconds))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("WHL-006"), Base + TEXT(".InitialMomentumNewtonMeterSeconds"), TEXT("Initial wheel momentum must be finite."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!IsFinite(Wheel.MaximumAbsoluteMomentumNewtonMeterSeconds)
                || Wheel.MaximumAbsoluteMomentumNewtonMeterSeconds < 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("WHL-007"), Base + TEXT(".MaximumAbsoluteMomentumNewtonMeterSeconds"), TEXT("Maximum absolute wheel momentum must be finite and nonnegative."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            else if (IsFinite(Wheel.InitialMomentumNewtonMeterSeconds)
                && FMath::Abs(Wheel.InitialMomentumNewtonMeterSeconds)
                    > Wheel.MaximumAbsoluteMomentumNewtonMeterSeconds + 1.0e-12)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("WHL-008"), Base + TEXT(".InitialMomentumNewtonMeterSeconds"), TEXT("The absolute initial wheel momentum cannot exceed the maximum absolute wheel momentum."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);

            FTGReactionWheelConfig Accepted;
            FText Warning;
            FText Error;
            const bool bValid = UTGActuatorEditingLibrary::ValidateReactionWheel(
                Scenario, Wheel, Accepted, Warning, Error);
            if (!bValid && !Error.IsEmpty()
                && Report.ErrorCount == ErrorCountBefore)
                Add(Report, ETGScenarioReviewSeverity::Error, InferWheelCode(Error.ToString()), Base, Error.ToString(), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (!Warning.IsEmpty()
                && Report.WarningCount == WarningCountBefore)
                Add(Report, ETGScenarioReviewSeverity::Warning, InferWheelCode(Warning.ToString()), Base, Warning.ToString(), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            if (Scenario.Control.Mode == ETGControlMode::None
                && Wheel.MaximumAbsoluteMomentumNewtonMeterSeconds > 0.0)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("WHL-009"), Base, TEXT("Reaction-wheel momentum will remain uncommanded because no user controller is selected."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
        }
    }

    void ValidateController(
        const UObject* WorldContextObject,
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        if (Scenario.Control.Mode == ETGControlMode::None
            && !Scenario.Control.ControllerId.IsNone())
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("CTL-009"), TEXT("Control.ControllerId"), TEXT("Stored controller selection is ignored because control mode is None."), ETGScenarioReviewSection::Controller, FGuid{}, FGuid{}, INDEX_NONE, Scenario.Control.ControllerId);

        FText Error;
        if (!UTGControlEditingLibrary::ValidateControlSelection(
                WorldContextObject, Scenario.Control, Error))
            Add(Report, ETGScenarioReviewSeverity::Error, InferControllerCode(Error.ToString()), TEXT("Control"), Error.ToString(), ETGScenarioReviewSection::Controller, FGuid{}, FGuid{}, INDEX_NONE, Scenario.Control.ControllerId);
    }

    void ValidateGravity(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        const TArray<FTGCelestialCatalogEntry> Catalog =
            UTGCelestialCatalogLibrary::GetCelestialCatalog();
        TMap<FName, int32> CatalogIndices;
        for (int32 Index = 0; Index < Catalog.Num(); ++Index)
            CatalogIndices.Add(Catalog[Index].CatalogKey, Index);

        TSet<FName> Seen;
        bool bOrderValid = Scenario.CelestialBodies.Num() == Catalog.Num();
        bool bAnyCanActivate = false;
        TMap<FName, bool> ExplicitSystemBarycenter;
        TMap<FName, bool> ExplicitSystemMember;

        for (int32 Index = 0; Index < Scenario.CelestialBodies.Num(); ++Index)
        {
            const FTGCelestialBodyConfig& Body = Scenario.CelestialBodies[Index];
            const FString Base = FString::Printf(TEXT("CelestialBodies[%d]"), Index);
            const int32* CatalogIndex = CatalogIndices.Find(Body.CatalogKey);
            if (CatalogIndex == nullptr)
            {
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-001"), TEXT("CelestialBodies"), FString::Printf(TEXT("Celestial-body row %d has an unknown catalog key '%s'."), Index, *Body.CatalogKey.ToString()), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
                bOrderValid = false;
                continue;
            }
            if (Seen.Contains(Body.CatalogKey))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-002"), TEXT("CelestialBodies"), FString::Printf(TEXT("Celestial-body catalog key '%s' appears more than once."), *Body.CatalogKey.ToString()), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
            Seen.Add(Body.CatalogKey);
            bOrderValid &= *CatalogIndex == Index;

            const FTGCelestialCatalogEntry& Entry = Catalog[*CatalogIndex];
            bAnyCanActivate |= Body.bGravityEnabled || Body.AutomaticActivationRadiusMeters > 0.0;
            if (Entry.SourceRole == ETGCelestialSourceRole::SystemBarycenter)
                ExplicitSystemBarycenter.FindOrAdd(Entry.SystemKey) |= Body.bGravityEnabled;
            if (Entry.SourceRole == ETGCelestialSourceRole::PhysicalSystemMember)
                ExplicitSystemMember.FindOrAdd(Entry.SystemKey) |= Body.bGravityEnabled;

            if (!IsFinite(Body.AutomaticActivationRadiusMeters)
                || Body.AutomaticActivationRadiusMeters < 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-005"), Base + TEXT(".AutomaticActivationRadiusMeters"), TEXT("Automatic activation radius must be finite and nonnegative."), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
            if (Entry.SourceRole == ETGCelestialSourceRole::SystemBarycenter)
            {
                if (!IsFinite(Body.BarycenterResolutionRadiusMeters)
                    || Body.BarycenterResolutionRadiusMeters < 0.0)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-006"), Base + TEXT(".BarycenterResolutionRadiusMeters"), TEXT("Barycenter resolution radius must be finite and nonnegative."), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
            }
            else if (Body.BarycenterResolutionRadiusMeters != 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-007"), Base + TEXT(".BarycenterResolutionRadiusMeters"), TEXT("Only a system barycenter may have a resolution radius."), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);

            if (!Entry.bSupportsHarmonicGravity
                && (!Body.HarmonicModelCsvFilePath.IsEmpty()
                    || Body.MaximumHarmonicDegreeUsed != 0))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-008"), Base + TEXT(".HarmonicModelCsvFilePath"), FString::Printf(TEXT("Catalog source '%s' supports point-mass gravity only; remove its harmonic configuration."), *Entry.DisplayName.ToString()), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
            if (Body.MaximumHarmonicDegreeUsed < 0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-009"), Base + TEXT(".MaximumHarmonicDegreeUsed"), TEXT("Maximum harmonic degree must be nonnegative."), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);

            if (Body.MaximumHarmonicDegreeUsed > 0 && Entry.bSupportsHarmonicGravity)
            {
                if (Body.HarmonicModelCsvFilePath.TrimStartAndEnd().IsEmpty())
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("HGM-001"), Base + TEXT(".HarmonicModelCsvFilePath"), TEXT("Select a harmonic-model CSV when maximum harmonic degree is positive."), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
                else
                {
                    FTGHarmonicCsvInspection Inspection;
                    if (!UTGHarmonicCsvLibrary::InspectHarmonicModelCsv(
                            Body.HarmonicModelCsvFilePath, Inspection))
                    {
                        for (const FString& Message : Inspection.ErrorMessages)
                            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("HGM-VALIDATION"), Base + TEXT(".HarmonicModelCsvFilePath"), Message, ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
                    }
                    else if (!UTGHarmonicCsvLibrary::SupportsMaximumDegree(
                                 Inspection, Body.MaximumHarmonicDegreeUsed))
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("HGM-017"), Base + TEXT(".MaximumHarmonicDegreeUsed"), FString::Printf(TEXT("Requested maximum degree %d exceeds file maximum degree %d."), Body.MaximumHarmonicDegreeUsed, Inspection.MaximumAvailableDegree), ETGScenarioReviewSection::Gravity, FGuid{}, FGuid{}, Index);
                }
            }
        }

        for (const FTGCelestialCatalogEntry& Entry : Catalog)
        {
            if (!Seen.Contains(Entry.CatalogKey))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-003"), TEXT("CelestialBodies"), FString::Printf(TEXT("Required celestial-body catalog row '%s' is missing."), *Entry.CatalogKey.ToString()), ETGScenarioReviewSection::Gravity);
        }
        if (!bOrderValid)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-004"), TEXT("CelestialBodies"), TEXT("Celestial-body rows are not in the fixed catalog order; use the Gravity panel's explicit catalog repair action."), ETGScenarioReviewSection::Gravity);

        for (const TPair<FName, bool>& Pair : ExplicitSystemBarycenter)
        {
            if (Pair.Value && ExplicitSystemMember.FindRef(Pair.Key))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("GRV-010"), TEXT("CelestialBodies"), FString::Printf(TEXT("Gravity system '%s' selects both its barycenter and physical members; select only one representation."), *Pair.Key.ToString()), ETGScenarioReviewSection::Gravity);
        }
        if (!bAnyCanActivate)
        {
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("GRV-011"), TEXT("CelestialBodies"), TEXT("No gravitational source is enabled and every automatic activation radius is zero; gravity will be absent."), ETGScenarioReviewSection::Gravity);
            if (Scenario.GravitySettings.bIncludeFirstPostNewtonianCorrection)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("GRV-012"), TEXT("GravitySettings.bIncludeFirstPostNewtonianCorrection"), TEXT("The 1PN option has no effect because no gravitational source can become active."), ETGScenarioReviewSection::Gravity);
        }
    }

    void ValidateSrp(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        const FTGSolarRadiationPressureConfig& Srp = Scenario.SolarRadiationPressure;
        const ETGScenarioReviewSection Section = ETGScenarioReviewSection::SolarRadiationPressure;
        if (!Srp.bEnabled)
            return;

        if (Srp.SunBodyName != TEXT("Sun"))
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SRP-001"), TEXT("SolarRadiationPressure.SunBodyName"), TEXT("Stored Sun source is ignored; the converter always uses Sun."), Section);
        if (!IsFinite(Srp.PressureAtOneAstronomicalUnitPascals)
            || !FMath::IsNearlyEqual(Srp.PressureAtOneAstronomicalUnitPascals, 4.5391e-6, 1.0e-12))
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SRP-002"), TEXT("SolarRadiationPressure.PressureAtOneAstronomicalUnitPascals"), TEXT("Stored solar pressure is ignored; the converter uses 4.5391e-6 Pa at one AU."), Section);
        const bool bUsesGlobalFallback =
            Scenario.Components.ContainsByPredicate(
                [](const FTGComponentConfig& Component)
                {
                    const FTGComponentSrpConfig& Config =
                        Component.SolarRadiationPressure;
                    return Config.bIncludedInProxy
                        && Config.bUseGlobalFallbackOpticalProperties;
                });
        FString Reason;
        if (bUsesGlobalFallback
            && !UTGSolarRadiationPressureEditingLibrary::IsValidOpticalProperties(
                Srp.GlobalFallbackOpticalProperties, Reason))
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-004"), TEXT("SolarRadiationPressure.GlobalFallbackOpticalProperties"), TEXT("Global fallback optical properties: ") + Reason, Section);

        int32 IncludedCount = 0;
        for (int32 Index = 0; Index < Scenario.Components.Num(); ++Index)
        {
            const FTGComponentConfig& Component = Scenario.Components[Index];
            const FTGComponentSrpConfig& Config = Component.SolarRadiationPressure;
            if (!Config.bIncludedInProxy)
                continue;
            ++IncludedCount;
            const FString Base = FString::Printf(TEXT("Components[%d].SolarRadiationPressure"), Index);
            if (Component.Visual.GeometrySource != ETGComponentGeometrySource::Primitive
                && Component.Visual.GeometrySource != ETGComponentGeometrySource::CustomStl)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C01"), Base, FString::Printf(TEXT("Component '%s' has no source geometry for its SRP proxy."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index);
            if (Config.ProxyResolutionMode != ETGSrpProxyResolutionMode::Automatic
                && Config.ProxyResolutionMode != ETGSrpProxyResolutionMode::CustomTargetTriangleCount)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C02"), Base + TEXT(".ProxyResolutionMode"), FString::Printf(TEXT("Component '%s' has an unsupported SRP proxy-resolution mode."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index);
            if (Config.ProxyResolutionMode == ETGSrpProxyResolutionMode::CustomTargetTriangleCount
                && Config.CustomTargetTriangleCount <= 0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C03"), Base + TEXT(".CustomTargetTriangleCount"), FString::Printf(TEXT("Component '%s' custom target triangle count must be positive."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index);
            if (!Config.bUseGlobalFallbackOpticalProperties
                && !UTGSolarRadiationPressureEditingLibrary::IsValidOpticalProperties(
                    Config.ComponentOpticalProperties, Reason))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C04"), Base + TEXT(".ComponentOpticalProperties"), FString::Printf(TEXT("Component '%s' optical properties: %s"), *Component.Name, *Reason), Section, Component.ComponentId, FGuid{}, Index);

            if (!Config.bApplyOneOpticalConfigurationToEntireComponent)
            {
                TSet<ETGSrpLogicalRegion> Regions;
                for (const FTGSrpLogicalRegionOverride& Override : Config.LogicalRegionOverrides)
                {
                    if (!IsRegionSupported(Component, Override.Region))
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C05"), Base + TEXT(".LogicalRegionOverrides"), FString::Printf(TEXT("Component '%s' has an override for a logical region its geometry does not support."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index, NAME_None, static_cast<uint8>(Override.Region));
                    if (Regions.Contains(Override.Region))
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C06"), Base + TEXT(".LogicalRegionOverrides"), FString::Printf(TEXT("Component '%s' has duplicate logical-region overrides."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index, NAME_None, static_cast<uint8>(Override.Region));
                    Regions.Add(Override.Region);
                    if (!UTGSolarRadiationPressureEditingLibrary::IsValidOpticalProperties(
                            Override.OpticalProperties, Reason))
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C07"), Base + TEXT(".LogicalRegionOverrides"), FString::Printf(TEXT("Component '%s' logical-region override: %s"), *Component.Name, *Reason), Section, Component.ComponentId, FGuid{}, Index, NAME_None, static_cast<uint8>(Override.Region));
                }

                if (!Config.TriangleOverrides.IsEmpty()
                    && Component.Visual.GeometrySource == ETGComponentGeometrySource::Primitive)
                {
                    const bool bSphere = Component.Visual.PrimitiveType == ETGPrimitiveGeometryType::Sphere;
                    Add(Report, ETGScenarioReviewSeverity::Error, bSphere ? TEXT("SRP-C09") : TEXT("SRP-C08"), Base + TEXT(".TriangleOverrides"), bSphere
                        ? FString::Printf(TEXT("Component '%s' is a sphere and cannot use triangle overrides."), *Component.Name)
                        : FString::Printf(TEXT("Component '%s' uses primitive geometry; use logical-region overrides instead of triangle overrides."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index);
                }
                TSet<int32> TriangleIndices;
                for (const FTGSrpTriangleOverride& Override : Config.TriangleOverrides)
                {
                    if (Override.ProxyTriangleIndex < 0)
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C11"), Base + TEXT(".TriangleOverrides"), FString::Printf(TEXT("Component '%s' triangle override index %d cannot be negative."), *Component.Name, Override.ProxyTriangleIndex), Section, Component.ComponentId, FGuid{}, Index, NAME_None, 0, Override.ProxyTriangleIndex);
                    if (TriangleIndices.Contains(Override.ProxyTriangleIndex))
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C10"), Base + TEXT(".TriangleOverrides"), FString::Printf(TEXT("Component '%s' has duplicate triangle override index %d."), *Component.Name, Override.ProxyTriangleIndex), Section, Component.ComponentId, FGuid{}, Index, NAME_None, 0, Override.ProxyTriangleIndex);
                    TriangleIndices.Add(Override.ProxyTriangleIndex);
                    if (!UTGSolarRadiationPressureEditingLibrary::IsValidOpticalProperties(
                            Override.OpticalProperties, Reason))
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C12"), Base + TEXT(".TriangleOverrides"), FString::Printf(TEXT("Component '%s' triangle override: %s"), *Component.Name, *Reason), Section, Component.ComponentId, FGuid{}, Index, NAME_None, 0, Override.ProxyTriangleIndex);
                    if (!Config.bProxyGenerationRequired && Config.GeneratedTriangleCount > 0
                        && Override.ProxyTriangleIndex >= Config.GeneratedTriangleCount)
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-C15"), Base + TEXT(".TriangleOverrides"), FString::Printf(TEXT("Component '%s' triangle override index %d is outside the generated proxy."), *Component.Name, Override.ProxyTriangleIndex), Section, Component.ComponentId, FGuid{}, Index, NAME_None, 0, Override.ProxyTriangleIndex);
                }
            }
            if (Config.bApplyOneOpticalConfigurationToEntireComponent
                && (!Config.LogicalRegionOverrides.IsEmpty() || !Config.TriangleOverrides.IsEmpty()))
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SRP-C13"), Base, FString::Printf(TEXT("Component '%s' applies one optical configuration; stored region and triangle overrides are currently ignored."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index);
            if (Config.bUseGlobalFallbackOpticalProperties)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SRP-C14"), Base + TEXT(".bUseGlobalFallbackOpticalProperties"), FString::Printf(TEXT("Component '%s' uses the global fallback optical properties."), *Component.Name), Section, Component.ComponentId, FGuid{}, Index);
        }
        if (Srp.bEnabled && IncludedCount == 0)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-005"), TEXT("Components"), TEXT("SRP is enabled, but no component is included in the SRP proxy."), Section);
        if (Srp.bEnabled && !Srp.bComputeEclipse)
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SRP-006"), TEXT("SolarRadiationPressure.bComputeEclipse"), TEXT("Celestial-body eclipse attenuation is disabled; every SRP facet will be treated as fully sunlit unless component shadowing blocks it."), Section);
        if (Srp.bEnabled && !Srp.bComputeComponentShadows)
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SRP-007"), TEXT("SolarRadiationPressure.bComputeComponentShadows"), TEXT("Mutual component shadowing is disabled; overlapping components may cause SRP to be overestimated."), Section);

        FText HelperWarning;
        FText HelperError;
        UTGSolarRadiationPressureEditingLibrary::
            ValidateSolarRadiationPressureAuthoring(
                Scenario, HelperWarning, HelperError);
        const auto HasSeverity = [&Report, Section](
            ETGScenarioReviewSeverity Severity)
        {
            return Report.Issues.ContainsByPredicate(
                [Severity, Section](const FTGScenarioReviewIssue& Issue)
                {
                    return Issue.Section == Section
                        && Issue.Severity == Severity;
                });
        };
        if (!HelperError.IsEmpty()
            && !HasSeverity(ETGScenarioReviewSeverity::Error))
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SRP-AUTHORING"), TEXT("SolarRadiationPressure"), HelperError.ToString(), Section);
        if (!HelperWarning.IsEmpty()
            && !HasSeverity(ETGScenarioReviewSeverity::Warning))
            Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("SRP-AUTHORING"), TEXT("SolarRadiationPressure"), HelperWarning.ToString(), Section);
    }

    void ValidateEnvironment(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        if (Scenario.Atmosphere.bEnabled)
        {
            const FTGAtmosphereConfig& Atmosphere = Scenario.Atmosphere;
            FTGCelestialCatalogEntry AtmosphereEntry;
            if (!FindCatalogByUserName(
                    Atmosphere.CentralBodyName, AtmosphereEntry)
                || AtmosphereEntry.SourceRole
                    == ETGCelestialSourceRole::SystemBarycenter)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ATM-001"), TEXT("Atmosphere.CentralBodyName"), TEXT("Atmosphere central body must resolve to a physical celestial-body catalog entry."), ETGScenarioReviewSection::Atmosphere);

            if (Atmosphere.Model != ETGAtmosphereModel::UploadedProfile
                && Atmosphere.Model != ETGAtmosphereModel::CubicHarrisPriesterEarth)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ATM-002"), TEXT("Atmosphere.Model"), TEXT("Select Uploaded Profile or Cubic Harris-Priester."), ETGScenarioReviewSection::Atmosphere);

            FText Summary;
            FText Error;
            if (Atmosphere.Model == ETGAtmosphereModel::UploadedProfile)
            {
                if (!UTGEnvironmentEditingLibrary::ValidateGeneralAtmosphereProfileCsv(
                        Atmosphere.GeneralProfileCsvPath, Summary, Error))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ATM-005"), TEXT("Atmosphere.GeneralProfileCsvPath"), Error.ToString(), ETGScenarioReviewSection::Atmosphere);
            }
            else if (Atmosphere.Model == ETGAtmosphereModel::CubicHarrisPriesterEarth)
            {
                if (!Atmosphere.CentralBodyName.Equals(
                        TEXT("Earth"), ESearchCase::IgnoreCase))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ATM-003"), TEXT("Atmosphere.CentralBodyName"), TEXT("Cubic Harris-Priester is Earth-only; select Earth as the atmosphere central body."), ETGScenarioReviewSection::Atmosphere);
                if (!IsFinite(Atmosphere.CenteredAverageF107SolarFluxUnits)
                    || Atmosphere.CenteredAverageF107SolarFluxUnits <= 0.0)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ATM-004"), TEXT("Atmosphere.CenteredAverageF107SolarFluxUnits"), TEXT("Centered 81-day average F10.7 must be a positive finite value."), ETGScenarioReviewSection::Atmosphere);
                if (!UTGEnvironmentEditingLibrary::ValidateChpCoefficientCsv(
                        Atmosphere.ChpCoefficientCsvPath, Summary, Error))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ATM-006"), TEXT("Atmosphere.ChpCoefficientCsvPath"), Error.ToString(), ETGScenarioReviewSection::Atmosphere);
                if (!UTGEnvironmentEditingLibrary::ValidateChpMolecularProfileCsv(
                        Atmosphere.ChpMolecularProfileCsvPath, Summary, Error))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ATM-007"), TEXT("Atmosphere.ChpMolecularProfileCsvPath"), Error.ToString(), ETGScenarioReviewSection::Atmosphere);
            }

            FText CompatibilityWarning;
            FText CompatibilityError;
            UTGEnvironmentEditingLibrary::ValidateAtmosphereScenario(
                Scenario, CompatibilityWarning, CompatibilityError);
        }

        if (Scenario.Aerodynamics.bEnabled)
        {
            const FTGAerodynamicsConfig& Aero = Scenario.Aerodynamics;
            if (!Scenario.Atmosphere.bEnabled)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-001"), TEXT("Aerodynamics.bEnabled"), TEXT("Aerodynamics requires Atmosphere to be enabled."), ETGScenarioReviewSection::Aerodynamics);
            if (!IsFinite(Aero.ReferenceAreaSquareMeters)
                || Aero.ReferenceAreaSquareMeters <= 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-002"), TEXT("Aerodynamics.ReferenceAreaSquareMeters"), TEXT("Aerodynamic reference area must be a positive finite value."), ETGScenarioReviewSection::Aerodynamics);
            if (!IsFinite(Aero.ReferenceLengthMeters)
                || Aero.ReferenceLengthMeters <= 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-003"), TEXT("Aerodynamics.ReferenceLengthMeters"), TEXT("Aerodynamic reference length must be a positive finite value."), ETGScenarioReviewSection::Aerodynamics);
            if (!IsFinite(Aero.MinimumDynamicPressurePascals)
                || Aero.MinimumDynamicPressurePascals < 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-004"), TEXT("Aerodynamics.MinimumDynamicPressurePascals"), TEXT("Minimum dynamic pressure must be a nonnegative finite value."), ETGScenarioReviewSection::Aerodynamics);
            if (!IsFinite(Aero.MaximumValidDynamicPressurePascals)
                || Aero.MaximumValidDynamicPressurePascals < 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-005"), TEXT("Aerodynamics.MaximumValidDynamicPressurePascals"), TEXT("Maximum valid dynamic pressure must be a nonnegative finite value."), ETGScenarioReviewSection::Aerodynamics);
            if (IsFinite(Aero.MinimumDynamicPressurePascals)
                && IsFinite(Aero.MaximumValidDynamicPressurePascals)
                && Aero.MaximumValidDynamicPressurePascals
                    < Aero.MinimumDynamicPressurePascals)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-006"), TEXT("Aerodynamics.MaximumValidDynamicPressurePascals"), TEXT("Maximum valid dynamic pressure must be greater than or equal to minimum dynamic pressure."), ETGScenarioReviewSection::Aerodynamics);
            if (!Aero.Database.bEnabled && !Aero.bEnableConstantDragFallback)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-007"), TEXT("Aerodynamics"), TEXT("Enable the aerodynamic coefficient database, the constant-drag fallback, or both."), ETGScenarioReviewSection::Aerodynamics);
            if (Aero.bEnableConstantDragFallback
                && (!IsFinite(Aero.FallbackDragCoefficient)
                    || Aero.FallbackDragCoefficient <= 0.0))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-008"), TEXT("Aerodynamics.FallbackDragCoefficient"), TEXT("Fallback drag coefficient must be a positive finite value."), ETGScenarioReviewSection::Aerodynamics);

            if (Aero.Database.bEnabled)
            {
                if (Aero.Database.Interpolation
                        != ETGAerodynamicDatabaseInterpolation::InverseDistance
                    && Aero.Database.Interpolation
                        != ETGAerodynamicDatabaseInterpolation::NearestRow)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-017"), TEXT("Aerodynamics.Database.Interpolation"), TEXT("Select Inverse Distance or Nearest Row database interpolation."), ETGScenarioReviewSection::Aerodynamics);
                if (Aero.Database.Extrapolation
                        != ETGAerodynamicDatabaseExtrapolation::ConstantDragFallback
                    && Aero.Database.Extrapolation
                        != ETGAerodynamicDatabaseExtrapolation::NearestRow)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-018"), TEXT("Aerodynamics.Database.Extrapolation"), TEXT("Select Constant-Drag Fallback or Nearest Row database extrapolation."), ETGScenarioReviewSection::Aerodynamics);
                if (Aero.Database.Extrapolation
                        == ETGAerodynamicDatabaseExtrapolation::ConstantDragFallback
                    && !Aero.bEnableConstantDragFallback)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("AER-009"), TEXT("Aerodynamics.Database.Extrapolation"), TEXT("Database extrapolation is set to Constant-Drag Fallback, but the constant-drag fallback is disabled."), ETGScenarioReviewSection::Aerodynamics);
                if (Aero.Database.Interpolation
                    == ETGAerodynamicDatabaseInterpolation::InverseDistance)
                {
                    if (Aero.Database.NeighborCount <= 0)
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-012"), TEXT("Aerodynamics.Database.NeighborCount"), TEXT("Aerodynamic database neighbor count must be a positive integer."), ETGScenarioReviewSection::Aerodynamics);
                    if (!IsFinite(Aero.Database.InverseDistancePower)
                        || Aero.Database.InverseDistancePower <= 0.0)
                        Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-014"), TEXT("Aerodynamics.Database.InverseDistancePower"), TEXT("Inverse-distance power must be a positive finite value."), ETGScenarioReviewSection::Aerodynamics);
                }
                if (Aero.Database.bUseMaximumNormalizedNeighborDistance
                    && (!IsFinite(Aero.Database.MaximumNormalizedNeighborDistance)
                        || Aero.Database.MaximumNormalizedNeighborDistance <= 0.0))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-015"), TEXT("Aerodynamics.Database.MaximumNormalizedNeighborDistance"), TEXT("Maximum normalized neighbor distance must be a positive finite value."), ETGScenarioReviewSection::Aerodynamics);
                if (!IsFinite(Aero.Database.MomentReferenceCenterBodyMeters))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-016"), TEXT("Aerodynamics.Database.MomentReferenceCenterBodyMeters"), TEXT("Aerodynamic moment reference center must contain finite B-frame values."), ETGScenarioReviewSection::Aerodynamics);

                FText Summary;
                FText DatabaseError;
                const bool bDatabaseValid =
                    UTGEnvironmentEditingLibrary::ValidateAerodynamicDatabaseCsv(
                        Aero.Database.CsvFilePath,
                        UTGEnvironmentEditingLibrary::GetFlattenedArticulationDofCount(Scenario),
                        Summary,
                        DatabaseError);
                if (!bDatabaseValid)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-VALIDATION"), TEXT("Aerodynamics.Database.CsvFilePath"), DatabaseError.ToString(), ETGScenarioReviewSection::Aerodynamics);
                else if (Aero.Database.Interpolation
                            == ETGAerodynamicDatabaseInterpolation::InverseDistance
                    && Aero.Database.NeighborCount > 0)
                {
                    TArray<FString> Lines;
                    if (FFileHelper::LoadFileToStringArray(
                            Lines, *Aero.Database.CsvFilePath))
                    {
                        int32 RowCount = 0;
                        for (const FString& Line : Lines)
                        {
                            if (!Line.TrimStartAndEnd().IsEmpty())
                            {
                                ++RowCount;
                            }
                        }
                        if (Aero.Database.NeighborCount > RowCount)
                            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("ADB-013"), TEXT("Aerodynamics.Database.NeighborCount"), FString::Printf(TEXT("Aerodynamic neighbor count %d exceeds the %d available rows; it will be clamped."), Aero.Database.NeighborCount, RowCount), ETGScenarioReviewSection::Aerodynamics);
                    }
                }
            }

            if (Aero.bEnableConstantDragFallback)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("AER-010"), TEXT("Aerodynamics.bEnableConstantDragFallback"), TEXT("Constant-drag fallback is translation-only: it produces no aerodynamic attitude torque or articulated-joint load."), ETGScenarioReviewSection::Aerodynamics);
            const int32 DofCount =
                UTGEnvironmentEditingLibrary::GetFlattenedArticulationDofCount(Scenario);
            if (Aero.Database.bEnabled && DofCount > 0)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("AER-011"), TEXT("Aerodynamics.Database"), FString::Printf(TEXT("The aggregate aerodynamic database supplies total force and moment but no uniquely resolved aerodynamic generalized joint loads for %d articulation DOF(s)."), DofCount), ETGScenarioReviewSection::Aerodynamics);

            // Keep the established helper in the validation path as a final
            // compatibility guard. Direct checks above aggregate all fields.
            FText Warning;
            FText Error;
            UTGEnvironmentEditingLibrary::ValidateAerodynamicsScenario(
                Scenario, Warning, Error);
        }
    }

    void ValidateCrossSystem(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        for (int32 Index = 0; Index < Scenario.Components.Num(); ++Index)
        {
            const FTGComponentConfig& Component = Scenario.Components[Index];
            if (!Component.bVariableMass)
                continue;
            const bool bUsed = Scenario.Thrusters.ContainsByPredicate(
                [&Component](const FTGThrusterConfig& Thruster)
                {
                    return Thruster.PropellantComponentName.Equals(
                        Component.Name, ESearchCase::IgnoreCase);
                });
            if (!bUsed)
                Add(Report, ETGScenarioReviewSeverity::Warning, TEXT("XMS-001"), FString::Printf(TEXT("Components[%d].bVariableMass"), Index), FString::Printf(TEXT("Variable-mass component '%s' is not assigned to any thruster; its mass will remain constant."), *Component.Name), ETGScenarioReviewSection::CrossSystemAndSpice, Component.ComponentId, FGuid{}, Index);
        }
    }

    void ValidateSpice(
        const FTGSimulationScenario& Scenario,
        FTGScenarioReviewReport& Report)
    {
        const ETGScenarioReviewSection Section = ETGScenarioReviewSection::CrossSystemAndSpice;
        FString Diagnostic;
        if (!FSpiceBridge::LoadKernels(Diagnostic))
        {
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-001"), TEXT("SPICE.Kernels"), TEXT("SPICE kernels could not be loaded: ") + Diagnostic, Section);
            return;
        }

        double StartEt = 0.0;
        double FinalEt = 0.0;
        bool bStartEtValid = false;
        bool bFinalEtValid = false;
        if (Scenario.ScenarioAndSolver.StartUtc != FDateTime::MinValue())
        {
            bStartEtValid = FSpiceBridge::ConvertUTCToET(
                Scenario.ScenarioAndSolver.StartUtc.ToIso8601(), StartEt, Diagnostic);
            if (!bStartEtValid)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-002"), TEXT("ScenarioAndSolver.StartUtc"), TEXT("SPICE could not convert StartUtc UTC to ephemeris time: ") + Diagnostic, Section);
        }
        FDateTime FinalUtc;
        if (ResolveEffectiveFinalUtc(Scenario.ScenarioAndSolver, FinalUtc))
        {
            bFinalEtValid = FSpiceBridge::ConvertUTCToET(
                FinalUtc.ToIso8601(), FinalEt, Diagnostic);
            if (!bFinalEtValid)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-002"), TEXT("ScenarioAndSolver.FinalUtc"), TEXT("SPICE could not convert FinalUtc UTC to ephemeris time: ") + Diagnostic, Section);
        }

        for (int32 Index = 0; Index < Scenario.Thrusters.Num(); ++Index)
        {
            const FTGThrusterConfig& Thruster = Scenario.Thrusters[Index];
            double IgnitionEt = 0.0;
            if (Thruster.IgnitionTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                && Thruster.IgnitionUtc != FDateTime::MinValue()
                && !FSpiceBridge::ConvertUTCToET(
                    Thruster.IgnitionUtc.ToIso8601(), IgnitionEt, Diagnostic))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-012"), FString::Printf(TEXT("Thrusters[%d].IgnitionUtc"), Index), TEXT("Enter a valid ignition UTC time."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
            double ShutdownEt = 0.0;
            if (!Thruster.bNeverShutsDown
                && Thruster.ShutdownTimeMode == ETGThrusterTimeMode::AbsoluteUtc
                && Thruster.ShutdownUtc != FDateTime::MinValue()
                && !FSpiceBridge::ConvertUTCToET(
                    Thruster.ShutdownUtc.ToIso8601(), ShutdownEt, Diagnostic))
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("THR-015"), FString::Printf(TEXT("Thrusters[%d].ShutdownUtc"), Index), TEXT("Enter a valid shutdown UTC time."), ETGScenarioReviewSection::Actuators, FGuid{}, FGuid{}, Index);
        }

        const FTGCompactMoonCatalogCoverageEvaluation Coverage =
            UTGGravityCoverageLibrary::EvaluateCompactMoonCatalogCoverage(Scenario);
        if (Coverage.bCompactCatalogRequired
            && Coverage.bScenarioIntervalResolved
            && !Coverage.bCoverageSatisfied)
            Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-003"), TEXT("CelestialBodies"), TEXT("The selected moon/barycenter configuration requires a simulation interval from 2000-01-01 inclusive to 2050-01-01 exclusive."), Section);

        if (!bStartEtValid || !bFinalEtValid)
            return;

        const TArray<FTGCelestialCatalogEntry> Catalog =
            UTGCelestialCatalogLibrary::GetCelestialCatalog();
        TMap<FName, FTGCelestialCatalogEntry> Entries;
        for (const FTGCelestialCatalogEntry& Entry : Catalog)
            Entries.Add(Entry.CatalogKey, Entry);

        TSet<FName> RequiredCatalogKeys;
        for (const FTGCelestialBodyConfig& Body : Scenario.CelestialBodies)
        {
            const FTGCelestialCatalogEntry* Entry = Entries.Find(Body.CatalogKey);
            const bool bCanBecomeActive = Body.bGravityEnabled
                || Body.AutomaticActivationRadiusMeters > 0.0;
            if (Entry == nullptr || !bCanBecomeActive)
                continue;
            RequiredCatalogKeys.Add(Body.CatalogKey);
            if (Entry->SourceRole == ETGCelestialSourceRole::SystemBarycenter
                && Body.BarycenterResolutionRadiusMeters > 0.0)
            {
                for (const FTGCelestialCatalogEntry& Candidate : Catalog)
                {
                    if (Candidate.SystemKey == Entry->SystemKey
                        && Candidate.SourceRole
                            == ETGCelestialSourceRole::PhysicalSystemMember)
                        RequiredCatalogKeys.Add(Candidate.CatalogKey);
                }
            }
        }

        for (int32 Index = 0; Index < Scenario.CelestialBodies.Num(); ++Index)
        {
            const FTGCelestialBodyConfig& Body = Scenario.CelestialBodies[Index];
            const FTGCelestialCatalogEntry* Entry = Entries.Find(Body.CatalogKey);
            if (Entry == nullptr)
                continue;
            const bool bRequired = RequiredCatalogKeys.Contains(Body.CatalogKey);
            if (!bRequired)
                continue;

            FVector StartPosition;
            FVector StartVelocity;
            FVector FinalPosition;
            FVector FinalVelocity;
            if (!FSpiceBridge::GetBodyICRFStateSI(
                    Entry->SpiceTarget, StartEt, StartPosition, StartVelocity, Diagnostic)
                || !FSpiceBridge::GetBodyICRFStateSI(
                    Entry->SpiceTarget, FinalEt, FinalPosition, FinalVelocity, Diagnostic))
            {
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-004"), FString::Printf(TEXT("CelestialBodies[%d]"), Index), FString::Printf(TEXT("SPICE cannot resolve body '%s' across the requested simulation interval."), *Entry->DisplayName.ToString()), Section, FGuid{}, FGuid{}, Index);
                continue;
            }

            double Gm = 0.0;
            double Radius = 0.0;
            if (!FSpiceBridge::GetBodyGravityMetadataSI(
                    Entry->SpiceTarget, Gm, Radius, Diagnostic)
                || !IsFinite(Gm) || Gm <= 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-005"), FString::Printf(TEXT("CelestialBodies[%d]"), Index), FString::Printf(TEXT("SPICE cannot resolve a positive gravitational parameter for '%s'."), *Entry->DisplayName.ToString()), Section, FGuid{}, FGuid{}, Index);

            if (Body.MaximumHarmonicDegreeUsed > 0)
            {
                tgsim::Mat3d Rotation;
                if (!FSpiceBridge::GetBodyFixedToICRF(
                        Entry->SpiceTarget, StartEt, Rotation, Diagnostic))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-006"), FString::Printf(TEXT("CelestialBodies[%d].HarmonicModelCsvFilePath"), Index), FString::Printf(TEXT("SPICE cannot resolve the body-fixed frame required by harmonic gravity for '%s'."), *Entry->DisplayName.ToString()), Section, FGuid{}, FGuid{}, Index);
            }

            const double Distance = FVector::Distance(
                Scenario.InitialState.PositionMeters, StartPosition);
            if (Body.MaximumHarmonicDegreeUsed == 0 && Distance <= 1.0e-6)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-010"), TEXT("InitialState.PositionMeters"), FString::Printf(TEXT("Initial spacecraft position coincides with gravity source '%s', where point-mass gravity is singular."), *Entry->DisplayName.ToString()), Section, FGuid{}, FGuid{}, Index);
        }

        if (Scenario.Atmosphere.bEnabled)
        {
            FTGCelestialCatalogEntry AtmosphereEntry;
            if (FindCatalogByUserName(Scenario.Atmosphere.CentralBodyName, AtmosphereEntry))
            {
                double Gm = 0.0;
                double Radius = 0.0;
                const bool bMetadataValid =
                    FSpiceBridge::GetBodyGravityMetadataSI(
                        AtmosphereEntry.SpiceTarget, Gm, Radius, Diagnostic)
                    && IsFinite(Radius) && Radius > 0.0;
                if (!bMetadataValid)
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-007"), TEXT("Atmosphere.CentralBodyName"), FString::Printf(TEXT("SPICE cannot resolve a positive physical radius for atmosphere body '%s'."), *Scenario.Atmosphere.CentralBodyName), Section);
                tgsim::Mat3d Rotation;
                tgsim::Vec3d AngularVelocity;
                if (!FSpiceBridge::GetBodyFixedToICRF(
                        AtmosphereEntry.SpiceTarget, StartEt, Rotation, Diagnostic)
                    || !FSpiceBridge::GetBodyAngularVelocityICRF(
                        AtmosphereEntry.SpiceTarget, StartEt, AngularVelocity, Diagnostic))
                    Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-008"), TEXT("Atmosphere.CentralBodyName"), FString::Printf(TEXT("SPICE cannot resolve the body-fixed frame and angular velocity required for atmosphere body '%s'."), *Scenario.Atmosphere.CentralBodyName), Section);
            }
        }

        if (Scenario.SolarRadiationPressure.bEnabled)
        {
            FVector StartPosition;
            FVector StartVelocity;
            FVector FinalPosition;
            FVector FinalVelocity;
            double Gm = 0.0;
            double Radius = 0.0;
            if (!FSpiceBridge::GetBodyICRFStateSI(
                    TEXT("Sun"), StartEt, StartPosition, StartVelocity, Diagnostic)
                || !FSpiceBridge::GetBodyICRFStateSI(
                    TEXT("Sun"), FinalEt, FinalPosition, FinalVelocity, Diagnostic)
                || !FSpiceBridge::GetBodyGravityMetadataSI(
                    TEXT("Sun"), Gm, Radius, Diagnostic)
                || Radius <= 0.0)
                Add(Report, ETGScenarioReviewSeverity::Error, TEXT("SPC-009"), TEXT("SolarRadiationPressure"), TEXT("SPICE cannot resolve the Sun state and physical radius required by SRP."), Section);
        }
    }
}

FText FTGScenarioReviewIssue::ToDisplayText() const
{
    const TCHAR* SeverityText =
        Severity == ETGScenarioReviewSeverity::Error
            ? TEXT("ERROR")
            : TEXT("WARNING");
    return FText::FromString(FString::Printf(
        TEXT("[%s] %s: %s"),
        SeverityText,
        *Path,
        *Message.ToString()));
}

FTGScenarioReviewReport
UTGScenarioReviewValidationLibrary::ValidateScenarioForAuthoringReview(
    const UObject* WorldContextObject,
    const FTGSimulationScenario& Scenario)
{
    FTGScenarioReviewReport Report;
    Report.bValidationCompleted = true;

    TGScenarioReview::ValidateSolver(Scenario, Report);
    TGScenarioReview::ValidateInitialState(Scenario, Report);
    TGScenarioReview::ValidateComponents(Scenario, Report);
    TGScenarioReview::ValidateActuators(Scenario, Report);
    TGScenarioReview::ValidateController(WorldContextObject, Scenario, Report);
    TGScenarioReview::ValidateGravity(Scenario, Report);
    TGScenarioReview::ValidateSrp(Scenario, Report);
    TGScenarioReview::ValidateEnvironment(Scenario, Report);
    TGScenarioReview::ValidateCrossSystem(Scenario, Report);
    TGScenarioReview::ValidateSpice(Scenario, Report);

    Report.Issues.StableSort(
        [](const FTGScenarioReviewIssue& Left,
           const FTGScenarioReviewIssue& Right)
        {
            if (Left.Section != Right.Section)
            {
                return static_cast<uint8>(Left.Section)
                    < static_cast<uint8>(Right.Section);
            }

            return static_cast<uint8>(Left.Severity)
                < static_cast<uint8>(Right.Severity);
        });

    return Report;
}

bool UTGScenarioReviewValidationLibrary::CanSimulateCurrentScenario(
    const UObject* WorldContextObject,
    FText& OutReason)
{
    OutReason = FText::GetEmpty();
    if (WorldContextObject == nullptr
        || WorldContextObject->GetWorld() == nullptr
        || WorldContextObject->GetWorld()->GetGameInstance() == nullptr)
    {
        OutReason = FText::FromString(
            TEXT("The simulation scenario service is unavailable."));
        return false;
    }

    const UTGSimulationSubsystem* Subsystem =
        WorldContextObject->GetWorld()->GetGameInstance()
            ->GetSubsystem<UTGSimulationSubsystem>();
    if (Subsystem == nullptr)
    {
        OutReason = FText::FromString(
            TEXT("The simulation scenario service is unavailable."));
        return false;
    }
    if (Subsystem->CanSimulateCurrentScenario(OutReason))
    {
        return true;
    }

    // The configuration Blueprint already calls this gate from Simulate.
    // Reuse the live Review panel so an unvalidated draft follows the exact
    // same validation and presentation path as BTN_ValidateScenario.
    TArray<UUserWidget*> ReviewWidgets;
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(
        WorldContextObject,
        ReviewWidgets,
        UTGConfigReviewWidgetBase::StaticClass(),
        false);
    for (UUserWidget* Widget : ReviewWidgets)
    {
        if (UTGConfigReviewWidgetBase* ReviewPanel =
                Cast<UTGConfigReviewWidgetBase>(Widget))
        {
            return ReviewPanel->PrepareCurrentDraftForSimulation(OutReason);
        }
    }

    return false;
}

bool UTGScenarioReviewValidationLibrary::RefreshReviewPanelInSwitcher(
    UWidgetSwitcher* Switcher)
{
    if (Switcher == nullptr)
        return false;
    for (int32 Index = 0; Index < Switcher->GetNumWidgets(); ++Index)
    {
        if (UTGConfigReviewWidgetBase* Panel =
                TGScenarioReview::
                    FindWidgetDescendantOfType<UTGConfigReviewWidgetBase>(
                        Switcher->GetWidgetAtIndex(Index)))
        {
            Panel->RefreshFromCurrentDraft();
            return true;
        }
    }
    return false;
}

bool UTGScenarioReviewValidationLibrary::
    NotifyReviewPanelSimulationBlockedInSwitcher(
        UWidgetSwitcher* Switcher,
        FText Reason)
{
    if (Switcher == nullptr)
        return false;
    for (int32 Index = 0; Index < Switcher->GetNumWidgets(); ++Index)
    {
        if (UTGConfigReviewWidgetBase* Panel =
                TGScenarioReview::
                    FindWidgetDescendantOfType<UTGConfigReviewWidgetBase>(
                        Switcher->GetWidgetAtIndex(Index)))
        {
            Panel->NotifySimulationBlocked(Reason);
            return true;
        }
    }
    return false;
}

bool UTGScenarioReviewValidationLibrary::NavigateReviewIssueInSwitcher(
    UWidgetSwitcher* Switcher,
    int32 TargetPanelIndex,
    const FTGScenarioReviewIssue& Issue)
{
    if (Switcher == nullptr
        || TargetPanelIndex < 0
        || TargetPanelIndex >= Switcher->GetNumWidgets())
    {
        return false;
    }

    UWidget* TargetPanel = Switcher->GetWidgetAtIndex(TargetPanelIndex);
    if (TargetPanel == nullptr)
    {
        return false;
    }

    Switcher->SetActiveWidgetIndex(TargetPanelIndex);

    UWidget* NavigationTarget =
        TGScenarioReview::FindReviewNavigationTarget(TargetPanel);
    const bool bExactNavigation = NavigationTarget != nullptr
        && ITGScenarioReviewNavigationTarget::
            Execute_NavigateToScenarioReviewIssue(NavigationTarget, Issue);

    if (!bExactNavigation)
    {
        TargetPanel->SetIsEnabled(true);
        TargetPanel->SetKeyboardFocus();
        TargetPanel->SetToolTipText(Issue.ToDisplayText());
    }

    return bExactNavigation;
}
