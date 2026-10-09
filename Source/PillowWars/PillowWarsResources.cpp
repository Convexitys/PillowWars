#include "PillowWarsGameMode.h"
#include "PillowWarsMatchState.h"
#include "PillowWarsPlayerController.h"
#include "PillowWarsWeapon.h"
#include "PillowWarsStuffingPickup.h"
#include "PillowWarsCover.h"
#include "PillowWarsPracticeTarget.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

void APillowWarsGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
    CloseResources(TEXT("world_end"));
    Super::EndPlay(Reason);
}

void APillowWarsGameMode::ProjectHeld(APillowWarsPlayerState* State)
{
    if(State){State->Stuffing=float(double(HeldUnits.FindRef(State))/1000.0);State->ForceNetUpdate();}
}
int64 APillowWarsGameMode::GetHeld(APillowWarsPlayerState* State)
{
    if(!bResourceOpen||!State)return 0;
    if(!HeldUnits.Contains(State))return 0;
#if WITH_EDITOR
    // Existing native regression suites use server-side property fixtures. Record
    // their adjustments explicitly; no shipping/client path can rebase a balance.
    if(GIsEditor)
    {
        const int64 View=FMath::Min<int64>(100000,PWResourceMath::Quantize(State->Stuffing));
        int64& Held=HeldUnits.FindChecked(State);
        if(View!=Held){if(View>Held)ResourceSupply+=View-Held;else ResourceLoss+=Held-View;Held=View;ResourceAudit(TEXT("editor_fixture"));}
    }
#endif
    return HeldUnits.FindRef(State);
}
void APillowWarsGameMode::OpenResources()
{
    if(bResourceOpen)return;
    ++ResourceGeneration;ResourceSerial=0;ResourceSequence=0;
    ResourceInitial=ResourceSupply=ResourceLoss=0;
    HeldUnits.Empty();PileBook.Empty();CoverBook.Empty();ResourceClocks.Empty();ContactReceipts.Empty();PracticeSteps.Empty();
    bResourceOpen=true;
    const double Now=GetWorld()->GetTimeSeconds();
    for(FConstPlayerControllerIterator It=GetWorld()->GetPlayerControllerIterator();It;++It)
        if(auto* Player=It->Get())if(auto* State=Player->GetPlayerState<APillowWarsPlayerState>())
        {
            if(!State->bEliminated){const int64 Held=FMath::Min<int64>(100000,PWResourceMath::Quantize(State->Stuffing));HeldUnits.Add(State,Held);ResourceInitial+=Held;}
            FPWResourceClock Clock;Clock.LastTime=Now;ResourceClocks.Add(Player,Clock);
            if(auto* Controller=Cast<APillowWarsPlayerController>(Player))Controller->ClientResourceSession(ResourceGeneration);
        }
    ResourceAudit(TEXT("open"));
}
void APillowWarsGameMode::CloseResources(FName Reason)
{
    if(!bResourceOpen)return;
    bResourceOpen=false; // Fence admission before any destruction callback.
    for(const auto& Pair:HeldUnits)ResourceLoss+=Pair.Value;
    for(const auto& Pair:PileBook)ResourceLoss+=Pair.Value.Units;
    for(const auto& Pair:CoverBook)ResourceLoss+=Pair.Value.Units;
    HeldUnits.Empty();PileBook.Empty();CoverBook.Empty();ResourceClocks.Empty();ContactReceipts.Empty();PracticeSteps.Empty();
    ResourceAudit(Reason);
}
FString APillowWarsGameMode::GetResourceLedgerSnapshot() const
{
    int64 Held=0,Loose=0,Cover=0;int32 Ambient=0,Funded=0;
    for(const auto& Pair:HeldUnits)Held+=Pair.Value;
    for(const auto& Pair:PileBook){Loose+=Pair.Value.Units;Pair.Value.bAmbient?++Ambient:++Funded;}
    for(const auto& Pair:CoverBook)Cover+=Pair.Value.Units;
    return FString::Printf(TEXT("{\"generation\":%u,\"sequence\":%llu,\"open\":%s,\"held\":%lld,\"loose\":%lld,\"cover\":%lld,\"loss\":%lld,\"initial\":%lld,\"supply\":%lld,\"residual\":%lld,\"ambient\":%d,\"funded\":%d}"),ResourceGeneration,ResourceSequence,bResourceOpen?TEXT("true"):TEXT("false"),Held,Loose,Cover,ResourceLoss,ResourceInitial,ResourceSupply,Held+Loose+Cover+ResourceLoss-ResourceInitial-ResourceSupply,Ambient,Funded);
}
void APillowWarsGameMode::ResourceAudit(FName Reason)
{
    ++ResourceSequence;
    UE_LOG(LogTemp,Log,TEXT("PW_LEDGER reason=%s time=%.6f %s"),*Reason.ToString(),GetWorld()->GetTimeSeconds(),*GetResourceLedgerSnapshot());
}
bool APillowWarsGameMode::Debit(APlayerController* Player,int64 Amount,FName Reason)
{
    auto* State=Player?Player->GetPlayerState<APillowWarsPlayerState>():nullptr;
    if(!bResourceOpen||!State||State->bEliminated||Amount<0||GetHeld(State)<Amount)return false;
    HeldUnits.FindChecked(State)-=Amount;ResourceLoss+=Amount;ProjectHeld(State);ResourceAudit(Reason);return true;
}
void APillowWarsGameMode::RetireOwner(APlayerController* Player)
{
    if(!bResourceOpen||!Player)return;
    auto* State=Player->GetPlayerState<APillowWarsPlayerState>();
    if(int64* Held=HeldUnits.Find(State)){ResourceLoss+=*Held;HeldUnits.Remove(State);State->Stuffing=0;State->ForceNetUpdate();}
    TArray<TWeakObjectPtr<APillowWarsCover>> Owned;
    for(const auto& Pair:CoverBook)if(Pair.Value.Owner==State)Owned.Add(Pair.Key);
    for(const auto& Cover:Owned)if(Cover.IsValid())ResolveCover(Cover.Get(),TEXT("owner_cleanup"));
    ResourceClocks.Remove(Player);PracticeSteps.Remove(Player);ResourceAudit(TEXT("owner_cleanup"));
}
APillowWarsStuffingPickup* APillowWarsGameMode::FindRecoveryPile(APlayerController* Player) const
{
    auto* Pawn=Player?Cast<ACharacter>(Player->GetPawn()):nullptr;
    if(!Pawn||!Pawn->GetCharacterMovement()->IsMovingOnGround()||Pawn->GetVelocity().Size2D()>=15.f)return nullptr;
    const auto* State=Player->GetPlayerState<APillowWarsPlayerState>();
    if(!State||State->bEliminated)return nullptr;
    const double Now=GetWorld()->GetTimeSeconds();
    const APillowWarsWeapon* Weapon=Weapons.FindRef(Pawn);
    if(Weapon&&(Weapon->ChargeStartTime>=0||Weapon->IsThrowing(Now)||Now-Weapon->SwingTime<.65f))return nullptr;
    APillowWarsStuffingPickup* Best=nullptr;uint64 Serial=MAX_uint64;
    for(const auto& Pair:PileBook)
    {
        auto* Pile=Pair.Key.Get();if(!Pile||!Pile->Registered||Pile->IsActorBeingDestroyed()||Pair.Value.Units<=0)continue;
        if(FVector::Dist2D(Pawn->GetActorLocation(),Pile->GetActorLocation())>115.f||FMath::Abs(Pawn->GetActorLocation().Z-Pile->GetActorLocation().Z)>160.f)continue;
        FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(Pawn);Query.AddIgnoredActor(Pile);
        if(GetWorld()->LineTraceSingleByChannel(Hit,Pawn->GetActorLocation(),Pile->GetActorLocation()+FVector(0,0,15),ECC_Visibility,Query))continue;
        if(Pair.Value.Serial<Serial){Best=Pile;Serial=Pair.Value.Serial;}
    }
    return Best;
}
void APillowWarsGameMode::SettleResources(APlayerController* Player,double Now)
{
    if(!bResourceOpen||!Player)return;
    auto* State=Player->GetPlayerState<APillowWarsPlayerState>();
    auto* Pawn=Cast<ACharacter>(Player->GetPawn());
    if(!State||!HeldUnits.Contains(State))return;
    int64 Held=GetHeld(State);FPWResourceClock& Clock=ResourceClocks.FindOrAdd(Player);
    if(Now<Clock.LastTime)return;
    const double Previous=Clock.LastTime,Dt=Now-Previous;
    const bool GuardWasActive=State->bGuarding;
    if(GuardWasActive && Dt>0)
    {
        const double OldCarry=Clock.GuardCarry.Remainder;
        const int64 Requested=Clock.GuardCarry.Integrate(Dt,8000.0);
        const int64 Drained=FMath::Min(Held,Requested);
        HeldUnits.FindChecked(State)-=Drained;ResourceLoss+=Drained;
        if(Drained>=Held)
        {
            const double Exhausted=Previous+FMath::Max(0.0,(double(Held)-OldCarry)/8000.0);
            State->bGuarding=false;Clock.GuardCarry.Reset();LastCombatAction.Add(Player,float(FMath::Min(Now,Exhausted)));
        }
        else LastCombatAction.Add(Player,float(Now));
        ProjectHeld(State);ResourceAudit(TEXT("guard_drain"));Held=HeldUnits.FindRef(State);
    }
    APillowWarsStuffingPickup* Pile=FindRecoveryPile(Player);
    const bool Continuous=Pile&&Pile==Clock.ObservedPile.Get()&&Pawn&&FVector::DistSquared(Pawn->GetActorLocation(),Clock.ObservedLocation)<1.f;
    if(Pile!=Clock.ObservedPile.Get())Clock.RefillCarry.Reset();
    const double EligibleStart=FMath::Max(Previous,double(LastCombatAction.FindRef(Player))+1.0);
    const bool CanRefill=!State->bGuarding&&Continuous&&Held<100000&&Now>EligibleStart;
    int64 Taken=0;
    if(CanRefill)
    {
        FPWResourceObject* Record=PileBook.Find(Pile);
        if(Record && Record->Generation==ResourceGeneration)
        {
            Taken=FMath::Min(Clock.RefillCarry.Integrate(Now-EligibleStart,24000.0),FMath::Min(100000-Held,Record->Units));
            HeldUnits.FindChecked(State)+=Taken;Record->Units-=Taken;
            Pile->RemainingStuffing=float(double(Record->Units)/1000.0);
            const bool Empty=Record->Units==0;
            if(Empty){PileBook.Remove(Pile);++State->RoundStuffingCollected;}
            if(Held+Taken>=100000||Empty)Clock.RefillCarry.Reset();
            ProjectHeld(State);Pile->ForceNetUpdate();ResourceAudit(TEXT("pickup_commit"));
            // Accounting is committed before callbacks can observe destruction.
            if(Empty)Pile->Destroy();
            if(Held<8000&&Held+Taken>=8000)
                if(auto* C=Cast<APillowWarsPlayerController>(Player))C->SendActionHint(ResourceGeneration,TEXT("affordable"),TEXT("Basic attack affordable. Charged attacks cost more."));
            if(Taken>0&&PracticeSteps.FindRef(Player)==2&&Held+Taken>=8000)PracticeProgress(Player,3);
        }
    }
    else if(!Continuous||State->bGuarding||Held>=100000)Clock.RefillCarry.Reset();
    State->bStuffingResting=CanRefill&&Taken>0;
    if(Pawn)
    {
        Pawn->GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch=true;
        if(State->bStuffingResting&&!Pawn->bIsCrouched)Pawn->Crouch();
        else if(!State->bStuffingResting&&Pawn->bIsCrouched)Pawn->UnCrouch();
        Clock.ObservedLocation=Pawn->GetActorLocation();
    }
    Clock.ObservedPile=Pile&&IsValid(Pile)&&!Pile->IsActorBeingDestroyed()?Pile:nullptr;Clock.LastTime=Now;
}
void APillowWarsGameMode::RecordResourceAction(APlayerController* Player,double Now)
{
    if(!bResourceOpen||!Player)return;
    LastCombatAction.Add(Player,float(Now));
    FPWResourceClock& Clock=ResourceClocks.FindOrAdd(Player);Clock.ObservedPile.Reset();Clock.RefillCarry.Reset();Clock.LastTime=Now;
}
int64 APillowWarsGameMode::PlaceLoose(const FVector& Origin,int64 Amount,bool bAmbient)
{
    if(!bResourceOpen||Amount<=0)return 0;
    const uint32 Generation=ResourceGeneration;
    FVector Location=Origin;FHitResult Floor;FCollisionQueryParams Query;
    for(TActorIterator<ACharacter> It(GetWorld());It;++It)Query.AddIgnoredActor(*It);
    for(TActorIterator<APillowWarsCover> It(GetWorld());It;++It)Query.AddIgnoredActor(*It);
    if(!GetWorld()->LineTraceSingleByChannel(Floor,Origin+FVector(0,0,80),Origin-FVector(0,0,700),ECC_Visibility,Query)||Floor.ImpactNormal.Z<.7f)return 0;
    Location=Floor.ImpactPoint;
    int32 Pool=0;APillowWarsStuffingPickup* Merge=nullptr;uint64 BestSerial=MAX_uint64;
    for(const auto& Pair:PileBook)
    {
        if(Pair.Value.bAmbient!=bAmbient)continue;++Pool;
        auto* Pile=Pair.Key.Get();
        if(!bAmbient&&IsValid(Pile)&&!Pile->IsActorBeingDestroyed()&&Pair.Value.Units<96000&&FVector::DistSquared(Location,Pile->GetActorLocation())<FMath::Square(150.f))
        {
            FHitResult Hit;FCollisionQueryParams Path;Path.AddIgnoredActor(Pile);
            if(!GetWorld()->LineTraceSingleByChannel(Hit,Location+FVector(0,0,20),Pile->GetActorLocation()+FVector(0,0,20),ECC_Visibility,Path)&&Pair.Value.Serial<BestSerial){Merge=Pile;BestSerial=Pair.Value.Serial;}
        }
    }
    if(Merge)
    {
        auto& Record=PileBook.FindChecked(Merge);const int64 Accepted=FMath::Min(Amount,96000-Record.Units);
        Record.Units+=Accepted;ResourceLoss-=Accepted;Merge->RemainingStuffing=float(double(Record.Units)/1000.0);Merge->ForceNetUpdate();return Accepted;
    }
    if(Pool>=4||PileBook.Num()>=8)return 0;
    FTransform Transform(FRotator::ZeroRotator,Location);
    auto* Pile=GetWorld()->SpawnActorDeferred<APillowWarsStuffingPickup>(APillowWarsStuffingPickup::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if(!Pile)return 0;
    Pile->RemainingStuffing=0;Pile->Registered=false;Pile->AmbientSource=bAmbient;
    Pile->FinishSpawning(Transform);
    if(!bResourceOpen||ResourceGeneration!=Generation||!IsValid(Pile)||Pile->IsActorBeingDestroyed()){if(IsValid(Pile))Pile->Destroy();return 0;}
    const int64 Accepted=FMath::Min<int64>(Amount,96000);
    FPWResourceObject Record;Record.Generation=Generation;Record.Serial=++ResourceSerial;Record.Units=Accepted;Record.bAmbient=bAmbient;
    PileBook.Add(Pile,Record);
    if(bAmbient)ResourceSupply+=Accepted;else ResourceLoss-=Accepted;
    Pile->ResourceGeneration=Generation;Pile->ResourceSerial=Record.Serial;Pile->RemainingStuffing=float(double(Accepted)/1000.0);Pile->Registered=true;Pile->ForceNetUpdate();return Accepted;
}
void APillowWarsGameMode::RetirePile(APillowWarsStuffingPickup* Pile)
{
    if(!bResourceOpen)return;
    if(const auto* Record=PileBook.Find(Pile)){ResourceLoss+=Record->Units;PileBook.Remove(Pile);ResourceAudit(TEXT("pile_destroy"));}
}
void APillowWarsGameMode::ResolveCover(APillowWarsCover* Cover,FName Reason)
{
    if(!bResourceOpen)return;
    const auto* Found=CoverBook.Find(Cover);if(!Found||Found->Generation!=ResourceGeneration)return;
    const FPWResourceObject Record=*Found;CoverBook.Remove(Cover);ResourceLoss+=Record.Units;
    // Claim before staging salvage: nested cleanup cannot claim this record again.
    int64 Salvage=0;
    if(Reason==TEXT("break")||Reason==TEXT("expiry")||Reason==TEXT("reclaim"))Salvage=PlaceLoose(Cover->GetActorLocation(),FMath::Min<int64>(15000,Record.Units),false);
    UE_LOG(LogTemp,Log,TEXT("PW_COVER_RECEIPT gen=%u object=%llu reason=%s salvage=%lld"),Record.Generation,Record.Serial,*Reason.ToString(),Salvage);
    ResourceAudit(Reason);
}
void APillowWarsGameMode::ReclaimCover(APlayerController* Player)
{
    if(!bResourceOpen||!Player||!Player->GetPawn())return;
    SettleResources(Player,GetWorld()->GetTimeSeconds());
    const auto* State=Player->GetPlayerState<APillowWarsPlayerState>();if(!State||State->bEliminated)return;
    APillowWarsCover* Best=nullptr;uint64 Serial=MAX_uint64;
    for(const auto& Pair:CoverBook)
    {
        if(Pair.Value.Owner!=State||!Pair.Key.IsValid()||Pair.Value.Serial>=Serial)continue;
        const FVector From=Player->GetPawn()->GetActorLocation(),To=Pair.Key->GetActorLocation();
        if(FVector::DistSquared2D(From,To)>=FMath::Square(180.f)||FMath::Abs(From.Z-To.Z)>160.f)continue;
        FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(Player->GetPawn());Query.AddIgnoredActor(Pair.Key.Get());
        if(GetWorld()->LineTraceSingleByChannel(Hit,From,To+FVector(0,0,20),ECC_Visibility,Query))continue;
        Best=Pair.Key.Get();Serial=Pair.Value.Serial;
    }
    if(Best){ResolveCover(Best,TEXT("reclaim"));RecordResourceAction(Player,GetWorld()->GetTimeSeconds());Best->Destroy();}
}
void APillowWarsGameMode::TestSetStuffing(APlayerController* Player,float Value)
{
#if WITH_EDITOR
    if(!GIsEditor||!bResourceOpen||!Player)return;
    if(auto* State=Player->GetPlayerState<APillowWarsPlayerState>()){State->Stuffing=FMath::Clamp(Value,0.f,100.f);GetHeld(State);ProjectHeld(State);}
#endif
}
void APillowWarsGameMode::PracticeProgress(APlayerController* Player,int32 Step)
{
    PracticeSteps.Add(Player,Step);
    if(auto* C=Cast<APillowWarsPlayerController>(Player))C->SendPracticeStep(ResourceGeneration,Step);
}
void APillowWarsGameMode::TogglePracticeChallenge(APlayerController* Player)
{
    if(!bResourceOpen||!bPracticeSession||!Player||!Player->GetPawn())return;
    if(PracticeSteps.Contains(Player)&&PracticeSteps.FindRef(Player)!=4){PracticeSteps.Remove(Player);if(auto* C=Cast<APillowWarsPlayerController>(Player))C->SendPracticeStep(ResourceGeneration,-1);return;}
    auto* State=Player->GetPlayerState<APillowWarsPlayerState>();if(!State||State->bEliminated)return;
    const int64 Held=GetHeld(State);ResourceSupply+=100000-Held;HeldUnits.FindChecked(State)=100000;ProjectHeld(State);
    PracticeProgress(Player,0);ResourceAudit(TEXT("practice_explicit_supply"));
}
void APillowWarsGameMode::PracticeContact(APlayerController* Player)
{
    if(!bPracticeSession||!PracticeSteps.Contains(Player))return;
    const int32 Step=PracticeSteps.FindRef(Player);
    if(Step==0)
    {
        auto* State=Player->GetPlayerState<APillowWarsPlayerState>();const int64 Held=GetHeld(State);
        if(Held>4000)Debit(Player,Held-4000,TEXT("practice_depletion_setup"));
        PlaceLoose(Player->GetPawn()->GetActorLocation()+Player->GetPawn()->GetActorRightVector()*190.f,48000,false);
        RecordResourceAction(Player,GetWorld()->GetTimeSeconds());PracticeProgress(Player,1);ResourceAudit(TEXT("practice_setup"));
    }
    else if(Step==3)PracticeProgress(Player,4);
}
