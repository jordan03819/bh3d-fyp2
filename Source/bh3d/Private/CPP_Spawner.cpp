// Fill out your copyright notice in the Description page of Project Settings.

#include "CPP_Spawner.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ACPP_Spawner::ACPP_Spawner()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACPP_Spawner::BeginPlay()
{
    Super::BeginPlay();



   // Set timer by event (FireBullet)
   // GetWorld()->GetTimerManager().SetTimer(
   //     FireTimerHandle,
   //     this,
   //     &ACPP_Spawner::FireBullet,
   //     FireRate,
   //     true
   // );

}

void ACPP_Spawner::Tick(float DeltaTime)
{
    // DONT TOUCH THIS PART
    Super::Tick(DeltaTime);
    SpawnLocation = GetActorLocation();
    // DONT TOUCH THIS PART
    
    TimeSinceStart += DeltaTime;
    if (TimeSinceStart >= NextFireTime) {
	    if (ShotsRemaining != 0) {
			SpawnBullets();

			if (ShotsRemaining > 0)
				ShotsRemaining--;

			NextFireTime += IntervalBetweenShots;
	    }
    } // is this still necessary?
}

void ACPP_Spawner::SpawnBullets()
{
    if (!BulletClass) return;

    int count = 16;
    float step = 2 * PI / count;

    for (int i = 0; i < count; i++) {
	float angle = i * step;

	FVector dir = FVector(FMath::Cos(angle),FMath::Sin(angle),0.f);

	FRotator rotation = dir.Rotation();

	GetWorld()->SpawnActor<AActor>(
    	    BulletClass,
    	    SpawnLocation,
    	    rotation
	);
    }
}
