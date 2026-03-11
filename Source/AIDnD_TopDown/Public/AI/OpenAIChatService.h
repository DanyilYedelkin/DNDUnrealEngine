#pragma once
#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "AI/ChatTypes.h"
#include "Interfaces/IHttpRequest.h"
#include "OpenAIChatService.generated.h"

DECLARE_DELEGATE_OneParam(FOnChatSuccess, const FString& /*Response*/);
DECLARE_DELEGATE_OneParam(FOnChatError,   const FString& /*Error*/);

UCLASS()
class AIDND_TOPDOWN_API UOpenAIChatService : public UObject
{
	GENERATED_BODY()
public:
	void Initialize();

	void SendChatRequest(const TArray<FChatMessage>& Messages,
						 FOnChatSuccess OnSuccess,
						 FOnChatError   OnError);

	// Cooldown в секундах между запросами на один NPC (анти-спам)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="OpenAI")
	float CooldownSeconds = 2.0f;

private:
	FString APIKey;
	FString ModelName;      // TODO: задать модель, например "gpt-4o-mini"
	FString EndpointURL;    // TODO: "https://api.openai.com/v1/chat/completions"

	double LastRequestTime = 0.0;
	bool bRequestInFlight = false;

	void OnResponseReceived(FHttpRequestPtr Request,
							FHttpResponsePtr Response,
							bool bSuccess,
							FOnChatSuccess OnSuccess,
							FOnChatError OnError);

	FString LoadAPIKeyFromConfig() const;
	TSharedPtr<FJsonObject> BuildRequestBody(const TArray<FChatMessage>& Messages) const;
};