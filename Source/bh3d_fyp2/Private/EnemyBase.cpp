#include "EnemyBase.h"
#include "Kismet/GameplayStatics.h"

AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();
    CurrentHealth = MaxHealth;
    CachedPlayer = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
}

void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    MoveTowardsPlayer(DeltaTime);
}

void AEnemyBase::TakeDamage_Enemy_Implementation(float Damage)
{
    CurrentHealth = FMath::Clamp(CurrentHealth - Damage, 0.f, MaxHealth);
    OnHealthChanged(CurrentHealth, -Damage); // Blueprint reacts visually

    if (CurrentHealth <= 0.f)
    {
        OnDeath(); // Blueprint handles the spectacle
        // Don't Destroy() here — let Blueprint decide timing
        // (e.g. wait for dissolve anim to finish)
    }
}

void AEnemyBase::MoveTowardsPlayer(float DeltaTime)
{
    if (!CachedPlayer)
    {
        UE_LOG(LogTemp, Warning, TEXT("EnemyBase: No player found!"));
        return;
    }

    UE_LOG(LogTemp, Warning, TEXT("EnemyBase: Moving towards player at %s"),
        *CachedPlayer->GetActorLocation().ToString());

    FVector EnemyLoc  = GetActorLocation();
    FVector PlayerLoc = CachedPlayer->GetActorLocation();
    FVector Dir       = (PlayerLoc - EnemyLoc).GetSafeNormal();

    // Sphere cast ahead for obstacle avoidance
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    bool bBlocked = GetWorld()->SweepSingleByChannel(
        Hit, EnemyLoc,
        EnemyLoc + Dir * ProbeDistance,
        FQuat::Identity,
        ECC_WorldStatic,
        FCollisionShape::MakeSphere(ProbeRadius),
        Params
    );

    FVector MoveDir = bBlocked
        ? FVector::VectorPlaneProject(Dir, Hit.Normal).GetSafeNormal()
        : Dir;

    FVector Delta = MoveDir * MoveSpeed * DeltaTime;
    FHitResult MoveHit;
    SetActorLocation(EnemyLoc + Delta, true, &MoveHit);

    if (MoveHit.bBlockingHit)
    {
        FVector Slide = FVector::VectorPlaneProject(
            Delta * (1.f - MoveHit.Time), MoveHit.Normal
        );
        SetActorLocation(GetActorLocation() + Slide, true);
    }
}
