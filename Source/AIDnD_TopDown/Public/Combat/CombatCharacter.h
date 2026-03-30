// Source/AIDnD_TopDown/Public/Combat/CombatCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/CombatantInterface.h"
#include "Combat/CombatTypes.h"
#include "CombatCharacter.generated.h"

class UCombatStatsComponent;
class UConditionComponent;

/**
 * Base class for all combat participants (players and enemies).
 * Implements ICombatant and owns the core combat components.
 * Extend this in Blueprints for specific characters.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API ACombatCharacter : public ACharacter, public ICombatant
{
    GENERATED_BODY()

public:

    ACombatCharacter();

    // ============================================================
    //  COMPONENTS
    // ============================================================

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCombatStatsComponent> CombatStats;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Components",
        meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UConditionComponent> ConditionComp;

    // ============================================================
    //  IDENTITY
    // ============================================================

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Identity")
    FText CharacterName;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat|Identity")
    bool bIsPlayerControlled = false;

    // ============================================================
    //  ICombatant INTERFACE IMPLEMENTATION
    // ============================================================

    virtual int32 GetInitiativeRoll_Implementation() override;
    virtual int32 GetActionPoints_Implementation() override;
    virtual int32 GetBonusActionPoints_Implementation() override;
    virtual float GetMovementRemaining_Implementation() override;
    virtual bool IsAlive_Implementation() override;
    virtual FText GetCombatantName_Implementation() override;
    virtual UCombatStatsComponent* GetCombatStats_Implementation() override;
    virtual void OnTurnStart_Implementation() override;
    virtual void OnTurnEnd_Implementation() override;
    virtual bool IsPlayerControlled_Implementation() override;

    // ============================================================
    //  BLUEPRINT EVENTS (override in BP for visuals/audio)
    // ============================================================

    /** Called when this character's turn begins — animate, highlight, etc. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Turn Started"))
    void BP_OnTurnStarted();

    /** Called when this character's turn ends */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Turn Ended"))
    void BP_OnTurnEnded();

    /** Called when this character takes damage */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Damage Taken"))
    void BP_OnDamageTaken(int32 Amount, EDamageType DamageType);

    /** Called when HP drops to 0 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Knocked Down"))
    void BP_OnKnockedDown();

    /** Called on death (3 failed death saves) */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Death"))
    void BP_OnDeath();

    /** Called when condition is applied */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Condition Applied"))
    void BP_OnConditionApplied(EConditionType Condition);

    // ============================================================
    //  COMBAT ACTIONS (BlueprintCallable for BP_PlayerController)
    // ============================================================

    /**
     * Deal damage to this character.
     * Routes through CombatStatsComponent and fires BP events.
     * @return Actual damage dealt
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    int32 ReceiveDamage(int32 Amount, EDamageType DamageType);

    /**
     * Heal this character.
     * @return Actual HP restored
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    int32 ReceiveHealing(int32 Amount);

    /**
     * Apply a status condition.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyCondition(EConditionType Condition, int32 DurationRounds = 1,
        AActor* Source = nullptr);

    // ============================================================
    //  QUERIES (BlueprintPure)
    // ============================================================

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    bool HasCondition(EConditionType Condition) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    float GetHPPercent() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    int32 GetCurrentHP() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    int32 GetMaxHP() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    int32 GetArmorClass() const;

    /** Returns advantage state for attacks made by this character */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    ERollAdvantage GetAttackAdvantage() const;

    /**
     * Returns advantage state for attacks made AGAINST this character.
     * @param bIsMeleeAttack  True for melee, false for ranged (affects Prone)
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    ERollAdvantage GetDefenseAdvantage(bool bIsMeleeAttack = true) const;

    /** Patrol points — assigned in the editor on the instance */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|Patrol")
    TArray<TObjectPtr<AActor>> PatrolPoints;

protected:

    virtual void BeginPlay() override;

private:

    UFUNCTION()
    void HandleDeath(AActor* DeadActor);
};