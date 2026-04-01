#include "AI/ChatNPC.h"
#include "AI/ChatManagerSubsystem.h"
#include "AI/ChatWidget.h"
#include "Components/SphereComponent.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AChatNPC::AChatNPC()
{
    InteractionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionSphere"));
    InteractionSphere->SetupAttachment(RootComponent);
    InteractionSphere->SetSphereRadius(200.f);
    InteractionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    InteractionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

    TrustComponent = CreateDefaultSubobject<UTrustComponent>(TEXT("TrustComponent"));
    TradeComponent = CreateDefaultSubobject<UNPCTradeComponent>(TEXT("TradeComponent"));
    QuestComponent = CreateDefaultSubobject<UNPCQuestComponent>(TEXT("QuestComponent"));
}

void AChatNPC::BeginPlay()
{
    Super::BeginPlay();
    InteractionSphere->SetSphereRadius(InteractionRadius);
    InteractionSphere->OnComponentBeginOverlap.AddDynamic(
        this, &AChatNPC::OnSphereBeginOverlap);
    InteractionSphere->OnComponentEndOverlap.AddDynamic(
        this, &AChatNPC::OnSphereEndOverlap);

    // Подписка на ответы Subsystem
    if (UChatManagerSubsystem* Sub = GetGameInstance()->
        GetSubsystem<UChatManagerSubsystem>())
    {
        Sub->OnNPCResponseReceived.AddDynamic(this, &AChatNPC::OnNPCResponse);
        Sub->OnNPCResponseError.AddDynamic(this, &AChatNPC::OnNPCError);
    }
}

void AChatNPC::OnSphereBeginOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (APawn* Pawn = Cast<APawn>(OtherActor))
    {
        if (Pawn->IsLocallyControlled())
        {
            OnPlayerEnterRange_Implementation(Pawn);
        }
    }
}

void AChatNPC::OnSphereEndOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32)
{
    if (APawn* Pawn = Cast<APawn>(OtherActor))
    {
        if (Pawn->IsLocallyControlled())
        {
            OnPlayerExitRange_Implementation(Pawn);
        }
    }
}

void AChatNPC::OnPlayerEnterRange_Implementation(APawn* PlayerPawn)
{
    CurrentInteractingPawn = PlayerPawn;
    // Показываем подсказку "Press E to Talk"
    if (InteractPromptWidgetClass && !PromptWidgetInstance)
    {
        APlayerController* PC = Cast<APlayerController>(PlayerPawn->GetController());
        if (PC)
        {
            PromptWidgetInstance = CreateWidget<UUserWidget>(PC, InteractPromptWidgetClass);
            if (PromptWidgetInstance)
                PromptWidgetInstance->AddToViewport();
        }
    }
}

void AChatNPC::OnPlayerExitRange_Implementation(APawn* PlayerPawn)
{
    CurrentInteractingPawn = nullptr;
    if (PromptWidgetInstance)
    {
        PromptWidgetInstance->RemoveFromParent();
        PromptWidgetInstance = nullptr;
    }
    if (bChatOpen)
        CloseChat();
}

void AChatNPC::OnInteract_Implementation(APawn* InstigatorPawn)
{
    if (bInteractCooldown) return;
    bInteractCooldown = true;

    if (!bChatOpen)
        OpenChat();
    else
        CloseChat();
    
    FTimerHandle CooldownTimer;
    GetWorldTimerManager().SetTimer(CooldownTimer, [this]()
    {
        bInteractCooldown = false;
    }, 0.3f, false);
}

void AChatNPC::OpenChat()
{
    if (bChatOpen || !ChatWidgetClass) return;
    bChatOpen = true;

    APlayerController* PC = nullptr;
    if (CurrentInteractingPawn)
        PC = Cast<APlayerController>(CurrentInteractingPawn->GetController());
    if (!PC)
        PC = UGameplayStatics::GetPlayerController(this, 0);
    if (!PC) return;

    if (InteractSound)
        UGameplayStatics::PlaySoundAtLocation(this, InteractSound, GetActorLocation());

    if (PromptWidgetInstance)
    {
        PromptWidgetInstance->RemoveFromParent();
        PromptWidgetInstance = nullptr;
    }

    ChatWidgetInstance = CreateWidget<UUserWidget>(PC, ChatWidgetClass);
    if (ChatWidgetInstance)
    {
        UChatWidget* W = Cast<UChatWidget>(ChatWidgetInstance);
        if (W)
        {
            W->InitWidget(this);
        }

        ChatWidgetInstance->AddToViewport(10);
        PC->SetInputMode(FInputModeGameAndUI());
        PC->bShowMouseCursor = true;
    }

    OnChatOpened.Broadcast();
}

void AChatNPC::CloseChat()
{
    if (!bChatOpen) return;
    bChatOpen = false;

    if (ChatWidgetInstance)
    {
        ChatWidgetInstance->RemoveFromParent();
        ChatWidgetInstance = nullptr;
    }

    APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
    if (PC)
    {
        FInputModeGameOnly InputMode;
        PC->SetInputMode(InputMode);
        PC->bShowMouseCursor = false;
    }

    OnChatClosed.Broadcast();
}

void AChatNPC::SendPlayerMessage(const FString& PlayerText)
{
    if (PlayerText.IsEmpty()) return;
    if (UChatManagerSubsystem* Sub = GetGameInstance()->
        GetSubsystem<UChatManagerSubsystem>())
    {
        Sub->SendMessage(NPCID, PlayerText, SystemPrompt);
    }
}

void AChatNPC::OnNPCResponse(const FString& ResponseNPCID, const FString& Response)
{
    if (ResponseNPCID != NPCID) return;
    OnResponseReceived.Broadcast(Response);
}

void AChatNPC::OnNPCError(const FString& Error)
{
    UE_LOG(LogTemp, Warning, TEXT("[ChatNPC] Error for %s: %s"), *NPCID, *Error);
    OnResponseReceived.Broadcast(FString::Printf(TEXT("[Ошибка: %s]"), *Error));
}

void AChatNPC::SendPlayerMessageWithContext(const FString& PlayerText)
{
    if (PlayerText.IsEmpty()) return;
    
    FString Context;

    if (TrustComponent)
        Context += TrustComponent->BuildMemoryContextString();

    if (QuestComponent)
        Context += QuestComponent->BuildQuestContextString();

    if (UChatManagerSubsystem* Sub = GetGameInstance()->
        GetSubsystem<UChatManagerSubsystem>())
    {
        Sub->SendMessageWithContext(NPCID, PlayerText, SystemPrompt, Context);
    }
}