// Source/AIDnD_TopDown/Public/NPC/TrustComponent.h
#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AI/NPC/NPCTypes.h"
#include "UTrustComponent.generated.h"

class UNPCRelationsSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrustChanged,
    int32, NewTrust, ENPCTrustTier, NewTier);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTrustTierChanged,
    ENPCTrustTier, OldTier, ENPCTrustTier, NewTier);

UCLASS(ClassGroup="NPC", meta=(BlueprintSpawnableComponent))
class AIDND_TOPDOWN_API UTrustComponent : public UActorComponent
{
    GENERATED_BODY()
public:

    UTrustComponent();

    virtual void BeginPlay() override;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trust|Config")
    FString NPCID; 
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trust|Config")
    TArray<FTrustTierConfig> TierConfigs;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trust|Config",
              meta=(ClampMin=-100, ClampMax=100))
    int32 DefaultTrust = 0;
    
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Trust|Config",
              meta=(ClampMin=5, ClampMax=100))
    int32 MaxMemoryEvents = 20;

    // ============================================================
    //  PUBLIC API
    // ============================================================

    UFUNCTION(BlueprintCallable, Category="Trust")
    void ModifyTrust(int32 Delta, ENPCMemoryEventType EventType,
                     const FString& Reason);

    UFUNCTION(BlueprintPure, Category="Trust")
    int32 GetTrust() const;

    UFUNCTION(BlueprintPure, Category="Trust")
    ENPCTrustTier GetTrustTier() const;
    
    UFUNCTION(BlueprintPure, Category="Trust")
    float GetPriceMultiplier() const;
    
    UFUNCTION(BlueprintPure, Category="Trust")
    FString GetPromptHint() const;

    UFUNCTION(BlueprintPure, Category="Trust")
    bool MeetsTrustRequirement(int32 RequiredTrust) const;
    
    UFUNCTION(BlueprintPure, Category="Trust")
    TArray<FNPCMemoryEvent> GetMemoryEvents() const;
    
    UFUNCTION(BlueprintPure, Category="Trust")
    FString BuildMemoryContextString() const;

    // ============================================================
    //  DELEGATES
    // ============================================================

    UPROPERTY(BlueprintAssignable, Category="Trust|Events")
    FOnTrustChanged OnTrustChanged;

    UPROPERTY(BlueprintAssignable, Category="Trust|Events")
    FOnTrustTierChanged OnTrustTierChanged;

private:

    UPROPERTY()
    UNPCRelationsSubsystem* RelationsSubsystem = nullptr;

    ENPCTrustTier ComputeTier(int32 Score) const;
    const FTrustTierConfig* FindTierConfig(ENPCTrustTier Tier) const;
    void SetupDefaultTierConfigs();
};