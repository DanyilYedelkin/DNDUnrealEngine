// Source/AIDnD_TopDown/Private/Combat/Actions/CombatAction.cpp
#include "Combat/Actions/CombatAction.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/ConditionComponent.h"
#include "Combat/CombatLog.h"

FActionResult UCombatAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    return MakeFailResult(TEXT("Execute not implemented"));
}

bool UCombatAction::CanExecute_Implementation(ACombatCharacter* Source,
    FText& OutReason)
{
    if (!Source)
    {
        OutReason = FText::FromString(TEXT("No source character"));
        return false;
    }

    UCombatStatsComponent* Stats = Source->CombatStats;
    if (!Stats)
    {
        OutReason = FText::FromString(TEXT("No combat stats"));
        return false;
    }

    // Check action economy
    switch (ActionType)
    {
        case EActionType::Action:
            if (Stats->CurrentActions < ActionPointCost)
            {
                OutReason = FText::FromString(TEXT("Not enough actions"));
                return false;
            }
            break;
        case EActionType::BonusAction:
            if (Stats->CurrentBonusActions < ActionPointCost)
            {
                OutReason = FText::FromString(TEXT("No bonus action available"));
                return false;
            }
            break;
        case EActionType::Reaction:
            if (Stats->CurrentReactions < ActionPointCost)
            {
                OutReason = FText::FromString(TEXT("No reaction available"));
                return false;
            }
            break;
        default:
            break;
    }

    // Check if incapacitated
    if (Source->ConditionComp && Source->ConditionComp->IsIncapacitated())
    {
        OutReason = FText::FromString(TEXT("Character is incapacitated"));
        return false;
    }

    return true;
}

FActionPreview UCombatAction::GetPreviewData_Implementation(ACombatCharacter* Source)
{
    FActionPreview Preview;
    Preview.Range = Range;
    return Preview;
}

bool UCombatAction::TrySpendActionPoints(ACombatCharacter* Source)
{
    if (!Source || !Source->CombatStats) return false;
    return Source->CombatStats->SpendAction(ActionType, ActionPointCost);
}

FActionResult UCombatAction::MakeFailResult(const FString& Reason)
{
    FActionResult Result;
    Result.bSuccess = false;
    Result.ResultMessage = FText::FromString(Reason);
    return Result;
}

FActionResult UCombatAction::MakeSuccessResult(const FString& Message,
    int32 Damage, int32 Healing)
{
    FActionResult Result;
    Result.bSuccess     = true;
    Result.ResultMessage = FText::FromString(Message);
    Result.DamageDealt  = Damage;
    Result.HealingDone  = Healing;
    return Result;
}