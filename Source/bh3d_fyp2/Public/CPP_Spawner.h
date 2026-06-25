#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AttackSequence.h"
#include "CPP_Spawner.generated.h"

/**
 * ACPP_Spawner
 *
 * Executes a UAttackSequence — steps through its phases, fires volleys
 * at the right times, and advances to the next phase when done.
 *
 * This actor is the only entry point into the bullet system at runtime.
 * It does not contain any pattern data itself — all data lives in the
 * DataAssets assigned to it.
 *
 * ── Typical setup ─────────────────────────────────────────────────────────
 *
 *   1. Place ACPP_Spawner (or a Blueprint subclass) in the level or
 *      attach it to an enemy.
 *   2. Assign a UAttackSequence DataAsset to the Sequence property.
 *   3. Optionally set a fallback DefaultBulletClass for patterns that
 *      don't specify one.
 *   4. Call SetSequence() at runtime to change attack patterns (e.g. on
 *      a boss phase transition).
 *
 * ── Extending behaviour ───────────────────────────────────────────────────
 *
 *   Override OnPhaseStarted() and OnSequenceFinished() in a Blueprint
 *   subclass to trigger sound effects, animations, or phase transitions
 *   without modifying C++.
 */
UCLASS(Blueprintable)
class BH3D_FYP2_API ACPP_Spawner : public AActor
{
    GENERATED_BODY()

public:
    ACPP_Spawner();

    // ── Configuration ─────────────────────────────────────────────────────

    /**
     * The attack sequence this spawner will execute.
     * Can be swapped at runtime via SetSequence().
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BulletSystem|Components",meta=(AllowPrivateAccess="true"))
    USceneComponent* SceneRoot;
    
    /** Draw beam directions as debug lines instead of spawning bullets.
 *  Use this to verify pattern geometry before live testing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BulletSystem|Debug")
    bool bDebugDrawOnly = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BulletSystem|Spawner")
    UAttackSequence* Sequence;

    /**
     * Fallback bullet class used when a pattern's DefaultBulletClass is null.
     * Acts as a global default so you don't have to set a class on every pattern.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BulletSystem|Spawner")
    TSubclassOf<AActor> DefaultBulletClass;

    // ── Runtime control ───────────────────────────────────────────────────

    /**
     * Replace the current sequence and restart from phase 0.
     * Safe to call at any time, including mid-sequence.
     *
     * @param NewSequence  The new sequence to run. Pass null to stop firing.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Spawner")
    void SetSequence(UAttackSequence* NewSequence);

    /** Pause or resume the spawner without discarding sequence state. */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Spawner")
    void SetPaused(bool bPause);

    // ── Blueprint hooks ───────────────────────────────────────────────────

    /**
     * Called when the spawner enters a new phase.
     * Override in Blueprint to play a sound, trigger an animation, etc.
     *
     * @param PhaseIndex  The index of the phase that just started.
     */
    UFUNCTION(BlueprintNativeEvent, Category="BulletSystem|Spawner")
    void OnPhaseStarted(int32 PhaseIndex);
    virtual void OnPhaseStarted_Implementation(int32 PhaseIndex) {}

    /**
     * Called when all phases have finished and the sequence is complete.
     * Not called if the last phase loops forever.
     */
    UFUNCTION(BlueprintNativeEvent, Category="BulletSystem|Spawner")
    void OnSequenceFinished();
    virtual void OnSequenceFinished_Implementation() {}

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:

    // ── Sequence state ────────────────────────────────────────────────────

    /** Index into Sequence->Phases for the currently active phase. */
    int32 CurrentPhaseIndex = 0;

    /** How many volleys have been fired during the current phase. */
    int32 ShotsThisPhase = 0;

    /**
     * Time accumulator within the current phase.
     * Starts negative when a StartDelay is set (counts up to 0 before firing).
     */
    float PhaseTimer = 0.f;

    /**
     * Accumulated spin angle in degrees, driven by the current pattern's SpinRate.
     * Resets to 0 when the phase changes so each phase starts with a fresh angle.
     */
    float AccumSpinAngle = 0.f;

    bool bPaused = false;

    // ── Internal helpers ──────────────────────────────────────────────────

    /** Reset all state and begin executing from phase 0. */
    void StartSequence();

    /** Move to the next phase (or finish the sequence). */
    void AdvancePhase();

    /**
     * Spawn all bullets described by Pattern.
     * Applies accumulated spin, beam layout, spread, spacing, and per-beam overrides.
     *
     * @param Pattern  The pattern to execute. Must not be null.
     */
    void SpawnVolley(UFirePattern* Pattern);

    /**
     * Resolve which bullet class to use for a given beam, falling back through:
     *   BeamDefinition.BulletClassOverride → Pattern.DefaultBulletClass → DefaultBulletClass
     */
    TSubclassOf<AActor> ResolveBulletClass(const UFirePattern* Pattern,
                                            const FBeamDefinition* BeamDef) const;

    /**
     * Resolve which motion params to use for a given beam, falling back through:
     *   BeamDefinition.MotionParamsOverride (if enabled) → Pattern.DefaultMotionParams
     */
    const FBulletMotionParams& ResolveMotionParams(const UFirePattern* Pattern,
                                                    const FBeamDefinition* BeamDef) const;
};
