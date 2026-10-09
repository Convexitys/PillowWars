#include "PillowWarsGameMode.h"
#include "PillowWarsPlayerController.h"
#include "PillowWarsBotController.h"
#include "PillowWarsMatchState.h"
#include "PillowWarsWeapon.h"
#include "PillowWarsProjectile.h"
#include "PillowWarsPracticeTarget.h"
#include "PillowWarsCover.h"
#include "PillowWarsStuffingPickup.h"
#include "PillowWarsFeather.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

APillowWarsGameMode::APillowWarsGameMode()
{
    PlayerControllerClass = APillowWarsPlayerController::StaticClass();
    PlayerStateClass = APillowWarsPlayerState::StaticClass();
    GameStateClass = APillowWarsMatchState::StaticClass();
    HUDClass = APillowWarsHUD::StaticClass();
    CoverClass = APillowWarsCover::StaticClass();
    PrimaryActorTick.bCanEverTick = true;
    static ConstructorHelpers::FClassFinder<APawn> PawnFinder(TEXT("/Game/PillowWars/Blueprints/BP_PillowFighter"));
    if (PawnFinder.Succeeded()) DefaultPawnClass = PawnFinder.Class;
}

void APillowWarsGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if (const auto* Match = GetGameState<APillowWarsMatchState>(); Match && Match->MatchPhase == EPWMatchPhase::RoundOver)
        ClearRematchVotes();
    if(NewPlayer&&NewPlayer->PlayerState)
    {
        const int32 DisplayNumber=GameState?GameState->PlayerArray.Num():1;
        NewPlayer->PlayerState->SetPlayerName(FString::Printf(TEXT("Dreamer %d"),FMath::Max(1,DisplayNumber)));
        if(auto* Fighter=NewPlayer->GetPlayerState<APillowWarsPlayerState>())Fighter->CharacterIndex=FMath::Abs(Fighter->GetPlayerId())%4;
        if(const auto* Match=GetGameState<APillowWarsMatchState>();HasActorBegunPlay() && Match && (Match->MatchPhase==EPWMatchPhase::Playing||Match->MatchPhase==EPWMatchPhase::RoundOver) && !bPracticeSession)
        {
            if(auto* Fighter=NewPlayer->GetPlayerState<APillowWarsPlayerState>()){Fighter->Health=0.f;Fighter->bEliminated=true;Fighter->ForceNetUpdate();}
            if(APawn* Pawn=NewPlayer->GetPawn())Pawn->Destroy();
        }
    }
}

void APillowWarsGameMode::RestartPlayer(AController* NewPlayer)
{
    const auto* Match=GetGameState<APillowWarsMatchState>();
    if(HasActorBegunPlay()&&Match&&!bPracticeSession&&(Match->MatchPhase==EPWMatchPhase::Playing||Match->MatchPhase==EPWMatchPhase::RoundOver))return;
    Super::RestartPlayer(NewPlayer);
}

void APillowWarsGameMode::Logout(AController* Exiting)
{
    const bool bRoundOver = GetGameState<APillowWarsMatchState>() &&
        GetGameState<APillowWarsMatchState>()->MatchPhase == EPWMatchPhase::RoundOver;
    if (APlayerController* Player=Cast<APlayerController>(Exiting))
    {
        SettleResources(Player,GetWorld()->GetTimeSeconds());RetireOwner(Player);
        LastSwingTime.Remove(Player);LastCombatAction.Remove(Player);LastCoverTime.Remove(Player);LastResourceNetUpdate.Remove(Player);
        if(ACharacter* Pawn=Cast<ACharacter>(Player->GetPawn())){LastPickupSpawnTime.Remove(Pawn);ClearCovers(Pawn);}
    }
    Super::Logout(Exiting);
    if (bRoundOver)
    {
        int32 ConnectedPlayers = 0;
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
            if (It->Get() && It->Get() != Exiting && It->Get()->PlayerState && !It->Get()->PlayerState->IsABot()) ++ConnectedPlayers;
        ClearRematchVotes(ConnectedPlayers);
    }
}

void APillowWarsGameMode::ClearRematchVotes(int32 ConnectedPlayers)
{
    RematchVoters.Empty();
    if (auto* Match = GetGameState<APillowWarsMatchState>())
    {
        Match->RematchVotes = 0;
        if (ConnectedPlayers < 0)
        {
            ConnectedPlayers = 0;
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
                if (It->Get() && It->Get()->PlayerState && !It->Get()->PlayerState->IsABot()) ++ConnectedPlayers;
        }
        Match->PlayersNeededToRematch = ConnectedPlayers;
        Match->ForceNetUpdate();
    }
}

void APillowWarsGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority()) return;
    if(auto* Match=GetGameState<APillowWarsMatchState>())
    {
        const float Now=GetWorld()->GetTimeSeconds();
        if(Match->MatchPhase==EPWMatchPhase::Countdown && Now>=Match->CountdownEndServerTime)
        {
            Match->MatchPhase=EPWMatchPhase::Playing;
            Match->MatchEndServerTime=Now+MatchDuration;
            Match->LobbyStatus=TEXT("Fight!");
            Match->ForceNetUpdate();
            for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(It->Get())ApplyPlayerSelection(It->Get());
        }
        if(Match->MatchPhase==EPWMatchPhase::Playing && Match->MatchEndServerTime>0 && Now>=Match->MatchEndServerTime && !bMatchOver)
        {
            // A clock expiring must not end a last-standing match without a victor.
            Match->MatchEndServerTime=0.f;
            AnnounceMoment(TEXT("OVERTIME: fight until one dreamer remains!"));
            Match->ForceNetUpdate();
        }
    }
    if (bMatchOver) return;
    const auto* CurrentMatch=GetGameState<APillowWarsMatchState>();
    const bool bGameplayActive=CurrentMatch&&CurrentMatch->MatchPhase==EPWMatchPhase::Playing;
    if(bGameplayActive&&!bResourceOpen)OpenResources();
    const float GameplayNow=GetWorld()->GetTimeSeconds();
    if(bGameplayActive&&!bWasGameplayActive)
    {
        AmbientPileTarget=FMath::RandRange(3,4);
        NextAmbientPileSpawnTime=GameplayNow;
    }
    bWasGameplayActive=bGameplayActive;
    if(bGameplayActive&&!bTestDisableAmbient&&GameplayNow>=NextAmbientPileSpawnTime)
    {
        int32 ActivePiles=0;
        for(const auto& Pair:PileBook)if(Pair.Value.bAmbient)++ActivePiles;
        if(ActivePiles<AmbientPileTarget)
        {
            const bool bSpawned=SpawnAmbientStuffingPile();
            NextAmbientPileSpawnTime=GameplayNow+(bSpawned?.45f:1.25f);
        }
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        ACharacter* Pawn = It->Get() ? Cast<ACharacter>(It->Get()->GetPawn()) : nullptr;
        if (Pawn && !Weapons.Contains(Pawn))
        {
            FActorSpawnParameters Params;
            Params.Owner = Pawn;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            APillowWarsWeapon* Weapon=GetWorld()->SpawnActor<APillowWarsWeapon>(Pawn->GetActorLocation(), Pawn->GetActorRotation(), Params);
            if (Weapon)
            {
                const auto* Player=It->Get()->GetPlayerState<APillowWarsPlayerState>();
                Weapon->SetVariant(4+(Player?Player->CharacterIndex:Weapons.Num()%4));
            }
            Weapons.Add(Pawn,Weapon);
            ApplyPlayerSelection(It->Get());
        }
        if(!bGameplayActive)
        {
            if(auto* RestState=It->Get()->GetPlayerState<APillowWarsPlayerState>();RestState&&RestState->bStuffingResting)
            {RestState->bStuffingResting=false;RestState->ForceNetUpdate();}
            if(Pawn&&Pawn->bIsCrouched)Pawn->UnCrouch();
            continue;
        }
        if (auto* State=It->Get()->GetPlayerState<APillowWarsPlayerState>())
        {
            SettleResources(It->Get(),GetWorld()->GetTimeSeconds());
        }
        const auto* Fighter=It->Get()->GetPlayerState<APillowWarsPlayerState>();
        if (Pawn && (Pawn->GetActorLocation().Z < -700.0f || (Fighter && (Fighter->Health<=0.f||Fighter->bEliminated))))
        {
            RetireOwner(It->Get());
            FighterStates.FindOrAdd(Pawn).Eliminated = true;
            ClearCovers(Pawn);
            if (auto* State = Pawn->GetPlayerState<APillowWarsPlayerState>())
            {
                State->bEliminated = true;
                State->Health=0.f;
                State->bGuarding=false; State->bStuffingResting=false;
                State->ForceNetUpdate();
            }
            UE_LOG(LogTemp, Log, TEXT("Pillow Wars elimination: %s"), *Pawn->GetName());
            if (APillowWarsWeapon* Weapon = Weapons.FindRef(Pawn)) Weapon->Destroy();
            Weapons.Remove(Pawn);
            Pawn->Destroy();
        }
    }
    if(!bGameplayActive)return;
    int32 Alive = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (It->Get() && It->Get()->GetPawn()) ++Alive;
    if (Alive >= 2 || bPracticeSession) bRoundStarted = true;
    // Practice intentionally has one dreamer. Do not immediately complete the
    // round during the first Playing tick; solo practice remains active until
    // the player chooses to restart or return to the menu.
    if (bRoundStarted && Alive <= 1 && !bPracticeSession)
    {
        bMatchOver = true;
        CloseResources(TEXT("round_over"));ClearCovers();ClearStuffingPickups();
        if (auto* Match = GetGameState<APillowWarsMatchState>())
        {
            Match->MatchPhase=EPWMatchPhase::RoundOver;
            Match->Result = TEXT("Draw! Press R to restart");
            for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
                if (It->Get() && It->Get()->GetPawn() && It->Get()->PlayerState)
                {
                    if(auto* Winner=It->Get()->GetPlayerState<APillowWarsPlayerState>())
                    {
                        ++Winner->RoundWins;
                        Winner->ForceNetUpdate();
                        Match->Result=Winner->GetPlayerName()+(Winner->RoundWins>=3
                            ? FString(TEXT(" is the Pillow Champion! Host: R to start a new set"))
                            : FString::Printf(TEXT(" wins! Round score %d/3 | Host: R or everyone votes"),Winner->RoundWins));
                    }
                }
            int32 Players=0; for(FConstPlayerControllerIterator Count=GetWorld()->GetPlayerControllerIterator();Count;++Count)if(Count->Get()&&Count->Get()->PlayerState&&!Count->Get()->PlayerState->IsABot())++Players;
            Match->PlayersNeededToRematch=Players;
            Match->RematchVotes=0;
            Match->ForceNetUpdate();
        }
        UE_LOG(LogTemp, Log, TEXT("Pillow Wars winner decided on server"));
    }
}

void APillowWarsGameMode::BeginPlay()
{
    Super::BeginPlay();
    // Arena geometry and lighting are saved in PillowWarsTest for every client.
    if (HasAuthority())
    {
        if(auto* Match=GetGameState<APillowWarsMatchState>())
        {
            Match->RoundNumber=1;
            Match->Result.Empty();
            Match->RematchVotes=0;
            Match->PlayersNeededToRematch=0;
            Match->MatchPhase=EPWMatchPhase::Lobby;
            Match->MatchEndServerTime=0;
            Match->LobbyStatus=TEXT("Choose Play for a lobby or Practice to train solo.");
            Match->ForceNetUpdate();
        }
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        TArray<AActor*> Training; UGameplayStatics::GetAllActorsWithTag(GetWorld(),TEXT("PW_TrainingSpawn"),Training);
        const FVector TrainingLocation=Training.Num()?Training[0]->GetActorLocation():FVector(350,100,330);
        GetWorld()->SpawnActor<APillowWarsPracticeTarget>(TrainingLocation, FRotator(0,180,0), Params);
    }
}

void APillowWarsGameMode::SpawnArenaBox(const FName& Label, const FVector& Location, const FVector& Scale, const FLinearColor& Color)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cube) return;
    FActorSpawnParameters Params; Params.Name = Label;
    AStaticMeshActor* Actor = GetWorld()->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, Params);
    if (!Actor) return;
#if WITH_EDITOR
    Actor->SetActorLabel(Label.ToString());
#endif
    Actor->GetStaticMeshComponent()->SetStaticMesh(Cube);
    Actor->SetActorScale3D(Scale);
    UMaterialInstanceDynamic* Material = Actor->GetStaticMeshComponent()->CreateAndSetMaterialInstanceDynamic(0);
    if (Material)
    {
        Material->SetVectorParameterValue(TEXT("Color"), Color);
        Material->SetVectorParameterValue(TEXT("BaseColor"), Color);
    }
}

void APillowWarsGameMode::BuildArena()
{
    SpawnArenaBox(TEXT("PW_ArenaFloor"), FVector(0, 0, -100), FVector(18, 14, 1), FLinearColor(0.12f, 0.16f, 0.23f));
    SpawnArenaBox(TEXT("PW_BedFrame"), FVector(0, 0, 80), FVector(10, 7, 1.2f), FLinearColor(0.35f, 0.12f, 0.10f));
    SpawnArenaBox(TEXT("PW_Mattress"), FVector(0, 0, 190), FVector(9.5f, 6.5f, 0.7f), FLinearColor(0.86f, 0.86f, 0.78f));
    SpawnArenaBox(TEXT("PW_Headboard"), FVector(-950, 0, 500), FVector(0.6f, 7, 3.8f), FLinearColor(0.24f, 0.10f, 0.06f));
    SpawnArenaBox(TEXT("PW_LeftPlatform"), FVector(1050, -1050, 300), FVector(3.5f, 2.5f, 0.45f), FLinearColor(0.18f, 0.32f, 0.58f));
    SpawnArenaBox(TEXT("PW_RightPlatform"), FVector(1050, 1050, 300), FVector(3.5f, 2.5f, 0.45f), FLinearColor(0.58f, 0.18f, 0.24f));
    SpawnArenaBox(TEXT("PW_PillowBlue"), FVector(-250, -300, 320), FVector(2.2f, 1.4f, 0.35f), FLinearColor(0.10f, 0.30f, 0.90f));
    SpawnArenaBox(TEXT("PW_PillowGold"), FVector(250, 300, 320), FVector(2.2f, 1.4f, 0.35f), FLinearColor(0.90f, 0.62f, 0.12f));
}

float APillowWarsGameMode::SwingStuffingCost(float UppercutPower)
{
    return UppercutPower<0.f?8.f:8.f+24.f*FMath::Clamp(UppercutPower,0.f,1.f);
}

float APillowWarsGameMode::CriticalChance(float ResonancePercent)
{
    return .015f+.535f*FMath::Clamp(ResonancePercent/100.f,0.f,1.f);
}

float APillowWarsGameMode::HealthDamage(float UppercutPower,float StuffingPower,bool bCritical,bool bGuarded)
{
    const float Base=UppercutPower<0.f?15.f:10.f+25.f*FMath::Clamp(UppercutPower,0.f,1.f);
    return Base*FMath::Clamp(StuffingPower,.55f,1.f)*(bCritical?1.75f:1.f)*(bGuarded?.36f:1.f);
}

void APillowWarsGameMode::ServerSwing(APlayerController* Attacker)
{
    SwingWithCharge(Attacker,-1.f,0.f);
}

void APillowWarsGameMode::SwingWithCharge(APlayerController* Attacker,float UppercutPower,float Held)
{
    SettleResources(Attacker,GetWorld()->GetTimeSeconds());
    if (!HasAuthority() || !Attacker || !Attacker->GetPawn() || bMatchOver) return;
    if(const auto* Match=GetGameState<APillowWarsMatchState>();Match&&Match->MatchPhase!=EPWMatchPhase::Playing)return;
    auto* State=Attacker->GetPlayerState<APillowWarsPlayerState>();
    if(!State||State->bEliminated||State->bGuarding)return;
    const float Now = GetWorld()->GetTimeSeconds();
    if (const float* Last = LastSwingTime.Find(Attacker); Last && Now - *Last < SwingCooldown)
    {if(auto* C=Cast<APillowWarsPlayerController>(Attacker))C->SendActionHint(ResourceGeneration,TEXT("cooldown"),TEXT("Attack cooling down. Wait, then try again."));return;}
    ACharacter* AttackerPawn = Cast<ACharacter>(Attacker->GetPawn());
    if (!AttackerPawn) return;
    if (APillowWarsWeapon* Weapon = Weapons.FindRef(AttackerPawn))
    {
        if(Weapon->IsThrowing(Now))return;
        const float Cost=SwingStuffingCost(UppercutPower);
        const int64 CostUnits=PWResourceMath::Quantize(Cost);
        if(GetHeld(State)<CostUnits)
        {
            if(auto* C=Cast<APillowWarsPlayerController>(Attacker))C->SendActionHint(ResourceGeneration,TEXT("stuffing"),TEXT("Not enough stuffing for this attack. Recover stuffing from a pile."));
            if(PracticeSteps.Contains(Attacker)&&PracticeSteps.FindRef(Attacker)==1)PracticeProgress(Attacker,2);
            return;
        }
        const float Strength=FMath::Max(.55f,1.f-(100.f-State->Stuffing)*.0045f);
        LastSwingTime.Add(Attacker, Now);
        if(!Debit(Attacker,CostUnits,TEXT("attack_payment")))return;
        State->ForceNetUpdate(); LastResourceNetUpdate.Add(Attacker,Now); LastCombatAction.Add(Attacker,Now);
        Weapon->StartSwing(Now,Strength);
        if(UppercutPower>=0.f){Weapon->AttackVariant=3;Weapon->ChargePower=FMath::Clamp(UppercutPower,0.f,1.f);Weapon->ReleasedChargeDuration=Held;Weapon->ForceNetUpdate();}
        RecordResourceAction(Attacker,Now);
        if(UppercutPower>=0.f)
        {PlaceLoose(AttackerPawn->GetActorLocation()+AttackerPawn->GetActorRightVector()*80.f,PWResourceMath::Quantize(8.f*FMath::Clamp(UppercutPower,0.f,1.f)),false);ResourceAudit(TEXT("paid_spill"));}
        if(auto* C=Cast<APillowWarsPlayerController>(Attacker))C->SendActionHint(ResourceGeneration,TEXT("accepted"),TEXT(""));
        AnnounceMoment(FString::Printf(TEXT("%s spent %.0f stuffing on a swing"),*State->GetPlayerName(),Cost));
    }
}

void APillowWarsGameMode::SetGuard(APlayerController* Player,bool bPressed)
{
    if(!HasAuthority()||!IsValid(Player)||Player->GetWorld()!=GetWorld())return;
    SettleResources(Player,GetWorld()->GetTimeSeconds());
    auto* State=Player->GetPlayerState<APillowWarsPlayerState>();
    const auto* Match=GetGameState<APillowWarsMatchState>();
    if(!State)return;
    if(!bPressed){State->bGuarding=false;State->ForceNetUpdate();return;}
    if(State->bGuarding)return;
    if(!Match||Match->MatchPhase!=EPWMatchPhase::Playing||bMatchOver||State->bEliminated||State->Stuffing<10.f||!Player->GetPawn())return;
    APillowWarsWeapon* Weapon=Weapons.FindRef(Cast<ACharacter>(Player->GetPawn()));
    const float Now=GetWorld()->GetTimeSeconds();
    if(Weapon&&(Now-Weapon->SwingTime<SwingCooldown||Weapon->ChargeStartTime>=0||Weapon->IsThrowing(Now)))return;
    State->bGuarding=true;RecordResourceAction(Player,Now);State->ForceNetUpdate();
}

void APillowWarsGameMode::PlaceCover(APlayerController* Player)
{
    if(!HasAuthority()||!IsValid(Player)||Player->GetWorld()!=GetWorld())return;
    SettleResources(Player,GetWorld()->GetTimeSeconds());
    auto* State=Player->GetPlayerState<APillowWarsPlayerState>();
    ACharacter* Pawn=Cast<ACharacter>(Player->GetPawn());
    const auto* Match=GetGameState<APillowWarsMatchState>();
    if(!State||!Pawn||!Match||Match->MatchPhase!=EPWMatchPhase::Playing||bMatchOver||State->bEliminated||State->bGuarding||State->Stuffing<25.f)return;
    const float Now=GetWorld()->GetTimeSeconds();
    if(Now-LastCoverTime.FindRef(Player)<1.f)return;
    int32 Owned=0;
    FVector Location=Pawn->GetActorLocation()+Pawn->GetActorForwardVector()*112.f;
    for(TActorIterator<APillowWarsCover> It(GetWorld());It;++It)
    {
        if(It->GetOwner()==Pawn)++Owned;
        if(FVector::DistSquared2D(It->GetActorLocation(),Location)<FMath::Square(105.f))return;
    }
    if(Owned>=2)return;
    FCollisionQueryParams Query;Query.AddIgnoredActor(Pawn);
    FHitResult Floor;
    if(!GetWorld()->LineTraceSingleByChannel(Floor,Location+FVector(0,0,90),Location-FVector(0,0,190),ECC_Visibility,Query))return;
    const float FeetZ=Pawn->GetActorLocation().Z-Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    if(FMath::Abs(Floor.ImpactPoint.Z-FeetZ)>55.f||Floor.ImpactNormal.Z<.7f)return;
    Location.Z=Floor.ImpactPoint.Z+44.f;
    FActorSpawnParameters Params;Params.Owner=Pawn;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
    const uint32 Generation=ResourceGeneration;
    FTransform Transform(Pawn->GetActorRotation(),Location);
    auto* Cover=GetWorld()->SpawnActorDeferred<APillowWarsCover>(CoverClass,Transform,Pawn,nullptr,ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding);
    if(!Cover)return;
    Cover->FinishSpawning(Transform);
    if(!bResourceOpen||Generation!=ResourceGeneration||!IsValid(Cover)||Cover->IsActorBeingDestroyed()||!Debit(Player,25000,TEXT("cover_payment"))){if(IsValid(Cover))Cover->Destroy();return;}
    FPWResourceObject Record;Record.Generation=Generation;Record.Serial=++ResourceSerial;Record.Units=25000;Record.Owner=State;
    CoverBook.Add(Cover,Record);ResourceLoss-=25000;ResourceAudit(TEXT("cover_commit"));
    RecordResourceAction(Player,Now);++State->RoundCoversPlaced;State->ForceNetUpdate();
    LastCoverTime.Add(Player,Now);LastCombatAction.Add(Player,Now);LastResourceNetUpdate.Add(Player,Now);
    AnnounceMoment(State->GetPlayerName()+TEXT(" placed pillow cover"));
}

bool APillowWarsGameMode::CollectStuffing(APlayerController* Player)
{
    // Legacy minting path is disabled. Recovery must transfer from a registered pile.
    return false;
}

void APillowWarsGameMode::AnnounceMoment(const FString& Moment)
{
    if(auto* Match=GetGameState<APillowWarsMatchState>())
    {Match->LastMoment=Moment.Left(64);Match->LastMomentServerTime=GetWorld()->GetTimeSeconds();Match->ForceNetUpdate();}
}

void APillowWarsGameMode::ClearCovers(const AActor* OwnerActor)
{
    TArray<APillowWarsCover*> ToDestroy;
    for(TActorIterator<APillowWarsCover> It(GetWorld());It;++It)if(!OwnerActor||It->GetOwner()==OwnerActor)ToDestroy.Add(*It);
    for(APillowWarsCover* Cover:ToDestroy)if(IsValid(Cover))Cover->Destroy();
}

void APillowWarsGameMode::ClearStuffingPickups()
{
    TArray<APillowWarsStuffingPickup*> ToDestroy;
    for(TActorIterator<APillowWarsStuffingPickup> It(GetWorld());It;++It)ToDestroy.Add(*It);
    for(APillowWarsStuffingPickup* Pickup:ToDestroy)if(IsValid(Pickup))Pickup->Destroy();
}

bool APillowWarsGameMode::SpawnAmbientStuffingPile()
{
    TArray<AActor*> Starts;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(),APlayerStart::StaticClass(),Starts);
    float MinX=0.f,MaxX=0.f,MinY=0.f,MaxY=0.f,MinZ=350.f,MaxZ=350.f;
    if(Starts.Num()>0)
    {
        const FVector First=Starts[0]->GetActorLocation();
        MinX=MaxX=First.X;MinY=MaxY=First.Y;MinZ=MaxZ=First.Z;
        for(const AActor* Start:Starts)
        {
            const FVector P=Start->GetActorLocation();
            MinX=FMath::Min(MinX,P.X);MaxX=FMath::Max(MaxX,P.X);
            MinY=FMath::Min(MinY,P.Y);MaxY=FMath::Max(MaxY,P.Y);
            MinZ=FMath::Min(MinZ,P.Z);MaxZ=FMath::Max(MaxZ,P.Z);
        }
    }
    if(MaxX-MinX<500.f){MinX-=1000.f;MaxX+=1000.f;}
    if(MaxY-MinY<500.f){MinY-=1000.f;MaxY+=1000.f;}
    const float MarginX=FMath::Min(150.f,(MaxX-MinX)*.15f);
    const float MarginY=FMath::Min(150.f,(MaxY-MinY)*.15f);
    for(int32 Try=0;Try<24;++Try)
    {
        FVector Location(FMath::FRandRange(MinX+MarginX,MaxX-MarginX),FMath::FRandRange(MinY+MarginY,MaxY-MarginY),0.f);
        bool bTooClose=false;
        for(const AActor* Start:Starts)if(FVector::DistSquared2D(Location,Start->GetActorLocation())<FMath::Square(300.f)){bTooClose=true;break;}
        if(bTooClose)continue;
        for(TActorIterator<APillowWarsStuffingPickup> It(GetWorld());It;++It)
            if(FVector::DistSquared2D(Location,It->GetActorLocation())<FMath::Square(360.f)){bTooClose=true;break;}
        if(bTooClose)continue;
        FCollisionQueryParams Query(SCENE_QUERY_STAT(AmbientStuffingPile),true);
        FHitResult Floor;
        const FVector TraceStart(Location.X,Location.Y,MaxZ+1500.f);
        const FVector TraceEnd(Location.X,Location.Y,MinZ-1200.f);
        if(!GetWorld()->LineTraceSingleByChannel(Floor,TraceStart,TraceEnd,ECC_Visibility,Query)||Floor.ImpactNormal.Z<.72f)continue;
        if(Floor.ImpactPoint.Z<MinZ-300.f||Floor.ImpactPoint.Z>MaxZ+1000.f)continue;
        Location=Floor.ImpactPoint;
        if(PlaceLoose(Location,48000,true)>0){ResourceAudit(TEXT("ambient_supply"));return true;}
    }
    return false;
}

void APillowWarsGameMode::SpawnStuffingPickup(ACharacter* Victim,const FVector& AwayFromAttacker)
{
    // Contacts never mint free 48-unit piles. Charged releases fund spill instead.
}

bool APillowWarsGameMode::ApplyPillowContact(APlayerController* Attacker,ACharacter* Victim,const FVector& ImpactDirection,float UppercutPower,bool bProjectile)
{
        if (!HasAuthority() || bMatchOver || !Attacker || !Attacker->GetPawn() || !IsValid(Victim) || Victim==Attacker->GetPawn()) return false;
        const auto* Match=GetGameState<APillowWarsMatchState>();
        const auto* AuthorityState=Attacker->GetPlayerState<APillowWarsPlayerState>();
        if(!Match||Match->MatchPhase!=EPWMatchPhase::Playing||!AuthorityState||AuthorityState->bEliminated||AuthorityState->Health<=0.f)return false;
        const FVector ToVictim=Victim->GetActorLocation()-Attacker->GetPawn()->GetActorLocation();
        APillowWarsWeapon* AttackWeapon=Weapons.FindRef(Cast<ACharacter>(Attacker->GetPawn()));
        const float Resonance=!bProjectile&&AttackWeapon?AttackWeapon->ResonanceAttackPower:0.f;
        bool bBlocked=false;
        auto* ReplicatedState=Victim->GetPlayerState<APillowWarsPlayerState>();
        if(ReplicatedState&&(ReplicatedState->bEliminated||ReplicatedState->Health<=0.f))return false;
        if(!ReplicatedState&&!Victim->IsA<APillowWarsPracticeTarget>())return false;
        if(!bResourceOpen)return false;
        if(auto* Controller=Cast<APlayerController>(Victim->GetController()))SettleResources(Controller,GetWorld()->GetTimeSeconds());
        if(!bProjectile&&AttackWeapon)
        {
            const FString Receipt=FString::Printf(TEXT("%u:%u:%d:%u"),ResourceGeneration,Attacker->GetUniqueID(),AttackWeapon->AttackSequence,Victim->GetUniqueID());
            if(ContactReceipts.Contains(Receipt))return false;ContactReceipts.Add(Receipt);
        }
        // Only accepted server contacts roll; wind-up and misses cannot be critical.
        // Throws use baseline odds, never stale power from a previous melee swing.
        const bool bCritical=FMath::FRand()<(AttackWeapon&&!bProjectile?AttackWeapon->AttackCriticalChance:.015f);
        if(AttackWeapon&&!bProjectile){AttackWeapon->bLastContactCritical=bCritical;AttackWeapon->ForceNetUpdate();}
        if(ReplicatedState)
        {
            const FVector ToAttacker=-ToVictim.GetSafeNormal2D();
            bBlocked=ReplicatedState->bGuarding&&ReplicatedState->Stuffing>=10.f&&FVector::DotProduct(Victim->GetActorForwardVector(),ToAttacker)>.25f;
            if(bBlocked)
            {
                Debit(Cast<APlayerController>(Victim->GetController()),10000,TEXT("block_payment"));
                ++ReplicatedState->RoundGuards;
                ReplicatedState->GuardPulseServerTime=GetWorld()->GetTimeSeconds();
                if(ReplicatedState->Stuffing<10.f)ReplicatedState->bGuarding=false;
            }
            if(auto* VictimController=Cast<APlayerController>(Victim->GetController()))RecordResourceAction(VictimController,GetWorld()->GetTimeSeconds());
        }
        const float SwingPower=!bProjectile&&AttackWeapon?AttackWeapon->SwingPower:1.f;
        FPillowFighterState& State = FighterStates.FindOrAdd(Victim);
        const float Power=FMath::Clamp(UppercutPower,0.f,1.f);
        const float UppercutScale=UppercutPower>=0.f?.35f+1.65f*Power:1.f;
        const float DazeScale=UppercutPower>=0.f?.50f+1.50f*Power:1.f;
        const float DazeGain=(25.f+10.f*Resonance)*SwingPower*DazeScale*(bBlocked?.36f:1.f);
        State.Daze = FMath::Clamp(State.Daze + DazeGain, 0.0f, 100.0f);
        if (ReplicatedState)
        {
            ReplicatedState->Daze = State.Daze;
            ReplicatedState->LastHitDamage=HealthDamage(UppercutPower,SwingPower,bCritical,bBlocked);
            ReplicatedState->LastHitServerTime=GetWorld()->GetTimeSeconds();
            ReplicatedState->bLastHitCritical=bCritical;
            ReplicatedState->Health=FMath::Max(0.f,ReplicatedState->Health-ReplicatedState->LastHitDamage);
            if(ReplicatedState->Health<=0.f)
            {
                RetireOwner(Cast<APlayerController>(Victim->GetController()));
                // Removed safely by the GameMode tick, not while iterating weapon sweeps.
                ReplicatedState->bEliminated=true;
                ReplicatedState->bGuarding=false;ReplicatedState->bStuffingResting=false;
                Victim->GetCharacterMovement()->DisableMovement();
            }
            ReplicatedState->ForceNetUpdate();
        }
        const float Knockback = (220.0f + State.Daze * 2.0f)*UppercutScale*(1.f+.65f*Resonance)*SwingPower*(bBlocked?.4f:1.f);
        if (auto* Target = Cast<APillowWarsPracticeTarget>(Victim))
        {
            Target->Daze = State.Daze;
            ++Target->Hits;
            Target->LastKnockback = Knockback;
            Target->ForceNetUpdate();
            Target->ReactToHit(ImpactDirection,Knockback);
            if(!bProjectile)PracticeContact(Attacker);
        }
        auto* Practice=Cast<APillowWarsPracticeTarget>(Victim);
        if(!Practice || !Practice->bAnchored)Victim->LaunchCharacter(ToVictim.GetSafeNormal2D() * Knockback + FVector(0, 0, UppercutPower>=0 ? 100+620*Power : 120), true, true);
        if(ReplicatedState){++ReplicatedState->RoundHitsTaken;ReplicatedState->ForceNetUpdate();}
        if(auto* AttackerState=Attacker->GetPlayerState<APillowWarsPlayerState>())
        {++AttackerState->RoundHits;AttackerState->ForceNetUpdate();}
        const FVector HitLocation=Victim->GetActorLocation()+FVector(0,0,Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*.72f);
        for(int32 FeatherIndex=0;FeatherIndex<7;++FeatherIndex)
        {
            FActorSpawnParameters FeatherParams;
            FeatherParams.Owner=Attacker;
            FeatherParams.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            if(APillowWarsFeather* Feather=GetWorld()->SpawnActor<APillowWarsFeather>(HitLocation,FVector(0,0,1).Rotation(),FeatherParams))
            {
                const float Side=FMath::RandRange(-1.f,1.f);
                const FVector Scatter=FVector::CrossProduct(ImpactDirection.GetSafeNormal(),FVector::UpVector).GetSafeNormal()*Side*260.f;
                const FVector Velocity=ImpactDirection.GetSafeNormal()*FMath::RandRange(120.f,280.f)+Scatter+FVector(0,0,FMath::RandRange(210.f,410.f));
                Feather->Launch(Velocity*(bBlocked?.70f:1.f));
            }
        }
        AnnounceMoment(ReplicatedState&&ReplicatedState->bEliminated?FString::Printf(TEXT("%s is OUT - health depleted"),*ReplicatedState->GetPlayerName()):bCritical?TEXT("CRITICAL PILLOW HIT! 1.75x damage"):bBlocked?TEXT("Pillow met a facing guard"):TEXT("Pillow hit"));
        if (APillowWarsWeapon* Weapon=Weapons.FindRef(Victim))
        {
            Weapon->ReceiveImpact(ImpactDirection,Knockback);
        }
        UE_LOG(LogTemp, Log, TEXT("Pillow hit: Daze=%.0f Knockback=%.0f Damage=%.2f Health=%.2f Critical=%d"), State.Daze, Knockback,ReplicatedState?ReplicatedState->LastHitDamage:0.f,ReplicatedState?ReplicatedState->Health:100.f,bCritical);
        return true;
}

void APillowWarsGameMode::CycleCharacter(APlayerController* Player)
{
    if (!HasAuthority() || !Player) return;
    if (APillowWarsWeapon* Weapon=Weapons.FindRef(Cast<ACharacter>(Player->GetPawn())))
        Weapon->SetVariant(Weapon->VariantIndex+1);
}

void APillowWarsGameMode::ApplyPlayerSelection(APlayerController* Player)
{
    if(!HasAuthority()||!Player)return;
    auto* State=Player->GetPlayerState<APillowWarsPlayerState>();
    ACharacter* Pawn=Cast<ACharacter>(Player->GetPawn());
    if(!State||!Pawn)return;
    const float Scales[]={1.f,1.13f,1.28f};
    Pawn->SetActorScale3D(FVector(Scales[FMath::Clamp(State->BodySizeIndex,0,2)]));
    if(APillowWarsWeapon* Weapon=Weapons.FindRef(Pawn))Weapon->SetVariant(4+FMath::Clamp(State->CharacterIndex,0,3));
}

void APillowWarsGameMode::SetPlayerReady(APlayerController* Player,bool bReady)
{
    if(!HasAuthority()||!Player)return;
    const auto* Match=GetGameState<APillowWarsMatchState>();
    if(!Match||Match->MatchPhase!=EPWMatchPhase::Lobby)return;
    if(auto* State=Player->GetPlayerState<APillowWarsPlayerState>()){State->bReady=bReady;State->ForceNetUpdate();}
    if(auto* MutableMatch=GetGameState<APillowWarsMatchState>()){MutableMatch->LobbyStatus=TEXT("All players ready? The host can press H to begin.");MutableMatch->ForceNetUpdate();}
}

void APillowWarsGameMode::SetPlayerSelection(APlayerController* Player,int32 Character,int32 Body,int32 Color,int32 Pillow)
{
    if(!HasAuthority()||!Player)return;
    auto* Match=GetGameState<APillowWarsMatchState>();
    if(!Match||Match->MatchPhase!=EPWMatchPhase::Lobby)return;
    if(auto* State=Player->GetPlayerState<APillowWarsPlayerState>())
    {
        State->CharacterIndex=FMath::Clamp(Character,0,3);State->BodySizeIndex=FMath::Clamp(Body,0,2);
        State->ColorIndex=FMath::Clamp(Color,0,3);State->PillowIndex=FMath::Clamp(Pillow,0,3);State->bReady=false;State->ForceNetUpdate();
        ApplyPlayerSelection(Player);
    }
}

void APillowWarsGameMode::StartFrontendMatch(APlayerController* Requester,bool bPractice)
{
    if(!HasAuthority()||!Requester)return;
    auto* Match=GetGameState<APillowWarsMatchState>();if(!Match)return;
    if(Match->MatchPhase!=EPWMatchPhase::Lobby)return;
    int32 Players=0,Ready=0;
    for(APlayerState* PS:Match->PlayerArray){++Players;if(const auto* Fighter=Cast<APillowWarsPlayerState>(PS);Fighter&&Fighter->bReady)++Ready;}
    const bool bHost=Requester->IsLocalPlayerController() && Requester->PlayerState && !Requester->PlayerState->IsABot();
    if(!bHost)
    {
        Match->LobbyStatus=TEXT("Only the host can start the match.");
        Match->ForceNetUpdate();return;
    }
    if(bPractice && Players!=1)
    {
        Match->LobbyStatus=TEXT("Practice is available when you are the only player in the lobby.");
        Match->ForceNetUpdate();return;
    }
    if(!bPractice && (Players<2 || Ready<Players))
    {
        Match->MatchPhase=EPWMatchPhase::Lobby;
        Match->LobbyStatus=Players<2?TEXT("Waiting for a second player to join..."):TEXT("Every player must be READY before the host starts.");
        Match->ForceNetUpdate();return;
    }
    const int32 Round=Match->RoundNumber;
    bPracticeSession=bPractice;
    ResetMatch();
    Match->RoundNumber=Round;
    bMatchOver=false;bRoundStarted=false;
    Match->Result.Empty();Match->MatchPhase=EPWMatchPhase::Countdown;Match->CountdownEndServerTime=GetWorld()->GetTimeSeconds()+3.f;
    Match->MatchEndServerTime=0;Match->LobbyStatus=TEXT("Loading arena and synchronizing dreamers...");Match->ForceNetUpdate();
}

void APillowWarsGameMode::ReturnToLobby(APlayerController* Requester)
{
    if(!HasAuthority()||!Requester)return;
    if(!Requester->IsLocalController() && GetNetMode()!=NM_DedicatedServer)return;
    CloseResources(TEXT("return_to_lobby"));ClearCovers();ClearStuffingPickups();
    bMatchOver=false;bRoundStarted=false;bPracticeSession=false;
    if(auto* Match=GetGameState<APillowWarsMatchState>())
    {
        Match->MatchPhase=EPWMatchPhase::Lobby;Match->Result.Empty();Match->MatchEndServerTime=0;Match->LobbyStatus=TEXT("Choose a dreamer and ready up.");
        for(APlayerState* PS:Match->PlayerArray)if(auto* Fighter=Cast<APillowWarsPlayerState>(PS)){Fighter->bReady=Fighter->IsABot();Fighter->ForceNetUpdate();}
        Match->ForceNetUpdate();
    }
}

void APillowWarsGameMode::ResetMatch()
{
    if (!HasAuthority()) return;
    if(const auto* Match=GetGameState<APillowWarsMatchState>();Match&&Match->MatchPhase==EPWMatchPhase::Playing&&!bMatchOver&&!bPracticeSession)return;
    CloseResources(TEXT("reset"));
    bool bNewSet=false;
    if(const auto* Match=GetGameState<APillowWarsMatchState>())
        for(const APlayerState* Player:Match->PlayerArray)
            if(const auto* Fighter=Cast<APillowWarsPlayerState>(Player);Fighter&&Fighter->RoundWins>=3)bNewSet=true;
    for(TActorIterator<APillowWarsProjectile> It(GetWorld());It;++It)It->Destroy();
    for (auto& Pair:Weapons) if(IsValid(Pair.Value)) Pair.Value->ResetAttack();
    bMatchOver = false; FighterStates.Empty(); LastSwingTime.Empty(); RematchVoters.Empty();
    LastCombatAction.Empty();LastCoverTime.Empty();LastResourceNetUpdate.Empty();LastPickupSpawnTime.Empty();
    ClearCovers();ClearStuffingPickups();
    bRoundStarted = false;
    for (TActorIterator<APillowWarsPracticeTarget> It(GetWorld()); It; ++It) It->ResetPractice();
    if (auto* Match = GetGameState<APillowWarsMatchState>())
    {
        Match->Result.Empty();
        Match->LastMoment.Empty();Match->LastMomentServerTime=-100.f;
        ++Match->RoundNumber;
        Match->RematchVotes=0;
        Match->PlayersNeededToRematch=0;
        Match->MatchPhase=EPWMatchPhase::Countdown;
        Match->CountdownEndServerTime=GetWorld()->GetTimeSeconds()+3.f;
        Match->MatchEndServerTime=0;
        Match->ForceNetUpdate();
    }
    TArray<AActor*> Starts; UGameplayStatics::GetAllActorsOfClass(GetWorld(), APlayerStart::StaticClass(), Starts);
    int32 Index = 0;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It, ++Index)
    {
        if (!It->Get()) continue;
        if (auto* State = It->Get()->GetPlayerState<APillowWarsPlayerState>())
        {
            State->Daze = 0;
            State->Health=100.f;State->LastHitDamage=0.f;State->bLastHitCritical=false;State->LastHitServerTime=-100.f;
            State->bEliminated = false;
            State->Stuffing=100.f;State->bGuarding=false;State->bStuffingResting=false;State->RoundHits=0;State->RoundHitsTaken=0;State->RoundGuards=0;State->RoundCoversPlaced=0;State->RoundCoversBroken=0;State->RoundStuffingCollected=0;
            State->GuardPulseServerTime=-100.f;
            if(bNewSet)State->RoundWins=0;
            State->ForceNetUpdate();
        }
        if (!It->Get()->GetPawn()) RestartPlayer(It->Get());
        if (ACharacter* Pawn = It->Get() ? Cast<ACharacter>(It->Get()->GetPawn()) : nullptr)
        {
            Pawn->SetActorLocation(Starts.IsValidIndex(Index) ? Starts[Index]->GetActorLocation() : FVector(0, Index * 500, 350));
            Pawn->GetCharacterMovement()->StopMovementImmediately();
            Pawn->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        }
        if(auto* Bot=Cast<APillowWarsBotController>(It->Get()))Bot->ResetBot();
    }
}

void APillowWarsGameMode::RequestReset(APlayerController* Requester)
{
    if(!HasAuthority()||!Requester)return;
    if(const auto* Match=GetGameState<APillowWarsMatchState>();Match&&Match->MatchPhase==EPWMatchPhase::Lobby)
    {
        StartFrontendMatch(Requester,false);
        return;
    }
    // No in-match respawn, including the host. Solo practice can reset its dummy.
    if(!bMatchOver&&!bPracticeSession)return;
    if(Requester->IsLocalController())
    {
        ResetMatch();
        return;
    }
    if(!bMatchOver)return;
    RematchVoters.Add(Requester);
    int32 Players=0;
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)if(It->Get()&&It->Get()->PlayerState&&!It->Get()->PlayerState->IsABot())++Players;
    if(auto* Match=GetGameState<APillowWarsMatchState>())
    {
        Match->RematchVotes=RematchVoters.Num();
        Match->PlayersNeededToRematch=Players;
        Match->ForceNetUpdate();
    }
    if(Players>0&&RematchVoters.Num()==Players)ResetMatch();
}

bool APillowWarsGameMode::EditBots(APlayerController* Requester, int32 Change)
{
    auto* Match = GetGameState<APillowWarsMatchState>();
    if (!HasAuthority() || !Requester || !Requester->IsLocalPlayerController() || !Requester->PlayerState ||
        Requester->PlayerState->IsABot() || !Match || Match->MatchPhase != EPWMatchPhase::Lobby || (Change != 1 && Change != -1)) return false;
    if (Change < 0)
    {
        APillowWarsBotController* Bot = nullptr;
        for (TActorIterator<APillowWarsBotController> It(GetWorld()); It; ++It) Bot = *It;
        if (!Bot) return false;
        ACharacter* Pawn = Cast<ACharacter>(Bot->GetPawn());
        if (APillowWarsWeapon* Weapon = Weapons.FindRef(Pawn)) Weapon->Destroy();
        Weapons.Remove(Pawn); FighterStates.Remove(Pawn);
        Logout(Bot);
        if (Pawn) Pawn->Destroy();
        Bot->Destroy();
        Match->LobbyStatus = TEXT("Removed a bot. F6 adds; F8 starts when every human is ready.");
        Match->ForceNetUpdate();
        return true;
    }
    if (Match->PlayerArray.Num() >= 10) { Match->LobbyStatus=TEXT("Arena is full (10 fighters)."); Match->ForceNetUpdate(); return false; }
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    APillowWarsBotController* Bot = GetWorld()->SpawnActor<APillowWarsBotController>(Params);
    if (!Bot || !Bot->PlayerState) { if(Bot)Bot->Destroy(); return false; }
    auto* State = Bot->GetPlayerState<APillowWarsPlayerState>();
    int32 BotNumber = 0, PlayerId = 1000;
    for (APlayerState* Player : Match->PlayerArray)
    { if(Player->IsABot())++BotNumber; PlayerId=FMath::Max(PlayerId,Player->GetPlayerId()+1); }
    State->SetIsABot(true); State->SetPlayerId(PlayerId);
    State->SetPlayerName(FString::Printf(TEXT("BOT %d"),BotNumber));
    State->bReady=true; State->CharacterIndex=BotNumber%4;
    TArray<AActor*> Starts; UGameplayStatics::GetAllActorsOfClass(GetWorld(),APlayerStart::StaticClass(),Starts);
    AActor* SafeStart=nullptr;
    for(AActor* Start:Starts)
    {
        bool bOccupied=false;
        for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
            if(It->Get()&&It->Get()->GetPawn()&&FVector::DistSquared(It->Get()->GetPawn()->GetActorLocation(),Start->GetActorLocation())<FMath::Square(240.f)) { bOccupied=true; break; }
        if(!bOccupied){SafeStart=Start;break;}
    }
    if(!SafeStart){Bot->Destroy();Match->LobbyStatus=TEXT("No separated bot spawn available.");Match->ForceNetUpdate();return false;}
    RestartPlayerAtPlayerStart(Bot,SafeStart);
    if(!Bot->GetPawn()){Bot->Destroy();return false;}
    Bot->ResetBot(); State->ForceNetUpdate();
    Match->LobbyStatus=TEXT("Bot added. Enter: ready. F6/F7: add/remove bot. F8: start offline or LAN match.");
    Match->ForceNetUpdate();
    UE_LOG(LogTemp,Log,TEXT("PW_BOT_ADDED %s pawn=%s"),*State->GetPlayerName(),*GetNameSafe(Bot->GetPawn()));
    return true;
}
