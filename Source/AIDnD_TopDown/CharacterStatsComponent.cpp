// CharacterStatsComponent.cpp
#include "CharacterStatsComponent.h"
#include "Kismet/KismetMathLibrary.h"

UCharacterStatsComponent::UCharacterStatsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCharacterStatsComponent::BeginPlay()
{
	Super::BeginPlay();
    
	// Initialize current HP to max HP
	Stats.CurrentHP = Stats.MaxHP;
}

void UCharacterStatsComponent::TakeDamage(int32 Damage)
{
	if (!IsAlive()) return;

	Stats.CurrentHP = FMath::Clamp(Stats.CurrentHP - Damage, 0, Stats.MaxHP);
    
	// Broadcast health changed event
	OnHealthChanged.Broadcast(Stats.CurrentHP, Stats.MaxHP);

	// Check if character died
	if (Stats.CurrentHP <= 0)
	{
		OnCharacterDied.Broadcast();
	}
}

void UCharacterStatsComponent::Heal(int32 HealAmount)
{
	if (!IsAlive()) return;

	Stats.CurrentHP = FMath::Clamp(Stats.CurrentHP + HealAmount, 0, Stats.MaxHP);
	OnHealthChanged.Broadcast(Stats.CurrentHP, Stats.MaxHP);
}

bool UCharacterStatsComponent::IsAlive() const
{
	return Stats.CurrentHP > 0;
}

int32 UCharacterStatsComponent::RollInitiative()
{
	// Roll d20 + Dexterity modifier
	int32 Roll = FMath::RandRange(1, 20);
	int32 DexModifier = GetModifier(Stats.Dexterity);
	Stats.Initiative = Roll + DexModifier;
    
	return Stats.Initiative;
}

int32 UCharacterStatsComponent::GetModifier(int32 StatValue) const
{
	return (StatValue - 10) / 2;
}