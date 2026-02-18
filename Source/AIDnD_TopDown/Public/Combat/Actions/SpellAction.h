// Source/AIDnD_TopDown/Public/Combat/Actions/SpellAction.h
#pragma once

#include "CoreMinimal.h"
#include "Combat/Actions/CombatAction.h"
#include "SpellAction.generated.h"

UENUM(BlueprintType)
enum class ESpellDelivery : uint8
{
    AttackRoll  UMETA(DisplayName = "Attack Roll"),   // d20 vs AC
    SavingThrow UMETA(DisplayName = "Saving Throw"),  // target rolls save
    Automatic   UMETA(DisplayName = "Automatic")      // always hits (e.g. Magic Missile)
};

/**
 * Base class for all spells.
 * Handles spell slots, attack rolls vs AC, and saving throws.
 * Subclass this for specific spells (Fireball, Cure Wounds, etc.)
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class AIDND_TOPDOWN_API USpellAction : public UCombatAction
{
    GENERATED_BODY()

public:

    USpellAction();

    /** Spell slot level required (0 = cantrip) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spell",
        meta = (ClampMin = 0, ClampMax = 9))
    int32 SpellLevel = 1;

    /** How the spell targets */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spell")
    ESpellDelivery Delivery = ESpellDelivery::SavingThrow;

    /** For SavingThrow delivery: which ability the target saves with */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spell")
    EAbilityType SaveAbility = EAbilityType::Dexterity;

    /** Spell save DC = 8 + ProfBonus + SpellcastingMod (set per caster) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spell",
        meta = (ClampMin = 1))
    int32 SpellSaveDC = 13;

    /** Damage on hit/fail */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spell")
    FDamageRoll SpellDamage;

    /** Damage on successful save (usually half) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spell")
    bool bHalfDamageOnSave = true;

    /** AoE radius in feet (0 = single target) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Spell",
        meta = (ClampMin = 0.f))
    float AoERadius = 0.f;

    virtual FActionResult Execute_Implementation(ACombatCharacter* Source,
        FVector TargetLocation, ACombatCharacter* TargetCharacter) override;

    virtual FActionPreview GetPreviewData_Implementation(
        ACombatCharacter* Source) override;

protected:

    /** Apply spell effect to a single target. Override in subclasses. */
    UFUNCTION(BlueprintNativeEvent, Category = "Spell")
    FActionResult ApplySpellEffect(ACombatCharacter* Source,
        ACombatCharacter* Target, bool bSaveSucceeded);

    virtual FActionResult ApplySpellEffect_Implementation(ACombatCharacter* Source,
        ACombatCharacter* Target, bool bSaveSucceeded);

private:

    /** Collect all targets in AoE radius */
    TArray<ACombatCharacter*> GetAoETargets(FVector Center, float Radius,
        UWorld* World) const;
};