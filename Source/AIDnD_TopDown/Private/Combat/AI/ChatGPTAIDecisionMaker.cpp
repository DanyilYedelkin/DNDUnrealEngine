// Source/AIDnD_TopDown/Private/Combat/AI/ChatGPTAIDecisionMaker.cpp
#include "Combat/AI/ChatGPTAIDecisionMaker.h"
#include "Combat/AI/LocalAIDecisionMaker.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatLog.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "TimerManager.h"

UChatGPTAIDecisionMaker::UChatGPTAIDecisionMaker()
{
    //FallbackAI = CreateDefaultSubobject<ULocalAIDecisionMaker>(TEXT("FallbackAI"));
}

// ============================================================
//  REQUEST
// ============================================================

void UChatGPTAIDecisionMaker::RequestDecision_Implementation(
    const FAIBattleContext& Context)
{
    PendingContext = Context;

    // If API key not set → use local AI immediately
    if (ApiKey.IsEmpty())
    {
        UE_LOG(LogCombat, Warning,
            TEXT("ChatGPTAI: No API key set — using fallback Local AI"));
        TriggerFallback(TEXT("No API key configured"));
        return;
    }

    const FString ContextJson = SerializeBattleContext(Context);
    const FString UserPrompt  = BuildUserPrompt(ContextJson);
    const FString SystemPrompt = BuildSystemPrompt();

    // Build JSON body
    TSharedPtr<FJsonObject> Root  = MakeShareable(new FJsonObject);
    Root->SetStringField(TEXT("model"), Model);
    Root->SetNumberField(TEXT("max_tokens"), 512);
    Root->SetBoolField(TEXT("stream"), false);

    TArray<TSharedPtr<FJsonValue>> Messages;

    // System message
    TSharedPtr<FJsonObject> SysMsg = MakeShareable(new FJsonObject);
    SysMsg->SetStringField(TEXT("role"), TEXT("system"));
    SysMsg->SetStringField(TEXT("content"), SystemPrompt);
    Messages.Add(MakeShareable(new FJsonValueObject(SysMsg)));

    // User message
    TSharedPtr<FJsonObject> UserMsg = MakeShareable(new FJsonObject);
    UserMsg->SetStringField(TEXT("role"), TEXT("user"));
    UserMsg->SetStringField(TEXT("content"), UserPrompt);
    Messages.Add(MakeShareable(new FJsonValueObject(UserMsg)));

    Root->SetArrayField(TEXT("messages"), Messages);

    FString BodyString;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

    // Build HTTP request
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest =
        FHttpModule::Get().CreateRequest();

    HttpRequest->SetURL(ApiEndpoint);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Authorization"),
        FString::Printf(TEXT("Bearer %s"), *ApiKey));
    HttpRequest->SetContentAsString(BodyString);

    HttpRequest->OnProcessRequestComplete().BindUObject(
        this, &UChatGPTAIDecisionMaker::OnHttpResponseReceived);

    // Start timeout timer
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            TimeoutHandle,
            [this]() { TriggerFallback(TEXT("Request timed out")); },
            TimeoutSeconds,
            false);
    }

    HttpRequest->ProcessRequest();

    UE_LOG(LogCombat, Log,
        TEXT("ChatGPTAI: Request sent for %s"),
        Context.SelfCharacter
            ? *Context.SelfCharacter->CharacterName.ToString()
            : TEXT("Unknown"));
}

// ============================================================
//  RESPONSE
// ============================================================

void UChatGPTAIDecisionMaker::OnHttpResponseReceived(
    FHttpRequestPtr Request,
    FHttpResponsePtr Response,
    bool bSuccess)
{
    // Cancel timeout
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(TimeoutHandle);

    if (!bSuccess || !Response.IsValid())
    {
        TriggerFallback(TEXT("HTTP request failed"));
        return;
    }

    if (Response->GetResponseCode() != 200)
    {
        UE_LOG(LogCombat, Warning,
            TEXT("ChatGPTAI: API returned code %d"),
            Response->GetResponseCode());
        TriggerFallback(FString::Printf(
            TEXT("API error code %d"), Response->GetResponseCode()));
        return;
    }

    const FString ResponseBody = Response->GetContentAsString();
    UE_LOG(LogCombat, Verbose,
        TEXT("ChatGPTAI: Response received (%d chars)"),
        ResponseBody.Len());

    // Extract content from OpenAI response format
    TSharedPtr<FJsonObject> JsonRoot;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(ResponseBody);

    if (!FJsonSerializer::Deserialize(Reader, JsonRoot) || !JsonRoot.IsValid())
    {
        TriggerFallback(TEXT("Failed to parse JSON response"));
        return;
    }

    // Navigate: choices[0].message.content
    const TArray<TSharedPtr<FJsonValue>>* Choices;
    if (!JsonRoot->TryGetArrayField(TEXT("choices"), Choices)
        || Choices->IsEmpty())
    {
        TriggerFallback(TEXT("No choices in response"));
        return;
    }

    const TSharedPtr<FJsonObject>* FirstChoice;
    if (!(*Choices)[0]->TryGetObject(FirstChoice)) 
    {
        TriggerFallback(TEXT("Invalid choice format"));
        return;
    }

    const TSharedPtr<FJsonObject>* MessageObj;
    if (!(*FirstChoice)->TryGetObjectField(TEXT("message"), MessageObj))
    {
        TriggerFallback(TEXT("No message in choice"));
        return;
    }

    FString Content;
    if (!(*MessageObj)->TryGetStringField(TEXT("content"), Content))
    {
        TriggerFallback(TEXT("No content in message"));
        return;
    }

    // Parse the content into a decision
    FAIDecision Decision = ParseGPTResponse(Content, PendingContext);

    UE_LOG(LogCombat, Log,
        TEXT("ChatGPTAI: Decision = %s — %s"),
        *UEnum::GetValueAsString(Decision.DecisionType),
        *Decision.Reasoning);

    OnDecisionReady.Broadcast(Decision);
}

void UChatGPTAIDecisionMaker::TriggerFallback(const FString& Reason)
{
    UE_LOG(LogCombat, Warning,
        TEXT("ChatGPTAI: Fallback triggered — %s"), *Reason);

    if (!FallbackAI)
        FallbackAI = NewObject<ULocalAIDecisionMaker>(this);

    if (!FallbackAI) return;

    // Используем AddDynamic вместо AddLambda
    FallbackAI->OnDecisionReady.AddDynamic(
        this, &UChatGPTAIDecisionMaker::OnFallbackDecisionReady);

    FallbackAI->RequestDecision(PendingContext);
}

void UChatGPTAIDecisionMaker::OnFallbackDecisionReady(FAIDecision Decision)
{
    Decision.bIsFallback = true;
    FallbackAI->OnDecisionReady.RemoveAll(this);
    OnDecisionReady.Broadcast(Decision);
}

// ============================================================
//  SERIALIZATION
// ============================================================

FString UChatGPTAIDecisionMaker::SerializeBattleContext(
    const FAIBattleContext& Context)
{
    TSharedPtr<FJsonObject> Root = MakeShareable(new FJsonObject);

    Root->SetNumberField(TEXT("round"), Context.CurrentRound);
    Root->SetNumberField(TEXT("self_hp"), Context.SelfHP);
    Root->SetNumberField(TEXT("self_max_hp"), Context.SelfMaxHP);

    // Enemies
    TArray<TSharedPtr<FJsonValue>> EnemyArray;
    for (const FAITargetInfo& E : Context.Enemies)
    {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("name"),
            E.Character ? E.Character->CharacterName.ToString() : TEXT("?"));
        Obj->SetNumberField(TEXT("hp_percent"),
            FMath::RoundToInt(E.HPPercent * 100));
        Obj->SetNumberField(TEXT("ac"), E.ArmorClass);
        Obj->SetNumberField(TEXT("distance_feet"),
            FMath::RoundToInt(E.DistanceFeet));
        EnemyArray.Add(MakeShareable(new FJsonValueObject(Obj)));
    }
    Root->SetArrayField(TEXT("enemies"), EnemyArray);

    // Available actions
    TArray<TSharedPtr<FJsonValue>> ActionArray;
    for (const FAIActionOption& A : Context.AvailableActions)
    {
        TSharedPtr<FJsonObject> Obj = MakeShareable(new FJsonObject);
        Obj->SetStringField(TEXT("name"), A.ActionName.ToString());
        Obj->SetNumberField(TEXT("range"), A.Range);
        ActionArray.Add(MakeShareable(new FJsonValueObject(Obj)));
    }
    Root->SetArrayField(TEXT("available_actions"), ActionArray);

    FString Result;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
    FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
    return Result;
}

FAIDecision UChatGPTAIDecisionMaker::ParseGPTResponse(
    const FString& JsonResponse, const FAIBattleContext& Context)
{
    FAIDecision Decision;

    // Expected GPT response format:
    // { "action": "Attack", "target": "Goblin", "reasoning": "..." }

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(JsonResponse);

    if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
    {
        UE_LOG(LogCombat, Warning,
            TEXT("ChatGPTAI: Failed to parse decision JSON"));
        Decision.DecisionType = EAIDecisionType::EndTurn;
        Decision.Reasoning    = TEXT("Parse error — defaulting to EndTurn");
        return Decision;
    }

    FString ActionStr;
    Json->TryGetStringField(TEXT("action"), ActionStr);
    Json->TryGetStringField(TEXT("reasoning"), Decision.Reasoning);

    FString TargetName;
    Json->TryGetStringField(TEXT("target"), TargetName);

    // Map action string to enum
    if (ActionStr == TEXT("Attack"))
        Decision.DecisionType = EAIDecisionType::Attack;
    else if (ActionStr == TEXT("Move"))
        Decision.DecisionType = EAIDecisionType::Move;
    else if (ActionStr == TEXT("Dodge"))
        Decision.DecisionType = EAIDecisionType::Dodge;
    else if (ActionStr == TEXT("Dash"))
        Decision.DecisionType = EAIDecisionType::Dash;
    else if (ActionStr == TEXT("Help"))
        Decision.DecisionType = EAIDecisionType::Help;
    else
        Decision.DecisionType = EAIDecisionType::EndTurn;

    // Resolve target by name
    if (!TargetName.IsEmpty())
    {
        for (const FAITargetInfo& Enemy : Context.Enemies)
        {
            if (Enemy.Character &&
                Enemy.Character->CharacterName.ToString() == TargetName)
            {
                Decision.TargetCharacter = Enemy.Character;
                Decision.TargetLocation  = Enemy.Character->GetActorLocation();
                break;
            }
        }
    }

    return Decision;
}

FString UChatGPTAIDecisionMaker::BuildSystemPrompt()
{
    return TEXT(
        "You are an AI controlling an enemy in a D&D 5e turn-based combat game. "
        "You will receive the current battle state as JSON. "
        "Respond ONLY with a JSON object in this exact format: "
        "{ \"action\": \"Attack|Move|Dodge|Dash|Help|EndTurn\", "
        "\"target\": \"<enemy name or empty>\", "
        "\"reasoning\": \"<brief explanation>\" }. "
        "Make tactical decisions based on HP, distance, and action economy. "
        "Do not include any text outside the JSON object."
    );
}

FString UChatGPTAIDecisionMaker::BuildUserPrompt(const FString& ContextJson)
{
    return FString::Printf(
        TEXT("Current battle state:\n%s\n\nWhat is your action?"),
        *ContextJson);
}