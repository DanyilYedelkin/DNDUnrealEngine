// Source/AIDnD_TopDown/Public/Combat/AI/LocalAIDecisionMaker.h
#pragma once

#include "CoreMinimal.h"
#include "Combat/AI/AIDecisionMaker.h"
#include "LocalAIDecisionMaker.generated.h"

/**
 * Simple heuristic AI. Fully synchronous.
 * Priority order:
 *   1. Attack weakest reachable enemy
 *   2. Move toward nearest enemy if out of range
 *   3. Dodge if HP < 30%
 *   4. End turn
 */
UCLASS(BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API ULocalAIDecisionMaker : public UAIDecisionMaker
{
	GENERATED_BODY()

public:

	/** HP threshold (0..1) below which AI prefers Dodge over attacking */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|Local",
		meta = (ClampMin = 0.f, ClampMax = 1.f))
	float LowHPThreshold = 0.30f;

	/** Melee range in feet */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "AI|Local",
		meta = (ClampMin = 0.f))
	float MeleeRangeFeet = 8.f;

	virtual void RequestDecision_Implementation(
		const FAIBattleContext& Context) override;

private:

	FAIDecision DecideAttack(const FAIBattleContext& Context) const;
	FAIDecision DecideMove(const FAIBattleContext& Context) const;
	FAIDecision DecideDodge(const FAIBattleContext& Context) const;

	/** Find the weakest (lowest HP%) living enemy */
	const FAITargetInfo* FindWeakestEnemy(
		const TArray<FAITargetInfo>& Enemies) const;

	/** Find the nearest enemy */
	const FAITargetInfo* FindNearestEnemy(
		const TArray<FAITargetInfo>& Enemies) const;

	/** True if any enemy is within melee range */
	bool HasEnemyInMeleeRange(const FAIBattleContext& Context) const;

	/** Find the reachable tile closest to a target location */
	FVector FindBestApproachTile(const FAIBattleContext& Context,
		FVector TargetLocation) const;
};