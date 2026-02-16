// CombatActionComponent.cpp
#include "CombatActionComponent.h"
#include "CombatCharacter.h"
#include "CharacterStatsComponent.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

UCombatActionComponent::UCombatActionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    RemainingMovementThisTurn = 0.0f;
}

void UCombatActionComponent::BeginPlay()
{
    Super::BeginPlay();
    
    OwnerCharacter = Cast<ACombatCharacter>(GetOwner());
    
    if (OwnerCharacter && OwnerCharacter->StatsComponent)
    {
        RemainingMovementThisTurn = OwnerCharacter->StatsComponent->Stats.MovementSpeed * 100.0f; // Convert to cm
    }
}

bool UCombatActionComponent::ExecuteAction(EActionType ActionType, AActor* Target)
{
    if (!OwnerCharacter || !CanExecuteAction(ActionType))
    {
        return false;
    }

    bool bSuccess = false;

    switch (ActionType)
    {
        case EActionType::Attack:
            bSuccess = ExecuteAttackAction(Target);
            if (bSuccess) OwnerCharacter->bHasUsedAction = true;
            break;

        case EActionType::Dash:
            bSuccess = ExecuteDashAction();
            if (bSuccess) OwnerCharacter->bHasUsedAction = true;
            break;

        case EActionType::EndTurn:
            OwnerCharacter->EndTurn();
            bSuccess = true;
            break;

        default:
            break;
    }

    OnActionExecuted.Broadcast(ActionType, bSuccess);
    return bSuccess;
}

bool UCombatActionComponent::CanExecuteAction(EActionType ActionType) const
{
    if (!OwnerCharacter || !OwnerCharacter->bIsMyTurn)
    {
        return false;
    }

    switch (ActionType)
    {
        case EActionType::Attack:
        case EActionType::Dash:
            return !OwnerCharacter->bHasUsedAction;

        case EActionType::Move:
            return !OwnerCharacter->bHasMoved && RemainingMovementThisTurn > 0;

        case EActionType::EndTurn:
            return true;

        default:
            return false;
    }
}

TArray<AActor*> UCombatActionComponent::GetValidTargetsInRange(float Range)
{
    TArray<AActor*> ValidTargets;

    if (!OwnerCharacter)
    {
        return ValidTargets;
    }

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatCharacter::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        if (Actor == OwnerCharacter)
        {
            continue;
        }

        ACombatCharacter* TargetCharacter = Cast<ACombatCharacter>(Actor);
        if (TargetCharacter && TargetCharacter->StatsComponent && TargetCharacter->StatsComponent->IsAlive())
        {
            float Distance = FVector::Dist(OwnerCharacter->GetActorLocation(), Actor->GetActorLocation());
            
            if (Distance <= Range * 100.0f) // Convert meters to cm
            {
                ValidTargets.Add(Actor);
            }
        }
    }

    return ValidTargets;
}

bool UCombatActionComponent::PerformAttack(ACombatCharacter* Target)
{
    if (!OwnerCharacter || !Target || !Target->StatsComponent)
    {
        return false;
    }

    // Roll attack (d20 + modifiers)
    int32 AttackRoll = RollAttack();
    int32 TargetAC = Target->StatsComponent->Stats.ArmorClass;

    // Log attack roll
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Cyan,
            FString::Printf(TEXT("Attack Roll: %d vs AC: %d"), AttackRoll, TargetAC));
    }

    // Check if attack hits
    if (AttackRoll >= TargetAC)
    {
        // Roll damage
        int32 Damage = RollDamage(1, 8); // Default: 1d8
        
        if (OwnerCharacter->StatsComponent)
        {
            int32 StrModifier = OwnerCharacter->StatsComponent->GetModifier(
                OwnerCharacter->StatsComponent->Stats.Strength);
            Damage += StrModifier;
        }

        Damage = FMath::Max(1, Damage); // Minimum 1 damage

        // Apply damage
        Target->StatsComponent->TakeDamage(Damage);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
                FString::Printf(TEXT("HIT! Damage: %d"), Damage));
        }

        return true;
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Yellow, TEXT("MISS!"));
        }
        return false;
    }
}

int32 UCombatActionComponent::RollAttack() const
{
    int32 D20Roll = FMath::RandRange(1, 20);
    
    int32 Modifier = 0;
    if (OwnerCharacter && OwnerCharacter->StatsComponent)
    {
        Modifier = OwnerCharacter->StatsComponent->GetModifier(
            OwnerCharacter->StatsComponent->Stats.Strength);
    }

    return D20Roll + Modifier;
}

int32 UCombatActionComponent::RollDamage(int32 DiceCount, int32 DiceSize) const
{
    int32 TotalDamage = 0;
    
    for (int32 i = 0; i < DiceCount; i++)
    {
        TotalDamage += FMath::RandRange(1, DiceSize);
    }

    return TotalDamage;
}

float UCombatActionComponent::GetRemainingMovement() const
{
    return RemainingMovementThisTurn;
}

void UCombatActionComponent::ConsumeMovement(float Distance)
{
    RemainingMovementThisTurn = FMath::Max(0.0f, RemainingMovementThisTurn - Distance);
}

void UCombatActionComponent::ResetMovement()
{
    if (OwnerCharacter && OwnerCharacter->StatsComponent)
    {
        RemainingMovementThisTurn = OwnerCharacter->StatsComponent->Stats.MovementSpeed * 100.0f;
    }
}

bool UCombatActionComponent::ExecuteAttackAction(AActor* Target)
{
    ACombatCharacter* TargetCharacter = Cast<ACombatCharacter>(Target);
    
    if (!TargetCharacter)
    {
        return false;
    }

    return PerformAttack(TargetCharacter);
}

bool UCombatActionComponent::ExecuteDashAction()
{
    // Dash doubles your movement speed for the turn
    if (OwnerCharacter && OwnerCharacter->StatsComponent)
    {
        float BaseMovement = OwnerCharacter->StatsComponent->Stats.MovementSpeed * 100.0f;
        RemainingMovementThisTurn += BaseMovement;
        
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, 
                TEXT("Dash! Movement doubled!"));
        }
        
        return true;
    }

    return false;
}