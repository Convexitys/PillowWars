#include "PillowWarsCover.h"
#include "PillowWarsGameMode.h"
#include "PillowWarsMatchState.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

APillowWarsCover::APillowWarsCover()
{
    bReplicates = true;
    SetReplicateMovement(false);
    Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("CoverCollision"));
    RootComponent = Collision;
    Collision->SetBoxExtent(FVector(58.f, 20.f, 42.f));
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionObjectType(ECC_WorldDynamic);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    Visual = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FoldedPillowCover"));
    Visual->SetupAttachment(Collision);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Mesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Mesh.Succeeded()) Visual->SetStaticMesh(Mesh.Object);
    Visual->SetRelativeScale3D(FVector(.9f,.55f,.45f));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Fabric(TEXT("/Game/PillowWars/Materials/M_PW_PillowBlue.M_PW_PillowBlue"));
    if (Fabric.Succeeded()) Visual->SetMaterial(0,Fabric.Object);
    InitialLifeSpan = 8.f;
}

void APillowWarsCover::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APillowWarsCover, Integrity);
}

bool APillowWarsCover::ReceivePillowHit(APlayerController* Attacker)
{
    if (!HasAuthority() || Integrity <= 0) return false;
    --Integrity;
    OnRep_Integrity();
    ForceNetUpdate();
    if (auto* Mode=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())
    {
        if (Integrity==0 && Attacker)
            if (auto* State=Attacker->GetPlayerState<APillowWarsPlayerState>())
            { ++State->RoundCoversBroken; State->ForceNetUpdate(); }
        Mode->AnnounceMoment(Integrity>0?TEXT("Pillow cover absorbed a hit"):TEXT("Pillow cover broke"));
    }
    if (Integrity == 0) SetLifeSpan(.18f);
    return true;
}

void APillowWarsCover::OnRep_Integrity()
{
    if (Visual) Visual->SetRelativeScale3D(FVector(.9f,.55f,.18f+.09f*FMath::Clamp(Integrity,0,3)));
}
