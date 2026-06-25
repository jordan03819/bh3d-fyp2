// EnemyBase.h

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyBase.generated.h"

class ACPP_Spawner;

UENUM(BlueprintType)
enum class EEnemyState : uint8
{
    Moving,
    Attacking
};

UCLASS()
class YOURGAME_API AEnemyBase : public AActor
{
    GENERATED_BODY()

public:
    AEnemyBase();

    virtual void Tick(float DeltaTime) override;

protected:
    virtual void BeginPlay() override;

    void UpdateMovement(float DeltaTime);
    void UpdateAttack(float DeltaTime);
    void ChooseNewDestination();

protected:

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    ACPP_Spawner* Spawner = nullptr;

    UPROPERTY(EditAnywhere, Category="Movement")
    float MoveSpeed = 300.f;

    UPROPERTY(EditAnywhere, Category="Movement")
    float AcceptanceRadius = 50.f;

    UPROPERTY(EditAnywhere, Category="Movement")
    float RepositionDistance = 400.f;

    UPROPERTY(EditAnywhere, Category="Attack")
    float AttackDuration = 3.f;

    UPROPERTY(BlueprintReadOnly)
    EEnemyState State = EEnemyState::Moving;

    FVector TargetLocation;

    float AttackTimer = 0.f;
};
