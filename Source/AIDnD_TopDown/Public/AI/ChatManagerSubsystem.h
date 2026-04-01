#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "ChatTypes.h"
#include "ChatManagerSubsystem.generated.h"

class USaveGame_ChatMemory;
class UOpenAIChatService;

// Delegate — visible in Blueprint
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnNPCResponseReceived,
    const FString&, NPCID, const FString&, ResponseText);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNPCResponseError,
    const FString&, ErrorMessage);

UCLASS()
class AIDND_TOPDOWN_API UChatManagerSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // --- Public API (accessible from BP) ---

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void SendMessage(const FString& NPCID, const FString& PlayerText,
                     const FString& SystemPrompt);

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    FNPCMemory& GetOrCreateMemory(const FString& NPCID, const FString& SystemPrompt);

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void AddMessageToMemory(const FString& NPCID, EChatRole Role, const FString& Content);

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void SaveMemory();

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void LoadMemory();

    UFUNCTION(BlueprintPure, Category="NPC Chat")
    TArray<FChatMessage> GetRecentMessages(const FString& NPCID) const;

    // Delegates — signing from BP or C++
    UPROPERTY(BlueprintAssignable, Category="NPC Chat")
    FOnNPCResponseReceived OnNPCResponseReceived;

    UPROPERTY(BlueprintAssignable, Category="NPC Chat")
    FOnNPCResponseError OnNPCResponseError;

    // Memory settings
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="NPC Chat|Memory")
    int32 MaxRecentMessages = 20;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category="NPC Chat|Memory")
    int32 SummarizationThreshold = 30;

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void SendMessageWithContext(const FString& NPCID,
                                const FString& PlayerText,
                                const FString& BaseSystemPrompt,
                                const FString& AdditionalContext);

private:
    UPROPERTY()
    USaveGame_ChatMemory* SaveGameData = nullptr;

    UPROPERTY()
    UOpenAIChatService* OpenAIService = nullptr;

    void TrimMemoryIfNeeded(FNPCMemory& Memory);
    void RequestSummaryUpdate(const FString& NPCID);
    TArray<FChatMessage> BuildContextMessages(const FNPCMemory& Memory) const;
};