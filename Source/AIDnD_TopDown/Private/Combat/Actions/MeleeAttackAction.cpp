// Source/AIDnD_TopDown/Private/Combat/Actions/MeleeAttackAction.cpp
#include "Combat/Actions/MeleeAttackAction.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/ConditionComponent.h"
#include "Combat/DiceRoller.h"
#include "Combat/CombatLog.h"

UMeleeAttackAction::UMeleeAttackAction()
{
    ActionName        = FText::FromString(TEXT("Melee Attack"));
    ActionDescription = FText::FromString(TEXT("Strike a nearby enemy with your weapon."));
    ActionType        = EActionType::Action;
    ActionPointCost   = 1;
    Range             = 5.f;

    WeaponDamage.DiceCount   = 1;
    WeaponDamage.DiceSides   = 8;
    WeaponDamage.DamageBonus = 0;
    WeaponDamage.DamageType  = EDamageType::Slashing;
}

bool UMeleeAttackAction::CanExecute_Implementation(ACombatCharacter* Source,
    FText& OutReason)
{
    if (!Super::CanExecute_Implementation(Source, OutReason)) return false;

    if (!Source->CombatStats)
    {
        OutReason = FText::FromString(TEXT("No combat stats"));
        return false;
    }

    return true;
}

FActionResult UMeleeAttackAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    FText Reason;
    if (!CanExecute_Implementation(Source, Reason))
        return MakeFailResult(Reason.ToString());

    if (!TargetCharacter)
        return MakeFailResult(TEXT("No target selected"));

    UCombatStatsComponent* SourceStats = Source->CombatStats;

    // Determine attack modifier
    const EAbilityType AttackAbility = bUseFinesse
        ? EAbilityType::Dexterity
        : EAbilityType::Strength;

    int32 AttackBonus = SourceStats->GetAbilityModifier(AttackAbility);
    if (bProficient)
        AttackBonus += SourceStats->GetProficiencyBonus();

    // Collect advantage from conditions
    ERollAdvantage Advantage = Source->GetAttackAdvantage();

    // Also check target's defense state (Prone, Stunned, etc.)
    const ERollAdvantage DefenseAdv = TargetCharacter->GetDefenseAdvantage(true);
    // Merge: if target gives us advantage, apply it
    if (DefenseAdv == ERollAdvantage::Advantage && Advantage == ERollAdvantage::Normal)
        Advantage = ERollAdvantage::Advantage;
    else if (DefenseAdv == ERollAdvantage::Advantage && Advantage == ERollAdvantage::Disadvantage)
        Advantage = ERollAdvantage::Normal;

    const int32 TargetAC = TargetCharacter->GetArmorClass();

    // Roll attack
    FAttackResult AttackResult = UDiceRoller::RollAttack(AttackBonus, TargetAC, Advantage);

    FActionResult Result;
    Result.AttackResult = AttackResult;

    if (!AttackResult.bHit)
    {
        TrySpendActionPoints(Source);
        Result.bSuccess = true;
        Result.ResultMessage = FText::FromString(FString::Printf(
            TEXT("%s attacks %s: %d+%d=%d vs AC %d — Miss!"),
            *Source->CharacterName.ToString(),
            *TargetCharacter->CharacterName.ToString(),
            AttackResult.AttackRoll.NaturalRoll,
            AttackBonus,
            AttackResult.FinalAttackValue,
            TargetAC));

        UE_LOG(LogCombat, Log, TEXT("%s"), *Result.ResultMessage.ToString());
        return Result;
    }

    // Critical hit — double damage dice
    FDamageRoll DmgRoll = WeaponDamage;
    DmgRoll.DamageBonus  = SourceStats->GetAbilityModifier(AttackAbility);
    DmgRoll.bDoubleDice  = AttackResult.bCriticalHit;

    FDiceResult DamageRoll = UDiceRoller::RollDamageFull(DmgRoll);
    const int32 ActualDamage = TargetCharacter->ReceiveDamage(
        DamageRoll.Total, WeaponDamage.DamageType);

    TrySpendActionPoints(Source);

    Result.bSuccess    = true;
    Result.DamageDealt = ActualDamage;
    Result.DamageType  = WeaponDamage.DamageType;
    Result.ResultMessage = FText::FromString(FString::Printf(
        TEXT("%s attacks %s: %d+%d=%d vs AC %d — %s! Damage: %dd%d+%d=%d %s"),
        *Source->CharacterName.ToString(),
        *TargetCharacter->CharacterName.ToString(),
        AttackResult.AttackRoll.NaturalRoll,
        AttackBonus,
        AttackResult.FinalAttackValue,
        TargetAC,
        AttackResult.bCriticalHit ? TEXT("CRITICAL HIT") : TEXT("Hit"),
        DmgRoll.bDoubleDice ? WeaponDamage.DiceCount * 2 : WeaponDamage.DiceCount,
        WeaponDamage.DiceSides,
        DmgRoll.DamageBonus,
        ActualDamage,
        *UEnum::GetValueAsString(WeaponDamage.DamageType)));

    UE_LOG(LogCombat, Log, TEXT("%s"), *Result.ResultMessage.ToString());
    return Result;
}