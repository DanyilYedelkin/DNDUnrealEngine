// Source/AIDnD_TopDown/Public/Combat/BT/BTTask_DetectPlayer.h
#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BTTask_DetectPlayer.generated.h"

UCLASS()
class AIDND_TOPDOWN_API UBTTask_DetectPlayer : public UBTTaskNode
{
	GENERATED_BODY()

public:

	UBTTask_DetectPlayer();
	
	UPROPERTY(EditAnywhere, Category = "Detection",
		meta = (ClampMin = 0.f))
	float DetectionRadius = 1500.f;
	
	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector TargetActorKey;

	UPROPERTY(EditAnywhere, Category = "Blackboard")
	FBlackboardKeySelector IsInCombatKey;

protected:

	virtual EBTNodeResult::Type ExecuteTask(
		UBehaviorTreeComponent& OwnerComp,
		uint8* NodeMemory) override;

	virtual FString GetStaticDescription() const override;
};