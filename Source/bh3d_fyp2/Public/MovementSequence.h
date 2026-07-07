#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FMovementPoint.h"
#include "MovementSequence.generated.h"

/**
 * UMovementSequence — Data-driven choreography with text import.
 *
 * Format (paste into ImportText):
 *
 *   # X Y Z AttackDuration
 *
 *   0 0 300 2
 *   200 100 300 3
 *   400 -100 250 2
 *
 * Click Rebuild to parse.
 */
UCLASS(Blueprintable)
class BH3D_FYP2_API UMovementSequence : public UDataAsset
{
    GENERATED_BODY()

public:
    /** The choreographed movement points. */
    UPROPERTY(BlueprintReadOnly, Category = "Movement")
    TArray<FMovementPoint> MovementPoints;

    /**
     * Plain text import format.
     * Format: X Y Z AttackDuration (one per line)
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Import", meta = (MultiLine = "true"))
    FString ImportText;

    /**
     * Parse ImportText and populate MovementPoints.
     * Call this after pasting text into ImportText.
     */
    UFUNCTION(CallInEditor, Category = "Import")
    void Rebuild();

    /**
     * Clear all movement points.
     */
    UFUNCTION(CallInEditor, Category = "Import")
    void Clear();
};
