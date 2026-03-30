// Source/AIDnD_TopDown/Public/Combat/Actions/UtilityActions.h
#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/CombatAction.h"
#include "UtilityActions.generated.h"

/**
 * Dodge: until next turn, all attacks against this character
 * are made with Disadvantage, and DEX saves with Advantage.
 * Implemented via applying a "Dodging" flag on CombatStats.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API UDodgeAction : public UCombatAction
{
	GENERATED_BODY()
public:
	UDodgeAction();
	virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
		FVector TargetLocation, ACombatCharacter* TargetCharacter) override;
};

/**
 * Dash: gain extra movement equal to your Speed this turn.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API UDashAction : public UCombatAction
{
	GENERATED_BODY()
public:
	UDashAction();
	virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
		FVector TargetLocation, ACombatCharacter* TargetCharacter) override;
};

/**
 * Help: give an ally Advantage on their next attack or ability check.
 * Implemented as applying a condition to the ally.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API UHelpAction : public UCombatAction
{
	GENERATED_BODY()
public:
	UHelpAction();
	virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
		FVector TargetLocation, ACombatCharacter* TargetCharacter) override;

	virtual bool CanExecute_Implementation(ACombatCharacter* Source,
		FText& OutReason) override;
};