#include "CPP_BulletManager.h"
#include "CPP_Bullet.h"
#include "BulletMovementComponent.h"   // for ResetForPool() call in DeactivateEntry
#include "BulletMotionParams.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

// ─── Collision channel ────────────────────────────────────────────────────────
//
// Add a custom Object Channel called "EnemyBullet" in:
//   Project Settings → Engine → Collision → Object Channels → New Channel
//   Name: EnemyBullet | Default Response: Ignore
//
// UE assigns it to the next free ECC_GameTraceChannel slot.
// Match the index below to whatever slot it gets assigned.
// Check DefaultEngine.ini: +DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,...)
//
static constexpr ECollisionChannel BULLET_OBJECT_CHANNEL = ECC_GameTraceChannel1;

// ActorTag set on every pooled bullet.  Used in QueryBulletsAt() to confirm
// a result is a bullet without a Cast<> — which is what your advisor meant by "use tags".
static const FName BULLET_ACTOR_TAG = TEXT("EnemyBullet");

// ─── Constructor ──────────────────────────────────────────────────────────────

ACPP_BulletManager::ACPP_BulletManager()
{
    // The manager itself does not tick — movement still lives in each bullet's
    // UBulletMovementComponent.  The manager is purely a lifecycle controller.
    PrimaryActorTick.bCanEverTick = false;
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

    // Un-hide and re-enable collision
    Bullet->SetActorHiddenInGame(false);
    Bullet->SetActorEnableCollision(true);

    // Hand motion params to the movement component — same call the spawner used to make
    Bullet->InitializeBullet(Params);

    // Notify Blueprint so it can re-activate the Niagara trail and any other
    // visual components. Niagara does NOT restart automatically on unhide.
    Bullet->OnActivatedFromPool();

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
    else
    {
        // Bullet was NOT spawned through this pool (e.g. spawner still using
        // SpawnActor on some patterns, or called from Blueprint directly).
        //
        // Without this fallback: Manager is non-null so the movement component's
        // else { Destroy() } branch is skipped, and the bullet lives forever —
        // ticking indefinitely and accumulating as "steadily increasing tick time".
        UE_LOG(LogTemp, Warning,
               TEXT("BulletManager::ReturnBullet — bullet [%s] not in pool. "
                    "Destroying conventionally. Check that ACPP_Spawner is using "
                    "FireBullet() and not SpawnActor()."),
               *GetNameSafe(Bullet));
        Bullet->Destroy();
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

void ACPP_BulletManager::QueryBulletsAt(const FVector&        Centre,
                                         float                 Radius,
                                         TArray<ACPP_Bullet*>& OutBullets) const
{
    OutBullets.Reset();

    TArray<FOverlapResult> Overlaps;

    // Named stat so this query shows up cleanly in the profiler
    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BulletHitCheck),
                                      /*bTraceComplex=*/false);
    QueryParams.AddIgnoredActor(this);

    // Only touches the "EnemyBullet" object channel — nothing else participates.
    // This single call replaces N individual overlap events.
    const FCollisionObjectQueryParams ObjectParams(BULLET_OBJECT_CHANNEL);

    GetWorld()->OverlapMultiByObjectType(
        Overlaps,
        Centre,
        FQuat::Identity,
        ObjectParams,
        FCollisionShape::MakeSphere(Radius),
        QueryParams
    );

    for (const FOverlapResult& Hit : Overlaps)
    {
        AActor* HitActor = Hit.GetActor();
        if (!IsValid(HitActor)) continue;

        // ── "Use tags" — your advisor's advice ────────────────────────────
        // ActorHasTag is a simple TArray<FName>::Contains — no RTTI, no vtable.
        // We tagged every pooled bullet "EnemyBullet" in GrowPool(), so this
        // confirms we got a bullet without a Cast<>.
        if (HitActor->ActorHasTag(BULLET_ACTOR_TAG))
        {
            // Cast is safe here because we know the class; we just use the tag
            // to avoid the cast on every result before we confirm it's a bullet.
            OutBullets.Add(static_cast<ACPP_Bullet*>(HitActor));
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
        // Notify Blueprint first so it can deactivate Niagara before we hide.
        // Niagara keeps simulating through SetActorHiddenInGame — explicit
        // Deactivate() in BP is the only way to stop it ticking on pooled bullets.
        Entry.Actor->OnReturnedToPool();

        Entry.Actor->SetActorHiddenInGame(true);
        Entry.Actor->SetActorEnableCollision(false);

        // Tell the movement component to disable its tick and wipe state.
        // Without this a bullet returned mid-flight would still tick one more frame.
        if (UBulletMovementComponent* MC =
                Entry.Actor->FindComponentByClass<UBulletMovementComponent>())
        {
            MC->ResetForPool();
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
