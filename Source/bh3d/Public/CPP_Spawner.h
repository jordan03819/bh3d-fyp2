// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CPP_Spawner.generated.h"

UCLASS()
class BH3D_API ACPP_Spawner : public AActor
{
    GENERATED_BODY()
    
public:	
    ACPP_Spawner();

protected:
    virtual void BeginPlay() override;

public:	
    virtual void Tick(float DeltaTime) override;

    // Variables
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FVector SpawnLocation;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRotator SpawnRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSubclassOf<AActor> BulletClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentHealth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float FireRate = 1.0f;

    // Timing
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float TimeSinceStart = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float NextFireTime = 0.f;

    // Shot control
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 ShotsRemaining = -1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float IntervalBetweenShots = 1.0f;

    // Function
    void SpawnBullets();//const FirePattern& pattern);
};
