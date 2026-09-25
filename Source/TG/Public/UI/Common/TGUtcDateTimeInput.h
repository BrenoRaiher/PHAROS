// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Components/EditableTextBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "TGUtcDateTimeInput.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FTGUtcInputValidationFailed,
    const FText&,
    ValidationMessage);

/**
 * Single-line UTC editor that permanently enforces
 * YYYY-MM-DDTHH:MM:SS.sssZ while allowing rolling or in-place digit entry.
 */
class TG_API STGUtcDateTimeInput : public SEditableTextBox
{
public:
    using FArguments = SEditableTextBox::FArguments;

    void Construct(const FArguments& InArgs);

    static FText GetEmptyUtcMask();
    void SetUtcText(const FText& InText, bool bNotifyTextChanged = true);

private:
    FOnTextChanged ExternalTextChanged;
    FOnTextCommitted ExternalTextCommitted;
    FOnBeginTextEdit ExternalBeginTextEdit;
    FOnKeyChar ExternalKeyCharHandler;
    FOnKeyDown ExternalKeyDownHandler;
    TArray<FString> RollingEntryHistory;
    bool bApplyingMaskedText = false;

    static FString NormalizeUtcText(const FString& InText);
    static FString ExtractDigits(const FString& InText);
    static FString FormatDigits(const FString& Digits);
    static int32 FindDigitAtOrAfter(int32 Offset);
    static int32 FindPreviousDigit(int32 Offset);
    static int32 FindNextCursorOffset(int32 DigitOffset);

    void ApplyMaskedText(
        const FString& MaskedText,
        int32 CursorOffset,
        bool bNotifyTextChanged);
    void HandleInternalTextChanged(const FText& InText);
    void HandleInternalBeginTextEdit(const FText& InText);
    void HandleInternalTextCommitted(
        const FText& InText,
        ETextCommit::Type CommitMethod);
    FReply HandleInternalKeyChar(
        const FGeometry& Geometry,
        const FCharacterEvent& CharacterEvent);
    FReply HandleInternalKeyDown(
        const FGeometry& Geometry,
        const FKeyEvent& KeyEvent);
};

/** Designer-usable UMG wrapper around STGUtcDateTimeInput. */
UCLASS(meta = (DisplayName = "PHAROS UTC Date-Time Input"))
class TG_API UTGUtcDateTimeInput : public UEditableTextBox
{
    GENERATED_BODY()

public:
    explicit UTGUtcDateTimeInput(
        const FObjectInitializer& ObjectInitializer);

    UPROPERTY(BlueprintAssignable, Category = "PHAROS|UTC")
    FTGUtcInputValidationFailed OnUtcValidationFailed;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void HandleOnTextChanged(const FText& Text) override;
    virtual void HandleOnTextCommitted(
        const FText& Text,
        ETextCommit::Type CommitMethod) override;

private:
    FText LastValidUtcText;
};
