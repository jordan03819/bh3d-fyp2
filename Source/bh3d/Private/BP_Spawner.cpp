// Fill out your copyright notice in the Description page of Project Settings.


#include "BP_Spawner.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"

// Sets default values
ABP_Spawner::ABP_Spawner()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
	
}

// Called when the game starts or when spawned
void ABP_Spawner::BeginPlay()
{
	Super::BeginPlay();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green, TEXT("Hello"));
	}
}
	
// Called every frame
void ABP_Spawner::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

