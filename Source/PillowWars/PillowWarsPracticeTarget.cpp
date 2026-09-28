#include "PillowWarsPracticeTarget.h"
#include "PillowWarsWeapon.h"
#include "Components/TextRenderComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

APillowWarsPracticeTarget::APillowWarsPracticeTarget()
{
    bReplicates = true;
    bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(40, 90);
    GetCharacterMovement()->bRunPhysicsWithNoController = true;
    auto* Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PracticeBase"));
    Body->SetupAttachment(GetRootComponent());
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    if (Cube.Succeeded()) Body->SetStaticMesh(Cube.Object);
    Body->SetRelativeScale3D(FVector(1.35f,1.35f,.10f));
    Body->SetRelativeLocation(FVector(0,0,-85));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Label=CreateDefaultSubobject<UTextRenderComponent>(TEXT("PracticeLabel"));
    Label->SetupAttachment(GetRootComponent()); Label->SetRelativeLocation(FVector(0,0,160));
    Label->SetHorizontalAlignment(EHTA_Center); Label->SetWorldSize(18);
    Label->SetTextRenderColor(FColor::Cyan);
}
void APillowWarsPracticeTarget::BeginPlay()
{
    Super::BeginPlay(); HomePosition=GetActorLocation();
    if(bAnchored)GetCharacterMovement()->SetMovementMode(MOVE_None);
    if(HasAuthority())
    {
        FActorSpawnParameters Params; Params.Owner=this;
        Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        PracticeVisual=GetWorld()->SpawnActor<APillowWarsWeapon>(GetActorLocation(),GetActorRotation(),Params);
        if(PracticeVisual)PracticeVisual->SetVariant(6);
    }
}
void APillowWarsPracticeTarget::ReactToHit(const FVector& Direction,float Strength)
{
    if(PracticeVisual)PracticeVisual->ReceiveImpact(Direction,Strength);
}
void APillowWarsPracticeTarget::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APillowWarsPracticeTarget, Daze);
    DOREPLIFETIME(APillowWarsPracticeTarget, Hits);
    DOREPLIFETIME(APillowWarsPracticeTarget, LastKnockback);
    DOREPLIFETIME(APillowWarsPracticeTarget, bAnchored);
    DOREPLIFETIME(APillowWarsPracticeTarget, PracticeVisual);
}
void APillowWarsPracticeTarget::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if(bAnchored)GetCharacterMovement()->SetMovementMode(MOVE_None);
    Label->SetText(FText::FromString(FString::Printf(TEXT("PRACTICE DUMMY\nHits %d | Daze %.0f\nR resets"),Hits,Daze)));
    if(auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0)) Label->SetWorldRotation((Camera->GetCameraLocation()-Label->GetComponentLocation()).Rotation());
    if (HasAuthority() && GetActorLocation().Z < -650)
    {
        SetActorLocation(HomePosition, false, nullptr, ETeleportType::TeleportPhysics);
        GetCharacterMovement()->StopMovementImmediately();
        ForceNetUpdate();
    }
}
void APillowWarsPracticeTarget::ResetPractice()
{
    if (!HasAuthority()) return;
    Daze = 0; Hits = 0; LastKnockback = 0;
    SetActorLocation(HomePosition, false, nullptr, ETeleportType::TeleportPhysics);
    if(PracticeVisual)PracticeVisual->ResetAttack();
    GetCharacterMovement()->StopMovementImmediately();
    ForceNetUpdate();
}
