// Source/AIDnD_TopDown/Public/NPC/NPCQuestComponent.h
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/NPC/NPCTypes.h"
#include "UNPCQuestComponent.generated.h"

class UQuestDataAsset;
class UTrustComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnQuestStateChanged,
    const FString&, QuestID, ENPCQuestState, NewState);

UCLASS(ClassGroup="NPC", meta=(BlueprintSpawnableComponent))
class AIDND_TOPDOWN_API UNPCQuestComponent : public UActorComponent
{
    GENERATED_BODY()
public:

    UNPCQuestComponent();
    virtual void BeginPlay() override;

    // ============================================================
    //  CONFIG
    // ============================================================
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest")
    TArray<TSoftObjectPtr<UQuestDataAsset>> AvailableQuests;

    // ============================================================
    //  PUBLIC API
    // ============================================================
    
    UFUNCTION(BlueprintCallable, Category="Quest")
    TArray<UQuestDataAsset*> GetOfferedQuests() const;
    
    UFUNCTION(BlueprintCallable, Category="Quest")
    bool AcceptQuest(const FString& QuestID, FString& OutFailReason);
    
    UFUNCTION(BlueprintCallable, Category="Quest")
    void CompleteQuestStep(const FString& QuestID, const FString& StepID);
    
    UFUNCTION(BlueprintCallable, Category="Quest")
    bool TurnInQuest(const FString& QuestID, FString& OutFailReason);
    
    UFUNCTION(BlueprintCallable, Category="Quest")
    void FailQuest(const FString& QuestID);

    UFUNCTION(BlueprintPure, Category="Quest")
    ENPCQuestState GetQuestState(const FString& QuestID) const;

    UFUNCTION(BlueprintPure, Category="Quest")
    bool HasActiveQuests() const;
    
    UFUNCTION(BlueprintPure, Category="Quest")
    FString BuildQuestContextString() const;

    // ============================================================
    //  DELEGATES
    // ============================================================

    UPROPERTY(BlueprintAssignable, Category="Quest|Events")
    FOnQuestStateChanged OnQuestStateChanged;

private:

    FString OwnerNPCID;

    UPROPERTY()
    class UNPCRelationsSubsystem* RelationsSubsystem = nullptr;

    UTrustComponent* GetTrustComp() const;
    UQuestDataAsset* ResolveQuest(const TSoftObjectPtr<UQuestDataAsset>& Ref) const;
    bool AreAllStepsComplete(const UQuestDataAsset* Quest) const;
};