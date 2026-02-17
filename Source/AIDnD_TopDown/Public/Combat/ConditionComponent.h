// Source/AIDnD_TopDown/Public/Combat/ConditionComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "ConditionComponent.generated.h"

// ----------------------------------------------------------------
//  Delegates
// ----------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnConditionApplied,
    EConditionType, Condition, int32, DurationRounds);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnConditionRemoved,
    EConditionType, Condition);

// ----------------------------------------------------------------
//  Active Condition Instance
// ----------------------------------------------------------------

USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FActiveCondition
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Condition")
    EConditionType Type = EConditionType::None;

    /** Rounds remaining. -1 = permanent until manually removed */
    UPROPERTY(BlueprintReadOnly, Category = "Condition")
    int32 DurationRounds = 1;

    /** Round when this condition was applied */
    UPROPERTY(BlueprintReadOnly, Category = "Condition")
    int32 AppliedOnRound = 0;

    /** Source actor that applied this condition (for Frightened etc.) */
    UPROPERTY(BlueprintReadOnly, Category = "Condition")
    TWeakObjectPtr<AActor> Source;

    bool IsExpired() const
    {
        return DurationRounds == 0;
    }
};

// ----------------------------------------------------------------
//  UConditionComponent
// ----------------------------------------------------------------

/**
 * Manages D&D 5e conditions (status effects) on a combatant.
 * Handles automatic expiry, roll modifiers, and turn effects.
 */
UCLASS(ClassGroup = "Combat", meta = (BlueprintSpawnableComponent))
class AIDND_TOPDOWN_API UConditionComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UConditionComponent();

    // ============================================================
    //  DELEGATES
    // ============================================================

    UPROPERTY(BlueprintAssignable, Category = "Combat|Conditions")
    FOnConditionApplied OnConditionApplied;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Conditions")
    FOnConditionRemoved OnConditionRemoved;

    // ============================================================
    //  APPLY / REMOVE
    // ============================================================

    /**
     * Apply a condition to this actor.
     * @param Duration  Rounds remaining (-1 = permanent)
     * @param Source    Actor that applied the condition
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Conditions")
    void ApplyCondition(EConditionType Condition, int32 Duration = 1,
        AActor* Source = nullptr);

    /** Remove a condition immediately */
    UFUNCTION(BlueprintCallable, Category = "Combat|Conditions")
    void RemoveCondition(EConditionType Condition);

    /** Remove all conditions */
    UFUNCTION(BlueprintCallable, Category = "Combat|Conditions")
    void RemoveAllConditions();

    // ============================================================
    //  QUERIES
    // ============================================================

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Conditions")
    bool HasCondition(EConditionType Condition) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Conditions")
    TArray<FActiveCondition> GetActiveConditions() const { return ActiveConditions; }

    /** Returns all conditions as a readable string (for UI/debug) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Conditions")
    FString GetConditionSummary() const;

    // ============================================================
    //  ROLL MODIFIERS
    // ============================================================

    /**
     * Returns the advantage state for an attack MADE BY this actor.
     * Combines all active condition modifiers.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Conditions")
    ERollAdvantage GetAttackRollModifier() const;

    /**
     * Returns the advantage state for attacks made AGAINST this actor.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Conditions")
    ERollAdvantage GetDefenseRollModifier() const;

    /**
     * Returns ability check advantage modifier from conditions.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Conditions")
    ERollAdvantage GetAbilityCheckModifier() const;

    /** True if this actor cannot take actions (Stunned, Paralyzed) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Conditions")
    bool IsIncapacitated() const;

    // ============================================================
    //  TURN LIFECYCLE
    // ============================================================

    /**
     * Called at the start of this actor's turn.
     * Applies per-turn condition effects (Bleeding, Burning etc.)
     * Does NOT decrement duration — that happens at turn END.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Conditions")
    void OnTurnStart(int32 CurrentRound);

    /**
     * Called at the end of this actor's turn.
     * Decrements duration and removes expired conditions.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Conditions")
    void OnTurnEnd();

private:

    UPROPERTY()
    TArray<FActiveCondition> ActiveConditions;

    /** Combine two advantage states into a single result */
    static ERollAdvantage CombineAdvantage(ERollAdvantage A, ERollAdvantage B);
};