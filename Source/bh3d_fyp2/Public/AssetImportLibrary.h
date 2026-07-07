#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MovementSequence.h"
#include "FirePattern.h"
#include "AttackSequence.h"
#include "AssetImportLibrary.generated.h"

/**
 * AssetImportLibrary — Text-based import for data-driven sequences.
 *
 * This library provides utilities to parse plain-text formats into structured
 * asset data. Designed for:
 *   - ChatGPT/Claude prompt generation
 *   - Human authoring (copy/paste from spreadsheets, text files)
 *   - Version control friendly (plain text diffs)
 *
 * Usage pattern:
 *   1. Paste text into an asset's ImportText field
 *   2. Call Rebuild() on the asset
 *   3. Rebuild() calls the appropriate parser
 *   4. Asset populates with parsed data
 *
 * Movement Format:
 *   origin X Y Z AttackDuration  — Move to SpawnOrigin + offset
 *   move X Y Z AttackDuration    — Move to CurrentPosition + offset
 *   absolute X Y Z AttackDuration — Move to world coordinate
 *   wait AttackDuration           — Stay in place and attack
 *   setorigin                     — Set CurrentOrigin to current position
 *
 * Example:
 *   origin 0 0 0 2
 *   origin 400 0 0 2
 *   setorigin
 *   origin 0 0 0 1
 *   move -300 0 0 1
 */
UCLASS()
class BH3D_FYP2_API UAssetImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// ────────────────────────────────────────────────────────────────────
	// Movement Sequence Importer
	// ────────────────────────────────────────────────────────────────────
	/**
	 * Parse movement text into an array of FMovementPoint.
	 *
	 * Format (keywords):
	 *   origin X Y Z AttackDuration
	 *   move X Y Z AttackDuration
	 *   absolute X Y Z AttackDuration
	 *   wait AttackDuration
	 *   setorigin
	 *
	 * Example:
	 *   origin 0 0 0 2
	 *   origin 400 0 0 2
	 *   setorigin
	 *   move -300 0 0 1
	 *
	 * Lines starting with # are comments.
	 * Empty lines are ignored.
	 *
	 * @param InText  Raw text to parse
	 * @param OutPoints  Populated with parsed movement points
	 * @param OutError  Error message if parsing fails
	 * @return true if parsing succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetImport")
	static bool ParseMovementText(
		const FString& InText,
		TArray<FMovementPoint>& OutPoints,
		FString& OutError);

	// ────────────────────────────────────────────────────────────────────
	// Custom Directions Importer (for FirePattern)
	// ────────────────────────────────────────────────────────────────────
	/**
	 * Parse custom direction text into an array of normalized FVector.
	 *
	 * Format (Cartesian): X Y Z
	 *
	 * Example:
	 *   1 0 0
	 *   0 1 0
	 *   0 0 1
	 *   -1 0 0
	 *
	 * All vectors are automatically normalized.
	 * Lines starting with # are comments.
	 * Empty lines are ignored.
	 *
	 * @param InText  Raw text to parse
	 * @param OutDirections  Populated with parsed normalized vectors
	 * @param OutError  Error message if parsing fails
	 * @return true if parsing succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetImport")
	static bool ParseDirectionsText(
		const FString& InText,
		TArray<FVector>& OutDirections,
		FString& OutError);

	// ────────────────────────────────────────────────────────────────────
	// Attack Sequence Importer
	// ────────────────────────────────────────────────────────────────────
	/**
	 * Parse attack sequence text into an array of FAttackPhase.
	 *
	 * Format:
	 *   phase
	 *   pattern <PatternName>
	 *   interval <float>
	 *   shots <int>
	 *   startdelay <float> (optional, default 0)
	 *   loop (optional, default false)
	 *
	 * Example:
	 *   phase
	 *   pattern Spiral
	 *   interval 0.1
	 *   shots 40
	 *
	 *   phase
	 *   pattern Ring
	 *   interval 0.2
	 *   shots 20
	 *   startdelay 0.5
	 *   loop
	 *
	 * Lines starting with # are comments.
	 * Empty lines are ignored.
	 * PatternName must match a FirePattern asset reference name.
	 *
	 * @param InText  Raw text to parse
	 * @param OutPhases  Populated with parsed attack phases
	 * @param OutError  Error message if parsing fails
	 * @return true if parsing succeeded
	 */
	UFUNCTION(BlueprintCallable, Category = "AssetImport")
	static bool ParseAttackSequenceText(
		const FString& InText,
		TArray<FAttackPhase>& OutPhases,
		FString& OutError);

private:
	// Helper: tokenize a line (whitespace-separated)
	static TArray<FString> TokenizeLine(const FString& Line);

	// Helper: check if string is a valid float
	static bool StringToFloat(const FString& Str, float& OutValue, FString& OutError);

	// Helper: check if string is a valid int
	static bool StringToInt(const FString& Str, int32& OutValue, FString& OutError);

	// Helper: parse a single movement line (keyword-based)
	static bool ParseMovementLine(
		const FString& Line,
		int32 LineNumber,
		FMovementPoint& OutPoint,
		FString& OutError);

	// Helper: parse a single direction line (X Y Z)
	static bool ParseDirectionLine(
		const FString& Line,
		int32 LineNumber,
		FVector& OutDirection,
		FString& OutError);
};
