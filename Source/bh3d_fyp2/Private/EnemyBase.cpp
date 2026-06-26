#include "EnemyBase.h"
#include "CPP_Spawner.h"
#include "Components/ChildActorComponent.h"

AEnemyBase::AEnemyBase()
{
	PrimaryActorTick.bCanEverTick = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	SpawnerComponent = CreateDefaultSubobject<UChildActorComponent>(TEXT("Spawner"));
	SpawnerComponent->SetupAttachment(RootComponent);
}

void AEnemyBase::BeginPlay()
{
	Super::BeginPlay();

	Spawner = Cast<ACPP_Spawner>(SpawnerComponent->GetChildActor());

	CurrentPoint = 0;
	State = EEnemyState::Moving;

	// Initialize both origins to spawn location
	SpawnOrigin = GetActorLocation();
	CurrentOrigin = SpawnOrigin;

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

	if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
	{
		return;
	}

	switch (State)
	{
		case EEnemyState::Moving:
			UpdateMovement(DeltaTime);
			break;

		case EEnemyState::Waiting:
			UpdateWaiting(DeltaTime);
			break;

		case EEnemyState::Attacking:
			UpdateWaiting(DeltaTime);  // Same behavior for now
			break;
	}
}

void AEnemyBase::ComputeTarget(const FMovementPoint& Point, FVector& OutTarget)
{
	switch (Point.Reference)
	{
		case EMovementReference::Origin:
			// Origin-relative: SpawnOrigin + offset
			OutTarget = CurrentOrigin + Point.Location;
			break;

		case EMovementReference::Move:
			// Move-relative: CurrentPosition + offset
			OutTarget = GetActorLocation() + Point.Location;
			break;

		case EMovementReference::Absolute:
			// Absolute: world coordinate
			OutTarget = Point.Location;
			break;
	}
}

void AEnemyBase::UpdateMovement(float DeltaTime)
{
	const FMovementPoint& Point = MovementSequence->MovementPoints[CurrentPoint];

	// Handle SetOrigin command
	if (Point.Command == EMovementCommand::SetOrigin)
	{
		CurrentOrigin = GetActorLocation();
		CurrentPoint++;

		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
		{
			Destroy();
			return;
		}

		// Don't change state, continue processing next point
		return;
	}

	// Handle Wait command
	if (Point.Command == EMovementCommand::Wait)
	{
		AttackTimer = 0.f;
		State = EEnemyState::Waiting;

		if (Spawner)
		{
			Spawner->SetPaused(false);
		}

		return;
	}

	// Handle MoveTo command
	FVector Target;
	ComputeTarget(Point, Target);

	FVector ToTarget = Target - GetActorLocation();
	float Distance = ToTarget.Size();

	if (Distance <= AcceptanceRadius)
	{
		// Arrive at target
		SetActorLocation(Target);
		AttackTimer = 0.f;
		State = EEnemyState::Attacking;

		if (Spawner)
		{
			Spawner->SetPaused(false);
		}

		return;
	}

	// Move toward target
	FVector Direction = ToTarget.GetSafeNormal();
	float MoveDistance = FMath::Min(MoveSpeed * DeltaTime, Distance);

	AddActorWorldOffset(Direction * MoveDistance, true);
}

void AEnemyBase::UpdateWaiting(float DeltaTime)
{
	const FMovementPoint& Point = MovementSequence->MovementPoints[CurrentPoint];

	AttackTimer += DeltaTime;

	if (AttackTimer >= Point.AttackDuration)
	{
		CurrentPoint++;

		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
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
