#include "AsteroidActor.h"
#include "Components/StaticMeshComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "Engine/World.h"

AAsteroidActor::AAsteroidActor()
{
	PrimaryActorTick.TickInterval = 0.0f;
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.016f;

	// Create and configure the mesh component
	AsteroidMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("AsteroidMesh"));
	RootComponent = AsteroidMesh;

	// Physics settings — safe in constructor
	AsteroidMesh->SetSimulatePhysics(true);
	AsteroidMesh->SetEnableGravity(false);
	
	// Collision settings — safe in constructor
	AsteroidMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	AsteroidMesh->SetCollisionObjectType(ECC_WorldDynamic); // Physics Actor
	AsteroidMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	AsteroidMesh->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block); // Bounce off level walls
	AsteroidMesh->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block); // Bounce off dynamic objects

	// Generate hit events
	AsteroidMesh->SetGenerateOverlapEvents(false);


	// NOTE: All BodyInstance access deferred to BeginPlay (via InitializePhysics)
	// because accessing BodyInstance in the constructor triggers
	// FBodyInstance::GetSimplePhysicalMaterial during CDO construction,
	// before GEngine is initialized.

	// Bind collision callback
	AsteroidMesh->OnComponentHit.AddDynamic(this, &AAsteroidActor::OnAsteroidHit);
}

void AAsteroidActor::BeginPlay()
{
	Super::BeginPlay();
	AsteroidMesh->SetMassScale(NAME_None, AsteroidMass);
	InitializePhysics();
}

void AAsteroidActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bMaintainConstantSpeed)
	{
		MaintainConstantSpeed(DeltaTime);
	}
}

void AAsteroidActor::InitializePhysics()
{
	if (!AsteroidMesh)
	{
		return;
	}

	// NOW safe to access BodyInstance (GEngine is initialized)
	
	// Unlock all translation and rotation axes
	AsteroidMesh->BodyInstance.bLockXTranslation = false;
	AsteroidMesh->BodyInstance.bLockYTranslation = false;
	AsteroidMesh->BodyInstance.bLockZTranslation = false;
	AsteroidMesh->BodyInstance.bLockXRotation = false;
	AsteroidMesh->BodyInstance.bLockYRotation = false;
	AsteroidMesh->BodyInstance.bLockZRotation = false;

	// Damping (critical for space behavior)
	AsteroidMesh->BodyInstance.LinearDamping = 0.0f; // No velocity decay
	AsteroidMesh->BodyInstance.AngularDamping = 0.0f; // No rotation decay

	// Max angular velocity (prevent physics engine from clamping spins)
	AsteroidMesh->BodyInstance.MaxAngularVelocity = 500.0f;

	// Random linear velocity
	FVector RandomDirection = FMath::VRand(); // Random unit vector
	float RandomSpeed = FMath::RandRange(MinLinearSpeed, MaxLinearSpeed);
	InitialLinearVelocity = RandomDirection * RandomSpeed;

	// Apply linear velocity
	AsteroidMesh->SetPhysicsLinearVelocity(InitialLinearVelocity);

	// Random angular velocity (spin)
	FVector RandomSpinAxis = FMath::VRand(); // Random unit vector for rotation axis
	float RandomAngularSpeed = FMath::RandRange(MinAngularSpeed, MaxAngularSpeed);
	InitialAngularVelocity = RandomSpinAxis * RandomAngularSpeed;

	// Apply angular velocity (in degrees per second)
	AsteroidMesh->SetPhysicsAngularVelocityInDegrees(InitialAngularVelocity);

	if (AsteroidMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("Asteroid initialized: Linear Vel = %s, Angular Vel = %s"),
			*InitialLinearVelocity.ToString(),
			*InitialAngularVelocity.ToString());
	}
}

void AAsteroidActor::MaintainConstantSpeed(float DeltaTime)
{
	if (!AsteroidMesh)
	{
		return;
	}

	FVector CurrentVelocity = AsteroidMesh->GetPhysicsLinearVelocity();
	float CurrentSpeed = CurrentVelocity.Length();
	float TargetSpeed = InitialLinearVelocity.Length();

	// Only correct if speed drops below threshold
	if (CurrentSpeed < (TargetSpeed - SpeedTolerance))
	{
		float SpeedDeficit = TargetSpeed - CurrentSpeed;
		FVector CorrectionDirection = CurrentVelocity.GetSafeNormal();
		FVector Impulse = CorrectionDirection * SpeedDeficit * AsteroidMesh->GetMass();

		AsteroidMesh->AddImpulse(Impulse);
	}
}

void AAsteroidActor::OnAsteroidHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// Collision happens automatically via physics engine
	// Restitution 0.95 handles bouncing
	// Torque from off-center impacts is applied automatically
	// Override this function if you need custom behavior (sounds, particle effects, damage, etc.)

	if (OtherActor && OtherActor != this)
	{
		// Log for debugging
		UE_LOG(LogTemp, Warning, TEXT("Asteroid hit: %s"), *OtherActor->GetName());

		// Example: Play a sound or particle effect here
		// PlayCollisionSound(Hit.ImpactPoint);
		// SpawnCollisionParticles(Hit.ImpactPoint);
	}
}
