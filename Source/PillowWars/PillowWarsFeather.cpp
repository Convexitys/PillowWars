#include "PillowWarsFeather.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

APillowWarsFeather::APillowWarsFeather()
{
    bReplicates=true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick=true;
    SetNetUpdateFrequency(30.f);
    InitialLifeSpan=1.15f;

    FeatherRoot=CreateDefaultSubobject<USceneComponent>(TEXT("FeatherRoot"));
    SetRootComponent(FeatherRoot);
    FeatherShape=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FeatherDown"));
    FeatherShape->SetupAttachment(FeatherRoot);
    FeatherShape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FeatherShape->SetRelativeScale3D(FVector(.13f,.045f,.012f));
    FeatherShaft=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FeatherQuill"));
    FeatherShaft->SetupAttachment(FeatherRoot);
    FeatherShaft->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FeatherShaft->SetRelativeRotation(FRotator(0,90,90));
    FeatherShaft->SetRelativeScale3D(FVector(.009f,.009f,.12f));

    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if(Sphere.Succeeded())FeatherShape->SetStaticMesh(Sphere.Object);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if(Cylinder.Succeeded())FeatherShaft->SetStaticMesh(Cylinder.Object);
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    if(Material.Succeeded())
    {
        if(UMaterialInstanceDynamic* Down=FeatherShape->CreateAndSetMaterialInstanceDynamicFromMaterial(0,Material.Object))
        {
            Down->SetVectorParameterValue(TEXT("Color"),FLinearColor(.98f,.94f,.82f));
            Down->SetVectorParameterValue(TEXT("BaseColor"),FLinearColor(.98f,.94f,.82f));
        }
        if(UMaterialInstanceDynamic* Shaft=FeatherShaft->CreateAndSetMaterialInstanceDynamicFromMaterial(0,Material.Object))
        {
            Shaft->SetVectorParameterValue(TEXT("Color"),FLinearColor(.86f,.78f,.58f));
            Shaft->SetVectorParameterValue(TEXT("BaseColor"),FLinearColor(.86f,.78f,.58f));
        }
    }

    Flight=CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("FeatherFlight"));
    Flight->SetUpdatedComponent(FeatherRoot);
    Flight->ProjectileGravityScale=1.25f;
    Flight->MaxSpeed=1000.f;
    Flight->bRotationFollowsVelocity=false;
    Flight->bAutoActivate=false;
}

void APillowWarsFeather::BeginPlay()
{
    Super::BeginPlay();
    if(!HasAuthority())Flight->Deactivate();
}

void APillowWarsFeather::Launch(const FVector& InitialVelocity)
{
    if(!HasAuthority())return;
    Flight->Velocity=InitialVelocity;
    Flight->Activate(true);
    ForceNetUpdate();
}

void APillowWarsFeather::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    FeatherShape->AddLocalRotation(FRotator(DeltaSeconds*390.f,DeltaSeconds*170.f,DeltaSeconds*230.f));
    FeatherShaft->AddLocalRotation(FRotator(DeltaSeconds*120.f,0,0));
}
