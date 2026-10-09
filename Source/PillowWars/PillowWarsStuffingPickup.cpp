#include "PillowWarsStuffingPickup.h"
#include "PillowWarsGameMode.h"
#include "PillowWarsMatchState.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APillowWarsStuffingPickup::APillowWarsStuffingPickup()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LooseStuffing"));
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("PileAnchor"));
    Visual->SetupAttachment(RootComponent);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetRelativeScale3D(FVector(.38f,.30f,.12f));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (Mesh.Succeeded()) Visual->SetStaticMesh(Mesh.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Fabric(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if (Fabric.Succeeded()) Visual->SetMaterial(0,Fabric.Object);
    for(int32 Index=0;Index<7;++Index)
    {
        UStaticMeshComponent* Puff=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("FeatherClump%d"),Index));
        Puff->SetupAttachment(RootComponent);
        Puff->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        const float Angle=Index*2.f*PI/7.f;
        Puff->SetRelativeLocation(FVector(FMath::Cos(Angle)*24,FMath::Sin(Angle)*21,5+(Index%3)*3));
        Puff->SetRelativeRotation(FRotator(10,Index*51,12));
        Puff->SetRelativeScale3D(FVector(.25f,.07f,.025f));
        if(Mesh.Succeeded())Puff->SetStaticMesh(Mesh.Object);
        if(Fabric.Succeeded())Puff->SetMaterial(0,Fabric.Object);
    }
}

void APillowWarsStuffingPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APillowWarsStuffingPickup,RemainingStuffing);
    DOREPLIFETIME(APillowWarsStuffingPickup,AmbientSource);
    DOREPLIFETIME(APillowWarsStuffingPickup,ResourceGeneration);
    DOREPLIFETIME(APillowWarsStuffingPickup,ResourceSerial);
}
float APillowWarsStuffingPickup::TakeStuffing(float Requested)
{
    // Legacy unowned transfers are disabled. GameMode commits both balances first.
    return 0;
}
void APillowWarsStuffingPickup::EndPlay(const EEndPlayReason::Type Reason)
{
    if(HasAuthority()&&Reason==EEndPlayReason::Destroyed)
        if(auto* Mode=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())Mode->RetirePile(this);
    Super::EndPlay(Reason);
}
void APillowWarsStuffingPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const float VisualTime=GetWorld()->GetTimeSeconds();
    // Only the child floats. Never reset the actor's sampled world location.
    Visual->SetRelativeLocation(FVector(0,0,7.f+FMath::Sin(VisualTime*3.f)*1.5f));
    const float Size=FMath::Lerp(.55f,1.f,FMath::Clamp(RemainingStuffing/48.f,0.f,1.f));
    Visual->SetRelativeScale3D(FVector(.38f,.30f,.12f)*Size);
}
