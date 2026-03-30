// Source/AIDnD_TopDown/Public/Combat/CombatantInterface.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "CombatantInterface.generated.h"

class UCombatStatsComponent;

UINTERFACE(MinimalAPI, BlueprintType)
class UCombatant : public UInterface
{
    GENERATED_BODY()
};

/**
 * Interface for any actor that can participate in turn-based combat.
 * Implement this on ACombatCharacter, AEnemyCharacter, etc.
 */
class AIDND_TOPDOWN_API ICombatant
{
    GENERATED_BODY()

public:

    /** Roll and return initiative value (d20 + DEX mod) */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    int32 GetInitiativeRoll();

    /** Remaining action points this turn */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    int32 GetActionPoints();

    /** Remaining bonus action points this turn */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    int32 GetBonusActionPoints();

    /** Remaining movement in feet */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    float GetMovementRemaining();

    /** True if character is alive (not dead, may be dying) */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    bool IsAlive();

    /** Display name for combat log and UI */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    FText GetCombatantName();

    /** Returns the combat stats component */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    UCombatStatsComponent* GetCombatStats();

    /** Called when this combatant's turn begins */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    void OnTurnStart();

    /** Called when this combatant's turn ends */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    void OnTurnEnd();

    /** True if this is a player-controlled character */
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Combat|Interface")
    bool IsPlayerControlled();
};