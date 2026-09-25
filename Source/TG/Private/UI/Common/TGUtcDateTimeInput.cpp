// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/Common/TGUtcDateTimeInput.h"

#include "InputCoreTypes.h"
#include "UI/TGHudFormattingLibrary.h"
#include "UI/Theme/TGUiTheme.h"

namespace TGUtcDateTimeInputPrivate
{
    constexpr TCHAR EmptyMask[] = TEXT("0000-00-00T00:00:00.000Z");
    constexpr int32 MaskLength = 24;
    constexpr int32 DigitCount = 17;
    constexpr int32 DigitOffsets[DigitCount] = {
        0, 1, 2, 3,
        5, 6,
        8, 9,
        11, 12,
        14, 15,
        17, 18,
        20, 21, 22
    };
}

void STGUtcDateTimeInput::Construct(const FArguments& InArgs)
{
    ExternalTextChanged = InArgs._OnTextChanged;
    ExternalTextCommitted = InArgs._OnTextCommitted;
    ExternalBeginTextEdit = InArgs._OnBeginTextEdit;
    ExternalKeyCharHandler = InArgs._OnKeyCharHandler;
    ExternalKeyDownHandler = InArgs._OnKeyDownHandler;

    FArguments MaskedArgs = InArgs;
    const FString InitialText = InArgs._Text.Get().ToString();
    MaskedArgs
        .Text(InitialText.IsEmpty()
            ? FText::GetEmpty()
            : FText::FromString(NormalizeUtcText(InitialText)))
        .IsCaretMovedWhenGainFocus(false)
        .SelectAllTextWhenFocused(false)
        .SelectAllTextOnCommit(false)
        .OnBeginTextEdit(
            this,
            &STGUtcDateTimeInput::HandleInternalBeginTextEdit)
        .OnTextChanged(
            this,
            &STGUtcDateTimeInput::HandleInternalTextChanged)
        .OnTextCommitted(
            this,
            &STGUtcDateTimeInput::HandleInternalTextCommitted)
        .OnKeyCharHandler(
            this,
            &STGUtcDateTimeInput::HandleInternalKeyChar)
        .OnKeyDownHandler(
            this,
            &STGUtcDateTimeInput::HandleInternalKeyDown);

    SEditableTextBox::Construct(MaskedArgs);
}

FText STGUtcDateTimeInput::GetEmptyUtcMask()
{
    return FText::FromString(TGUtcDateTimeInputPrivate::EmptyMask);
}

void STGUtcDateTimeInput::SetUtcText(
    const FText& InText,
    const bool bNotifyTextChanged)
{
    RollingEntryHistory.Reset();
    const FString InputString = InText.ToString();
    ApplyMaskedText(
        InputString.IsEmpty() ? FString() : NormalizeUtcText(InputString),
        InputString.IsEmpty()
            ? 0
            : TGUtcDateTimeInputPrivate::MaskLength,
        bNotifyTextChanged);
}

FString STGUtcDateTimeInput::NormalizeUtcText(const FString& InText)
{
    return FormatDigits(ExtractDigits(InText));
}

FString STGUtcDateTimeInput::ExtractDigits(const FString& InText)
{
    FString Digits;
    Digits.Reserve(InText.Len());
    for (const TCHAR Character : InText)
    {
        if (FChar::IsDigit(Character))
        {
            Digits.AppendChar(Character);
        }
    }
    if (Digits.Len() > TGUtcDateTimeInputPrivate::DigitCount)
    {
        Digits = Digits.Right(TGUtcDateTimeInputPrivate::DigitCount);
    }
    return Digits;
}

FString STGUtcDateTimeInput::FormatDigits(const FString& Digits)
{
    FString PaddedDigits = Digits;
    if (PaddedDigits.Len() < TGUtcDateTimeInputPrivate::DigitCount)
    {
        PaddedDigits = FString::ChrN(
            TGUtcDateTimeInputPrivate::DigitCount - PaddedDigits.Len(),
            TEXT('0')) + PaddedDigits;
    }

    FString Result(TGUtcDateTimeInputPrivate::EmptyMask);
    for (int32 Index = 0;
         Index < TGUtcDateTimeInputPrivate::DigitCount;
         ++Index)
    {
        Result[TGUtcDateTimeInputPrivate::DigitOffsets[Index]] =
            PaddedDigits[Index];
    }
    return Result;
}

int32 STGUtcDateTimeInput::FindDigitAtOrAfter(const int32 Offset)
{
    for (const int32 DigitOffset : TGUtcDateTimeInputPrivate::DigitOffsets)
    {
        if (DigitOffset >= Offset)
        {
            return DigitOffset;
        }
    }
    return INDEX_NONE;
}

int32 STGUtcDateTimeInput::FindPreviousDigit(const int32 Offset)
{
    for (int32 Index = TGUtcDateTimeInputPrivate::DigitCount - 1;
         Index >= 0;
         --Index)
    {
        if (TGUtcDateTimeInputPrivate::DigitOffsets[Index] < Offset)
        {
            return TGUtcDateTimeInputPrivate::DigitOffsets[Index];
        }
    }
    return INDEX_NONE;
}

int32 STGUtcDateTimeInput::FindNextCursorOffset(const int32 DigitOffset)
{
    const int32 NextDigit = FindDigitAtOrAfter(DigitOffset + 1);
    return NextDigit == INDEX_NONE
        ? TGUtcDateTimeInputPrivate::MaskLength
        : NextDigit;
}

void STGUtcDateTimeInput::ApplyMaskedText(
    const FString& MaskedText,
    const int32 CursorOffset,
    const bool bNotifyTextChanged)
{
    const FText NewText = FText::FromString(MaskedText);
    bApplyingMaskedText = true;
    SEditableTextBox::SetText(NewText);
    bApplyingMaskedText = false;

    if (EditableText.IsValid())
    {
        EditableText->ClearSelection();
        EditableText->GoTo(FTextLocation(
            0,
            FMath::Clamp(
                CursorOffset,
                0,
                TGUtcDateTimeInputPrivate::MaskLength)));
    }
    if (bNotifyTextChanged)
    {
        ExternalTextChanged.ExecuteIfBound(NewText);
    }
}

void STGUtcDateTimeInput::HandleInternalTextChanged(const FText& InText)
{
    if (bApplyingMaskedText)
    {
        return;
    }

    RollingEntryHistory.Reset();
    if (InText.IsEmpty())
    {
        ExternalTextChanged.ExecuteIfBound(InText);
        return;
    }
    const FString Normalized = NormalizeUtcText(InText.ToString());
    if (Normalized != InText.ToString())
    {
        ApplyMaskedText(
            Normalized,
            TGUtcDateTimeInputPrivate::MaskLength,
            true);
        return;
    }
    ExternalTextChanged.ExecuteIfBound(InText);
}

void STGUtcDateTimeInput::HandleInternalBeginTextEdit(const FText& InText)
{
    FText EditableValue = InText;
    if (InText.IsEmpty())
    {
        EditableValue = GetEmptyUtcMask();
        ApplyMaskedText(
            EditableValue.ToString(),
            TGUtcDateTimeInputPrivate::MaskLength,
            true);
    }
    ExternalBeginTextEdit.ExecuteIfBound(EditableValue);
}

void STGUtcDateTimeInput::HandleInternalTextCommitted(
    const FText& InText,
    const ETextCommit::Type CommitMethod)
{
    const FText Normalized = InText.IsEmpty()
        ? FText::GetEmpty()
        : FText::FromString(NormalizeUtcText(InText.ToString()));
    ExternalTextCommitted.ExecuteIfBound(Normalized, CommitMethod);
}

FReply STGUtcDateTimeInput::HandleInternalKeyChar(
    const FGeometry& Geometry,
    const FCharacterEvent& CharacterEvent)
{
    if (ExternalKeyCharHandler.IsBound())
    {
        const FReply ExternalReply =
            ExternalKeyCharHandler.Execute(Geometry, CharacterEvent);
        if (ExternalReply.IsEventHandled())
        {
            return ExternalReply;
        }
    }

    const TCHAR Character = CharacterEvent.GetCharacter();
    if (!FChar::IsDigit(Character))
    {
        return Character >= TEXT(' ')
            ? FReply::Handled()
            : FReply::Unhandled();
    }

    FString MaskedText = NormalizeUtcText(GetText().ToString());
    const FTextSelection Selection = EditableText->GetSelection();
    const int32 SelectionStart = Selection.GetBeginning().GetOffset();
    const int32 SelectionEnd = Selection.GetEnd().GetOffset();
    const bool bHasSelection = SelectionStart != SelectionEnd;

    int32 TargetDigit = INDEX_NONE;
    if (bHasSelection)
    {
        for (const int32 DigitOffset : TGUtcDateTimeInputPrivate::DigitOffsets)
        {
            if (DigitOffset >= SelectionStart && DigitOffset < SelectionEnd)
            {
                if (TargetDigit == INDEX_NONE)
                {
                    TargetDigit = DigitOffset;
                }
                MaskedText[DigitOffset] = TEXT('0');
            }
        }
        if (TargetDigit == INDEX_NONE)
        {
            TargetDigit = FindDigitAtOrAfter(SelectionStart);
        }
        RollingEntryHistory.Reset();
    }
    else
    {
        const int32 CursorOffset = Selection.LocationB.GetOffset();
        TargetDigit = FindDigitAtOrAfter(CursorOffset);
        if (TargetDigit == INDEX_NONE)
        {
            RollingEntryHistory.Add(MaskedText);
            FString Digits = ExtractDigits(MaskedText);
            Digits.RemoveAt(0);
            Digits.AppendChar(Character);
            ApplyMaskedText(
                FormatDigits(Digits),
                TGUtcDateTimeInputPrivate::MaskLength,
                true);
            return FReply::Handled();
        }
        RollingEntryHistory.Reset();
    }

    if (TargetDigit != INDEX_NONE)
    {
        MaskedText[TargetDigit] = Character;
        ApplyMaskedText(
            MaskedText,
            FindNextCursorOffset(TargetDigit),
            true);
    }
    return FReply::Handled();
}

FReply STGUtcDateTimeInput::HandleInternalKeyDown(
    const FGeometry& Geometry,
    const FKeyEvent& KeyEvent)
{
    if (ExternalKeyDownHandler.IsBound())
    {
        const FReply ExternalReply =
            ExternalKeyDownHandler.Execute(Geometry, KeyEvent);
        if (ExternalReply.IsEventHandled())
        {
            return ExternalReply;
        }
    }

    const bool bBackspace = KeyEvent.GetKey() == EKeys::BackSpace;
    const bool bDelete = KeyEvent.GetKey() == EKeys::Delete;
    if (!bBackspace && !bDelete)
    {
        return FReply::Unhandled();
    }

    FString MaskedText = NormalizeUtcText(GetText().ToString());
    const FTextSelection Selection = EditableText->GetSelection();
    const int32 SelectionStart = Selection.GetBeginning().GetOffset();
    const int32 SelectionEnd = Selection.GetEnd().GetOffset();
    if (SelectionStart != SelectionEnd)
    {
        int32 FirstDigit = INDEX_NONE;
        for (const int32 DigitOffset : TGUtcDateTimeInputPrivate::DigitOffsets)
        {
            if (DigitOffset >= SelectionStart && DigitOffset < SelectionEnd)
            {
                FirstDigit = FirstDigit == INDEX_NONE
                    ? DigitOffset
                    : FirstDigit;
                MaskedText[DigitOffset] = TEXT('0');
            }
        }
        RollingEntryHistory.Reset();
        ApplyMaskedText(
            MaskedText,
            FirstDigit == INDEX_NONE ? SelectionStart : FirstDigit,
            true);
        return FReply::Handled();
    }

    const int32 CursorOffset = Selection.LocationB.GetOffset();
    if (bBackspace &&
        CursorOffset >= TGUtcDateTimeInputPrivate::MaskLength &&
        !RollingEntryHistory.IsEmpty())
    {
        const FString PreviousText = RollingEntryHistory.Pop();
        ApplyMaskedText(
            PreviousText,
            TGUtcDateTimeInputPrivate::MaskLength,
            true);
        return FReply::Handled();
    }

    const int32 TargetDigit = bBackspace
        ? FindPreviousDigit(CursorOffset)
        : FindDigitAtOrAfter(CursorOffset);
    if (TargetDigit != INDEX_NONE)
    {
        RollingEntryHistory.Reset();
        MaskedText[TargetDigit] = TEXT('0');
        ApplyMaskedText(MaskedText, TargetDigit, true);
    }
    return FReply::Handled();
}

UTGUtcDateTimeInput::UTGUtcDateTimeInput(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    WidgetStyle = TGUiTheme::MakeEditableTextBoxStyle();
    SetIsCaretMovedWhenGainFocus(false);
    SetSelectAllTextWhenFocused(false);
    SetSelectAllTextOnCommit(false);
}

void UTGUtcDateTimeInput::HandleOnTextChanged(const FText& InText)
{
    FDateTime ParsedUtc;
    FText ParseError;
    const FText PreviousText = GetText();
    if (UTGHudFormattingLibrary::ParseUtcDateTime(
            PreviousText,
            ParsedUtc,
            ParseError))
    {
        LastValidUtcText = PreviousText;
    }

    Super::HandleOnTextChanged(InText);
}

void UTGUtcDateTimeInput::HandleOnTextCommitted(
    const FText& InText,
    const ETextCommit::Type CommitMethod)
{
    if (CommitMethod != ETextCommit::OnCleared)
    {
        FDateTime ParsedUtc;
        FText ParseError;
        if (!UTGHudFormattingLibrary::ParseUtcDateTime(
                InText,
                ParsedUtc,
                ParseError))
        {
            SetText(LastValidUtcText);
            OnUtcValidationFailed.Broadcast(ParseError);
            return;
        }

        LastValidUtcText = InText;
    }

    Super::HandleOnTextCommitted(InText, CommitMethod);
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
TSharedRef<SWidget> UTGUtcDateTimeInput::RebuildWidget()
{
    MyEditableTextBlock = SNew(STGUtcDateTimeInput)
        .Style(&WidgetStyle)
        .IsReadOnly(IsReadOnly)
        .IsPassword(IsPassword)
        .MinDesiredWidth(MinimumDesiredWidth)
        .IsCaretMovedWhenGainFocus(false)
        .SelectAllTextWhenFocused(false)
        .RevertTextOnEscape(RevertTextOnEscape)
        .ClearKeyboardFocusOnCommit(ClearKeyboardFocusOnCommit)
        .SelectAllTextOnCommit(false)
        .AllowContextMenu(AllowContextMenu)
        .OnTextChanged(BIND_UOBJECT_DELEGATE(
            FOnTextChanged,
            HandleOnTextChanged))
        .OnTextCommitted(BIND_UOBJECT_DELEGATE(
            FOnTextCommitted,
            HandleOnTextCommitted))
        .VirtualKeyboardType(
            EVirtualKeyboardType::AsKeyboardType(KeyboardType.GetValue()))
        .VirtualKeyboardOptions(VirtualKeyboardOptions)
        .VirtualKeyboardTrigger(VirtualKeyboardTrigger)
        .VirtualKeyboardDismissAction(VirtualKeyboardDismissAction)
        .Justification(Justification)
        .OverflowPolicy(OverflowPolicy);

    return MyEditableTextBlock.ToSharedRef();
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
