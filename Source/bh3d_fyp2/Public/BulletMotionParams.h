#pragma once

#include "CoreMinimal.h"
#include "BulletMotionParams.generated.h"

/**
 * FBulletMotionParams
 *
 * A plain data struct that fully describes how a bullet moves after it is spawned.
 * It is passed from the spawner to the bullet's movement component at spawn time.
 * The spawner never touches the bullet again after that.
 *
 * Designed to live inside UFirePattern (as the default) and FBeamDefinition
 * (as a per-beam override), so every beam in a pattern can move differently.
 */
USTRUCT(BlueprintType)
struct FBulletMotionParams
{
    GENERATED_BODY()

    // ── Speed ──────────────────────────────────────────────────────────────

    /** Speed at the moment of spawn, in cm/s. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion|Speed")
    float InitialSpeed = 600.f;

    /**
     * Change in speed per second, in cm/s².
     * Positive = accelerates, negative = decelerates.
     * The bullet will not go below 0 speed.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion|Speed")
    float LinearAcceleration = 0.f;

    /**
     * Speed cap, in cm/s. Only applied when LinearAcceleration != 0.
     * Set to 0 to leave speed uncapped.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion|Speed")
    float MaxSpeed = 0.f;

    // ── Direction ──────────────────────────────────────────────────────────

    /**
     * Rotation applied to the bullet's velocity direction each second.
     * This curves the bullet's path without changing its speed.
     *
     * Examples:
     *   FRotator(0, 45, 0)  → curves 45°/s around world Z (yaws left in top-down view)
     *   FRotator(45, 0, 0)  → curves 45°/s around world Y (pitches upward)
     *
     * For classic shmup curving bullets, set only the Yaw component.
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion|Direction")
    FRotator AngularVelocity = FRotator::ZeroRotator;

    // ── Lifetime ───────────────────────────────────────────────────────────

    /**
     * How long the bullet lives, in seconds.
     * Set to 0 for an immortal bullet (use with care — always ensure
     * some other mechanism cleans it up, e.g. a kill-plane).
     */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Motion|Lifetime")
    float Lifetime = 8.f;
};
