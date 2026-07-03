#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_BulletManager.generated.h"

class ACPP_Bullet;
class UBulletMovementComponent;
struct FBulletMotionParams;

/**
 * ACPP_BulletManager
 *
 * WHAT IT DOES
 *   1. Object Pool  — pre-spawns bullets so SpawnActor / Destroy never run at runtime.
 *   2. FireBullet() — pulls a dormant bullet from the free list, places it, calls
 *                     InitializeBullet(), then registers its UBulletMovementComponent
 *                     in ActiveComponents. Movement math still lives entirely inside
 *                     UBulletMovementComponent (and its subclasses) — unchanged.
 *                     The only thing that moved is WHO calls UpdateMotion(): the
 *                     manager's single Tick() now drives every active component
 *                     directly, instead of each component ticking itself. This
 *                     collapses N per-bullet tick registrations into 1.
 *   3. ReturnBullet() — deactivates and re-pools a bullet.  Called by:
 *                       • UBulletMovementComponent when lifetime expires (replaces Destroy)
 *                       • Player/enemy code on a hit
 *                       • ReturnAllBullets() on stage clear / respawn
 *   4. QueryBulletsAt() — ONE sphere overlap per frame from the player/enemy replaces
 *                         N per-bullet overlap events.  This is the "use tags" fix:
 *                         bullets get an "EnemyBullet" ActorTag so QueryBulletsAt can
 *                         confirm them without Cast<> (see implementation).
 *
 * WHAT IT DOES NOT DO
 *   Movement math stays in UBulletMovementComponent.  The manager is not an ECS rewrite
 *   — it adds pooling and centralises hit detection.
 *
 * SETUP
 *   1. Place ONE instance in the persistent level (or spawn it from GameMode::BeginPlay).
 *   2. Fill BulletClasses in the Details panel with every BP_Bullet subclass you use.
 *   3. In ACPP_Spawner, replace SpawnActor calls with ACPP_BulletManager::Get(this)->FireBullet(...).
 *   4. In UBulletMovementComponent, replace Destroy() with ReturnBullet() — see patch file.
 *   5. Turn off GenerateOverlapEvents on the Box in BP_BulletNew — see CollisionSetup.md.
 */
UCLASS()
class BH3D_FYP2_API ACPP_BulletManager : public AActor
{
    GENERATED_BODY()

public:
    ACPP_BulletManager();

    // ─── Configuration ────────────────────────────────────────────────────

    /** Every bullet Blueprint subclass you fire.  One sub-pool per class. */
    UPROPERTY(EditDefaultsOnly, Category="Bullet Pool")
    TArray<TSubclassOf<ACPP_Bullet>> BulletClasses;

    /** Instances pre-spawned per class on BeginPlay. */
    UPROPERTY(EditDefaultsOnly, Category="Bullet Pool")
    int32 PoolSizePerClass = 500;

    /**
     * When a sub-pool runs dry, grow it by this many.
     * 0 = hard cap (oldest active bullet of that class gets recycled).
     * Tune this so it never fires in normal play; a log warning will tell you.
     */
    UPROPERTY(EditDefaultsOnly, Category="Bullet Pool")
    int32 OverflowGrowBy = 64;

    // ─── Runtime API ──────────────────────────────────────────────────────

    /**
     * Activate a pooled bullet.
     *
     * @param BulletClass   Must be in BulletClasses (or a sub-pool will be created).
     * @param Location      World spawn position.
     * @param Direction     Unit travel vector.  Converted to rotator for SetActorLocation.
     * @param Params        Forwarded verbatim to InitializeBullet().
     * @return              The activated actor, or nullptr if pool is exhausted and
     *                      OverflowGrowBy == 0.
     */
    UFUNCTION(BlueprintCallable, Category="Bullet Pool")
    ACPP_Bullet* FireBullet(TSubclassOf<ACPP_Bullet> BulletClass,
                            const FVector&           Location,
                            const FVector&           Direction,
                            const FBulletMotionParams& Params);

    /**
     * Deactivate a bullet and push it back onto the free list.
     *
     * This is the pool-aware replacement for GetOwner()->Destroy() inside
     * UBulletMovementComponent.  See BulletMovementComponent_Patch.cpp.
     */
    UFUNCTION(BlueprintCallable, Category="Bullet Pool")
    void ReturnBullet(ACPP_Bullet* Bullet);

    /** Deactivate every active bullet instantly (stage clear, player death, etc.). */
    UFUNCTION(BlueprintCallable, Category="Bullet Pool")
    void ReturnAllBullets();

    /**
     * Sphere-overlap query against the "EnemyBullet" collision channel.
     *
     * Call this ONCE per frame from the player character (or enemy) instead of
     * responding to per-bullet overlap events.  Internally it runs one
     * OverlapMultiByObjectType call and filters by the "EnemyBullet" ActorTag —
     * the "use tags" part your advisor mentioned.
     *
     * @param Centre      Hit-detection origin (e.g. player capsule centre).
     * @param Radius      Hitbox radius in cm.
     * @param OutBullets  Filled with all bullets overlapping the sphere.
     */
    UFUNCTION(BlueprintCallable, Category="Bullet Pool")
    void QueryBulletsAt(const FVector& Centre, float Radius,
                        TArray<ACPP_Bullet*>& OutBullets) const;

    // ─── Static accessor ──────────────────────────────────────────────────

    /** Find the manager without a hard UPROPERTY reference. */
    UFUNCTION(BlueprintPure, meta=(WorldContext="WorldContextObject"),
              Category="Bullet Pool")
    static ACPP_BulletManager* Get(const UObject* WorldContextObject);

    // ─── Overrides ────────────────────────────────────────────────────────

    virtual void BeginPlay() override;

    /**
     * Drives every active bullet's movement in one pass.
     * Replaces per-component TickComponent() — see UBulletMovementComponent.
     */
    virtual void Tick(float DeltaTime) override;

private:
    // ─── Pool internals ───────────────────────────────────────────────────

    struct FPoolEntry
    {
        ACPP_Bullet* Actor  = nullptr;
        bool         bActive = false;
    };

    struct FClassPool
    {
        TArray<FPoolEntry> Entries;
        TArray<int32>      FreeList;
    };

    // One sub-pool per bullet class
    TMap<UClass*, FClassPool> Pools;

    // Reverse map: actor → {ClassPtr, EntryIndex} for O(1) ReturnBullet
    TMap<ACPP_Bullet*, TPair<UClass*, int32>> ActorToEntry;

    // Flat list of every currently-active bullet's movement component.
    // Tick() walks this once per frame and calls UpdateMotion() on each —
    // this is what replaces per-component ticking.
    UPROPERTY()
    TArray<UBulletMovementComponent*> ActiveComponents;

    // Helpers
    void        GrowPool(TSubclassOf<ACPP_Bullet> Class, int32 Count);
    FClassPool& GetOrCreatePool(TSubclassOf<ACPP_Bullet> Class);
    void        DeactivateEntry(UClass* Class, int32 Index);
};
