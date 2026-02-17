// Source/YourProject/Private/Combat/DiceRoller.cpp
#include "Combat/DiceRoller.h"
#include "Math/RandomStream.h"

DEFINE_LOG_CATEGORY_STATIC(LogCombatDice, Log, All);

// ----------------------------------------------------------------
//  Helpers
// ----------------------------------------------------------------

static FRandomStream MakeStream(int32 Seed)
{
    // Seed == 0 → use truly random seed from current time
    return (Seed != 0) ? FRandomStream(Seed)
                       : FRandomStream(FMath::Rand());
}

int32 UDiceRoller::RollSingleDie(int32 Sides, FRandomStream& Stream)
{
    if (Sides <= 0)
    {
        UE_LOG(LogCombatDice, Warning, TEXT("RollSingleDie called with Sides <= 0, returning 0"));
        return 0;
    }
    return Stream.RandRange(1, Sides);
}

// ----------------------------------------------------------------
//  Core rolling
// ----------------------------------------------------------------

FDiceResult UDiceRoller::RollDice(int32 DiceCount, int32 DiceSides, int32 Seed)
{
    FDiceResult Result;
    Result.UsedSeed = Seed;

    if (DiceCount <= 0 || DiceSides <= 0)
    {
        UE_LOG(LogCombatDice, Warning,
            TEXT("RollDice called with invalid params: %dd%d"), DiceCount, DiceSides);
        return Result;
    }

    FRandomStream Stream = MakeStream(Seed);
    Result.Rolls.Reserve(DiceCount);

    for (int32 i = 0; i < DiceCount; ++i)
    {
        const int32 Roll = RollSingleDie(DiceSides, Stream);
        Result.Rolls.Add(Roll);
        Result.Total += Roll;
    }

    UE_LOG(LogCombatDice, Verbose,
        TEXT("RollDice: %dd%d = %d (seed=%d)"), DiceCount, DiceSides, Result.Total, Seed);

    return Result;
}

FDiceResult UDiceRoller::RollD20(int32 Seed)
{
    FDiceResult Result = RollDice(1, 20, Seed);

    if (Result.Rolls.Num() > 0)
    {
        Result.NaturalRoll = Result.Rolls[0];
        Result.bIsCritical  = (Result.NaturalRoll == 20);
        Result.bIsCritFail  = (Result.NaturalRoll == 1);
    }

    UE_LOG(LogCombatDice, Log,
        TEXT("RollD20: %d%s"),
        Result.NaturalRoll,
        Result.bIsCritical ? TEXT(" [CRITICAL HIT]") : (Result.bIsCritFail ? TEXT(" [CRITICAL MISS]") : TEXT("")));

    return Result;
}

FDiceResult UDiceRoller::RollWithAdvantage(int32 Seed)
{
    FRandomStream Stream = MakeStream(Seed);

    const int32 Roll1 = RollSingleDie(20, Stream);
    const int32 Roll2 = RollSingleDie(20, Stream);
    const int32 Chosen = FMath::Max(Roll1, Roll2);

    FDiceResult Result;
    Result.UsedSeed     = Seed;
    Result.Rolls        = { Roll1, Roll2 };
    Result.Total        = Chosen;
    Result.NaturalRoll  = Chosen;
    Result.bIsCritical  = (Chosen == 20);
    Result.bIsCritFail  = (Chosen == 1);   // only if BOTH are 1

    UE_LOG(LogCombatDice, Log,
        TEXT("RollWithAdvantage: [%d, %d] → kept %d"), Roll1, Roll2, Chosen);

    return Result;
}

FDiceResult UDiceRoller::RollWithDisadvantage(int32 Seed)
{
    FRandomStream Stream = MakeStream(Seed);

    const int32 Roll1 = RollSingleDie(20, Stream);
    const int32 Roll2 = RollSingleDie(20, Stream);
    const int32 Chosen = FMath::Min(Roll1, Roll2);

    FDiceResult Result;
    Result.UsedSeed     = Seed;
    Result.Rolls        = { Roll1, Roll2 };
    Result.Total        = Chosen;
    Result.NaturalRoll  = Chosen;
    Result.bIsCritical  = (Chosen == 20);  // only if BOTH are 20
    Result.bIsCritFail  = (Chosen == 1);

    UE_LOG(LogCombatDice, Log,
        TEXT("RollWithDisadvantage: [%d, %d] → kept %d"), Roll1, Roll2, Chosen);

    return Result;
}

// ----------------------------------------------------------------
//  Combat-specific
// ----------------------------------------------------------------

int32 UDiceRoller::RollInitiative(int32 DexModifier, int32 Seed)
{
    return RollInitiativeFull(DexModifier, Seed).Total;
}

FDiceResult UDiceRoller::RollInitiativeFull(int32 DexModifier, int32 Seed)
{
    FDiceResult Result = RollD20(Seed);
    Result.Total += DexModifier;

    UE_LOG(LogCombatDice, Log,
        TEXT("RollInitiative: d20(%d) + DexMod(%d) = %d"),
        Result.NaturalRoll, DexModifier, Result.Total);

    return Result;
}

FAttackResult UDiceRoller::RollAttack(
    int32 AttackBonus,
    int32 TargetAC,
    ERollAdvantage Advantage,
    int32 Seed)
{
    FAttackResult Result;
    Result.AttackBonus = AttackBonus;
    Result.TargetAC    = TargetAC;

    // Roll d20 with appropriate advantage state
    switch (Advantage)
    {
        case ERollAdvantage::Advantage:
            Result.AttackRoll = RollWithAdvantage(Seed);
            break;
        case ERollAdvantage::Disadvantage:
            Result.AttackRoll = RollWithDisadvantage(Seed);
            break;
        default:
            Result.AttackRoll = RollD20(Seed);
            break;
    }

    Result.FinalAttackValue = Result.AttackRoll.Total + AttackBonus;
    Result.bCriticalHit     = Result.AttackRoll.bIsCritical;
    Result.bCriticalMiss    = Result.AttackRoll.bIsCritFail;

    // nat-20 auto-hits, nat-1 auto-misses, otherwise compare to AC
    if (Result.bCriticalHit)
    {
        Result.bHit = true;
    }
    else if (Result.bCriticalMiss)
    {
        Result.bHit = false;
    }
    else
    {
        Result.bHit = (Result.FinalAttackValue >= TargetAC);
    }

    UE_LOG(LogCombatDice, Log,
        TEXT("RollAttack: d20(%d) + bonus(%d) = %d vs AC %d → %s"),
        Result.AttackRoll.NaturalRoll,
        AttackBonus,
        Result.FinalAttackValue,
        TargetAC,
        Result.bCriticalHit ? TEXT("CRITICAL HIT") :
        Result.bCriticalMiss ? TEXT("CRITICAL MISS") :
        Result.bHit ? TEXT("Hit") : TEXT("Miss"));

    return Result;
}

int32 UDiceRoller::RollDamage(const FDamageRoll& DamageRoll, int32 Seed)
{
    return RollDamageFull(DamageRoll, Seed).Total;
}

FDiceResult UDiceRoller::RollDamageFull(const FDamageRoll& DamageRoll, int32 Seed)
{
    // Critical hits double the number of damage dice (PHB rule)
    const int32 ActualDiceCount = DamageRoll.bDoubleDice
        ? DamageRoll.DiceCount * 2
        : DamageRoll.DiceCount;

    FDiceResult Result = RollDice(ActualDiceCount, DamageRoll.DiceSides, Seed);
    Result.Total += DamageRoll.DamageBonus;

    // Minimum 1 damage on a hit (per 5e rules)
    Result.Total = FMath::Max(1, Result.Total);

    UE_LOG(LogCombatDice, Log,
        TEXT("RollDamage: %dd%d+%d = %d [%s]%s"),
        ActualDiceCount,
        DamageRoll.DiceSides,
        DamageRoll.DamageBonus,
        Result.Total,
        *UEnum::GetValueAsString(DamageRoll.DamageType),
        DamageRoll.bDoubleDice ? TEXT(" (Critical doubled)") : TEXT(""));

    return Result;
}

// ----------------------------------------------------------------
//  Utility
// ----------------------------------------------------------------

FString UDiceRoller::FormatModifier(int32 Modifier)
{
    return (Modifier >= 0)
        ? FString::Printf(TEXT("+%d"), Modifier)
        : FString::Printf(TEXT("%d"), Modifier);    // negative already has '-'
}

FString UDiceRoller::FormatDamageRoll(const FDamageRoll& DamageRoll)
{
    const FString DiceStr = FString::Printf(
        TEXT("%dd%d"), DamageRoll.DiceCount, DamageRoll.DiceSides);

    if (DamageRoll.DamageBonus == 0)
        return DiceStr;

    return DiceStr + FormatModifier(DamageRoll.DamageBonus);
}