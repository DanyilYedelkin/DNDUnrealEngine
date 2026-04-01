// Source/AIDnD_TopDown/Public/NPC/NPCRelationsSubsystem.h
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "AI/NPC/NPCTypes.h"
#include "UNPCRelationsSubsystem.generated.h"

class USaveGame_NPCRelations;

UCLASS()
class AIDND_TOPDOWN_API UNPCRelationsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ============================================================
    //  TRUST
    // ============================================================

    UFUNCTION(BlueprintCallable, Category="NPC Relations")
    void EnsureRelationExists(const FString& NPCID, int32 DefaultTrust = 0);

    UFUNCTION(BlueprintCallable, Category="NPC Relations")
    void ModifyTrust(const FString& NPCID, int32 Delta,
                     const FNPCMemoryEvent& Event, int32 MaxEvents = 20);

    UFUNCTION(BlueprintPure, Category="NPC Relations")
    int32 GetTrust(const FString& NPCID) const;

    UFUNCTION(BlueprintPure, Category="NPC Relations")
    TArray<FNPCMemoryEvent> GetMemoryEvents(const FString& NPCID) const;

    // ============================================================
    //  QUESTS
    // ============================================================

    UFUNCTION(BlueprintCallable, Category="NPC Relations")
    void SetQuestState(const FString& NPCID, const FString& QuestID,
                       ENPCQuestState State);

    UFUNCTION(BlueprintPure, Category="NPC Relations")
    ENPCQuestState GetQuestState(const FString& NPCID,
                                  const FString& QuestID) const;

    UFUNCTION(BlueprintCallable, Category="NPC Relations")
    void MarkQuestStepComplete(const FString& NPCID, const FString& QuestID,
                                const FString& StepID);

    UFUNCTION(BlueprintPure, Category="NPC Relations")
    bool IsQuestStepComplete(const FString& NPCID, const FString& QuestID,
                              const FString& StepID) const;

    // ============================================================
    //  TRADE
    // ============================================================

    UFUNCTION(BlueprintCallable, Category="NPC Relations")
    void RecordPurchase(const FString& NPCID, const FString& ItemID);

    UFUNCTION(BlueprintPure, Category="NPC Relations")
    bool WasItemPurchased(const FString& NPCID, const FString& ItemID) const;

    // ============================================================
    //  PERSISTENCE
    // ============================================================

    UFUNCTION(BlueprintCallable, Category="NPC Relations")
    void Save();

    UFUNCTION(BlueprintCallable, Category="NPC Relations")
    void Load();

private:

    UPROPERTY()
    USaveGame_NPCRelations* SaveData = nullptr;
    
    static FString MakeStepKey(const FString& QuestID, const FString& StepID)
    {
        return QuestID + TEXT("::") + StepID;
    }

    FNPCRelationData& GetOrCreate(const FString& NPCID, int32 DefaultTrust = 0);
};