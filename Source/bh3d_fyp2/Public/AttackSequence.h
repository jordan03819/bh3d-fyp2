#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FirePattern.h"
#include "AttackSequence.generated.h"

/**
 * FAttackPhase
 *
 * One timed entry in an UAttackSequence. It binds a UFirePattern to
 * timing information: how often to fire, how many times, and when to start.
 *
 * The spawner steps through phases in order. When a phase ends it moves to
 * the next, or loops if bLoop is true.
 */
USTRUCT(BlueprintType)
struct FAttackPhase
{
    GENERATED_BODY()

    /** The pattern to fire during this phase. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase")
    UFirePattern* Pattern = nullptr;

    /**
     * Time between volleys in this phase, in seconds.
     * Lower values = faster fire rate.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase",
              meta=(ClampMin="0.01"))
    float Interval = 1.f;

    /**
     * How many times to fire before moving to the next phase.
     * Set to -1 to fire indefinitely (combine with bLoop=true to loop this phase).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase")
    int32 ShotCount = -1;

    /**
     * Delay in seconds before this phase begins firing, measured from
     * when the previous phase ended (or from sequence start for phase 0).
     * Useful for pauses and telegraphs between attacks.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase")
    float StartDelay = 0.f;

    /**
     * If true, this phase loops forever and the sequence never advances past it.
     * Useful for the final phase of a boss that should repeat until the boss dies.
     * ShotCount is still respected per loop iteration when >= 0.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase")
    bool bLoop = false;

    /**
     * If true, reorient the fire pattern so that the +Z direction points toward
     * the player pawn. The pattern geometry is preserved; only the Z component
     * of each beam direction is replaced with the normalized Z of the vector from
     * spawner to player.
     * Useful for making patterns "aim" vertically at the player without changing
     * the pattern's core geometry.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Phase")
    bool bFacePlayerOnZ = false;
};


/**
 * UAttackSequence
 *
 * A DataAsset that chains multiple FAttackPhases together to form
 * a complete attack pattern for an enemy or boss.
 *
 * The spawner executes phases in order. Timing, repetition count, and
 * transitions between patterns all live here — UFirePattern knows nothing
 * about any of this.
 *
 * ── Example sequences ────────────────────────────────────────────────────
 *
 *   Simple looping enemy:
 *     Phase 0: Pattern=RingPattern, Interval=0.5, ShotCount=-1, bLoop=true
 *
 *   Boss burst-then-spiral:
 *     Phase 0: Pattern=BurstRing, Interval=0.1,  ShotCount=5
 *     Phase 1: Pattern=null,      Interval=2.0,  ShotCount=1   ← pause
 *     Phase 2: Pattern=Spiral,    Interval=0.15, ShotCount=-1, bLoop=true
 *
 *   One-shot fairy:
 *     Phase 0: Pattern=SpreadShot, Interval=1.0, ShotCount=1
 *     (sequence ends, spawner goes idle)
 */
UCLASS(BlueprintType)
class BH3D_FYP2_API UAttackSequence : public UDataAsset
{
    GENERATED_BODY()

public:

    /**
     * Ordered list of phases this sequence executes.
     * The spawner starts at index 0 and advances forward.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sequence")
    TArray<FAttackPhase> Phases;
};
