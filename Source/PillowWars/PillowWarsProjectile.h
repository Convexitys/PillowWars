#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PillowWarsProjectile.generated.h"
class USphereComponent;
class UStaticMeshComponent;
class UProjectileMovementComponent;
UCLASS()
class PILLOWWARS_API APillowWarsProjectile : public AActor
{
    GENERATED_BODY()
public:
    APillowWarsProjectile();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Launch(const FVector& Direction);
private:
    UPROPERTY() TObjectPtr<USphereComponent> Collision;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Pillow;
    UPROPERTY() TObjectPtr<UProjectileMovementComponent> Movement;
    UFUNCTION() void OnImpact(UPrimitiveComponent* Component,AActor* Other,UPrimitiveComponent* OtherComponent,FVector Impulse,const FHitResult& Hit);
    bool bImpacted=false;
};
