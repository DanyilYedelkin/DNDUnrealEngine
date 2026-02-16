// CombatActionComponent.h
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatTypes.h"
#include "CombatActionComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionExecuted, EActionType, ActionType, bool, bSuccess);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AIDND_TOPDOWN_API UCombatActionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UCombatActionComponent();

    // Available actions
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actions")
    TArray<FActionData> AvailableActions;

    // Delegates
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnActionExecuted OnActionExecuted;

    // Action functions
    UFUNCTION(BlueprintCallable, Category = "Actions")
    bool ExecuteAction(EActionType ActionType, AActor* Target = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Actions")
    bool CanExecuteAction(EActionType ActionType) const;

    UFUNCTION(BlueprintCallable, Category = "Actions")
    TArray<AActor*> GetValidTargetsInRange(float Range);

    // Attack functions
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool PerformAttack(class ACombatCharacter* Target);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    int32 RollAttack() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    int32 RollDamage(int32 DiceCount, int32 DiceSize) const;

    // Movement
    UFUNCTION(BlueprintCallable, Category = "Movement")
    float GetRemainingMovement() const;

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void ConsumeMovement(float Distance);

    UFUNCTION(BlueprintCallable, Category = "Movement")
    void ResetMovement();

protected:
    virtual void BeginPlay() override;

private:
    UPROPERTY()
    class ACombatCharacter* OwnerCharacter;

    float RemainingMovementThisTurn;

    bool ExecuteMoveAction(const FVector& TargetLocation);
    bool ExecuteAttackAction(AActor* Target);
    bool ExecuteDashAction();
};