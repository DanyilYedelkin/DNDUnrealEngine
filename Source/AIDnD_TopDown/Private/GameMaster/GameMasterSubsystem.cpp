#include "GameMaster/GameMasterSubsystem.h"
#include "GameMaster/WorldActionExecutor.h"
#include "GameMaster/WorldCatalogDataAsset.h"
#include "GameMaster/LevelGenerationSettingsDataAsset.h"
#include "GameMaster/SaveGame_CampaignMemory.h"
#include "AI/OpenAIChatService.h"
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
    
    OpenAIService = NewObject<UOpenAIChatService>(this);
    OpenAIService->Initialize();
    OpenAIService->CooldownSeconds = RequestCooldown;
    
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
//  Level generation
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
    
    ActionExecutor->Catalog  = WorldCatalog;
    ActionExecutor->Settings = LevelSettings;
    ActionExecutor->ResetCounters();

    TArray<FChatMessage> Messages;
    Messages.Add(FChatMessage(EChatRole::System, BuildGMSystemPrompt()));
    Messages.Add(FChatMessage(EChatRole::User,   BuildLevelGenerationPrompt()));

    UE_LOG(LogGameMaster, Log, TEXT("Sending level generation request to GM..."));
    
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

                // Fallback: default for a spawn
                Result.bSuccess = true;
                Result.OpeningNarration = TEXT("You find yourself in a mysterious place. Your adventure begins...");
            }

            ApplyLevelResultToCampaign(Result);
            
            TArray<FGMAction> ValidActions = ValidateActions(Result.Actions);
            ActionExecutor->ExecuteActions(ValidActions, W);

            if (!Result.OpeningNarration.IsEmpty())
            {
                OnNarrationReady.Broadcast(Result.OpeningNarration);
            }

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
                Response.bSuccess = true;
                Response.NarrationText = TEXT("The world reacts to your actions...");
            }

            ApplyNarrativeResponseToCampaign(Response);

            if (!Response.NarrationText.IsEmpty())
            {
                OnNarrationReady.Broadcast(Response.NarrationText);
            }

            if (W && Response.Actions.Num() > 0)
            {
                TArray<FGMAction> Valid = ValidateActions(Response.Actions);
                ActionExecutor->ExecuteActions(Valid, W);
            }

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
    Prompt += TEXT("Your role: create worlds, narrate stories, spawn characters, manage the campaign.\n\n");

    Prompt += TEXT("CRITICAL: Respond ONLY with valid JSON. No markdown, no ```json, no text outside JSON.\n");
    Prompt += TEXT("All property names and string values must use double quotes.\n");
    Prompt += TEXT("Numbers must be plain numbers, not strings: {\"x\":0} not {\"x\":\"0\"}\n\n");
    
    Prompt += TEXT("REQUIRED JSON SCHEMA (follow exactly):\n");
    Prompt += TEXT("{\n");
    Prompt += TEXT("  \"level_metadata\": {\"theme\":\"string\",\"seed\":42},\n");
    Prompt += TEXT("  \"narration\": \"Opening scene description (2-3 sentences)\",\n");
    Prompt += TEXT("  \"actions\": [\n");
    Prompt += TEXT("    {\n");
    Prompt += TEXT("      \"type\": \"SpawnActor\",\n");
    Prompt += TEXT("      \"assetId\": \"EXACT_ID_FROM_LIST\",\n");
    Prompt += TEXT("      \"transform\": {\n");
    Prompt += TEXT("        \"location\": {\"x\":0.0, \"y\":0.0, \"z\":0.0},\n");
    Prompt += TEXT("        \"rotation\": {\"pitch\":0.0, \"yaw\":0.0, \"roll\":0.0},\n");
    Prompt += TEXT("        \"scale\":    {\"x\":1.0, \"y\":1.0, \"z\":1.0}\n");
    Prompt += TEXT("      }\n");
    Prompt += TEXT("    },\n");
    Prompt += TEXT("    {\n");
    Prompt += TEXT("      \"type\": \"SpawnNPC\",\n");
    Prompt += TEXT("      \"assetId\": \"EXACT_NPC_ID_FROM_LIST\",\n");
    Prompt += TEXT("      \"personaPrompt\": \"NPC personality in 2-3 sentences\",\n");
    Prompt += TEXT("      \"transform\": {\"location\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"rotation\":{\"pitch\":0.0,\"yaw\":0.0,\"roll\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}\n");
    Prompt += TEXT("    },\n");
    Prompt += TEXT("    {\n");
    Prompt += TEXT("      \"type\": \"SpawnEnemy\",\n");
    Prompt += TEXT("      \"assetId\": \"EXACT_ENEMY_ID_FROM_LIST\",\n");
    Prompt += TEXT("      \"count\": 2,\n");
    Prompt += TEXT("      \"transform\": {\"location\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"rotation\":{\"pitch\":0.0,\"yaw\":0.0,\"roll\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}\n");
    Prompt += TEXT("    },\n");
    Prompt += TEXT("    {\n");
    Prompt += TEXT("      \"type\": \"SpawnManager\",\n");
    Prompt += TEXT("      \"assetId\": \"EXACT_MANAGER_ID_FROM_LIST\",\n");
    Prompt += TEXT("      \"transform\": {\"location\":{\"x\":0.0,\"y\":0.0,\"z\":0.0},\"rotation\":{\"pitch\":0.0,\"yaw\":0.0,\"roll\":0.0},\"scale\":{\"x\":1.0,\"y\":1.0,\"z\":1.0}}\n");
    Prompt += TEXT("    },\n");
    Prompt += TEXT("    {\n");
    Prompt += TEXT("      \"type\": \"SetQuest\",\n");
    Prompt += TEXT("      \"quest\": {\n");
    Prompt += TEXT("        \"questId\": \"q1\",\n");
    Prompt += TEXT("        \"title\": \"Quest title\",\n");
    Prompt += TEXT("        \"description\": \"Quest description\",\n");
    Prompt += TEXT("        \"steps\": [{\"stepId\":\"s1\",\"description\":\"First step\"}]\n");
    Prompt += TEXT("      }\n");
    Prompt += TEXT("    }\n");
    Prompt += TEXT("  ],\n");
    Prompt += TEXT("  \"memory_update\": {\"summary\":\"1-2 sentences about this location\",\"facts\":[]}\n");
    Prompt += TEXT("}\n\n");

    Prompt += TEXT("ASSET RULES:\n");
    Prompt += TEXT("- Use ONLY assetIds from the list below — no invented names\n");
    Prompt += TEXT("- SpawnActor: use Environment assets for walls/floors/props\n");
    Prompt += TEXT("- SpawnNPC: use ONLY NPC assets — you MUST spawn every NPC from the list\n");
    Prompt += TEXT("- SpawnEnemy: use ONLY Enemy assets — you MUST spawn every Enemy type from the list\n");
    Prompt += TEXT("- SpawnManager: use ONLY Manager assets — spawn all of them\n");
    Prompt += TEXT("- NPC and Enemy scale must always be (1,1,1) — never scale characters\n\n");

    if (SaveData)
    {
        FString CampaignContext = SaveData->BuildGMContextString();
        if (!CampaignContext.IsEmpty())
        {
            Prompt += TEXT("[CAMPAIGN MEMORY - use for narrative continuity]:\n");
            Prompt += CampaignContext;
            Prompt += TEXT("\n");
        }
    }

    Prompt += TEXT("[AVAILABLE ASSET IDs — use EXACT strings, no changes]:\n");
    Prompt += GetCatalogSummaryForPrompt();

    return Prompt;
}

FString UGameMasterSubsystem::BuildLevelGenerationPrompt_Implementation() const
{
    if (!LevelSettings) 
        return TEXT("Generate narration and characters for a dungeon. Return JSON.");

    FString Prompt;
    Prompt += FString::Printf(TEXT("Level: \"%s\", difficulty: %s, atmosphere: %s\n\n"),
        *LevelSettings->LevelName,
        *LevelSettings->GetDifficultyString(),
        *LevelSettings->Atmosphere);

    if (LevelSettings->IsDungeon())
    {
        // Стены уже будут сгенерированы кодом — GPT только добавляет персонажей
        Prompt += TEXT("The dungeon layout (walls/floors) is already built by the engine.\n");
        Prompt += TEXT("Your job: add characters, quest, and narration.\n\n");

        Prompt += TEXT("ROOM LOCATIONS for character placement:\n");
        Prompt += TEXT("  Room 1 (player start): center (0, 0, 100)\n");
        Prompt += TEXT("  Room 2 (NPC room):     center (-2400, 0, 100)\n");
        Prompt += TEXT("  Room 3 (enemy room):   center (-2400, -2400, 100)\n\n");
        Prompt += TEXT("IMPORTANT: All NPC and Enemy Z location must be 100 or higher. Never spawn characters at Z=0.\n");
    }

    Prompt += FString::Printf(
        TEXT("Spawn limits: max %d NPCs, max %d enemies.\n"),
        LevelSettings->MaxNPCs, LevelSettings->MaxEnemies);

    if (!LevelSettings->StartingQuestHint.IsEmpty())
        Prompt += FString::Printf(TEXT("Quest theme: %s\n"), *LevelSettings->StartingQuestHint);

    if (LevelSettings->RequiredManagerIDs.Num() > 0)
    {
        Prompt += TEXT("Required managers: ");
        for (const FString& ID : LevelSettings->RequiredManagerIDs)
            Prompt += ID + TEXT(" ");
        Prompt += TEXT("\n");
    }

    if (!LevelSettings->ExtraGMInstructions.IsEmpty())
        Prompt += LevelSettings->ExtraGMInstructions + TEXT("\n");

    Prompt += TEXT("\nRespond ONLY with valid JSON.");
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

    OpenAIService->MaxTokens = MaxTokens;
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
    UE_LOG(LogTemp, Warning, TEXT("=== GM RAW RESPONSE ===\n%s"), *JsonString);
    
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

    // Quest (for SetQuest)
    if (const TSharedPtr<FJsonObject>* QObj = nullptr;
        Obj->TryGetObjectField(TEXT("quest"), QObj) && QObj)
    {
        Action.Quest = ParseQuest(*QObj);
    }

    // type
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
    FString Cleaned = RawResponse.TrimStartAndEnd();

    if (Cleaned.StartsWith(TEXT("```")))
    {
        int32 Start = Cleaned.Find(TEXT("{"));
        int32 End   = Cleaned.Find(TEXT("}"), ESearchCase::IgnoreCase,
                                    ESearchDir::FromEnd);
        if (Start != INDEX_NONE && End != INDEX_NONE && End > Start)
            Cleaned = Cleaned.Mid(Start, End - Start + 1);
    }
    
    TArray<FString> BadPatterns = {
        TEXT("\"x:"), TEXT("\"y:"), TEXT("\"z:"),
        TEXT("\"pitch:"), TEXT("\"yaw:"), TEXT("\"roll:")
    };
    TArray<FString> GoodPatterns = {
        TEXT("\"x\":"), TEXT("\"y\":"), TEXT("\"z\":"),
        TEXT("\"pitch\":"), TEXT("\"yaw\":"), TEXT("\"roll\":")
    };

    for (int32 i = 0; i < BadPatterns.Num(); i++)
    {
        Cleaned = Cleaned.Replace(*BadPatterns[i], *GoodPatterns[i],
                                   ESearchCase::CaseSensitive);
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
    if (!LevelSettings)
    {
        return Actions;
    }

    int32 Limit = FMath::Min(Actions.Num(), LevelSettings->MaxActionsPerResponse);

    for (int32 i = 0; i < Limit; ++i)
    {
        const FGMAction& A = Actions[i];

        if (A.ActionType == EGMActionType::Unknown)
        {
            UE_LOG(LogGameMaster, Warning,
                TEXT("Skipping unknown action type: '%s'"), *A.RawType);
            continue;
        }
        
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

    for (const FGMQuest& Existing : SaveData->CampaignState.ActiveQuests)
    {
        if (Existing.QuestID == Quest.QuestID) return;
    }
    SaveData->CampaignState.ActiveQuests.Add(Quest);
}

FString UGameMasterSubsystem::GetCatalogSummaryForPrompt() const
{
    if (!WorldCatalog)
    {
        return TEXT("(catalog not assigned)\n");
    }

    FString Summary;

    // Environment
    TArray<FString> EnvIDs;
    WorldCatalog->EnvironmentActors.GetKeys(EnvIDs);
    if (EnvIDs.Num() > 0)
    {
        Summary += TEXT("  Environment (use for walls/floor/props, type=SpawnActor):\n");
        for (const FString& ID : EnvIDs)
        {
            Summary += TEXT("    - \"") + ID + TEXT("\"\n");
        }
    }

    // NPCs
    TArray<FString> NPCIDs;
    WorldCatalog->NPCActors.GetKeys(NPCIDs);
    if (NPCIDs.Num() > 0)
    {
        Summary += TEXT("  NPCs (type=SpawnNPC, include personaPrompt):\n");
        for (const FString& ID : NPCIDs)
        {
            Summary += TEXT("    - \"") + ID + TEXT("\"\n");
        }
    }

    // Enemies
    TArray<FString> EnemyIDs;
    WorldCatalog->EnemyActors.GetKeys(EnemyIDs);
    if (EnemyIDs.Num() > 0)
    {
        Summary += TEXT("  Enemies (type=SpawnEnemy, include count 1-4):\n");
        for (const FString& ID : EnemyIDs)
        {
            Summary += TEXT("    - \"") + ID + TEXT("\"\n");
        }
    }

    // Managers
    TArray<FString> MgrIDs;
    WorldCatalog->ManagerActors.GetKeys(MgrIDs);
    if (MgrIDs.Num() > 0)
    {
        Summary += TEXT("  Managers (type=SpawnManager):\n");
        for (const FString& ID : MgrIDs)
        {
            Summary += TEXT("    - \"") + ID + TEXT("\"\n");
        }
    }

    if (Summary.IsEmpty())
    {
        return TEXT("  (catalog is empty! Fill DA_WorldCatalog)\n");
    }

    return Summary;
}
