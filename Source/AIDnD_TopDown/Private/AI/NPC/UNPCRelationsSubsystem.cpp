#include "AI/NPC/UNPCRelationsSubsystem.h"
#include "AI/NPC/USaveGame_NPCRelations.h"
#include "Kismet/GameplayStatics.h"

static constexpr int32 QUEST_STEP_INCOMPLETE = 0;
static constexpr int32 QUEST_STEP_COMPLETE   = 1;

void UNPCRelationsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Load();
}

void UNPCRelationsSubsystem::Deinitialize()
{
    Save();
    Super::Deinitialize();
}

void UNPCRelationsSubsystem::Load()
{
    if (UGameplayStatics::DoesSaveGameExist(
        USaveGame_NPCRelations::SaveSlotName,
        USaveGame_NPCRelations::UserIndex))
    {
        SaveData = Cast<USaveGame_NPCRelations>(
            UGameplayStatics::LoadGameFromSlot(
                USaveGame_NPCRelations::SaveSlotName,
                USaveGame_NPCRelations::UserIndex));
    }
    if (!SaveData)
    {
        SaveData = Cast<USaveGame_NPCRelations>(
            UGameplayStatics::CreateSaveGameObject(
                USaveGame_NPCRelations::StaticClass()));
    }
}

void UNPCRelationsSubsystem::Save()
{
    if (SaveData)
    {
        UGameplayStatics::SaveGameToSlot(SaveData,
            USaveGame_NPCRelations::SaveSlotName,
            USaveGame_NPCRelations::UserIndex);
    }
}

void UNPCRelationsSubsystem::EnsureRelationExists(const FString& NPCID,
                                                    int32 DefaultTrust)
{
    GetOrCreate(NPCID, DefaultTrust);
}

void UNPCRelationsSubsystem::ModifyTrust(const FString& NPCID, int32 Delta,
                                          const FNPCMemoryEvent& Event,
                                          int32 MaxEvents)
{
    if (!SaveData) return;
    FNPCRelationData& Rel = GetOrCreate(NPCID);

    Rel.TrustScore = FMath::Clamp(Rel.TrustScore + Delta, -100, 100);
    Rel.MemoryEvents.Add(Event);

    while (Rel.MemoryEvents.Num() > MaxEvents)
        Rel.MemoryEvents.RemoveAt(0);

    Save();
}

int32 UNPCRelationsSubsystem::GetTrust(const FString& NPCID) const
{
    if (!SaveData) return 0;
    if (const FNPCRelationData* Rel = SaveData->Relations.Find(NPCID))
        return Rel->TrustScore;
    return 0;
}

TArray<FNPCMemoryEvent> UNPCRelationsSubsystem::GetMemoryEvents(
    const FString& NPCID) const
{
    if (!SaveData) return {};
    if (const FNPCRelationData* Rel = SaveData->Relations.Find(NPCID))
        return Rel->MemoryEvents;
    return {};
}

void UNPCRelationsSubsystem::SetQuestState(const FString& NPCID,
                                             const FString& QuestID,
                                             ENPCQuestState State)
{
    if (!SaveData) return;
    FNPCRelationData& Rel = GetOrCreate(NPCID);
    Rel.QuestStates.Add(QuestID, static_cast<int32>(State));
    Save();
}

ENPCQuestState UNPCRelationsSubsystem::GetQuestState(const FString& NPCID,
                                                       const FString& QuestID) const
{
    if (!SaveData) return ENPCQuestState::Available;
    const FNPCRelationData* Rel = SaveData->Relations.Find(NPCID);
    if (!Rel) return ENPCQuestState::Available;

    const int32* StateVal = Rel->QuestStates.Find(QuestID);
    if (!StateVal) return ENPCQuestState::Available;

    return static_cast<ENPCQuestState>(*StateVal);
}

void UNPCRelationsSubsystem::MarkQuestStepComplete(const FString& NPCID,
                                                     const FString& QuestID,
                                                     const FString& StepID)
{
    if (!SaveData) return;
    FNPCRelationData& Rel = GetOrCreate(NPCID);
    Rel.QuestStates.Add(MakeStepKey(QuestID, StepID), QUEST_STEP_COMPLETE);
    Save();
}

bool UNPCRelationsSubsystem::IsQuestStepComplete(const FString& NPCID,
                                                   const FString& QuestID,
                                                   const FString& StepID) const
{
    if (!SaveData) return false;
    const FNPCRelationData* Rel = SaveData->Relations.Find(NPCID);
    if (!Rel) return false;

    const int32* Val = Rel->QuestStates.Find(MakeStepKey(QuestID, StepID));
    return Val && *Val == QUEST_STEP_COMPLETE;
}

void UNPCRelationsSubsystem::RecordPurchase(const FString& NPCID,
                                             const FString& ItemID)
{
    if (!SaveData) return;
    FNPCRelationData& Rel = GetOrCreate(NPCID);
    int32& Count = Rel.PurchasedItems.FindOrAdd(ItemID, 0);
    Count++;
    Save();
}

bool UNPCRelationsSubsystem::WasItemPurchased(const FString& NPCID,
                                               const FString& ItemID) const
{
    if (!SaveData) return false;
    const FNPCRelationData* Rel = SaveData->Relations.Find(NPCID);
    if (!Rel) return false;
    const int32* Count = Rel->PurchasedItems.Find(ItemID);
    return Count && *Count > 0;
}

FNPCRelationData& UNPCRelationsSubsystem::GetOrCreate(const FString& NPCID,
                                                        int32 DefaultTrust)
{
    if (!SaveData->Relations.Contains(NPCID))
    {
        FNPCRelationData NewRel;
        NewRel.NPCID      = NPCID;
        NewRel.TrustScore = DefaultTrust;
        SaveData->Relations.Add(NPCID, NewRel);
    }
    return SaveData->Relations[NPCID];
}