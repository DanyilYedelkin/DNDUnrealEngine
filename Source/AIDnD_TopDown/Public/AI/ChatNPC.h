#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ChatInteractable.h"
#include "NPC/UTrustComponent.h"
#include "NPC/UNPCTradeComponent.h"
#include "NPC/UNPCQuestComponent.h"
#include "ChatNPC.generated.h"

class UWidgetComponent;
class USphereComponent;
class UChatManagerSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChatOpenedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnChatClosedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnResponseReceivedDelegate,
    const FString&, ResponseText);

UCLASS(Blueprintable)
class AIDND_TOPDOWN_API AChatNPC : public ACharacter, public IChatInteractable
{
    GENERATED_BODY()
public:
    AChatNPC();

protected:
    virtual void BeginPlay() override;

public:
    // --- Настройки в Details (BP-friendly) ---

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Identity")
    FString NPCID = TEXT("NPC_Default");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Identity")
    FString NPCDisplayName = TEXT("NPC");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|AI",
              meta=(MultiLine=true))
    FString SystemPrompt = TEXT("You are a friendly NPC in a fantasy world.");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|UI")
    float InteractionRadius = 200.f;

    // Класс виджета — можно переопределить в BP
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|UI")
    TSubclassOf<UUserWidget> ChatWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|UI")
    TSubclassOf<UUserWidget> InteractPromptWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|UI")
    TSubclassOf<UUserWidget> TradeWidgetClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|UI")
    UTexture2D* NPCPortrait = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NPC|Audio")
    USoundBase* InteractSound = nullptr;

    // --- Делегаты (подписка из BP) ---
    UPROPERTY(BlueprintAssignable, Category="NPC Chat")
    FOnChatOpenedDelegate OnChatOpened;

    UPROPERTY(BlueprintAssignable, Category="NPC Chat")
    FOnChatClosedDelegate OnChatClosed;

    UPROPERTY(BlueprintAssignable, Category="NPC Chat")
    FOnResponseReceivedDelegate OnResponseReceived;

    // --- Публичные функции ---
    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void OpenChat();

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void CloseChat();

    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void SendPlayerMessage(const FString& PlayerText);

    UFUNCTION(BlueprintPure, Category="NPC Chat")
    bool IsChatOpen() const { return bChatOpen; }
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Systems")
    UTrustComponent* TrustComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Systems")
    UNPCTradeComponent* TradeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NPC|Systems")
    UNPCQuestComponent* QuestComponent;
    
    UFUNCTION(BlueprintCallable, Category="NPC Chat")
    void SendPlayerMessageWithContext(const FString& PlayerText);

    UFUNCTION(BlueprintCallable, Category="NPC Trade")
    void OpenTrade();

    UFUNCTION(BlueprintCallable, Category="NPC Trade")
    void CloseTrade();

    UFUNCTION(BlueprintPure, Category="NPC Trade")
    bool IsTradeOpen() const { return bTradeOpen; }

    UPROPERTY(BlueprintAssignable, Category="NPC Trade")
    FOnChatOpenedDelegate OnTradeOpened;

    UPROPERTY(BlueprintAssignable, Category="NPC Trade")
    FOnChatClosedDelegate OnTradeClosed;

    // IChatInteractable
    virtual void OnInteract_Implementation(APawn* InstigatorPawn) override;
    virtual void OnPlayerEnterRange_Implementation(APawn* PlayerPawn) override;
    virtual void OnPlayerExitRange_Implementation(APawn* PlayerPawn) override;

protected:
    UPROPERTY(BlueprintReadOnly, Category="NPC Trade")
    UUserWidget* TradeWidgetInstance = nullptr;
    
    UPROPERTY(VisibleAnywhere, Category="NPC")
    USphereComponent* InteractionSphere;

    UPROPERTY(BlueprintReadOnly, Category="NPC Chat")
    UUserWidget* ChatWidgetInstance = nullptr;

    UPROPERTY()
    UUserWidget* PromptWidgetInstance = nullptr;

    bool bInteractCooldown = false;
    bool bChatOpen = false;
    APawn* CurrentInteractingPawn = nullptr;

    UFUNCTION()
    void OnSphereBeginOverlap(UPrimitiveComponent* OverlappedComp,
                               AActor* OtherActor,
                               UPrimitiveComponent* OtherComp,
                               int32 OtherBodyIndex,
                               bool bFromSweep,
                               const FHitResult& SweepResult);
    UFUNCTION()
    void OnSphereEndOverlap(UPrimitiveComponent* OverlappedComp,
                             AActor* OtherActor,
                             UPrimitiveComponent* OtherComp,
                             int32 OtherBodyIndex);

    UFUNCTION()
    void OnNPCResponse(const FString& ResponseNPCID, const FString& Response);

    UFUNCTION()
    void OnNPCError(const FString& Error);

private:
    bool bTradeOpen = false;
};