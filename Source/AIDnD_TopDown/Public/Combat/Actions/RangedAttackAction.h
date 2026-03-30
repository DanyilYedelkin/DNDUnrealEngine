// Source/AIDnD_TopDown/Public/Combat/Actions/RangedAttackAction.h
#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/CombatAction.h"
#include "RangedAttackAction.generated.h"

/**
 * Ranged attack: d20 + DEX mod vs AC.
 * Disadvantage if enemy is within 5 feet (in melee).
 * Long range also imposes Disadvantage.
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API URangedAttackAction : public UCombatAction
{
	GENERATED_BODY()

public:

	URangedAttackAction();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ranged")
	bool bProficient = true;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ranged")
	FDamageRoll WeaponDamage;

	/** Normal range in feet */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ranged",
		meta = (ClampMin = 0.f))
	float NormalRange = 80.f;

	/** Long range in feet — attacks beyond NormalRange get Disadvantage */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Ranged",
		meta = (ClampMin = 0.f))
	float LongRange = 320.f;

	virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
		FVector TargetLocation, ACombatCharacter* TargetCharacter) override;

	virtual bool CanExecute_Implementation(ACombatCharacter* Source,
		FText& OutReason) override;

private:

	/** Check if any enemy is within 5 feet of source (imposes Disadvantage) */
	bool HasEnemyInMelee(ACombatCharacter* Source) const;
};