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
	WaitingForPhase
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

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	class UChildActorComponent* SpawnerComponent;

	UPROPERTY(BlueprintReadOnly, Category = "Attack")
	ACPP_Spawner* Spawner;

	void UpdateMovement(float DeltaTime);
	void UpdateWaiting(float DeltaTime);
	void UpdateWaitingForPhase(float DeltaTime);
	void ComputeTarget(const FMovementPoint& Point, FVector& OutTarget);
	bool IsCurrentAttackPhaseComplete() const;
	void SkipToNextAttackPhase();
	void PauseAttackSequence();
	void ResumeAttackSequence();

private:
	float AttackTimer = 0.f;
};
