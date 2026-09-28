#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PillowWarsCover.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class APlayerController;

UCLASS()
class PILLOWWARS_API APillowWarsCover : public AActor
{
    GENERATED_BODY()
public:
    APillowWarsCover();
    bool ReceivePillowHit(APlayerController* Attacker);
    UPROPERTY(ReplicatedUsing=OnRep_Integrity, BlueprintReadOnly) int32 Integrity = 3;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
private:
    UPROPERTY() TObjectPtr<UBoxComponent> Collision;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Visual;
    UFUNCTION() void OnRep_Integrity();
};
