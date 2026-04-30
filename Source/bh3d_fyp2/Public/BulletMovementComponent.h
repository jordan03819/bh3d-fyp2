#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BulletMotionParams.h"
#include "BulletMovementComponent.generated.h"

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
 *   SpawnActor() creates the bullet.
 *   Spawner calls bullet->InitializeBullet(Params), which calls Initialize() here.
 *   TickComponent() calls UpdateMotion() every frame until the bullet is destroyed.
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

    /** Seconds elapsed since Initialize() was called. */
    UPROPERTY(BlueprintReadOnly, Category="BulletSystem|Movement")
    float ElapsedTime = 0.f;

private:
    bool bInitialized = false;
};
