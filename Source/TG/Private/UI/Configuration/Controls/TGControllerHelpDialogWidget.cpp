// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Controls/TGControllerHelpDialogWidget.h"

#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "UI/Theme/TGUiTheme.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

namespace TGControllerHelpDialogPrivate
{
    const FSlateBrush& BackdropBrush()
    {
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            FLinearColor::White,
            0.0f);
        return Brush;
    }

    const FSlateBrush& DialogBrush()
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette.Panel,
            4.0f,
            Palette.BorderStrong,
            1.0f);
        return Brush;
    }

    const FSlateBrush& CodeBrush()
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        static const FSlateBrush Brush = TGUiTheme::MakeRoundedBrush(
            Palette.Input,
            4.0f,
            Palette.Border,
            1.0f);
        return Brush;
    }

    TSharedRef<SWidget> MakeSection(
        const FString& Heading,
        const FString& Description,
        const FString& Code)
    {
        const FTGUiPalette& Palette = TGUiTheme::GetPalette();
        const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();

        return SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(STextBlock)
                .Text(FText::FromString(Heading))
                .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Section))
                .ColorAndOpacity(Palette.TextPrimary)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, Spacing.ExtraSmall, 0.0f, 0.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(Description))
                .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Body))
                .ColorAndOpacity(Palette.TextSecondary)
                .AutoWrapText(true)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, Spacing.Small, 0.0f, 0.0f)
            [
                SNew(SBorder)
                .BorderImage(&CodeBrush())
                .Padding(Spacing.Regular)
                [
                    SNew(STextBlock)
                    .Text(FText::FromString(Code))
                    .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Numeric))
                    .ColorAndOpacity(Palette.TextPrimary)
                ]
            ];
    }
}

void UTGControllerHelpDialogWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true);
    SetKeyboardFocus();
}

FReply UTGControllerHelpDialogWidget::NativeOnKeyDown(
    const FGeometry& InGeometry,
    const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        CloseDialog();
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

TSharedRef<SWidget> UTGControllerHelpDialogWidget::RebuildWidget()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    const FTGUiSpacing& Spacing = TGUiTheme::GetSpacing();

    RootSlateWidget =
        SNew(SOverlay)

        + SOverlay::Slot()
        [
            SNew(SBorder)
            .BorderImage(&TGControllerHelpDialogPrivate::BackdropBrush())
            .BorderBackgroundColor(Palette.Overlay)
            .Padding(0.0f)
        ]

        + SOverlay::Slot()
        .HAlign(HAlign_Center)
        .VAlign(VAlign_Center)
        .Padding(Spacing.Section)
        [
            SNew(SBox)
            .WidthOverride(760.0f)
            .HeightOverride(680.0f)
            [
                SNew(SBorder)
                .BorderImage(&TGControllerHelpDialogPrivate::DialogBrush())
                .Padding(Spacing.Wide)
                [
                    SNew(SVerticalBox)

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(
                            TEXT("CONTROLLER COMMAND REFERENCE")))
                        .Font(TGUiTheme::GetSlateFont(
                            ETGUiTextStyle::PanelTitle))
                        .ColorAndOpacity(Palette.TextPrimary)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Small, 0.0f, Spacing.Regular)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(
                            "Write commands in PHAROSUserController::ComputeControl. "
                            "Input arrays describe the current simulation state; "
                            "matching output indices command the corresponding "
                            "actuators. All output arrays begin at zero for each "
                            "evaluation.")))
                        .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Body))
                        .ColorAndOpacity(Palette.TextSecondary)
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .FillHeight(1.0f)
                    [
                        SNew(SScrollBox)
                        .Style(&TGUiTheme::GetScrollBoxStyle())
                        .ScrollBarStyle(&TGUiTheme::GetScrollBarStyle())
                        .ScrollBarThickness(FVector2D(8.0f, 8.0f))
                        .ScrollBarAlwaysVisible(true)
                        .AllowOverscroll(EAllowOverscroll::No)
                        .WheelScrollMultiplier(4.0f)

                        + SScrollBox::Slot()
                        [
                            SNew(SVerticalBox)

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            [
                                TGControllerHelpDialogPrivate::MakeSection(
                                    TEXT("Finding Actuators by Name"),
                                    TEXT(
                                        "Use the names authored in the scenario instead of "
                                        "assuming fixed indices. PHAROSStringView is not "
                                        "null-terminated, so compare both its length and data. "
                                        "The index found in each input array is the index used "
                                        "in the matching output array."),
                                    TEXT(
                                        "#include <cstring>\n\n"
                                        "bool Equals(PHAROSStringView view, const char* text)\n"
                                        "{\n"
                                        "    const size_t length = std::strlen(text);\n"
                                        "    return view.Length == static_cast<uint64_t>(length) &&\n"
                                        "        (length == 0 || (view.Data != nullptr &&\n"
                                        "         std::memcmp(view.Data, text, length) == 0));\n"
                                        "}\n\n"
                                        "uint64_t FindThruster(const PHAROSControlInput& input, const char* name)\n"
                                        "{\n"
                                        "    for (uint64_t i = 0; i < input.ThrusterCount; ++i)\n"
                                        "        if (Equals(input.Thrusters[i].Name, name)) return i;\n"
                                        "    return PHAROS_CONTROLLER_INVALID_INDEX;\n"
                                        "}\n\n"
                                        "uint64_t FindJoint(const PHAROSControlInput& input, const char* name)\n"
                                        "{\n"
                                        "    for (uint64_t i = 0; i < input.JointCount; ++i)\n"
                                        "        if (Equals(input.Joints[i].Name, name)) return i;\n"
                                        "    return PHAROS_CONTROLLER_INVALID_INDEX;\n"
                                        "}\n\n"
                                        "uint64_t FindWheel(const PHAROSControlInput& input, const char* name)\n"
                                        "{\n"
                                        "    for (uint64_t i = 0; i < input.ReactionWheelCount; ++i)\n"
                                        "        if (Equals(input.ReactionWheels[i].Name, name)) return i;\n"
                                        "    return PHAROS_CONTROLLER_INVALID_INDEX;\n"
                                        "}"))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                            [
                                TGControllerHelpDialogPrivate::MakeSection(
                                    TEXT("Thrusters"),
                                    TEXT(
                                        "The output index matches Input.Thrusters[index]. "
                                        "Only commanded thrusters use these values. Throttle "
                                        "is limited to 0 through 1; positive throttle requires "
                                        "a finite, positive specific impulse in seconds."),
                                    TEXT(
                                        "const uint64_t index = FindThruster(Input, \"Main Thruster\");\n"
                                        "if (index != PHAROS_CONTROLLER_INVALID_INDEX &&\n"
                                        "    index < Output.ThrusterCommandCount)\n"
                                        "{\n"
                                        "    Output.ThrusterCommands[index].Throttle = 0.40;\n"
                                        "    Output.ThrusterCommands[index].SpecificImpulseSeconds = 300.0;\n"
                                        "}"))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                            [
                                TGControllerHelpDialogPrivate::MakeSection(
                                    TEXT("Articulation DOFs"),
                                    TEXT(
                                        "The output index matches Input.Joints[index]. Use "
                                        "torque in N m for rotational DOFs and force in N for "
                                        "translational DOFs. Positive effort follows AxisJoint."),
                                    TEXT(
                                        "const uint64_t index = FindJoint(Input, \"Panel Hinge\");\n"
                                        "if (index != PHAROS_CONTROLLER_INVALID_INDEX &&\n"
                                        "    index < Output.JointEffortCount)\n"
                                        "{\n"
                                        "    Output.JointEffortsNewtonMetersOrNewtons[index] = effort;\n"
                                        "}"))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                            [
                                TGControllerHelpDialogPrivate::MakeSection(
                                    TEXT("Thruster Discharge Derivatives"),
                                    TEXT(
                                        "For a commanded thruster with a smooth analytic "
                                        "law, the matching thruster command may provide the "
                                        "total derivative of actual outward propellant "
                                        "discharge q = T/(g0 Isp), in kg/s^2. This is not a "
                                        "throttle derivative or an additional thrust command. "
                                        "Leave MassFlowDerivativeProvided false when the "
                                        "derivative is unknown; "
                                        "PHAROS then omits only its mass-second-derivative "
                                        "center-of-mass correction for that evaluation."),
                                    TEXT(
                                        "if (index < Output.ThrusterCommandCount)\n"
                                        "{\n"
                                        "    Output.ThrusterCommands[index].\n"
                                        "        MassFlowDerivativeProvided = 1;\n"
                                        "    Output.ThrusterCommands[index].\n"
                                        "        MassFlowDerivativeKilogramsPerSecondSquared = analyticQDot;\n"
                                        "}"))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                            [
                                TGControllerHelpDialogPrivate::MakeSection(
                                    TEXT("Reaction Wheels"),
                                    TEXT(
                                        "The output index matches Input.ReactionWheels[index]. "
                                        "The command is the stored-momentum derivative in N m; "
                                        "positive values increase momentum along AxisComponent."),
                                    TEXT(
                                        "const uint64_t index = FindWheel(Input, \"Z Wheel\");\n"
                                        "if (index != PHAROS_CONTROLLER_INVALID_INDEX &&\n"
                                        "    index < Output.ReactionWheelMomentumRateCount)\n"
                                        "{\n"
                                        "    Output.ReactionWheelMomentumRatesNewtonMeters[index] = momentumRate;\n"
                                        "}"))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                            [
                                TGControllerHelpDialogPrivate::MakeSection(
                                    TEXT("Additional External Torque"),
                                    TEXT(
                                        "This optional spacecraft-level torque is expressed in "
                                        "spacecraft axes in N m. Prefer modeled actuators when "
                                        "they represent the physical source."),
                                    TEXT(
                                        "Output.AdditionalExternalTorqueBodyNewtonMeters = {Tx, Ty, Tz};"))
                            ]

                            + SVerticalBox::Slot()
                            .AutoHeight()
                            .Padding(0.0f, Spacing.Section, 0.0f, 0.0f)
                            [
                                TGControllerHelpDialogPrivate::MakeSection(
                                    TEXT("Hard Command Switches"),
                                    TEXT(
                                        "When a command changes discontinuously at a known elapsed "
                                        "time, return that next switch time. Smooth controllers may "
                                        "return positive infinity."),
                                    TEXT(
                                        "double NextDiscontinuityElapsedTime(double currentTime) const;"))
                            ]
                        ]
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, Spacing.Regular, 0.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(TEXT(
                            "Use names from the input views when fixed array indices "
                            "would be fragile. The complete field reference and example "
                            "controller are provided in the PHAROS documentation.")))
                        .Font(TGUiTheme::GetSlateFont(ETGUiTextStyle::Caption))
                        .ColorAndOpacity(Palette.TextMuted)
                        .AutoWrapText(true)
                    ]

                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .HAlign(HAlign_Right)
                    .Padding(0.0f, Spacing.Regular, 0.0f, 0.0f)
                    [
                        SNew(SBox)
                        .WidthOverride(112.0f)
                        .HeightOverride(38.0f)
                        [
                            SNew(SButton)
                            .ButtonStyle(&TGUiTheme::GetButtonStyle(
                                ETGUiButtonStyle::Secondary))
                            .HAlign(HAlign_Center)
                            .VAlign(VAlign_Center)
                            .OnClicked(FOnClicked::CreateUObject(
                                this,
                                &UTGControllerHelpDialogWidget::
                                    HandleCloseClicked))
                            [
                                SNew(STextBlock)
                                .Text(FText::FromString(TEXT("Close")))
                                .Font(TGUiTheme::GetSlateFont(
                                    ETGUiTextStyle::BodyStrong))
                                .ColorAndOpacity(Palette.TextPrimary)
                            ]
                        ]
                    ]
                ]
            ]
        ];

    return RootSlateWidget.ToSharedRef();
}

void UTGControllerHelpDialogWidget::ReleaseSlateResources(
    const bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    RootSlateWidget.Reset();
}

void UTGControllerHelpDialogWidget::CloseDialog()
{
    RemoveFromParent();
}

FReply UTGControllerHelpDialogWidget::HandleCloseClicked()
{
    CloseDialog();
    return FReply::Handled();
}
