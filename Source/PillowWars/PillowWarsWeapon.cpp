#include "PillowWarsWeapon.h"
#include "PillowWarsGameMode.h"
#include "PillowWarsProjectile.h"
#include "PillowWarsCover.h"
#include "PillowWarsMatchState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

APillowWarsWeapon::APillowWarsWeapon()
{
    bReplicates = true; bAlwaysRelevant = true;
    PrimaryActorTick.bCanEverTick = true;
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("VisualRoot"));
    BobbleVisual = CreateDefaultSubobject<UPoseableMeshComponent>(TEXT("PlushRig"));
    BobbleVisual->SetupAttachment(RootComponent);
    BobbleVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BackPillow=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackPillow")); BackPillow->SetupAttachment(RootComponent);
    BackPillow->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> ThrowMesh(TEXT("/Game/PillowWars/Props/ThrowPillow/SM_ThrowPillow.SM_ThrowPillow"));
    if(ThrowMesh.Succeeded())BackPillow->SetStaticMesh(ThrowMesh.Object);
    static ConstructorHelpers::FObjectFinder<USoundBase> SwingAsset(TEXT("/Game/PillowWars/Audio/S_Swing.S_Swing"));
    static ConstructorHelpers::FObjectFinder<USoundBase> HitAsset(TEXT("/Game/PillowWars/Audio/S_Hit.S_Hit"));
    static ConstructorHelpers::FObjectFinder<USoundBase> ResonanceAsset(TEXT("/Game/PillowWars/Audio/S_Resonance.S_Resonance"));
    SwingSound=SwingAsset.Object; HitSound=HitAsset.Object; ResonanceSound=ResonanceAsset.Object;
    const TCHAR* Names[] = {TEXT("Blue_Stargazer"),TEXT("Red_Lightning"),TEXT("Green_BlanketScout"),TEXT("Purple_DreamPrankster")};
    for (const TCHAR* Name : Names)
    {
        const FString Path = FString::Printf(TEXT("/Game/PillowWars/Characters/V4/%s/SK_%s_V4.SK_%s_V4"),Name,Name,Name);
        ConstructorHelpers::FObjectFinder<USkeletalMesh> Asset(*Path);
        Variants.Add(Asset.Object);
    }
    for (const TCHAR* Color : {TEXT("Blue"),TEXT("Red"),TEXT("Green"),TEXT("Purple")})
    {
        const FString Path=FString::Printf(TEXT("/Game/PillowWars/Characters/PolishedNinjas/Ninja_%s/SK_Ninja_%s.SK_Ninja_%s"),Color,Color,Color);
        ConstructorHelpers::FObjectFinder<USkeletalMesh> Asset(*Path);
        Variants.Add(Asset.Object);
    }
}
void APillowWarsWeapon::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APillowWarsWeapon,SwingTime);
    DOREPLIFETIME(APillowWarsWeapon,HitPulseTime);
    DOREPLIFETIME(APillowWarsWeapon,ContactTime);
    DOREPLIFETIME(APillowWarsWeapon,VariantIndex);
    DOREPLIFETIME(APillowWarsWeapon,AttackVariant);
    DOREPLIFETIME(APillowWarsWeapon,AttackSequence);
    DOREPLIFETIME(APillowWarsWeapon,ChargeStartTime);
    DOREPLIFETIME(APillowWarsWeapon,ChargePower);
    DOREPLIFETIME(APillowWarsWeapon,ReleasedChargeDuration);
    DOREPLIFETIME(APillowWarsWeapon,SwingPower);
    DOREPLIFETIME(APillowWarsWeapon,Impacts);
    DOREPLIFETIME(APillowWarsWeapon,ReactionSequence);
    DOREPLIFETIME(APillowWarsWeapon,ResonanceCharge);
    DOREPLIFETIME(APillowWarsWeapon,ResonanceAttackPower);
    DOREPLIFETIME(APillowWarsWeapon,AttackCriticalChance);
    DOREPLIFETIME(APillowWarsWeapon,bLastContactCritical);
    DOREPLIFETIME(APillowWarsWeapon,LastResonanceImpactTime);
    DOREPLIFETIME(APillowWarsWeapon,ThrowTime);
    DOREPLIFETIME(APillowWarsWeapon,ThrowReadyTime);
    DOREPLIFETIME(APillowWarsWeapon,ThrowSequence);
}
void APillowWarsWeapon::ReceiveImpact(const FVector& Direction,float Strength)
{
    if(!HasAuthority())return;
    FPillowImpact Impact; Impact.Time=GetWorld()->GetTimeSeconds();
    Impact.Direction=Direction.GetSafeNormal(); Impact.Strength=FMath::Clamp(Strength/270.f,.5f,1.6f);
    ResonanceCharge=FMath::Clamp(ResonanceCharge+20.f+Impact.Strength*18.f,0.f,100.f);
    // Store the gauge with this event: spending/expiring Resonance must not
    // abruptly change an already-playing incoming-hit reaction on any client.
    Impact.Resonance=ResonanceCharge/100.f;
    Impacts.Add(Impact); if(Impacts.Num()>8)Impacts.RemoveAt(0);
    HitPulseTime=Impact.Time;
    LastResonanceImpactTime=Impact.Time;
    ++ReactionSequence; ForceNetUpdate();
}
float APillowWarsWeapon::ResonanceReactionGain(float ResonancePercent)
{
    const float R=FMath::Clamp(ResonancePercent/100.f,0.f,1.f);
    return 1.f+.9f*R*R*(3.f-2.f*R);
}
bool APillowWarsWeapon::IsResonanceWindow(float Now) const
{
    const float Age=Now-LastResonanceImpactTime;
    return ResonanceCharge>=25.f && Age>=.20f && Age<=2.40f && FMath::Sin(Age*9.f)<=-.45f;
}
void APillowWarsWeapon::MulticastSwingAudio_Implementation(bool bResonant)
{
    if(USoundBase* Sound=bResonant?ResonanceSound:SwingSound)
        UGameplayStatics::PlaySoundAtLocation(this,Sound,GetActorLocation(),bResonant?1.f:.72f,bResonant?.92f:1.f);
}
void APillowWarsWeapon::MulticastHitAudio_Implementation(FVector Location,bool bResonant)
{
    if(USoundBase* Sound=bResonant?ResonanceSound:HitSound)
        UGameplayStatics::PlaySoundAtLocation(this,Sound,Location,bResonant?1.15f:.92f,bResonant?.78f:1.f);
}
void APillowWarsWeapon::SetVariant(int32 Index)
{
    if (!HasAuthority()) return;
    VariantIndex = (Index % Variants.Num() + Variants.Num()) % Variants.Num();
    ForceNetUpdate();
}
void APillowWarsWeapon::StartSwing(float Now,float Power)
{
    if (!HasAuthority()) return;
    SwingPower=FMath::Clamp(Power,.55f,1.f);
    AttackCriticalChance=APillowWarsGameMode::CriticalChance(ResonanceCharge);
    bLastContactCritical=false;
    ResonanceAttackPower=0;
    if(IsResonanceWindow(Now))
    {
        ResonanceAttackPower=ResonanceCharge/100.f;
        ResonanceCharge=0;
    }
    AttackVariant=AttackSequence%3; ++AttackSequence;
    ChargePower=0; ReleasedChargeDuration=0; ChargeStartTime=-100;
    SwingTime = Now; LastTraceTime = Now; PreviousTraceWorld=VisualWorld(); HitActors.Reset();HitCovers.Reset(); ForceNetUpdate();
    MulticastSwingAudio(ResonanceAttackPower>0);
}
void APillowWarsWeapon::ResetAttack()
{
    ThrowTime=-100; ThrowReadyTime=0; ThrowSequence=0; bThrowReleased=true;
    ChargeStartTime=-100; ChargePower=0; ReleasedChargeDuration=0;
    SwingTime = -100; HitPulseTime = -100; ContactTime = -100; HitActors.Reset();HitCovers.Reset(); Impacts.Reset(); ReactionSequence=0; AttackVariant=0; AttackSequence=0;SwingPower=1.f;
    ResonanceCharge=0; ResonanceAttackPower=0; LastResonanceImpactTime=-100; ForceNetUpdate();
    AttackCriticalChance=.015f; bLastContactCritical=false;
    HeadAngle=0;HeadVelocity=0;BobbleStrength=0;BobbleVisibility=1;
    ResonanceBobble=FVector2D::ZeroVector;HeadReaction=FVector2D::ZeroVector;HeadDip=0;
}
bool APillowWarsWeapon::StartThrow(float Now)
{
    if(!HasAuthority()||ChargeStartTime>=0||Now<ThrowReadyTime||Now-SwingTime<.65f||IsThrowing(Now))return false;
    ThrowTime=Now; ThrowReadyTime=Now+15; ++ThrowSequence; bThrowReleased=false; ForceNetUpdate(); return true;
}
FVector APillowWarsWeapon::ThrowHand(float Age) const
{
    const FVector Rest(-42,2,97),Back(-22,-33,148),Loaded(-38,12,180),Release(-18,48,146);
    if(Age<0||Age>=.85f)return Rest;
    if(Age<.22f)return FMath::Lerp(Rest,Back,FMath::SmoothStep(0.f,.22f,Age));
    if(Age<.36f)return FMath::Lerp(Back,Loaded,FMath::SmoothStep(.22f,.36f,Age));
    if(Age<.48f)return FMath::Lerp(Loaded,Release,FMath::SmoothStep(.36f,.48f,Age));
    return FMath::Lerp(Release,Rest,FMath::SmoothStep(.48f,.85f,Age));
}
FTransform APillowWarsWeapon::VisualWorld() const
{
    const ACharacter* Fighter = Cast<ACharacter>(GetOwner());
    if (!Fighter) return FTransform::Identity;
    return FTransform(FRotator(0,Fighter->GetActorRotation().Yaw-90,0),
        Fighter->GetActorLocation()-FVector(0,0,Fighter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3));
}
FTransform APillowWarsWeapon::PillowPose(float Age, bool bDeform) const
{
    if(VariantIndex>=4)
    {
        // Grip the centre of the short edge. The long fabric body hangs below it.
        const FVector Grip(42,0,0), Carry(43,2,97);
        // One shared authored pose for visuals, grip and sweep. Local +Y is forward:
        // rotate in the sagittal plane OUTSIDE the torso, not through the body.
        auto ChargePose=[&](float Held,FVector& Hand,FQuat& Rotation) {
            const float Blend=FMath::SmoothStep(0.f,.28f,Held);
            const float Angle=Held*2.f*PI*.85f;
            const FVector Radial(0,FMath::Sin(Angle),-FMath::Cos(Angle));
            Hand=FMath::Lerp(Carry,FVector(49,0,142)+Radial*18.f,Blend);
            const FQuat CarryRotation=FQuat::FindBetweenNormals(-FVector::ForwardVector,FVector(0,.1f,-1).GetSafeNormal());
            Rotation=FQuat::Slerp(CarryRotation,FQuat::FindBetweenNormals(-FVector::ForwardVector,Radial),Blend);
        };
        FVector Hands[5]={Carry,FVector(48,-8,106),FVector(25,45,100),FVector(-6,25,85),Carry};
        FVector Axes[5]={FVector(0,.10f,-1),FVector(.7f,-.5f,.3f),FVector(-.25f,1,.05f),FVector(-.7f,.2f,-.7f),FVector(0,.10f,-1)};
        if(AttackVariant==1) { Hands[1]=FVector(5,27,100); Hands[2]=FVector(25,35,100); Hands[3]=FVector(45,10,77); Axes[1]=FVector(-.8f,.2f,.4f); Axes[2]=FVector(.3f,1,.05f); Axes[3]=FVector(.8f,.1f,-.65f); }
        if(AttackVariant==2) { Hands[1]=FVector(44,12,136); Hands[2]=FVector(25,47,98); Hands[3]=FVector(36,28,74); Axes[1]=FVector(.5f,-.2f,1); Axes[2]=FVector(.1f,1,-.4f); Axes[3]=FVector(.1f,.3f,-1); }
        // Only the legs grew: attack targets follow the translated upper body.
        for(int I=1;I<4;++I)Hands[I].Z+=40;
        if(AttackVariant==3) {
            Hands[1]=FVector(43,-2,104); Hands[2]=FVector(28,42,144); Hands[3]=FVector(32,25,176);
            Axes[1]=FVector(0,-.2f,-1); Axes[2]=FVector(0,1,.25f); Axes[3]=FVector(0,.35f,1);
        }
        const float Times[5]={0,.20f,.35f,.46f,.65f};
        auto Sample=[&](float T,FVector& H,FQuat& R) {
            H=Carry; R=FQuat::FindBetweenNormals(-FVector::ForwardVector,Axes[0].GetSafeNormal());
            if(ChargeStartTime>=0.f) {
                const auto* GameState=GetWorld()->GetGameState();
                const float ServerNow=GameState?GameState->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
                ChargePose(FMath::Max(0.f,ServerNow-ChargeStartTime),H,R); return;
            }
            if(AttackVariant==3&&ReleasedChargeDuration>0.f&&T<0.f) {
                ChargePose(ReleasedChargeDuration,H,R); return;
            }
            if(T<0 || T>=.65f)return;
            if(AttackVariant==3) {
                // Draw behind the shoulder, then orbit down and forward into the
                // uppercut. Both the gripping hand and fabric travel round the arc;
                // this is not a pillow rotating in place or a straight upward shove.
                auto Circle=[&](float Degrees,FVector& Hand,FQuat& Rotation) {
                    const float A=FMath::DegreesToRadians(Degrees);
                    const FVector Radial(0,FMath::Sin(A),-FMath::Cos(A));
                    Hand=FVector(43,0,145)+Radial*26;
                    Rotation=FQuat::FindBetweenNormals(-FVector::ForwardVector,Radial);
                };
                FVector Start,End; FQuat StartR,EndR;
                Circle(-135,Start,StartR); Circle(135,End,EndR);
                if(T<.12f) {
                    const float U=FMath::SmoothStep(0.f,.12f,T);
                    FVector ReleasedHand=Carry; FQuat ReleasedRotation=R;
                    if(ReleasedChargeDuration>0.f)ChargePose(ReleasedChargeDuration,ReleasedHand,ReleasedRotation);
                    H=FMath::Lerp(ReleasedHand,Start,U); R=FQuat::Slerp(ReleasedRotation,StartR,U);
                } else if(T<.47f) {
                    Circle(-135+270*FMath::SmoothStep(.12f,.47f,T),H,R);
                } else {
                    const float U=FMath::SmoothStep(.47f,.65f,T);
                    H=FMath::Lerp(End,Carry,U); R=FQuat::Slerp(EndR,R,U);
                }
                return;
            }
            for(int I=0;I<4;++I)if(T>=Times[I]&&T<Times[I+1]) {
                const float U=FMath::SmoothStep(Times[I],Times[I+1],T);
                H=FMath::Lerp(Hands[I],Hands[I+1],U)+FVector(0,8,5)*FMath::Sin(U*PI);
                R=FQuat::Slerp(FQuat::FindBetweenNormals(-FVector::ForwardVector,Axes[I].GetSafeNormal()),FQuat::FindBetweenNormals(-FVector::ForwardVector,Axes[I+1].GetSafeNormal()),U); break;
            }
        };
        FVector H,Unused; FQuat R,UnusedR; Sample(Age,H,UnusedR); Sample(Age-.025f,Unused,R);
        const auto* State=GetWorld()->GetGameState(); const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
        const float T=Now-ContactTime;
        const float Squash=bDeform&&T>=0&&T<.16f?.16f*FMath::Sin(PI*T/.16f):0;
        const FVector Scale(1-Squash*.35f,1-Squash,1+Squash*.2f);
        return FTransform(R,H-R.RotateVector(Grip*Scale),Scale);
    }
    // The right lower-corner grip is the pivot: no handle and no moving grip point.
    const FVector Corner(38,2,-21), Carry(38,54,64);
    FVector Wind,Contact,Follow; float WindAngle,ContactAngle,FollowAngle;
    const bool Overhead=AttackVariant==2;
    if(Overhead) { Wind=FVector(48,27,45); Contact=FVector(34,66,112); Follow=FVector(28,55,48); WindAngle=28; ContactAngle=-72; FollowAngle=-154; }
    else if(AttackVariant==0) { Wind=FVector(-22,27,43); Contact=FVector(24,69,90); Follow=FVector(62,26,62); WindAngle=-82; ContactAngle=18; FollowAngle=96; }
    else { Wind=FVector(64,22,46); Contact=FVector(18,69,88); Follow=FVector(-16,32,64); WindAngle=88; ContactAngle=-16; FollowAngle=-88; }
    const bool Ninja=VariantIndex>=4;
    // Pajama ninjas let gravity lower the pillow beside the hip, then accelerate it
    // through a body-led diagonal arc. The hand leads and the fabric follows.
    // The existing cast retains its original choreography for comparison and rollback.
    if(!Ninja)
    {
        if(Overhead){Wind=FVector(40,36,140);Contact=FVector(38,64,100);Follow=FVector(38,58,59);WindAngle=-38;ContactAngle=-92;FollowAngle=-155;}
        else if(AttackVariant==0){Wind=FVector(-18,28,97);Contact=FVector(25,70,86);Follow=FVector(64,12,64);WindAngle=-78;ContactAngle=12;FollowAngle=88;}
        else {Wind=FVector(64,12,100);Contact=FVector(22,70,86);Follow=FVector(-18,28,64);WindAngle=88;ContactAngle=-12;FollowAngle=-78;}
    }
    auto Bezier=[](const FVector& A,const FVector& B,const FVector& Control,float T)
    {
        const float S=1-T;
        return S*S*A+2*S*T*Control+T*T*B;
    };
    auto Sample=[&](float T,FVector& Hand,float& Angle)
    {
        Hand=Carry; Angle=0;
        if(T<0||T>=.64f)return;
        if(T<.20f)
        {
            const float U=FMath::SmoothStep(.045f,.20f,T);
            Hand=Bezier(Carry,Wind,(Carry+Wind)*.5f+FVector(0,-9,-14),U);
            Angle=FMath::Lerp(0.f,WindAngle,U);
        }
        else if(T<.39f)
        {
            const float U=FMath::SmoothStep(.20f,.39f,T);
            const FVector Control=(Wind+Contact)*.5f+FVector(0,Overhead?11.f:24.f,Overhead?58.f:22.f);
            Hand=Bezier(Wind,Contact,Control,U);
            Angle=FMath::Lerp(WindAngle,ContactAngle,U);
        }
        else if(T<.47f)
        {
            const float U=FMath::SmoothStep(.39f,.47f,T);
            Hand=Bezier(Contact,Follow,(Contact+Follow)*.5f+FVector(0,7,Overhead?-28.f:5.f),U);
            Angle=FMath::Lerp(ContactAngle,FollowAngle,U);
        }
        else if(T<.60f)
        {
            const float U=FMath::SmoothStep(.47f,.60f,T);
            Hand=Bezier(Follow,Carry,(Follow+Carry)*.5f+FVector(0,9,-8),U);
            Angle=FMath::Lerp(FollowAngle,0.f,U);
        }
    };
    FVector Hand,Unused; float Angle,LagAngle; Sample(Age,Hand,Angle); Sample(Age-.025f,Unused,LagAngle);
    float Tilt=0;
    if(Overhead)
    {
        const float T=Age-.025f;
        if(T>=.16f&&T<.32f)Tilt=82*FMath::SmoothStep(.16f,.32f,T);
        else if(T>=.32f&&T<.49f)Tilt=82*(1-FMath::SmoothStep(.32f,.49f,T));
    }
    // Rotation trails the controlling hand slightly; both meet the carry pose before regrip.
    const FQuat Rotation=FQuat(Overhead?FVector::ForwardVector:FVector::UpVector,FMath::DegreesToRadians(LagAngle))*FQuat(FVector::RightVector,FMath::DegreesToRadians(Tilt));
    const auto* State=GetWorld()->GetGameState();
    const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    const float ContactAge=Now-ContactTime;
    const float Squash = bDeform && ContactAge>=0 && ContactAge<.16f ? .08f*FMath::Sin(ContactAge/.16f*PI) : 0;
    const FVector Scale(1+Squash*.2f,1-Squash,1);
    return FTransform(Rotation,Hand-Rotation.RotateVector(Corner*Scale),Scale);
}
float APillowWarsWeapon::ReleaseAt(float Age) const
{
    if(VariantIndex>=4)return 1; // One-handed carry as well as attack.
    if(Age<0||Age>=.64f)return 0;
    return FMath::SmoothStep(0.f,.065f,Age)*(1-FMath::SmoothStep(.53f,.64f,Age));
}
FQuat APillowWarsWeapon::FlexRotation(float Age) const
{
    if(VariantIndex>=4 && ChargeStartTime>=0.f)
    {
        const auto* State=GetWorld()->GetGameState();
        const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
        const float Held=FMath::Max(0.f,Now-ChargeStartTime);
        // A small, time-synchronized fabric lag leaves the gripped edge fixed.
        // Charging never queries contacts; release still uses the shared sweep.
        const float Bend=(10.f+4.f*FMath::Sin(Held*2.f*PI*.85f))*FMath::SmoothStep(0.f,.28f,Held);
        return FQuat(FVector::RightVector,FMath::DegreesToRadians(-Bend));
    }
    if(Age<0||Age>=.64f)return FQuat::Identity;
    // The far half sags while loading, trails the hand during acceleration, and
    // overshoots briefly in follow-through. This is a bounded soft-body cue.
    const float Drop=FMath::Sin(FMath::Clamp(Age/.20f,0.f,1.f)*PI*.5f)*(1-FMath::SmoothStep(.20f,.43f,Age));
    const float Whip=FMath::Sin(FMath::Clamp((Age-.18f)/.31f,0.f,1.f)*PI);
    const float Settle=Age>.43f?FMath::Sin((Age-.43f)/.21f*PI)*.35f:0.f;
    const float SideSign=AttackVariant==1?-1.f:1.f;
    const float SideLag=(AttackVariant==2?7.f:SideSign*17.f)*Whip-SideSign*7.f*Settle;
    const float Sag=-16.f*Drop-(AttackVariant==2?13.f:8.f)*Whip;
    return FQuat(FVector::UpVector,FMath::DegreesToRadians(SideLag))*FQuat(FVector::RightVector,FMath::DegreesToRadians(Sag));
}
FVector APillowWarsWeapon::PillowPoint(float Age,const FVector& Point) const
{
    const FVector Corner=VariantIndex>=4?FVector(42,0,0):FVector(38,2,-21);
    const float Weight=FMath::SmoothStep(0.f,1.f,FMath::Clamp(float((38-Point.X)/70),0.f,1.f));
    return PillowPose(Age).TransformPosition(FMath::Lerp(Point,Corner+FlexRotation(Age).RotateVector(Point-Corner),Weight));
}
void APillowWarsWeapon::CheckContacts(float Now)
{
    if (!HasAuthority()) return;
    ACharacter* Fighter = Cast<ACharacter>(GetOwner());
    APillowWarsGameMode* GM=GetWorld()->GetAuthGameMode<APillowWarsGameMode>();
    if (!Fighter || !GM || ChargeStartTime>=0.f) return;
    // The uppercut's rear/downward wind-up is visual preparation only.
    // Damage starts when the circular path turns forward and upward.
    const float ActiveStart=AttackVariant==3?.30f:.22f;
    const float From=FMath::Max(LastTraceTime-SwingTime,ActiveStart);
    const float To=FMath::Min(Now-SwingTime,.47f);
    const float PreviousTime=LastTraceTime;
    const FTransform CurrentWorld=VisualWorld();
    const FTransform StartWorld=FVector::Dist(PreviousTraceWorld.GetLocation(),CurrentWorld.GetLocation())<300?PreviousTraceWorld:CurrentWorld;
    PreviousTraceWorld=CurrentWorld;
    LastTraceTime=Now;
    if (From>To || Now<SwingTime+ActiveStart || From>.47f) return;
    FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    FCollisionQueryParams Query; Query.AddIgnoredActor(Fighter); Query.AddIgnoredActor(this);
    const int32 Steps=FMath::Clamp(FMath::CeilToInt((To-From)/.015f),1,16);
    auto RootAt=[&](float Age)
    {
        FTransform Result;
        Result.Blend(StartWorld,CurrentWorld,FMath::Clamp((SwingTime+Age-PreviousTime)/FMath::Max(Now-PreviousTime,.001f),0.f,1.f));
        return Result;
    };
    for (int32 I=0; I<Steps; ++I)
    {
        const float AgeA=FMath::Lerp(From,To,float(I)/Steps),AgeB=FMath::Lerp(From,To,float(I+1)/Steps);
        for(int32 Strip=0;Strip<4;++Strip)
        {
        const FVector Center(-39+Strip*26,0,0);
        const FVector A=RootAt(AgeA).TransformPosition(PillowPoint(AgeA,Center));
        const FVector B=RootAt(AgeB).TransformPosition(PillowPoint(AgeB,Center));
        const float Weight=FMath::SmoothStep(0.f,1.f,FMath::Clamp(float((38-Center.X)/70),0.f,1.f));
        const FQuat Rotation=RootAt(AgeB).GetRotation()*PillowPose(AgeB).GetRotation()*FQuat::Slerp(FQuat::Identity,FlexRotation(AgeB),Weight);
        TArray<FHitResult> Hits;
        // The query matches the pillow body, not an invisible radial attack area.
        GetWorld()->SweepMultiByObjectType(Hits,A,B,Rotation,Objects,FCollisionShape::MakeBox(FVector(13,17,28)),Query);
        for (const FHitResult& Hit : Hits)
        {
            if(APillowWarsCover* Cover=Cast<APillowWarsCover>(Hit.GetActor()))
            {
                if(!HitCovers.Contains(Cover))
                {
                    HitCovers.Add(Cover);
                    Cover->ReceivePillowHit(Cast<APlayerController>(Fighter->GetController()));
                    ContactTime=Now;ForceNetUpdate();MulticastHitAudio(Hit.ImpactPoint,false);
                }
                continue;
            }
            ACharacter* Victim=Cast<ACharacter>(Hit.GetActor());
            if (!Victim || HitActors.Contains(Victim) || FVector::Dist(Fighter->GetActorLocation(),Victim->GetActorLocation())>180.f) continue;
            FCollisionQueryParams WallQuery; WallQuery.AddIgnoredActor(Fighter); WallQuery.AddIgnoredActor(Victim); WallQuery.AddIgnoredActor(this);
            FHitResult Wall;
            if (GetWorld()->LineTraceSingleByChannel(Wall,Fighter->GetActorLocation(),Victim->GetActorLocation(),ECC_Visibility,WallQuery)) continue;
            // Gameplay knockback remains away from the attacker. This vector drives the
            // replicated soft head reaction and follows the authored strike plane.
            const FVector LocalImpact=AttackVariant==3 ? FVector(0,.45f,.89f) : AttackVariant==2
                ? FVector(.55f,0,-.84f)
                : FVector(.20f,AttackVariant==0?.95f:-.95f,.23f);
            const FVector VisualImpact=RootAt(AgeB).TransformVectorNoScale(LocalImpact).GetSafeNormal();
            if (GM->ApplyPillowContact(Cast<APlayerController>(Fighter->GetController()),Victim,VisualImpact,AttackVariant==3?ChargePower:-1.f))
            {
                HitActors.Add(Victim); ContactTime=Now; ForceNetUpdate();
                MulticastHitAudio(Victim->GetActorLocation(),ResonanceAttackPower>0);
            }
        }
        }
    }
}
void APillowWarsWeapon::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    ACharacter* Fighter=Cast<ACharacter>(GetOwner());
    if (!IsValid(Fighter)) { if(HasAuthority()) Destroy(); return; }
    const auto* State=GetWorld()->GetGameState();
    const auto* FighterState=Fighter->GetPlayerState<APillowWarsPlayerState>();
    const bool bStuffingResting=FighterState&&FighterState->bStuffingResting;
    const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
    if(HasAuthority()&&ResonanceCharge>0&&Now-LastResonanceImpactTime>4.f)
    {
        ResonanceCharge=0;
        ForceNetUpdate();
    }
    CheckContacts(Now);
    const float ThrowAge=Now-ThrowTime;
    if(HasAuthority()&&!bThrowReleased&&ThrowAge>=.48f) {
        bThrowReleased=true;
        FActorSpawnParameters Params; Params.Owner=Fighter; Params.Instigator=Fighter; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        const FVector Start=VisualWorld().TransformPosition(ThrowHand(.48f)+FVector(22,1,-13));
        FCollisionQueryParams Query;Query.AddIgnoredActor(Fighter);Query.AddIgnoredActor(this);FHitResult Block;
        if(!GetWorld()->LineTraceSingleByChannel(Block,Fighter->GetActorLocation(),Start,ECC_Visibility,Query)) {
            auto* Projectile=GetWorld()->SpawnActor<APillowWarsProjectile>(Start,VisualWorld().Rotator(),Params);
            if(Projectile)Projectile->Launch(Fighter->GetActorForwardVector()+FVector(0,0,.34f));
        }
    }
    BackPillow->SetVisibility(Fighter->IsPlayerControlled()&&(Now>=ThrowReadyTime||(ThrowAge>=0&&ThrowAge<.48f)));
    BackPillow->SetRelativeLocation(ThrowAge>=.22f&&ThrowAge<.48f?ThrowHand(ThrowAge)+FVector(22,1,-13):FVector(0,-32,VariantIndex>=4?135:78));
    BackPillow->SetRelativeRotation(FRotator(0,0,90));
    if (LoadedVariant!=VariantIndex && Variants.IsValidIndex(VariantIndex) && Variants[VariantIndex])
    {
        BobbleVisual->SetSkinnedAssetAndUpdate(Variants[VariantIndex]); LoadedVariant=VariantIndex;
    }
    USkeletalMesh* Asset=Cast<USkeletalMesh>(BobbleVisual->GetSkinnedAsset());
    if (!Asset) return; // Missing import leaves original mannequin visible.
    Fighter->GetMesh()->SetVisibility(false,false);
    SetActorTransform(VisualWorld());
    const auto& Rig=Asset->GetRefSkeleton(); const auto& Rest=Rig.GetRefBonePose();
    const bool bSleepyRig=Rig.FindBoneIndex(TEXT("Chest"))>=0;
    TArray<FTransform> RestC=Rest, Local=Rest, C=Rest;
    for (int32 I=0; I<Rest.Num(); ++I) if(Rig.GetParentIndex(I)>=0) RestC[I]=Rest[I]*RestC[Rig.GetParentIndex(I)];
    auto Rotate=[&](const TCHAR* Name,FVector Axis,float Deg)
    {
        int32 I=Rig.FindBoneIndex(FName(Name)); if(I<0)return;
        int32 P=Rig.GetParentIndex(I); if(P>=0) Axis=RestC[P].InverseTransformVectorNoScale(Axis);
        Local[I].SetRotation((FQuat(Axis,FMath::DegreesToRadians(Deg))*Local[I].GetRotation()).GetNormalized());
    };
    const float Age=Now-SwingTime, Speed=Fighter->GetVelocity().Size2D(), Dt=FMath::Min(DeltaSeconds,.033f);
    const float ChargeAge=ChargeStartTime>=0.f?FMath::Max(0.f,Now-ChargeStartTime):0.f;
    const float ChargeAlpha=ChargeStartTime>=0.f?FMath::Clamp(ChargeAge/2.f,0.f,1.f):0.f;
    const bool bIdleSwing=Age<0.f||Age>=.65f;
    StuffingBlend=FMath::FInterpTo(StuffingBlend,bStuffingResting&&bIdleSwing&&ChargeStartTime<0.f?1.f:0.f,DeltaSeconds,10.f);
    WalkPhase+=DeltaSeconds*FMath::Clamp(Speed/(bSleepyRig?20.f:42.f),0.f,bSleepyRig?24.f:13.f);
    const bool Air=Fighter->GetCharacterMovement()->IsFalling();
    GaitBlend=FMath::FInterpTo(GaitBlend,FMath::Clamp(Speed/450.f,0.f,1.f),DeltaSeconds,7.f);
    const float Run=GaitBlend;
    if(bSleepyRig) {
        const FVector Acceleration=(Fighter->GetVelocity()-PreviousVelocity)/FMath::Max(DeltaSeconds,.001f);
        const FVector2D Desired(
            FMath::Clamp(FVector::DotProduct(Acceleration,Fighter->GetActorForwardVector())*.0025f+Run*5.f,-9.f,12.f),
            FMath::Clamp(FVector::DotProduct(Acceleration,Fighter->GetActorRightVector())*.003f,-9.f,9.f));
        const int32 Steps=FMath::Clamp(FMath::CeilToInt(DeltaSeconds/.008f),1,16);
        const float Step=FMath::Min(DeltaSeconds,.1f)/Steps;
        for(int32 I=0;I<Steps;++I) {
            BalanceVelocity+=(Desired-BalanceAngle)*(55.f*Step)-BalanceVelocity*(13.f*Step);
            BalanceAngle+=BalanceVelocity*Step;
        }
        if(bWasAirborne&&!Air)LandingSquash=FMath::Clamp(-PreviousFallSpeed*.008f,0.f,7.f);
        LandingSquash=FMath::FInterpTo(LandingSquash,0.f,DeltaSeconds,9.f);
        bWasAirborne=Air; PreviousFallSpeed=Fighter->GetVelocity().Z;
    }
    const float Stride=Air?0:FMath::Sin(WalkPhase)*36*Run;
    Rotate(TEXT("Leg_L"),FVector(1,0,0),Air?15:Stride);
    Rotate(TEXT("Leg_R"),FVector(1,0,0),Air?-15:-Stride);
    Rotate(TEXT("Knee_L"),FVector(1,0,0),Air?55:(12+FMath::Max(0.f,-FMath::Cos(WalkPhase))*65)*Run);
    Rotate(TEXT("Knee_R"),FVector(1,0,0),Air?40:(12+FMath::Max(0.f,FMath::Cos(WalkPhase))*65)*Run);
    if(StuffingBlend>.001f&&VariantIndex>=4)
    {
        Rotate(TEXT("Spine"),FVector::ForwardVector,(-10.f-2.f*FMath::Sin(Now*7.f))*StuffingBlend);
        Rotate(TEXT("Knee_L"),FVector(1,0,0),34.f*StuffingBlend);
        Rotate(TEXT("Knee_R"),FVector(1,0,0),34.f*StuffingBlend);
        Rotate(TEXT("Leg_L"),FVector(1,0,0),-8.f*StuffingBlend);
        Rotate(TEXT("Leg_R"),FVector(1,0,0),-8.f*StuffingBlend);
        const int32 RestHip=Rig.FindBoneIndex(TEXT("Pelvis"));
        if(RestHip>=0)Local[RestHip].AddToTranslation(FVector(0,0,-10.f*StuffingBlend));
    }
    if(VariantIndex>=4) {
        const float CombatBalance=Age>=0&&Age<.65f?.35f:1.f;
        Rotate(TEXT("Spine"),FVector::ForwardVector,bSleepyRig?-BalanceAngle.X*CombatBalance:-17.f*Run);
        if(bSleepyRig) {
            Rotate(TEXT("Chest"),FVector::ForwardVector,-2*Run);
            Rotate(TEXT("Spine"),FVector::RightVector,-BalanceAngle.Y*.6f*CombatBalance);
        }
        Rotate(TEXT("Head_Bobble"),FVector::ForwardVector,(bSleepyRig?4.f:11.f)*Run);
        const float Sway=Air?0:FMath::Sin(WalkPhase)*4*Run;
        Rotate(TEXT("Pelvis"),FVector::RightVector,Sway);
        Rotate(TEXT("Spine"),FVector::RightVector,-Sway*.65f);
        const int32 Hip=Rig.FindBoneIndex(TEXT("Pelvis"));
        if(Hip>=0&&!Air)Local[Hip].AddToTranslation(FVector(bSleepyRig?FMath::Sin(WalkPhase)*1.8f*Run:0,0,
            bSleepyRig ? -4.f-2.f*Run-LandingSquash+Run*(1-FMath::Cos(WalkPhase*2))*1.1f : Run*(1-FMath::Cos(WalkPhase*2))*1.5f));
    }
    if(ChargeAlpha>0.f&&(Age<0.f||Age>=.65f))
    {
        Rotate(TEXT("Spine"),FVector::ForwardVector,-10.f*ChargeAlpha);
        Rotate(TEXT("Spine"),FVector::UpVector,7.f*ChargeAlpha);
    }
    SupportRelease=ReleaseAt(Age); PillowBend=FMath::RadiansToDegrees(FlexRotation(Age).GetAngle());
    PillowCenter=PillowPoint(Age,FVector::ZeroVector);
    PillowLeadingEdge=PillowPoint(Age,FVector(-39,0,0));
    PillowPathSpeed=DeltaSeconds>.001f?FVector::Dist(PillowCenter,PreviousPillowCenter)/DeltaSeconds:0;
    PreviousPillowCenter=PillowCenter;
    // Turn the right shoulder toward the across-body wind-up, not away from its grip.
    TorsoYaw=VariantIndex>=4 ? (Age>=0&&Age<.65f ? (AttackVariant==1?-1.f:1.f)*28*FMath::Sin((Age/.65f*2-0.5f)*PI)*FMath::Sin(Age/.65f*PI):0) : -PillowPose(Age).Rotator().Yaw*.40f;
    if(!bSleepyRig)Rotate(TEXT("Pelvis"),FVector(1,0,0),-PillowPose(Age).Rotator().Yaw*.04f);
    Rotate(TEXT("Spine"),FVector::UpVector,TorsoYaw);
    if(AttackVariant==2)Rotate(TEXT("Spine"),FVector::ForwardVector,bSleepyRig
        ? (Age>=.15f&&Age<.55f?-8.f*FMath::Sin((Age-.15f)/.40f*PI):0.f)
        : PillowPose(Age).Rotator().Roll*.18f);
    const FVector V=Fighter->GetVelocity();
    const float Accel=FVector::DotProduct((V-PreviousVelocity)/FMath::Max(DeltaSeconds,.001f),Fighter->GetActorForwardVector()); PreviousVelocity=V;
    HeadReaction=FVector2D::ZeroVector; HeadDip=0; LastImpactDirection=FVector::ZeroVector; FVector2D Recoil=FVector2D::ZeroVector;
    float ReactionResonance=0.f;
    for(const FPillowImpact& Impact:Impacts)
    {
        const float T=Now-Impact.Time; if(T<0 || T>4)continue;
        const float R=FMath::Clamp(Impact.Resonance,0.f,1.f);
        ReactionResonance=FMath::Max(ReactionResonance,R);
        const float Gain=ResonanceReactionGain(R*100.f);
        const float Envelope=FMath::Exp(-(3.8f-1.3f*R)*T)*(1.f-FMath::SmoothStep(2.8f,4.f,T));
        const FVector Dir=VisualWorld().InverseTransformVectorNoScale(Impact.Direction);
        LastImpactDirection=Impact.Direction;
        HeadDip+=Dir.Z*5*Impact.Strength*Gain*Envelope*FMath::Sin(10*T);
        const FVector2D Axis(-Dir.Y,Dir.X);
        HeadReaction+=Axis*(42*Impact.Strength*Gain*Envelope*FMath::Sin((9.f-R)*T));
        Recoil+=Axis*(10*Impact.Strength*FMath::Exp(-6*T)*FMath::Sin(8*T));
    }
    const float ReactionLimit=22.f+6.f*ReactionResonance;
    HeadReaction.X=ReactionLimit*FMath::Tanh(HeadReaction.X/ReactionLimit);
    HeadReaction.Y=ReactionLimit*FMath::Tanh(HeadReaction.Y/ReactionLimit);
    Rotate(TEXT("Spine"),FVector(1,0,0),Recoil.X);
    Rotate(TEXT("Spine"),FVector(0,1,0),Recoil.Y);
    const float Target=FMath::Clamp(-Accel*.004f,-10.f,10.f)+FMath::Sin(Now*2)*1.2f;
    HeadVelocity+=(55*(Target-HeadAngle)-12*HeadVelocity)*Dt;
    HeadAngle=FMath::Clamp(HeadAngle+HeadVelocity*Dt,-18.f,18.f);
    BobbleStrength=FMath::FInterpTo(BobbleStrength,FMath::Clamp(ResonanceCharge/100.f,0.f,1.f),DeltaSeconds,5.f);
    BobbleVisibility=FMath::FInterpTo(BobbleVisibility,bIdleSwing&&ChargeStartTime<0.f&&!IsThrowing(Now)?1.f:0.f,DeltaSeconds,12.f);
    const float ReadyAmplitude=4.f*BobbleStrength*BobbleStrength*BobbleVisibility;
    // Slow, smooth sway, not random jitter. Camera, collision and combat are untouched.
    ResonanceBobble=FVector2D(FMath::Sin(Now*5.f)*ReadyAmplitude,FMath::Sin(Now*4.f+.7f)*ReadyAmplitude*.7f);
    Rotate(TEXT("Head_Bobble"),FVector(1,0,0),FMath::Clamp(HeadAngle+HeadReaction.X+ResonanceBobble.X,-30.f,30.f));
    Rotate(TEXT("Head_Bobble"),FVector(0,1,0),FMath::Clamp(HeadReaction.Y+ResonanceBobble.Y,-30.f,30.f));
    HeadDip=4*FMath::Tanh(HeadDip/4);
    const int32 HeadIndex=Rig.FindBoneIndex(TEXT("Head_Bobble"));
    if(HeadIndex>=0)Local[HeadIndex].AddToTranslation(FVector(0,0,HeadDip));
    // Head counter-turn lags the shoulders during the fast arc and follow-through.
    // No attack-derived Euler yaw subtraction: wrapping across 180 caused head snapping.
    // Incoming-hit spring reactions above remain active.
    Rotate(TEXT("Brow_L"),FVector(0,1,0),Age>=0&&Age<.6f?-8:0);
    Rotate(TEXT("Brow_R"),FVector(0,1,0),Age>=0&&Age<.6f?8:0);
    for(int32 I=0;I<Rest.Num();++I) {const int32 P=Rig.GetParentIndex(I); C[I]=P>=0?Local[I]*C[P]:Local[I];}
    FTransform Pillow=PillowPose(Age,true);
    if(bSleepyRig) {
        const int32 U=Rig.FindBoneIndex(TEXT("UpperArm_R")), E=Rig.FindBoneIndex(TEXT("LowerArm_R")), H=Rig.FindBoneIndex(TEXT("Hand_R"));
        if(U>=0&&E>=0&&H>=0) {
            // Carry follows the moving shoulder at full arm length. Blend out before
            // the active sweep, keeping collision and visible contact identical.
            const float CarryEnd=AttackVariant==3?.12f:.20f;
            const float CarryWeight=(ChargeStartTime>=0.f?1-FMath::SmoothStep(0.f,.28f,ChargeAge):
                AttackVariant==3&&ReleasedChargeDuration>0.f&&Age>=0.f&&Age<.47f?0.f:
                bIdleSwing?1.f:Age<CarryEnd?1-FMath::SmoothStep(0.f,CarryEnd,Age):Age>.47f?FMath::SmoothStep(.47f,.65f,Age):0.f)*(1-StuffingBlend);
            const float Length=FVector::Dist(RestC[U].GetLocation(),RestC[E].GetLocation())+FVector::Dist(RestC[E].GetLocation(),RestC[H].GetLocation())-.02f;
            const FVector Hand=C[U].GetLocation()+FVector(18,2,-56).GetSafeNormal()*Length;
            // Use the authored carry grip, not PillowPose(-1): during a released
            // charge that sample is the release pose, which displaced recovery.
            const FVector Delta=Hand-FVector(43,2,97);
            Pillow.AddToTranslation(Delta*CarryWeight);
        }
    }
    if(StuffingBlend>.001f&&VariantIndex>=4) {
        // Hold the opening at the belly. The free hand scoops, inserts, then presses.
        const float Pump=.5f+.5f*FMath::Sin(Now*7.f);
        const FVector Hand(24,30,115);
        const FQuat Rotation=FQuat::FindBetweenNormals(-FVector::ForwardVector,FVector(-1,.18f,-.12f).GetSafeNormal());
        const FVector Scale(1.f,1.f-.08f*Pump,1.f+.025f*Pump);
        const FTransform StuffPose(Rotation,Hand-Rotation.RotateVector(FVector(42,0,0)*Scale),Scale);
        FTransform Blended; Blended.Blend(Pillow,StuffPose,StuffingBlend); Pillow=Blended;
    }
    TMap<int32,FTransform> Override;
    const int32 PillowIndex=Rig.FindBoneIndex(TEXT("Pillow")); if(PillowIndex<0)return;
    Override.Add(PillowIndex,Pillow);
    const int32 FlexIndex=Rig.FindBoneIndex(TEXT("PillowFlex"));
    const FVector FlexPivot=VariantIndex>=4?FVector(42,0,0):FVector(38,2,-21);
    if(FlexIndex>=0)Override.Add(FlexIndex,FTransform(Pillow.GetRotation()*FlexRotation(Age),Pillow.TransformPosition(FlexPivot+FlexRotation(Age).RotateVector(FVector(38,2,-21)-FlexPivot)),Pillow.GetScale3D()));
    GripError=0;
    for(const TCHAR* Side : {TEXT("L"),TEXT("R")})
    {
        const int32 U=Rig.FindBoneIndex(FName(*FString::Printf(TEXT("UpperArm_%s"),Side)));
        const int32 E=Rig.FindBoneIndex(FName(*FString::Printf(TEXT("LowerArm_%s"),Side)));
        const int32 H=Rig.FindBoneIndex(FName(*FString::Printf(TEXT("Hand_%s"),Side)));
        const int32 G=Rig.FindBoneIndex(FName(*FString::Printf(TEXT("Grip_%s"),Side)));
        if(U<0||E<0||H<0||G<0)continue;
        const FVector S=C[U].GetLocation();
        const FVector FabricGrip=Pillow.TransformPosition(VariantIndex>=4?FVector(42,0,0):RestC[G].GetLocation()-RestC[PillowIndex].GetLocation());
        const bool Supporting=Side[0]=='L';
        // Free arm stays outside its shoulder and counterbalances rather than crossing the torso.
        const FVector Balance=S+(bSleepyRig
            ? FVector(-17,2-12*Run+FMath::Sin(WalkPhase+.4f)*8*Run,-56+Run*5)
            : FVector(-18,-12-20*Run+FMath::Sin(WalkPhase+.4f)*12*Run,-28+Run*6));
        FVector Grip=Supporting?(IsThrowing(Now)?ThrowHand(ThrowAge):FMath::Lerp(FabricGrip,Balance,SupportRelease)):FabricGrip;
        if(Supporting&&StuffingBlend>.001f) {
            const float Pump=.5f+.5f*FMath::Sin(Now*7.f);
            const FVector Scoop(-36,27,103), Opening(4,33,116);
            Grip=FMath::Lerp(Grip,FMath::Lerp(Scoop,Opening,FMath::SmoothStep(0.f,1.f,Pump)),StuffingBlend);
        }
        const float L1=FVector::Dist(RestC[U].GetLocation(),RestC[E].GetLocation());
        const float L2=FVector::Dist(RestC[E].GetLocation(),RestC[H].GetLocation());
        const FVector Dir=(Grip-S).GetSafeNormal(); const float D=FMath::Clamp(FVector::Dist(Grip,S),FMath::Abs(L1-L2)+.01f,L1+L2-.01f);
        const FVector Hand=S+Dir*D;
        FVector Pole(RestC[U].GetLocation().X<0?-1.f:1.f,0,-.65f); Pole=(Pole-Dir*FVector::DotProduct(Pole,Dir)).GetSafeNormal();
        const float Along=(L1*L1-L2*L2+D*D)/(2*D);
        const FVector Elbow=S+Dir*Along+Pole*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
        FTransform Upper=C[U], Fore=C[E], Palm=C[H];
        Upper.SetRotation((FQuat::FindBetweenNormals((RestC[E].GetLocation()-RestC[U].GetLocation()).GetSafeNormal(),(Elbow-S).GetSafeNormal())*RestC[U].GetRotation()).GetNormalized());
        Fore.SetLocation(Elbow);
        Fore.SetRotation((FQuat::FindBetweenNormals((RestC[H].GetLocation()-RestC[E].GetLocation()).GetSafeNormal(),(Hand-Elbow).GetSafeNormal())*RestC[E].GetRotation()).GetNormalized());
        Palm.SetLocation(Hand);
        // New mitten rest pose points down, not forward like the original finger rig.
        Palm.SetRotation(bSleepyRig
            ? (Supporting?Fore.GetRotation():Pillow.GetRotation()*FQuat(FVector::RightVector,PI*.5f))
            : (Supporting?FQuat::Slerp(Pillow.GetRotation(),RestC[H].GetRotation(),SupportRelease):Pillow.GetRotation()));
        Override.Add(U,Upper); Override.Add(E,Fore); Override.Add(H,Palm);
        if(FVector::Dist(Hand,Grip)>1.f)UE_LOG(LogTemp,Warning,TEXT("PW_GRIP_REACH side=%s variant=%d attack=%d age=%.3f shoulder=%s target=%s lengths=%.2f/%.2f"),Side,VariantIndex,AttackVariant,Age,*S.ToString(),*Grip.ToString(),L1,L2);
        GripError=FMath::Max(GripError,FVector::Dist(Hand,Grip));
    }
    if(bSleepyRig&&!Air)for(int32 Side=0;Side<2;++Side) {
        const TCHAR* Suffix=Side==0?TEXT("L"):TEXT("R");
        const int32 U=Rig.FindBoneIndex(FName(*FString::Printf(TEXT("Leg_%s"),Suffix)));
        const int32 K=Rig.FindBoneIndex(FName(*FString::Printf(TEXT("Knee_%s"),Suffix)));
        const int32 F=Rig.FindBoneIndex(FName(*FString::Printf(TEXT("Foot_%s"),Suffix)));
        if(U<0||K<0||F<0)continue;
        // A low stance stroke and a rounded lifted return replace the stiff knee pump.
        const float Phase=FMath::Fmod(WalkPhase+(Side==0?0.f:PI),2*PI);
        const bool Swing=Phase>PI; const float T=Swing?(Phase-PI)/PI:Phase/PI;
        const float Travel=Swing?FMath::Lerp(-30.f,30.f,FMath::SmoothStep(0.f,1.f,T)):FMath::Lerp(30.f,-30.f,T);
        FVector Goal=RestC[F].GetLocation()+FVector((Side==0?-1.f:1.f)*2*Run,Travel*Run,3);
        FHitResult Floor; FCollisionQueryParams Query; Query.AddIgnoredActor(Fighter); Query.AddIgnoredActor(this);
        const FVector WorldFoot=VisualWorld().TransformPosition(Goal);
        if(GetWorld()->LineTraceSingleByChannel(Floor,WorldFoot+FVector(0,0,35),WorldFoot-FVector(0,0,70),ECC_Visibility,Query)&&Floor.ImpactNormal.Z>.65f)
            Goal.Z=VisualWorld().InverseTransformPosition(Floor.ImpactPoint).Z+RestC[F].GetLocation().Z;
        Goal.Z+=(Swing?FMath::Sin(PI*T)*14.f:0.f)*Run;
        const FVector Hip=C[U].GetLocation();
        const float L1=FVector::Dist(RestC[U].GetLocation(),RestC[K].GetLocation()),L2=FVector::Dist(RestC[K].GetLocation(),RestC[F].GetLocation());
        const FVector Dir=(Goal-Hip).GetSafeNormal();
        const float D=FMath::Clamp(FVector::Dist(Goal,Hip),FMath::Abs(L1-L2)+.1f,L1+L2-.1f);
        Goal=Hip+Dir*D;
        FVector Pole=FVector(0,1,0)-Dir*FVector::DotProduct(FVector(0,1,0),Dir); Pole.Normalize();
        const float Along=(L1*L1-L2*L2+D*D)/(2*D);
        const FVector Knee=Hip+Dir*Along+Pole*FMath::Sqrt(FMath::Max(0.f,L1*L1-Along*Along));
        FTransform Upper=C[U],Lower=C[K],Foot=C[F];
        Upper.SetRotation((FQuat::FindBetweenNormals((RestC[K].GetLocation()-RestC[U].GetLocation()).GetSafeNormal(),(Knee-Hip).GetSafeNormal())*RestC[U].GetRotation()).GetNormalized());
        Lower.SetLocation(Knee); Lower.SetRotation((FQuat::FindBetweenNormals((RestC[F].GetLocation()-RestC[K].GetLocation()).GetSafeNormal(),(Goal-Knee).GetSafeNormal())*RestC[K].GetRotation()).GetNormalized());
        Foot.SetLocation(Goal); Foot.SetRotation(RestC[F].GetRotation());
        Override.Add(U,Upper); Override.Add(K,Lower); Override.Add(F,Foot);
    }
    // Forward propagation preserves finger/thumb articulation and the spring's skin weights.
    for(int32 I=0;I<Rest.Num();++I)
    {
        const int32 P=Rig.GetParentIndex(I);
        C[I]=Override.Contains(I)?Override[I]:(P>=0?Local[I]*C[P]:Local[I]);
        BobbleVisual->BoneSpaceTransforms[I]=P>=0?C[I].GetRelativeTransform(C[P]):C[I];
    }
    BobbleVisual->MarkRefreshTransformDirty(); BobbleVisual->RefreshBoneTransforms();
    if(!bLoggedRig)
    {
        if(auto* Boom=Fighter->FindComponentByClass<USpringArmComponent>()) {Boom->TargetArmLength=500; Boom->SocketOffset=FVector(0,0,110);}
        UE_LOG(LogTemp,Log,TEXT("PW_TWOHAND_READY bones=%d variant=%d"),Rest.Num(),VariantIndex); bLoggedRig=true;
    }
}
