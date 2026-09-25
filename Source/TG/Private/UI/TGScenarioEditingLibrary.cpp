// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/TGScenarioEditingLibrary.h"

FTGScenarioSolverConfig
UTGScenarioEditingLibrary::SetScenarioSolverDoubleField(
    const FTGScenarioSolverConfig& Config,
    const ETGScenarioSolverDoubleField Field,
    const double Value)
{
    FTGScenarioSolverConfig Result = Config;

    switch (Field)
    {
        case ETGScenarioSolverDoubleField::DurationSeconds:
            Result.DurationSeconds = Value;
            break;

        case ETGScenarioSolverDoubleField::
            MaximumIntegratorStepSeconds:
            Result.MaximumIntegratorStepSeconds = Value;
            break;

        case ETGScenarioSolverDoubleField::
            InitialIntegratorStepSeconds:
            Result.InitialIntegratorStepSeconds = Value;
            break;

        case ETGScenarioSolverDoubleField::AbsoluteTolerance:
            Result.AbsoluteTolerance = Value;
            break;

        case ETGScenarioSolverDoubleField::RelativeTolerance:
            Result.RelativeTolerance = Value;
            break;

        case ETGScenarioSolverDoubleField::OutputStepSeconds:
            Result.OutputStepSeconds = Value;
            break;

        default:
            break;
    }

    return Result;
}

FTGScenarioSolverConfig
UTGScenarioEditingLibrary::SetScenarioSolverIntegerField(
    const FTGScenarioSolverConfig& Config,
    const ETGScenarioSolverIntegerField Field,
    const int32 Value)
{
    FTGScenarioSolverConfig Result = Config;

    switch (Field)
    {
        case ETGScenarioSolverIntegerField::
            MaximumIntegrationSteps:
            Result.MaximumIntegrationSteps = Value;
            break;

        case ETGScenarioSolverIntegerField::
            MaximumOutputSamples:
            Result.MaximumOutputSamples = Value;
            break;

        default:
            break;
    }

    return Result;
}