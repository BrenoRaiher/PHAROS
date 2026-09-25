// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "UI/Configuration/Review/TGReviewIssueRowWidget.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGReviewIssueRowUserFacingTextTest,
    "TG.UI.Review.IssueRow.UserFacingText",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGReviewIssueRowUserFacingTextTest::RunTest(const FString& Parameters)
{
    FTGScenarioReviewIssue Issue;
    Issue.Severity = ETGScenarioReviewSeverity::Error;
    Issue.Path = TEXT("ScenarioAndSolver.AbsoluteTolerance");
    Issue.Message = FText::FromString(
        TEXT("Absolute tolerance must be positive and finite."));
    const FString Displayed =
        UTGReviewIssueRowWidget::MakeUserFacingSummary(Issue).ToString();
    TestTrue(TEXT("The row includes a plain-language severity"),
        Displayed.StartsWith(TEXT("Error: ")));
    TestTrue(TEXT("The row includes the corrective message"),
        Displayed.Contains(TEXT("Absolute tolerance must be positive")));
    TestFalse(TEXT("The row does not expose the internal property path"),
        Displayed.Contains(Issue.Path));

    return !HasAnyErrors();
}

#endif
