// Source/AIDnD_TopDown/Public/NPC/QuestDataAsset.h
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/NPC/NPCTypes.h"
#include "UQuestDataAsset.generated.h"

UCLASS(BlueprintType)
class AIDND_TOPDOWN_API UQuestDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Quest"), GetFName());
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Identity")
	FString QuestID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Identity")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Identity",
			  meta=(MultiLine=true))
	FText Description;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Requirements",
			  meta=(ClampMin=-100, ClampMax=100))
	int32 RequiredTrust = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Requirements")
	bool bIsRepeatable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Steps")
	TArray<FNPCQuestStep> Steps;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Rewards",
			  meta=(ClampMin=0, ClampMax=100))
	int32 TrustRewardOnComplete = 10;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Rewards",
			  meta=(ClampMin=-100, ClampMax=0))
	int32 TrustPenaltyOnFail = -5;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Rewards",
			  meta=(ClampMin=0))
	int32 GoldReward = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Quest|Narrative",
			  meta=(MultiLine=true))
	FString QuestGivePromptHint;
};