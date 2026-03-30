// Source/AIDnD_TopDown/Private/Combat/CombatCharacter.cpp
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/ConditionComponent.h"
#include "Combat/DiceRoller.h"
#include "Combat/CombatLog.h"

ACombatCharacter::ACombatCharacter()
{
    PrimaryActorTick.bCanEverTick = false;

    CombatStats = CreateDefaultSubobject<UCombatStatsComponent>(TEXT("CombatStats"));
    ConditionComp = CreateDefaultSubobject<UConditionComponent>(TEXT("ConditionComp"));
}

void ACombatCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Bind death delegate
    if (CombatStats)
    {
        CombatStats->OnDeath.AddDynamic(this, &ACombatCharacter::HandleDeath);
    }
}

// ============================================================
//  ICombatant IMPLEMENTATION
// ============================================================

int32 ACombatCharacter::GetInitiativeRoll_Implementation()
{
    if (!CombatStats) return 0;
    const int32 DexMod = CombatStats->GetAbilityModifier(EAbilityType::Dexterity);
    return UDiceRoller::RollInitiative(DexMod);
}

int32 ACombatCharacter::GetActionPoints_Implementation()
{
    return CombatStats ? CombatStats->CurrentActions : 0;
}

int32 ACombatCharacter::GetBonusActionPoints_Implementation()
{
    return CombatStats ? CombatStats->CurrentBonusActions : 0;
}

float ACombatCharacter::GetMovementRemaining_Implementation()
{
    return CombatStats ? CombatStats->MovementRemaining : 0.f;
}

bool ACombatCharacter::IsAlive_Implementation()
{
    return CombatStats ? CombatStats->IsAlive() : false;
}

FText ACombatCharacter::GetCombatantName_Implementation()
{
    return CharacterName.IsEmpty()
        ? FText::FromString(GetName())
        : CharacterName;
}

UCombatStatsComponent* ACombatCharacter::GetCombatStats_Implementation()
{
    return CombatStats;
}

void ACombatCharacter::OnTurnStart_Implementation()
{
    if (CombatStats)
        CombatStats->OnTurnStart();

    if (ConditionComp)
        ConditionComp->OnTurnStart(0); // Round passed from TurnManager via event

    BP_OnTurnStarted();

    UE_LOG(LogCombat, Log,
        TEXT("ACombatCharacter: %s turn started"), *GetCombatantName_Implementation().ToString());
}

void ACombatCharacter::OnTurnEnd_Implementation()
{
    if (ConditionComp)
        ConditionComp->OnTurnEnd();

    if (CombatStats)
        CombatStats->OnTurnEnd();

    BP_OnTurnEnded();
}

bool ACombatCharacter::IsPlayerControlled_Implementation()
{
    return bIsPlayerControlled;
}

// ============================================================
//  COMBAT ACTIONS
// ============================================================

int32 ACombatCharacter::ReceiveDamage(int32 Amount, EDamageType DamageType)
{
    if (!CombatStats) return 0;

    const int32 ActualDamage = CombatStats->TakeDamage(Amount, DamageType);

    if (ActualDamage > 0)
    {
        BP_OnDamageTaken(ActualDamage, DamageType);

        if (!CombatStats->IsConscious())
            BP_OnKnockedDown();
    }

    return ActualDamage;
}

int32 ACombatCharacter::ReceiveHealing(int32 Amount)
{
    return CombatStats ? CombatStats->HealHP(Amount) : 0;
}

void ACombatCharacter::ApplyCondition(EConditionType Condition,
    int32 DurationRounds, AActor* Source)
{
    if (!ConditionComp) return;

    ConditionComp->ApplyCondition(Condition, DurationRounds, Source);
    BP_OnConditionApplied(Condition);
}

// ============================================================
//  QUERIES
// ============================================================

bool ACombatCharacter::HasCondition(EConditionType Condition) const
{
    return ConditionComp ? ConditionComp->HasCondition(Condition) : false;
}

float ACombatCharacter::GetHPPercent() const
{
    return CombatStats ? CombatStats->GetHPPercent() : 0.f;
}

int32 ACombatCharacter::GetCurrentHP() const
{
    return CombatStats ? CombatStats->CurrentHP : 0;
}

int32 ACombatCharacter::GetMaxHP() const
{
    return CombatStats ? CombatStats->MaxHP : 0;
}

int32 ACombatCharacter::GetArmorClass() const
{
    return CombatStats ? CombatStats->ArmorClass : 10;
}

ERollAdvantage ACombatCharacter::GetAttackAdvantage() const
{
    return ConditionComp
        ? ConditionComp->GetAttackRollModifier()
        : ERollAdvantage::Normal;
}

ERollAdvantage ACombatCharacter::GetDefenseAdvantage(bool bIsMeleeAttack) const
{
    if (!ConditionComp) return ERollAdvantage::Normal;

    ERollAdvantage Result = ConditionComp->GetDefenseRollModifier();

    // Prone: melee attackers have Advantage, ranged have Disadvantage
    if (ConditionComp->HasCondition(EConditionType::Prone))
    {
        const ERollAdvantage ProneEffect = bIsMeleeAttack
            ? ERollAdvantage::Advantage
            : ERollAdvantage::Disadvantage;

        // Merge with existing result (Adv + Dis = Normal per 5e)
        if (Result == ERollAdvantage::Normal)
            Result = ProneEffect;
        else if (Result != ProneEffect)
            Result = ERollAdvantage::Normal;
    }

    return Result;
}

// ============================================================
//  PRIVATE
// ============================================================

void ACombatCharacter::HandleDeath(AActor* DeadActor)
{
    BP_OnDeath();
    UE_LOG(LogCombat, Warning,
        TEXT("ACombatCharacter: %s has died"),
        *GetCombatantName_Implementation().ToString());
}