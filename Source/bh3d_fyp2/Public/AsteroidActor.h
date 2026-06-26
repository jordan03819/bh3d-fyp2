#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AsteroidActor.generated.h"

UCLASS()
class BH3D_FYP2_API AAsteroidActor : public AActor
{
	GENERATED_BODY()

public:
	AAsteroidActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// Static mesh component with physics
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Asteroid")
	class UStaticMeshComponent* AsteroidMesh;

	// Asteroid parameters (editable in blueprint or per-instance)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Physics")
	float MinLinearSpeed = 200.0f; // cm/s
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Physics")
	float MaxLinearSpeed = 500.0f; // cm/s
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Physics")
	float MinAngularSpeed = 30.0f; // deg/s
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Physics")
	float MaxAngularSpeed = 120.0f; // deg/s
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Physics")
	float AsteroidMass = 100.0f; // kg
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Physics")
	bool bMaintainConstantSpeed = false; // Optional: keep speed from drifting
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Asteroid|Physics")
	float SpeedTolerance = 5.0f; // Only correct if speed drops below this threshold

	// Stored velocities for maintenance loop
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Asteroid|Debug")
	FVector InitialLinearVelocity;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Asteroid|Debug")
	FVector InitialAngularVelocity;

	// Collision callbacks
	UFUNCTION()
	void OnAsteroidHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

protected:
	// Initialize velocity and spin (called from BeginPlay)
	void InitializePhysics();
	
	// Maintain constant speed (called from Tick if bMaintainConstantSpeed is true)
	void MaintainConstantSpeed(float DeltaTime);
};
