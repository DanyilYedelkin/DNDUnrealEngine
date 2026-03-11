// GameMasterSubsystem.cpp
#include "GameMaster/GameMasterSubsystem.h"
#include "GameMaster/WorldActionExecutor.h"
#include "GameMaster/WorldCatalogDataAsset.h"
#include "GameMaster/LevelGenerationSettingsDataAsset.h"
#include "GameMaster/SaveGame_CampaignMemory.h"
#include "AI/OpenAIChatService.h"    // существующий HTTP сервис
#include "AI/ChatTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameMaster, Log, All);

// ============================================================
//  LIFECYCLE
// ============================================================

void UGameMasterSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Создаём HTTP сервис (переиспользуем существующий UOpenAIChatService)
    OpenAIService = NewObject<UOpenAIChatService>(this);
    OpenAIService->Initialize();
    // GM может ждать дольше — повышаем cooldown
    OpenAIService->CooldownSeconds = RequestCooldown;

    // Создаём исполнитель действий
    ActionExecutor = NewObject<UWorldActionExecutor>(this);

    EnsureSaveDataExists();

    UE_LOG(LogGameMaster, Log, TEXT("UGameMasterSubsystem initialized."));
}

void UGameMasterSubsystem::Deinitialize()
{
    SaveCampaign();
    Super::Deinitialize();
}

// ============================================================
//  ГЕНЕРАЦИЯ УРОВНЯ
// ============================================================

void UGameMasterSubsystem::GenerateLevel(UWorld* World)
{
    if (!World)
    {
        UE_LOG(LogGameMaster, Error, TEXT("GenerateLevel: World is null!"));
        return;
    }
    if (bRequestInFlight)
    {
        UE_LOG(LogGameMaster, Warning, TEXT("GenerateLevel: Request already in flight"));
        return;
    }
    if (!LevelSettings)
    {
        OnGameMasterError.Broadcast(TEXT("LevelSettings DataAsset not assigned!"));
        return;
    }
    if (!WorldCatalog)
    {
        OnGameMasterError.Broadcast(TEXT("WorldCatalog DataAsset not assigned!"));
        return;
    }

    // Настраиваем executor
    ActionExecutor->Catalog  = WorldCatalog;
    ActionExecutor->Settings = LevelSettings;
    ActionExecutor->ResetCounters();

    // Строим сообщения для OpenAI
    TArray<FChatMessage> Messages;
    Messages.Add(FChatMessage(EChatRole::System, BuildGMSystemPrompt()));
    Messages.Add(FChatMessage(EChatRole::User,   BuildLevelGenerationPrompt()));

    UE_LOG(LogGameMaster, Log, TEXT("Sending level generation request to GM..."));

    // Сохраняем World для использования в callback (weak ptr через UWorld*)
    TWeakObjectPtr<UWorld> WeakWorld = World;

    SendGMRequest(
        Messages,
        LevelGenerationMaxTokens,
        // OnSuccess
        [this, WeakWorld](const FString& JsonResponse)
        {
            UWorld* W = WeakWorld.Get();
            if (!W) return;

            FGMLevelGenerationResult Result = ParseLevelGenerationResponse(JsonResponse);

            if (!Result.bSuccess)
            {
                UE_LOG(LogGameMaster, Error,
                    TEXT("Level generation parse failed: %s"), *Result.ErrorMessage);
                OnGameMasterError.Broadcast(Result.ErrorMessage);

                // Fallback: базовый нарратив без спавна
                Result.bSuccess = true;
                Result.OpeningNarration = TEXT("You find yourself in a mysterious place. Your adventure begins...");
            }

            // Применяем к кампании
            ApplyLevelResultToCampaign(Result);

            // Выполняем actions в мире
            TArray<FGMAction> ValidActions = ValidateActions(Result.Actions);
            ActionExecutor->ExecuteActions(ValidActions, W);

            // Уведомляем о нарративе
            if (!Result.OpeningNarration.IsEmpty())
            {
                OnNarrationReady.Broadcast(Result.OpeningNarration);
            }

            // Регистрируем квесты
            for (const FGMQuest& Quest : Result.InitialQuests)
            {
                AddQuestToCampaign(Quest);
                OnQuestUpdated.Broadcast(Quest);
            }

            bLevelGenerated = true;
            SaveCampaign();

            OnLevelGenerationComplete.Broadcast(Result);

            UE_LOG(LogGameMaster, Log,
                TEXT("Level generation complete. Narration: '%s'"),
                *Result.OpeningNarration.Left(100));
        },
        // OnError
        [this](const FString& Error)
        {
            UE_LOG(LogGameMaster, Error, TEXT("Level generation error: %s"), *Error);
            OnGameMasterError.Broadcast(Error);

            // Graceful fallback
            FGMLevelGenerationResult FallbackResult;
            FallbackResult.bSuccess = true;
            FallbackResult.OpeningNarration =
                TEXT("The Game Master is unavailable. Your adventure begins in silence...");
            OnLevelGenerationComplete.Broadcast(FallbackResult);
        }
    );
}

// ============================================================
//  CAMPAIGN EVENTS
// ============================================================

void UGameMasterSubsystem::SendCampaignEvent(const FGMCampaignEvent& Event,
                                               UWorld* World)
{
    if (bRequestInFlight)
    {
        UE_LOG(LogGameMaster, Verbose, TEXT("SendCampaignEvent: Skipped — request in flight"));
        return;
    }

    // Логируем событие в памяти
    EnsureSaveDataExists();
    SaveData->CampaignState.RecentEventLog.Add(Event.Description);
    SaveData->TrimHistory();
    SaveData->CampaignState.TurnCount++;

    TArray<FChatMessage> Messages;
    Messages.Add(FChatMessage(EChatRole::System, BuildGMSystemPrompt()));
    Messages.Add(FChatMessage(EChatRole::User,   BuildCampaignEventPrompt(Event)));

    TWeakObjectPtr<UWorld> WeakWorld = World;

    SendGMRequest(
        Messages,
        NarrativeMaxTokens,
        [this, WeakWorld](const FString& JsonResponse)
        {
            UWorld* W = WeakWorld.Get();

            FGMNarrativeResponse Response = ParseNarrativeResponse(JsonResponse);

            if (!Response.bSuccess)
            {
                // Soft fallback — хотя бы нарратив
                Response.bSuccess = true;
                Response.NarrationText = TEXT("The world reacts to your actions...");
            }

            ApplyNarrativeResponseToCampaign(Response);

            if (!Response.NarrationText.IsEmpty())
            {
                OnNarrationReady.Broadcast(Response.NarrationText);
            }

            // Выполняем дополнительные actions если есть
            if (W && Response.Actions.Num() > 0)
            {
                TArray<FGMAction> Valid = ValidateActions(Response.Actions);
                ActionExecutor->ExecuteActions(Valid, W);
            }

            // Обновляем квесты
            for (const FGMAction& Action : Response.Actions)
            {
                if (Action.ActionType == EGMActionType::SetQuest)
                {
                    AddQuestToCampaign(Action.Quest);
                    OnQuestUpdated.Broadcast(Action.Quest);
                }
            }

            SaveCampaign();
            OnNarrativeResponseReceived.Broadcast(Response);
        },
        [this](const FString& Error)
        {
            UE_LOG(LogGameMaster, Warning,
                TEXT("Campaign event error: %s"), *Error);
            // Не показываем ошибку игроку — просто тихий fallback
            FGMNarrativeResponse Fallback;
            Fallback.bSuccess = true;
            Fallback.NarrationText = TEXT("...");
            OnNarrativeResponseReceived.Broadcast(Fallback);
        }
    );
}

void UGameMasterSubsystem::SendSimpleEvent(const FString& EventDescription,
                                            UWorld* World)
{
    FGMCampaignEvent Event(EGMEventType::PlayerAction, EventDescription);
    SendCampaignEvent(Event, World);
}

// ============================================================
//  QUEST MANAGEMENT
// ============================================================

void UGameMasterSubsystem::CompleteQuest(const FString& QuestID)
{
    EnsureSaveDataExists();
    for (FGMQuest& Quest : SaveData->CampaignState.ActiveQuests)
    {
        if (Quest.QuestID == QuestID)
        {
            Quest.bActive = false;
            SaveData->CampaignState.CompletedQuests.Add(Quest);
            OnQuestUpdated.Broadcast(Quest);
            break;
        }
    }
    SaveData->CampaignState.ActiveQuests.RemoveAll(
        [](const FGMQuest& Q){ return !Q.bActive; });
    SaveCampaign();
}

void UGameMasterSubsystem::CompleteQuestStep(const FString& QuestID,
                                               const FString& StepID)
{
    EnsureSaveDataExists();
    for (FGMQuest& Quest : SaveData->CampaignState.ActiveQuests)
    {
        if (Quest.QuestID == QuestID)
        {
            for (FGMQuestStep& Step : Quest.Steps)
            {
                if (Step.StepID == StepID)
                {
                    Step.bCompleted = true;
                    OnQuestUpdated.Broadcast(Quest);
                    break;
                }
            }
        }
    }
    SaveCampaign();
}

FGMCampaignState UGameMasterSubsystem::GetCampaignState() const
{
    return SaveData ? SaveData->CampaignState : FGMCampaignState{};
}

TArray<FGMQuest> UGameMasterSubsystem::GetActiveQuests() const
{
    return SaveData ? SaveData->CampaignState.ActiveQuests : TArray<FGMQuest>{};
}

void UGameMasterSubsystem::ClearGeneratedWorld()
{
    if (ActionExecutor)
    {
        ActionExecutor->DestroyAllSpawnedActors();
    }
    bLevelGenerated = false;
}

// ============================================================
//  SAVE / LOAD
// ============================================================

void UGameMasterSubsystem::SaveCampaign()
{
    if (SaveData)
    {
        UGameplayStatics::SaveGameToSlot(SaveData,
            USaveGame_CampaignMemory::SaveSlotName,
            USaveGame_CampaignMemory::UserIndex);
    }
}

void UGameMasterSubsystem::LoadCampaign()
{
    if (UGameplayStatics::DoesSaveGameExist(
        USaveGame_CampaignMemory::SaveSlotName,
        USaveGame_CampaignMemory::UserIndex))
    {
        SaveData = Cast<USaveGame_CampaignMemory>(
            UGameplayStatics::LoadGameFromSlot(
                USaveGame_CampaignMemory::SaveSlotName,
                USaveGame_CampaignMemory::UserIndex));
    }
    EnsureSaveDataExists();
}

void UGameMasterSubsystem::EnsureSaveDataExists()
{
    if (!SaveData)
    {
        SaveData = Cast<USaveGame_CampaignMemory>(
            UGameplayStatics::CreateSaveGameObject(
                USaveGame_CampaignMemory::StaticClass()));
    }
}

// ============================================================
//  PROMPT BUILDING
// ============================================================

FString UGameMasterSubsystem::BuildGMSystemPrompt_Implementation() const
{
    FString Prompt;
    Prompt += TEXT("You are an expert Dungeon Master for a D&D 5e game running in Unreal Engine 5.\n");
    Prompt += TEXT("Your role is to create immersive worlds, compelling narratives, ");
    Prompt += TEXT("react to player actions, and manage the campaign.\n\n");

    Prompt += TEXT("CRITICAL: You MUST respond ONLY with valid JSON. No markdown, no explanation outside JSON.\n\n");

    // ---- Режим карты ----
    bool bIsDungeon = LevelSettings && LevelSettings->IsDungeon();

    if (bIsDungeon)
    {
        // Данжен: GM расставляет блоки одной стены с разным масштабом и поворотом
        Prompt += TEXT("MAP TYPE: DUNGEON (indoor)\n");
        Prompt += TEXT("You create dungeon rooms and corridors by placing WALL BLOCKS.\n");
        Prompt += TEXT("There is ONE wall prefab asset. You control its shape via 'scale' in the transform:\n");
        Prompt += TEXT("  - Long corridor wall: scale x=8, y=1, z=2\n");
        Prompt += TEXT("  - Short wall segment: scale x=2, y=1, z=2\n");
        Prompt += TEXT("  - Wide room wall:     scale x=12, y=1, z=2\n");
        Prompt += TEXT("  - Use yaw rotation (0=North, 90=East) to orient walls.\n");
        Prompt += TEXT("  - Place walls to form rooms and corridors. Leave gaps for doors/entrances.\n");
        Prompt += TEXT("  - Default scale (1,1,1) = one wall unit (~100x100x200 Unreal units).\n\n");
    }
    else
    {
        // Открытая местность: GM расставляет декоративные prefab'ы
        Prompt += TEXT("MAP TYPE: OPEN WORLD (outdoor)\n");
        Prompt += TEXT("You scatter decorative prefabs across the landscape:\n");
        Prompt += TEXT("  - Trees, rocks, bushes for natural terrain.\n");
        Prompt += TEXT("  - Houses and ruins as decoration (NOT enterable buildings).\n");
        Prompt += TEXT("  - Vary scale slightly (0.8-1.5) for natural variety.\n");
        Prompt += TEXT("  - Spread objects across the entire map area.\n");
        Prompt += TEXT("  - Leave open areas for combat and movement.\n\n");
    }

    // ---- JSON-схема ответа ----
    Prompt += TEXT("Respond with this exact JSON schema:\n");
    Prompt += TEXT("{\n");
    Prompt += TEXT("  \"level_metadata\": { \"theme\": \"...\", \"seed\": 42 },\n");
    Prompt += TEXT("  \"narration\": \"Opening scene description (2-4 sentences, immersive)\",\n");
    Prompt += TEXT("  \"actions\": [\n");

    if (bIsDungeon)
    {
        Prompt += TEXT("    { \"type\": \"SpawnActor\", \"assetId\": \"Wall_A\",\n");
        Prompt += TEXT("      \"transform\": {\"location\":{\"x\":0,\"y\":0,\"z\":0},\n");
        Prompt += TEXT("                     \"rotation\":{\"pitch\":0,\"yaw\":0,\"roll\":0},\n");
        Prompt += TEXT("                     \"scale\":{\"x\":8,\"y\":1,\"z\":2}} },\n");
    }
    else
    {
        Prompt += TEXT("    { \"type\": \"SpawnActor\", \"assetId\": \"Tree_A\",\n");
        Prompt += TEXT("      \"transform\": {\"location\":{\"x\":500,\"y\":300,\"z\":0},\n");
        Prompt += TEXT("                     \"rotation\":{\"pitch\":0,\"yaw\":45,\"roll\":0},\n");
        Prompt += TEXT("                     \"scale\":{\"x\":1.2,\"y\":1.2,\"z\":1.2}} },\n");
    }

    Prompt += TEXT("    { \"type\": \"SpawnNPC\",    \"assetId\": \"Merchant_01\",\n");
    Prompt += TEXT("      \"personaPrompt\": \"Describe the NPC personality and knowledge in 2-3 sentences.\",\n");
    Prompt += TEXT("      \"transform\": {\"location\":{\"x\":200,\"y\":100,\"z\":0},\n");
    Prompt += TEXT("                     \"rotation\":{\"pitch\":0,\"yaw\":180,\"roll\":0},\n");
    Prompt += TEXT("                     \"scale\":{\"x\":1,\"y\":1,\"z\":1}} },\n");
    Prompt += TEXT("    { \"type\": \"SpawnEnemy\",  \"assetId\": \"Goblin_A\", \"count\": 2,\n");
    Prompt += TEXT("      \"transform\": {\"location\":{\"x\":-500,\"y\":0,\"z\":0},\n");
    Prompt += TEXT("                     \"rotation\":{\"pitch\":0,\"yaw\":0,\"roll\":0},\n");
    Prompt += TEXT("                     \"scale\":{\"x\":1,\"y\":1,\"z\":1}} },\n");
    Prompt += TEXT("    { \"type\": \"SpawnManager\", \"assetId\": \"QuestManager\",\n");
    Prompt += TEXT("      \"transform\": {\"location\":{\"x\":0,\"y\":0,\"z\":0},\n");
    Prompt += TEXT("                     \"rotation\":{\"pitch\":0,\"yaw\":0,\"roll\":0},\n");
    Prompt += TEXT("                     \"scale\":{\"x\":1,\"y\":1,\"z\":1}} },\n");
    Prompt += TEXT("    { \"type\": \"SetQuest\", \"quest\": {\"questId\": \"q1\", \"title\": \"...\",\n");
    Prompt += TEXT("      \"description\": \"...\", \"steps\": [{\"stepId\":\"s1\",\"description\":\"...\"}]} }\n");
    Prompt += TEXT("  ],\n");
    Prompt += TEXT("  \"memory_update\": { \"summary\": \"1-2 sentences about this location\", \"facts\": [] }\n");
    Prompt += TEXT("}\n\n");

    // ---- Контекст кампании ----
    if (SaveData)
    {
        FString CampaignContext = SaveData->BuildGMContextString();
        if (!CampaignContext.IsEmpty())
        {
            Prompt += TEXT("[CAMPAIGN MEMORY]:\n") + CampaignContext + TEXT("\n");
        }
    }

    // ---- Каталог ----
    Prompt += TEXT("[AVAILABLE ASSET IDs — use ONLY these exact strings]:\n");
    Prompt += GetCatalogSummaryForPrompt();

    return Prompt;
}

FString UGameMasterSubsystem::BuildLevelGenerationPrompt_Implementation() const
{
    if (!LevelSettings)
    {
        return TEXT("Generate a small dungeon with 2 NPCs and 3 enemies. Return JSON only.");
    }

    FString Prompt;
    Prompt += FString::Printf(TEXT("Generate a %s D&D level named \"%s\".\n"),
        *LevelSettings->GetDifficultyString(),
        *LevelSettings->LevelName);

    Prompt += FString::Printf(TEXT("Atmosphere: %s\n"), *LevelSettings->Atmosphere);

    // Границы координат
    Prompt += FString::Printf(
        TEXT("World bounds (Unreal units): X[%.0f .. %.0f]  Y[%.0f .. %.0f]  Z[0 .. 400]\n"),
        LevelSettings->WorldMin.X, LevelSettings->WorldMax.X,
        LevelSettings->WorldMin.Y, LevelSettings->WorldMax.Y);

    // Лимиты
    Prompt += FString::Printf(
        TEXT("Spawn limits: max %d environment objects, max %d NPCs, max %d enemies.\n"),
        LevelSettings->MaxEnvironmentActors,
        LevelSettings->MaxNPCs,
        LevelSettings->MaxEnemies);

    // Инструкции по типу карты
    if (LevelSettings->IsDungeon())
    {
        Prompt += TEXT("\n--- DUNGEON LAYOUT INSTRUCTIONS ---\n");
        Prompt += TEXT("Use SpawnActor with assetId=\"Wall_A\" and SCALE to build the dungeon:\n");
        Prompt += TEXT("  * Create at least 2-3 connected rooms.\n");
        Prompt += TEXT("  * Each room: 4 walls forming a rectangle, leave gaps (200-300 units) for passages.\n");
        Prompt += TEXT("  * Corridor: 2 long parallel walls (scale x=6..10, y=1, z=2) facing each other.\n");
        Prompt += TEXT("  * Use yaw rotation: 0=wall faces South, 90=wall faces West, 180=North, 270=East.\n");
        Prompt += TEXT("  * Place player start area near (0,0,0). Spread enemies in different rooms.\n");
        Prompt += TEXT("  * NPCs should be in safe rooms away from enemies.\n");
        Prompt += TEXT("  * 1 unit scale = 100 Unreal units. Standard room = 10x10 scale units.\n");
        Prompt += TEXT("  * Total wall actors: aim for 20-40 pieces.\n\n");
    }
    else
    {
        Prompt += TEXT("\n--- OPEN WORLD LAYOUT INSTRUCTIONS ---\n");
        Prompt += TEXT("Scatter decorative prefabs naturally across the map:\n");
        Prompt += TEXT("  * Use Tree/Rock/Bush assets for terrain decoration — vary positions.\n");
        Prompt += TEXT("  * House assets are DECORATIONS only, no need to build interiors.\n");
        Prompt += TEXT("  * Spread objects: don't cluster everything at origin.\n");
        Prompt += TEXT("  * Vary scale between 0.7 and 1.5 for natural feel.\n");
        Prompt += TEXT("  * Leave open flat areas (300+ units) for combat.\n");
        Prompt += TEXT("  * Place NPCs near houses or points of interest.\n");
        Prompt += TEXT("  * Place enemies at the edges or in dangerous-looking areas.\n\n");
    }

    // Квест
    if (!LevelSettings->StartingQuestHint.IsEmpty())
    {
        Prompt += FString::Printf(TEXT("Starting quest theme: %s\n"),
            *LevelSettings->StartingQuestHint);
    }

    // Seed
    if (LevelSettings->Seed != 0)
    {
        Prompt += FString::Printf(TEXT("Use seed %d for layout consistency.\n"), LevelSettings->Seed);
    }

    // Обязательные менеджеры
    if (LevelSettings->RequiredManagerIDs.Num() > 0)
    {
        Prompt += TEXT("You MUST spawn these managers (use SpawnManager): ");
        for (const FString& ID : LevelSettings->RequiredManagerIDs)
            Prompt += ID + TEXT(", ");
        Prompt += TEXT("\n");
    }

    // Дополнительные инструкции от дизайнера
    if (!LevelSettings->ExtraGMInstructions.IsEmpty())
    {
        Prompt += TEXT("Extra instructions: ") + LevelSettings->ExtraGMInstructions + TEXT("\n");
    }

    Prompt += TEXT("\nIMPORTANT: Respond with ONLY valid JSON. No text before or after the JSON object.");

    return Prompt;
}

FString UGameMasterSubsystem::BuildCampaignEventPrompt_Implementation(
    const FGMCampaignEvent& Event) const
{
    FString Prompt;
    Prompt += FString::Printf(TEXT("PLAYER ACTION: %s\n\n"), *Event.Description);

    if (SaveData)
    {
        Prompt += TEXT("Current campaign state:\n");
        Prompt += SaveData->BuildGMContextString();
    }

    Prompt += TEXT("\nAs the Dungeon Master, respond to this player action.\n");
    Prompt += TEXT("Respond with JSON:\n");
    Prompt += TEXT("{\n");
    Prompt += TEXT("  \"narration\": \"What happens as a result\",\n");
    Prompt += TEXT("  \"actions\": [],\n");
    Prompt += TEXT("  \"memory_update\": { \"summary\": \"updated summary\", \"facts\": [] }\n");
    Prompt += TEXT("}\n");
    Prompt += TEXT("Keep narration vivid, immersive, under 200 words. ");
    Prompt += TEXT("Only add actions if something spawns or changes. Respond ONLY with JSON.");

    return Prompt;
}

// ============================================================
//  HTTP
// ============================================================

void UGameMasterSubsystem::SendGMRequest(
    const TArray<FChatMessage>& Messages,
    int32 MaxTokens,
    TFunction<void(const FString&)> OnSuccess,
    TFunction<void(const FString&)> OnError)
{
    if (!OpenAIService)
    {
        if (OnError) OnError(TEXT("OpenAIService not initialized"));
        return;
    }

    bRequestInFlight = true;

    // UOpenAIChatService использует делегаты, адаптируем к lambda
    OpenAIService->SendChatRequest(
        Messages,
        FOnChatSuccess::CreateLambda([this, OnSuccess](const FString& Response)
        {
            bRequestInFlight = false;
            FString CleanJson = ExtractJsonFromResponse(Response);
            if (OnSuccess) OnSuccess(CleanJson);
        }),
        FOnChatError::CreateLambda([this, OnError](const FString& Error)
        {
            bRequestInFlight = false;
            if (OnError) OnError(Error);
        })
    );
}

// ============================================================
//  JSON PARSING
// ============================================================

FGMLevelGenerationResult UGameMasterSubsystem::ParseLevelGenerationResponse(
    const FString& JsonString) const
{
    FGMLevelGenerationResult Result;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Result.ErrorMessage = FString::Printf(
            TEXT("Failed to parse JSON. Raw: %s"), *JsonString.Left(200));
        return Result;
    }

    // level_metadata
    if (const TSharedPtr<FJsonObject>* MetaObj = nullptr;
        Root->TryGetObjectField(TEXT("level_metadata"), MetaObj) && MetaObj)
    {
        (*MetaObj)->TryGetStringField(TEXT("theme"), Result.LevelTheme);
        (*MetaObj)->TryGetStringField(TEXT("biome"), Result.BiomeType);
        int32 Seed = 0;
        (*MetaObj)->TryGetNumberField(TEXT("seed"), Seed);
        Result.Seed = Seed;
    }

    // narration
    Root->TryGetStringField(TEXT("narration"), Result.OpeningNarration);

    // actions
    const TArray<TSharedPtr<FJsonValue>>* ActionsArray = nullptr;
    if (Root->TryGetArrayField(TEXT("actions"), ActionsArray) && ActionsArray)
    {
        for (const TSharedPtr<FJsonValue>& ActionVal : *ActionsArray)
        {
            if (ActionVal->Type == EJson::Object)
            {
                Result.Actions.Add(ParseSingleAction(ActionVal->AsObject()));
            }
        }
    }

    // memory_update
    if (const TSharedPtr<FJsonObject>* MemObj = nullptr;
        Root->TryGetObjectField(TEXT("memory_update"), MemObj) && MemObj)
    {
        (*MemObj)->TryGetStringField(TEXT("summary"), Result.MemorySummary);
        const TArray<TSharedPtr<FJsonValue>>* FactsArr = nullptr;
        if ((*MemObj)->TryGetArrayField(TEXT("facts"), FactsArr) && FactsArr)
        {
            for (const auto& F : *FactsArr)
            {
                FString Fact;
                if (F->TryGetString(Fact)) Result.MemoryFacts.Add(Fact);
            }
        }
    }

    // quests
    const TArray<TSharedPtr<FJsonValue>>* QuestsArr = nullptr;
    if (Root->TryGetArrayField(TEXT("quests"), QuestsArr) && QuestsArr)
    {
        for (const auto& QV : *QuestsArr)
        {
            if (QV->Type == EJson::Object)
                Result.InitialQuests.Add(ParseQuest(QV->AsObject()));
        }
    }

    Result.bSuccess = true;
    return Result;
}

FGMNarrativeResponse UGameMasterSubsystem::ParseNarrativeResponse(
    const FString& JsonString) const
{
    FGMNarrativeResponse Response;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);

    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        Response.ErrorMessage = TEXT("Failed to parse narrative JSON");
        return Response;
    }

    Root->TryGetStringField(TEXT("narration"), Response.NarrationText);

    const TArray<TSharedPtr<FJsonValue>>* ActionsArr = nullptr;
    if (Root->TryGetArrayField(TEXT("actions"), ActionsArr) && ActionsArr)
    {
        for (const auto& AV : *ActionsArr)
        {
            if (AV->Type == EJson::Object)
                Response.Actions.Add(ParseSingleAction(AV->AsObject()));
        }
    }

    if (const TSharedPtr<FJsonObject>* MemObj = nullptr;
        Root->TryGetObjectField(TEXT("memory_update"), MemObj) && MemObj)
    {
        (*MemObj)->TryGetStringField(TEXT("summary"), Response.MemoryUpdate);
        const TArray<TSharedPtr<FJsonValue>>* FactsArr = nullptr;
        if ((*MemObj)->TryGetArrayField(TEXT("facts"), FactsArr) && FactsArr)
        {
            for (const auto& F : *FactsArr)
            {
                FString Fact;
                if (F->TryGetString(Fact)) Response.NewFacts.Add(Fact);
            }
        }
    }

    Response.bSuccess = true;
    return Response;
}

FGMAction UGameMasterSubsystem::ParseSingleAction(
    const TSharedPtr<FJsonObject>& Obj) const
{
    FGMAction Action;
    if (!Obj.IsValid()) return Action;

    Obj->TryGetStringField(TEXT("type"), Action.RawType);
    Obj->TryGetStringField(TEXT("assetId"), Action.AssetID);

    // Fallback: npcId, enemyId, managerId, classId
    if (Action.AssetID.IsEmpty()) Obj->TryGetStringField(TEXT("npcId"), Action.AssetID);
    if (Action.AssetID.IsEmpty()) Obj->TryGetStringField(TEXT("enemyId"), Action.AssetID);
    if (Action.AssetID.IsEmpty()) Obj->TryGetStringField(TEXT("managerId"), Action.AssetID);

    int32 Count = 1;
    Obj->TryGetNumberField(TEXT("count"), Count);
    Action.Count = FMath::Clamp(Count, 1, 20);

    Obj->TryGetStringField(TEXT("personaPrompt"), Action.PersonaPrompt);
    Obj->TryGetStringField(TEXT("narration"), Action.NarrationText);

    // Transform
    if (const TSharedPtr<FJsonObject>* TrObj = nullptr;
        Obj->TryGetObjectField(TEXT("transform"), TrObj) && TrObj)
    {
        // Location
        if (const TSharedPtr<FJsonObject>* LocObj = nullptr;
            (*TrObj)->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
        {
            double X=0,Y=0,Z=0;
            (*LocObj)->TryGetNumberField(TEXT("x"), X);
            (*LocObj)->TryGetNumberField(TEXT("y"), Y);
            (*LocObj)->TryGetNumberField(TEXT("z"), Z);
            Action.SpawnTransform.Location = FVector(X, Y, Z);
        }
        // Rotation
        if (const TSharedPtr<FJsonObject>* RotObj = nullptr;
            (*TrObj)->TryGetObjectField(TEXT("rotation"), RotObj) && RotObj)
        {
            double P=0,Ya=0,R=0;
            (*RotObj)->TryGetNumberField(TEXT("pitch"), P);
            (*RotObj)->TryGetNumberField(TEXT("yaw"), Ya);
            (*RotObj)->TryGetNumberField(TEXT("roll"), R);
            Action.SpawnTransform.Rotation = FRotator(P, Ya, R);
        }
        // Scale
        if (const TSharedPtr<FJsonObject>* ScObj = nullptr;
            (*TrObj)->TryGetObjectField(TEXT("scale"), ScObj) && ScObj)
        {
            double X=1,Y=1,Z=1;
            (*ScObj)->TryGetNumberField(TEXT("x"), X);
            (*ScObj)->TryGetNumberField(TEXT("y"), Y);
            (*ScObj)->TryGetNumberField(TEXT("z"), Z);
            Action.SpawnTransform.Scale = FVector(X, Y, Z);
        }
    }

    // Quest (для SetQuest)
    if (const TSharedPtr<FJsonObject>* QObj = nullptr;
        Obj->TryGetObjectField(TEXT("quest"), QObj) && QObj)
    {
        Action.Quest = ParseQuest(*QObj);
    }

    // Тип
    static const TMap<FString, EGMActionType> TypeMap =
    {
        { TEXT("SpawnActor"),    EGMActionType::SpawnActor },
        { TEXT("SpawnNPC"),      EGMActionType::SpawnNPC },
        { TEXT("SpawnEnemy"),    EGMActionType::SpawnEnemy },
        { TEXT("SpawnEnemyGroup"), EGMActionType::SpawnEnemy },
        { TEXT("SpawnManager"),  EGMActionType::SpawnManager },
        { TEXT("SpawnNavMesh"),  EGMActionType::SpawnNavMesh },
        { TEXT("SpawnNavMeshBounds"), EGMActionType::SpawnNavMesh },
        { TEXT("SetQuest"),      EGMActionType::SetQuest },
        { TEXT("PlayNarration"), EGMActionType::PlayNarration },
        { TEXT("SetWeather"),    EGMActionType::SetWeather },
        { TEXT("TriggerEvent"),  EGMActionType::TriggerEvent },
    };

    const EGMActionType* Found = TypeMap.Find(Action.RawType);
    Action.ActionType = Found ? *Found : EGMActionType::Unknown;

    return Action;
}

FGMQuest UGameMasterSubsystem::ParseQuest(
    const TSharedPtr<FJsonObject>& QObj) const
{
    FGMQuest Quest;
    if (!QObj.IsValid()) return Quest;

    QObj->TryGetStringField(TEXT("questId"), Quest.QuestID);
    QObj->TryGetStringField(TEXT("title"),   Quest.Title);
    QObj->TryGetStringField(TEXT("description"), Quest.Description);

    const TArray<TSharedPtr<FJsonValue>>* StepsArr = nullptr;
    if (QObj->TryGetArrayField(TEXT("steps"), StepsArr) && StepsArr)
    {
        for (const auto& SV : *StepsArr)
        {
            if (SV->Type == EJson::Object)
            {
                FGMQuestStep Step;
                SV->AsObject()->TryGetStringField(TEXT("stepId"), Step.StepID);
                SV->AsObject()->TryGetStringField(TEXT("description"), Step.Description);
                Quest.Steps.Add(Step);
            }
        }
    }
    Quest.bActive = true;
    return Quest;
}

FString UGameMasterSubsystem::ExtractJsonFromResponse(
    const FString& RawResponse) const
{
    // Убираем markdown-обёртки если модель добавила ```json ... ```
    FString Cleaned = RawResponse.TrimStartAndEnd();

    if (Cleaned.StartsWith(TEXT("```")))
    {
        // Найти первый { и последний }
        int32 Start = Cleaned.Find(TEXT("{"));
        int32 End   = Cleaned.Find(TEXT("}"), ESearchCase::IgnoreCase,
                                    ESearchDir::FromEnd);
        if (Start != INDEX_NONE && End != INDEX_NONE && End > Start)
        {
            return Cleaned.Mid(Start, End - Start + 1);
        }
    }
    return Cleaned;
}

// ============================================================
//  VALIDATION
// ============================================================

TArray<FGMAction> UGameMasterSubsystem::ValidateActions(
    const TArray<FGMAction>& Actions) const
{
    TArray<FGMAction> Valid;
    if (!LevelSettings) return Actions; // без настроек — пропускаем

    int32 Limit = FMath::Min(Actions.Num(), LevelSettings->MaxActionsPerResponse);

    for (int32 i = 0; i < Limit; ++i)
    {
        const FGMAction& A = Actions[i];

        // Фильтруем Unknown
        if (A.ActionType == EGMActionType::Unknown)
        {
            UE_LOG(LogGameMaster, Warning,
                TEXT("Skipping unknown action type: '%s'"), *A.RawType);
            continue;
        }

        // Пустой assetId для spawn-типов — пропускаем
        if (A.AssetID.IsEmpty() &&
            (A.ActionType == EGMActionType::SpawnActor ||
             A.ActionType == EGMActionType::SpawnNPC ||
             A.ActionType == EGMActionType::SpawnEnemy ||
             A.ActionType == EGMActionType::SpawnManager))
        {
            UE_LOG(LogGameMaster, Warning, TEXT("Skipping action with empty assetId"));
            continue;
        }

        Valid.Add(A);
    }

    return Valid;
}

// ============================================================
//  CAMPAIGN HELPERS
// ============================================================

void UGameMasterSubsystem::ApplyLevelResultToCampaign(
    const FGMLevelGenerationResult& Result)
{
    EnsureSaveDataExists();

    if (!Result.MemorySummary.IsEmpty())
        SaveData->UpdateSummary(Result.MemorySummary);

    for (const FString& Fact : Result.MemoryFacts)
        SaveData->AddWorldFact(Fact);

    if (!Result.OpeningNarration.IsEmpty())
        SaveData->AddNarration(Result.OpeningNarration);

    SaveData->CampaignState.CurrentBiome = Result.BiomeType;
    SaveData->CampaignState.CurrentTheme = Result.LevelTheme;
}

void UGameMasterSubsystem::ApplyNarrativeResponseToCampaign(
    const FGMNarrativeResponse& Response)
{
    EnsureSaveDataExists();

    if (!Response.MemoryUpdate.IsEmpty())
        SaveData->UpdateSummary(Response.MemoryUpdate);

    for (const FString& Fact : Response.NewFacts)
        SaveData->AddWorldFact(Fact);

    if (!Response.NarrationText.IsEmpty())
        SaveData->AddNarration(Response.NarrationText);
}

void UGameMasterSubsystem::AddQuestToCampaign(const FGMQuest& Quest)
{
    EnsureSaveDataExists();
    // Проверяем — нет ли уже такого квеста
    for (const FGMQuest& Existing : SaveData->CampaignState.ActiveQuests)
    {
        if (Existing.QuestID == Quest.QuestID) return;
    }
    SaveData->CampaignState.ActiveQuests.Add(Quest);
}

FString UGameMasterSubsystem::GetCatalogSummaryForPrompt() const
{
    if (!WorldCatalog) return TEXT("(catalog not assigned)\n");

    FString Summary;
    TArray<FString> IDs = WorldCatalog->GetAllRegisteredIDs();
    for (const FString& ID : IDs)
    {
        Summary += TEXT("  - ") + ID + TEXT("\n");
    }
    return Summary.IsEmpty() ? TEXT("  (empty catalog)\n") : Summary;
}
