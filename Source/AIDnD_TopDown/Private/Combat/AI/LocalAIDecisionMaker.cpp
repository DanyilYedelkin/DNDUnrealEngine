// Source/AIDnD_TopDown/Private/Combat/AI/LocalAIDecisionMaker.cpp
#include "Combat/AI/LocalAIDecisionMaker.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/Actions/MeleeAttackAction.h"
#include "Combat/Actions/UtilityActions.h"
#include "Combat/CombatLog.h"

void ULocalAIDecisionMaker::RequestDecision_Implementation(
    const FAIBattleContext& Context)
{
    if (Context.Enemies.IsEmpty())
    {
        FAIDecision EndTurn;
        EndTurn.DecisionType = EAIDecisionType::EndTurn;
        EndTurn.Reasoning    = TEXT("No enemies found");
        OnDecisionReady.Broadcast(EndTurn);
        return;
    }

    const float SelfHPPct = (Context.SelfMaxHP > 0)
        ? (float)Context.SelfHP / (float)Context.SelfMaxHP
        : 1.f;

    FAIDecision Decision;

    // Priority 1: Dodge at low HP (if not already dodging)
    if (SelfHPPct < LowHPThreshold && HasEnemyInMeleeRange(Context))
    {
        Decision = DecideDodge(Context);
    }
    // Priority 2: Attack if enemy in range
    else if (HasEnemyInMeleeRange(Context))
    {
        Decision = DecideAttack(Context);
    }
    // Priority 3: Move toward nearest enemy
    else if (Context.SelfCharacter &&
             Context.SelfCharacter->CombatStats &&
             Context.SelfCharacter->CombatStats->MovementRemaining > 0.f)
    {
        Decision = DecideMove(Context);
    }
    else
    {
        Decision.DecisionType = EAIDecisionType::EndTurn;
        Decision.Reasoning    = TEXT("No valid actions available");
    }

    UE_LOG(LogCombat, Log,
        TEXT("LocalAI: Decision = %s — %s"),
        *UEnum::GetValueAsString(Decision.DecisionType),
        *Decision.Reasoning);

    OnDecisionReady.Broadcast(Decision);
}

FAIDecision ULocalAIDecisionMaker::DecideAttack(
    const FAIBattleContext& Context) const
{
    FAIDecision Decision;

    const FAITargetInfo* Target = FindWeakestEnemy(Context.Enemies);
    if (!Target)
    {
        Decision.DecisionType = EAIDecisionType::EndTurn;
        Decision.Reasoning    = TEXT("No attackable target");
        return Decision;
    }

    Decision.DecisionType  = EAIDecisionType::Attack;
    Decision.TargetCharacter = Target->Character;
    Decision.TargetLocation  = Target->Character
        ? Target->Character->GetActorLocation()
        : FVector::ZeroVector;
    Decision.ChosenAction    = UMeleeAttackAction::StaticClass();
    Decision.Reasoning       = FString::Printf(
        TEXT("Attacking weakest enemy %s (%.0f%% HP)"),
        Target->Character ? *Target->Character->CharacterName.ToString() : TEXT("?"),
        Target->HPPercent * 100.f);

    return Decision;
}

FAIDecision ULocalAIDecisionMaker::DecideMove(
    const FAIBattleContext& Context) const
{
    FAIDecision Decision;

    const FAITargetInfo* NearestEnemy = FindNearestEnemy(Context.Enemies);
    if (!NearestEnemy || !NearestEnemy->Character)
    {
        Decision.DecisionType = EAIDecisionType::EndTurn;
        Decision.Reasoning    = TEXT("No enemy to move toward");
        return Decision;
    }

    const FVector EnemyLoc = NearestEnemy->Character->GetActorLocation();
    const FVector BestTile = FindBestApproachTile(Context, EnemyLoc);

    if (BestTile.IsZero())
    {
        Decision.DecisionType = EAIDecisionType::EndTurn;
        Decision.Reasoning    = TEXT("No reachable tile toward enemy");
        return Decision;
    }

    Decision.DecisionType  = EAIDecisionType::Move;
    Decision.TargetLocation = BestTile;
    Decision.Reasoning      = FString::Printf(
        TEXT("Moving toward %s"),
        *NearestEnemy->Character->CharacterName.ToString());

    return Decision;
}

FAIDecision ULocalAIDecisionMaker::DecideDodge(
    const FAIBattleContext& Context) const
{
    FAIDecision Decision;
    Decision.DecisionType = EAIDecisionType::Dodge;
    Decision.ChosenAction = UDodgeAction::StaticClass();
    Decision.Reasoning    = FString::Printf(
        TEXT("HP low (%.0f%%) — taking Dodge action"),
        Context.SelfMaxHP > 0
            ? (float)Context.SelfHP / Context.SelfMaxHP * 100.f
            : 0.f);
    return Decision;
}

const FAITargetInfo* ULocalAIDecisionMaker::FindWeakestEnemy(
    const TArray<FAITargetInfo>& Enemies) const
{
    const FAITargetInfo* Weakest = nullptr;
    float LowestHP = 1.f;

    for (const FAITargetInfo& Info : Enemies)
    {
        if (!Info.bIsAlive) continue;
        if (Info.HPPercent < LowestHP)
        {
            LowestHP = Info.HPPercent;
            Weakest  = &Info;
        }
    }
    return Weakest;
}

const FAITargetInfo* ULocalAIDecisionMaker::FindNearestEnemy(
    const TArray<FAITargetInfo>& Enemies) const
{
    const FAITargetInfo* Nearest = nullptr;
    float MinDist = MAX_FLT;

    for (const FAITargetInfo& Info : Enemies)
    {
        if (!Info.bIsAlive) continue;
        if (Info.DistanceFeet < MinDist)
        {
            MinDist  = Info.DistanceFeet;
            Nearest  = &Info;
        }
    }
    return Nearest;
}

bool ULocalAIDecisionMaker::HasEnemyInMeleeRange(
    const FAIBattleContext& Context) const
{
    for (const FAITargetInfo& Info : Context.Enemies)
    {
        if (Info.bIsAlive && Info.DistanceFeet <= MeleeRangeFeet)
            return true;
    }
    return false;
}

FVector ULocalAIDecisionMaker::FindBestApproachTile(
    const FAIBattleContext& Context, FVector TargetLocation) const
{
    if (Context.ReachableTiles.IsEmpty()) return FVector::ZeroVector;

    FVector BestTile  = FVector::ZeroVector;
    float   BestDist  = MAX_FLT;

    for (const FVector& Tile : Context.ReachableTiles)
    {
        const float Dist = FVector::Dist2D(Tile, TargetLocation);
        if (Dist < BestDist)
        {
            BestDist = Dist;
            BestTile = Tile;
        }
    }

    return BestTile;
}