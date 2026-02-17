// Source/YourProject/Public/Combat/CombatTypes.h
#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CombatTypes.generated.h"

// ============================================================
//  ENUMS
// ============================================================

/** D&D 5e ability scores */
UENUM(BlueprintType)
enum class EAbilityType : uint8
{
    Strength        UMETA(DisplayName = "Strength"),
    Dexterity       UMETA(DisplayName = "Dexterity"),
    Constitution    UMETA(DisplayName = "Constitution"),
    Intelligence    UMETA(DisplayName = "Intelligence"),
    Wisdom          UMETA(DisplayName = "Wisdom"),
    Charisma        UMETA(DisplayName = "Charisma")
};

/** All D&D 5e damage types */
UENUM(BlueprintType)
enum class EDamageType : uint8
{
    Slashing    UMETA(DisplayName = "Slashing"),
    Piercing    UMETA(DisplayName = "Piercing"),
    Bludgeoning UMETA(DisplayName = "Bludgeoning"),
    Fire        UMETA(DisplayName = "Fire"),
    Cold        UMETA(DisplayName = "Cold"),
    Lightning   UMETA(DisplayName = "Lightning"),
    Poison      UMETA(DisplayName = "Poison"),
    Psychic     UMETA(DisplayName = "Psychic"),
    Necrotic    UMETA(DisplayName = "Necrotic"),
    Radiant     UMETA(DisplayName = "Radiant"),
    Thunder     UMETA(DisplayName = "Thunder"),
    Force       UMETA(DisplayName = "Force"),
    Acid        UMETA(DisplayName = "Acid")
};

/** D&D 5e conditions */
UENUM(BlueprintType)
enum class EConditionType : uint8
{
    None            UMETA(DisplayName = "None"),
    Poisoned        UMETA(DisplayName = "Poisoned"),
    Blinded         UMETA(DisplayName = "Blinded"),
    Stunned         UMETA(DisplayName = "Stunned"),
    Prone           UMETA(DisplayName = "Prone"),
    Frightened      UMETA(DisplayName = "Frightened"),
    Paralyzed       UMETA(DisplayName = "Paralyzed"),
    Burning         UMETA(DisplayName = "Burning"),
    Bleeding        UMETA(DisplayName = "Bleeding"),
    Invisible       UMETA(DisplayName = "Invisible"),
    Concentrating   UMETA(DisplayName = "Concentrating")
};

/** Advantage state for rolls */
UENUM(BlueprintType)
enum class ERollAdvantage : uint8
{
    Normal      UMETA(DisplayName = "Normal"),
    Advantage   UMETA(DisplayName = "Advantage"),
    Disadvantage UMETA(DisplayName = "Disadvantage")
};

/** Combat action economy types */
UENUM(BlueprintType)
enum class EActionType : uint8
{
    Action      UMETA(DisplayName = "Action"),
    BonusAction UMETA(DisplayName = "Bonus Action"),
    Reaction    UMETA(DisplayName = "Reaction"),
    FreeAction  UMETA(DisplayName = "Free Action")
};

/** Combat log event categories */
UENUM(BlueprintType)
enum class ECombatLogEventType : uint8
{
    Attack          UMETA(DisplayName = "Attack"),
    Damage          UMETA(DisplayName = "Damage"),
    Healing         UMETA(DisplayName = "Healing"),
    SavingThrow     UMETA(DisplayName = "Saving Throw"),
    AbilityCheck    UMETA(DisplayName = "Ability Check"),
    ConditionApplied    UMETA(DisplayName = "Condition Applied"),
    ConditionRemoved    UMETA(DisplayName = "Condition Removed"),
    Initiative      UMETA(DisplayName = "Initiative"),
    DeathSave       UMETA(DisplayName = "Death Saving Throw"),
    TurnStart       UMETA(DisplayName = "Turn Start"),
    TurnEnd         UMETA(DisplayName = "Turn End"),
    CombatStart     UMETA(DisplayName = "Combat Start"),
    CombatEnd       UMETA(DisplayName = "Combat End"),
    Misc            UMETA(DisplayName = "Miscellaneous")
};

// ============================================================
//  CORE D&D STRUCTURES
// ============================================================

/**
 * D&D 5e Ability Scores with auto-computed modifiers.
 * All scores clamped to [1, 30].
 */
USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FDnDStats
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Ability Scores",
        meta = (ClampMin = 1, ClampMax = 30))
    int32 Strength = 10;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Ability Scores",
        meta = (ClampMin = 1, ClampMax = 30))
    int32 Dexterity = 10;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Ability Scores",
        meta = (ClampMin = 1, ClampMax = 30))
    int32 Constitution = 10;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Ability Scores",
        meta = (ClampMin = 1, ClampMax = 30))
    int32 Intelligence = 10;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Ability Scores",
        meta = (ClampMin = 1, ClampMax = 30))
    int32 Wisdom = 10;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "Ability Scores",
        meta = (ClampMin = 1, ClampMax = 30))
    int32 Charisma = 10;

    // ----------------------------------------------------------------
    //  Computed helpers (pure functions, no UPROPERTY needed)
    // ----------------------------------------------------------------

    /** Returns the D&D 5e ability modifier: floor((Score - 10) / 2) */
    static int32 CalcModifier(int32 Score)
    {
        // Integer floor division for negative numbers
        return FMath::FloorToInt((Score - 10) / 2.0f);
    }

    int32 GetModifier(EAbilityType Ability) const
    {
        switch (Ability)
        {
            case EAbilityType::Strength:     return CalcModifier(Strength);
            case EAbilityType::Dexterity:    return CalcModifier(Dexterity);
            case EAbilityType::Constitution: return CalcModifier(Constitution);
            case EAbilityType::Intelligence: return CalcModifier(Intelligence);
            case EAbilityType::Wisdom:       return CalcModifier(Wisdom);
            case EAbilityType::Charisma:     return CalcModifier(Charisma);
            default:                          return 0;
        }
    }

    int32 GetScore(EAbilityType Ability) const
    {
        switch (Ability)
        {
            case EAbilityType::Strength:     return Strength;
            case EAbilityType::Dexterity:    return Dexterity;
            case EAbilityType::Constitution: return Constitution;
            case EAbilityType::Intelligence: return Intelligence;
            case EAbilityType::Wisdom:       return Wisdom;
            case EAbilityType::Charisma:     return Charisma;
            default:                          return 10;
        }
    }

    /**
     * D&D 5e Proficiency Bonus by character level.
     * Formula: ceil(Level / 4) + 1, clamped to [2, 6]
     */
    static int32 CalcProficiencyBonus(int32 Level)
    {
        // Official D&D 5e table: +2 at lv1, +3 at lv5, +4 at lv9, +5 at lv13, +6 at lv17
        const int32 ClampedLevel = FMath::Clamp(Level, 1, 20);
        return FMath::CeilToInt(ClampedLevel / 4.0f) + 1;
    }
};

// ============================================================
//  DICE RESULT STRUCTURES
// ============================================================

/** Result of a single dice roll operation */
USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FDiceResult
{
    GENERATED_BODY()

    /** Individual die values */
    UPROPERTY(BlueprintReadOnly, Category = "Dice")
    TArray<int32> Rolls;

    /** Sum of all dice (before any bonuses) */
    UPROPERTY(BlueprintReadOnly, Category = "Dice")
    int32 Total = 0;

    /** True if d20 rolled a natural 20 */
    UPROPERTY(BlueprintReadOnly, Category = "Dice")
    bool bIsCritical = false;

    /** True if d20 rolled a natural 1 */
    UPROPERTY(BlueprintReadOnly, Category = "Dice")
    bool bIsCritFail = false;

    /** The raw d20 value (valid only when rolling a d20) */
    UPROPERTY(BlueprintReadOnly, Category = "Dice")
    int32 NaturalRoll = 0;

    /** Seed used for this roll (reproducibility / debug) */
    UPROPERTY(BlueprintReadOnly, Category = "Dice")
    int32 UsedSeed = 0;
};

/** Describes a damage roll formula, e.g. 2d6 + 3 Fire */
USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FDamageRoll
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    int32 DiceCount = 1;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    int32 DiceSides = 6;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    int32 DamageBonus = 0;

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    EDamageType DamageType = EDamageType::Bludgeoning;

    /** If true, doubles the dice count (critical hit rule) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Damage")
    bool bDoubleDice = false;
};

/** Full result of an attack roll */
USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FAttackResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    FDiceResult AttackRoll;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    int32 AttackBonus = 0;

    /** AttackRoll.Total + AttackBonus */
    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    int32 FinalAttackValue = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    int32 TargetAC = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    bool bHit = false;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    bool bCriticalHit = false;

    UPROPERTY(BlueprintReadOnly, Category = "Attack")
    bool bCriticalMiss = false;
};

// ============================================================
//  COMBAT LOG
// ============================================================

/**
 * A single entry in the combat log.
 * Formatted BG3-style: "Tavita attacks Goblin: 14+3=17 vs AC 13 — Hit! Damage: 2d6+3=11 Slashing"
 */
USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FCombatLogEntry
{
    GENERATED_BODY()

    /** Wall-clock timestamp */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    FDateTime Timestamp;

    /** Game round when this event occurred */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    int32 Round = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    ECombatLogEventType EventType = ECombatLogEventType::Misc;

    /** Human-readable formatted message */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    FText Message;

    /** Optional raw dice results for UI dice animation */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    FDiceResult PrimaryRoll;

    /** Optional secondary roll (e.g. damage after attack) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    FDiceResult SecondaryRoll;

    /** Name of the actor who initiated the event */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    FText SourceName;

    /** Name of the target (may be empty) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    FText TargetName;

    /** Numeric value relevant to the event (damage dealt, heal amount, etc.) */
    UPROPERTY(BlueprintReadOnly, Category = "Combat Log")
    int32 Value = 0;
};