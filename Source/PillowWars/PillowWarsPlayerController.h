#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PillowWarsPlayerController.generated.h"

UENUM(BlueprintType)
enum class EPWLocalScreen : uint8
{
    MainMenu,
    Lobby,
    CharacterSelect,
    Settings,
    Tutorial,
    Credits,
    Loading,
    InGame,
    Pause,
    JoinParty
};

UCLASS()
class PILLOWWARS_API APillowWarsPlayerController : public APlayerController
{
    GENERATED_BODY()
protected:
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void SetupInputComponent() override;
    virtual bool InputKey(const FInputKeyParams& Params) override;
public:
    // Authority-only bridge. Bots share the exact charge/release/guard/throw validation used by input RPCs.
    void ApplyBotControls(const FVector& Direction, bool bJump, bool bAttackHeld, bool bGuard, bool bThrow);
    UFUNCTION(Exec) void PWAddBot();
    UFUNCTION(Exec) void PWRemoveBot();
    UFUNCTION(Exec) void PWStartBots();
    UFUNCTION(BlueprintCallable) void RequestSwing() { SwingPressed(); }
    UFUNCTION(BlueprintCallable) void RequestRestart() { RestartPressed(); }
    UFUNCTION(BlueprintPure) EPWLocalScreen GetLocalScreen() const { return LocalScreen; }
    UFUNCTION(BlueprintPure) FString GetJoinAddress() const { return JoinAddress; }
    int32 GetMenuIndex() const { return MenuIndex; }
    UFUNCTION(BlueprintPure) float GetMasterVolume() const { return MasterVolume; }
    UFUNCTION(BlueprintPure) float GetLookSensitivity() const { return LookSensitivity; }
    UFUNCTION(BlueprintPure) int32 GetGraphicsQuality() const { return GraphicsQuality; }
    UFUNCTION(BlueprintPure) bool GetHighContrast() const { return bHighContrast; }
    UFUNCTION(BlueprintPure) bool GetFullscreen() const { return bFullscreen; }
    float GetLoadingProgress() const;
    FString GetLoadingTip() const;
    const FString& GetFrontendStatus() const { return FrontendStatus; }
    UFUNCTION(BlueprintPure) FString GetActionHint() const;
    UFUNCTION(BlueprintPure) int32 GetPracticeStep() const { return PracticeStep; }
    void SendActionHint(uint32 Generation, const FString& Reason, const FString& Text);
    UFUNCTION(Client, Reliable) void ClientActionHint(uint32 Generation, uint32 Sequence, const FString& Reason, const FString& Text);
    UFUNCTION(Client, Reliable) void ClientResourceSession(uint32 Generation);
    void SendPracticeStep(uint32 Generation,int32 Step);
    UFUNCTION(Client, Reliable) void ClientPracticeStep(uint32 Generation,uint32 Sequence,int32 Step);
private:
    uint32 HintGeneration=0, HintSequence=0, ServerHintSequence=0;
    uint32 PracticeSequence=0,ServerPracticeSequence=0;
    FString ActionHint, LastHintReason;
    float ActionHintExpires=0.f, LastHintSent=-100.f;
    int32 PracticeStep=-1;
    void PracticePressed();
    void ReclaimPressed();
    UFUNCTION(Server, Reliable) void ServerPracticeChallenge();
    UFUNCTION(Server, Reliable) void ServerReclaimCover();
    bool bBotAttackHeld = false;
    void AddBotPressed();
    void RemoveBotPressed();
    void StartBotsPressed();
    UFUNCTION(Server, Reliable) void ServerEditBots(int32 Change);
    UFUNCTION(Server, Reliable) void ServerStartBots();
    EPWLocalScreen LocalScreen = EPWLocalScreen::MainMenu;
    EPWLocalScreen SettingsReturnScreen = EPWLocalScreen::MainMenu;
    int32 MenuIndex = 0;
    float MasterVolume = 1.f;
    float LookSensitivity = 1.f;
    int32 GraphicsQuality = 3;
    bool bHighContrast = false;
    bool bFullscreen = false;
    bool bFrontendInputBlocked = false;
    float LoadingStartedAt = 0.f;
    float JoinAttemptStartedAt = 0.f;
    bool bJoinAttemptPending = false;
    bool bMoveForwardKeyDown = false;
    bool bMoveBackwardKeyDown = false;
    bool bMoveLeftKeyDown = false;
    bool bMoveRightKeyDown = false;
    float GamepadMoveForward = 0.f;
    float GamepadMoveRight = 0.f;
    FString JoinAddress = TEXT("127.0.0.1");
    FString FrontendStatus;
    void SetLocalScreen(EPWLocalScreen NewScreen);
    void NavigateMenu(int32 Direction);
    void AdjustMenu(int32 Direction);
    void SaveLocalPreferences();
    void AdjustHorizontal(int32 Direction);
    void NavigateUp();
    void NavigateDown();
    void AdjustLeft();
    void AdjustRight();
    void ConfirmMenu();
    void BackMenu();
    void OpenCharacterSelect();
    void HostStartPressed();
    void JoinPressed();
    void JoinAddressPressed();
    void ConnectToHost();
    void SelectionColorPrevious();
    void SelectionColorNext();
    void SelectionPillowPrevious();
    void SelectionPillowNext();
    UFUNCTION(Server,Reliable) void ServerSetReady(bool bNewReady);
    UFUNCTION(Server,Reliable) void ServerSetSelection(int32 CharacterChoice,int32 BodyChoice,int32 ColorChoice,int32 PillowChoice);
    UFUNCTION(Server,Reliable) void ServerAdjustSelection(int32 CharacterDelta,int32 BodyDelta,int32 ColorDelta,int32 PillowDelta);
    UFUNCTION(Server,Reliable) void ServerStartFrontendMatch(bool bPractice);
    UFUNCTION(Server,Reliable) void ServerReturnToLobby();
    void SwingPressed();
    void SwingReleased();
    UFUNCTION(Server, Reliable) void ServerBeginCharge();
    UFUNCTION(Server, Reliable) void ServerReleaseCharge();
    void RestartPressed();
    void CyclePressed();
    void ThrowPressed();
    void GuardPressed();
    void GuardReleased();
    void CoverPressed();
    void MoveForwardKeyPressed();
    void MoveForwardKeyReleased();
    void MoveBackwardKeyPressed();
    void MoveBackwardKeyReleased();
    void MoveLeftKeyPressed();
    void MoveLeftKeyReleased();
    void MoveRightKeyPressed();
    void MoveRightKeyReleased();
    void MoveForward(float Value);
    void MoveRight(float Value);
    void Turn(float Value);
    void LookUp(float Value);
    void JumpPressed();
    void JumpReleased();
    UFUNCTION(Server,Reliable) void ServerThrow();
    UFUNCTION(Server,Reliable) void ServerSetGuard(bool bPressed);
    UFUNCTION(Server,Reliable) void ServerPlaceCover();
    UFUNCTION(Server,Reliable) void ServerCycle();
    UFUNCTION(Server, Reliable) void ServerSwing();
    UFUNCTION(Server, Reliable) void ServerRestart();
};
