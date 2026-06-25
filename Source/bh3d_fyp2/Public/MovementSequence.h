#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FMovementPoint.h"
#include "MovementSequence.generated.h"

UCLASS(BlueprintType)
class BH3D_FYP2_API UMovementSequence : public UDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FMovementPoint> Points;
};
