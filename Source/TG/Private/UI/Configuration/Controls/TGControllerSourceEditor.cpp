// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Configuration/Controls/TGControllerSourceEditor.h"

#include "UI/Theme/TGUiTheme.h"

UTGControllerSourceEditor::UTGControllerSourceEditor(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    ApplyControllerEditorStyle();
}

void UTGControllerSourceEditor::SynchronizeProperties()
{
    ApplyControllerEditorStyle();
    Super::SynchronizeProperties();
}

void UTGControllerSourceEditor::ApplyControllerEditorStyle()
{
    const FTGUiPalette& Palette = TGUiTheme::GetPalette();
    FEditableTextBoxStyle Style = TGUiTheme::MakeEditableTextBoxStyle();
    const FSlateBrush Background = TGUiTheme::MakeRoundedBrush(
        Palette.Input,
        4.0f,
        Palette.Border,
        1.0f);
    const FSlateBrush HoveredBackground = TGUiTheme::MakeRoundedBrush(
        Palette.Input,
        4.0f,
        Palette.Accent,
        1.0f);
    const FSlateBrush FocusedBackground = TGUiTheme::MakeRoundedBrush(
        Palette.Input,
        4.0f,
        Palette.Focus,
        2.0f);

    Style.SetBackgroundImageNormal(Background)
        .SetBackgroundImageHovered(HoveredBackground)
        .SetBackgroundImageFocused(FocusedBackground)
        .SetBackgroundImageReadOnly(Background)
        .SetScrollBarStyle(TGUiTheme::MakeScrollBarStyle())
        .SetPadding(FMargin(10.0f, 8.0f));
    Style.TextStyle.SetFont(
        TGUiTheme::GetSlateFont(ETGUiTextStyle::Numeric));
    Style.TextStyle.SetColorAndOpacity(
        FSlateColor(Palette.TextPrimary));
    Style.SetForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetFocusedForegroundColor(FSlateColor(Palette.TextPrimary))
        .SetReadOnlyForegroundColor(FSlateColor(Palette.TextSecondary));

    WidgetStyle = MoveTemp(Style);
}
