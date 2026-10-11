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
void APillowWarsProjectile::Launch(const FVector& Direction){LastFlightDirection=Direction.GetSafeNormal();Movement->Velocity=LastFlightDirection*1000;}
void APillowWarsProjectile::Tick(float Dt){Super::Tick(Dt);Pillow->AddLocalRotation(FRotator(0,Dt*720,0));if(HasAuthority()&&!Movement->Velocity.IsNearlyZero())LastFlightDirection=Movement->Velocity.GetSafeNormal();}
void APillowWarsProjectile::OnImpact(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,FVector,const FHitResult& Hit)
{
    if(!HasAuthority()||bImpacted||Other==GetOwner())return;
    bImpacted=true;
    // The collision sweep records the current substep before movement stops.
    FVector Incoming=(Hit.TraceEnd-Hit.TraceStart).GetSafeNormal();
    if(Incoming.IsNearlyZero())Incoming=Movement->Velocity.GetSafeNormal();
    if(Incoming.IsNearlyZero())Incoming=LastFlightDirection;
    if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())
        if(auto* Victim=Cast<ACharacter>(Other))
            if(auto* Shooter=Cast<APawn>(GetOwner())) GM->ApplyPillowContact(Cast<APlayerController>(Shooter->GetController()),Victim,Incoming,-1.f,true);
    Destroy();
}
