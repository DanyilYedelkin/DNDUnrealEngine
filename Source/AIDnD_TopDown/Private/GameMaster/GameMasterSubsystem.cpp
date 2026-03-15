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
    Prompt += TEXT("Your role is to create immersive worlds, compelling narratives, ");
    Prompt += TEXT("react to player actions, and manage the campaign.\n\n");

    Prompt += TEXT("CRITICAL: You MUST respond ONLY with valid JSON. No markdown, no explanation outside JSON.\n\n");

    bool bIsDungeon = LevelSettings && LevelSettings->IsDungeon();

    if (bIsDungeon)
    {
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
        Prompt += TEXT("MAP TYPE: OPEN WORLD (outdoor)\n");
        Prompt += TEXT("You scatter decorative prefabs across the landscape:\n");
        Prompt += TEXT("  - Trees, rocks, bushes for natural terrain.\n");
        Prompt += TEXT("  - Houses and ruins as decoration (NOT enterable buildings).\n");
        Prompt += TEXT("  - Vary scale slightly (0.8-1.5) for natural variety.\n");
        Prompt += TEXT("  - Spread objects across the entire map area.\n");
        Prompt += TEXT("  - Leave open areas for combat and movement.\n\n");
    }
    
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
    
    if (SaveData)
    {
        FString CampaignContext = SaveData->BuildGMContextString();
        if (!CampaignContext.IsEmpty())
        {
            Prompt += TEXT("[CAMPAIGN MEMORY]:\n") + CampaignContext + TEXT("\n");
        }
    }

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

    // coordinates limits
    Prompt += FString::Printf(
        TEXT("World bounds (Unreal units): X[%.0f .. %.0f]  Y[%.0f .. %.0f]  Z[0 .. 400]\n"),
        LevelSettings->WorldMin.X, LevelSettings->WorldMax.X,
        LevelSettings->WorldMin.Y, LevelSettings->WorldMax.Y);

    // limits
    Prompt += FString::Printf(
        TEXT("Spawn limits: max %d environment objects, max %d NPCs, max %d enemies.\n"),
        LevelSettings->MaxEnvironmentActors,
        LevelSettings->MaxNPCs,
        LevelSettings->MaxEnemies);

    // instructions for a map's type
    if (LevelSettings->IsDungeon())
    {
        Prompt += TEXT("\n--- DUNGEON BUILDING RULES (read carefully) ---\n");
        Prompt += TEXT("Asset 'Wall_Stone' is a cube: 100x100x100 Unreal Units at scale(1,1,1).\n");
        Prompt += TEXT("scale(X,Y,Z) means the cube becomes X*100 x Y*100 x Z*100 UU.\n\n");

        Prompt += TEXT("=== FLOOR ===\n");
        Prompt += TEXT("Always ONE big floor per room. Flat, thin.\n");
        Prompt += TEXT("scale: X=room_width, Y=room_depth, Z=0.5\n");
        Prompt += TEXT("location Z = 0 (sits on ground level)\n\n");

        Prompt += TEXT("=== WALLS ===\n");
        Prompt += TEXT("Walls are THIN and TALL: thickness Y=0.5, height Z=5\n");
        Prompt += TEXT("North/South walls: scale(room_width, 0.5, 5), run along X axis\n");
        Prompt += TEXT("East/West walls:   scale(0.5, room_depth, 5), run along Y axis\n");
        Prompt += TEXT("Wall center Z = 250 (= 5*100/2, sits on top of floor)\n\n");

        Prompt += TEXT("=== WALL PLACEMENT FORMULA ===\n");
        Prompt += TEXT("If room floor is at (cx, cy) with scale(W, D, 0.5):\n");
        Prompt += TEXT("  North wall: location(cx,          cy + D*50 + 25, 250), scale(W,   0.5, 5)\n");
        Prompt += TEXT("  South wall: location(cx,          cy - D*50 - 25, 250), scale(W,   0.5, 5)\n");
        Prompt += TEXT("  East wall:  location(cx + W*50 + 25, cy,          250), scale(0.5, D,   5)\n");
        Prompt += TEXT("  West wall:  location(cx - W*50 - 25, cy,          250), scale(0.5, D,   5)\n\n");

        Prompt += TEXT("=== CONCRETE EXAMPLE: 20x20 room at origin ===\n");
        Prompt += TEXT("Floor: location(0,0,0)       scale(20, 20, 0.5)\n");
        Prompt += TEXT("North: location(0, 1025, 250) scale(20, 0.5, 5)\n");
        Prompt += TEXT("South: location(0,-1025, 250) scale(20, 0.5, 5)\n");
        Prompt += TEXT("East:  location(1025, 0, 250) scale(0.5, 20, 5)\n");
        Prompt += TEXT("West:  location(-1025,0, 250) scale(0.5, 20, 5)\n\n");

        Prompt += TEXT("=== CORRIDOR connecting two rooms ===\n");
        Prompt += TEXT("Floor: scale(4, 10, 0.5) between room centers\n");
        Prompt += TEXT("Left wall:  scale(0.5, 10, 5)\n");
        Prompt += TEXT("Right wall: scale(0.5, 10, 5)\n");
        Prompt += TEXT("NO end walls where corridor meets rooms (leave gap for passage)\n\n");

        Prompt += TEXT("=== DOOR GAPS ===\n");
        Prompt += TEXT("Where corridor connects to room: remove that wall segment.\n");
        Prompt += TEXT("Replace one full wall with two half-walls leaving 300 UU gap in middle.\n");
        Prompt += TEXT("Half-wall scale: (W/2 - 1.5, 0.5, 5), placed left and right of gap.\n\n");

        Prompt += TEXT("=== GENERATE THIS LAYOUT ===\n");
        Prompt += TEXT("Room 1 (start): 20x20, centered at (0, 0)\n");
        Prompt += TEXT("Corridor:       4x12, going North from Room 1 center\n");
        Prompt += TEXT("Room 2:         16x16, centered at (0, 2000)\n");
        Prompt += TEXT("Corridor:       4x10, going East from Room 2\n");
        Prompt += TEXT("Room 3:         12x12, centered at (1800, 2000)\n\n");

        Prompt += TEXT("PLAYER START RULES:\n");
        Prompt += TEXT("- Room 1 center (0,0) is the spawn point — NO walls within 300 UU of (0,0,0)\n");
        Prompt += TEXT("- The very center of Room 1 must be completely open\n");
        Prompt += TEXT("- Walls of Room 1 start at distance 500+ UU from center\n");
        Prompt += TEXT("- Place NPCs in Room 2, enemies in Room 3 only\n");
        Prompt += TEXT("- NEVER place any actor at location closer than 200 UU to (0,0)\n\n");
        Prompt += TEXT("ALWAYS include floor for every room and corridor.\n");
        Prompt += TEXT("DO NOT use rotation — use scale X/Y swap instead for orientation.\n");
        Prompt += TEXT("Total actors: 20-30 (floors + walls).\n\n");
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

    // quest
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

    // managers
    if (LevelSettings->RequiredManagerIDs.Num() > 0)
    {
        Prompt += TEXT("You MUST spawn these managers (use SpawnManager): ");
        for (const FString& ID : LevelSettings->RequiredManagerIDs)
            Prompt += ID + TEXT(", ");
        Prompt += TEXT("\n");
    }
    
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
