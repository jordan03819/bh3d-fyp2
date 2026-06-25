# Bullet Optimisation Setup Guide

## Why each profiler entry is expensive and what fixes it

```
UpdateOverlaps          ← MoveUpdatedComponent(bSweep=true)       → Fix: TeleportPhysics
MoveComponent(scene)    ← same sweep path                          → Fix: TeleportPhysics
Begin Actor Overlap     ← Box: GenerateOverlapEvents=true          → Fix: turn it OFF
                        ← Box: OverlapAllDynamic (wide response)   → Fix: custom channel
World tick time         ← per-bullet component tick × bullet count → Fix: pool (dormant bullets don't tick)
Tick time               ← same                                     → Fix: pool + tick-on-demand
```

All five items go away with three code changes + two Blueprint settings.

---

## 1  Project Settings — add "EnemyBullet" Object Channel

```
Project Settings
  → Engine → Collision
    → Object Channels → New Channel
      Name:             EnemyBullet
      Default Response: Ignore
```

UE appends to DefaultEngine.ini automatically:
```ini
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Ignore,
    bTraceType=False,bStaticObject=False,Name="EnemyBullet")
```

Note which `ECC_GameTraceChannel` index it gets (1, 2, etc.) and match the alias in
`CPP_BulletManager.cpp`:
```cpp
static constexpr ECollisionChannel BULLET_OBJECT_CHANNEL = ECC_GameTraceChannel1;
```

---

## 2  Project Settings — create "EnemyBullet" Collision Preset

```
Project Settings
  → Engine → Collision → Preset → New
    Name:              EnemyBullet
    CollisionEnabled:  Query Only (No Physics)   ← not QueryAndPhysics
    Object Type:       EnemyBullet               ← your new channel
    All channels:      Ignore                    ← bullets don't collide with anything
```

---

## 3  BP_BulletNew — fix the Box component (2 checkboxes, 1 dropdown)

Open BP_BulletNew.  Select the Box component.  In Details:

| Setting | Old value | New value | Why |
|---|---|---|---|
| Collision Preset | OverlapAllDynamic | EnemyBullet | Stops overlap events firing against walls, other bullets, enemies, etc. |
| Generate Overlap Events | ✓ True | ✗ **False** | Eliminates "Begin Actor Overlap" from the profiler entirely |
| Simulation Generates Hit Events | ✓ True | ✗ **False** | No physics sim = no hit events needed |

With `GenerateOverlapEvents = false` the bullet becomes **invisible to the overlap system**.
You detect it from the player side instead (step 6 below).

---

## 4  Add "EnemyBullet" Actor Tag in BP_BulletNew

Still in BP_BulletNew, select the root (Self).  In Details → Actor → Tags:

```
+ EnemyBullet
```

This tag is what `QueryBulletsAt()` checks with `ActorHasTag()` — your advisor's "use tags"
advice.  It avoids `Cast<ACPP_Bullet>()` (RTTI lookup) on every query result.

*(The manager also adds the tag in `GrowPool()` for C++-spawned pool entries, but setting
it in the Blueprint default means any Blueprint-spawned bullet also carries it.)*

---

## 5  Place ACPP_BulletManager in the level

Drag `BP_BulletManager` (a Blueprint wrapping `ACPP_BulletManager`) into the persistent
level.  In its Details panel:

```
Bullet Classes:    [ BP_BulletNew, BP_BulletHoming, ... ]   ← all subclasses you use
Pool Size Per Class: 500                                     ← tune to your max on-screen count
Overflow Grow By:    64
```

---

## 6  Wire ACPP_Spawner — replace SpawnActor with FireBullet

Find every `SpawnActor` call in your spawner that creates bullets.

**Before:**
```cpp
ACPP_Bullet* Bullet = GetWorld()->SpawnActor<ACPP_Bullet>(
    BulletClass, SpawnLocation, SpawnRotation, SpawnParams);
Bullet->InitializeBullet(MotionParams);
```

**After:**
```cpp
ACPP_BulletManager* Manager = ACPP_BulletManager::Get(this);
if (Manager)
{
    Manager->FireBullet(BulletClass, SpawnLocation, Direction, MotionParams);
}
```

`Direction` is the unit vector you already compute per beam.
`MotionParams` is the `FBulletMotionParams` you already build from the `UFirePattern`.
Nothing else changes in the spawner logic.

---

## 7  Wire the player — replace OnActorBeginOverlap with QueryBulletsAt

**Before (expensive):**
```cpp
// In player Blueprint or C++:
// Box/Capsule has GenerateOverlapEvents=true
// Bound: OnActorBeginOverlap → fires for every bullet that enters proximity
void ACPP_Player::OnBulletOverlap(AActor* OtherActor, ...)
{
    if (Cast<ACPP_Bullet>(OtherActor)) { TakeDamage(...); }
}
```

**After (one call per frame):**
```cpp
// In ACPP_Player::Tick or a sub-tick timer:
void ACPP_Player::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (bInvincible) return;

    ACPP_BulletManager* Manager = ACPP_BulletManager::Get(this);
    if (!Manager) return;

    TArray<ACPP_Bullet*> HitBullets;
    // HitboxRadius = half the player's visual hitbox (typically very small in bullet hells)
    Manager->QueryBulletsAt(GetActorLocation(), HitboxRadius, HitBullets);

    for (ACPP_Bullet* Bullet : HitBullets)
    {
        if (!Bullet) continue;
        OnHitByBullet(Bullet);           // your damage / death logic
        Manager->ReturnBullet(Bullet);   // recycle immediately on hit
        break;                           // one hit per frame (or remove break for grazing)
    }
}
```

Also remove the `OnActorBeginOverlap` binding from the player and turn off
`GenerateOverlapEvents` on the player's collision component for the bullet channel.

---

## 8  Apply BulletMovementComponent patches

See `BulletMovementComponent_Patch.cpp`.  Summary of the 3 changes:

1. `MoveUpdatedComponent(..., false, nullptr, ETeleportType::TeleportPhysics)` — fixes UpdateOverlaps
2. `PrimaryComponentTick.bStartWithTickEnabled = false` + re-enable in `Initialize()` — dormant bullets cost zero tick
3. `OnLifetimeExpired()` calls `Manager->ReturnBullet()` instead of `GetOwner()->Destroy()`

---

## Expected profiler result after all changes

| Entry | Before | After |
|---|---|---|
| UpdateOverlaps | High (per-bullet sweep) | Near zero |
| MoveComponent(scene) | High (per-bullet sweep) | Near zero |
| Begin Actor Overlap | High (per-bullet per-overlap) | Zero |
| Tick time | High (N component ticks) | Low (only active bullets tick; dormant ones don't) |
| World tick time | High (aggregate) | Proportionally reduced |
| SpawnActor / GC | Spiky (per-fire) | Zero at runtime (all pre-spawned) |

The remaining tick cost will be the actual math inside `UBulletMovementComponent::TickComponent`
(speed integration, angular velocity rotation, etc.) — which is unavoidable and will be
proportional to active bullet count.  That is the correct baseline.
