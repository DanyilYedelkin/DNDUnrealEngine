// Source/AIDnD_TopDown/Public/Combat/AI/ChatGPTAIDecisionMaker.h
#pragma once

#include "CoreMinimal.h"
#include "Combat/AI/AIDecisionMaker.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "ChatGPTAIDecisionMaker.generated.h"

class ULocalAIDecisionMaker;

/**
 * ChatGPT-powered AI decision maker.
 * Sends battle context as JSON to OpenAI API, parses the response.
 *
 * Currently a STUB — all infrastructure is in place but the endpoint
 * is not active. Set ApiEndpoint and ApiKey in Project Settings / config
 * when ready to activate.
 *
 * On timeout → falls back to ULocalAIDecisionMaker automatically.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API UChatGPTAIDecisionMaker : public UAIDecisionMaker
{
    GENERATED_BODY()

public:

    UChatGPTAIDecisionMaker();

    /** OpenAI-compatible API endpoint */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|ChatGPT")
    FString ApiEndpoint = TEXT("https://api.openai.com/v1/chat/completions");

    /** API key — set via config, never hardcode in source */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|ChatGPT")
    FString ApiKey;

    /** Model to use */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|ChatGPT")
    FString Model = TEXT("gpt-4o");

    /** Seconds before falling back to Local AI */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|ChatGPT",
        meta = (ClampMin = 1.f))
    float TimeoutSeconds = 5.f;

    virtual void RequestDecision_Implementation(
        const FAIBattleContext& Context) override;

    /** Serialize battle context to JSON string for the API prompt */
    UFUNCTION(BlueprintCallable, Category = "AI|ChatGPT")
    static FString SerializeBattleContext(const FAIBattleContext& Context);

    /** Parse GPT JSON response into FAIDecision */
    UFUNCTION(BlueprintCallable, Category = "AI|ChatGPT")
    static FAIDecision ParseGPTResponse(const FString& JsonResponse,
        const FAIBattleContext& Context);

private:

    UFUNCTION()
    void OnFallbackDecisionReady(FAIDecision Decision);

    /** Cached context for use in response callback */
    FAIBattleContext PendingContext;

    /** Fallback AI used on timeout or parse error */
    UPROPERTY()
    TObjectPtr<ULocalAIDecisionMaker> FallbackAI;

    /** Timer handle for timeout */
    FTimerHandle TimeoutHandle;

    void OnHttpResponseReceived(FHttpRequestPtr Request,
        FHttpResponsePtr Response, bool bSuccess);

    void TriggerFallback(const FString& Reason);

    /** Build the system prompt describing the D&D 5e rules context */
    static FString BuildSystemPrompt();

    /** Build the user prompt from serialized battle context */
    static FString BuildUserPrompt(const FString& ContextJson);
};