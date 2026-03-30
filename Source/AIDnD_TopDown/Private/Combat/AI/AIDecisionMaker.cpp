// Source/AIDnD_TopDown/Private/Combat/AI/AIDecisionMaker.cpp
#include "Combat/AI/AIDecisionMaker.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/ConditionComponent.h"
#include "Combat/MovementGridManager.h"
#include "Combat/CombatLog.h"

FAIBattleContext UAIDecisionMaker::BuildContext(ACombatCharacter* InSelf,
    const TArray<ACombatCharacter*>& AllCombatants)
{
    FAIBattleContext Context;
    if (!InSelf || !InSelf->CombatStats) return Context;

    Context.SelfCharacter = InSelf;
    Context.SelfStats     = InSelf->CombatStats->AbilityScores;
    Context.SelfHP        = InSelf->GetCurrentHP();
    Context.SelfMaxHP     = InSelf->GetMaxHP();

    if (InSelf->ConditionComp)
    {
        for (const FActiveCondition& C : InSelf->ConditionComp->GetActiveConditions())
            Context.SelfConditions.Add(C.Type);
    }

    for (ACombatCharacter* Other : AllCombatants)
    {
        if (!Other || Other == InSelf) continue;
        if (!Other->CombatStats || !Other->CombatStats->IsAlive()) continue;

        FAITargetInfo Info;
        Info.Character   = Other;
        Info.HPPercent   = Other->GetHPPercent();
        Info.CurrentHP   = Other->GetCurrentHP();
        Info.ArmorClass  = Other->GetArmorClass();
        Info.DistanceFeet = FVector::Dist(
            InSelf->GetActorLocation(),
            Other->GetActorLocation()) / 30.48f;
        Info.bIsAlive    = true;

        if (Other->ConditionComp)
        {
            for (const FActiveCondition& C : Other->ConditionComp->GetActiveConditions())
                Info.ActiveConditions.Add(C.Type);
        }

        if (Other->bIsPlayerControlled != InSelf->bIsPlayerControlled)
            Context.Enemies.Add(Info);
        else
            Context.Allies.Add(Info);
    }

    // Reachable tiles from GridManager
    AMovementGridManager* Grid =
        AMovementGridManager::GetGridManager(InSelf);
    if (Grid)
        Context.ReachableTiles = Grid->GetReachableTiles(InSelf);

    return Context;
}