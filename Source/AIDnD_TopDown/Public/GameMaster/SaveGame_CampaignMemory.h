#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "GameMaster/GameMasterTypes.h"
#include "SaveGame_CampaignMemory.generated.h"

UCLASS()
class AIDND_TOPDOWN_API USaveGame_CampaignMemory : public USaveGame
{
    GENERATED_BODY()

public:

    UPROPERTY()
    FGMCampaignState CampaignState;

    UPROPERTY()
    TArray<FString> NarrationHistory;

    static const int32   MaxNarrationHistory = 20;
    static const FString SaveSlotName;
    static const int32   UserIndex;

    void AddNarration(const FString& Text)
    {
        NarrationHistory.Add(Text);
        TrimHistory();
    }

    void AddWorldFact(const FString& Fact)
    {
        if (!Fact.IsEmpty() && !CampaignState.WorldFacts.Contains(Fact))
        {
            CampaignState.WorldFacts.Add(Fact);
        }
    }

    void UpdateSummary(const FString& NewSummary)
    {
        CampaignState.NarrativeSummary = NewSummary;
    }

    void TrimHistory()
    {
        while (NarrationHistory.Num() > MaxNarrationHistory)
        {
            NarrationHistory.RemoveAt(0);
        }
        while (CampaignState.RecentEventLog.Num() > 30)
        {
            CampaignState.RecentEventLog.RemoveAt(0);
        }
    }

    FString BuildGMContextString() const
    {
        FString Context;
        if (!CampaignState.NarrativeSummary.IsEmpty())
        {
            Context += TEXT("[Campaign Summary]: ") + CampaignState.NarrativeSummary + TEXT("\n\n");
        }

        if (CampaignState.WorldFacts.Num() > 0)
        {
            Context += TEXT("[Known Facts]:\n");
            for (const FString& Fact : CampaignState.WorldFacts)
            {
                Context += TEXT("- ") + Fact + TEXT("\n");
            }
            Context += TEXT("\n");
        }

        if (CampaignState.ActiveQuests.Num() > 0)
        {
            Context += TEXT("[Active Quests]:\n");
            for (const FGMQuest& Quest : CampaignState.ActiveQuests)
            {
                Context += FString::Printf(TEXT("- %s: %s\n"), *Quest.Title, *Quest.Description);
            }
            Context += TEXT("\n");
        }

        if (CampaignState.RecentEventLog.Num() > 0)
        {
            Context += TEXT("[Recent Events]:\n");
            int32 StartIdx = FMath::Max(0, CampaignState.RecentEventLog.Num() - 10);
            for (int32 i = StartIdx; i < CampaignState.RecentEventLog.Num(); ++i)
            {
                Context += TEXT("- ") + CampaignState.RecentEventLog[i] + TEXT("\n");
            }
        }
        return Context;
    }
};
