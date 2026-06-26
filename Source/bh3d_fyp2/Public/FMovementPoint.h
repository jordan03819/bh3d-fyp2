#pragma once

#include "CoreMinimal.h"
#include "FMovementPoint.generated.h"

UENUM(BlueprintType)
enum class EMovementReference : uint8
{
	Origin UMETA(DisplayName = "Origin-Relative (spawn offset)"),
	Move UMETA(DisplayName = "Move-Relative (current offset)"),
	Absolute UMETA(DisplayName = "Absolute (world position)")
};

UENUM(BlueprintType)
enum class EMovementCommand : uint8
{
	MoveTo UMETA(DisplayName = "Move to target"),
	Wait UMETA(DisplayName = "Wait (stay in place)"),
	SetOrigin UMETA(DisplayName = "Set new origin point")
};

USTRUCT(BlueprintType)
struct FMovementPoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMovementCommand Command = EMovementCommand::MoveTo;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	EMovementReference Reference = EMovementReference::Origin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float AttackDuration = 2.f;
};
