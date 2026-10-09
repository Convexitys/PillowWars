#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PillowWarsResourceMath.h"
#include "PillowWarsGameMode.generated.h"

class ACharacter;
class APlayerController;
class APillowWarsWeapon;
class APillowWarsCover;
class APillowWarsStuffingPickup;
class APillowWarsPlayerState;
struct FPWResourceClock
{
    double LastTime=0;
    TWeakObjectPtr<APillowWarsStuffingPickup> ObservedPile;
    PWResourceMath::RateCarry GuardCarry,RefillCarry;
    FVector ObservedLocation=FVector::ZeroVector;
};
struct FPWResourceObject
{
    uint32 Generation=0;uint64 Serial=0;int64 Units=0;bool bAmbient=false;
    TWeakObjectPtr<APillowWarsPlayerState> Owner;
};

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
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void RestartPlayer(AController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ServerSwing(APlayerController* Attacker);
    void SwingWithCharge(APlayerController* Attacker, float UppercutPower, float Held);
    UFUNCTION(BlueprintPure) static float SwingStuffingCost(float UppercutPower);
    UFUNCTION(BlueprintPure) static float CriticalChance(float ResonancePercent);
    UFUNCTION(BlueprintPure) static float HealthDamage(float UppercutPower, float StuffingPower, bool bCritical, bool bGuarded);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void ResetMatch();
    void RequestReset(APlayerController* Requester);
    bool ApplyPillowContact(APlayerController* Attacker, ACharacter* Victim, const FVector& ImpactDirection, float UppercutPower = -1.f, bool bProjectile = false);
    void SetGuard(APlayerController* Player, bool bPressed);
    void PlaceCover(APlayerController* Player);
    bool CollectStuffing(APlayerController* Player);
    void AnnounceMoment(const FString& Moment);
    void CycleCharacter(APlayerController* Player);
    void SetPlayerReady(APlayerController* Player, bool bReady);
    void SetPlayerSelection(APlayerController* Player, int32 Character, int32 Body, int32 Color, int32 Pillow);
    void StartFrontendMatch(APlayerController* Requester, bool bPractice);
    void ReturnToLobby(APlayerController* Requester);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) bool EditBots(APlayerController* Requester, int32 Change);
    void SettleResources(APlayerController* Player,double Now);
    void RecordResourceAction(APlayerController* Player,double Now);
    uint32 GetResourceGeneration() const { return ResourceGeneration; }
    void ResolveCover(APillowWarsCover* Cover,FName Reason);
    void RetirePile(APillowWarsStuffingPickup* Pile);
    void ReclaimCover(APlayerController* Player);
    void TogglePracticeChallenge(APlayerController* Player);
    UFUNCTION(BlueprintPure) FString GetResourceLedgerSnapshot() const;
    void TestSetStuffing(APlayerController* Player,float Value);
    bool bTestDisableAmbient=false;
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
    bool bResourceOpen=false;
    uint32 ResourceGeneration=0;
    uint64 ResourceSerial=0,ResourceSequence=0;
    int64 ResourceInitial=0,ResourceSupply=0,ResourceLoss=0;
    TMap<TWeakObjectPtr<APillowWarsPlayerState>,int64> HeldUnits;
    TMap<TWeakObjectPtr<APillowWarsStuffingPickup>,FPWResourceObject> PileBook;
    TMap<TWeakObjectPtr<APillowWarsCover>,FPWResourceObject> CoverBook;
    TMap<TWeakObjectPtr<APlayerController>,FPWResourceClock> ResourceClocks;
    TMap<TWeakObjectPtr<APlayerController>,int32> PracticeSteps;
    TSet<FString> ContactReceipts;
    void OpenResources();
    void CloseResources(FName Reason);
    void RetireOwner(APlayerController* Player);
    int64 GetHeld(APillowWarsPlayerState* State);
    void ProjectHeld(APillowWarsPlayerState* State);
    void ResourceAudit(FName Reason);
    APillowWarsStuffingPickup* FindRecoveryPile(APlayerController* Player) const;
    int64 PlaceLoose(const FVector& Location,int64 Amount,bool bAmbient);
    bool Debit(APlayerController* Player,int64 Amount,FName Reason);
    void PracticeContact(APlayerController* Player);
    void PracticeProgress(APlayerController* Player,int32 Step);
};
