// Source/AIDnD_TopDown/Public/NPC/NPCTradeComponent.h
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/NPC/NPCTypes.h"
#include "UNPCTradeComponent.generated.h"

class UItemDataAsset;
class UTrustComponent;

USTRUCT(BlueprintType)
struct FTradeSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    TSoftObjectPtr<UItemDataAsset> Item;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin=-1))
    int32 Stock = -1;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly)
    bool bVisibleWhenLocked = true;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnItemPurchased,
    const FString&, ItemID, int32, FinalPrice);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTradeFailed,
    const FString&, Reason);

UCLASS(ClassGroup="NPC", meta=(BlueprintSpawnableComponent))
class AIDND_TOPDOWN_API UNPCTradeComponent : public UActorComponent
{
    GENERATED_BODY()
public:

    UNPCTradeComponent();
    virtual void BeginPlay() override;

    // ============================================================
    //  CONFIG
    // ============================================================

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trade|Inventory")
    TArray<FTradeSlot> TradeInventory;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trade|Config")
    bool bCanBuyFromPlayer = false;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trade|Config",
              meta=(ClampMin=0.0f, ClampMax=1.0f))
    float BuybackMultiplier = 0.5f;

    // ============================================================
    //  PUBLIC API
    // ============================================================
    
    UFUNCTION(BlueprintPure, Category="Trade")
    int32 GetFinalPrice(const UItemDataAsset* Item) const;
    
    UFUNCTION(BlueprintCallable, Category="Trade")
    bool TryPurchase(const FString& ItemID, int32 PlayerGold,
                     FString& OutFailReason);
    
    UFUNCTION(BlueprintCallable, Category="Trade")
    TArray<UItemDataAsset*> GetAvailableItems() const;
    
    UFUNCTION(BlueprintPure, Category="Trade")
    const TArray<FTradeSlot>& GetAllSlots() const { return TradeInventory; }
    
    UFUNCTION(BlueprintPure, Category="Trade")
    bool IsItemAlreadyPurchased(const FString& ItemID) const;

    // ============================================================
    //  DELEGATES
    // ============================================================

    UPROPERTY(BlueprintAssignable, Category="Trade|Events")
    FOnItemPurchased OnItemPurchased;

    UPROPERTY(BlueprintAssignable, Category="Trade|Events")
    FOnTradeFailed OnTradeFailed;

private:
    
    UPROPERTY()
    UTrustComponent* TrustComp = nullptr;

    UPROPERTY()
    class UNPCRelationsSubsystem* RelationsSubsystem = nullptr;

    FString OwnerNPCID;

    UTrustComponent* GetTrustComp() const;
    UItemDataAsset* ResolveItem(const FTradeSlot& Slot) const;
};