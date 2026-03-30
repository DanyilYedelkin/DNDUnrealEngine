// Source/AIDnD_TopDown/Public/Combat/BT/BTTask_Patrol.h
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_Patrol.generated.h"

UCLASS()
class AIDND_TOPDOWN_API UBTTask_Patrol : public UBTTaskNode
{
	GENERATED_BODY()

public:

	UBTTask_Patrol();
	
	UPROPERTY(EditAnywhere, Category = "Patrol",
		meta = (ClampMin = 10.f))
	float AcceptanceRadius = 100.f;
	
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector PatrolLocationKey;

protected:

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual void TickTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		float DeltaSeconds) override;

	virtual void OnTaskFinished(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory,
		EBTNodeResult::Type TaskResult) override;

	virtual FString GetStaticDescription() const override;
};