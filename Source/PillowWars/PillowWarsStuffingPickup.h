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
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated, BlueprintReadOnly) float RemainingStuffing = 48.f;
    float TakeStuffing(float Requested);
    UPROPERTY(Replicated,BlueprintReadOnly) bool AmbientSource=true;
    UPROPERTY(Replicated) uint32 ResourceGeneration=0;
    UPROPERTY(Replicated,BlueprintReadOnly) int64 ResourceSerial=0;
    bool Registered=false;
private:
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    float SpawnedAt = 0.f;
};
