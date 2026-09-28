#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PillowWarsStuffingPickup.generated.h"

class UStaticMeshComponent;

UCLASS()
class PILLOWWARS_API APillowWarsStuffingPickup : public AActor
{
    GENERATED_BODY()
public:
    APillowWarsStuffingPickup();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated, BlueprintReadOnly) float RemainingStuffing = 48.f;
    float TakeStuffing(float Requested);
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    float SpawnedAt = 0.f;
};
