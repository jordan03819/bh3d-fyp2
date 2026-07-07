#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FMovementPoint.h"
#include "MovementSequence.h"
#include "EnemyBase.generated.h"

class ACPP_Spawner;

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
	Moving,
	Attacking,
	Waiting,
	WaitingForPhase,
	// Sequence finished: moving back toward SpawnOrigin over a fixed duration.
	ReturningToSpawn
};

UCLASS()
class BH3D_FYP2_API AEnemyBase : public AActor
{
	GENERATED_BODY()

public:
	AEnemyBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	UMovementSequence* MovementSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float MoveSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float AcceptanceRadius = 50.f;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	int32 CurrentPoint = 0;

	UPROPERTY(BlueprintReadOnly, Category = "State")
	EEnemyState State = EEnemyState::Moving;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector SpawnOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Movement")
	FVector CurrentOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Attack")
	bool bAttackPaused = false;

	// If true, restores the old behavior: the enemy is destroyed immediately
	// once it runs out of movement points. If false (default), it idles,
	// returns to SpawnOrigin, resets its attack sequence, and loops.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Completion")
	bool bDestroyOnSequenceComplete = false;

	// How long the return-to-spawn move takes, in seconds.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Sequence Completion")
	float ReturnToSpawnDuration = 5.f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UChildActorComponent* SpawnerComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Attack")
	ACPP_Spawner* Spawner;

	void UpdateMovement(float DeltaTime);
	void UpdateWaiting(float DeltaTime);
	void UpdateWaitingForPhase(float DeltaTime);
	void UpdateReturningToSpawn(float DeltaTime);
	void ComputeTarget(const FMovementPoint& Point, FVector& OutTarget);
	bool IsCurrentAttackPhaseComplete() const;
	void SkipToNextAttackPhase();
	void PauseAttackSequence();
	void ResumeAttackSequence();

	// Called whenever CurrentPoint runs off the end of the movement sequence.
	// Replaces the old unconditional Destroy() call site-by-site.
	void HandleSequenceComplete();

	// Resets CurrentPoint, CurrentOrigin, and the spawner's attack sequence
	// back to their starting state, then re-enters Moving to loop the
	// choreography from the top.
	void ResetAttackSequence();

private:
	float AttackTimer = 0.f;

	// Elapsed time since ReturningToSpawn began.
	float ReturnTimer = 0.f;

	// Actor location at the moment ReturningToSpawn began; lerp start point.
	FVector ReturnStartLocation = FVector::ZeroVector;

	// True after we've called WaitForPhaseCompletion() on the spawner.
	// Cleared when the spawner's flag is cleared by AdvancePhase(), which
	// is the signal that the phase we were waiting for has finished.
	bool bPhaseWaitRegistered = false;
};
