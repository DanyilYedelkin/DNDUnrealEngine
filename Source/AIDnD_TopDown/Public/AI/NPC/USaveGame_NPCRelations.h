// Source/AIDnD_TopDown/Public/NPC/SaveGame_NPCRelations.h
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AI/NPC/NPCTypes.h"
#include "USaveGame_NPCRelations.generated.h"

UCLASS()
class AIDND_TOPDOWN_API USaveGame_NPCRelations : public USaveGame
{
	GENERATED_BODY()
public:

	// NPCID -> Relation Data
	UPROPERTY()
	TMap<FString, FNPCRelationData> Relations;

	static const FString SaveSlotName;
	static const int32   UserIndex;
};