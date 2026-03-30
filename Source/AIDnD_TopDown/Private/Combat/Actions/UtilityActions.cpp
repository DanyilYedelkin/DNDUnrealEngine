// Source/AIDnD_TopDown/Private/Combat/Actions/UtilityActions.cpp
#include "Combat/Actions/UtilityActions.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/ConditionComponent.h"
#include "Combat/CombatLog.h"

// ================================================================
//  DODGE
// ================================================================

UDodgeAction::UDodgeAction()
{
    ActionName        = FText::FromString(TEXT("Dodge"));
    ActionDescription = FText::FromString(
        TEXT("Focus on avoiding attacks. Until your next turn, attacks against you have Disadvantage."));
    ActionType        = EActionType::Action;
    ActionPointCost   = 1;
}

FActionResult UDodgeAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    FText Reason;
    if (!CanExecute_Implementation(Source, Reason))
        return MakeFailResult(Reason.ToString());

    // Apply Dodging as Invisible condition proxy for 1 round
    // In a full implementation you'd add a dedicated EDodging condition
    // For now we mark it via a custom flag approach using the condition system
    // Using EConditionType::Invisible as a stand-in — replace when you add Dodging condition
    if (Source->ConditionComp)
        Source->ConditionComp->ApplyCondition(EConditionType::Invisible, 1);

    TrySpendActionPoints(Source);

    UE_LOG(LogCombat, Log,
        TEXT("DodgeAction: %s takes the Dodge action"),
        *Source->CharacterName.ToString());

    return MakeSuccessResult(FString::Printf(
        TEXT("%s takes the Dodge action — attacks against them have Disadvantage"),
        *Source->CharacterName.ToString()));
}

// ================================================================
//  DASH
// ================================================================

UDashAction::UDashAction()
{
    ActionName        = FText::FromString(TEXT("Dash"));
    ActionDescription = FText::FromString(
        TEXT("Double your movement speed this turn."));
    ActionType        = EActionType::Action;
    ActionPointCost   = 1;
}

FActionResult UDashAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    FText Reason;
    if (!CanExecute_Implementation(Source, Reason))
        return MakeFailResult(Reason.ToString());

    const float ExtraMovement = Source->CombatStats
        ? Source->CombatStats->Speed : 30.f;

    if (Source->CombatStats)
        Source->CombatStats->AddMovement(ExtraMovement);

    TrySpendActionPoints(Source);

    UE_LOG(LogCombat, Log,
        TEXT("DashAction: %s dashes — +%.0f feet of movement"),
        *Source->CharacterName.ToString(), ExtraMovement);

    return MakeSuccessResult(FString::Printf(
        TEXT("%s dashes — gains %.0f extra feet of movement"),
        *Source->CharacterName.ToString(), ExtraMovement));
}

// ================================================================
//  HELP
// ================================================================

UHelpAction::UHelpAction()
{
    ActionName        = FText::FromString(TEXT("Help"));
    ActionDescription = FText::FromString(
        TEXT("Aid an ally — they gain Advantage on their next attack."));
    ActionType        = EActionType::Action;
    ActionPointCost   = 1;
    Range             = 5.f;
}

bool UHelpAction::CanExecute_Implementation(ACombatCharacter* Source,
    FText& OutReason)
{
    if (!Super::CanExecute_Implementation(Source, OutReason)) return false;
    return true;
}

FActionResult UHelpAction::Execute_Implementation(ACombatCharacter* Source,
    FVector TargetLocation, ACombatCharacter* TargetCharacter)
{
    FText Reason;
    if (!CanExecute_Implementation(Source, Reason))
        return MakeFailResult(Reason.ToString());

    if (!TargetCharacter)
        return MakeFailResult(TEXT("No ally targeted"));

    // Must be an ally
    if (TargetCharacter->bIsPlayerControlled != Source->bIsPlayerControlled)
        return MakeFailResult(TEXT("Can only Help allies"));

    // Grant advantage for 1 round via Invisible proxy
    // (same note as Dodge — add EConditionType::Helped in a future pass)
    if (TargetCharacter->ConditionComp)
        TargetCharacter->ConditionComp->ApplyCondition(EConditionType::Invisible, 1);

    TrySpendActionPoints(Source);

    UE_LOG(LogCombat, Log,
        TEXT("HelpAction: %s helps %s — Advantage on next attack"),
        *Source->CharacterName.ToString(),
        *TargetCharacter->CharacterName.ToString());

    return MakeSuccessResult(FString::Printf(
        TEXT("%s helps %s — Advantage on next attack roll"),
        *Source->CharacterName.ToString(),
        *TargetCharacter->CharacterName.ToString()));
}