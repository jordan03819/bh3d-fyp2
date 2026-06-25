#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyBase.generated.h"

class UChildActorComponent;
class UMovementSequence;
class ACPP_Spawner;

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Moving,
    Attacking
};

UCLASS()
class BH3D_FYP2_API AEnemyBase : public AActor
{
    GENERATED_BODY()

public:
    AEnemyBase();

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

    void UpdateMovement(float DeltaTime);
    void UpdateAttack(float DeltaTime);

protected:

    UPROPERTY(VisibleAnywhere)
    UChildActorComponent* SpawnerComponent;

    UPROPERTY(BlueprintReadOnly)
    ACPP_Spawner* Spawner = nullptr;

    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="Movement")
    UMovementSequence* MovementSequence = nullptr;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Movement")
    float MoveSpeed = 300.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Movement")
    float AcceptanceRadius = 25.f;

    UPROPERTY(BlueprintReadOnly)
    EEnemyState State = EEnemyState::Moving;

    UPROPERTY(BlueprintReadOnly)
    int32 CurrentPoint = 0;

    FVector SpawnLocation;

    float AttackTimer = 0.f;
};
