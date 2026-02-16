// CharacterStatsComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "CharacterStatsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHealthChanged, int32, NewHealth, int32, MaxHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDied);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AIDND_TOPDOWN_API UCharacterStatsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCharacterStatsComponent();

	// Stats
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
	FCharacterStats Stats;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	bool bIsPlayerControlled = false;

	// Delegates
	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Events")
	FOnCharacterDied OnCharacterDied;

	// Functions
	UFUNCTION(BlueprintCallable, Category = "Stats")
	void TakeDamage(int32 Damage);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	void Heal(int32 HealAmount);

	UFUNCTION(BlueprintCallable, Category = "Stats")
	bool IsAlive() const;

	UFUNCTION(BlueprintCallable, Category = "Stats")
	int32 RollInitiative();

	UFUNCTION(BlueprintCallable, Category = "Stats")
	int32 GetModifier(int32 StatValue) const;

protected:
	virtual void BeginPlay() override;
};