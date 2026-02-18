// Source/AIDnD_TopDown/Private/Combat/Actions/SpellAction.cpp
#include "Combat/Actions/SpellAction.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/DiceRoller.h"
#include "Combat/CombatLog.h"
#include "Kismet/GameplayStatics.h"

USpellAction::USpellAction()
{
    ActionName      = FText::FromString(TEXT("Spell"));
    ActionType      = EActionType::Action;
    ActionPointCost = 1;
    Range           = 60.f;

    SpellDamage.DiceCount  = 3;
    SpellDamage.DiceSides  = 6;
    SpellDamage.DamageType = EDamageType::Fire;
}

FActionResult USpellAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    FText Reason;
    if (!CanExecute_Implementation(Source, Reason))
        return MakeFailResult(Reason.ToString());

    // Gather targets
    TArray<ACombatCharacter*> Targets;
    if (AoERadius > 0.f)
    {
        Targets = GetAoETargets(TargetLocation, AoERadius, Source->GetWorld());
    }
    else if (TargetCharacter)
    {
        Targets.Add(TargetCharacter);
    }

    if (Targets.IsEmpty())
        return MakeFailResult(TEXT("No valid targets"));

    int32 TotalDamage = 0;
    FActionResult FinalResult;
    FinalResult.bSuccess = true;

    for (ACombatCharacter* Target : Targets)
    {
        bool bSaveSucceeded = false;

        if (Delivery == ESpellDelivery::AttackRoll)
        {
            // Spell attack: d20 + SpellcastingMod + ProfBonus vs AC
            const int32 SpellMod = Source->CombatStats
                ? Source->CombatStats->GetAbilityModifier(EAbilityType::Intelligence)
                  + Source->CombatStats->GetProficiencyBonus()
                : 0;

            FAttackResult AttackRoll = UDiceRoller::RollAttack(
                SpellMod, Target->GetArmorClass());

            FinalResult.AttackResult = AttackRoll;

            if (!AttackRoll.bHit)
            {
                UE_LOG(LogCombat, Log,
                    TEXT("SpellAction: %s spell misses %s"),
                    *Source->CharacterName.ToString(),
                    *Target->CharacterName.ToString());
                continue;
            }

            SpellDamage.bDoubleDice = AttackRoll.bCriticalHit;
        }
        else if (Delivery == ESpellDelivery::SavingThrow)
        {
            bSaveSucceeded = Target->CombatStats
                ? Target->CombatStats->SavingThrow(SaveAbility, SpellSaveDC)
                : false;
        }

        FActionResult TargetResult = ApplySpellEffect(Source, Target, bSaveSucceeded);
        TotalDamage += TargetResult.DamageDealt;
    }

    TrySpendActionPoints(Source);

    FinalResult.DamageDealt  = TotalDamage;
    FinalResult.DamageType   = SpellDamage.DamageType;
    FinalResult.ResultMessage = FText::FromString(FString::Printf(
        TEXT("%s casts %s — %d total damage"),
        *Source->CharacterName.ToString(),
        *ActionName.ToString(),
        TotalDamage));

    UE_LOG(LogCombat, Log, TEXT("%s"), *FinalResult.ResultMessage.ToString());
    return FinalResult;
}

FActionResult USpellAction::ApplySpellEffect_Implementation(ACombatCharacter* Source,
    ACombatCharacter* Target, bool bSaveSucceeded)
{
    FDamageRoll DmgRoll = SpellDamage;

    int32 RawDamage = UDiceRoller::RollDamage(DmgRoll);

    // Half damage on successful save
    if (bSaveSucceeded && bHalfDamageOnSave)
        RawDamage = FMath::FloorToInt(RawDamage / 2.f);

    const int32 Actual = Target->ReceiveDamage(RawDamage, SpellDamage.DamageType);

    UE_LOG(LogCombat, Log,
        TEXT("SpellAction: %s takes %d %s damage (save: %s)"),
        *Target->CharacterName.ToString(),
        Actual,
        *UEnum::GetValueAsString(SpellDamage.DamageType),
        bSaveSucceeded ? TEXT("succeeded") : TEXT("failed"));

    return MakeSuccessResult(TEXT(""), Actual);
}

FActionPreview USpellAction::GetPreviewData_Implementation(ACombatCharacter* Source)
{
    FActionPreview Preview;
    Preview.Range     = Range;
    Preview.AoERadius = AoERadius;
    return Preview;
}

TArray<ACombatCharacter*> USpellAction::GetAoETargets(FVector Center,
    float Radius, UWorld* World) const
{
    TArray<ACombatCharacter*> Result;
    if (!World) return Result;

    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(World,
        ACombatCharacter::StaticClass(), Found);

    const float RadiusCm = Radius * 100.f; // feet → UU

    for (AActor* Actor : Found)
    {
        ACombatCharacter* Char = Cast<ACombatCharacter>(Actor);
        if (!Char || !Char->CombatStats || !Char->CombatStats->IsAlive()) continue;

        if (FVector::Dist(Center, Char->GetActorLocation()) <= RadiusCm)
            Result.Add(Char);
    }

    return Result;
}