// Source/AIDnD_TopDown/Private/NPC/TrustComponent.cpp
#include "AI/NPC/UTrustComponent.h"
#include "AI/NPC/UNPCRelationsSubsystem.h"
#include "AI/ChatNPC.h"

UTrustComponent::UTrustComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UTrustComponent::BeginPlay()
{
    Super::BeginPlay();
    
    if (AChatNPC* Owner = Cast<AChatNPC>(GetOwner()))
    {
        NPCID = Owner->NPCID;
    }

    // Subsystem
    if (UGameInstance* GI = GetWorld()->GetGameInstance())
    {
        RelationsSubsystem = GI->GetSubsystem<UNPCRelationsSubsystem>();
    }
    
    if (TierConfigs.IsEmpty())
    {
        SetupDefaultTierConfigs();
    }
    
    if (RelationsSubsystem && !NPCID.IsEmpty())
    {
        RelationsSubsystem->EnsureRelationExists(NPCID, DefaultTrust);
    }
}

void UTrustComponent::ModifyTrust(int32 Delta, ENPCMemoryEventType EventType,
                                   const FString& Reason)
{
    if (!RelationsSubsystem || NPCID.IsEmpty()) return;

    ENPCTrustTier OldTier = GetTrustTier();

    FNPCMemoryEvent Event(EventType, Reason, Delta);
    RelationsSubsystem->ModifyTrust(NPCID, Delta, Event, MaxMemoryEvents);

    int32 NewTrust = GetTrust();
    ENPCTrustTier NewTier = GetTrustTier();

    OnTrustChanged.Broadcast(NewTrust, NewTier);

    if (NewTier != OldTier)
    {
        OnTrustTierChanged.Broadcast(OldTier, NewTier);
        UE_LOG(LogTemp, Log, TEXT("[Trust] %s tier changed: %d -> %d (trust=%d)"),
            *NPCID,
            static_cast<int32>(OldTier),
            static_cast<int32>(NewTier),
            NewTrust);
    }
}

int32 UTrustComponent::GetTrust() const
{
    if (!RelationsSubsystem || NPCID.IsEmpty()) return DefaultTrust;
    return RelationsSubsystem->GetTrust(NPCID);
}

ENPCTrustTier UTrustComponent::GetTrustTier() const
{
    return ComputeTier(GetTrust());
}

float UTrustComponent::GetPriceMultiplier() const
{
    ENPCTrustTier Tier = GetTrustTier();
    if (const FTrustTierConfig* Cfg = FindTierConfig(Tier))
        return Cfg->PriceMultiplier;
    return 1.0f;
}

FString UTrustComponent::GetPromptHint() const
{
    ENPCTrustTier Tier = GetTrustTier();
    if (const FTrustTierConfig* Cfg = FindTierConfig(Tier))
        return Cfg->PromptHint;
    return TEXT("");
}

bool UTrustComponent::MeetsTrustRequirement(int32 RequiredTrust) const
{
    return GetTrust() >= RequiredTrust;
}

TArray<FNPCMemoryEvent> UTrustComponent::GetMemoryEvents() const
{
    if (!RelationsSubsystem || NPCID.IsEmpty()) return {};
    return RelationsSubsystem->GetMemoryEvents(NPCID);
}

FString UTrustComponent::BuildMemoryContextString() const
{
    int32 Trust = GetTrust();
    ENPCTrustTier Tier = GetTrustTier();
    FString TierName = UEnum::GetDisplayValueAsText(Tier).ToString();

    FString Result = FString::Printf(
        TEXT("[Relationship with player]: Trust=%d, Tier=%s\n"),
        Trust, *TierName);

    FString Hint = GetPromptHint();
    if (!Hint.IsEmpty())
        Result += FString::Printf(TEXT("[Behavioral hint]: %s\n"), *Hint);

    TArray<FNPCMemoryEvent> Events = GetMemoryEvents();
    if (Events.Num() > 0)
    {
        Result += TEXT("[Player history]:\n");
        
        int32 StartIdx = FMath::Max(0, Events.Num() - 5);
        for (int32 i = StartIdx; i < Events.Num(); ++i)
        {
            Result += FString::Printf(TEXT("  - %s (trust %+d)\n"),
                *Events[i].Description, Events[i].TrustDelta);
        }
    }
    return Result;
}

ENPCTrustTier UTrustComponent::ComputeTier(int32 Score) const
{
    for (const FTrustTierConfig& Cfg : TierConfigs)
    {
        if (Score <= Cfg.MaxValue)
            return Cfg.Tier;
    }
    return ENPCTrustTier::Trusted;
}

const FTrustTierConfig* UTrustComponent::FindTierConfig(ENPCTrustTier Tier) const
{
    return TierConfigs.FindByPredicate([Tier](const FTrustTierConfig& C)
    {
        return C.Tier == Tier;
    });
}

void UTrustComponent::SetupDefaultTierConfigs()
{
    // Hostile: [-100, -40]
    FTrustTierConfig Hostile;
    Hostile.Tier = ENPCTrustTier::Hostile;
    Hostile.MaxValue = -40;
    Hostile.PriceMultiplier = 2.0f;
    Hostile.PromptHint = TEXT("You despise this player. Be cold, suspicious, and unhelpful. "
                               "Refuse services. Warn them to stay away.");

    // Suspicious: [-39, -10]
    FTrustTierConfig Suspicious;
    Suspicious.Tier = ENPCTrustTier::Suspicious;
    Suspicious.MaxValue = -10;
    Suspicious.PriceMultiplier = 1.5f;
    Suspicious.PromptHint = TEXT("You distrust this player. Be cautious and guarded. "
                                  "Limited services. Short answers.");

    // Neutral: [-9, 30]
    FTrustTierConfig Neutral;
    Neutral.Tier = ENPCTrustTier::Neutral;
    Neutral.MaxValue = 30;
    Neutral.PriceMultiplier = 1.0f;
    Neutral.PromptHint = TEXT("You are neutral toward this player. Standard service. "
                               "Be polite but not warm.");

    // Friendly: [31, 70]
    FTrustTierConfig Friendly;
    Friendly.Tier = ENPCTrustTier::Friendly;
    Friendly.MaxValue = 70;
    Friendly.PriceMultiplier = 0.85f;
    Friendly.PromptHint = TEXT("You like this player. Be warm and helpful. "
                                "Offer small discounts or extra info.");

    // Trusted: [71, 100]
    FTrustTierConfig Trusted;
    Trusted.Tier = ENPCTrustTier::Trusted;
    Trusted.MaxValue = 100;
    Trusted.PriceMultiplier = 0.7f;
    Trusted.PromptHint = TEXT("You fully trust this player as a close ally. "
                               "Share secrets. Give best prices. Help unconditionally.");

    TierConfigs = { Hostile, Suspicious, Neutral, Friendly, Trusted };
}