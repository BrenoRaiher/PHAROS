// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#include "UI/TGHudFormattingLibrary.h"
#include "Internationalization/Internationalization.h"
#include "Containers/UnrealString.h"
#include "Misc/DateTime.h"

namespace TGHudFormatting
{
    constexpr int32 ExpectedUtcStringLength = 24;

    static const TCHAR* const RequiredUtcFormat =
        TEXT("YYYY-MM-DDTHH:MM:SS.mmmZ");

    static const TCHAR* const RequiredUtcExample =
        TEXT("2026-07-27T18:30:00.123Z");

    bool ContainsOnlyDigits(
        const FString& Value,
        const int32 StartIndex,
        const int32 CharacterCount)
    {
        for (int32 Index = StartIndex;
             Index < StartIndex + CharacterCount;
             ++Index)
        {
            if (!FChar::IsDigit(Value[Index]))
            {
                return false;
            }
        }

        return true;
    }

    bool HasCorrectFixedStructure(const FString& Value)
    {
        if (Value.Len() != ExpectedUtcStringLength)
        {
            return false;
        }

        // YYYY-MM-DDTHH:MM:SS.mmmZ
        if (Value[4]  != TEXT('-') ||
            Value[7]  != TEXT('-') ||
            Value[10] != TEXT('T') ||
            Value[13] != TEXT(':') ||
            Value[16] != TEXT(':') ||
            Value[19] != TEXT('.') ||
            Value[23] != TEXT('Z'))
        {
            return false;
        }

        return
            ContainsOnlyDigits(Value, 0, 4)  && // Year
            ContainsOnlyDigits(Value, 5, 2)  && // Month
            ContainsOnlyDigits(Value, 8, 2)  && // Day
            ContainsOnlyDigits(Value, 11, 2) && // Hour
            ContainsOnlyDigits(Value, 14, 2) && // Minute
            ContainsOnlyDigits(Value, 17, 2) && // Second
            ContainsOnlyDigits(Value, 20, 3);   // Millisecond
    }

    FText MakeFormatError()
    {
        return FText::FromString(
            FString::Printf(
                TEXT("Use the exact UTC format %s, for example %s."),
                RequiredUtcFormat,
                RequiredUtcExample));
    }
}

FText UTGHudFormattingLibrary::FormatUtcDateTime(
    const FDateTime& UtcDateTime)
{
    if (UtcDateTime == FDateTime::MinValue())
    {
        return FText::GetEmpty();
    }

    const FString FormattedDateTime = FString::Printf(
        TEXT("%04d-%02d-%02dT%02d:%02d:%02d.%03dZ"),
        UtcDateTime.GetYear(),
        UtcDateTime.GetMonth(),
        UtcDateTime.GetDay(),
        UtcDateTime.GetHour(),
        UtcDateTime.GetMinute(),
        UtcDateTime.GetSecond(),
        UtcDateTime.GetMillisecond());

    return FText::FromString(FormattedDateTime);
}

bool UTGHudFormattingLibrary::ParseUtcDateTime(
    const FText& InputText,
    FDateTime& OutUtcDateTime,
    FText& OutError)
{
    FString InputString = InputText.ToString();
    InputString.TrimStartAndEndInline();

    OutUtcDateTime = FDateTime::MinValue();
    OutError = FText::GetEmpty();

    if (!TGHudFormatting::HasCorrectFixedStructure(InputString))
    {
        OutError = TGHudFormatting::MakeFormatError();
        return false;
    }

    const int32 Year =
        FCString::Atoi(*InputString.Mid(0, 4));

    const int32 Month =
        FCString::Atoi(*InputString.Mid(5, 2));

    const int32 Day =
        FCString::Atoi(*InputString.Mid(8, 2));

    const int32 Hour =
        FCString::Atoi(*InputString.Mid(11, 2));

    const int32 Minute =
        FCString::Atoi(*InputString.Mid(14, 2));

    const int32 Second =
        FCString::Atoi(*InputString.Mid(17, 2));

    const int32 Millisecond =
        FCString::Atoi(*InputString.Mid(20, 3));

    if (!FDateTime::Validate(
            Year,
            Month,
            Day,
            Hour,
            Minute,
            Second,
            Millisecond))
    {
        OutError = FText::FromString(
            FString::Printf(
                TEXT("The UTC value has an invalid calendar date or time. "
                     "Example: %s."),
                TGHudFormatting::RequiredUtcExample));

        return false;
    }

    OutUtcDateTime = FDateTime(
        Year,
        Month,
        Day,
        Hour,
        Minute,
        Second,
        Millisecond);

    return true;
}

FText UTGHudFormattingLibrary::FormatDoubleForHud(
    const double Value,
    const int32 MaximumFractionDigits)
{
    FNumberFormattingOptions Options;
    Options.SetUseGrouping(false);
    Options.SetMinimumFractionalDigits(0);
    Options.SetMaximumFractionalDigits(
        FMath::Clamp(MaximumFractionDigits, 0, 15));

    return FText::AsNumber(
        Value,
        &Options,
        FInternationalization::Get().GetInvariantCulture());
}

FText UTGHudFormattingLibrary::FormatQuaternionNormForHud(
    const double Norm,
    const int32 FractionDigits,
    const double UnitTolerance)
{
    const int32 ClampedFractionDigits =
        FMath::Clamp(FractionDigits, 0, 15);
    const double ClampedTolerance =
        FMath::Max(0.0, UnitTolerance);
    const double DisplayNorm =
        FMath::IsNearlyEqual(Norm, 1.0, ClampedTolerance)
            ? 1.0
            : Norm;

    FNumberFormattingOptions Options;
    Options.SetUseGrouping(false);
    Options.SetMinimumFractionalDigits(ClampedFractionDigits);
    Options.SetMaximumFractionalDigits(ClampedFractionDigits);

    return FText::AsNumber(
        DisplayNorm,
        &Options,
        FInternationalization::Get().GetInvariantCulture());
}

bool UTGHudFormattingLibrary::ParseHudDouble(
    const FText& InputText,
    double& OutValue,
    FText& OutError)
{
    FString InputString = InputText.ToString();
    InputString.TrimStartAndEndInline();

    OutValue = 0.0;
    OutError = FText::GetEmpty();

    if (InputString.IsEmpty())
    {
        OutError = FText::FromString(
            TEXT("Enter a numeric value."));

        return false;
    }

    double ParsedValue = 0.0;

    if (!LexTryParseString(ParsedValue, *InputString))
    {
        OutError = FText::FromString(
            TEXT("Enter a valid decimal number using a period."));

        return false;
    }

    if (!FMath::IsFinite(ParsedValue))
    {
        OutError = FText::FromString(
            TEXT("The numeric value must be finite."));

        return false;
    }

    OutValue = ParsedValue;
    return true;
}

FText UTGHudFormattingLibrary::FormatIntegerForHud(
    const int32 Value)
{
    FNumberFormattingOptions Options;
    Options.SetUseGrouping(false);

    return FText::AsNumber(
        Value,
        &Options,
        FInternationalization::Get().GetInvariantCulture());
}

bool UTGHudFormattingLibrary::ParseHudInteger(
    const FText& InputText,
    int32& OutValue,
    FText& OutError)
{
    FString InputString = InputText.ToString();
    InputString.TrimStartAndEndInline();

    OutValue = 0;
    OutError = FText::GetEmpty();

    if (InputString.IsEmpty())
    {
        OutError = FText::FromString(TEXT("Enter an integer value."));
        return false;
    }

    int64 ParsedValue = 0;

    if (!LexTryParseString(ParsedValue, *InputString))
    {
        OutError = FText::FromString(
            TEXT("Enter a valid whole number."));

        return false;
    }

    if (ParsedValue < MIN_int32 || ParsedValue > MAX_int32)
    {
        OutError = FText::FromString(
            TEXT("The integer is outside the supported range."));

        return false;
    }

    OutValue = static_cast<int32>(ParsedValue);
    return true;
}

