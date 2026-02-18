// Source/AIDnD_TopDown/Public/Combat/Actions/MeleeAttackAction.h
#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/CombatAction.h"
#include "MeleeAttackAction.generated.h"

/**
 * Standard melee attack: d20 + STR/DEX mod vs target AC.
 * Uses STR by default; set bUseFinesse=true for DEX (Finesse weapons).
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API UMeleeAttackAction : public UCombatAction
{
	GENERATED_BODY()

public:

	UMeleeAttackAction();

	/** If true, uses DEX modifier instead of STR (Finesse) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Melee")
	bool bUseFinesse = false;

	/** True if character is proficient with this weapon */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Melee")
	bool bProficient = true;

	/** Damage dealt on a hit */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Melee")
	FDamageRoll WeaponDamage;

	virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
		FVector TargetLocation, ACombatCharacter* TargetCharacter) override;

	virtual bool CanExecute_Implementation(ACombatCharacter* Source,
		FText& OutReason) override;
};