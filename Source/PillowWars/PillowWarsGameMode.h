#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PillowWarsGameMode.generated.h"

class ACharacter;
class APlayerController;
class APillowWarsWeapon;
class APillowWarsCover;
class APillowWarsStuffingPickup;

USTRUCT()
struct FPillowFighterState
{
    GENERATED_BODY()
    UPROPERTY() float Daze = 0.0f;
    UPROPERTY() bool Eliminated = false;
};

UCLASS()
class PILLOWWARS_API APillowWarsGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    APillowWarsGameMode();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ServerSwing(APlayerController* Attacker);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ResetMatch();
    void RequestReset(APlayerController* Requester);
    bool ApplyPillowContact(APlayerController* Attacker, ACharacter* Victim, const FVector& ImpactDirection, float UppercutPower = -1.f);
    void SetGuard(APlayerController* Player, bool bPressed);
    void PlaceCover(APlayerController* Player);
    bool CollectStuffing(APlayerController* Player);
    void AnnounceMoment(const FString& Moment);
    void CycleCharacter(APlayerController* Player);
    void SetPlayerReady(APlayerController* Player, bool bReady);
    void SetPlayerSelection(APlayerController* Player, int32 Character, int32 Body, int32 Color, int32 Pillow);
    void StartFrontendMatch(APlayerController* Requester, bool bPractice);
    void ReturnToLobby(APlayerController* Requester);
protected:
    UPROPERTY(EditDefaultsOnly, Category="Pillow Wars") float SwingCooldown = 0.65f;
private:
    TMap<TObjectPtr<ACharacter>, FPillowFighterState> FighterStates;
    TMap<TObjectPtr<APlayerController>, float> LastSwingTime;
    TMap<TObjectPtr<APlayerController>, float> LastCombatAction;
    TMap<TObjectPtr<APlayerController>, float> LastCoverTime;
    TMap<TObjectPtr<APlayerController>, float> LastResourceNetUpdate;
    TMap<TObjectPtr<ACharacter>, float> LastPickupSpawnTime;
    bool bMatchOver = false;
    bool bRoundStarted = false;
    bool bPracticeSession = false;
    bool bWasGameplayActive = false;
    int32 AmbientPileTarget = 4;
    float NextAmbientPileSpawnTime = 0.f;
    float MatchDuration = 180.f;
    TSet<TWeakObjectPtr<APlayerController>> RematchVoters;
    UPROPERTY(EditDefaultsOnly, Category="Pillow Wars|Stuffing") FVector RecoveryCenter = FVector::ZeroVector;
    UPROPERTY(EditDefaultsOnly, Category="Pillow Wars|Stuffing", meta=(ClampMin="100.0")) float RecoveryRadius = 1500.0f;
    UPROPERTY(EditDefaultsOnly, Category="Pillow Wars|Cover") TSubclassOf<APillowWarsCover> CoverClass;
    UPROPERTY() TMap<TObjectPtr<ACharacter>, TObjectPtr<APillowWarsWeapon>> Weapons;
    void BuildArena();
    void SpawnArenaBox(const FName& Label, const FVector& Location, const FVector& Scale, const FLinearColor& Color);
    void ApplyPlayerSelection(APlayerController* Player);
    void ClearRematchVotes(int32 ConnectedPlayers = -1);
    void ClearCovers(const AActor* OwnerActor = nullptr);
    void ClearStuffingPickups();
    bool SpawnAmbientStuffingPile();
    void SpawnStuffingPickup(ACharacter* Victim, const FVector& AwayFromAttacker);
};
