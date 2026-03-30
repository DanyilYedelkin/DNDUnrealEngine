// Source/AIDnD_TopDown/Public/Combat/BT/BTTask_EnterCombat.h
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_EnterCombat.generated.h"

UCLASS()
class AIDND_TOPDOWN_API UBTTask_EnterCombat : public UBTTaskNode
{
	GENERATED_BODY()

public:
	UBTTask_EnterCombat();

protected:
	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;
};