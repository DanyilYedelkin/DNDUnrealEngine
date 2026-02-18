// Source/AIDnD_TopDown/Public/Combat/TurnManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/CombatantInterface.h"
#include "Combat/CombatTypes.h"
#include "TurnManager.generated.h"

// ----------------------------------------------------------------
//  Structs
// ----------------------------------------------------------------

/** One entry in the initiative order */
USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FCombatantInitiative
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Initiative")
    TScriptInterface<ICombatant> Combatant;

    UPROPERTY(BlueprintReadOnly, Category = "Initiative")
    int32 InitiativeValue = 0;

    /** Raw d20 roll (for tiebreaking display) */
    UPROPERTY(BlueprintReadOnly, Category = "Initiative")
    int32 NaturalRoll = 0;

    /** DEX modifier at time of roll */
    UPROPERTY(BlueprintReadOnly, Category = "Initiative")
    int32 DexModifier = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Initiative")
    FText CombatantName;

    UPROPERTY(BlueprintReadOnly, Category = "Initiative")
    bool bIsPlayerControlled = false;
};

/** Combat state machine */
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    Inactive        UMETA(DisplayName = "Inactive"),
    RollingInit     UMETA(DisplayName = "Rolling Initiative"),
    PlayerTurn      UMETA(DisplayName = "Player Turn"),
    EnemyTurn       UMETA(DisplayName = "Enemy Turn"),
    ResolvingAction UMETA(DisplayName = "Resolving Action"),
    CombatEnd       UMETA(DisplayName = "Combat End")
};

// ----------------------------------------------------------------
//  Delegates
// ----------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundStarted, int32, RoundNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundEnded, int32, RoundNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnStarted,
    const FCombatantInitiative&, CurrentCombatant);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnEnded,
    const FCombatantInitiative&, CurrentCombatant);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInitiativeRolled,
    const FCombatantInitiative&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged,
    ECombatState, NewState);

// ----------------------------------------------------------------
//  ATurnManager
// ----------------------------------------------------------------

/**
 * Singleton actor that manages the full turn-based combat cycle.
 * Place one instance on the level. Access via GetTurnManager().
 *
 * Flow: StartCombat → RollInitiative → BeginRound → BeginTurn
 *       → [player/AI acts] → EndTurn → next combatant → ...
 *       → EndRound → BeginRound → ... → EndCombat
 */
UCLASS(BlueprintType)
class AIDND_TOPDOWN_API ATurnManager : public AActor
{
    GENERATED_BODY()

public:

    ATurnManager();

    // ============================================================
    //  SINGLETON ACCESS
    // ============================================================

    /** Returns the ATurnManager instance from the current world */
    UFUNCTION(BlueprintCallable, BlueprintPure,
        Category = "Combat|TurnManager",
        meta = (WorldContext = "WorldContextObject",
                DisplayName = "Get Turn Manager"))
    static ATurnManager* GetTurnManager(const UObject* WorldContextObject);

    // ============================================================
    //  CONFIGURATION
    // ============================================================

    /**
     * When true, player always wins initiative tiebreaks.
     * When false, tiebreak is resolved with a reroll.
     */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat|Config")
    bool bPlayerWinsTiebreak = true;

    /** Delay between enemy actions (seconds) for "thinking" feel */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat|Config",
        meta = (ClampMin = 0.f))
    float EnemyActionDelay = 0.8f;

    // ============================================================
    //  STATE (READ-ONLY)
    // ============================================================

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    ECombatState CombatState = ECombatState::Inactive;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    int32 CurrentRound = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    int32 CurrentTurnIndex = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|State")
    TArray<FCombatantInitiative> InitiativeOrder;

    // ============================================================
    //  DELEGATES
    // ============================================================

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCombatStarted OnCombatStarted;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCombatEnded OnCombatEnded;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnRoundStarted OnRoundStarted;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnRoundEnded OnRoundEnded;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnTurnStarted OnTurnStarted;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnTurnEnded OnTurnEnded;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnInitiativeRolled OnInitiativeRolled;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnCombatStateChanged OnCombatStateChanged;

    // ============================================================
    //  COMBAT LIFECYCLE
    // ============================================================

    /**
     * Start combat with a given set of combatants.
     * Triggers initiative rolls and begins round 1.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|TurnManager")
    void StartCombat(const TArray<TScriptInterface<ICombatant>>& Combatants);

    /** Register a new combatant mid-combat (e.g. summoned creature) */
    UFUNCTION(BlueprintCallable, Category = "Combat|TurnManager")
    void RegisterCombatant(TScriptInterface<ICombatant> Combatant);

    /** Remove a combatant from the order (death, flee, etc.) */
    UFUNCTION(BlueprintCallable, Category = "Combat|TurnManager")
    void UnregisterCombatant(TScriptInterface<ICombatant> Combatant);

    /** Roll initiative for all combatants and sort the order */
    UFUNCTION(BlueprintCallable, Category = "Combat|TurnManager")
    void RollInitiativeForAll();

    /** End the current combatant's turn and advance to the next */
    UFUNCTION(BlueprintCallable, Category = "Combat|TurnManager")
    void EndCurrentTurn();

    /** Force end combat (all enemies dead, fled, etc.) */
    UFUNCTION(BlueprintCallable, Category = "Combat|TurnManager")
    void EndCombat(bool bPlayerVictory);

    // ============================================================
    //  QUERIES
    // ============================================================

    /** Returns the combatant whose turn it currently is */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|TurnManager")
    FCombatantInitiative GetCurrentTurnCombatant() const;

    /** True if it's currently a player-controlled character's turn */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|TurnManager")
    bool IsPlayerTurn() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|TurnManager")
    bool IsCombatActive() const { return CombatState != ECombatState::Inactive
                                      && CombatState != ECombatState::CombatEnd; }

    /** Returns sorted initiative order for UI display */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|TurnManager")
    TArray<FCombatantInitiative> GetInitiativeOrder() const { return InitiativeOrder; }

    /** Check if all enemies or all players are dead → auto-end combat */
    UFUNCTION(BlueprintCallable, Category = "Combat|TurnManager")
    void CheckCombatEndCondition();

protected:

    virtual void BeginPlay() override;

private:

    void SetCombatState(ECombatState NewState);
    void BeginRound();
    void EndRound();
    void BeginTurn(int32 TurnIndex);

    /** Advance to next living combatant */
    void AdvanceToNextCombatant();

    /** Resolve tiebreak between two initiative entries */
    bool ResolveTiebreak(const FCombatantInitiative& A,
                         const FCombatantInitiative& B) const;

    /** Remove dead combatants from order */
    void PruneDeadCombatants();

    /** Registered combatants (before initiative sort) */
    TArray<TScriptInterface<ICombatant>> Combatants;
};