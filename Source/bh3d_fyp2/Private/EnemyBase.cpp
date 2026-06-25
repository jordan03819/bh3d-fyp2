#include "EnemyBase.h"
#include "CPP_Spawner.h"
#include "MovementSequence.h"

#include "Components/ChildActorComponent.h"

AEnemyBase::AEnemyBase()
{
    PrimaryActorTick.bCanEverTick = true;

    RootComponent =
        CreateDefaultSubobject<USceneComponent>(
            TEXT("Root"));

    SpawnerComponent =
        CreateDefaultSubobject<UChildActorComponent>(
            TEXT("Spawner"));

    SpawnerComponent->SetupAttachment(RootComponent);
}

void AEnemyBase::BeginPlay()
{
    Super::BeginPlay();

    SpawnLocation = GetActorLocation();

    Spawner =
        Cast<ACPP_Spawner>(
            SpawnerComponent->GetChildActor());

    CurrentPoint = 0;

    State = EEnemyState::Moving;

    if (Spawner)
    {
        Spawner->SetPaused(true);
    }
}

void AEnemyBase::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!MovementSequence)
    {
        return;
    }

    if (!MovementSequence->Points.IsValidIndex(CurrentPoint))
    {
        return;
    }

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
    const FMovementPoint& Point =
        MovementSequence->Points[CurrentPoint];

    FVector Target =
        SpawnLocation + Point.Offset;

    FVector ToTarget =
        Target - GetActorLocation();

    float Distance =
        ToTarget.Size();

    if (Distance <= AcceptanceRadius)
    {
        SetActorLocation(Target);

        AttackTimer = 0.f;

        State = EEnemyState::Attacking;

        if (Spawner)
        {
            Spawner->SetPaused(false);
        }

        return;
    }

    FVector Direction =
        ToTarget.GetSafeNormal();

    float MoveDistance =
        FMath::Min(
            MoveSpeed * DeltaTime,
            Distance);

    AddActorWorldOffset(
        Direction * MoveDistance,
        true);
}

void AEnemyBase::UpdateAttack(float DeltaTime)
{
    const FMovementPoint& Point =
        MovementSequence->Points[CurrentPoint];

    AttackTimer += DeltaTime;

    if (AttackTimer >= Point.AttackDuration)
    {
        CurrentPoint++;

        if (!MovementSequence->Points.IsValidIndex(CurrentPoint))
        {
            Destroy();
            return;
        }

        State = EEnemyState::Moving;

        if (Spawner)
        {
            Spawner->SetPaused(true);
        }
    }
}
