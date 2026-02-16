// CombatManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CombatTypes.h"
#include "CombatManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatEnded);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTurnChanged, class ACombatCharacter*, CurrentCharacter);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoundChanged, int32, RoundNumber);

UCLASS()
class AIDND_TOPDOWN_API ACombatManager : public AActor
{
    GENERATED_BODY()
    
public:    
    ACombatManager();

    // Combat state
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    ECombatState CurrentCombatState;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    bool bIsInCombat = false;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    int32 CurrentRound = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    int32 CurrentTurnIndex = 0;

    // Combat participants
    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    TArray<ACombatCharacter*> CombatParticipants;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    TArray<FCombatTurn> TurnOrder;

    UPROPERTY(BlueprintReadOnly, Category = "Combat")
    ACombatCharacter* CurrentTurnCharacter;

    // References
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    class AGridManager* GridManager;

    // Combat settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    float TurnTimeLimit = 60.0f; // seconds

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    bool bAutoEndTurnOnTimeout = false;

    // Delegates
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCombatStarted OnCombatStarted;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCombatEnded OnCombatEnded;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnTurnChanged OnTurnChanged;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnRoundChanged OnRoundChanged;

    // Combat control functions
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void StartCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EndCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void AddCombatant(ACombatCharacter* Character);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RemoveCombatant(ACombatCharacter* Character);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void NextTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void EndCurrentTurn();

    // Initiative functions
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RollInitiative();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SortTurnOrder();

    // Query functions
    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatCharacter* GetCurrentTurnCharacter() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    TArray<ACombatCharacter*> GetPlayerCharacters() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    TArray<ACombatCharacter*> GetEnemyCharacters() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool AreAllEnemiesDead() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool AreAllPlayersDead() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void CheckCombatEndConditions();

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    float CurrentTurnTime = 0.0f;
    
    void StartNewRound();
    void ProcessAITurn();
    void OnCharacterDied(ACombatCharacter* Character);
    void CleanupDeadCharacters();
};