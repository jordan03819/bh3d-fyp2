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

	bAttackPaused = true;
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

		case EEnemyState::WaitingForPhase:
			UpdateWaitingForPhase(DeltaTime);
			break;

		case EEnemyState::Attacking:
			UpdateWaiting(DeltaTime);
			break;
	}
}

void AEnemyBase::ComputeTarget(const FMovementPoint& Point, FVector& OutTarget)
{
	switch (Point.Reference)
	{
		case EMovementReference::Origin:
			// Origin-relative: CurrentOrigin + offset
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

void AEnemyBase::PauseAttackSequence()
{
	if (!bAttackPaused && Spawner)
	{
		bAttackPaused = true;
		Spawner->SetPaused(true);
	}
}

void AEnemyBase::ResumeAttackSequence()
{
	if (bAttackPaused && Spawner)
	{
		bAttackPaused = false;
		Spawner->SetPaused(false);
	}
}

bool AEnemyBase::IsCurrentAttackPhaseComplete() const
{
	if (!Spawner)
	{
		return true;
	}

	// TODO: Implement phase completion check in CPP_Spawner
	// For now, assume there's a method to check current phase status
	// return Spawner->IsCurrentPhaseComplete();
	return true;
}

void AEnemyBase::SkipToNextAttackPhase()
{
	if (!Spawner)
	{
		return;
	}

	// TODO: Implement phase skip in CPP_Spawner
	// Spawner->SkipToNextPhase();
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

	// Handle Skip command
	if (Point.Command == EMovementCommand::Skip)
	{
		SkipToNextAttackPhase();
		CurrentPoint++;

		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
		{
			Destroy();
			return;
		}

		return;
	}

	// Handle Wait command
	if (Point.Command == EMovementCommand::Wait)
	{
		// Resume attack for wait commands
		ResumeAttackSequence();

		AttackTimer = 0.f;
		State = EEnemyState::Waiting;
		return;
	}

	// Handle MoveTo command
	// Manage attack pause/resume based on bAttackWhileMoving
	if (!Point.bAttackWhileMoving)
	{
		PauseAttackSequence();
	}
	else
	{
		ResumeAttackSequence();
	}

	FVector Target;
	ComputeTarget(Point, Target);

	FVector ToTarget = Target - GetActorLocation();
	float Distance = ToTarget.Size();

	if (Distance <= AcceptanceRadius)
	{
		// Arrive at target
		SetActorLocation(Target);
		AttackTimer = 0.f;

		// Determine next state based on attack duration type
		if (Point.DurationType == EAttackDurationType::UntilPhase)
		{
			// Wait for phase to complete
			ResumeAttackSequence();
			State = EEnemyState::WaitingForPhase;
		}
		else
		{
			// Fixed duration
			ResumeAttackSequence();
			State = EEnemyState::Attacking;
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

	float Duration = Point.AttackDuration;

	if (AttackTimer >= Duration)
	{
		CurrentPoint++;

		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
		{
			Destroy();
			return;
		}

		State = EEnemyState::Moving;
	}
}

void AEnemyBase::UpdateWaitingForPhase(float DeltaTime)
{
	if (IsCurrentAttackPhaseComplete())
	{
		CurrentPoint++;

		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
		{
			Destroy();
			return;
		}

		State = EEnemyState::Moving;
	}
}
