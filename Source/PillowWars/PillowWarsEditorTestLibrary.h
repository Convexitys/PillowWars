#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PillowWarsEditorTestLibrary.generated.h"

// Editor test setup only. Does not change saved user preferences or shipping gameplay.
UCLASS()
class PILLOWWARS_API UPillowWarsEditorTestLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) static void ConfigurePIE(int32 Players);
    UFUNCTION(BlueprintCallable) static void RestorePIE();
    UFUNCTION(BlueprintCallable) static float ProbeSpawn(UObject* Context,FVector Location);
    UFUNCTION(BlueprintCallable) static FString GuardLocalInput(UObject* BlueprintObject,bool Apply=false);
    UFUNCTION(BlueprintCallable) static void QueuePlayerKey(APlayerController* Player, FName Key, float HoldSeconds=0.1f);
    UFUNCTION(BlueprintCallable) static void ResourceFixture(APlayerController* Player,float Stuffing,bool DisableAmbient=false);
    UFUNCTION(BlueprintCallable) static void ResourceCallback(AActor* Object,FName Reason);
};
