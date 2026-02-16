// CombatCharacter.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "CombatTypes.h"
#include "CombatCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnStarted, ACombatCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnEnded, ACombatCharacter*, Character);

UCLASS()
class AIDND_TOPDOWN_API ACombatCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    ACombatCharacter();

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    class UCharacterStatsComponent* StatsComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    class UCombatActionComponent* ActionComponent;

    // Visual feedback
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    class UDecalComponent* SelectionDecal;

    // Current state
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsInCombat = false;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsMyTurn = false;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bHasUsedAction = false;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bHasUsedBonusAction = false;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bHasMoved = false;

    // Grid position
    UPROPERTY(BlueprintReadWrite, Category = "Grid")
    FIntPoint GridPosition;

    // Delegates
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTurnStarted OnTurnStarted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTurnEnded OnTurnEnded;

    // Combat functions
    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void StartTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    virtual void EndTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EnterCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ExitCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool CanTakeAction() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetTurnActions();

    // Selection
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SetSelected(bool bSelected);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool IsPlayerControlledCharacter() const;

    // Grid functions
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SetGridPosition(FIntPoint NewPosition);

    UFUNCTION(BlueprintCallable, Category = "Grid")
    FIntPoint GetGridPosition() const;

protected:
    virtual void BeginPlay() override;

public:
    virtual void Tick(float DeltaTime) override;

private:
    void SetupSelectionDecal();
};