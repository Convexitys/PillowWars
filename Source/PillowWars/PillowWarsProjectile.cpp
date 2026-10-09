#include "PillowWarsProjectile.h"
#include "PillowWarsGameMode.h"
#include "Engine/StaticMesh.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "UObject/ConstructorHelpers.h"
APillowWarsProjectile::APillowWarsProjectile()
{
    bReplicates=true; SetReplicateMovement(true); PrimaryActorTick.bCanEverTick=true; InitialLifeSpan=4;
    Collision=CreateDefaultSubobject<USphereComponent>(TEXT("FlightCollision")); SetRootComponent(Collision);
    Collision->InitSphereRadius(17); Collision->SetCollisionProfileName(TEXT("BlockAllDynamic"));
    Collision->OnComponentHit.AddDynamic(this,&APillowWarsProjectile::OnImpact);
    Pillow=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThrowPillow")); Pillow->SetupAttachment(Collision); Pillow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Game/PillowWars/Props/ThrowPillow/SM_ThrowPillow.SM_ThrowPillow"));
    if(Mesh.Succeeded())Pillow->SetStaticMesh(Mesh.Object);
    Movement=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("Flight")); Movement->UpdatedComponent=Collision;
    Movement->InitialSpeed=1000; Movement->MaxSpeed=1150; Movement->ProjectileGravityScale=1.0f; Movement->bForceSubStepping=true;
}
void APillowWarsProjectile::BeginPlay()
{
    Super::BeginPlay(); Collision->IgnoreActorWhenMoving(GetOwner(),true);
    if(!HasAuthority()){Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);Movement->Deactivate();}
}
void APillowWarsProjectile::Launch(const FVector& Direction){Movement->Velocity=Direction.GetSafeNormal()*1000;}
void APillowWarsProjectile::Tick(float Dt){Super::Tick(Dt);Pillow->AddLocalRotation(FRotator(0,Dt*720,0));}
void APillowWarsProjectile::OnImpact(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,FVector,const FHitResult&)
{
    if(!HasAuthority()||bImpacted||Other==GetOwner())return;
    bImpacted=true;
    if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())
        if(auto* Victim=Cast<ACharacter>(Other))
            if(auto* Shooter=Cast<APawn>(GetOwner())) GM->ApplyPillowContact(Cast<APlayerController>(Shooter->GetController()),Victim,Movement->Velocity.GetSafeNormal(),-1.f,true);
    Destroy();
}
