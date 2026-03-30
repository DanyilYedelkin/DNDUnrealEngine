// Source/AIDnD_TopDown/Private/Combat/Actions/RangedAttackAction.cpp
#include "Combat/Actions/RangedAttackAction.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/DiceRoller.h"
#include "Combat/CombatLog.h"
#include "Kismet/GameplayStatics.h"

URangedAttackAction::URangedAttackAction()
{
    ActionName        = FText::FromString(TEXT("Ranged Attack"));
    ActionDescription = FText::FromString(TEXT("Fire a ranged weapon at a target."));
    ActionType        = EActionType::Action;
    Range             = 320.f;

    WeaponDamage.DiceCount  = 1;
    WeaponDamage.DiceSides  = 8;
    WeaponDamage.DamageType = EDamageType::Piercing;
}

bool URangedAttackAction::CanExecute_Implementation(ACombatCharacter* Source,
    FText& OutReason)
{
    if (!Super::CanExecute_Implementation(Source, OutReason)) return false;
    return true;
}

FActionResult URangedAttackAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    FText Reason;
    if (!CanExecute_Implementation(Source, Reason))
        return MakeFailResult(Reason.ToString());

    if (!TargetCharacter)
        return MakeFailResult(TEXT("No target selected"));

    UCombatStatsComponent* SourceStats = Source->CombatStats;

    int32 AttackBonus = SourceStats->GetAbilityModifier(EAbilityType::Dexterity);
    if (bProficient)
        AttackBonus += SourceStats->GetProficiencyBonus();

    // Determine advantage state
    ERollAdvantage Advantage = Source->GetAttackAdvantage();

    // Enemy in melee → Disadvantage
    if (HasEnemyInMelee(Source))
    {
        if (Advantage == ERollAdvantage::Advantage)
            Advantage = ERollAdvantage::Normal;
        else
            Advantage = ERollAdvantage::Disadvantage;
    }

    // Long range → Disadvantage
    const float Distance = FVector::Dist(Source->GetActorLocation(),
                                          TargetCharacter->GetActorLocation()) / 100.f; // UU → feet approx

    if (Distance > NormalRange)
    {
        if (Advantage == ERollAdvantage::Advantage)
            Advantage = ERollAdvantage::Normal;
        else
            Advantage = ERollAdvantage::Disadvantage;
    }

    // Defense advantage from target conditions (ranged = not melee)
    const ERollAdvantage DefenseAdv = TargetCharacter->GetDefenseAdvantage(false);
    if (DefenseAdv == ERollAdvantage::Advantage && Advantage == ERollAdvantage::Normal)
        Advantage = ERollAdvantage::Advantage;
    else if (DefenseAdv == ERollAdvantage::Advantage && Advantage == ERollAdvantage::Disadvantage)
        Advantage = ERollAdvantage::Normal;

    const int32 TargetAC = TargetCharacter->GetArmorClass();
    FAttackResult AttackResult = UDiceRoller::RollAttack(AttackBonus, TargetAC, Advantage);

    FActionResult Result;
    Result.AttackResult = AttackResult;

    if (!AttackResult.bHit)
    {
        TrySpendActionPoints(Source);
        Result.bSuccess      = true;
        Result.ResultMessage = FText::FromString(FString::Printf(
            TEXT("%s fires at %s: %d+%d=%d vs AC %d — Miss!"),
            *Source->CharacterName.ToString(),
            *TargetCharacter->CharacterName.ToString(),
            AttackResult.AttackRoll.NaturalRoll, AttackBonus,
            AttackResult.FinalAttackValue, TargetAC));
        return Result;
    }

    FDamageRoll DmgRoll  = WeaponDamage;
    DmgRoll.DamageBonus  = SourceStats->GetAbilityModifier(EAbilityType::Dexterity);
    DmgRoll.bDoubleDice  = AttackResult.bCriticalHit;

    FDiceResult DamageRoll   = UDiceRoller::RollDamageFull(DmgRoll);
    const int32 ActualDamage = TargetCharacter->ReceiveDamage(
        DamageRoll.Total, WeaponDamage.DamageType);

    TrySpendActionPoints(Source);

    Result.bSuccess      = true;
    Result.DamageDealt   = ActualDamage;
    Result.DamageType    = WeaponDamage.DamageType;
    Result.ResultMessage = FText::FromString(FString::Printf(
        TEXT("%s fires at %s: %d+%d=%d vs AC %d — %s! Damage: %d %s"),
        *Source->CharacterName.ToString(),
        *TargetCharacter->CharacterName.ToString(),
        AttackResult.AttackRoll.NaturalRoll, AttackBonus,
        AttackResult.FinalAttackValue, TargetAC,
        AttackResult.bCriticalHit ? TEXT("CRITICAL HIT") : TEXT("Hit"),
        ActualDamage,
        *UEnum::GetValueAsString(WeaponDamage.DamageType)));

    UE_LOG(LogCombat, Log, TEXT("%s"), *Result.ResultMessage.ToString());
    return Result;
}

bool URangedAttackAction::HasEnemyInMelee(ACombatCharacter* Source) const
{
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(Source->GetWorld(),
        ACombatCharacter::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        ACombatCharacter* Other = Cast<ACombatCharacter>(Actor);
        if (!Other || Other == Source) continue;
        if (Other->bIsPlayerControlled == Source->bIsPlayerControlled) continue;
        if (!Other->CombatStats || !Other->CombatStats->IsAlive()) continue;

        const float DistFeet = FVector::Dist(
            Source->GetActorLocation(), Other->GetActorLocation()) / 100.f;

        if (DistFeet <= 5.f) return true;
    }
    return false;
}