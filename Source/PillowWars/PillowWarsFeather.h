#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PillowWarsFeather.generated.h"

class UProjectileMovementComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class PILLOWWARS_API APillowWarsFeather : public AActor
{
    GENERATED_BODY()
public:
    APillowWarsFeather();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    void Launch(const FVector& InitialVelocity);
private:
    UPROPERTY() TObjectPtr<USceneComponent> FeatherRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FeatherShape;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> FeatherShaft;
    UPROPERTY() TObjectPtr<UProjectileMovementComponent> Flight;
};
