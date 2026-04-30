#include "CPP_Bullet.h"
#include "BulletMovementComponent.h"

ACPP_Bullet::ACPP_Bullet()
{
    PrimaryActorTick.bCanEverTick = false; // The component ticks, not the actor.

    MovementComponent = CreateDefaultSubobject<UBulletMovementComponent>(
        TEXT("BulletMovementComponent"));
}

void ACPP_Bullet::BeginPlay()
{
    Super::BeginPlay();
}

void ACPP_Bullet::InitializeBullet(const FBulletMotionParams& Params)
{
    if (MovementComponent)
    {
        MovementComponent->Initialize(Params);
    }
}
