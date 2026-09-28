#include "PillowWarsEditorTestLibrary.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#if WITH_EDITOR
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
namespace { bool Saved=false, OneProcess=true; int32 SavedPlayerCount=1,Width=1280,Height=720; EPlayNetMode Mode=PIE_Standalone; }
#endif
void UPillowWarsEditorTestLibrary::ConfigurePIE(int32 Players)
{
#if WITH_EDITOR
    auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    if(!Saved)
    {
        Settings->GetPlayNetMode(Mode); Settings->GetPlayNumberOfClients(SavedPlayerCount); Settings->GetRunUnderOneProcess(OneProcess); Width=Settings->NewWindowWidth; Height=Settings->NewWindowHeight; Saved=true;
    }
    Settings->SetPlayNumberOfClients(FMath::Clamp(Players,1,10));
    Settings->SetPlayNetMode(Players>1?PIE_ListenServer:PIE_Standalone);
    Settings->SetRunUnderOneProcess(true);
    if(Players>2){ Settings->NewWindowWidth=640; Settings->NewWindowHeight=360; }
#endif
}
void UPillowWarsEditorTestLibrary::RestorePIE()
{
#if WITH_EDITOR
    if(!Saved)return;
    auto* Settings=GetMutableDefault<ULevelEditorPlaySettings>();
    Settings->SetPlayNumberOfClients(SavedPlayerCount); Settings->SetPlayNetMode(Mode); Settings->SetRunUnderOneProcess(OneProcess); Settings->NewWindowWidth=Width; Settings->NewWindowHeight=Height; Saved=false;
#endif
}

void UPillowWarsEditorTestLibrary::QueuePlayerKey(APlayerController* Player,FName Key,float HoldSeconds)
{
#if WITH_EDITOR
    if(!IsValid(Player)||!Player->IsLocalController()) return;
    // Execute during the game world's tick, not editor Python's forced-local RPC scope.
    TWeakObjectPtr<APlayerController> WeakPlayer(Player);
    Player->GetWorld()->GetTimerManager().SetTimerForNextTick([WeakPlayer,Key,HoldSeconds]()
    {
        if(!WeakPlayer.IsValid())return;
        WeakPlayer->InputKey(FInputKeyParams(FKey(Key),IE_Pressed,1.0));
        if(HoldSeconds<=0)
        {
            WeakPlayer->InputKey(FInputKeyParams(FKey(Key),IE_Released,0.0));
            if(HoldSeconds<0)
            {
                WeakPlayer->InputKey(FInputKeyParams(FKey(Key),IE_Pressed,1.0));
                WeakPlayer->InputKey(FInputKeyParams(FKey(Key),IE_Released,0.0));
            }
            return;
        }
        FTimerHandle Release;
        WeakPlayer->GetWorld()->GetTimerManager().SetTimer(Release,[WeakPlayer,Key]()
        {
            if(WeakPlayer.IsValid())WeakPlayer->InputKey(FInputKeyParams(FKey(Key),IE_Released,0.0));
        },FMath::Max(HoldSeconds,0.05f),false);
    });
#endif
}
float UPillowWarsEditorTestLibrary::ProbeSpawn(UObject* Context,FVector Location)
{
#if WITH_EDITOR
    UWorld* World=Context?Context->GetWorld():nullptr; if(!World)return -1;
    FCollisionQueryParams Query;
    if(World->OverlapBlockingTestByChannel(Location,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(35,90),Query))return -1;
    FHitResult Hit;
    if(!World->LineTraceSingleByChannel(Hit,Location,Location-FVector(0,0,300),ECC_Visibility,Query)||Hit.Normal.Z<.9f)return -2;
    return Hit.Distance;
#else
    return -1;
#endif
}
FString UPillowWarsEditorTestLibrary::GuardLocalInput(UObject* BlueprintObject,bool Apply)
{
#if WITH_EDITOR
    auto* BP=Cast<UBlueprint>(BlueprintObject); if(!BP)return TEXT("ERROR not a Blueprint");
    if(Apply && !BP->GetPathName().StartsWith(TEXT("/Game/PillowWars/")))return TEXT("ERROR protected template path");
    FString Report;
    for(UEdGraph* Graph:BP->UbergraphPages)
    {
        const auto Nodes=Graph->Nodes;
        for(UEdGraphNode* Node:Nodes)
        {
            auto* Call=Cast<UK2Node_CallFunction>(Node); if(!Call)continue;
            Report+=Call->FunctionReference.GetMemberName().ToString()+TEXT("\n");
            if(Call->FunctionReference.GetMemberName()!=TEXT("AddMappingContext"))continue;
            UEdGraphPin* Exec=Call->FindPin(UEdGraphSchema_K2::PN_Execute);
            UEdGraphPin* Then=Call->FindPin(UEdGraphSchema_K2::PN_Then);
            if(!Exec || Exec->LinkedTo.Num()!=1 || (Then && Then->LinkedTo.Num()))return Report+TEXT("ERROR unexpected mapping execution graph");
            Report+=TEXT("Mapping has one incoming exec and no downstream exec.\n");
            if(!Apply)continue;
            Graph->Modify(); BP->Modify();
            auto* Local=NewObject<UK2Node_CallFunction>(Graph);
            Local->SetFromFunction(APawn::StaticClass()->FindFunctionByName(TEXT("IsLocallyControlled")));
            Graph->AddNode(Local,true,false); Local->CreateNewGuid(); Local->AllocateDefaultPins(); Local->NodePosX=Call->NodePosX-350; Local->NodePosY=Call->NodePosY+180;
            auto* Branch=NewObject<UK2Node_IfThenElse>(Graph); Graph->AddNode(Branch,true,false); Branch->CreateNewGuid(); Branch->AllocateDefaultPins(); Branch->NodePosX=Call->NodePosX-250; Branch->NodePosY=Call->NodePosY;
            auto* Schema=GetDefault<UEdGraphSchema_K2>(); UEdGraphPin* Previous=Exec->LinkedTo[0]; Previous->BreakLinkTo(Exec);
            const bool OK=Schema->TryCreateConnection(Previous,Branch->GetExecPin()) && Schema->TryCreateConnection(Local->GetReturnValuePin(),Branch->GetConditionPin()) && Schema->TryCreateConnection(Branch->GetThenPin(),Exec);
            if(!OK)return Report+TEXT("ERROR guard connections failed; do not save");
            FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
            if(BP->Status==BS_Error)return Report+TEXT("ERROR guarded Blueprint failed compilation");
            Report+=TEXT("GUARD_COMPILED\n");
        }
    }
    return Report;
#else
    return TEXT("Editor only");
#endif
}
