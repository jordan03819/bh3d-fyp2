#pragma once

#include "CoreMinimal.h"
#include "FMovementPoint.generated.h"

USTRUCT(BlueprintType)
struct FMovementPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector Offset = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float AttackDuration = 3.f;
};
