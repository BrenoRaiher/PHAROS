// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGThemedComboBoxString.h"

#include "UI/Theme/TGUiTheme.h"

UTGThemedComboBoxString::UTGThemedComboBoxString(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetWidgetStyle(TGUiTheme::MakeComboBoxStyle());
    SetItemStyle(TGUiTheme::MakeTableRowStyle());
    InitScrollBarStyle(TGUiTheme::MakeScrollBarStyle());
    SetContentPadding(FMargin(10.0f, 7.0f));
    SetMaxListHeight(420.0f);
    InitFont(TGUiTheme::GetSlateFont(ETGUiTextStyle::FieldLabel));
    InitForegroundColor(FSlateColor(FLinearColor(
        0.775822f, 0.814847f, 0.854993f, 1.0f)));
}
