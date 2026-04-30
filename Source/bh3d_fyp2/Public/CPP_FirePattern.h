// FirePattern.h
#pragma once

#include "CoreMinimal.h"
#include "CPP_FirePattern.generated.h"

USTRUCT(BlueprintType)
struct FirePattern
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FVector> Directions;

    FirePattern() {}
    FirePattern(TArray<FVector>& directions)
        : Directions(directions)
    {}
};
