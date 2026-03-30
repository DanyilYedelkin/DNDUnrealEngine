#pragma once
#include "CoreMinimal.h"
#include "ChatTypes.generated.h"

UENUM(BlueprintType)
enum class EChatRole : uint8
{
	System   UMETA(DisplayName = "System"),
	User     UMETA(DisplayName = "User"),
	Assistant UMETA(DisplayName = "Assistant")
};

USTRUCT(BlueprintType)
struct FChatMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EChatRole Role = EChatRole::User;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString Content;

	UPROPERTY(BlueprintReadWrite)
	FDateTime Timestamp;

	FChatMessage() : Timestamp(FDateTime::UtcNow()) {}
	FChatMessage(EChatRole InRole, const FString& InContent)
		: Role(InRole), Content(InContent), Timestamp(FDateTime::UtcNow()) {}
};

USTRUCT(BlueprintType)
struct FNPCMemory
{
	GENERATED_BODY()

	// Unique NPC ID (set in Details)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString NPCID;

	// Character system prompt
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString SystemPrompt;

	// Periodically updated summary of past conversations
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	FString ConversationSummary;

	// Last N messages (scrolling window)
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<FChatMessage> RecentMessages;

	// How many messages were there in total (for statistics)?
	UPROPERTY(BlueprintReadWrite)
	int32 TotalMessageCount = 0;
};