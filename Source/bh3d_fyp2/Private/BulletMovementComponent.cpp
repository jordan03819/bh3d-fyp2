#include "BulletMovementComponent.h"

UBulletMovementComponent::UBulletMovementComponent()
{
    PrimaryComponentTick.bCanEverTick = true;

    // Disable tick until the component is initialized by the spawner.
    // This prevents UpdateMotion running on an uninitialized bullet.
    PrimaryComponentTick.bStartWithTickEnabled = false;
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
    // The spawner sets the rotation before calling Initialize, so
    // GetActorForwardVector() already points in the intended fire direction.
    if (AActor* Owner = GetOwner())
    {
        Velocity = Owner->GetActorForwardVector() * MotionParams.InitialSpeed;
    }

    // Now safe to tick.
    SetComponentTickEnabled(true);
}

void UBulletMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    UpdateMotion(DeltaTime);
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
        Owner->Destroy();
        return;
    }

    // ── Speed ─────────────────────────────────────────────────────────────
    // Extract current speed, apply acceleration, clamp if needed.
    float Speed = Velocity.Length();

    if (MotionParams.LinearAcceleration != 0.f)
    {
        Speed += MotionParams.LinearAcceleration * DeltaTime;
        Speed  = FMath::Max(Speed, 0.f);

        if (MotionParams.MaxSpeed > 0.f)
            Speed = FMath::Min(Speed, MotionParams.MaxSpeed);
    }

    // ── Direction (angular velocity) ──────────────────────────────────────
    // Rotate the velocity direction without changing speed.
    // AngularVelocity is in degrees/second — scale by DeltaTime for this frame.
    if (!MotionParams.AngularVelocity.IsZero())
    {
        FRotator FrameRot = MotionParams.AngularVelocity * DeltaTime;
        FVector  Dir      = FrameRot.RotateVector(Velocity.GetSafeNormal());
        Velocity = Dir * Speed;
    }
    else
    {
        // No rotation — just reapply possibly-changed speed to the same direction.
        Velocity = Velocity.GetSafeNormal() * Speed;
    }

    // ── Position ──────────────────────────────────────────────────────────
    FVector NewLocation = Owner->GetActorLocation() + Velocity * DeltaTime;
    Owner->SetActorLocation(NewLocation);

    // Keep actor rotation aligned with travel direction (matters for
    // child components like meshes and hit shapes that rely on forward vector).
    if (!Velocity.IsNearlyZero())
        Owner->SetActorRotation(Velocity.Rotation());
}
