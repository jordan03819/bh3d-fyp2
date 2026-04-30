#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BulletMotionParams.h"
#include "CPP_Bullet.generated.h"

/**
 * ACPP_Bullet
 *
 * The base bullet actor. It is intentionally thin — it owns a visual,
 * a collision shape, and a UBulletMovementComponent. Nothing else.
 *
 * All movement logic lives in UBulletMovementComponent (or its subclasses).
 * All gameplay logic (damage, effects on hit) should be added in a
 * Blueprint subclass or a separate component.
 *
 * ── How to use ────────────────────────────────────────────────────────────
 *
 *   1. Create a Blueprint subclass of ACPP_Bullet.
 *   2. Add a mesh, particle, or sprite component for the visual.
 *   3. Add a collision component and bind OnActorHit or OnComponentBeginOverlap.
 *   4. Optionally swap the default UBulletMovementComponent for a subclass
 *      (e.g. UHomingBulletMovementComponent) in the Blueprint defaults.
 *
 * ── How the spawner interacts with it ────────────────────────────────────
 *
 *   The spawner calls InitializeBullet(Params) once, immediately after
 *   SpawnActor(). After that it never touches this bullet again.
 */
UCLASS(Blueprintable)
class BH3D_FYP2_API ACPP_Bullet : public AActor
{
    GENERATED_BODY()

public:
    ACPP_Bullet();

    /**
     * Called by ACPP_Spawner immediately after this bullet is spawned.
     * Forwards the motion params to the attached movement component.
     *
     * If you replace the default movement component in a Blueprint subclass,
     * make sure the replacement also implements Initialize(FBulletMotionParams).
     *
     * @param Params  Motion data from the spawner (speed, acceleration, etc.)
     */
    UFUNCTION(BlueprintCallable, Category="BulletSystem|Bullet")
    void InitializeBullet(const FBulletMotionParams& Params);

protected:
    virtual void BeginPlay() override;

private:
    /**
     * The component that drives this bullet's position each tick.
     * Swap this in a Blueprint subclass for custom motion behaviours
     * (homing, sine wave, orbit, etc.) without touching C++.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BulletSystem|Components",
              meta=(AllowPrivateAccess="true"))
    class UBulletMovementComponent* MovementComponent;
};
