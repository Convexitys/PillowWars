#pragma once

#include "CoreMinimal.h"
#include "PillowWarsPlayerController.h"
#include "PillowWarsBotController.generated.h"

// A server-only controller sharing the existing player combat path. It never owns a viewport.
UCLASS()
class PILLOWWARS_API APillowWarsBotController : public APillowWarsPlayerController
{
    GENERATED_BODY()
public:
    APillowWarsBotController();
    virtual bool IsLocalController() const override { return HasAuthority() && !bInitializingPawn; }
    virtual void Tick(float DeltaSeconds) override;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ResetBot();
    UPROPERTY(BlueprintReadOnly) FString Decision = TEXT("Waiting in lobby");
    UPROPERTY(BlueprintReadOnly) int32 DecisionsMade = 0;
    UPROPERTY(BlueprintReadOnly) FVector Steering = FVector::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bThinkingEnabled = true;
protected:
    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* PawnToPossess) override;
    virtual void PlayerTick(float DeltaSeconds) override {}
private:
    bool bInitializingPawn = false;
    TWeakObjectPtr<APawn> Opponent;
    FVector Home = FVector::ZeroVector;
    FVector MoveDirection = FVector::ZeroVector;
    FVector FacingDirection = FVector::ForwardVector;
    float NextThink = 0.f;
    float NextAttack = 0.f;
    float ReleaseAttackAt = -1.f;
    float NextJump = 0.f;
    float GuardUntil = 0.f;
    float LastSeenAt = -100.f;
    FVector LastSeenPosition = FVector::ZeroVector;
    FRandomStream Random;
    void Think(float Now);
    bool CanSee(const AActor* Actor) const;
    FVector SafeDirection(const FVector& Desired, bool& bJump) const;
};
