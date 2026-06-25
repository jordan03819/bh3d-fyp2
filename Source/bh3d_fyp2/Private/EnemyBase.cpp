// EnemyBase.cpp

#include "EnemyBase.h"
#include "CPP_Spawner.h"

AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = true;
}

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    ChooseNewDestination();

    State = EEnemyState::Moving;

    if (Spawner)
    {
        Spawner->SetAttackPaused(true);
    }
}

void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    switch (State)
    {
        case EEnemyState::Moving:
            UpdateMovement(DeltaTime);
            break;

        case EEnemyState::Attacking:
            UpdateAttack(DeltaTime);
            break;
    }
}

void AEnemyBase::UpdateMovement(float DeltaTime)
{
    if (Spawner)
    {
        Spawner->SetAttackPaused(true);
    }

    FVector Direction =
        (TargetLocation - GetActorLocation()).GetSafeNormal();

    AddActorWorldOffset(
        Direction * MoveSpeed * DeltaTime,
        true);

    float Distance =
        FVector::Dist(GetActorLocation(), TargetLocation);

    if (Distance <= AcceptanceRadius)
    {
        AttackTimer = 0.f;
        State = EEnemyState::Attacking;

        if (Spawner)
        {
            Spawner->SetAttackPaused(false);
        }
    }
}

void AEnemyBase::UpdateAttack(float DeltaTime)
{
    if (Spawner)
    {
        Spawner->SetAttackPaused(false);
    }

    AttackTimer += DeltaTime;

    if (AttackTimer >= AttackDuration)
    {
        ChooseNewDestination();

        State = EEnemyState::Moving;

        if (Spawner)
        {
            Spawner->SetAttackPaused(true);
        }
    }
}

void AEnemyBase::ChooseNewDestination()
{
    FVector Direction = FMath::VRand();

    Direction.Z = 0.f;
    Direction.Normalize();

    TargetLocation =
        GetActorLocation() +
        Direction * RepositionDistance;
}
