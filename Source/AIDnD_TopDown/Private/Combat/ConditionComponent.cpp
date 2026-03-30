// Source/AIDnD_TopDown/Private/Combat/ConditionComponent.cpp
#include "Combat/ConditionComponent.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/CombatLog.h"

UConditionComponent::UConditionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

// ============================================================
//  APPLY / REMOVE
// ============================================================

void UConditionComponent::ApplyCondition(EConditionType Condition,
    int32 Duration, AActor* Source)
{
    if (Condition == EConditionType::None) return;

    // Update duration if already active
    for (FActiveCondition& Existing : ActiveConditions)
    {
        if (Existing.Type == Condition)
        {
            Existing.DurationRounds = FMath::Max(Existing.DurationRounds, Duration);
            UE_LOG(LogCombat, Log,
                TEXT("%s: Condition %s duration refreshed to %d rounds"),
                *GetOwner()->GetName(),
                *UEnum::GetValueAsString(Condition),
                Existing.DurationRounds);
            return;
        }
    }

    // Add new condition
    FActiveCondition NewCondition;
    NewCondition.Type           = Condition;
    NewCondition.DurationRounds = Duration;
    NewCondition.Source         = Source;
    ActiveConditions.Add(NewCondition);

    UE_LOG(LogCombat, Log,
        TEXT("%s: Condition %s applied (%d rounds)"),
        *GetOwner()->GetName(),
        *UEnum::GetValueAsString(Condition),
        Duration);

    OnConditionApplied.Broadcast(Condition, Duration);
}

void UConditionComponent::RemoveCondition(EConditionType Condition)
{
    const int32 Removed = ActiveConditions.RemoveAll(
        [Condition](const FActiveCondition& C) { return C.Type == Condition; });

    if (Removed > 0)
    {
        UE_LOG(LogCombat, Log,
            TEXT("%s: Condition %s removed"),
            *GetOwner()->GetName(),
            *UEnum::GetValueAsString(Condition));

        OnConditionRemoved.Broadcast(Condition);
    }
}

void UConditionComponent::RemoveAllConditions()
{
    TArray<EConditionType> ToRemove;
    for (const FActiveCondition& C : ActiveConditions)
        ToRemove.Add(C.Type);

    ActiveConditions.Empty();

    for (EConditionType Type : ToRemove)
        OnConditionRemoved.Broadcast(Type);
}

// ============================================================
//  QUERIES
// ============================================================

bool UConditionComponent::HasCondition(EConditionType Condition) const
{
    return ActiveConditions.ContainsByPredicate(
        [Condition](const FActiveCondition& C) { return C.Type == Condition; });
}

FString UConditionComponent::GetConditionSummary() const
{
    if (ActiveConditions.IsEmpty()) return TEXT("None");

    TArray<FString> Names;
    for (const FActiveCondition& C : ActiveConditions)
    {
        Names.Add(FString::Printf(TEXT("%s(%d)"),
            *UEnum::GetValueAsString(C.Type), C.DurationRounds));
    }
    return FString::Join(Names, TEXT(", "));
}

// ============================================================
//  ROLL MODIFIERS
// ============================================================

ERollAdvantage UConditionComponent::GetAttackRollModifier() const
{
    ERollAdvantage Result = ERollAdvantage::Normal;

    // Poisoned: Disadvantage on attack rolls
    if (HasCondition(EConditionType::Poisoned))
        Result = CombineAdvantage(Result, ERollAdvantage::Disadvantage);

    // Blinded: Disadvantage on attacks
    if (HasCondition(EConditionType::Blinded))
        Result = CombineAdvantage(Result, ERollAdvantage::Disadvantage);

    // Invisible: Advantage on attacks
    if (HasCondition(EConditionType::Invisible))
        Result = CombineAdvantage(Result, ERollAdvantage::Advantage);

    return Result;
}

ERollAdvantage UConditionComponent::GetDefenseRollModifier() const
{
    ERollAdvantage Result = ERollAdvantage::Normal;

    // Prone: melee attacks against have Advantage, ranged have Disadvantage
    // Returned as Advantage since most common case is melee
    // Caller should handle ranged separately
    if (HasCondition(EConditionType::Prone))
        Result = CombineAdvantage(Result, ERollAdvantage::Advantage);

    // Stunned: attacks against have Advantage
    if (HasCondition(EConditionType::Stunned))
        Result = CombineAdvantage(Result, ERollAdvantage::Advantage);

    // Paralyzed: attacks within 5 feet are automatic crits
    if (HasCondition(EConditionType::Paralyzed))
        Result = CombineAdvantage(Result, ERollAdvantage::Advantage);

    // Blinded: attackers have Advantage against blinded targets
    if (HasCondition(EConditionType::Blinded))
        Result = CombineAdvantage(Result, ERollAdvantage::Advantage);

    // Invisible: attackers have Disadvantage against invisible
    if (HasCondition(EConditionType::Invisible))
        Result = CombineAdvantage(Result, ERollAdvantage::Disadvantage);

    return Result;
}

ERollAdvantage UConditionComponent::GetAbilityCheckModifier() const
{
    ERollAdvantage Result = ERollAdvantage::Normal;

    if (HasCondition(EConditionType::Poisoned))
        Result = CombineAdvantage(Result, ERollAdvantage::Disadvantage);

    return Result;
}

bool UConditionComponent::IsIncapacitated() const
{
    return HasCondition(EConditionType::Stunned)
        || HasCondition(EConditionType::Paralyzed);
}

// ============================================================
//  TURN LIFECYCLE
// ============================================================

void UConditionComponent::OnTurnStart(int32 CurrentRound)
{
    // Apply per-turn damage effects
    if (HasCondition(EConditionType::Burning))
    {
        UCombatStatsComponent* Stats =
            GetOwner()->FindComponentByClass<UCombatStatsComponent>();
        if (Stats)
        {
            const int32 FireDamage = FMath::RandRange(1, 4); // 1d4 fire
            Stats->TakeDamage(FireDamage, EDamageType::Fire);
            UE_LOG(LogCombat, Log,
                TEXT("%s takes %d fire damage from Burning"),
                *GetOwner()->GetName(), FireDamage);
        }
    }

    if (HasCondition(EConditionType::Bleeding))
    {
        UCombatStatsComponent* Stats =
            GetOwner()->FindComponentByClass<UCombatStatsComponent>();
        if (Stats)
        {
            const int32 BleedDamage = FMath::RandRange(1, 4); // 1d4 piercing
            Stats->TakeDamage(BleedDamage, EDamageType::Piercing);
            UE_LOG(LogCombat, Log,
                TEXT("%s takes %d piercing damage from Bleeding"),
                *GetOwner()->GetName(), BleedDamage);
        }
    }
}

void UConditionComponent::OnTurnEnd()
{
    // Decrement duration and remove expired conditions
    TArray<EConditionType> Expired;

    for (FActiveCondition& C : ActiveConditions)
    {
        if (C.DurationRounds > 0) // -1 = permanent
        {
            C.DurationRounds--;
            if (C.DurationRounds == 0)
                Expired.Add(C.Type);
        }
    }

    for (EConditionType Type : Expired)
        RemoveCondition(Type);
}

// ============================================================
//  PRIVATE
// ============================================================

ERollAdvantage UConditionComponent::CombineAdvantage(
    ERollAdvantage A, ERollAdvantage B)
{
    // D&D 5e rule: Advantage and Disadvantage cancel out to Normal
    if (A == ERollAdvantage::Normal) return B;
    if (B == ERollAdvantage::Normal) return A;
    if (A == B) return A; // Both same = keep
    return ERollAdvantage::Normal; // Advantage + Disadvantage = Normal
}