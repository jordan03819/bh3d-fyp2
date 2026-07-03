#include "BulletMovementComponent.h"
#include "CPP_Bullet.h"           // for Cast<ACPP_Bullet> in lifetime expiry
#include "CPP_BulletManager.h"    // for ReturnBullet — replaces Destroy()

UBulletMovementComponent::UBulletMovementComponent()
{
    // CHANGED: this component no longer ticks itself. ACPP_BulletManager
    // calls UpdateMotion() directly on every active component from one
    // Actor tick, instead of each bullet registering its own tick function.
    // This removes per-bullet tick-graph overhead (registration, prereq
    // sorting, dispatch) at scale — the math itself was already cheap.
    PrimaryComponentTick.bCanEverTick = false;
}

void UBulletMovementComponent::BeginPlay()
{
    Super::BeginPlay();
    // Tick remains disabled until Initialize() is called.
}

void UBulletMovementComponent::Initialize(const FBulletMotionParams& Params)
{
    MotionParams  = Params;
    ElapsedTime   = 0.f;
    bInitialized  = true;

    // Build the initial velocity from the owner's forward vector.
    // The spawner (or BulletManager::FireBullet) sets the rotation before
    // calling Initialize, so GetActorForwardVector() already points in the
    // intended fire direction.
    if (AActor* Owner = GetOwner())
    {
        // CHANGED: also cache CurrentSpeed so UpdateMotion never calls Velocity.Size().
        CurrentSpeed = MotionParams.InitialSpeed;
        Velocity     = Owner->GetActorForwardVector() * CurrentSpeed;
    }

    // CHANGED: no longer enables component tick here — BulletManager::FireBullet
    // adds this component to its ActiveComponents list right after calling
    // InitializeBullet(), which is what makes it start receiving UpdateMotion().
}

// POOL: called by BulletManager::DeactivateEntry instead of Owner->Destroy().
void UBulletMovementComponent::ResetForPool()
{
    // Clear state so the next Initialize() starts completely clean.
    // CHANGED: no tick to disable here anymore — BulletManager::DeactivateEntry
    // removes this component from ActiveComponents, which stops UpdateMotion
    // calls immediately.
    Velocity     = FVector::ZeroVector;
    CurrentSpeed = 0.f;
    ElapsedTime  = 0.f;
    bInitialized = false;
}

void UBulletMovementComponent::UpdateMotion_Implementation(float DeltaTime)
{
    if (!bInitialized) return;

    AActor* Owner = GetOwner();
    if (!Owner) return;

    ElapsedTime += DeltaTime;

    // ── Lifetime check ────────────────────────────────────────────────────
    if (MotionParams.Lifetime > 0.f && ElapsedTime >= MotionParams.Lifetime)
    {
        // CHANGED: Return to pool instead of Destroy().
        // Destroy() is never called at runtime — BulletManager reuses the actor.
        // Falls back to Destroy() if no manager exists (e.g. PIE without manager).
        ACPP_BulletManager* Manager = ACPP_BulletManager::Get(this);
        if (Manager)
        {
            Manager->ReturnBullet(Cast<ACPP_Bullet>(Owner));
        }
        else
        {
            Owner->Destroy();
        }
        return;
    }

    // ── Speed ─────────────────────────────────────────────────────────────
    // CHANGED: Use cached CurrentSpeed — avoids Velocity.Size() (sqrt) every tick.
    // Previously: float Speed = Velocity.Length();
    if (MotionParams.LinearAcceleration != 0.f)
    {
        CurrentSpeed += MotionParams.LinearAcceleration * DeltaTime;
        CurrentSpeed  = FMath::Max(CurrentSpeed, 0.f);

        if (MotionParams.MaxSpeed > 0.f)
            CurrentSpeed = FMath::Min(CurrentSpeed, MotionParams.MaxSpeed);
    }

    // ── Direction (angular velocity) ──────────────────────────────────────
    // Rotate the velocity direction without changing speed.
    // AngularVelocity is in degrees/second — scale by DeltaTime for this frame.
    if (!MotionParams.AngularVelocity.IsZero())
    {
        FRotator FrameRot = MotionParams.AngularVelocity * DeltaTime;
        // CHANGED: Velocity.GetSafeNormal() dropped from the else branch (was a
        // wasted sqrt when no rotation). Here it's still needed to get the unit
        // direction before rotating, but only runs on the angular-velocity path.
        FVector Dir = FrameRot.RotateVector(Velocity.GetSafeNormal());
        Velocity    = Dir * CurrentSpeed;
    }
    else
    {
        // No rotation — direction is fixed. Reapply speed to the existing direction
        // using the cached CurrentSpeed, no normalise needed.
        // Previously: Velocity = Velocity.GetSafeNormal() * Speed;  ← sqrt every frame
        Velocity = Velocity.GetUnsafeNormal() * CurrentSpeed;
        // GetUnsafeNormal is safe here: Velocity was set from a valid direction in
        // Initialize() and only ever scaled, never zeroed in this code path.
    }

    // ── Position + Rotation ───────────────────────────────────────────────
    const FVector NewLocation = Owner->GetActorLocation() + Velocity * DeltaTime;

    // ── World border cull ─────────────────────────────────────────────────
    // Half-extents match the blocking volume: scale 3000,3000,2250 on a unit box.
    // Checked against NewLocation so we cull before the SetActorLocation call,
    // costing no extra position read — we already have the value.
    if (FMath::Abs(NewLocation.X) > 3000.f ||
        FMath::Abs(NewLocation.Y) > 3000.f ||
        FMath::Abs(NewLocation.Z) > 2250.f)
    {
        ACPP_BulletManager* Manager = ACPP_BulletManager::Get(this);
        if (Manager)
            Manager->ReturnBullet(Cast<ACPP_Bullet>(Owner));
        else
            Owner->Destroy();
        return;
    }

    // CHANGED: was two separate calls:
    //   Owner->SetActorLocation(NewLocation);             ← ETeleportType::None → UpdateOverlaps
    //   Owner->SetActorRotation(Velocity.Rotation());     ← ETeleportType::None → UpdateOverlaps again
    //
    // SetActorLocationAndRotation with TeleportPhysics does ONE transform update
    // and skips UpdateOverlaps entirely. This is the fix for the three profiler
    // entries: "UpdateOverlaps", "MoveComponent(scene comp)", and the overlap
    // portion of "World tick time".
    //
    // Collision detection is now handled by BulletManager::QueryBulletsAt(),
    // called once per frame from the player/enemy — not by per-bullet overlap events.
    if (!Velocity.IsNearlyZero())
    {
        Owner->SetActorLocationAndRotation(
            NewLocation,
            Velocity.Rotation(),
            /*bSweep=*/false,
            /*OutSweepHitResult=*/nullptr,
            ETeleportType::TeleportPhysics    // ← skips UpdateOverlaps
        );
    }
    else
    {
        Owner->SetActorLocation(
            NewLocation,
            /*bSweep=*/false,
            /*OutSweepHitResult=*/nullptr,
            ETeleportType::TeleportPhysics
        );
    }
}
