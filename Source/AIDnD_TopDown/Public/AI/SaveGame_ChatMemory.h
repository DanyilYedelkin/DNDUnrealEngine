#pragma once
#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ChatTypes.h"
#include "SaveGame_ChatMemory.generated.h"

UCLASS()
class AIDND_TOPDOWN_API USaveGame_ChatMemory : public USaveGame
{
	GENERATED_BODY()
public:
	// Key — NPCID (string), value — NPC memory
	UPROPERTY()
	TMap<FString, FNPCMemory> NPCMemories;

	static const FString SaveSlotName;
	static const int32 UserIndex;
};