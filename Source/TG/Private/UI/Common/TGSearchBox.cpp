// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGSearchBox.h"

#include "UI/Theme/TGUiTheme.h"
#include "Widgets/Input/SSearchBox.h"

UTGSearchBox::UTGSearchBox(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    WidgetStyle = TGUiTheme::GetSearchBoxStyle().TextBoxStyle;
}

TSharedRef<SWidget> UTGSearchBox::RebuildWidget()
{
    MyEditableTextBlock = SNew(SSearchBox)
        .Style(&TGUiTheme::GetSearchBoxStyle())
        .InitialText(GetText())
        .HintText(GetHintText())
        .MinDesiredWidth(GetMinimumDesiredWidth())
        .SelectAllTextWhenFocused(GetSelectAllTextWhenFocused())
        .DelayChangeNotificationsWhileTyping(false)
        .OnTextChanged(BIND_UOBJECT_DELEGATE(
            FOnTextChanged,
            HandleOnTextChanged))
        .OnTextCommitted(BIND_UOBJECT_DELEGATE(
            FOnTextCommitted,
            HandleOnTextCommitted));

    return MyEditableTextBlock.ToSharedRef();
}
