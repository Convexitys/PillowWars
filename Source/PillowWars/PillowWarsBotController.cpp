#include "PillowWarsBotController.h"
#include "PillowWarsMatchState.h"
#include "PillowWarsWeapon.h"
#include "PillowWarsStuffingPickup.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

APillowWarsBotController::APillowWarsBotController()
{
    // Its pawn, PlayerState and weapon replicate normally; the brain stays on the server.
    bReplicates = false;
    PrimaryActorTick.bCanEverTick = true;
}

void APillowWarsBotController::BeginPlay()
{
    // Do not run the human frontend, input capture or preference changes.
    APlayerController::BeginPlay();
    if (PlayerState) PlayerState->SetIsABot(true);
    Random.Initialize(GetUniqueID());
}

void APillowWarsBotController::ResetBot()
{
    Opponent.Reset(); MoveDirection = FVector::ZeroVector;
    Home = GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector;
    NextThink = 0.f; ReleaseAttackAt = -1.f; GuardUntil = 0.f;
    NextAttack = GetWorld()->GetTimeSeconds() + Random.FRandRange(.4f, 1.f);
    ApplyBotControls(FVector::ZeroVector, false, false, false, false);
}

void APillowWarsBotController::OnPossess(APawn* PawnToPossess)
{
    // The existing pawn's possession event installs Enhanced Input for local humans.
    // A server brain has no ULocalPlayer; keep that path closed during initialization.
    bInitializingPawn = true;
    Super::OnPossess(PawnToPossess);
    bInitializingPawn = false;
    AcknowledgedPawn = GetPawn();
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    if (ACharacter* Fighter = Cast<ACharacter>(GetPawn()))
        Fighter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

bool APillowWarsBotController::CanSee(const AActor* Actor) const
{
    if (!GetPawn() || !IsValid(Actor)) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PWBotSight), false);
    Query.AddIgnoredActor(GetPawn());
    FHitResult Hit;
    const bool bBlocked = GetWorld()->LineTraceSingleByChannel(Hit,
        GetPawn()->GetActorLocation(), Actor->GetActorLocation() + FVector(0,0,20), ECC_Visibility, Query);
    return !bBlocked || Hit.GetActor() == Actor;
}

FVector APillowWarsBotController::SafeDirection(const FVector& Desired, bool& bJump) const
{
    bJump = false;
    const ACharacter* Fighter = Cast<ACharacter>(GetPawn());
    if (!Fighter || Desired.IsNearlyZero()) return FVector::ZeroVector;
    const FVector Position = Fighter->GetActorLocation();
    const float FeetZ = Position.Z - Fighter->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PWBotFooting), false);
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        if (It->Get() && It->Get()->GetPawn()) Query.AddIgnoredActor(It->Get()->GetPawn());
    // Fan around local obstacles. Never authorize a blind leap across an elimination edge.
    const float Angles[] = {0.f, 30.f, -30.f, 60.f, -60.f, 90.f, -90.f};
    for (const float Angle : Angles)
    {
        const FVector Direction = Desired.GetSafeNormal2D().RotateAngleAxis(Angle, FVector::UpVector);
        const FVector Probe = Position + Direction * 150.f;
        FHitResult Floor;
        if (!GetWorld()->LineTraceSingleByChannel(Floor, FVector(Probe.X,Probe.Y,FeetZ+280.f),
            FVector(Probe.X,Probe.Y,FeetZ-110.f), ECC_Visibility, Query) || Floor.ImpactNormal.Z < .72f) continue;
        const float Rise = Floor.ImpactPoint.Z - FeetZ;
        if (Rise > 180.f || Rise < -90.f) continue;
        FHitResult Obstacle;
        const bool bBlocked = GetWorld()->SweepSingleByChannel(Obstacle, Position,
            Position + Direction * 100.f, FQuat::Identity, ECC_Visibility,
            FCollisionShape::MakeSphere(Fighter->GetCapsuleComponent()->GetScaledCapsuleRadius() + 4.f), Query);
        if (bBlocked && Rise < 12.f) continue;
        bJump = Rise > 35.f || bBlocked;
        return Direction;
    }
    return FVector::ZeroVector;
}

void APillowWarsBotController::Think(float Now)
{
    ++DecisionsMade;
    const ACharacter* Fighter = Cast<ACharacter>(GetPawn());
    const auto* State = GetPlayerState<APillowWarsPlayerState>();
    if (!Fighter || !State) return;
    const FVector Position = Fighter->GetActorLocation();
    APawn* Nearest = nullptr;
    float BestDistance = 2600.f;
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Other = It->Get();
        const auto* OtherState = Other ? Other->GetPlayerState<APillowWarsPlayerState>() : nullptr;
        APawn* Candidate = Other ? Other->GetPawn() : nullptr;
        if (Other == this || !Candidate || !OtherState || OtherState->bEliminated) continue;
        const float Distance = FVector::Dist(Position, Candidate->GetActorLocation());
        if (Distance < BestDistance && CanSee(Candidate)) { Nearest = Candidate; BestDistance = Distance; }
    }
    if (Nearest) { Opponent = Nearest; LastSeenAt = Now; LastSeenPosition = Nearest->GetActorLocation(); }
    else if (Now - LastSeenAt > 2.f) Opponent.Reset();

    // Refill through the same stationary feather-pile interaction as a human, not a cheat refill.
    APillowWarsStuffingPickup* Pile = nullptr;
    float PileDistance = 1800.f;
    if (State->Stuffing < 35.f || (State->bStuffingResting && State->Stuffing < 80.f))
        for (TActorIterator<APillowWarsStuffingPickup> It(GetWorld()); It; ++It)
        {
            const float Distance = FVector::Dist(Position, It->GetActorLocation());
            if (It->RemainingStuffing > 0.f && Distance < PileDistance && CanSee(*It)) { Pile = *It; PileDistance = Distance; }
        }
    FVector Desired = FVector::ZeroVector;
    bool bThrow = false;
    if (Pile)
    {
        const FVector Delta = Pile->GetActorLocation() - Position;
        if (Delta.Size2D() > 65.f) Desired = Delta.GetSafeNormal2D();
        Decision = Desired.IsNearlyZero() ? TEXT("Restuffing beside feathers") : TEXT("Seeking feathers");
        GuardUntil = 0.f;
    }
    else if (Nearest)
    {
        const FVector Delta = Nearest->GetActorLocation() - Position;
        FacingDirection = Delta.GetSafeNormal2D();
        if (Delta.Size2D() > 100.f) Desired = FacingDirection;
        Decision = TEXT("Approaching opponent");
        if (Delta.Size2D() < 145.f && FMath::Abs(Delta.Z) < 90.f && Now >= NextAttack && ReleaseAttackAt < 0.f)
        {
            GuardUntil = 0.f;
            ReleaseAttackAt = Now + (Random.FRand() < .25f ? Random.FRandRange(.6f, 1.4f) : .08f);
            NextAttack = ReleaseAttackAt + Random.FRandRange(.8f, 1.3f);
        }
        for (TActorIterator<APillowWarsWeapon> It(GetWorld()); It; ++It)
            if (It->GetOwner() == Nearest && Now - It->SwingTime < .24f && BestDistance < 200.f &&
                ReleaseAttackAt < 0.f && Random.FRand() < .35f && State->Stuffing > 20.f) GuardUntil = Now + .30f;
        bThrow = BestDistance > 350.f && BestDistance < 650.f && FMath::Abs(Delta.Z) < 60.f &&
            ReleaseAttackAt < 0.f && Random.FRand() < .04f;
    }
    else
    {
        const FVector Destination = Opponent.IsValid() ? LastSeenPosition : Home;
        if (FVector::DistSquared2D(Destination, Position) > FMath::Square(100.f)) Desired = (Destination-Position).GetSafeNormal2D();
        Decision = TEXT("Searching / returning to safe ground");
    }
    bool bJump = false;
    MoveDirection = SafeDirection(Desired, bJump);
    Steering = MoveDirection;
    if (!Nearest && !MoveDirection.IsNearlyZero()) FacingDirection = MoveDirection;
    if (ReleaseAttackAt > Now) Decision = TEXT("Preparing pillow attack");
    const bool bDoJump = bJump && Now >= NextJump;
    if (bDoJump) NextJump = Now + 1.f;
    ApplyBotControls(MoveDirection, bDoJump, ReleaseAttackAt > Now, GuardUntil > Now, bThrow);
}

void APillowWarsBotController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority()) return;
    const auto* Match = GetWorld()->GetGameState<APillowWarsMatchState>();
    const auto* State = GetPlayerState<APillowWarsPlayerState>();
    ACharacter* Fighter = Cast<ACharacter>(GetPawn());
    if (!bThinkingEnabled || !Match || Match->MatchPhase != EPWMatchPhase::Playing || !State || State->bEliminated || !Fighter)
    {
        MoveDirection = FVector::ZeroVector;
        ApplyBotControls(MoveDirection, false, false, false, false);
        Decision = TEXT("Waiting / eliminated");
        return;
    }
    const float Now = GetWorld()->GetTimeSeconds();
    if (ReleaseAttackAt >= 0.f && Now >= ReleaseAttackAt)
    {
        ApplyBotControls(FVector::ZeroVector, false, false, false, false);
        ReleaseAttackAt = -1.f;
    }
    if (Now >= NextThink) { Think(Now); NextThink = Now + .12f; }
    const FRotator TargetRotation = FacingDirection.Rotation();
    const FRotator Rotation = FMath::RInterpConstantTo(Fighter->GetActorRotation(), TargetRotation, DeltaSeconds, 300.f);
    SetControlRotation(Rotation);
    Fighter->SetActorRotation(Rotation);
    Fighter->AddMovementInput(MoveDirection, .75f);
}
