#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PillowWarsWeapon.generated.h"
class UPoseableMeshComponent;
class USkeletalMesh;
class ACharacter;
class UStaticMeshComponent;
class USoundBase;

USTRUCT()
struct FPillowImpact
{
    GENERATED_BODY()
    UPROPERTY() float Time = -100;
    UPROPERTY() FVector Direction = FVector::ZeroVector;
    UPROPERTY() float Strength = 0;
    UPROPERTY() float Resonance = 0;
};

UCLASS()
class PILLOWWARS_API APillowWarsWeapon : public AActor
{
    GENERATED_BODY()
public:
    APillowWarsWeapon();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    UPROPERTY(Replicated, BlueprintReadOnly) float SwingTime = -100;
    UPROPERTY(Replicated, BlueprintReadOnly) float HitPulseTime = -100;
    UPROPERTY(Replicated, BlueprintReadOnly) float ContactTime = -100;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 VariantIndex = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 AttackVariant = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 AttackSequence = 0;
    UPROPERTY(BlueprintReadOnly) float SupportRelease = 0;
    UPROPERTY(BlueprintReadOnly) float PillowBend = 0;
    UPROPERTY(BlueprintReadOnly) float HeadDip = 0;
    UPROPERTY(BlueprintReadOnly) FVector LastImpactDirection = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float GripError = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 ReactionSequence = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float ResonanceCharge = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float ResonanceAttackPower = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float AttackCriticalChance = .015f;
    UPROPERTY(Replicated, BlueprintReadOnly) bool bLastContactCritical = false;
    UPROPERTY(Replicated, BlueprintReadOnly) float LastResonanceImpactTime = -100;
    UPROPERTY(BlueprintReadOnly) FVector2D HeadReaction = FVector2D::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector2D ResonanceBobble = FVector2D::ZeroVector;
    UFUNCTION(BlueprintPure) static float ResonanceReactionGain(float ResonancePercent);
    UPROPERTY(BlueprintReadOnly) float TorsoYaw = 0;
    UPROPERTY(BlueprintReadOnly) FVector PillowCenter = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) FVector PillowLeadingEdge = FVector::ZeroVector;
    UPROPERTY(BlueprintReadOnly) float PillowPathSpeed = 0;
    void ReceiveImpact(const FVector& Direction, float Strength);
    void StartSwing(float Now, float Power = 1.0f);
    bool IsResonanceWindow(float Now) const;
    UPROPERTY(Replicated, BlueprintReadOnly) float ChargeStartTime = -100;
    UPROPERTY(Replicated, BlueprintReadOnly) float ChargePower = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float ReleasedChargeDuration = 0;
    UPROPERTY(Replicated, BlueprintReadOnly) float SwingPower = 1.0f;
    void ResetAttack();
    bool StartThrow(float Now);
    UPROPERTY(Replicated, BlueprintReadOnly) float ThrowTime=-100;
    UPROPERTY(Replicated, BlueprintReadOnly) float ThrowReadyTime=0;
    UPROPERTY(Replicated, BlueprintReadOnly) int32 ThrowSequence=0;
    bool IsThrowing(float Now) const {return Now-ThrowTime>=0 && Now-ThrowTime<.85f;}
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly) void SetVariant(int32 Index);
private:
    UFUNCTION(NetMulticast, Unreliable) void MulticastSwingAudio(bool bResonant);
    UFUNCTION(NetMulticast, Unreliable) void MulticastHitAudio(FVector Location, bool bResonant);
    UPROPERTY() TObjectPtr<USoundBase> SwingSound;
    UPROPERTY() TObjectPtr<USoundBase> HitSound;
    UPROPERTY() TObjectPtr<USoundBase> ResonanceSound;
    UPROPERTY() TObjectPtr<UPoseableMeshComponent> BobbleVisual;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> BackPillow;
    FVector ThrowHand(float Age) const;
    bool bThrowReleased=true;
    UPROPERTY() TArray<TObjectPtr<USkeletalMesh>> Variants;
    UPROPERTY(Replicated) TArray<FPillowImpact> Impacts;
    int32 LoadedVariant = -1;
    float LastTraceTime = -100;
    FTransform PreviousTraceWorld = FTransform::Identity;
    TSet<TWeakObjectPtr<ACharacter>> HitActors;
    TSet<TWeakObjectPtr<class APillowWarsCover>> HitCovers;
    FTransform PillowPose(float Age, bool bDeform = false) const;
    FVector PillowPoint(float Age, const FVector& Point) const;
    FQuat FlexRotation(float Age) const;
    float ReleaseAt(float Age) const;
    FTransform VisualWorld() const;
    void CheckContacts(float Now);
    float WalkPhase = 0;
    float GaitBlend = 0;
    float StuffingBlend = 0;
    FVector2D BalanceAngle = FVector2D::ZeroVector;
    FVector2D BalanceVelocity = FVector2D::ZeroVector;
    float LandingSquash = 0;
    float PreviousFallSpeed = 0;
    bool bWasAirborne = false;
    float HeadAngle = 0;
    float HeadVelocity = 0;
    float BobbleStrength = 0;
    float BobbleVisibility = 1;
    FVector PreviousVelocity = FVector::ZeroVector;
    FVector PreviousPillowCenter = FVector::ZeroVector;
    bool bLoggedRig = false;
};
