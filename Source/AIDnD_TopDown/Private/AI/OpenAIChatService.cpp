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
    // TODO: вынести в конфиг или параметр
    ModelName = TEXT("gpt-4o-mini");
    EndpointURL = TEXT("https://api.openai.com/v1/chat/completions");
}

FString UOpenAIChatService::LoadAPIKeyFromConfig() const
{
    // Читаем из DefaultGame.ini секции [OpenAI]
    // В репозиторий кладём ТОЛЬКО заглушку, реальный ключ — в локальный
    // Config/DefaultGame.ini или переменную окружения (см. секцию безопасности)
    FString Key;
    GConfig->GetString(TEXT("OpenAI"), TEXT("APIKey"), Key, GGameIni);
    if (Key.IsEmpty())
    {
        // Fallback: переменная окружения OPENAI_API_KEY
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

    // ЗАМЕНИ ВСЕ ПРОВЕРКИ В НАЧАЛЕ НА ЭТО:
    if (bRequestInFlight)
    {
        UE_LOG(LogTemp, Warning, TEXT("Forcing reset of stuck request"));
        bRequestInFlight = false; // принудительно сбрасываем
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
    LastRequestTime = Now;

    // Сериализация JSON
    TSharedPtr<FJsonObject> RequestBody = BuildRequestBody(Messages);
    FString BodyString;
    auto Writer = TJsonWriterFactory<>::Create(&BodyString);
    FJsonSerializer::Serialize(RequestBody.ToSharedRef(), Writer);

    // HTTP запрос
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

    // test...
    UE_LOG(LogTemp, Warning, TEXT("=== SENDING HTTP REQUEST TO: %s ==="), *EndpointURL);
    UE_LOG(LogTemp, Warning, TEXT("=== API KEY LENGTH: %d ==="), APIKey.Len());
    UE_LOG(LogTemp, Warning, TEXT("=== BODY LENGTH: %d chars ==="), BodyString.Len());
    HttpRequest->ProcessRequest();
    
    HttpRequest->ProcessRequest();
}

void UOpenAIChatService::OnResponseReceived(FHttpRequestPtr Request,
                                             FHttpResponsePtr Response,
                                             bool bSuccess,
                                             FOnChatSuccess OnSuccess,
                                             FOnChatError   OnError)
{
    bRequestInFlight = false;
    ActiveRequest.Reset();

    if (!bSuccess || !Response.IsValid())
    {
        OnError.ExecuteIfBound(TEXT("Network error or timeout."));
        return;
    }

    int32 StatusCode = Response->GetResponseCode();
    if (StatusCode != 200)
    {
        OnError.ExecuteIfBound(FString::Printf(
            TEXT("HTTP %d: %s"), StatusCode, *Response->GetContentAsString()));
        return;
    }

    // Парсинг ответа
    TSharedPtr<FJsonObject> JsonResponse;
    TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Response->GetContentAsString());

    if (!FJsonSerializer::Deserialize(Reader, JsonResponse) || !JsonResponse.IsValid())
    {
        OnError.ExecuteIfBound(TEXT("Failed to parse JSON response."));
        return;
    }

    // Извлекаем: choices[0].message.content
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

    OnSuccess.ExecuteIfBound(Content.TrimStartAndEnd());
}