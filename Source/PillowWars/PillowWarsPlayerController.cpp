#include "PillowWarsPlayerController.h"
#include "PillowWarsGameMode.h"
#include "InputCoreTypes.h"
#include "PillowWarsWeapon.h"
#include "PillowWarsMatchState.h"
#include "EngineUtils.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"

void APillowWarsPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if(IsLocalController())
    {
        SetLocalScreen(EPWLocalScreen::MainMenu);
        if(GConfig)
        {
            GConfig->GetFloat(TEXT("PillowWars.Preferences"),TEXT("MasterVolume"),MasterVolume,GGameUserSettingsIni);
            GConfig->GetFloat(TEXT("PillowWars.Preferences"),TEXT("LookSensitivity"),LookSensitivity,GGameUserSettingsIni);
            GConfig->GetBool(TEXT("PillowWars.Preferences"),TEXT("HighContrast"),bHighContrast,GGameUserSettingsIni);
        }
        MasterVolume=FMath::Clamp(MasterVolume,0.f,1.f);
        LookSensitivity=FMath::Clamp(LookSensitivity,.2f,3.f);
        FApp::SetVolumeMultiplier(MasterVolume);
        // Scale at the input handler, independent of legacy input-scale settings.
PRAGMA_DISABLE_DEPRECATION_WARNINGS
        SetDeprecatedInputYawScale(1.f);SetDeprecatedInputPitchScale(1.f);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
        if(UGameUserSettings* Settings=GEngine?GEngine->GetGameUserSettings():nullptr)
        {
            GraphicsQuality=Settings->GetOverallScalabilityLevel();
            bFullscreen=Settings->GetFullscreenMode()!=EWindowMode::Windowed;
        }
    }
}

void APillowWarsPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if(!IsLocalController())return;
    if(LocalScreen==EPWLocalScreen::Loading)
    {
        if(const auto* Match=GetWorld()->GetGameState<APillowWarsMatchState>())
        {
            const float LoadingAge=GetWorld()->GetTimeSeconds()-LoadingStartedAt;
            if((Match->MatchPhase==EPWMatchPhase::Playing || Match->MatchPhase==EPWMatchPhase::RoundOver) && LoadingAge>.65f)
                SetLocalScreen(EPWLocalScreen::InGame);
            else if(LoadingAge>15.f && Match->MatchPhase!=EPWMatchPhase::Countdown)
            {
                SetLocalScreen(GetNetMode()==NM_Client?EPWLocalScreen::Lobby:EPWLocalScreen::MainMenu);
                FrontendStatus=TEXT("Match did not start. Check that the host started it, then try again.");
            }
        }
    }
    else if(LocalScreen==EPWLocalScreen::JoinParty && bJoinAttemptPending)
    {
        if(GetWorld()->GetTimeSeconds()-JoinAttemptStartedAt>12.f)
        {
            bJoinAttemptPending=false;
            FrontendStatus=TEXT("No connection yet. Check the host address and try again.");
        }
    }
    else if(LocalScreen==EPWLocalScreen::Pause)
    {
        if(const auto* Match=GetWorld()->GetGameState<APillowWarsMatchState>();Match&&Match->MatchPhase==EPWMatchPhase::Lobby)
            SetLocalScreen(EPWLocalScreen::Lobby);
    }
    else if(LocalScreen==EPWLocalScreen::MainMenu || LocalScreen==EPWLocalScreen::Lobby || LocalScreen==EPWLocalScreen::CharacterSelect)
    {
        if(const auto* Match=GetWorld()->GetGameState<APillowWarsMatchState>();Match&&Match->MatchPhase==EPWMatchPhase::Countdown)
        {
            LoadingStartedAt=GetWorld()->GetTimeSeconds();SetLocalScreen(EPWLocalScreen::Loading);
        }
    }

    // Keep keyboard movement on the same direct key-event path used by combat.
    // The project uses Enhanced Input classes, so relying on legacy WASD axis
    // mappings here was needlessly fragile. The gamepad still feeds its axes.
    if(LocalScreen==EPWLocalScreen::InGame)
    {
        const float Forward=FMath::Clamp(
            (bMoveForwardKeyDown?1.f:0.f)-(bMoveBackwardKeyDown?1.f:0.f)+GamepadMoveForward,-1.f,1.f);
        const float Right=FMath::Clamp(
            (bMoveRightKeyDown?1.f:0.f)-(bMoveLeftKeyDown?1.f:0.f)+GamepadMoveRight,-1.f,1.f);
        if(APawn* ControlledPawn=GetPawn())
        {
            const FRotator YawRotation(0.f,GetControlRotation().Yaw,0.f);
            ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X),Forward);
            ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y),Right);
        }
    }
}

void APillowWarsPlayerController::SetLocalScreen(EPWLocalScreen NewScreen)
{
    const EPWLocalScreen PreviousScreen=LocalScreen;
    LocalScreen=NewScreen; MenuIndex=0; FrontendStatus.Empty();
    const bool bGameplay=NewScreen==EPWLocalScreen::InGame;
    if(!bGameplay)
    {
        bMoveForwardKeyDown=false;bMoveBackwardKeyDown=false;
        bMoveLeftKeyDown=false;bMoveRightKeyDown=false;
        GamepadMoveForward=0.f;GamepadMoveRight=0.f;
    }
    // These APIs increment/decrement ignore counters. Only change our one
    // frontend-owned block when crossing between gameplay and a menu.
    if(bFrontendInputBlocked!=!bGameplay)
    {
        SetIgnoreMoveInput(!bGameplay);
        SetIgnoreLookInput(!bGameplay);
        bFrontendInputBlocked=!bGameplay;
    }
    bShowMouseCursor=NewScreen!=EPWLocalScreen::InGame;
    SetInputMode(FInputModeGameOnly());
    UE_LOG(LogTemp,Log,TEXT("PW_SCREEN %d -> %d pawn=%s moveIgnored=%d"),
        static_cast<uint8>(PreviousScreen),static_cast<uint8>(NewScreen),*GetNameSafe(GetPawn()),IsMoveInputIgnored());
}

float APillowWarsPlayerController::GetLoadingProgress() const
{
    return FMath::Clamp((GetWorld()->GetTimeSeconds()-LoadingStartedAt)/3.f,.08f,1.f);
}

FString APillowWarsPlayerController::GetLoadingTip() const
{
    const int32 Tip=FMath::FloorToInt(GetWorld()->GetTimeSeconds()/3.f)%4;
    const TCHAR* Tips[]={TEXT("TIP: Higher Daze means stronger knockback."),TEXT("TIP: Counter during the purple Resonance pulse."),TEXT("TIP: Recovery platforms can save a fall."),TEXT("TIP: Your throw pillow regenerates after 15 seconds.")};
    return Tips[Tip];
}

void APillowWarsPlayerController::NavigateMenu(int32 Direction)
{
    if(LocalScreen==EPWLocalScreen::CharacterSelect){AdjustMenu(Direction);return;}
    int32 Count=1;
    if(LocalScreen==EPWLocalScreen::MainMenu)Count=7;
    else if(LocalScreen==EPWLocalScreen::Settings)Count=7;
    else if(LocalScreen==EPWLocalScreen::Pause)Count=4;
    MenuIndex=(MenuIndex+Direction+Count)%Count;
}

void APillowWarsPlayerController::AdjustMenu(int32 Direction)
{
    if(LocalScreen==EPWLocalScreen::CharacterSelect)
    {
        ServerAdjustSelection(0,Direction,0,0);
        return;
    }
    if(LocalScreen!=EPWLocalScreen::Settings)return;
    if(MenuIndex==0)MasterVolume=FMath::Clamp(MasterVolume+Direction*.1f,0.f,1.f);
    else if(MenuIndex==1)
    {
        LookSensitivity=FMath::Clamp(LookSensitivity+Direction*.1f,.2f,3.f);
    }
    else if(MenuIndex==2)
    {
        GraphicsQuality=FMath::Clamp(GraphicsQuality+Direction,0,4);
        if(UGameUserSettings* S=GEngine?GEngine->GetGameUserSettings():nullptr){S->SetOverallScalabilityLevel(GraphicsQuality);S->ApplySettings(false);S->SaveSettings();}
    }
    else if(MenuIndex==3)
    {
        bFullscreen=!bFullscreen;
        if(UGameUserSettings* S=GEngine?GEngine->GetGameUserSettings():nullptr){S->SetFullscreenMode(bFullscreen?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);S->ApplySettings(false);S->SaveSettings();}
    }
    else if(MenuIndex==4)bHighContrast=!bHighContrast;
    FApp::SetVolumeMultiplier(MasterVolume);
    SaveLocalPreferences();
}

void APillowWarsPlayerController::SaveLocalPreferences()
{
    if(!GConfig || !IsLocalController())return;
    GConfig->SetFloat(TEXT("PillowWars.Preferences"),TEXT("MasterVolume"),MasterVolume,GGameUserSettingsIni);
    GConfig->SetFloat(TEXT("PillowWars.Preferences"),TEXT("LookSensitivity"),LookSensitivity,GGameUserSettingsIni);
    GConfig->SetBool(TEXT("PillowWars.Preferences"),TEXT("HighContrast"),bHighContrast,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
}

void APillowWarsPlayerController::AdjustHorizontal(int32 Direction)
{
    if(LocalScreen==EPWLocalScreen::CharacterSelect)
    {
        ServerAdjustSelection(Direction,0,0,0);
        return;
    }
    AdjustMenu(Direction);
}

void APillowWarsPlayerController::NavigateUp(){NavigateMenu(-1);}
void APillowWarsPlayerController::NavigateDown(){NavigateMenu(1);}
void APillowWarsPlayerController::AdjustLeft(){AdjustHorizontal(-1);}
void APillowWarsPlayerController::AdjustRight(){AdjustHorizontal(1);}

void APillowWarsPlayerController::ConfirmMenu()
{
    if(LocalScreen==EPWLocalScreen::MainMenu)
    {
        if(MenuIndex==0){SetLocalScreen(EPWLocalScreen::Lobby);ServerSetReady(false);}
        else if(MenuIndex==1)JoinAddressPressed();
        else if(MenuIndex==2)ServerStartFrontendMatch(true);
        else if(MenuIndex==3){SettingsReturnScreen=EPWLocalScreen::MainMenu;SetLocalScreen(EPWLocalScreen::Settings);}
        else if(MenuIndex==4)SetLocalScreen(EPWLocalScreen::Tutorial);
        else if(MenuIndex==5)SetLocalScreen(EPWLocalScreen::Credits);
        else if(MenuIndex==6)ConsoleCommand(TEXT("quit"));
    }
    else if(LocalScreen==EPWLocalScreen::Lobby)
    {
        if(auto* PS=GetPlayerState<APillowWarsPlayerState>())ServerSetReady(!PS->bReady);
    }
    else if(LocalScreen==EPWLocalScreen::CharacterSelect)SetLocalScreen(EPWLocalScreen::Lobby);
    else if(LocalScreen==EPWLocalScreen::Settings)
    {
        if(MenuIndex==5)
        {
            MasterVolume=1;LookSensitivity=1;GraphicsQuality=3;bFullscreen=false;bHighContrast=false;
            if(UGameUserSettings* S=GEngine?GEngine->GetGameUserSettings():nullptr)
            {
                S->SetOverallScalabilityLevel(GraphicsQuality);
                S->SetFullscreenMode(EWindowMode::Windowed);
                S->ApplySettings(false);S->SaveSettings();
            }
            FApp::SetVolumeMultiplier(MasterVolume);SaveLocalPreferences();
        }
        else if(MenuIndex==6)SetLocalScreen(SettingsReturnScreen);
        else AdjustMenu(1);
    }
    else if(LocalScreen==EPWLocalScreen::Pause)
    {
        if(MenuIndex==0)SetLocalScreen(EPWLocalScreen::InGame);
        else if(MenuIndex==1){SettingsReturnScreen=EPWLocalScreen::Pause;SetLocalScreen(EPWLocalScreen::Settings);}
        else if(MenuIndex==2)
        {
            if(GetNetMode()==NM_Client)FrontendStatus=TEXT("Only the host can return the party to the lobby.");
            else ServerReturnToLobby();
        }
        else ConsoleCommand(TEXT("quit"));
    }
}

void APillowWarsPlayerController::BackMenu()
{
    if(LocalScreen==EPWLocalScreen::InGame)SetLocalScreen(EPWLocalScreen::Pause);
    else if(LocalScreen==EPWLocalScreen::MainMenu)return;
    else if(LocalScreen==EPWLocalScreen::Settings)SetLocalScreen(SettingsReturnScreen);
    else if(LocalScreen==EPWLocalScreen::JoinParty){bJoinAttemptPending=false;SetLocalScreen(EPWLocalScreen::MainMenu);}
    else if(LocalScreen==EPWLocalScreen::Pause)SetLocalScreen(EPWLocalScreen::InGame);
    else SetLocalScreen(EPWLocalScreen::MainMenu);
}

void APillowWarsPlayerController::OpenCharacterSelect(){if(LocalScreen==EPWLocalScreen::Lobby)SetLocalScreen(EPWLocalScreen::CharacterSelect);}
void APillowWarsPlayerController::HostStartPressed()
{
    if(LocalScreen!=EPWLocalScreen::Lobby)return;
    if(GetNetMode()==NM_Standalone)
    {
        ServerStartFrontendMatch(true);
        return;
    }
    if(GetNetMode()==NM_Client)
    {
        FrontendStatus=TEXT("Only the host can start the match.");
        return;
    }
    // Stay in the lobby until the server accepts the request and replicates
    // the countdown. A rejected start must never strand this client on Loading.
    ServerStartFrontendMatch(false);
}
void APillowWarsPlayerController::JoinAddressPressed()
{
    JoinAddress=TEXT("127.0.0.1");
    bJoinAttemptPending=false;
    SetLocalScreen(EPWLocalScreen::JoinParty);
}
void APillowWarsPlayerController::JoinPressed()
{
    if(LocalScreen==EPWLocalScreen::Lobby)JoinAddressPressed();
}
void APillowWarsPlayerController::ConnectToHost()
{
    if(JoinAddress.IsEmpty() || JoinAddress.Len()>21)
    {
        FrontendStatus=TEXT("Enter a host IPv4 address, then press Enter.");
        return;
    }
    TArray<FString> HostAndPort;
    JoinAddress.ParseIntoArray(HostAndPort,TEXT(":"),false);
    TArray<FString> Octets;
    if(HostAndPort.Num()<1 || HostAndPort.Num()>2)
    {
        FrontendStatus=TEXT("That address is not valid. Use 192.168.1.20 (or add :7777).");
        return;
    }
    HostAndPort[0].ParseIntoArray(Octets,TEXT("."),false);
    bool bValid=Octets.Num()==4;
    for(const FString& Octet:Octets)
    {
        if(Octet.IsEmpty() || Octet.Len()>3){bValid=false;break;}
        for(const TCHAR Digit:Octet)if(!FChar::IsDigit(Digit))bValid=false;
        if(FCString::Atoi(*Octet)>255)bValid=false;
    }
    if(HostAndPort.Num()==2)
    {
        const FString& Port=HostAndPort[1];
        if(Port.IsEmpty() || Port.Len()>5)bValid=false;
        for(const TCHAR Digit:Port)if(!FChar::IsDigit(Digit))bValid=false;
        const int32 PortNumber=FCString::Atoi(*Port);
        if(PortNumber<1 || PortNumber>65535)bValid=false;
    }
    if(!bValid)
    {
        FrontendStatus=TEXT("That address is not valid. Use 192.168.1.20 (or add :7777).");
        return;
    }
    FrontendStatus=TEXT("Connecting to host...");
    JoinAttemptStartedAt=GetWorld()->GetTimeSeconds();
    bJoinAttemptPending=true;
    ConsoleCommand(FString::Printf(TEXT("open %s"),*JoinAddress));
}

bool APillowWarsPlayerController::InputKey(const FInputKeyParams& Params)
{
    if(IsLocalController() && LocalScreen==EPWLocalScreen::JoinParty && Params.Event==IE_Pressed)
    {
        const FKey DigitKeys[]={EKeys::Zero,EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Six,EKeys::Seven,EKeys::Eight,EKeys::Nine,
            EKeys::NumPadZero,EKeys::NumPadOne,EKeys::NumPadTwo,EKeys::NumPadThree,EKeys::NumPadFour,EKeys::NumPadFive,EKeys::NumPadSix,EKeys::NumPadSeven,EKeys::NumPadEight,EKeys::NumPadNine};
        for(int32 Index=0;Index<UE_ARRAY_COUNT(DigitKeys);++Index)
        {
            if(Params.Key==DigitKeys[Index])
            {
                const TCHAR Digit=TEXT('0')+(Index%10);
                if(JoinAddress.Len()<21)JoinAddress.AppendChar(Digit);
                FrontendStatus.Empty();
                return true;
            }
        }
        if(Params.Key==EKeys::Period || Params.Key==EKeys::Decimal)
        {
            if(JoinAddress.Len()<21)JoinAddress.AppendChar(TEXT('.'));
            FrontendStatus.Empty();
            return true;
        }
        if(Params.Key==EKeys::BackSpace)
        {
            if(!JoinAddress.IsEmpty())JoinAddress.LeftChopInline(1);
            FrontendStatus.Empty();
            return true;
        }
        if(Params.Key==EKeys::Enter){ConnectToHost();return true;}
        if(Params.Key==EKeys::Escape){BackMenu();return true;}
    }
    return Super::InputKey(Params);
}
void APillowWarsPlayerController::SelectionColorPrevious(){if(LocalScreen==EPWLocalScreen::CharacterSelect)ServerAdjustSelection(0,0,-1,0);}
void APillowWarsPlayerController::SelectionColorNext(){if(LocalScreen==EPWLocalScreen::CharacterSelect)ServerAdjustSelection(0,0,1,0);}
void APillowWarsPlayerController::SelectionPillowPrevious(){if(LocalScreen==EPWLocalScreen::CharacterSelect)ServerAdjustSelection(0,0,0,-1);}
void APillowWarsPlayerController::SelectionPillowNext(){if(LocalScreen==EPWLocalScreen::CharacterSelect)ServerAdjustSelection(0,0,0,1);}

void APillowWarsPlayerController::ThrowPressed(){if(IsLocalController()&&LocalScreen!=EPWLocalScreen::InGame)return;if(HasAuthority())ServerThrow_Implementation();else ServerThrow();}
void APillowWarsPlayerController::GuardPressed(){if(!IsLocalController()||LocalScreen!=EPWLocalScreen::InGame)return;if(HasAuthority())ServerSetGuard_Implementation(true);else ServerSetGuard(true);}
void APillowWarsPlayerController::GuardReleased(){if(HasAuthority())ServerSetGuard_Implementation(false);else ServerSetGuard(false);}
void APillowWarsPlayerController::CoverPressed(){if(!IsLocalController()||LocalScreen!=EPWLocalScreen::InGame)return;if(HasAuthority())ServerPlaceCover_Implementation();else ServerPlaceCover();}
void APillowWarsPlayerController::MoveForwardKeyPressed(){bMoveForwardKeyDown=true;UE_LOG(LogTemp,Log,TEXT("PW_MOVE_KEY W down screen=%d pawn=%s"),static_cast<uint8>(LocalScreen),*GetNameSafe(GetPawn()));}
void APillowWarsPlayerController::MoveForwardKeyReleased(){bMoveForwardKeyDown=false;}
void APillowWarsPlayerController::MoveBackwardKeyPressed(){bMoveBackwardKeyDown=true;UE_LOG(LogTemp,Log,TEXT("PW_MOVE_KEY S down screen=%d pawn=%s"),static_cast<uint8>(LocalScreen),*GetNameSafe(GetPawn()));}
void APillowWarsPlayerController::MoveBackwardKeyReleased(){bMoveBackwardKeyDown=false;}
void APillowWarsPlayerController::MoveLeftKeyPressed(){bMoveLeftKeyDown=true;UE_LOG(LogTemp,Log,TEXT("PW_MOVE_KEY A down screen=%d pawn=%s"),static_cast<uint8>(LocalScreen),*GetNameSafe(GetPawn()));}
void APillowWarsPlayerController::MoveLeftKeyReleased(){bMoveLeftKeyDown=false;}
void APillowWarsPlayerController::MoveRightKeyPressed(){bMoveRightKeyDown=true;UE_LOG(LogTemp,Log,TEXT("PW_MOVE_KEY D down screen=%d pawn=%s"),static_cast<uint8>(LocalScreen),*GetNameSafe(GetPawn()));}
void APillowWarsPlayerController::MoveRightKeyReleased(){bMoveRightKeyDown=false;}
void APillowWarsPlayerController::MoveForward(float Value){GamepadMoveForward=Value;}
void APillowWarsPlayerController::MoveRight(float Value){GamepadMoveRight=Value;}
void APillowWarsPlayerController::Turn(float Value){if(LocalScreen==EPWLocalScreen::InGame)AddYawInput(Value*LookSensitivity);}
void APillowWarsPlayerController::LookUp(float Value){if(LocalScreen==EPWLocalScreen::InGame)AddPitchInput(Value*LookSensitivity);}
void APillowWarsPlayerController::JumpPressed(){if(LocalScreen==EPWLocalScreen::InGame)if(ACharacter* PlayerCharacter=Cast<ACharacter>(GetPawn()))PlayerCharacter->Jump();}
void APillowWarsPlayerController::JumpReleased(){if(ACharacter* PlayerCharacter=Cast<ACharacter>(GetPawn()))PlayerCharacter->StopJumping();}
void APillowWarsPlayerController::ServerSetGuard_Implementation(bool bPressed){if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())GM->SetGuard(this,bPressed);}
void APillowWarsPlayerController::ServerPlaceCover_Implementation(){if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())GM->PlaceCover(this);}
void APillowWarsPlayerController::ServerThrow_Implementation()
{
    if(!GetPawn())return;
    if(auto* S=GetPlayerState<APillowWarsPlayerState>();S&&S->bEliminated)return;
    if(auto* M=GetWorld()->GetGameState<APillowWarsMatchState>();M&&!M->Result.IsEmpty())return;
    for(TActorIterator<APillowWarsWeapon> It(GetWorld());It;++It)if(It->GetOwner()==GetPawn()){It->StartThrow(GetWorld()->GetTimeSeconds());break;}
}

void APillowWarsPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    InputComponent->BindKey(EKeys::W,IE_Pressed,this,&APillowWarsPlayerController::MoveForwardKeyPressed);
    InputComponent->BindKey(EKeys::W,IE_Released,this,&APillowWarsPlayerController::MoveForwardKeyReleased);
    InputComponent->BindKey(EKeys::S,IE_Pressed,this,&APillowWarsPlayerController::MoveBackwardKeyPressed);
    InputComponent->BindKey(EKeys::S,IE_Released,this,&APillowWarsPlayerController::MoveBackwardKeyReleased);
    InputComponent->BindKey(EKeys::A,IE_Pressed,this,&APillowWarsPlayerController::MoveLeftKeyPressed);
    InputComponent->BindKey(EKeys::A,IE_Released,this,&APillowWarsPlayerController::MoveLeftKeyReleased);
    InputComponent->BindKey(EKeys::D,IE_Pressed,this,&APillowWarsPlayerController::MoveRightKeyPressed);
    InputComponent->BindKey(EKeys::D,IE_Released,this,&APillowWarsPlayerController::MoveRightKeyReleased);
    InputComponent->BindAxis(TEXT("MoveForward"),this,&APillowWarsPlayerController::MoveForward);
    InputComponent->BindAxis(TEXT("MoveRight"),this,&APillowWarsPlayerController::MoveRight);
    InputComponent->BindAxis(TEXT("Turn"),this,&APillowWarsPlayerController::Turn);
    InputComponent->BindAxis(TEXT("LookUp"),this,&APillowWarsPlayerController::LookUp);
    InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&APillowWarsPlayerController::JumpPressed);
    InputComponent->BindKey(EKeys::SpaceBar,IE_Released,this,&APillowWarsPlayerController::JumpReleased);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom,IE_Pressed,this,&APillowWarsPlayerController::JumpPressed);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom,IE_Released,this,&APillowWarsPlayerController::JumpReleased);
    InputComponent->BindKey(EKeys::Q,IE_Pressed,this,&APillowWarsPlayerController::ThrowPressed);
    InputComponent->BindKey(EKeys::F, IE_Pressed, this, &APillowWarsPlayerController::ServerBeginCharge);
    InputComponent->BindKey(EKeys::F, IE_Released, this, &APillowWarsPlayerController::SwingReleased);
    InputComponent->BindKey(EKeys::R, IE_Pressed, this, &APillowWarsPlayerController::RestartPressed);
    InputComponent->BindKey(EKeys::V, IE_Pressed, this, &APillowWarsPlayerController::CyclePressed);
    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &APillowWarsPlayerController::GuardPressed);
    InputComponent->BindKey(EKeys::G, IE_Released, this, &APillowWarsPlayerController::GuardReleased);
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APillowWarsPlayerController::CoverPressed);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left,IE_Pressed,this,&APillowWarsPlayerController::ServerBeginCharge);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left,IE_Released,this,&APillowWarsPlayerController::SwingReleased);
    InputComponent->BindKey(EKeys::Gamepad_RightShoulder,IE_Pressed,this,&APillowWarsPlayerController::ThrowPressed);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top,IE_Pressed,this,&APillowWarsPlayerController::CyclePressed);
    InputComponent->BindKey(EKeys::Gamepad_LeftShoulder,IE_Pressed,this,&APillowWarsPlayerController::GuardPressed);
    InputComponent->BindKey(EKeys::Gamepad_LeftShoulder,IE_Released,this,&APillowWarsPlayerController::GuardReleased);
    InputComponent->BindKey(EKeys::Gamepad_LeftTrigger,IE_Pressed,this,&APillowWarsPlayerController::CoverPressed);
    InputComponent->BindKey(EKeys::Gamepad_Special_Right,IE_Pressed,this,&APillowWarsPlayerController::RestartPressed);
    InputComponent->BindKey(EKeys::Enter,IE_Pressed,this,&APillowWarsPlayerController::ConfirmMenu);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom,IE_Pressed,this,&APillowWarsPlayerController::ConfirmMenu);
    InputComponent->BindKey(EKeys::Escape,IE_Pressed,this,&APillowWarsPlayerController::BackMenu);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right,IE_Pressed,this,&APillowWarsPlayerController::BackMenu);
    InputComponent->BindKey(EKeys::Up,IE_Pressed,this,&APillowWarsPlayerController::NavigateUp);
    InputComponent->BindKey(EKeys::Down,IE_Pressed,this,&APillowWarsPlayerController::NavigateDown);
    InputComponent->BindKey(EKeys::Left,IE_Pressed,this,&APillowWarsPlayerController::AdjustLeft);
    InputComponent->BindKey(EKeys::Right,IE_Pressed,this,&APillowWarsPlayerController::AdjustRight);
    InputComponent->BindKey(EKeys::Gamepad_DPad_Up,IE_Pressed,this,&APillowWarsPlayerController::NavigateUp);
    InputComponent->BindKey(EKeys::Gamepad_DPad_Down,IE_Pressed,this,&APillowWarsPlayerController::NavigateDown);
    InputComponent->BindKey(EKeys::Gamepad_DPad_Left,IE_Pressed,this,&APillowWarsPlayerController::AdjustLeft);
    InputComponent->BindKey(EKeys::Gamepad_DPad_Right,IE_Pressed,this,&APillowWarsPlayerController::AdjustRight);
    InputComponent->BindKey(EKeys::C,IE_Pressed,this,&APillowWarsPlayerController::OpenCharacterSelect);
    InputComponent->BindKey(EKeys::H,IE_Pressed,this,&APillowWarsPlayerController::HostStartPressed);
    InputComponent->BindKey(EKeys::J,IE_Pressed,this,&APillowWarsPlayerController::JoinPressed);
    InputComponent->BindKey(EKeys::E,IE_Pressed,this,&APillowWarsPlayerController::SelectionColorNext);
    InputComponent->BindKey(EKeys::Q,IE_Pressed,this,&APillowWarsPlayerController::SelectionColorPrevious);
    InputComponent->BindKey(EKeys::Z,IE_Pressed,this,&APillowWarsPlayerController::SelectionPillowPrevious);
    InputComponent->BindKey(EKeys::X,IE_Pressed,this,&APillowWarsPlayerController::SelectionPillowNext);
}
void APillowWarsPlayerController::SwingPressed() { UE_LOG(LogTemp,Log,TEXT("PW_INPUT Swing local=%d authority=%d world=%s"),IsLocalController(),HasAuthority(),*GetWorld()->GetName()); if (HasAuthority()) ServerSwing_Implementation(); else ServerSwing(); }
void APillowWarsPlayerController::SwingReleased() { ServerReleaseCharge(); }
void APillowWarsPlayerController::ServerBeginCharge_Implementation()
{
    if(!GetPawn())return;
    if(auto* S=GetPlayerState<APillowWarsPlayerState>();S&&(S->bEliminated||S->bGuarding))return;
    if(auto* M=GetWorld()->GetGameState<APillowWarsMatchState>();M&&!M->Result.IsEmpty())return;
    const float Now=GetWorld()->GetTimeSeconds();
    for(TActorIterator<APillowWarsWeapon> It(GetWorld());It;++It)if(It->GetOwner()==GetPawn()) {
        if(It->ChargeStartTime>=0||It->IsThrowing(Now)||Now-It->SwingTime<.65f)return;
        It->ChargeStartTime=Now; It->ForceNetUpdate(); break;
    }
}
void APillowWarsPlayerController::ServerReleaseCharge_Implementation()
{
    for(TActorIterator<APillowWarsWeapon> It(GetWorld());It;++It)if(It->GetOwner()==GetPawn()) {
        if(It->ChargeStartTime<0)return;
        const float Held=GetWorld()->GetTimeSeconds()-It->ChargeStartTime;
        It->ChargeStartTime=-100; It->ForceNetUpdate();
        const int32 Sequence=It->AttackSequence;
        ServerSwing_Implementation();
        if(It->AttackSequence!=Sequence&&Held>=.20f) {
            It->AttackVariant=3; It->ReleasedChargeDuration=Held; It->ChargePower=FMath::Clamp(Held/2.f,0.f,1.f); It->ForceNetUpdate();
            UE_LOG(LogTemp,Log,TEXT("PW_UPPERCUT_RELEASE held=%.2f power=%.2f"),Held,It->ChargePower);
        }
        break;
    }
}
void APillowWarsPlayerController::RestartPressed() { if (HasAuthority()) ServerRestart_Implementation(); else ServerRestart(); }
void APillowWarsPlayerController::CyclePressed() { if(LocalScreen==EPWLocalScreen::Lobby){OpenCharacterSelect();return;} if(HasAuthority()) ServerCycle_Implementation(); else ServerCycle(); }
void APillowWarsPlayerController::ServerCycle_Implementation() { if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>()) GM->CycleCharacter(this); }
void APillowWarsPlayerController::ServerSwing_Implementation() { UE_LOG(LogTemp,Log,TEXT("PW_INPUT Swing server received controller=%s pawn=%s"),*GetName(),*GetNameSafe(GetPawn())); if (APillowWarsGameMode* GM = GetWorld()->GetAuthGameMode<APillowWarsGameMode>()) GM->ServerSwing(this); }
void APillowWarsPlayerController::ServerRestart_Implementation() { if (APillowWarsGameMode* GM = GetWorld()->GetAuthGameMode<APillowWarsGameMode>()) GM->RequestReset(this); }
void APillowWarsPlayerController::ServerSetReady_Implementation(bool bNewReady){if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())GM->SetPlayerReady(this,bNewReady);}
void APillowWarsPlayerController::ServerSetSelection_Implementation(int32 CharacterChoice,int32 BodyChoice,int32 ColorChoice,int32 PillowChoice){if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())GM->SetPlayerSelection(this,CharacterChoice,BodyChoice,ColorChoice,PillowChoice);}
void APillowWarsPlayerController::ServerAdjustSelection_Implementation(int32 CharacterDelta,int32 BodyDelta,int32 ColorDelta,int32 PillowDelta)
{
    if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())
        if(auto* PS=GetPlayerState<APillowWarsPlayerState>())
            GM->SetPlayerSelection(this,((PS->CharacterIndex+CharacterDelta)%4+4)%4,((PS->BodySizeIndex+BodyDelta)%3+3)%3,((PS->ColorIndex+ColorDelta)%4+4)%4,((PS->PillowIndex+PillowDelta)%4+4)%4);
}
void APillowWarsPlayerController::ServerStartFrontendMatch_Implementation(bool bPractice){if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())GM->StartFrontendMatch(this,bPractice);}
void APillowWarsPlayerController::ServerReturnToLobby_Implementation(){if(auto* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>())GM->ReturnToLobby(this);}
