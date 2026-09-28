#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PillowWarsPracticeTarget.generated.h"

UCLASS()
class PILLOWWARS_API APillowWarsPracticeTarget : public ACharacter
{
    GENERATED_BODY()
public:
    APillowWarsPracticeTarget();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    void ResetPractice();
    UPROPERTY(Replicated, BlueprintReadOnly) float Daze = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 Hits = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float LastKnockback = 0;
    UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite) bool bAnchored = true;
    UPROPERTY(Replicated, BlueprintReadOnly) TObjectPtr<class APillowWarsWeapon> PracticeVisual;
    void ReactToHit(const FVector& Direction,float Strength);
private:
    UPROPERTY() TObjectPtr<class UTextRenderComponent> Label;
    FVector HomePosition;
};
