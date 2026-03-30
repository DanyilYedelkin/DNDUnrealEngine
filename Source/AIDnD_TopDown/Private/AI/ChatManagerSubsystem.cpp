#include "AI/ChatManagerSubsystem.h"
#include "AI/SaveGame_ChatMemory.h"
#include "AI/OpenAIChatService.h"
#include "Kismet/GameplayStatics.h"

void UChatManagerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadMemory();
    OpenAIService = NewObject<UOpenAIChatService>(this);
    OpenAIService->Initialize();
}

void UChatManagerSubsystem::Deinitialize()
{
    SaveMemory();
    Super::Deinitialize();
}

void UChatManagerSubsystem::LoadMemory()
{
    if (UGameplayStatics::DoesSaveGameExist(USaveGame_ChatMemory::SaveSlotName,
                                             USaveGame_ChatMemory::UserIndex))
    {
        SaveGameData = Cast<USaveGame_ChatMemory>(
            UGameplayStatics::LoadGameFromSlot(USaveGame_ChatMemory::SaveSlotName,
                                               USaveGame_ChatMemory::UserIndex));
    }
    if (!SaveGameData)
    {
        SaveGameData = Cast<USaveGame_ChatMemory>(
            UGameplayStatics::CreateSaveGameObject(USaveGame_ChatMemory::StaticClass()));
    }
}

void UChatManagerSubsystem::SaveMemory()
{
    if (SaveGameData)
    {
        UGameplayStatics::SaveGameToSlot(SaveGameData,
            USaveGame_ChatMemory::SaveSlotName,
            USaveGame_ChatMemory::UserIndex);
    }
}

FNPCMemory& UChatManagerSubsystem::GetOrCreateMemory(const FString& NPCID,
                                                      const FString& SystemPrompt)
{
    if (!SaveGameData->NPCMemories.Contains(NPCID))
    {
        FNPCMemory NewMemory;
        NewMemory.NPCID = NPCID;
        NewMemory.SystemPrompt = SystemPrompt;
        SaveGameData->NPCMemories.Add(NPCID, NewMemory);
    }
    return SaveGameData->NPCMemories[NPCID];
}

void UChatManagerSubsystem::AddMessageToMemory(const FString& NPCID,
                                                EChatRole Role,
                                                const FString& Content)
{
    if (!SaveGameData) return;
    FNPCMemory& Memory = GetOrCreateMemory(NPCID, TEXT(""));
    Memory.RecentMessages.Add(FChatMessage(Role, Content));
    Memory.TotalMessageCount++;
    TrimMemoryIfNeeded(Memory);
    // Автосохранение после каждого ответа NPC
    if (Role == EChatRole::Assistant)
    {
        SaveMemory();
    }
}

void UChatManagerSubsystem::TrimMemoryIfNeeded(FNPCMemory& Memory)
{
    if (Memory.RecentMessages.Num() >= SummarizationThreshold)
    {
        RequestSummaryUpdate(Memory.NPCID);
    }
    // Держим только последние MaxRecentMessages
    while (Memory.RecentMessages.Num() > MaxRecentMessages)
    {
        Memory.RecentMessages.RemoveAt(0);
    }
}

TArray<FChatMessage> UChatManagerSubsystem::BuildContextMessages(
    const FNPCMemory& Memory) const
{
    TArray<FChatMessage> Context;

    // 1. System prompt
    FString FullSystem = Memory.SystemPrompt;
    if (!Memory.ConversationSummary.IsEmpty())
    {
        FullSystem += TEXT("\n\n[Previous conversation summary]: ") + Memory.ConversationSummary;
    }
    Context.Add(FChatMessage(EChatRole::System, FullSystem));

    // 2. Recent messages (sliding window)
    for (const FChatMessage& Msg : Memory.RecentMessages)
    {
        Context.Add(Msg);
    }
    return Context;
}

void UChatManagerSubsystem::SendMessage(const FString& NPCID,
                                         const FString& PlayerText,
                                         const FString& SystemPrompt)
{
    if (!OpenAIService || !SaveGameData) return;

    // Добавляем сообщение игрока в память
    AddMessageToMemory(NPCID, EChatRole::User, PlayerText);

    FNPCMemory& Memory = GetOrCreateMemory(NPCID, SystemPrompt);
    TArray<FChatMessage> Context = BuildContextMessages(Memory);

    // Лямбда-колбэк
    OpenAIService->SendChatRequest(Context,
    FOnChatSuccess::CreateLambda([this, NPCID](const FString& Response)
    {
        AddMessageToMemory(NPCID, EChatRole::Assistant, Response);
        OnNPCResponseReceived.Broadcast(NPCID, Response);
    }),
    FOnChatError::CreateLambda([this](const FString& Error)
    {
        OnNPCResponseError.Broadcast(Error);
    }));
}

TArray<FChatMessage> UChatManagerSubsystem::GetRecentMessages(const FString& NPCID) const
{
    if (!SaveGameData || !SaveGameData->NPCMemories.Contains(NPCID))
        return {};
    return SaveGameData->NPCMemories[NPCID].RecentMessages;
}

void UChatManagerSubsystem::RequestSummaryUpdate(const FString& NPCID)
{
    // Запрашиваем у OpenAI краткое саммари истории
    // Это асинхронный запрос, который обновит Memory.ConversationSummary
    if (!SaveGameData->NPCMemories.Contains(NPCID)) return;
    FNPCMemory& Memory = SaveGameData->NPCMemories[NPCID];

    FString SummaryPromptText = TEXT("Summarize the following conversation in 3-5 sentences, "
        "preserving key facts, player preferences, and important decisions:\n\n");

    for (const FChatMessage& Msg : Memory.RecentMessages)
    {
        FString RoleStr = (Msg.Role == EChatRole::User) ? TEXT("Player") : TEXT("NPC");
        SummaryPromptText += FString::Printf(TEXT("%s: %s\n"), *RoleStr, *Msg.Content);
    }

    TArray<FChatMessage> SummaryContext;
    SummaryContext.Add(FChatMessage(EChatRole::System,
        TEXT("You are a concise summarizer. Output only the summary, no preamble.")));
    SummaryContext.Add(FChatMessage(EChatRole::User, SummaryPromptText));

    OpenAIService->SendChatRequest(SummaryContext,
     FOnChatSuccess::CreateLambda([this, NPCID](const FString& SummaryResponse)
     {
         if (SaveGameData && SaveGameData->NPCMemories.Contains(NPCID))
         {
             SaveGameData->NPCMemories[NPCID].ConversationSummary = SummaryResponse;
             TArray<FChatMessage>& Msgs = SaveGameData->NPCMemories[NPCID].RecentMessages;
             if (Msgs.Num() > 5)
                 Msgs.RemoveAt(0, Msgs.Num() - 5);
             SaveMemory();
         }
     }),
     FOnChatError::CreateLambda([](const FString&)
     {
         /* summary failure is non-critical */
     }));
}