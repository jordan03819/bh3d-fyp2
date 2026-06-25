#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BulletMotionParams.h"
#include "BulletMovementComponent.generated.h"

class ACPP_BulletManager;

/**
 * UBulletMovementComponent
 *
 * Drives a bullet actor's movement each tick based on FBulletMotionParams.
 * This component is the only thing that moves the bullet — the bullet actor
 * itself is passive.
 *
 * ── Extension ────────────────────────────────────────────────────────────
 *
 * To add a new movement behaviour (homing, sine wave, orbit, etc.):
 *   1. Subclass UBulletMovementComponent.
 *   2. Override UpdateMotion(float DeltaTime).
 *   3. Add any extra parameters your subclass needs.
 *   4. Attach your subclass to a bullet Blueprint instead of this one.
 *
 * The spawner only calls Initialize() — it does not care which subclass
 * is attached. Nothing else needs to change.
 *
 * ── Lifecycle ────────────────────────────────────────────────────────────
 *
 *   BulletManager::GrowPool() pre-spawns the bullet (tick disabled).
 *   BulletManager::FireBullet() calls bullet->InitializeBullet(Params)
 *       → Initialize() here re-enables tick and sets initial velocity.
 *   TickComponent() calls UpdateMotion() every frame while the bullet is live.
 *   On lifetime expiry, UpdateMotion calls BulletManager::ReturnBullet()
 *       → ResetForPool() here disables tick and clears state ready for reuse.
 */
UCLASS(ClassGroup=(BulletSystem), Blueprintable,
       meta=(BlueprintSpawnableComponent))
class BH3D_FYP2_API UBulletMovementComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UBulletMovementComponent();

    /**
     * Initialise this component with motion parameters.
     * Must be called immediately after the bullet is spawned.
     * Calling it a second time resets the bullet's motion entirely.
     *
     * @param Params  The motion description received from the spawner.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Movement")
    void Initialize(const FBulletMotionParams& Params);

    /**
     * Called by ACPP_BulletManager when this bullet is returned to the pool.
     * Disables tick and clears motion state so the next Initialize() starts clean.
     * Do NOT call Destroy() — the manager reuses this actor.
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Movement")
    void ResetForPool();

protected:
    virtual void BeginPlay() override;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType,
                               FActorComponentTickFunction* ThisTickFunction) override;

    /**
     * Core motion update called every tick.
     * Override this in subclasses to implement custom movement behaviour.
     * The base implementation provides: linear velocity, linear acceleration,
     * speed cap, angular velocity (path curving), and lifetime expiry.
     *
     * @param DeltaTime  Time elapsed since last tick, in seconds.
     */
    UFUNCTION(BlueprintNativeEvent, Category="BulletSystem|Movement")
    void UpdateMotion(float DeltaTime);
    virtual void UpdateMotion_Implementation(float DeltaTime);

    // ── State (accessible to subclasses) ──────────────────────────────────

    /** A copy of the params passed to Initialize(). */
    UPROPERTY(BlueprintReadOnly, Category="BulletSystem|Movement")
    FBulletMotionParams MotionParams;

    /**
     * Current velocity vector in world space, in cm/s.
     * Subclasses may read and write this directly.
     * Initialized from GetOwner()->GetActorForwardVector() * InitialSpeed.
     */
    UPROPERTY(BlueprintReadOnly, Category="BulletSystem|Movement")
    FVector Velocity = FVector::ZeroVector;

    /**
     * Cached scalar speed (cm/s) — kept in sync with Velocity to avoid
     * a Velocity.Size() sqrt call every tick.
     * Subclasses: if you modify Velocity directly, also update CurrentSpeed.
     */
    UPROPERTY(BlueprintReadOnly, Category="BulletSystem|Movement")
    float CurrentSpeed = 0.f;

    /** Seconds elapsed since Initialize() was called. */
    UPROPERTY(BlueprintReadOnly, Category="BulletSystem|Movement")
    float ElapsedTime = 0.f;

private:
    bool bInitialized = false;
};
