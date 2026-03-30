// Source/AIDnD_TopDown/Public/Combat/AI/EnemyAIController.h
#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "Combat/AI/AIDecisionMaker.h"
#include "Combat/CombatTypes.h"
#include "EnemyAIController.generated.h"

class ACombatCharacter;
class ATurnManager;

// ----------------------------------------------------------------
//  Delegates
// ----------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDecisionMade,
    const FAIDecision&, Decision);

// ----------------------------------------------------------------
//  AEnemyAIController
// ----------------------------------------------------------------

/**
 * AI Controller for all enemy combatants.
 * Receives turn notifications from ATurnManager,
 * builds battle context, requests a decision from UAIDecisionMaker,
 * and executes the result in the game world.
 *
 * Swap DecisionMaker at runtime to change AI behavior:
 *   ULocalAIDecisionMaker  — heuristic (default)
 *   UChatGPTAIDecisionMaker — GPT-powered (future)
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API AEnemyAIController : public AAIController
{
    GENERATED_BODY()

public:

    AEnemyAIController();

    // ============================================================
    //  CONFIGURATION
    // ============================================================

    /**
     * Which decision maker to use.
     * Set to BP_LocalAI or BP_ChatGPTAI in the editor.
     * Default: ULocalAIDecisionMaker
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|Config")
    TSubclassOf<UAIDecisionMaker> DecisionMakerClass;

    /**
     * Delay in seconds between each action within a turn.
     * Gives the "thinking enemy" feel.
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|Config",
        meta = (ClampMin = 0.f))
    float ActionDelay = 0.8f;

    /**
     * Max actions per turn before forced EndTurn
     * (safety guard against infinite loops).
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|Config",
        meta = (ClampMin = 1))
    int32 MaxActionsPerTurn = 5;

    // ============================================================
    //  DELEGATES
    // ============================================================

    /** Fired after each decision so UI can react */
    UPROPERTY(BlueprintAssignable, Category = "AI|Events")
    FOnEnemyDecisionMade OnEnemyDecisionMade;

    // ============================================================
    //  PUBLIC API
    // ============================================================

    /**
     * Start the AI turn for the given character.
     * Called by ATurnManager via OnTurnStarted event.
     */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void StartAITurn(ACombatCharacter* InCharacter);

    /** Force-end the AI turn immediately */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void ForceEndTurn();

    /** Returns the active decision maker instance */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AI")
    UAIDecisionMaker* GetDecisionMaker() const { return DecisionMaker; }

    /**
     * Swap decision maker at runtime.
     * Example: switch from Local to ChatGPT mid-game.
     */
    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetDecisionMakerClass(TSubclassOf<UAIDecisionMaker> NewClass);

    // ============================================================
    //  BLUEPRINT EVENTS (override in BP for animations/VFX)
    // ============================================================

    /** Called when the AI starts "thinking" (requesting decision) */
    UFUNCTION(BlueprintImplementableEvent, Category = "AI|Events",
        meta = (DisplayName = "On AI Thinking"))
    void BP_OnAIThinking();

    /** Called when the AI executes a decision */
    UFUNCTION(BlueprintImplementableEvent, Category = "AI|Events",
        meta = (DisplayName = "On AI Action Executed"))
    void BP_OnActionExecuted(const FAIDecision& Decision,
        const FActionResult& Result);

    /** Called when the AI turn ends */
    UFUNCTION(BlueprintImplementableEvent, Category = "AI|Events",
        meta = (DisplayName = "On AI Turn Ended"))
    void BP_OnTurnEnded();

    // ============================================================
    //  PATROL
    // ============================================================

    /** Patrol points **/
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|Patrol")
    TArray<AActor*> PatrolPoints;

    /** Current patrol point index **/
    UPROPERTY(BlueprintReadOnly, Category = "AI|Patrol")
    int32 PatrolIndex = 0;

    /** Returns the next patrol point **/
    UFUNCTION(BlueprintCallable, Category = "AI|Patrol")
    FVector GetNextPatrolPoint() const;

    /** Moves on to the next point **/
    UFUNCTION(BlueprintCallable, Category = "AI|Patrol")
    void AdvancePatrolIndex();

    /** True while the AI takes its turn **/
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "AI")
    bool IsTurnActive() const { return bTurnActive; }

    // ============================================================
    //  BEHAVIOR TREE
    // ============================================================

    /** Behavior Tree asset **/
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|BehaviorTree")
    TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

protected:

    virtual void BeginPlay() override;
    virtual void OnPossess(APawn* InPawn) override;

private:

    UFUNCTION()
    void OnTurnStartedHandler(const FCombatantInitiative& Entry);

    UPROPERTY()
    TObjectPtr<UAIDecisionMaker> DecisionMaker;

    UPROPERTY()
    TObjectPtr<ACombatCharacter> ControlledCharacter;

    UPROPERTY()
    TObjectPtr<ATurnManager> CachedTurnManager;

    /** How many decisions have been made this turn */
    int32 ActionsThisTurn = 0;

    /** True while waiting for async decision or action delay */
    bool bTurnActive = false;

    FTimerHandle ActionDelayHandle;

    // ---- Turn flow ----

    /** Request the next decision from DecisionMaker */
    void RequestNextDecision();

    /** Called when DecisionMaker fires OnDecisionReady */
    UFUNCTION()
    void OnDecisionReceived(FAIDecision Decision);

    /** Execute the decision in the game world */
    void ExecuteDecision(const FAIDecision& Decision);

    /** Execute Move decision */
    void ExecuteMove(const FAIDecision& Decision);

    /** Execute Attack decision */
    void ExecuteAttack(const FAIDecision& Decision);

    /** Execute Dodge decision */
    void ExecuteDodge(const FAIDecision& Decision);

    /** Execute Dash decision */
    void ExecuteDash(const FAIDecision& Decision);

    /** End this AI's turn via TurnManager */
    void EndAITurn();

    /** Collect all combat characters on the level */
    TArray<ACombatCharacter*> GetAllCombatants() const;

    ATurnManager* GetTurnManager();
};