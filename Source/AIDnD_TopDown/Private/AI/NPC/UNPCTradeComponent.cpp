// Source/AIDnD_TopDown/Private/NPC/NPCTradeComponent.cpp
#include "AI/NPC/UNPCTradeComponent.h"
#include "AI/NPC/UTrustComponent.h"
#include "AI/NPC/UNPCRelationsSubsystem.h"
#include "AI/NPC/UItemDataAsset.h"
#include "AI/ChatNPC.h"

UNPCTradeComponent::UNPCTradeComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNPCTradeComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AChatNPC* Owner = Cast<AChatNPC>(GetOwner()))
        OwnerNPCID = Owner->NPCID;

    if (UGameInstance* GI = GetWorld()->GetGameInstance())
        RelationsSubsystem = GI->GetSubsystem<UNPCRelationsSubsystem>();
}

int32 UNPCTradeComponent::GetFinalPrice(const UItemDataAsset* Item) const
{
    if (!Item) return 0;

    float Multiplier = 1.0f;
    if (UTrustComponent* TC = GetTrustComp())
        Multiplier = TC->GetPriceMultiplier();

    return FMath::CeilToInt(Item->BasePrice * Multiplier);
}

bool UNPCTradeComponent::TryPurchase(const FString& ItemID, int32 PlayerGold,
                                      FString& OutFailReason)
{
    FTradeSlot* FoundSlot = TradeInventory.FindByPredicate(
        [&ItemID](const FTradeSlot& Slot)
        {
            UItemDataAsset* Asset = Slot.Item.Get();
            return Asset && Asset->ItemID == ItemID;
        });

    if (!FoundSlot)
    {
        OutFailReason = TEXT("Item not found in inventory.");
        OnTradeFailed.Broadcast(OutFailReason);
        return false;
    }

    UItemDataAsset* Item = FoundSlot->Item.Get();
    if (!Item)
    {
        Item = FoundSlot->Item.LoadSynchronous();
    }
    if (!Item)
    {
        OutFailReason = TEXT("Item data asset could not be loaded.");
        OnTradeFailed.Broadcast(OutFailReason);
        return false;
    }
    
    UTrustComponent* TC = GetTrustComp();
    if (TC && !TC->MeetsTrustRequirement(Item->RequiredTrust))
    {
        OutFailReason = FString::Printf(
            TEXT("Not enough trust. Required: %d, Current: %d"),
            Item->RequiredTrust, TC->GetTrust());
        OnTradeFailed.Broadcast(OutFailReason);
        return false;
    }
    
    if (Item->bIsUnique && IsItemAlreadyPurchased(ItemID))
    {
        OutFailReason = TEXT("You have already purchased this item.");
        OnTradeFailed.Broadcast(OutFailReason);
        return false;
    }
    
    if (FoundSlot->Stock == 0)
    {
        OutFailReason = TEXT("Out of stock.");
        OnTradeFailed.Broadcast(OutFailReason);
        return false;
    }

    int32 FinalPrice = GetFinalPrice(Item);
    if (PlayerGold < FinalPrice)
    {
        OutFailReason = FString::Printf(
            TEXT("Not enough gold. Price: %d, You have: %d"), FinalPrice, PlayerGold);
        OnTradeFailed.Broadcast(OutFailReason);
        return false;
    }
    
    if (FoundSlot->Stock > 0)
        FoundSlot->Stock--;
    
    if (RelationsSubsystem)
        RelationsSubsystem->RecordPurchase(OwnerNPCID, ItemID);
    
    if (TC)
    {
        TC->ModifyTrust(1, ENPCMemoryEventType::TradeCompleted,
            FString::Printf(TEXT("Purchased '%s' for %d gold"),
                *Item->DisplayName.ToString(), FinalPrice));
    }

    OnItemPurchased.Broadcast(ItemID, FinalPrice);
    return true;
}

TArray<UItemDataAsset*> UNPCTradeComponent::GetAvailableItems() const
{
    TArray<UItemDataAsset*> Result;
    UTrustComponent* TC = GetTrustComp();

    for (const FTradeSlot& Slot : TradeInventory)
    {
        UItemDataAsset* Item = Slot.Item.Get();
        if (!Item) continue;
        if (Slot.Stock == 0) continue;
        if (TC && !TC->MeetsTrustRequirement(Item->RequiredTrust)) continue;
        if (Item->bIsUnique && IsItemAlreadyPurchased(Item->ItemID)) continue;
        Result.Add(Item);
    }
    return Result;
}

bool UNPCTradeComponent::IsItemAlreadyPurchased(const FString& ItemID) const
{
    if (!RelationsSubsystem) return false;
    return RelationsSubsystem->WasItemPurchased(OwnerNPCID, ItemID);
}

UTrustComponent* UNPCTradeComponent::GetTrustComp() const
{
    if (TrustComp) return TrustComp;
    if (AActor* Owner = GetOwner())
    {
        const_cast<UNPCTradeComponent*>(this)->TrustComp =
            Owner->FindComponentByClass<UTrustComponent>();
    }
    return TrustComp;
}

UItemDataAsset* UNPCTradeComponent::ResolveItem(const FTradeSlot& Slot) const
{
    if (Slot.Item.IsValid()) return Slot.Item.Get();
    return Slot.Item.LoadSynchronous();
}