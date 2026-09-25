// Copyright (c) 2026 Breno Raiher.
// SPDX-License-Identifier: MIT

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "TGHudFormattingLibrary.generated.h"

/**
 * Reusable formatting and parsing functions used by the TG HUD.
 *
 * Keep presentation-specific conversions here instead of rebuilding
 * equivalent formatting graphs in every Widget Blueprint.
 */
UCLASS()
class TG_API UTGHudFormattingLibrary final
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    /**
     * Formats an Unreal DateTime as a fixed-width UTC timestamp:
     *
     * YYYY-MM-DDTHH:MM:SS.mmmZ
     *
     * A default/minimum DateTime is displayed as an empty field.
     */
    UFUNCTION(
        BlueprintPure,
        Category = "PHAROS|HUD|Date Time",
        meta = (DisplayName = "Format UTC Date Time"))
    static FText FormatUtcDateTime(const FDateTime& UtcDateTime);

    /**
     * Parses the exact HUD UTC representation:
     *
     * YYYY-MM-DDTHH:MM:SS.mmmZ
     *
     * The Boolean result appears as Success and Failure execution pins
     * in Blueprint.
     */
    UFUNCTION(
        BlueprintCallable,
        Category = "PHAROS|HUD|Date Time",
        meta = (
            DisplayName = "Parse UTC Date Time",
            ExpandBoolAsExecs = "ReturnValue"))
    static bool ParseUtcDateTime(
        const FText& InputText,
        FDateTime& OutUtcDateTime,
        FText& OutError);
		
	/**
	 * Formats a double for an editable HUD field without digit grouping.
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "PHAROS|HUD|Numbers",
		meta = (
			DisplayName = "Format Double For HUD",
			AdvancedDisplay = "MaximumFractionDigits"))
	static FText FormatDoubleForHud(
		double Value,
		int32 MaximumFractionDigits = 6);

	/**
	 * Formats a quaternion norm with stable precision for status text.
	 * Floating-point values sufficiently close to unit length are displayed
	 * as exactly one.
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "PHAROS|HUD|Numbers",
		meta = (
			DisplayName = "Format Quaternion Norm For HUD",
			AdvancedDisplay = "FractionDigits,UnitTolerance"))
	static FText FormatQuaternionNormForHud(
		double Norm,
		int32 FractionDigits = 6,
		double UnitTolerance = 1.0e-6);

	/**
	 * Parses a finite decimal number from a HUD text field.
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "PHAROS|HUD|Numbers",
		meta = (
			DisplayName = "Parse HUD Double",
			ExpandBoolAsExecs = "ReturnValue"))
	static bool ParseHudDouble(
		const FText& InputText,
		double& OutValue,
		FText& OutError);
		
	/**
	 * Formats an integer for an editable HUD field without digit grouping.
	 */
	UFUNCTION(
		BlueprintPure,
		Category = "PHAROS|HUD|Numbers",
		meta = (DisplayName = "Format Integer For HUD"))
	static FText FormatIntegerForHud(int32 Value);

	/**
	 * Parses a complete signed 32-bit integer from a HUD field.
	 */
	UFUNCTION(
		BlueprintCallable,
		Category = "PHAROS|HUD|Numbers",
		meta = (
			DisplayName = "Parse HUD Integer",
			ExpandBoolAsExecs = "ReturnValue"))
	static bool ParseHudInteger(
		const FText& InputText,
		int32& OutValue,
		FText& OutError);
};
