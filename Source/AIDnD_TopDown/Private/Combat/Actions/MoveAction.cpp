// Source/AIDnD_TopDown/Private/Combat/Actions/MoveAction.cpp
#include "Combat/Actions/MoveAction.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/CombatLog.h"
#include "Combat/ConditionComponent.h"

UMoveAction::UMoveAction()
{
    ActionName        = FText::FromString(TEXT("Move"));
    ActionType        = EActionType::FreeAction; // Movement is free, not an Action
    ActionPointCost   = 0;
    bRequiresLineOfSight = false;
}

bool UMoveAction::CanExecute_Implementation(ACombatCharacter* Source, FText& OutReason)
{
    if (!Source || !Source->CombatStats)
    {
        OutReason = FText::FromString(TEXT("No combat stats"));
        return false;
    }

    if (Source->CombatStats->MovementRemaining < MovementCostFeet)
    {
        OutReason = FText::FromString(FString::Printf(
            TEXT("Not enough movement (%.0f needed, %.0f remaining)"),
            MovementCostFeet, Source->CombatStats->MovementRemaining));
        return false;
    }

    if (Source->ConditionComp && Source->ConditionComp->IsIncapacitated())
    {
        OutReason = FText::FromString(TEXT("Cannot move while incapacitated"));
        return false;
    }

    return true;
}

FActionResult UMoveAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    FText Reason;
    if (!CanExecute_Implementation(Source, Reason))
        return MakeFailResult(Reason.ToString());

    Source->CombatStats->SpendMovement(MovementCostFeet);

    UE_LOG(LogCombat, Log,
        TEXT("MoveAction: %s moves %.0f feet (%.0f remaining)"),
        *Source->CharacterName.ToString(),
        MovementCostFeet,
        Source->CombatStats->MovementRemaining);

    return MakeSuccessResult(FString::Printf(
        TEXT("%s moves %.0f feet"),
        *Source->CharacterName.ToString(), MovementCostFeet));
}