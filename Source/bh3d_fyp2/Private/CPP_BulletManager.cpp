#include "CPP_BulletManager.h"
#include "CPP_Bullet.h"
#include "BulletMovementComponent.h"   // for ResetForPool() + UpdateMotion() + registration
#include "BulletMotionParams.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// ─── Bullet tag ───────────────────────────────────────────────────────────────
//
// ActorTag set on every pooled bullet. No longer used by QueryBulletsAt (see
// below — that's array-based now), but left in place in case other code
// (BP logic, VFX triggers, etc.) checks for it. Safe to remove if unused
// elsewhere.
static const FName BULLET_ACTOR_TAG = TEXT("EnemyBullet");

// Half-extent (cm) added to every QueryBulletsAt radius check, approximating
// the bullet's physical size now that hit detection is point-to-point.
// Sized for the engine Cube mesh at scale (1.0, 0.16, 0.16):
// 100cm * 0.16 / 2 = 8cm cross-section half-extent.
static constexpr float BULLET_HIT_RADIUS = 8.0f;

// ─── Constructor ──────────────────────────────────────────────────────────────

ACPP_BulletManager::ACPP_BulletManager()
{
    // CHANGED: the manager now ticks — once, for every active bullet.
    // UBulletMovementComponent no longer ticks itself (bCanEverTick = false
    // there); this single Tick() calls UpdateMotion() on each active
    // component directly instead. Same math, one tick registration instead
    // of hundreds.
    PrimaryActorTick.bCanEverTick = true;
}

// ─── BeginPlay ────────────────────────────────────────────────────────────────

void ACPP_BulletManager::BeginPlay()
{
    Super::BeginPlay();

    for (const TSubclassOf<ACPP_Bullet>& Class : BulletClasses)
    {
        if (IsValid(Class))
        {
            GrowPool(Class, PoolSizePerClass);
        }
    }
}

// ─── Tick ─────────────────────────────────────────────────────────────────────
//
// Replaces per-bullet TickComponent(). We loop ActiveComponents backward so
// that a component removing itself mid-loop (ReturnBullet → DeactivateEntry
// → ActiveComponents.RemoveSingleSwap) is safe: RemoveSingleSwap moves the
// current last element into the freed slot, and since we iterate from the
// end, anything swapped in has already been ticked this frame.

void ACPP_BulletManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    for (int32 i = ActiveComponents.Num() - 1; i >= 0; --i)
    {
        UBulletMovementComponent* MC = ActiveComponents[i];

        // Defensive: shouldn't happen (bullets are pooled, not destroyed),
        // but guards against a stray Destroy() call somewhere.
        if (!IsValid(MC))
        {
            ActiveComponents.RemoveAtSwap(i, /*Count=*/1, /*bAllowShrinking=*/false);
            continue;
        }

        MC->UpdateMotion(DeltaTime);
    }
}

// ─── Get ──────────────────────────────────────────────────────────────────────

ACPP_BulletManager* ACPP_BulletManager::Get(const UObject* WorldContextObject)
{
    UWorld* World = GEngine->GetWorldFromContextObject(
        WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
    if (!World) return nullptr;

    return Cast<ACPP_BulletManager>(
        UGameplayStatics::GetActorOfClass(World, ACPP_BulletManager::StaticClass()));
}

// ─── FireBullet ───────────────────────────────────────────────────────────────

ACPP_Bullet* ACPP_BulletManager::FireBullet(TSubclassOf<ACPP_Bullet> BulletClass,
                                             const FVector&           Location,
                                             const FVector&           Direction,
                                             const FBulletMotionParams& Params)
{
    if (!IsValid(BulletClass)) return nullptr;

    FClassPool& Pool = GetOrCreatePool(BulletClass);

    // ── Handle pool exhaustion ────────────────────────────────────────────
    if (Pool.FreeList.IsEmpty())
    {
        if (OverflowGrowBy > 0)
        {
            UE_LOG(LogTemp, Warning,
                   TEXT("BulletManager: sub-pool for [%s] exhausted — growing by %d. "
                        "Increase PoolSizePerClass if this fires frequently."),
                   *BulletClass->GetName(), OverflowGrowBy);
            GrowPool(BulletClass, OverflowGrowBy);
        }

        if (Pool.FreeList.IsEmpty())
        {
            // Hard-cap reached: silently drop the bullet.
            // Alternative: find oldest active bullet of this class and recycle it.
            return nullptr;
        }
    }

    // ── Activate ──────────────────────────────────────────────────────────
    const int32  Idx   = Pool.FreeList.Pop(/*bAllowShrinking=*/false);
    FPoolEntry&  Entry = Pool.Entries[Idx];
    ACPP_Bullet* Bullet = Entry.Actor;

    Entry.bActive = true;

    // Place the bullet.
    // ETeleportType::TeleportPhysics → skips sweep AND skips UpdateOverlaps.
    // This is safe here because we're placing from a dormant state, not sliding
    // through geometry.
    const FRotator FacingRotation = Direction.GetSafeNormal().ToOrientationRotator();
    Bullet->SetActorLocationAndRotation(
        Location,
        FacingRotation,
        /*bSweep=*/false,
        /*OutSweepHitResult=*/nullptr,
        ETeleportType::TeleportPhysics
    );

    // Un-hide. Collision is intentionally left disabled — QueryBulletsAt no
    // longer uses physics overlaps (see below), so bullets never need a live
    // physics body. This also removes the Phys SetBodyTransform cost that
    // used to fire every time an active bullet moved.
    Bullet->SetActorHiddenInGame(false);

    // Hand motion params to the movement component — same call the spawner used to make
    Bullet->InitializeBullet(Params);

    // CHANGED: register this bullet's movement component so Tick() starts
    // calling UpdateMotion() on it. The component itself never ticks.
    if (UBulletMovementComponent* MC =
            Bullet->FindComponentByClass<UBulletMovementComponent>())
    {
        ActiveComponents.AddUnique(MC);
    }

    ActorToEntry.Add(Bullet, { BulletClass.Get(), Idx });

    return Bullet;
}

// ─── ReturnBullet ─────────────────────────────────────────────────────────────

void ACPP_BulletManager::ReturnBullet(ACPP_Bullet* Bullet)
{
    if (!IsValid(Bullet)) return;

    const TPair<UClass*, int32>* Found = ActorToEntry.Find(Bullet);
    if (Found)
    {
        DeactivateEntry(Found->Key, Found->Value);
    }
}

// ─── ReturnAllBullets ─────────────────────────────────────────────────────────

void ACPP_BulletManager::ReturnAllBullets()
{
    // Collect all active entries first to avoid mutation-during-iteration
    TArray<TPair<UClass*, int32>> ToDeactivate;
    ToDeactivate.Reserve(ActorToEntry.Num());

    for (const auto& KV : ActorToEntry)
    {
        ToDeactivate.Add(KV.Value);
    }

    for (const TPair<UClass*, int32>& Entry : ToDeactivate)
    {
        DeactivateEntry(Entry.Key, Entry.Value);
    }
}

// ─── QueryBulletsAt ───────────────────────────────────────────────────────────
//
// CHANGED: array scan instead of a physics overlap. We already have every
// active bullet's UBulletMovementComponent in ActiveComponents — GetOwner()
// gives the actor, GetActorLocation() gives its position. No physics body,
// no collision channel, no ActorTag check needed.
//
// If your bullets have different sizes, add a HitRadius field to
// FBulletMotionParams and read it off MC->MotionParams here instead of
// using a single flat Radius for every bullet.

void ACPP_BulletManager::QueryBulletsAt(const FVector&        Centre,
                                         float                 Radius,
                                         TArray<ACPP_Bullet*>& OutBullets) const
{
    OutBullets.Reset();

    // CHANGED: add BulletHitRadius so a bullet's edge (not just its exact
    // centre point) counts as reaching the query sphere — approximates the
    // reach the old box-vs-sphere overlap gave for free.
    const float CombinedRadius = Radius + BULLET_HIT_RADIUS;
    const float RadiusSq       = CombinedRadius * CombinedRadius;

    for (UBulletMovementComponent* MC : ActiveComponents)
    {
        if (!IsValid(MC)) continue;

        AActor* Owner = MC->GetOwner();
        if (!IsValid(Owner)) continue;

        if (FVector::DistSquared(Owner->GetActorLocation(), Centre) <= RadiusSq)
        {
            OutBullets.Add(static_cast<ACPP_Bullet*>(Owner));
        }
    }
}

// ─── GrowPool ─────────────────────────────────────────────────────────────────

void ACPP_BulletManager::GrowPool(TSubclassOf<ACPP_Bullet> Class, int32 Count)
{
    if (!IsValid(Class) || Count <= 0) return;

    FClassPool& Pool = GetOrCreatePool(Class);

    Pool.Entries.Reserve(Pool.Entries.Num() + Count);
    Pool.FreeList.Reserve(Pool.FreeList.Num() + Count);

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    for (int32 i = 0; i < Count; ++i)
    {
        ACPP_Bullet* Bullet = GetWorld()->SpawnActor<ACPP_Bullet>(
            Class,
            FVector(0.f, 0.f, -100000.f),  // park far below the level
            FRotator::ZeroRotator,
            SpawnParams
        );

        if (!IsValid(Bullet)) continue;

        // ── Tag for QueryBulletsAt ─────────────────────────────────────────
        // ActorHasTag checks this FName — much cheaper than Cast<> per query hit.
        Bullet->Tags.AddUnique(BULLET_ACTOR_TAG);

        // ── Start dormant ──────────────────────────────────────────────────
        Bullet->SetActorHiddenInGame(true);
        Bullet->SetActorEnableCollision(false);

        const int32 NewIdx = Pool.Entries.AddDefaulted();
        Pool.Entries[NewIdx].Actor   = Bullet;
        Pool.Entries[NewIdx].bActive = false;

        Pool.FreeList.Add(NewIdx);
        ActorToEntry.Add(Bullet, { Class.Get(), NewIdx });
    }
}

// ─── DeactivateEntry ──────────────────────────────────────────────────────────

void ACPP_BulletManager::DeactivateEntry(UClass* Class, int32 Index)
{
    FClassPool* Pool = Pools.Find(Class);
    if (!Pool || !Pool->Entries.IsValidIndex(Index)) return;

    FPoolEntry& Entry = Pool->Entries[Index];
    if (!Entry.bActive) return;   // already returned, ignore double-return

    Entry.bActive = false;

    if (IsValid(Entry.Actor))
    {
        Entry.Actor->SetActorHiddenInGame(true);
        Entry.Actor->SetActorEnableCollision(false);

        // Wipe motion state and, critically, unregister from ActiveComponents —
        // that's what actually stops UpdateMotion() being called on this bullet
        // next frame now that the component doesn't tick itself.
        if (UBulletMovementComponent* MC =
                Entry.Actor->FindComponentByClass<UBulletMovementComponent>())
        {
            MC->ResetForPool();
            ActiveComponents.RemoveSingleSwap(MC, /*bAllowShrinking=*/false);
        }
    }

    Pool->FreeList.Add(Index);
    ActorToEntry.Remove(Entry.Actor);
}

// ─── GetOrCreatePool ──────────────────────────────────────────────────────────

ACPP_BulletManager::FClassPool& ACPP_BulletManager::GetOrCreatePool(
    TSubclassOf<ACPP_Bullet> Class)
{
    return Pools.FindOrAdd(Class.Get());
}
