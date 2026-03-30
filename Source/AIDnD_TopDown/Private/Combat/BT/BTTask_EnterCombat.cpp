// Source/AIDnD_TopDown/Private/Combat/BT/BTTask_EnterCombat.cpp
#include "Combat/BT/BTTask_EnterCombat.h"
#include "Combat/AI/EnemyAIController.h"
#include "Combat/TurnManager.h"
#include "Combat/CombatLog.h"
#include "Kismet/GameplayStatics.h"

UBTTask_EnterCombat::UBTTask_EnterCombat()
{
	NodeName = TEXT("Enter Combat");
}

EBTNodeResult::Type UBTTask_EnterCombat::ExecuteTask(
	UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	ATurnManager* TM = ATurnManager::GetTurnManager(GetWorld());
	if (!TM) return EBTNodeResult::Failed;
	
	if (TM->IsCombatActive())
		return EBTNodeResult::Succeeded;
	
	TArray<AActor*> AllActors;
	UGameplayStatics::GetAllActorsWithInterface(
		GetWorld(),
		UCombatant::StaticClass(),
		AllActors);

	if (AllActors.IsEmpty())
		return EBTNodeResult::Failed;

	TM->StartCombat(AllActors);

	UE_LOG(LogCombat, Log,
		TEXT("BTTask_EnterCombat: Combat started with %d actors"),
		AllActors.Num());

	return EBTNodeResult::Succeeded;
}