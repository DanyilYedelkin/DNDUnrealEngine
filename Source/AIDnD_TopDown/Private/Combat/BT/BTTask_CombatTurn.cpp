// Source/AIDnD_TopDown/Private/Combat/BT/BTTask_CombatTurn.cpp
#include "Combat/BT/BTTask_CombatTurn.h"
#include "Combat/AI/EnemyAIController.h"
#include "Combat/TurnManager.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatLog.h"

UBTTask_CombatTurn::UBTTask_CombatTurn()
{
    NodeName    = TEXT("Combat Turn");
    bNotifyTick = true;
}

EBTNodeResult::Type UBTTask_CombatTurn::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AEnemyAIController* Controller =
        Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
    if (!Controller) return EBTNodeResult::Failed;

    ATurnManager* TM = ATurnManager::GetTurnManager(GetWorld());
    if (!TM || !TM->IsCombatActive())
        return EBTNodeResult::Failed;
    
    return EBTNodeResult::InProgress;
}

void UBTTask_CombatTurn::TickTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory, float DeltaSeconds)
{
    AEnemyAIController* Controller =
        Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
    if (!Controller)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    ATurnManager* TM = ATurnManager::GetTurnManager(GetWorld());
    if (!TM || !TM->IsCombatActive())
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
        return;
    }
    
    FCombatantInitiative Current = TM->GetCurrentTurnCombatant();
    ACombatCharacter* TurnChar =
        Cast<ACombatCharacter>(Current.Combatant.GetObject());
    ACombatCharacter* OurChar =
        Cast<ACombatCharacter>(Controller->GetPawn());

    if (TurnChar && OurChar && TurnChar == OurChar)
    {
        if (!Controller->IsTurnActive())
            Controller->StartAITurn(OurChar);
    }
    else if (Controller->IsTurnActive() == false)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}