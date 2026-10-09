#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/HUD.h"
#include "PillowWarsMatchState.generated.h"

UENUM(BlueprintType)
enum class EPWMatchPhase : uint8
{
    Lobby,
    Countdown,
    Playing,
    RoundOver
};

UCLASS()
class PILLOWWARS_API APillowWarsPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly) float Health = 100.f;
    UPROPERTY(Replicated, BlueprintReadOnly) float LastHitDamage = 0.f;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bLastHitCritical = false;
    UPROPERTY(Replicated, BlueprintReadOnly) float LastHitServerTime = -100.f;
    UPROPERTY(Replicated, BlueprintReadOnly) float Daze = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bEliminated = false;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundWins = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bReady = false;
    UPROPERTY(Replicated, BlueprintReadOnly) float Stuffing = 100.0f;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bStuffingResting = false;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bGuarding = false;
    UPROPERTY(Replicated, BlueprintReadOnly) float GuardPulseServerTime = -100.0f;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundHits = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundHitsTaken = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundGuards = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundCoversPlaced = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundCoversBroken = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundStuffingCollected = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 CharacterIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 BodySizeIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 ColorIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 PillowIndex = 0;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class PILLOWWARS_API APillowWarsMatchState : public AGameStateBase
{
    GENERATED_BODY()
public:
    UPROPERTY(Replicated, BlueprintReadOnly) FString Result;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RoundNumber = 1;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 RematchVotes = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 PlayersNeededToRematch = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) EPWMatchPhase MatchPhase = EPWMatchPhase::Playing;
    UPROPERTY(Replicated, BlueprintReadOnly) float CountdownEndServerTime = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float MatchEndServerTime = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) FString LobbyStatus;
    UPROPERTY(Replicated, BlueprintReadOnly) FString LastMoment;
    UPROPERTY(Replicated, BlueprintReadOnly) float LastMomentServerTime = -100.0f;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

UCLASS()
class PILLOWWARS_API APillowWarsHUD : public AHUD
{
    GENERATED_BODY()
public:
    APillowWarsHUD();
    virtual void DrawHUD() override;
private:
    UPROPERTY() TObjectPtr<class UTexture2D> MenuArtwork;
};
