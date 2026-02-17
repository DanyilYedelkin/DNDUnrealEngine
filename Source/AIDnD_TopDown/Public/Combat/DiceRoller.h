// Source/YourProject/Public/Combat/DiceRoller.h
#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Combat/CombatTypes.h"
#include "DiceRoller.generated.h"

/**
 * Stateless dice rolling library.
 * All methods are static and BlueprintCallable.
 *
 * Seeding: pass Seed > 0 to reproduce a specific roll sequence.
 * Seed == 0 (default) → truly random via FMath::RandRange.
 */
UCLASS()
class AIDND_TOPDOWN_API UDiceRoller : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:

    // ----------------------------------------------------------------
    //  Core rolling
    // ----------------------------------------------------------------

    /**
     * Roll DiceCount dice with DiceSides sides.
     * @param Seed   Optional deterministic seed (0 = random)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll Dice", Keywords = "dice roll random"))
    static FDiceResult RollDice(int32 DiceCount, int32 DiceSides, int32 Seed = 0);
    
    
    /** Roll a single d20 and populate critical/critfail flags. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll D20"))
    static FDiceResult RollD20(int32 Seed = 0);

    /**
     * Roll 2d20 and keep the higher result (Advantage).
     * Both individual rolls are stored in FDiceResult::Rolls.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll With Advantage"))
    static FDiceResult RollWithAdvantage(int32 Seed = 0);

    /**
     * Roll 2d20 and keep the lower result (Disadvantage).
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll With Disadvantage"))
    static FDiceResult RollWithDisadvantage(int32 Seed = 0);

    // ----------------------------------------------------------------
    //  Combat-specific rolls
    // ----------------------------------------------------------------

    /**
     * Roll initiative: d20 + DexModifier.
     * @return Final initiative value (used for ordering)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll Initiative"))
    static int32 RollInitiative(int32 DexModifier, int32 Seed = 0);

    /**
     * Roll initiative and return the full dice result (for log/UI).
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll Initiative (Full Result)"))
    static FDiceResult RollInitiativeFull(int32 DexModifier, int32 Seed = 0);

    /**
     * Perform a full attack roll.
     * Handles nat-20 (auto-hit, critical), nat-1 (auto-miss).
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll Attack"))
    static FAttackResult RollAttack(
        int32 AttackBonus,
        int32 TargetAC,
        ERollAdvantage Advantage = ERollAdvantage::Normal,
        int32 Seed = 0);

    /**
     * Roll damage for a given formula.
     * Pass bDoubleDice = true on FDamageRoll for critical hits.
     * @return Final damage value (dice sum + bonus, minimum 1 on a hit)
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll Damage"))
    static int32 RollDamage(const FDamageRoll& DamageRoll, int32 Seed = 0);

    /**
     * Roll damage and return full FDiceResult for log display.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Dice",
        meta = (DisplayName = "Roll Damage (Full Result)"))
    static FDiceResult RollDamageFull(const FDamageRoll& DamageRoll, int32 Seed = 0);

    // ----------------------------------------------------------------
    //  Utility
    // ----------------------------------------------------------------

    /** Returns modifier string with sign, e.g. "+3" or "-1" */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Dice",
        meta = (DisplayName = "Format Modifier"))
    static FString FormatModifier(int32 Modifier);

    /**
     * Format a damage roll formula as a string, e.g. "2d6+3".
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Dice",
        meta = (DisplayName = "Format Damage Roll"))
    static FString FormatDamageRoll(const FDamageRoll& DamageRoll);

private:

    /** Internal: roll a single die. Uses seeded or unseeded RNG. */
    static int32 RollSingleDie(int32 Sides, FRandomStream& Stream);
};