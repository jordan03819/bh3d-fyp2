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

	// ReturningToSpawn runs after CurrentPoint has already run off the end
	// of MovementPoints, so it must not be gated behind the
	// IsValidIndex(CurrentPoint) check used by the other states below.
	if (State == EEnemyState::ReturningToSpawn)
	{
		UpdateReturningToSpawn(DeltaTime);
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

		default:
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

	// The spawner clears bWaitingForPhaseCompletion in AdvancePhase().
	// We registered via WaitForPhaseCompletion() on arrival, so if the flag
	// is now false, the phase has advanced and we can proceed.
	return !Spawner->IsWaitingForPhaseCompletion();
}

void AEnemyBase::SkipToNextAttackPhase()
{
	if (!Spawner)
	{
		return;
	}

	Spawner->SkipToNextPhase();
}

void AEnemyBase::UpdateWaitingForPhase(float DeltaTime)
{
	if (!Spawner)
	{
		// No spawner — just advance
		CurrentPoint++;
		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
		{
			HandleSequenceComplete();
			return;
		}
		bPhaseWaitRegistered = false;
		State = EEnemyState::Moving;
		return;
	}

	if (!bPhaseWaitRegistered)
	{
		// First tick in this state: register the wait with the spawner.
		// WaitForPhaseCompletion() sets bWaitingForPhaseCompletion = true.
		// AdvancePhase() clears it — that's the signal the phase has finished.
		Spawner->WaitForPhaseCompletion();
		bPhaseWaitRegistered = true;
		return;
	}

	// Flag was cleared by AdvancePhase() — phase is done, advance movement.
	if (!Spawner->IsWaitingForPhaseCompletion())
	{
		bPhaseWaitRegistered = false;
		CurrentPoint++;

		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
		{
			HandleSequenceComplete();
			return;
		}

		State = EEnemyState::Moving;

		// Peek at next point to set correct pause state
		const FMovementPoint& Next = MovementSequence->MovementPoints[CurrentPoint];
		if (Next.Command == EMovementCommand::MoveTo && Next.bAttackWhileMoving)
		{
			ResumeAttackSequence();
		}
		else
		{
			PauseAttackSequence();
		}
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
			HandleSequenceComplete();
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
			HandleSequenceComplete();
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
	// Note: pause/resume is set once on transition into Moving state (from
	// UpdateWaiting/UpdateWaitingForPhase), not re-applied every frame.
	// If bAttackWhileMoving is true, the spawner was unpaused before entering
	// this state; if false, it was paused.

	FVector Target;
	ComputeTarget(Point, Target);

	FVector ToTarget = Target - GetActorLocation();
	float Distance = ToTarget.Size();

	if (Distance <= AcceptanceRadius)
	{
		// Arrived at waypoint — always unpause to attack here
		SetActorLocation(Target);
		AttackTimer = 0.f;
		ResumeAttackSequence();

		if (Point.DurationType == EAttackDurationType::UntilPhase)
		{
			State = EEnemyState::WaitingForPhase;
		}
		else
		{
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

	if (AttackTimer >= Point.AttackDuration)
	{
		CurrentPoint++;

		if (!MovementSequence->MovementPoints.IsValidIndex(CurrentPoint))
		{
			HandleSequenceComplete();
			return;
		}

		State = EEnemyState::Moving;

		// Set correct pause state for the upcoming movement.
		// Peek at next point: if it's a MoveTo with parallel=1, keep firing;
		// otherwise pause. For non-MoveTo commands (setorigin, skip, wait)
		// we default to pausing — UpdateMovement will correct it immediately.
		const FMovementPoint& Next = MovementSequence->MovementPoints[CurrentPoint];
		if (Next.Command == EMovementCommand::MoveTo && Next.bAttackWhileMoving)
		{
			ResumeAttackSequence();
		}
		else
		{
			PauseAttackSequence();
		}
	}
}

void AEnemyBase::HandleSequenceComplete()
{
	if (bDestroyOnSequenceComplete)
	{
		// Old behavior: disappear immediately.
		Destroy();
		return;
	}

	// Stop attacking, then head straight back to SpawnOrigin over
	// ReturnToSpawnDuration seconds — no firing during the return.
	PauseAttackSequence();

	ReturnTimer = 0.f;
	ReturnStartLocation = GetActorLocation();
	State = EEnemyState::ReturningToSpawn;
}

void AEnemyBase::UpdateReturningToSpawn(float DeltaTime)
{
	ReturnTimer += DeltaTime;

	const float Duration = FMath::Max(ReturnToSpawnDuration, KINDA_SMALL_NUMBER);
	const float Alpha = FMath::Clamp(ReturnTimer / Duration, 0.f, 1.f);

	const FVector NewLocation = FMath::Lerp(ReturnStartLocation, SpawnOrigin, Alpha);
	SetActorLocation(NewLocation);

	if (Alpha >= 1.f)
	{
		ResetAttackSequence();
	}
}

void AEnemyBase::ResetAttackSequence()
{
	CurrentPoint = 0;
	CurrentOrigin = SpawnOrigin;
	bPhaseWaitRegistered = false;
	AttackTimer = 0.f;

	if (Spawner)
	{
		// SetSequence() re-runs StartSequence(), resetting phase index,
		// shot counter, spin angle, and phase timer back to phase 0.
		Spawner->SetSequence(Spawner->Sequence);
	}

	// Stay paused — UpdateMovement will resume the spawner itself once it
	// reaches the first attack point, same as on initial BeginPlay.
	State = EEnemyState::Moving;
}
