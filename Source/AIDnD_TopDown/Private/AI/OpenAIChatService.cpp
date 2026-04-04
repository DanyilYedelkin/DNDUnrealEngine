#include "AI/OpenAIChatService.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/ConfigCacheIni.h"

void UOpenAIChatService::Initialize()
{
    APIKey = LoadAPIKeyFromConfig();
    ModelName = TEXT("gpt-4o-mini");
    EndpointURL = TEXT("https://api.openai.com/v1/chat/completions");
}

FString UOpenAIChatService::LoadAPIKeyFromConfig() const
{
    FString Key;
    GConfig->GetString(TEXT("OpenAI"), TEXT("APIKey"), Key, GGameIni);
    if (Key.IsEmpty())
    {
        Key = FPlatformMisc::GetEnvironmentVariable(TEXT("OPENAI_API_KEY"));
    }
    if (Key.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("OpenAI API key not found! "
            "Set [OpenAI] APIKey in DefaultGame.ini or OPENAI_API_KEY env var."));
    }
    return Key;
}

TSharedPtr<FJsonObject> UOpenAIChatService::BuildRequestBody(
    const TArray<FChatMessage>& Messages) const
{
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("model"), ModelName);
    Root->SetNumberField(TEXT("max_tokens"), MaxTokens);
    Root->SetNumberField(TEXT("temperature"), 0.8);

    TArray<TSharedPtr<FJsonValue>> MessagesArray;
    for (const FChatMessage& Msg : Messages)
    {
        auto MsgObj = MakeShared<FJsonObject>();
        FString RoleStr;
        switch (Msg.Role)
        {
            case EChatRole::System:    RoleStr = TEXT("system");    break;
            case EChatRole::User:      RoleStr = TEXT("user");      break;
            case EChatRole::Assistant: RoleStr = TEXT("assistant"); break;
        }
        MsgObj->SetStringField(TEXT("role"),    RoleStr);
        MsgObj->SetStringField(TEXT("content"), Msg.Content);
        MessagesArray.Add(MakeShared<FJsonValueObject>(MsgObj));
    }
    Root->SetArrayField(TEXT("messages"), MessagesArray);
    return Root;
}

void UOpenAIChatService::SendChatRequest(const TArray<FChatMessage>& Messages,
                                          FOnChatSuccess OnSuccess,
                                          FOnChatError   OnError)
{
    double Now = FPlatformTime::Seconds();

    if (bRequestInFlight)
    {
        UE_LOG(LogTemp, Warning, TEXT("Forcing reset of stuck request"));
        bRequestInFlight = false;
    }

    if ((Now - LastRequestTime) < CooldownSeconds)
    {
        OnError.ExecuteIfBound(TEXT("Cooldown active."));
        return;
    }
    if (APIKey.IsEmpty())
    {
        OnError.ExecuteIfBound(TEXT("API key not configured."));
        return;
    }

    bRequestInFlight = true;
    LastRequestTime  = Now;  

    TSharedPtr<FJsonObject> RequestBody = BuildRequestBody(Messages);
    FString BodyString;
    auto Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    if (ActiveRequest.IsValid())
    {
        ActiveRequest->CancelRequest();
        ActiveRequest.Reset();
    }

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> HttpRequest =
        FHttpModule::Get().CreateRequest();
    ActiveRequest = HttpRequest;

    HttpRequest->SetURL(EndpointURL);
    HttpRequest->SetVerb(TEXT("POST"));
    HttpRequest->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
    HttpRequest->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + APIKey);
    HttpRequest->SetContentAsString(BodyString);
    HttpRequest->SetTimeout(90.0f);

    HttpRequest->OnProcessRequestComplete().BindUObject(
        this, &UOpenAIChatService::OnResponseReceived,
        OnSuccess, OnError);
    
    FString RequestType = (MaxTokens >= 1000)
        ? TEXT("LevelGeneration")
        : (MaxTokens >= 200 ? TEXT("NPCDialogue") : TEXT("CombatAI"));
    UE_LOG(LogTemp, Warning,
        TEXT("[LATENCY] >>> Request START | type=%s | bodyLen=%d chars"),
        *RequestType, BodyString.Len());

    HttpRequest->ProcessRequest(); 
}

void UOpenAIChatService::OnResponseReceived(FHttpRequestPtr Request,
                                             FHttpResponsePtr Response,
                                             bool bSuccess,
                                             FOnChatSuccess OnSuccess,
                                             FOnChatError   OnError)
{
    double ElapsedMs = (FPlatformTime::Seconds() - LastRequestTime) * 1000.0;
    UE_LOG(LogTemp, Warning,
        TEXT("[LATENCY] <<< Response received | elapsed=%.0f ms | success=%s"),
        ElapsedMs, bSuccess ? TEXT("true") : TEXT("false"));

    bRequestInFlight = false;
    ActiveRequest.Reset();

    if (!bSuccess || !Response.IsValid())
    {
        UE_LOG(LogTemp, Error,
            TEXT("[LATENCY] Request FAILED after %.0f ms"), ElapsedMs);
        OnError.ExecuteIfBound(TEXT("Network error or timeout."));
        return;
    }

    int32 StatusCode = Response->GetResponseCode();
    if (StatusCode != 200)
    {
        UE_LOG(LogTemp, Error,
            TEXT("[LATENCY] HTTP %d after %.0f ms"), StatusCode, ElapsedMs);
        OnError.ExecuteIfBound(FString::Printf(
            TEXT("HTTP %d: %s"), StatusCode, *Response->GetContentAsString()));
        return;
    }

    TSharedPtr<FJsonObject> JsonResponse;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (!FJsonSerializer::Deserialize(Reader, JsonResponse) || !JsonResponse.IsValid())
    {
        OnError.ExecuteIfBound(TEXT("Failed to parse JSON response."));
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Choices;
    if (!JsonResponse->TryGetArrayField(TEXT("choices"), Choices) ||
        Choices->Num() == 0)
    {
        OnError.ExecuteIfBound(TEXT("Empty choices in response."));
        return;
    }

    TSharedPtr<FJsonObject> FirstChoice = (*Choices)[0]->AsObject();
    TSharedPtr<FJsonObject> Message = FirstChoice->GetObjectField(TEXT("message"));
    FString Content = Message->GetStringField(TEXT("content"));

    UE_LOG(LogTemp, Warning,
        TEXT("[LATENCY] SUCCESS | elapsed=%.0f ms | responseLen=%d chars"),
        ElapsedMs, Content.Len());

    OnSuccess.ExecuteIfBound(Content.TrimStartAndEnd());
}