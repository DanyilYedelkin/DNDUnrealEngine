// Source/AIDnD_TopDown/Public/AI/NPC/NPCTypes.h
#pragma once
#include "CoreMinimal.h"
#include "NPCTypes.generated.h"

// ============================================================
//  TRUST
// ============================================================

UENUM(BlueprintType)
enum class ENPCTrustTier : uint8
{
    Hostile    UMETA(DisplayName = "Hostile"),    // [-100, -40)
    Suspicious UMETA(DisplayName = "Suspicious"), // [-40,  -10)
    Neutral    UMETA(DisplayName = "Neutral"),    // [-10,   30)
    Friendly   UMETA(DisplayName = "Friendly"),  // [30,    70)
    Trusted    UMETA(DisplayName = "Trusted")    // [70,   100]
};

USTRUCT(BlueprintType)
struct FTrustTierConfig
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    ENPCTrustTier Tier = ENPCTrustTier::Neutral;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    int32 MaxValue = 30;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float PriceMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString PromptHint;
};

// ============================================================
//  NPC MEMORY EVENTS
// ============================================================

UENUM(BlueprintType)
enum class ENPCMemoryEventType : uint8
{
    // Positive
    PlayerHelped       UMETA(DisplayName = "Player Helped"),
    QuestCompleted     UMETA(DisplayName = "Quest Completed"),
    GiftReceived       UMETA(DisplayName = "Gift Received"),
    PaymentReceived    UMETA(DisplayName = "Payment Received"),
    // Negative
    PlayerAttacked     UMETA(DisplayName = "Player Attacked"),
    PlayerStole        UMETA(DisplayName = "Player Stole"),
    QuestFailed        UMETA(DisplayName = "Quest Failed"),
    ThreatMade         UMETA(DisplayName = "Threat Made"),
    // Neutral
    TradeCompleted     UMETA(DisplayName = "Trade Completed"),
    QuestAccepted      UMETA(DisplayName = "Quest Accepted"),
    Greeted            UMETA(DisplayName = "Greeted"),
    Custom             UMETA(DisplayName = "Custom")
};

USTRUCT(BlueprintType)
struct FNPCMemoryEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    ENPCMemoryEventType EventType = ENPCMemoryEventType::Greeted;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Description;
    
    UPROPERTY(BlueprintReadWrite)
    int32 TrustDelta = 0;

    UPROPERTY(BlueprintReadWrite)
    FDateTime Timestamp;

    FNPCMemoryEvent() : Timestamp(FDateTime::UtcNow()) {}
    FNPCMemoryEvent(ENPCMemoryEventType InType, const FString& InDesc, int32 InDelta)
        : EventType(InType), Description(InDesc), TrustDelta(InDelta)
        , Timestamp(FDateTime::UtcNow()) {}
};

// ============================================================
//  NPC RELATION DATA 
// ============================================================

USTRUCT(BlueprintType)
struct FNPCRelationData
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite)
    FString NPCID;

    UPROPERTY(BlueprintReadWrite)
    int32 TrustScore = 0; // [-100, 100]

    UPROPERTY(BlueprintReadWrite)
    TArray<FNPCMemoryEvent> MemoryEvents;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, int32> QuestStates;

    UPROPERTY(BlueprintReadWrite)
    TMap<FString, int32> PurchasedItems;
};

// ============================================================
//  QUEST STATE
// ============================================================

UENUM(BlueprintType)
enum class ENPCQuestState : uint8
{
    Locked     UMETA(DisplayName = "Locked"), 
    Available  UMETA(DisplayName = "Available"),
    Active     UMETA(DisplayName = "Active"),
    Completed  UMETA(DisplayName = "Completed"),
    Failed     UMETA(DisplayName = "Failed")
};

USTRUCT(BlueprintType)
struct FNPCQuestStep
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString StepID;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName CompletionTag;

    UPROPERTY(BlueprintReadWrite)
    bool bCompleted = false;
};

// ============================================================
//  ITEM — for purchase
// ============================================================

UENUM(BlueprintType)
enum class ENPCItemCategory : uint8
{
    Weapon    UMETA(DisplayName = "Weapon"),
    Armor     UMETA(DisplayName = "Armor"),
    Consumable UMETA(DisplayName = "Consumable"),
    Upgrade   UMETA(DisplayName = "Upgrade"),
    Quest     UMETA(DisplayName = "Quest Item"),
    Misc      UMETA(DisplayName = "Miscellaneous")
};

USTRUCT(BlueprintType)
struct FItemStatModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    FName StatName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    float Delta = 0.f;
};