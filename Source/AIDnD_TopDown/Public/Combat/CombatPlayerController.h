// Source/AIDnD_TopDown/Public/Combat/CombatPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Combat/CombatTypes.h"
#include "Combat/TurnManager.h"
#include "CombatPlayerController.generated.h"

class ACombatCharacter;
class UCombatAction;

/**
 * C++ base for BP_PlayerController.
 * Handles combat mode switching and exposes BlueprintCallable methods
 * that BP_PlayerController can call.
 *
 * IMPORTANT: Movement logic stays in BP_PlayerController — do not move it here.
 * This class only adds combat layer on top.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API ACombatPlayerController : public APlayerController
{
    GENERATED_BODY()

public:

    ACombatPlayerController();

    // ============================================================
    //  STATE
    // ============================================================

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    bool bInCombatMode = false;

    /** Currently selected player character (for multi-character parties) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    TObjectPtr<ACombatCharacter> SelectedCharacter;

    /** Action queued for execution (set via SelectAction) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    TSubclassOf<UCombatAction> SelectedActionClass;

    /** Target location confirmed by player */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    FVector ConfirmedMoveTarget = FVector::ZeroVector;

    // ============================================================
    //  BLUEPRINT CALLABLE — used by BP_PlayerController
    // ============================================================

    /**
     * Select which character the player is controlling this turn.
     * Safe to call from BP_PlayerController.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SelectCharacter(ACombatCharacter* InCharacter);

    /** Атаковать цель ближним боем */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    FActionResult AttackTarget(ACombatCharacter* Target);

    /**
     * Queue an action to be executed on ConfirmAction.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SelectAction(TSubclassOf<UCombatAction> ActionClass);

    /**
     * Confirm a move to TargetLocation.
     * Deducts movement from CombatStats — actual movement handled in BP.
     * @return True if move is valid and movement was deducted
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool ConfirmMove(FVector TargetLocation, float CostInFeet);

    /**
     * End the current player's turn.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EndTurn();

    /**
     * Returns the character whose turn it currently is.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    ACombatCharacter* GetCurrentTurnCharacter() const;

    /**
     * Returns true if it's currently the player's turn.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat")
    bool IsMyTurn() const;

    /**
     * Enter combat mode — called when ATurnManager starts combat.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EnterCombatMode();

    /**
     * Exit combat mode — called when ATurnManager ends combat.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ExitCombatMode();

    // ============================================================
    //  BLUEPRINT IMPLEMENTABLE EVENTS
    //  Override these in BP_PlayerController for UI/visual response
    // ============================================================

    /** Called when entering combat — show combat UI, disable free movement */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Enter Combat Mode"))
    void OnEnterCombatMode();

    /** Called when exiting combat — hide combat UI, re-enable free movement */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Exit Combat Mode"))
    void OnExitCombatMode();

    /** Called when this player's turn starts */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Turn Started"))
    void OnTurnStarted(ACombatCharacter* InCharacter);

    /** Called after any action is executed — update UI */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Action Executed"))
    void OnActionExecuted(const FActionResult& Result);

    /** Called after any dice roll — trigger dice animation in UI */
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Events",
        meta = (DisplayName = "On Dice Rolled"))
    void OnDiceRolled(const FDiceResult& Result);

protected:

    virtual void BeginPlay() override;

private:

    UPROPERTY()
    TObjectPtr<ATurnManager> CachedTurnManager;

    ATurnManager* GetTurnManager() const;

    // TurnManager event handlers
    UFUNCTION()
    void HandleTurnStarted(const FCombatantInitiative& CombatantEntry);

    UFUNCTION()
    void HandleCombatEnded();
};