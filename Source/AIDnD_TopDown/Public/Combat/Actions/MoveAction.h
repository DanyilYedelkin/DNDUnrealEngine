// Source/AIDnD_TopDown/Public/Combat/Actions/MoveAction.h
#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/CombatAction.h"
#include "MoveAction.generated.h"

/**
 * Move action — spends movement from CombatStats.
 * Actual pathfinding and locomotion handled by BP_PlayerController / AI.
 * This class only validates and deducts movement cost.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API UMoveAction : public UCombatAction
{
	GENERATED_BODY()

public:

	UMoveAction();

	/** Cost in feet to move to target. Set before calling Execute. */
	UPROPERTY(BlueprintReadWrite, Category = "Move")
	float MovementCostFeet = 0.f;

	virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
		FVector TargetLocation, ACombatCharacter* TargetCharacter) override;

	virtual bool CanExecute_Implementation(ACombatCharacter* Source,
		FText& OutReason) override;
};