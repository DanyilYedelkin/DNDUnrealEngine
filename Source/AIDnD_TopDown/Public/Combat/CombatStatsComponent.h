// Source/AIDnD_TopDown/Public/Combat/CombatStatsComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Combat/CombatTypes.h"
#include "CombatStatsComponent.generated.h"

// Forward declarations
class UCombatLogger;

// ----------------------------------------------------------------
//  Delegates
// ----------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHPChanged,
    int32, NewHP, int32, MaxHP);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath,
    AActor*, DeadActor);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDeathSavingThrow,
    bool, bSuccess, int32, TotalSuccesses);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnStabilized);

// ----------------------------------------------------------------
//  Death Save State
// ----------------------------------------------------------------

USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FDeathSaveState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Death Saves")
    int32 Successes = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Death Saves")
    int32 Failures = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Death Saves")
    bool bIsDying = false;      // HP == 0, rolling death saves

    UPROPERTY(BlueprintReadOnly, Category = "Death Saves")
    bool bIsDead = false;       // 3 failures

    UPROPERTY(BlueprintReadOnly, Category = "Death Saves")
    bool bIsStabilized = false; // 3 successes or stabilized by ally

    void Reset()
    {
        Successes   = 0;
        Failures    = 0;
        bIsDying    = false;
        bIsDead     = false;
        bIsStabilized = false;
    }
};

// ----------------------------------------------------------------
//  Resistance / Immunity / Vulnerability
// ----------------------------------------------------------------

UENUM(BlueprintType)
enum class EDamageAffinity : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    Resistant   UMETA(DisplayName = "Resistant"),   // half damage
    Immune      UMETA(DisplayName = "Immune"),       // no damage
    Vulnerable  UMETA(DisplayName = "Vulnerable")    // double damage
};

USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FDamageAffinityEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat")
    EDamageType DamageType = EDamageType::Bludgeoning;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat")
    EDamageAffinity Affinity = EDamageAffinity::Normal;
};

// ----------------------------------------------------------------
//  UCombatStatsComponent
// ----------------------------------------------------------------

/**
 * Core D&D 5e combat statistics component.
 * Attach to any actor that participates in combat.
 */
UCLASS(ClassGroup = "Combat", meta = (BlueprintSpawnableComponent))
class AIDND_TOPDOWN_API UCombatStatsComponent : public UActorComponent
{
    GENERATED_BODY()

public:

    UCombatStatsComponent();

    // ============================================================
    //  PROPERTIES
    // ============================================================

    /** D&D 5e Ability Scores */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Abilities")
    FDnDStats AbilityScores;

    /** Character level — affects ProficiencyBonus */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Level",
        meta = (ClampMin = 1, ClampMax = 20))
    int32 CharacterLevel = 1;

    // --- Hit Points ---

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|HP",
        meta = (ClampMin = 0))
    int32 MaxHP = 10;

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Combat|HP")
    int32 CurrentHP = 10;

    /** Temporary HP — absorbs damage first, doesn't stack */
    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Combat|HP")
    int32 TempHP = 0;

    // --- Armor Class ---

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Defense",
        meta = (ClampMin = 1))
    int32 ArmorClass = 10;

    // --- Speed ---

    /** Movement speed in feet (1 foot = 1 UU in our system) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Movement",
        meta = (ClampMin = 0))
    float Speed = 30.f;

    // --- Action Economy ---

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat|Actions",
        meta = (ClampMin = 0, ClampMax = 3))
    int32 MaxActions = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Actions")
    int32 CurrentActions = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat|Actions",
        meta = (ClampMin = 0, ClampMax = 1))
    int32 MaxBonusActions = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Actions")
    int32 CurrentBonusActions = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat|Actions",
        meta = (ClampMin = 0, ClampMax = 1))
    int32 MaxReactions = 1;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Actions")
    int32 CurrentReactions = 1;

    /** Remaining movement this turn (in feet) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat|Movement")
    float MovementRemaining = 30.f;

    // --- Damage Affinities ---

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Defense")
    TArray<FDamageAffinityEntry> DamageAffinities;

    // --- Proficiency ---

    /** Override: if 0, auto-computed from CharacterLevel */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Proficiency",
        meta = (ClampMin = 0))
    int32 ProficiencyBonusOverride = 0;

    /** Saving throw proficiencies */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Combat|Proficiency")
    TArray<EAbilityType> SavingThrowProficiencies;

    // --- Death Saves ---

    UPROPERTY(BlueprintReadOnly, SaveGame, Category = "Combat|Death")
    FDeathSaveState DeathSaveState;

    // ============================================================
    //  DELEGATES
    // ============================================================

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnHPChanged OnHPChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnDeath OnDeath;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnDeathSavingThrow OnDeathSavingThrow;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FOnStabilized OnStabilized;

    // ============================================================
    //  COMPUTED GETTERS
    // ============================================================

    /** Returns proficiency bonus (override or auto from level) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Proficiency")
    int32 GetProficiencyBonus() const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Abilities")
    int32 GetAbilityModifier(EAbilityType Ability) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Defense")
    EDamageAffinity GetDamageAffinity(EDamageType DamageType) const;

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|HP")
    bool IsAlive() const { return !DeathSaveState.bIsDead && (CurrentHP > 0 || DeathSaveState.bIsDying); }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|HP")
    bool IsConscious() const { return CurrentHP > 0; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|HP")
    float GetHPPercent() const;

    // ============================================================
    //  HP MODIFICATION
    // ============================================================

    /**
     * Apply damage to this character.
     * Accounts for TempHP, resistances, immunities, vulnerabilities.
     * @return Actual damage dealt after all modifiers
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|HP")
    int32 TakeDamage(int32 Amount, EDamageType DamageType);

    /**
     * Heal this character.
     * @return Actual HP restored
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|HP")
    int32 HealHP(int32 Amount);

    /**
     * Set temporary HP. Does not stack — takes the higher value.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|HP")
    void SetTempHP(int32 Amount);

    // ============================================================
    //  D&D 5e ROLLS
    // ============================================================

    /**
     * Perform a D&D 5e saving throw.
     * @param Ability   Which ability to use
     * @param DC        Difficulty Class to beat
     * @param Advantage Roll advantage state
     * @return true if the save succeeds (roll + modifier >= DC)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Rolls")
    bool SavingThrow(EAbilityType Ability, int32 DC,
        ERollAdvantage Advantage = ERollAdvantage::Normal);

    /**
     * Perform an ability check.
     * @return Full dice result including total with modifier
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Rolls")
    FDiceResult AbilityCheck(EAbilityType Ability,
        ERollAdvantage Advantage = ERollAdvantage::Normal);

    /**
     * Roll a death saving throw (D&D 5e rules).
     * Called automatically when CurrentHP == 0 at turn start.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Death")
    void RollDeathSavingThrow();

    /**
     * Stabilize this character (no more death saves needed).
     * Called when healed or stabilized by ally.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Death")
    void Stabilize();

    // ============================================================
    //  TURN MANAGEMENT
    // ============================================================

    /** Called at the start of this character's turn — resets action economy */
    UFUNCTION(BlueprintCallable, Category = "Combat|Turn")
    void OnTurnStart();

    /** Called at the end of this character's turn */
    UFUNCTION(BlueprintCallable, Category = "Combat|Turn")
    void OnTurnEnd();

    /** Spend an action. Returns false if not enough actions. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Actions")
    bool SpendAction(EActionType ActionType, int32 Cost = 1);

    /** Spend movement. Returns false if not enough remaining. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Movement")
    bool SpendMovement(float Feet);

    /** Add extra movement (e.g. from Dash action) */
    UFUNCTION(BlueprintCallable, Category = "Combat|Movement")
    void AddMovement(float Feet);

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Actions")
    bool HasActionsRemaining() const { return CurrentActions > 0; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Actions")
    bool HasBonusActionRemaining() const { return CurrentBonusActions > 0; }

    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Actions")
    bool HasReactionRemaining() const { return CurrentReactions > 0; }

protected:

    virtual void BeginPlay() override;

private:

    /** Apply affinity multiplier to raw damage amount */
    int32 ApplyDamageAffinity(int32 RawDamage, EDamageType DamageType) const;

    /** Handle dropping to 0 HP */
    void HandleZeroHP();
};