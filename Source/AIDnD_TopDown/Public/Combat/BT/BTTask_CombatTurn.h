// Source/AIDnD_TopDown/Public/Combat/BT/BTTask_CombatTurn.h
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_CombatTurn.generated.h"

UCLASS()
class AIDND_TOPDOWN_API UBTTask_CombatTurn : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_CombatTurn();

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual void TickTask(UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory, float DeltaSeconds) override;
};