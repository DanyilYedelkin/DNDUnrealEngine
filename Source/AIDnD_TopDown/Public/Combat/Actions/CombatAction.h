// Source/AIDnD_TopDown/Public/Combat/Actions/CombatAction.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Combat/CombatTypes.h"
#include "CombatAction.generated.h"

class ACombatCharacter;

// ----------------------------------------------------------------
//  Preview data для подсветки UI перед исполнением
// ----------------------------------------------------------------

USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FActionPreview
{
    GENERATED_BODY()

    /** Tiles to highlight as movement range */
    UPROPERTY(BlueprintReadOnly, Category = "Preview")
    TArray<FVector> MovementTiles;

    /** Tiles to highlight as attack range */
    UPROPERTY(BlueprintReadOnly, Category = "Preview")
    TArray<FVector> AttackTiles;

    /** Tiles to highlight as spell range */
    UPROPERTY(BlueprintReadOnly, Category = "Preview")
    TArray<FVector> SpellTiles;

    /** Radius of AoE effect (0 = no AoE) */
    UPROPERTY(BlueprintReadOnly, Category = "Preview")
    float AoERadius = 0.f;

    /** Range of the action in feet */
    UPROPERTY(BlueprintReadOnly, Category = "Preview")
    float Range = 0.f;
};

// ----------------------------------------------------------------
//  UCombatAction — Command Pattern base
// ----------------------------------------------------------------

/**
 * Base class for all combat actions (Attack, Move, Spell, etc.)
 * Uses the Command pattern: each action knows how to execute itself.
 * Instantiate via NewObject<USpecificAction>() and call Execute().
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API UCombatAction : public UObject
{
    GENERATED_BODY()

public:

    // ============================================================
    //  CONFIGURATION (set in subclass defaults or BP)
    // ============================================================

    /** Display name shown in UI */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Action")
    FText ActionName;

    /** Short description for tooltip */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Action")
    FText ActionDescription;

    /** Which action economy slot this uses */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Action")
    EActionType ActionType = EActionType::Action;

    /** How many action points this costs */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Action",
        meta = (ClampMin = 0))
    int32 ActionPointCost = 1;

    /** Range in feet (0 = melee/self) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Action",
        meta = (ClampMin = 0.f))
    float Range = 5.f;

    /** Whether line-of-sight is required */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Action")
    bool bRequiresLineOfSight = true;

    // ============================================================
    //  CORE INTERFACE
    // ============================================================

    /**
     * Execute this action.
     * @param Source            Character performing the action
     * @param TargetLocation    World location of the target
     * @param TargetCharacter   Target character (may be null for ground-targeted)
     * @return Result with hit/damage/message info
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Action")
    FActionResult Execute(ACombatCharacter* Source,
                          FVector TargetLocation,
                          ACombatCharacter* TargetCharacter);

    virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
                                                  FVector TargetLocation,
                                                  ACombatCharacter* TargetCharacter);

    /**
     * Check whether this action can be executed.
     * @param OutReason  Human-readable reason if false (for UI tooltip)
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Action")
    bool CanExecute(ACombatCharacter* Source, FText& OutReason);

    virtual bool CanExecute_Implementation(ACombatCharacter* Source,
                                            FText& OutReason);

    /**
     * Returns preview data for UI highlighting before confirmation.
     */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Action")
    FActionPreview GetPreviewData(ACombatCharacter* Source);

    virtual FActionPreview GetPreviewData_Implementation(ACombatCharacter* Source);

protected:

    /** Helper: spend action points and return false if insufficient */
    bool TrySpendActionPoints(ACombatCharacter* Source);

    /** Helper: build a failed FActionResult with a message */
    static FActionResult MakeFailResult(const FString& Reason);

    /** Helper: build a successful FActionResult */
    static FActionResult MakeSuccessResult(const FString& Message,
                                            int32 Damage = 0,
                                            int32 Healing = 0);
};