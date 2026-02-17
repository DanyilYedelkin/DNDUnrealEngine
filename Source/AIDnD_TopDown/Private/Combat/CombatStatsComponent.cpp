// Source/AIDnD_TopDown/Private/Combat/CombatStatsComponent.cpp
#include "Combat/CombatStatsComponent.h"
#include "Combat/DiceRoller.h"
#include "Combat/CombatLog.h"

UCombatStatsComponent::UCombatStatsComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UCombatStatsComponent::BeginPlay()
{
    Super::BeginPlay();

    // Sync current HP with max on spawn
    CurrentHP          = MaxHP;
    MovementRemaining  = Speed;
    CurrentActions     = MaxActions;
    CurrentBonusActions = MaxBonusActions;
    CurrentReactions   = MaxReactions;
}

// ============================================================
//  COMPUTED GETTERS
// ============================================================

int32 UCombatStatsComponent::GetProficiencyBonus() const
{
    if (ProficiencyBonusOverride > 0)
        return ProficiencyBonusOverride;

    return FDnDStats::CalcProficiencyBonus(CharacterLevel);
}

int32 UCombatStatsComponent::GetAbilityModifier(EAbilityType Ability) const
{
    return AbilityScores.GetModifier(Ability);
}

EDamageAffinity UCombatStatsComponent::GetDamageAffinity(EDamageType DamageType) const
{
    for (const FDamageAffinityEntry& Entry : DamageAffinities)
    {
        if (Entry.DamageType == DamageType)
            return Entry.Affinity;
    }
    return EDamageAffinity::Normal;
}

float UCombatStatsComponent::GetHPPercent() const
{
    if (MaxHP <= 0) return 0.f;
    return FMath::Clamp((float)CurrentHP / (float)MaxHP, 0.f, 1.f);
}

// ============================================================
//  HP MODIFICATION
// ============================================================

int32 UCombatStatsComponent::TakeDamage(int32 Amount, EDamageType DamageType)
{
    if (Amount <= 0) return 0;
    if (DeathSaveState.bIsDead) return 0;

    int32 FinalDamage = ApplyDamageAffinity(Amount, DamageType);

    // Absorb with TempHP first
    if (TempHP > 0)
    {
        const int32 Absorbed = FMath::Min(TempHP, FinalDamage);
        TempHP      -= Absorbed;
        FinalDamage -= Absorbed;

        UE_LOG(LogCombat, Log,
            TEXT("%s: TempHP absorbed %d damage (%d TempHP remaining)"),
            *GetOwner()->GetName(), Absorbed, TempHP);
    }

    if (FinalDamage <= 0) return 0;

    CurrentHP = FMath::Max(0, CurrentHP - FinalDamage);

    UE_LOG(LogCombat, Log,
        TEXT("%s took %d %s damage → HP: %d/%d"),
        *GetOwner()->GetName(),
        FinalDamage,
        *UEnum::GetValueAsString(DamageType),
        CurrentHP, MaxHP);

    OnHPChanged.Broadcast(CurrentHP, MaxHP);

    if (CurrentHP == 0)
        HandleZeroHP();

    return FinalDamage;
}

int32 UCombatStatsComponent::HealHP(int32 Amount)
{
    if (Amount <= 0) return 0;
    if (DeathSaveState.bIsDead) return 0;

    const int32 OldHP  = CurrentHP;
    CurrentHP = FMath::Clamp(CurrentHP + Amount, 0, MaxHP);
    const int32 Healed = CurrentHP - OldHP;

    // Healing stabilizes a dying character
    if (DeathSaveState.bIsDying && CurrentHP > 0)
        Stabilize();

    UE_LOG(LogCombat, Log,
        TEXT("%s healed %d HP → HP: %d/%d"),
        *GetOwner()->GetName(), Healed, CurrentHP, MaxHP);

    OnHPChanged.Broadcast(CurrentHP, MaxHP);
    return Healed;
}

void UCombatStatsComponent::SetTempHP(int32 Amount)
{
    // TempHP doesn't stack — keep the higher value
    TempHP = FMath::Max(TempHP, Amount);
    UE_LOG(LogCombat, Log,
        TEXT("%s TempHP set to %d"), *GetOwner()->GetName(), TempHP);
}

// ============================================================
//  D&D 5e ROLLS
// ============================================================

bool UCombatStatsComponent::SavingThrow(EAbilityType Ability, int32 DC,
    ERollAdvantage Advantage)
{
    const int32 Modifier = AbilityScores.GetModifier(Ability);

    // Add proficiency bonus if proficient in this save
    int32 TotalBonus = Modifier;
    if (SavingThrowProficiencies.Contains(Ability))
        TotalBonus += GetProficiencyBonus();

    FDiceResult Roll;
    switch (Advantage)
    {
        case ERollAdvantage::Advantage:
            Roll = UDiceRoller::RollWithAdvantage();
            break;
        case ERollAdvantage::Disadvantage:
            Roll = UDiceRoller::RollWithDisadvantage();
            break;
        default:
            Roll = UDiceRoller::RollD20();
            break;
    }

    const int32 FinalResult = Roll.NaturalRoll + TotalBonus;
    const bool  bSuccess    = (FinalResult >= DC);

    UE_LOG(LogCombat, Log,
        TEXT("%s Saving Throw (%s): d20(%d) + %d = %d vs DC %d → %s"),
        *GetOwner()->GetName(),
        *UEnum::GetValueAsString(Ability),
        Roll.NaturalRoll, TotalBonus, FinalResult, DC,
        bSuccess ? TEXT("SUCCESS") : TEXT("FAILURE"));

    return bSuccess;
}

FDiceResult UCombatStatsComponent::AbilityCheck(EAbilityType Ability,
    ERollAdvantage Advantage)
{
    FDiceResult Roll;
    switch (Advantage)
    {
        case ERollAdvantage::Advantage:
            Roll = UDiceRoller::RollWithAdvantage();
            break;
        case ERollAdvantage::Disadvantage:
            Roll = UDiceRoller::RollWithDisadvantage();
            break;
        default:
            Roll = UDiceRoller::RollD20();
            break;
    }

    const int32 Modifier = AbilityScores.GetModifier(Ability);
    Roll.Total = Roll.NaturalRoll + Modifier;

    UE_LOG(LogCombat, Log,
        TEXT("%s Ability Check (%s): d20(%d) + %d = %d"),
        *GetOwner()->GetName(),
        *UEnum::GetValueAsString(Ability),
        Roll.NaturalRoll, Modifier, Roll.Total);

    return Roll;
}

void UCombatStatsComponent::RollDeathSavingThrow()
{
    if (!DeathSaveState.bIsDying) return;

    const FDiceResult Roll = UDiceRoller::RollD20();

    // Nat-20: regain 1 HP immediately
    if (Roll.bIsCritical)
    {
        UE_LOG(LogCombat, Log,
            TEXT("%s Death Save: NAT 20 — regains 1 HP!"), *GetOwner()->GetName());
        HealHP(1);
        return;
    }

    // Nat-1 counts as 2 failures
    if (Roll.bIsCritFail)
    {
        DeathSaveState.Failures += 2;
        UE_LOG(LogCombat, Log,
            TEXT("%s Death Save: NAT 1 — 2 failures (%d/3)"),
            *GetOwner()->GetName(), DeathSaveState.Failures);
    }
    else if (Roll.NaturalRoll >= 10)
    {
        DeathSaveState.Successes++;
        UE_LOG(LogCombat, Log,
            TEXT("%s Death Save: %d — SUCCESS (%d/3)"),
            *GetOwner()->GetName(), Roll.NaturalRoll, DeathSaveState.Successes);
    }
    else
    {
        DeathSaveState.Failures++;
        UE_LOG(LogCombat, Log,
            TEXT("%s Death Save: %d — FAILURE (%d/3)"),
            *GetOwner()->GetName(), Roll.NaturalRoll, DeathSaveState.Failures);
    }

    OnDeathSavingThrow.Broadcast(Roll.NaturalRoll >= 10, DeathSaveState.Successes);

    // 3 successes → stabilized
    if (DeathSaveState.Successes >= 3)
    {
        Stabilize();
        return;
    }

    // 3 failures → dead
    if (DeathSaveState.Failures >= 3)
    {
        DeathSaveState.bIsDead  = true;
        DeathSaveState.bIsDying = false;
        UE_LOG(LogCombat, Warning,
            TEXT("%s has DIED (3 death save failures)"), *GetOwner()->GetName());
        OnDeath.Broadcast(GetOwner());
    }
}

void UCombatStatsComponent::Stabilize()
{
    DeathSaveState.bIsStabilized = true;
    DeathSaveState.bIsDying      = false;
    UE_LOG(LogCombat, Log,
        TEXT("%s has been stabilized"), *GetOwner()->GetName());
    OnStabilized.Broadcast();
}

// ============================================================
//  TURN MANAGEMENT
// ============================================================

void UCombatStatsComponent::OnTurnStart()
{
    // Reset action economy
    CurrentActions      = MaxActions;
    CurrentBonusActions = MaxBonusActions;
    CurrentReactions    = MaxReactions;
    MovementRemaining   = Speed;

    // Death saving throw if dying
    if (DeathSaveState.bIsDying)
        RollDeathSavingThrow();

    UE_LOG(LogCombat, Verbose,
        TEXT("%s turn started — Actions:%d BonusActions:%d Movement:%.0f"),
        *GetOwner()->GetName(),
        CurrentActions, CurrentBonusActions, MovementRemaining);
}

void UCombatStatsComponent::OnTurnEnd()
{
    UE_LOG(LogCombat, Verbose,
        TEXT("%s turn ended"), *GetOwner()->GetName());
}

bool UCombatStatsComponent::SpendAction(EActionType ActionType, int32 Cost)
{
    switch (ActionType)
    {
        case EActionType::Action:
            if (CurrentActions < Cost) return false;
            CurrentActions -= Cost;
            return true;

        case EActionType::BonusAction:
            if (CurrentBonusActions < Cost) return false;
            CurrentBonusActions -= Cost;
            return true;

        case EActionType::Reaction:
            if (CurrentReactions < Cost) return false;
            CurrentReactions -= Cost;
            return true;

        case EActionType::FreeAction:
            return true; // Free actions always succeed

        default:
            return false;
    }
}

bool UCombatStatsComponent::SpendMovement(float Feet)
{
    if (MovementRemaining < Feet) return false;
    MovementRemaining -= Feet;
    return true;
}

void UCombatStatsComponent::AddMovement(float Feet)
{
    MovementRemaining += Feet;
}

// ============================================================
//  PRIVATE
// ============================================================

int32 UCombatStatsComponent::ApplyDamageAffinity(int32 RawDamage,
    EDamageType DamageType) const
{
    const EDamageAffinity Affinity = GetDamageAffinity(DamageType);

    switch (Affinity)
    {
        case EDamageAffinity::Immune:
            return 0;
        case EDamageAffinity::Resistant:
            return FMath::FloorToInt(RawDamage / 2.f); // floor per 5e rules
        case EDamageAffinity::Vulnerable:
            return RawDamage * 2;
        default:
            return RawDamage;
    }
}

void UCombatStatsComponent::HandleZeroHP()
{
    // Player characters and important NPCs roll death saves
    // Minions just die — this can be configured per-character in BP
    DeathSaveState.bIsDying = true;
    DeathSaveState.Successes = 0;
    DeathSaveState.Failures  = 0;

    UE_LOG(LogCombat, Warning,
        TEXT("%s dropped to 0 HP — now dying"), *GetOwner()->GetName());
}