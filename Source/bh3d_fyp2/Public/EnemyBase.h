#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EnemyBase.generated.h"

UCLASS()
class BH3D_FYP2_API AEnemyBase : public AActor
{
    GENERATED_BODY()

public:
    AEnemyBase();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

    // -------------------------------------------------------
    // HEALTH — editable per Blueprint, per instance
    // -------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Stats")
    float MaxHealth = 100.f;

    UPROPERTY(BlueprintReadOnly, Category = "Enemy|Stats")
    float CurrentHealth;

    // -------------------------------------------------------
    // MOVEMENT — tweak per enemy type in Blueprint
    // -------------------------------------------------------
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
    float MoveSpeed = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
    float ProbeDistance = 150.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Enemy|Movement")
    float ProbeRadius = 40.f;

    // -------------------------------------------------------
    // EVENTS — Blueprint implements the visual/sound response
    // -------------------------------------------------------

    // Called when health changes — Blueprint hooks in VFX, UI, sound
    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Events")
    void OnHealthChanged(float NewHealth, float Delta);

    // Called on death — Blueprint handles ragdoll, dissolve, drops
    UFUNCTION(BlueprintImplementableEvent, Category = "Enemy|Events")
    void OnDeath();

    // Optional: Blueprint can override AND call parent logic
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Enemy|Combat")
    void TakeDamage_Enemy(float Damage);
    
    virtual void TakeDamage_Enemy_Implementation(float Damage);

private:
    void MoveTowardsPlayer(float DeltaTime);
    AActor* CachedPlayer;
};
