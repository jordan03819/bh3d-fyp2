#include "AssetImportLibrary.h"
#include "Containers/StringConv.h"

// ────────────────────────────────────────────────────────────────────────────
// Helper Functions
// ────────────────────────────────────────────────────────────────────────────

TArray<FString> UAssetImportLibrary::TokenizeLine(const FString& Line)
{
	TArray<FString> Tokens;
	FString CurrentToken;

	for (TCHAR Char : Line)
	{
		if (FChar::IsWhitespace(Char))
		{
			if (!CurrentToken.IsEmpty())
			{
				Tokens.Add(CurrentToken);
				CurrentToken.Empty();
			}
		}
		else
		{
			CurrentToken += Char;
		}
	}

	if (!CurrentToken.IsEmpty())
	{
		Tokens.Add(CurrentToken);
	}

	return Tokens;
}

bool UAssetImportLibrary::StringToFloat(const FString& Str, float& OutValue, FString& OutError)
{
	if (!Str.IsNumeric())
	{
		OutError = FString::Printf(TEXT("'%s' is not a valid float"), *Str);
		return false;
	}

	OutValue = FCString::Atof(*Str);
	return true;
}

bool UAssetImportLibrary::StringToInt(const FString& Str, int32& OutValue, FString& OutError)
{
	if (!Str.IsNumeric())
	{
		OutError = FString::Printf(TEXT("'%s' is not a valid integer"), *Str);
		return false;
	}

	OutValue = FCString::Atoi(*Str);
	return true;
}

bool UAssetImportLibrary::ParseMovementLine(
	const FString& Line,
	int32 LineNumber,
	FMovementPoint& OutPoint,
	FString& OutError)
{
	TArray<FString> Tokens = TokenizeLine(Line);

	if (Tokens.Num() == 0)
	{
		OutError = FString::Printf(TEXT("Line %d: Empty line"), LineNumber);
		return false;
	}

	FString Keyword = Tokens[0].ToLower();

	// setorigin command (no parameters)
	if (Keyword == TEXT("setorigin"))
	{
		OutPoint.Command = EMovementCommand::SetOrigin;
		OutPoint.Location = FVector::ZeroVector;
		OutPoint.Reference = EMovementReference::Origin;
		OutPoint.DurationType = EAttackDurationType::Fixed;
		OutPoint.AttackDuration = 0.f;
		OutPoint.bAttackWhileMoving = false;
		return true;
	}

	// skip command (no parameters)
	if (Keyword == TEXT("skip"))
	{
		OutPoint.Command = EMovementCommand::Skip;
		OutPoint.Location = FVector::ZeroVector;
		OutPoint.Reference = EMovementReference::Origin;
		OutPoint.DurationType = EAttackDurationType::Fixed;
		OutPoint.AttackDuration = 0.f;
		OutPoint.bAttackWhileMoving = false;
		return true;
	}

	// wait command (duration or "phase")
	if (Keyword == TEXT("wait"))
	{
		if (Tokens.Num() < 2)
		{
			OutError = FString::Printf(TEXT("Line %d: 'wait' requires duration or 'phase'"), LineNumber);
			return false;
		}

		FString DurationToken = Tokens[1].ToLower();

		if (DurationToken == TEXT("phase"))
		{
			OutPoint.DurationType = EAttackDurationType::UntilPhase;
		}
		else
		{
			float Duration;
			FString TempError;
			if (!StringToFloat(Tokens[1], Duration, TempError))
			{
				OutError = FString::Printf(TEXT("Line %d, wait parameter: %s"), LineNumber, *TempError);
				return false;
			}
			OutPoint.DurationType = EAttackDurationType::Fixed;
			OutPoint.AttackDuration = Duration;
		}

		OutPoint.Command = EMovementCommand::Wait;
		OutPoint.Location = FVector::ZeroVector;
		OutPoint.Reference = EMovementReference::Origin;
		OutPoint.bAttackWhileMoving = false;
		return true;
	}

	// Movement commands: origin, move, absolute
	EMovementReference Reference;

	if (Keyword == TEXT("origin"))
	{
		Reference = EMovementReference::Origin;
	}
	else if (Keyword == TEXT("move"))
	{
		Reference = EMovementReference::Move;
	}
	else if (Keyword == TEXT("absolute"))
	{
		Reference = EMovementReference::Absolute;
	}
	else
	{
		OutError = FString::Printf(
			TEXT("Line %d: Unknown keyword '%s'. Expected 'origin', 'move', 'absolute', 'wait', 'setorigin', or 'skip'"),
			LineNumber, *Keyword);
		return false;
	}

	// Parse X Y Z DurationOrPhase Parallel for movement commands
	if (Tokens.Num() != 6)
	{
		OutError = FString::Printf(
			TEXT("Line %d: '%s' requires 5 values (X Y Z AttackDuration|phase Parallel), got %d"),
			LineNumber, *Keyword, Tokens.Num() - 1);
		return false;
	}

	float X, Y, Z;
	FString TempError;

	if (!StringToFloat(Tokens[1], X, TempError))
	{
		OutError = FString::Printf(TEXT("Line %d, X: %s"), LineNumber, *TempError);
		return false;
	}

	if (!StringToFloat(Tokens[2], Y, TempError))
	{
		OutError = FString::Printf(TEXT("Line %d, Y: %s"), LineNumber, *TempError);
		return false;
	}

	if (!StringToFloat(Tokens[3], Z, TempError))
	{
		OutError = FString::Printf(TEXT("Line %d, Z: %s"), LineNumber, *TempError);
		return false;
	}

	// Parse duration or "phase"
	FString DurationToken = Tokens[4].ToLower();

	if (DurationToken == TEXT("phase"))
	{
		OutPoint.DurationType = EAttackDurationType::UntilPhase;
		OutPoint.AttackDuration = 0.f;  // Not used for phase wait
	}
	else
	{
		float Duration;
		if (!StringToFloat(Tokens[4], Duration, TempError))
		{
			OutError = FString::Printf(TEXT("Line %d, AttackDuration: %s"), LineNumber, *TempError);
			return false;
		}
		OutPoint.DurationType = EAttackDurationType::Fixed;
		OutPoint.AttackDuration = Duration;
	}

	// Parse parallel flag (0 or 1)
	int32 ParallelFlag;
	if (!StringToInt(Tokens[5], ParallelFlag, TempError))
	{
		OutError = FString::Printf(TEXT("Line %d, Parallel flag: must be 0 or 1"), LineNumber);
		return false;
	}

	if (ParallelFlag != 0 && ParallelFlag != 1)
	{
		OutError = FString::Printf(TEXT("Line %d, Parallel flag: must be 0 or 1, got %d"), LineNumber, ParallelFlag);
		return false;
	}

	OutPoint.Command = EMovementCommand::MoveTo;
	OutPoint.Location = FVector(X, Y, Z);
	OutPoint.Reference = Reference;
	OutPoint.bAttackWhileMoving = (ParallelFlag != 0);
	return true;
}

bool UAssetImportLibrary::ParseDirectionLine(
	const FString& Line,
	int32 LineNumber,
	FVector& OutDirection,
	FString& OutError)
{
	TArray<FString> Tokens = TokenizeLine(Line);

	if (Tokens.Num() != 3)
	{
		OutError = FString::Printf(
			TEXT("Line %d: Expected 3 values (X Y Z), got %d"),
			LineNumber, Tokens.Num());
		return false;
	}

	float X, Y, Z;
	FString TempError;

	if (!StringToFloat(Tokens[0], X, TempError))
	{
		OutError = FString::Printf(TEXT("Line %d, X: %s"), LineNumber, *TempError);
		return false;
	}

	if (!StringToFloat(Tokens[1], Y, TempError))
	{
		OutError = FString::Printf(TEXT("Line %d, Y: %s"), LineNumber, *TempError);
		return false;
	}

	if (!StringToFloat(Tokens[2], Z, TempError))
	{
		OutError = FString::Printf(TEXT("Line %d, Z: %s"), LineNumber, *TempError);
		return false;
	}

	OutDirection = FVector(X, Y, Z).GetSafeNormal();
	return true;
}

// ────────────────────────────────────────────────────────────────────────────
// Movement Sequence Parser
// ────────────────────────────────────────────────────────────────────────────

bool UAssetImportLibrary::ParseMovementText(
	const FString& InText,
	TArray<FMovementPoint>& OutPoints,
	FString& OutError)
{
	OutPoints.Empty();
	OutError.Empty();

	TArray<FString> Lines;
	InText.ParseIntoArrayLines(Lines, true);

	int32 LineNumber = 0;

	for (const FString& Line : Lines)
	{
		LineNumber++;

		// Remove leading/trailing whitespace
		FString TrimmedLine = Line.TrimStart().TrimEnd();

		// Skip comments and empty lines
		if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT("#")))
		{
			continue;
		}

		FMovementPoint Point;
		if (!ParseMovementLine(TrimmedLine, LineNumber, Point, OutError))
		{
			return false;
		}

		OutPoints.Add(Point);
	}

	if (OutPoints.Num() == 0)
	{
		OutError = TEXT("No movement points parsed (file empty or only comments)");
		return false;
	}

	return true;
}

// ────────────────────────────────────────────────────────────────────────────
// Custom Directions Parser
// ────────────────────────────────────────────────────────────────────────────

bool UAssetImportLibrary::ParseDirectionsText(
	const FString& InText,
	TArray<FVector>& OutDirections,
	FString& OutError)
{
	OutDirections.Empty();
	OutError.Empty();

	TArray<FString> Lines;
	InText.ParseIntoArrayLines(Lines, true);

	int32 LineNumber = 0;

	for (const FString& Line : Lines)
	{
		LineNumber++;

		// Remove leading/trailing whitespace
		FString TrimmedLine = Line.TrimStart().TrimEnd();

		// Skip comments and empty lines
		if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT("#")))
		{
			continue;
		}

		FVector Direction;
		if (!ParseDirectionLine(TrimmedLine, LineNumber, Direction, OutError))
		{
			return false;
		}

		OutDirections.Add(Direction);
	}

	if (OutDirections.Num() == 0)
	{
		OutError = TEXT("No directions parsed (file empty or only comments)");
		return false;
	}

	return true;
}

// ────────────────────────────────────────────────────────────────────────────
// Attack Sequence Parser
// ────────────────────────────────────────────────────────────────────────────

bool UAssetImportLibrary::ParseAttackSequenceText(
	const FString& InText,
	TArray<FAttackPhase>& OutPhases,
	FString& OutError)
{
	OutPhases.Empty();
	OutError.Empty();

	TArray<FString> Lines;
	InText.ParseIntoArrayLines(Lines, true);

	FAttackPhase CurrentPhase;
	bool bInPhase = false;
	int32 LineNumber = 0;
	bool bPatternSet = false;

	for (const FString& Line : Lines)
	{
		LineNumber++;

		FString TrimmedLine = Line.TrimStart().TrimEnd();

		// Skip comments and empty lines
		if (TrimmedLine.IsEmpty() || TrimmedLine.StartsWith(TEXT("#")))
		{
			continue;
		}

		TArray<FString> Tokens = TokenizeLine(TrimmedLine);
		if (Tokens.Num() == 0) continue;

		FString Keyword = Tokens[0].ToLower();

		if (Keyword == TEXT("phase"))
		{
			// End previous phase if one was being built
			if (bInPhase)
			{
				if (!bPatternSet)
				{
					OutError = FString::Printf(
						TEXT("Line %d: Phase missing pattern specification"),
						LineNumber);
					return false;
				}
				OutPhases.Add(CurrentPhase);
			}

			// Start new phase
			CurrentPhase = FAttackPhase();
			CurrentPhase.Interval = 0.1f;
			CurrentPhase.ShotCount = 1;
			CurrentPhase.StartDelay = 0.f;
			CurrentPhase.bLoop = false;
			bInPhase = true;
			bPatternSet = false;
		}
		else if (Keyword == TEXT("pattern"))
		{
			if (!bInPhase)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'pattern' keyword outside of phase block"),
					LineNumber);
				return false;
			}

			if (Tokens.Num() < 2)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'pattern' requires a pattern name"),
					LineNumber);
				return false;
			}

			// For now, store the pattern name as a string.
			// In practice, you'd load the FirePattern asset by name.
			// This is a placeholder — you'll need to resolve it in Rebuild().
			CurrentPhase.Pattern = nullptr;  // To be resolved during Rebuild()
			// TODO: Store pattern name for later resolution
			bPatternSet = true;
		}
		else if (Keyword == TEXT("interval"))
		{
			if (!bInPhase)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'interval' keyword outside of phase block"),
					LineNumber);
				return false;
			}

			if (Tokens.Num() < 2)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'interval' requires a float value"),
					LineNumber);
				return false;
			}

			float Interval;
			if (!StringToFloat(Tokens[1], Interval, OutError))
			{
				OutError = FString::Printf(
					TEXT("Line %d, interval: %s"), LineNumber, *OutError);
				return false;
			}

			CurrentPhase.Interval = Interval;
		}
		else if (Keyword == TEXT("shots"))
		{
			if (!bInPhase)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'shots' keyword outside of phase block"),
					LineNumber);
				return false;
			}

			if (Tokens.Num() < 2)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'shots' requires an integer value"),
					LineNumber);
				return false;
			}

			int32 ShotCount;
			if (!StringToInt(Tokens[1], ShotCount, OutError))
			{
				OutError = FString::Printf(
					TEXT("Line %d, shots: %s"), LineNumber, *OutError);
				return false;
			}

			CurrentPhase.ShotCount = ShotCount;
		}
		else if (Keyword == TEXT("startdelay"))
		{
			if (!bInPhase)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'startdelay' keyword outside of phase block"),
					LineNumber);
				return false;
			}

			if (Tokens.Num() < 2)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'startdelay' requires a float value"),
					LineNumber);
				return false;
			}

			float StartDelay;
			if (!StringToFloat(Tokens[1], StartDelay, OutError))
			{
				OutError = FString::Printf(
					TEXT("Line %d, startdelay: %s"), LineNumber, *OutError);
				return false;
			}

			CurrentPhase.StartDelay = StartDelay;
		}
		else if (Keyword == TEXT("loop"))
		{
			if (!bInPhase)
			{
				OutError = FString::Printf(
					TEXT("Line %d: 'loop' keyword outside of phase block"),
					LineNumber);
				return false;
			}

			CurrentPhase.bLoop = true;
		}
		else
		{
			OutError = FString::Printf(
				TEXT("Line %d: Unknown keyword '%s'"),
				LineNumber, *Keyword);
			return false;
		}
	}

	// End the last phase if one was being built
	if (bInPhase)
	{
		if (!bPatternSet)
		{
			OutError = FString::Printf(
				TEXT("End of file: Last phase missing pattern specification"));
			return false;
		}
		OutPhases.Add(CurrentPhase);
	}

	if (OutPhases.Num() == 0)
	{
		OutError = TEXT("No phases parsed (file empty or only comments)");
		return false;
	}

	return true;
}
