#include "PillowWarsMatchState.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "PillowWarsPracticeTarget.h"
#include "PillowWarsWeapon.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "PillowWarsPlayerController.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"

APillowWarsHUD::APillowWarsHUD()
{
    static ConstructorHelpers::FObjectFinder<UTexture2D> Art(TEXT("/Game/PillowWars/UI/T_PillowWarsTitleArt.T_PillowWarsTitleArt"));
    MenuArtwork=Art.Object;
}

void APillowWarsPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APillowWarsPlayerState, Daze);
    DOREPLIFETIME(APillowWarsPlayerState, bEliminated);
    DOREPLIFETIME(APillowWarsPlayerState, RoundWins);
    DOREPLIFETIME(APillowWarsPlayerState, bReady);
    DOREPLIFETIME(APillowWarsPlayerState, Stuffing);
    DOREPLIFETIME(APillowWarsPlayerState, bStuffingResting);
    DOREPLIFETIME(APillowWarsPlayerState, bGuarding);
    DOREPLIFETIME(APillowWarsPlayerState, GuardPulseServerTime);
    DOREPLIFETIME(APillowWarsPlayerState, RoundHits);
    DOREPLIFETIME(APillowWarsPlayerState, RoundHitsTaken);
    DOREPLIFETIME(APillowWarsPlayerState, RoundGuards);
    DOREPLIFETIME(APillowWarsPlayerState, RoundCoversPlaced);
    DOREPLIFETIME(APillowWarsPlayerState, RoundCoversBroken);
    DOREPLIFETIME(APillowWarsPlayerState, RoundStuffingCollected);
    DOREPLIFETIME(APillowWarsPlayerState, CharacterIndex);
    DOREPLIFETIME(APillowWarsPlayerState, BodySizeIndex);
    DOREPLIFETIME(APillowWarsPlayerState, ColorIndex);
    DOREPLIFETIME(APillowWarsPlayerState, PillowIndex);
}
void APillowWarsMatchState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APillowWarsMatchState, Result);
    DOREPLIFETIME(APillowWarsMatchState, RoundNumber);
    DOREPLIFETIME(APillowWarsMatchState, RematchVotes);
    DOREPLIFETIME(APillowWarsMatchState, PlayersNeededToRematch);
    DOREPLIFETIME(APillowWarsMatchState, MatchPhase);
    DOREPLIFETIME(APillowWarsMatchState, CountdownEndServerTime);
    DOREPLIFETIME(APillowWarsMatchState, MatchEndServerTime);
    DOREPLIFETIME(APillowWarsMatchState, LobbyStatus);
    DOREPLIFETIME(APillowWarsMatchState, LastMoment);
    DOREPLIFETIME(APillowWarsMatchState, LastMomentServerTime);
}
void APillowWarsHUD::DrawHUD()
{
    Super::DrawHUD();
    if(!Canvas||!bShowHUD)return;
    const float W=Canvas->ClipX,H=Canvas->ClipY,S=FMath::Clamp(FMath::Min(W/1280.f,H/720.f),.45f,1.35f);
    const auto FitText=[&](const FString& Text,FLinearColor Color,float X,float Y,float Width,float Scale)
    {
        float TW=0,TH=0;GetTextSize(Text,TW,TH,nullptr,Scale);
        DrawText(Text,Color,X,Y,nullptr,Scale*FMath::Min(1.f,Width/FMath::Max(TW,1.f)));
    };
    const auto* State = GetWorld()->GetGameState<APillowWarsMatchState>();
    if (!State) return;
    const float Now=State->GetServerWorldTimeSeconds();
    const auto* PWController=Cast<APillowWarsPlayerController>(PlayerOwner);
    if(PWController && PWController->GetLocalScreen()!=EPWLocalScreen::InGame)
    {
        const EPWLocalScreen Screen=PWController->GetLocalScreen();
        if(MenuArtwork)DrawTexture(MenuArtwork,0,0,W,H,0,0,1,1,FLinearColor(.55f,.55f,.65f,1),BLEND_Translucent);
        const bool bContrast=PWController->GetHighContrast();
        DrawRect(bContrast?FLinearColor(0,0,0,.72f):FLinearColor(.01f,.015f,.045f,.48f),0,0,W,H);
        DrawRect(bContrast?FLinearColor(.02f,.02f,.02f,.96f):FLinearColor(.08f,.16f,.34f,.84f),W*.12f,H*.10f,W*.76f,H*.80f);
        DrawRect(FLinearColor(.16f,.06f,.28f,.85f),W*.12f,H*.10f,W*.76f,86*S);
        DrawText(TEXT("PILLOW WARS"),FLinearColor(.55f,.92f,1),W*.5f-145*S,H*.125f,nullptr,1.8f*S);
        DrawText(TEXT("DREAM RESONANCE"),FLinearColor(.92f,.62f,1),W*.5f-110*S,H*.19f,nullptr,.80f*S);
        const auto DrawMenu=[&](const TArray<FString>& Items,float StartY)
        {
            // Menus share a fixed-height panel; calculate row spacing from the
            // current viewport so the last option never collides with the footer
            // or falls below the bottom edge at short window sizes.
            // Keep a deliberate quiet zone above the footer. The previous
            // calculation let the last two settings rows crowd the hint text
            // in short PIE windows and laptop-height viewports.
            const float FooterY=H*.76f;
            const float Count=FMath::Max(1,Items.Num());
            const float RowStep=FMath::Min(54*S,FMath::Max(22*S,(FooterY-StartY-34*S)/Count));
            const float RowHeight=FMath::Min(43*S,RowStep*.76f);
            for(int32 I=0;I<Items.Num();++I)
            {
                const bool Selected=I==PWController->GetMenuIndex();
                const float RowY=StartY+I*RowStep;
                if(Selected)DrawRect(bContrast?FLinearColor(1,.55f,0,1):FLinearColor(.25f,.65f,1,.55f),W*.31f,RowY+(RowStep-RowHeight)*.5f,W*.38f,RowHeight);
                DrawText(Items[I],Selected?FLinearColor::White:FLinearColor(.72f,.78f,.9f),W*.35f,RowY+(RowStep-16*S)*.5f,nullptr,.95f*S);
            }
        };
        if(Screen==EPWLocalScreen::MainMenu)
        {
            DrawMenu({TEXT("PLAY / PARTY LOBBY"),TEXT("JOIN PARTY"),TEXT("PRACTICE"),TEXT("SETTINGS"),TEXT("TUTORIAL"),TEXT("CREDITS"),TEXT("QUIT")},H*.28f);
            DrawText(TEXT("Arrow keys / D-pad navigate   |   Enter / A select"),FLinearColor(.65f,.78f,.95f),W*.31f,H*.82f,nullptr,.66f*S);
        }
        else if(Screen==EPWLocalScreen::Lobby)
        {
            DrawText(TEXT("DREAMER LOBBY"),FLinearColor::White,W*.18f,H*.27f,nullptr,1.2f*S);
            DrawText(TEXT("Enter / A: ready   C / Y: choose character   H: open LAN host / start   J: join by address"),FLinearColor(.75f,.84f,1),W*.18f,H*.33f,nullptr,.62f*S);
            int32 LobbyIndex=0;
            for(const APlayerState* Player:State->PlayerArray)
                if(const auto* Fighter=Cast<APillowWarsPlayerState>(Player))
                {
                    const FString Body=Fighter->BodySizeIndex==0?TEXT("NORMAL"):Fighter->BodySizeIndex==1?TEXT("BIG"):TEXT("XL");
                    const float LX=W*(LobbyIndex<5?.18f:.52f),LY=H*.40f+(LobbyIndex%5)*H*.058f;
                    const FString Name=Fighter->GetPlayerName().Left(18);
                    FitText(FString::Printf(TEXT("%s  |  %s  |  %s"),*Name,Fighter->bReady?TEXT("READY"):TEXT("CHOOSING"),*Body),Fighter->bReady?FLinearColor(.45f,1,.65f):FLinearColor(1,.82f,.35f),LX,LY,W*.29f,.78f*S);
                    ++LobbyIndex;
                }
            FitText(State->LobbyStatus,FLinearColor(.65f,.95f,1),W*.18f,H*.73f,W*.64f,.72f*S);
            DrawText(TEXT("Host: share your IPv4 address. Guests: choose JOIN PARTY and enter it."),FLinearColor(.8f,.82f,.95f),W*.22f,H*.79f,nullptr,.63f*S);
        }
        else if(Screen==EPWLocalScreen::JoinParty)
        {
            DrawText(TEXT("JOIN A PARTY"),FLinearColor::White,W*.34f,H*.28f,nullptr,1.25f*S);
            DrawText(TEXT("HOST IPv4 ADDRESS"),FLinearColor(.78f,.85f,1),W*.34f,H*.40f,nullptr,.75f*S);
            DrawRect(FLinearColor(.02f,.035f,.08f,.95f),W*.29f,H*.46f,W*.42f,58*S);
            DrawText(PWController->GetJoinAddress()+TEXT("_"),FLinearColor(.55f,.95f,1),W*.34f,H*.475f,nullptr,1.05f*S);
            DrawText(TEXT("Type the host address with number keys and periods."),FLinearColor(.82f,.86f,.96f),W*.25f,H*.58f,nullptr,.70f*S);
            DrawText(TEXT("Enter: connect     Backspace: edit     Esc: back"),FLinearColor(.65f,1,.75f),W*.29f,H*.67f,nullptr,.70f*S);
            DrawText(TEXT("Same computer: 127.0.0.1     Local network: 192.168.1.20"),FLinearColor(.73f,.79f,.90f),W*.20f,H*.76f,nullptr,.65f*S);
        }
        else if(Screen==EPWLocalScreen::CharacterSelect)
        {
            const APillowWarsPlayerState* PS=PlayerOwner?PlayerOwner->GetPlayerState<APillowWarsPlayerState>():nullptr;
            DrawText(TEXT("CHOOSE YOUR DREAMER"),FLinearColor::White,W*.27f,H*.28f,nullptr,1.2f*S);
            if(PS)
            {
                const TCHAR* Names[]={TEXT("BLUE STARGAZER"),TEXT("RED LIGHTNING"),TEXT("GREEN BLANKET SCOUT"),TEXT("PURPLE DREAM PRANKSTER")};
                const TCHAR* Bodies[]={TEXT("NORMAL"),TEXT("BIG"),TEXT("EXTRA LARGE")};
                DrawText(FString::Printf(TEXT("<  %s  >"),Names[FMath::Clamp(PS->CharacterIndex,0,3)]),FLinearColor(.55f,.92f,1),W*.31f,H*.40f,nullptr,1.05f*S);
                DrawText(FString::Printf(TEXT("BODY: %s   |   COLOR %d   |   PILLOW %d"),Bodies[FMath::Clamp(PS->BodySizeIndex,0,2)],PS->ColorIndex+1,PS->PillowIndex+1),FLinearColor(.9f,.75f,1),W*.29f,H*.49f,nullptr,.80f*S);
            }
            DrawText(TEXT("Left/Right: hero   Up/Down: body   Q/E: color   Z/X: pillow"),FLinearColor::White,W*.23f,H*.61f,nullptr,.72f*S);
            DrawText(TEXT("Enter / A: confirm and return to lobby"),FLinearColor(.65f,1,.75f),W*.30f,H*.69f,nullptr,.72f*S);
        }
        else if(Screen==EPWLocalScreen::Settings)
        {
            DrawText(TEXT("SETTINGS"),FLinearColor::White,W*.39f,H*.27f,nullptr,1.25f*S);
            const TCHAR* QualityNames[]={TEXT("LOW"),TEXT("MEDIUM"),TEXT("HIGH"),TEXT("EPIC"),TEXT("CINEMATIC")};
            const int32 Quality=PWController->GetGraphicsQuality();
            DrawMenu({FString::Printf(TEXT("MASTER VOLUME   %.0f%%"),PWController->GetMasterVolume()*100),FString::Printf(TEXT("LOOK SENSITIVITY %.1f"),PWController->GetLookSensitivity()),FString::Printf(TEXT("GRAPHICS        %s"),Quality>=0&&Quality<=4?QualityNames[Quality]:TEXT("CUSTOM")),FString::Printf(TEXT("WINDOW MODE     %s"),PWController->GetFullscreen()?TEXT("FULLSCREEN"):TEXT("WINDOWED")),FString::Printf(TEXT("HIGH CONTRAST   %s"),PWController->GetHighContrast()?TEXT("ON"):TEXT("OFF")),TEXT("RESTORE DEFAULTS"),TEXT("BACK")},H*.33f);
            DrawText(TEXT("Left/Right adjust   |   Enter select   |   changes save immediately"),FLinearColor(.68f,.80f,1),W*.27f,H*.82f,nullptr,.68f*S);
        }
        else if(Screen==EPWLocalScreen::Tutorial)
        {
            DrawText(TEXT("HOW TO DREAM-FIGHT"),FLinearColor::White,W*.28f,H*.27f,nullptr,1.15f*S);
            TArray<FString> Tips={TEXT("WASD / left stick: move     Mouse / right stick: look"),TEXT("Space / A: jump across furniture and recovery platforms"),TEXT("Tap F / X: cycle three soft-pillow swings"),TEXT("Hold F / X: charge the circular uppercut"),TEXT("Q / RB: throw the regenerating back pillow"),TEXT("Hits build Daze; higher Daze creates stronger knockback"),TEXT("Counter during the purple Resonance pulse for bonus force"),TEXT("Fall below the room boundary and you are eliminated"),TEXT("Last dreamer standing wins; first to three rounds is champion")};
            Tips.Insert(TEXT("G / LB: guard   E / LT: build a temporary pillow shield"),5);
            Tips.Insert(TEXT("Stop beside a feather pile to crouch and refill; moving cancels"),6);
            float TY=H*.36f;for(const FString& Tip:Tips){FitText(Tip,FLinearColor(.82f,.88f,1),W*.20f,TY,W*.60f,.69f*S);TY+=FMath::Min(36*S,H*.42f/Tips.Num());}
            DrawText(TEXT("Esc / B: back"),FLinearColor(.65f,1,.75f),W*.20f,H*.82f,nullptr,.70f*S);
        }
        else if(Screen==EPWLocalScreen::Credits)
        {
            DrawText(TEXT("CREDITS"),FLinearColor::White,W*.40f,H*.29f,nullptr,1.25f*S);
            DrawText(TEXT("Design and direction: Pillow Wars team"),FLinearColor(.75f,.87f,1),W*.31f,H*.42f,nullptr,.80f*S);
            DrawText(TEXT("Built with Unreal Engine 5.5.3"),FLinearColor(.86f,.72f,1),W*.34f,H*.49f,nullptr,.76f*S);
            DrawText(TEXT("Esc / B: back"),FLinearColor(.65f,1,.75f),W*.40f,H*.66f,nullptr,.70f*S);
        }
        else if(Screen==EPWLocalScreen::Loading)
        {
            const float Pulse=.5f+.5f*FMath::Sin(Now*4.f);
            DrawText(TEXT("ENTERING THE DREAM..."),FLinearColor(.6f+.3f*Pulse,.8f,1),W*.30f,H*.42f,nullptr,1.35f*S);
            DrawRect(FLinearColor(.04f,.05f,.10f,1),W*.25f,H*.55f,W*.50f,18*S);
            DrawRect(FLinearColor(.55f,.3f,1,1),W*.25f,H*.55f,W*.50f*PWController->GetLoadingProgress(),18*S);
            DrawText(PWController->GetLoadingTip(),FLinearColor(.8f,.86f,1),W*.26f,H*.62f,nullptr,.70f*S);
        }
        else if(Screen==EPWLocalScreen::Pause)
        {
            DrawText(TEXT("PAUSED"),FLinearColor::White,W*.40f,H*.29f,nullptr,1.3f*S);
            DrawMenu({TEXT("RESUME"),TEXT("SETTINGS"),TEXT("RETURN TO LOBBY"),TEXT("QUIT")},H*.40f);
        }
        if(!PWController->GetFrontendStatus().IsEmpty())FitText(PWController->GetFrontendStatus(),FLinearColor(1,.65f,.35f),W*.20f,H*.87f,W*.60f,.68f*S);
        return;
    }

    if(State->MatchPhase==EPWMatchPhase::Countdown)
    {
        const int32 Count=FMath::Max(1,FMath::CeilToInt(State->CountdownEndServerTime-Now));
        DrawText(FString::FromInt(Count),FLinearColor(1,.8f,.25f),W*.5f-30*S,H*.35f,nullptr,3.f*S);
    }
    else if(State->MatchPhase==EPWMatchPhase::Playing && State->MatchEndServerTime>Now)
    {
        const int32 Remaining=FMath::CeilToInt(State->MatchEndServerTime-Now);
        DrawText(FString::Printf(TEXT("%02d:%02d"),Remaining/60,Remaining%60),FLinearColor::White,W*.5f-34*S,22*S,nullptr,.9f*S);
    }
    DrawRect(FLinearColor(.015f,.025f,.055f,.88f),18*S,16*S,420*S,116*S);
    DrawText(TEXT("PILLOW WARS"),FLinearColor(.55f,.9f,1),30*S,23*S,nullptr,1.32f*S);
    DrawText(FString::Printf(TEXT("DREAM RESONANCE  |  ROUND %d"),State->RoundNumber),FLinearColor(.75f,.65f,1),30*S,49*S,nullptr,.78f*S);
    DrawText(TEXT("WASD / LS move  |  Space / A jump  |  F / X attack"),FLinearColor::White,30*S,76*S,nullptr,.70f*S);
    DrawText(TEXT("G / LB guard  |  E / LT pillow cover  |  Q / RB throw"),FLinearColor(.82f,.86f,.95f),30*S,99*S,nullptr,.60f*S);

    const APillowWarsPlayerState* LocalState=PlayerOwner?PlayerOwner->GetPlayerState<APillowWarsPlayerState>():nullptr;
    const float Daze=LocalState?LocalState->Daze:0;
    const FLinearColor DazeColor=Daze<50?FLinearColor(.2f,.9f,.45f):Daze<80?FLinearColor(1,.72f,.12f):FLinearColor(1,.2f,.18f);
    const float PanelY=H-178*S;
    DrawRect(FLinearColor(.015f,.025f,.055f,.88f),18*S,PanelY,380*S,160*S);
    DrawText(FString::Printf(TEXT("DAZE %.0f%%     WINS %d/3"),Daze,LocalState?LocalState->RoundWins:0),FLinearColor::White,30*S,PanelY+10*S,nullptr,.78f*S);
    DrawRect(FLinearColor(.08f,.08f,.11f,1),30*S,PanelY+38*S,320*S,17*S);
    DrawRect(DazeColor,32*S,PanelY+40*S,316*S*FMath::Clamp(Daze/100.f,0.f,1.f),13*S);
    const float Stuffing=LocalState?LocalState->Stuffing:0.f;
    DrawText(FString::Printf(TEXT("STUFFING %.0f%%  %s"),Stuffing,LocalState&&LocalState->bGuarding?TEXT("GUARDING"):LocalState&&LocalState->bStuffingResting?TEXT("STUFFING PILLOW"):TEXT("")),FLinearColor(.6f,1,.85f),30*S,PanelY+57*S,nullptr,.66f*S);
    DrawRect(FLinearColor(.08f,.08f,.11f,1),30*S,PanelY+79*S,316*S,9*S);
    DrawRect(FLinearColor(.2f,.8f,.68f,1),32*S,PanelY+81*S,312*S*FMath::Clamp(Stuffing/100.f,0.f,1.f),5*S);
    DrawText(TEXT("Rest beside feathers: refill +24/s | Move to cancel"),FLinearColor(.76f,.82f,.88f),30*S,PanelY+136*S,nullptr,.55f*S);

    APillowWarsWeapon* LocalWeapon=nullptr;
    for(TActorIterator<APillowWarsWeapon> It(GetWorld());It;++It)if(PlayerOwner&&It->GetOwner()==PlayerOwner->GetPawn()){LocalWeapon=*It;break;}
    if(LocalWeapon)
    {
        const float Wait=FMath::Max(0.f,LocalWeapon->ThrowReadyTime-Now);
        DrawText(Wait>0?FString::Printf(TEXT("THROW %.1fs"),Wait):TEXT("THROW READY"),Wait>0?FLinearColor(.6f,.7f,.8f):FLinearColor(0,1,1),30*S,PanelY+106*S,nullptr,.68f*S);
        DrawText(TEXT("RESONANCE"),FLinearColor(.8f,.65f,1),160*S,PanelY+106*S,nullptr,.64f*S);
        DrawRect(FLinearColor(.08f,.08f,.11f,1),250*S,PanelY+110*S,98*S,11*S);
        DrawRect(FLinearColor(.7f,.28f,1),252*S,PanelY+112*S,94*S*LocalWeapon->ResonanceCharge/100.f,7*S);
        if(LocalWeapon->IsResonanceWindow(Now))
            DrawText(TEXT("RESONANCE NOW!"),FLinearColor(1,.55f,1),W*.5f-100*S,H*.70f,nullptr,1.15f*S);
        if(LocalWeapon->ChargeStartTime>=0)
        {
            const float Power=FMath::Clamp((Now-LocalWeapon->ChargeStartTime)/2.f,0.f,1.f);
            DrawRect(FLinearColor(.03f,.03f,.05f,.92f),W*.5f-162*S,H-70*S,324*S,31*S);
            DrawRect(FLinearColor(1,.48f,.08f),W*.5f-157*S,H-65*S,314*S*Power,21*S);
            DrawText(Power>=1.f?TEXT("FULL POWER  |  RELEASE F / X"):FString::Printf(TEXT("WINDING UP  %.0f%%  |  RELEASE F / X"),100*Power),Power>=1.f?FLinearColor(.55f,1.f,.72f):FLinearColor(1.f,.82f,.55f),W*.5f-128*S,H-97*S,nullptr,.82f*S);
        }
        const float HitAge=Now-LocalWeapon->HitPulseTime;
        const float ContactAge=Now-LocalWeapon->ContactTime;
        if(HitAge>=0&&HitAge<.18f)DrawRect(FLinearColor(1,.08f,.08f,.17f*(1-HitAge/.18f)),0,0,W,H);
        if(ContactAge>=0&&ContactAge<.24f)
            DrawText(LocalWeapon->ResonanceAttackPower>0?TEXT("RESONANCE STRIKE!"):TEXT("PILLOW HIT!"),
                LocalWeapon->ResonanceAttackPower>0?FLinearColor(1,.4f,1):FLinearColor(1,.92f,.35f),W*.5f-94*S,H*.34f,nullptr,1.25f*S);
    }
    if(LocalState)
    {
        const float GuardAge=Now-LocalState->GuardPulseServerTime;
        if(GuardAge>=0.f&&GuardAge<.55f)
            DrawText(TEXT("GUARD ABSORBED THE HIT"),FLinearColor(.55f,1,.78f,1.f-GuardAge/.7f),W*.5f-112*S,H*.40f,nullptr,.88f*S);
    }

    float Y=22*S;
    DrawRect(FLinearColor(.015f,.025f,.055f,.82f),W-260*S,16*S,242*S,FMath::Max(62.f,38.f+State->PlayerArray.Num()*24.f)*S);
    DrawText(TEXT("DREAMERS"),FLinearColor(.55f,.9f,1),W-246*S,Y,nullptr,.78f*S);Y+=27*S;
    for (const auto& Player : State->PlayerArray)
    {
        if (const auto* Fighter = Cast<APillowWarsPlayerState>(Player))
        {
            FitText(FString::Printf(TEXT("%s  %.0f%%  [%d] %s"),*Fighter->GetPlayerName().Left(18),Fighter->Daze,Fighter->RoundWins,
                Fighter->bEliminated?TEXT("OUT"):TEXT("")),Fighter->bEliminated?FLinearColor(.5f,.5f,.55f):FLinearColor(1,.9f,.35f),W-246*S,Y,222*S,.66f*S);
            Y+=22*S;
        }
    }
    if(!State->Result.IsEmpty())
    {
        DrawRect(FLinearColor(.025f,.02f,.07f,.94f),W*.15f,H*.39f,W*.70f,158*S);
        FitText(State->Result,FLinearColor(.55f,1,.72f),W*.19f,H*.43f,W*.62f,1.25f*S);
        DrawText(FString::Printf(TEXT("REMATCH VOTES %d / %d"),State->RematchVotes,State->PlayersNeededToRematch),FLinearColor::White,W*.36f,H*.50f,nullptr,.80f*S);
        if(LocalState)DrawText(FString::Printf(TEXT("YOUR ROUND  Hits %d | Taken %d | Guards %d | Covers %d/%d | Pickups %d"),LocalState->RoundHits,LocalState->RoundHitsTaken,LocalState->RoundGuards,LocalState->RoundCoversPlaced,LocalState->RoundCoversBroken,LocalState->RoundStuffingCollected),FLinearColor(.72f,.9f,1),W*.19f,H*.555f,nullptr,.62f*S);
    }
    else if(State->PlayerArray.Num()<2)
        DrawText(TEXT("PRACTICE MODE - A SECOND DREAMER STARTS THE MATCH"),FLinearColor(.6f,.85f,1),W*.5f-245*S,146*S,nullptr,.72f*S);
    if(Now<5.f&&State->Result.IsEmpty())
    {
        DrawRect(FLinearColor(.015f,.02f,.07f,.90f),W*.20f,H*.32f,W*.60f,150*S);
        DrawText(TEXT("PILLOW WARS"),FLinearColor(.55f,.9f,1),W*.5f-135*S,H*.36f,nullptr,1.8f*S);
        DrawText(TEXT("DREAM RESONANCE"),FLinearColor(.85f,.55f,1),W*.5f-115*S,H*.42f,nullptr,1.05f*S);
        DrawText(TEXT("Be the last dreamer on the bed"),FLinearColor::White,W*.5f-145*S,H*.48f,nullptr,.78f*S);
    }

    const float MomentAge=Now-State->LastMomentServerTime;
    if(State->LastMoment.Len()&&MomentAge>=0.f&&MomentAge<2.5f)
        DrawText(State->LastMoment,FLinearColor(1.f,.9f,.48f,1.f-MomentAge/3.f),W*.5f-130*S,155*S,nullptr,.82f*S);

    Y=H-28*S;
    for (TActorIterator<APillowWarsPracticeTarget> It(GetWorld()); It; ++It)
    {
        const APawn* Viewer = PlayerOwner ? PlayerOwner->GetPawn() : nullptr;
        const float Range = Viewer ? FVector::Dist(Viewer->GetActorLocation(), It->GetActorLocation()) : 0;
        if(Range<700)
            DrawText(FString::Printf(TEXT("TRAINING DUMMY  |  HITS %d  |  DAZE %.0f%%  |  %.0f cm"),It->Hits,It->Daze,Range),FLinearColor(0,1,1),W*.5f-190*S,Y,nullptr,.62f*S);
    }
}
