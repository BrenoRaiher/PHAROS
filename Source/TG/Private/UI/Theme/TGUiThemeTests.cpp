// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

#include "Styling/SlateTypes.h"
#include "UI/Theme/TGUiTheme.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGUiThemeAccessibilityTest,
    "TG.UI.Theme.Accessibility",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGUiThemeAccessibilityTest::RunTest(const FString& Parameters)
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();

    auto TestContrast = [this](
        const TCHAR* Label,
        const FLinearColor& Foreground,
        const FLinearColor& Background,
        const double Minimum)
    {
        const double Actual =
            TGUiTheme::ContrastRatio(Foreground, Background);
        TestTrue(
            FString::Printf(
                TEXT("%s contrast is %.2f:1 (minimum %.2f:1)"),
                Label,
                Actual,
                Minimum),
            Actual >= Minimum);
    };

    // 4.5:1 is the WCAG AA threshold for ordinary text. The project uses
    // these pairs in compact, information-dense engineering controls.
    TestContrast(TEXT("Primary text on rows"),
        Palette.TextPrimary, Palette.Surface, 4.5);
    TestContrast(TEXT("Primary text on selected rows"),
        Palette.TextPrimary, Palette.Selection, 4.5);
    TestContrast(TEXT("Secondary text on rows"),
        Palette.TextSecondary, Palette.Surface, 4.5);
    TestContrast(TEXT("Muted caption text on rows"),
        Palette.TextMuted, Palette.Surface, 4.5);
    TestContrast(TEXT("Accent on surface"),
        Palette.Accent, Palette.Surface, 4.5);
    TestContrast(TEXT("Information on surface"),
        Palette.Info, Palette.Surface, 4.5);
    TestContrast(TEXT("Success on surface"),
        Palette.Success, Palette.Surface, 4.5);
    TestContrast(TEXT("Warning on surface"),
        Palette.Warning, Palette.Surface, 4.5);
    TestContrast(TEXT("Error on surface"),
        Palette.Error, Palette.Surface, 4.5);
    TestContrast(TEXT("Strong input boundary"),
        Palette.BorderStrong, Palette.Input, 3.0);

    return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FTGUiThemeTokenContractTest,
    "TG.UI.Theme.TokenContract",
    EAutomationTestFlags::EditorContext |
        EAutomationTestFlags::EngineFilter)

bool FTGUiThemeTokenContractTest::RunTest(const FString& Parameters)
{
    const FTGUiTypography& Typography = TGUiTheme::GetTypography();
    TestTrue(TEXT("Caption is smaller than field labels"),
        Typography.Caption < Typography.FieldLabel);
    TestTrue(TEXT("Field labels are smaller than body text"),
        Typography.FieldLabel < Typography.Body);
    TestEqual(TEXT("Section and body sizes share the 12 px grid"),
        Typography.Section, Typography.Body);
    TestTrue(TEXT("Body is smaller than panel titles"),
        Typography.Body < Typography.PanelTitle);
    TestTrue(TEXT("Panel titles are smaller than screen titles"),
        Typography.PanelTitle < Typography.ScreenTitle);
    TestTrue(TEXT("Screen titles are smaller than product titles"),
        Typography.ScreenTitle < Typography.ProductTitle);

    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();
    TestTrue(TEXT("Spacing begins above zero"),
        Spacing.ExtraSmall > 0.0f);
    TestTrue(TEXT("Spacing tokens increase: XS to S"),
        Spacing.ExtraSmall < Spacing.Small);
    TestTrue(TEXT("Spacing tokens increase: S to regular"),
        Spacing.Small < Spacing.Regular);
    TestTrue(TEXT("Spacing tokens increase: regular to large"),
        Spacing.Regular < Spacing.Large);
    TestTrue(TEXT("Spacing tokens increase: large to section"),
        Spacing.Large < Spacing.Section);
    TestTrue(TEXT("Spacing tokens increase: section to wide"),
        Spacing.Section < Spacing.Wide);
    TestTrue(TEXT("Spacing tokens increase: wide to extra-wide"),
        Spacing.Wide < Spacing.ExtraWide);

    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    TestFalse(TEXT("Selected rows differ from ordinary rows"),
        Palette.Selection.Equals(Palette.Surface));
    TestFalse(TEXT("Error and warning signals are distinct"),
        Palette.Error.Equals(Palette.Warning));

    const FCheckBoxStyle CheckBoxStyle = TGUiTheme::MakeCheckBoxStyle();
    TestEqual(TEXT("Checkbox indicator width is 18 Slate units"),
        CheckBoxStyle.UncheckedImage.ImageSize.X, 18.0f);
    TestEqual(TEXT("Checkbox indicator height is 18 Slate units"),
        CheckBoxStyle.UncheckedImage.ImageSize.Y, 18.0f);
    TestFalse(TEXT("Checkboxes retain an engine checked-state glyph"),
        CheckBoxStyle.CheckedImage.GetResourceName().IsNone());
    TestFalse(TEXT("Checked and unchecked checkbox glyphs are distinct"),
        CheckBoxStyle.CheckedImage.GetResourceName()
            == CheckBoxStyle.UncheckedImage.GetResourceName());
    TestEqual(TEXT("Checkbox background width is 18 Slate units"),
        CheckBoxStyle.BackgroundImage.ImageSize.X, 18.0f);
    TestEqual(TEXT("Checkbox background height is 18 Slate units"),
        CheckBoxStyle.BackgroundImage.ImageSize.Y, 18.0f);

    const FComboBoxStyle ComboBoxStyle = TGUiTheme::MakeComboBoxStyle();
    TestFalse(TEXT("Combo boxes retain an engine down-arrow resource"),
        ComboBoxStyle.ComboButtonStyle.DownArrowImage.GetResourceName()
            .IsNone());
    const FLinearColor AtmosphereButtonNormal(
        0.012286f, 0.018500f, 0.027321f, 1.0f);
    const FLinearColor AtmosphereButtonHovered(
        0.019382f, 0.030713f, 0.045186f, 1.0f);
    const FLinearColor AtmosphereButtonPressed(
        0.008568f, 0.063010f, 0.122139f, 1.0f);
    TestTrue(TEXT("Combo normal fill matches Atmosphere"),
        ComboBoxStyle.ComboButtonStyle.ButtonStyle.Normal.TintColor
            .GetSpecifiedColor().Equals(AtmosphereButtonNormal));
    TestTrue(TEXT("Combo hover fill matches Atmosphere"),
        ComboBoxStyle.ComboButtonStyle.ButtonStyle.Hovered.TintColor
            .GetSpecifiedColor().Equals(AtmosphereButtonHovered));
    TestTrue(TEXT("Combo pressed fill matches Atmosphere"),
        ComboBoxStyle.ComboButtonStyle.ButtonStyle.Pressed.TintColor
            .GetSpecifiedColor().Equals(AtmosphereButtonPressed));
    TestTrue(TEXT("Combo menu background matches Atmosphere"),
        ComboBoxStyle.ComboButtonStyle.MenuBorderBrush.TintColor
            .GetSpecifiedColor().Equals(AtmosphereButtonHovered));
    TestEqual(TEXT("Combo arrow width matches Atmosphere"),
        ComboBoxStyle.ComboButtonStyle.DownArrowImage.ImageSize.X, 16.0f);
    TestEqual(TEXT("Combo content left padding matches Atmosphere"),
        ComboBoxStyle.ContentPadding.Left, 10.0f);
    TestEqual(TEXT("Combo row top padding matches Atmosphere"),
        ComboBoxStyle.MenuRowPadding.Top, 8.0f);

    const FTableRowStyle ComboRowStyle = TGUiTheme::MakeTableRowStyle();
    TestTrue(TEXT("Combo selected row matches Atmosphere"),
        ComboRowStyle.ActiveBrush.TintColor.GetSpecifiedColor().Equals(
            AtmosphereButtonPressed));
    TestTrue(TEXT("Combo ordinary row matches Atmosphere"),
        ComboRowStyle.EvenRowBackgroundBrush.TintColor.GetSpecifiedColor()
            .Equals(AtmosphereButtonNormal));

    const FScrollBarStyle ScrollBarStyle = TGUiTheme::MakeScrollBarStyle();
    const FLinearColor GravityRestingThumb(
        0.034340f, 0.054480f, 0.080220f, 1.0f);
    const FLinearColor GravityActiveThumb(
        0.070360f, 0.479320f, 0.686685f, 1.0f);
    TestTrue(TEXT("Scrollbar resting thumb matches Gravity Sources"),
        ScrollBarStyle.NormalThumbImage.TintColor.GetSpecifiedColor().Equals(
            GravityRestingThumb));
    TestTrue(TEXT("Scrollbar hover thumb matches Gravity Sources"),
        ScrollBarStyle.HoveredThumbImage.TintColor.GetSpecifiedColor().Equals(
            GravityActiveThumb));
    TestTrue(TEXT("Scrollbar dragged thumb matches Gravity Sources"),
        ScrollBarStyle.DraggedThumbImage.TintColor.GetSpecifiedColor().Equals(
            GravityActiveThumb));
    TestEqual(TEXT("Scrollbar thickness is 8 Slate units"),
        ScrollBarStyle.Thickness, 8.0f);

    return !HasAnyErrors();
}

#endif
