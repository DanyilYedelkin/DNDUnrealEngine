// CombatPlayerController.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "CombatPlayerController.generated.h"

// Forward declarations
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

UCLASS()
class AIDND_TOPDOWN_API ACombatPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACombatPlayerController();

    // ============================================
    // COMBAT SYSTEM
    // ============================================
    
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    class ACombatCharacter* SelectedCharacter;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bIsInCombatMode = false;

    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bIsWaitingForMovement = false;

    // ============================================
    // ENHANCED INPUT
    // ============================================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_CombatSelect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_EndTurn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_Attack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_ToggleCombat;

    // ============================================
    // COMBAT FUNCTIONS
    // ============================================

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void SelectCharacter(ACombatCharacter* CombatChar);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void DeselectCharacter();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatCharacter* GetSelectedCharacter() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RequestAttack(ACombatCharacter* Target);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RequestMovement(const FVector& TargetLocation);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void RequestEndTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ToggleCombatMode();

    // ============================================
    // HELPER FUNCTIONS
    // ============================================

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HandleCombatClick();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatCharacter* GetCharacterUnderCursor() const;

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool GetWorldLocationUnderCursor(FVector& OutLocation) const;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    void OnCombatSelectTriggered(const FInputActionValue& Value);
    void OnEndTurnTriggered(const FInputActionValue& Value);
    void OnAttackTriggered(const FInputActionValue& Value);
    void OnToggleCombatTriggered(const FInputActionValue& Value);

    void HandleCharacterSelection(ACombatCharacter* CombatChar);
    void HandleEnemyClick(ACombatCharacter* Enemy);
    void HandleGroundClick(const FVector& Location);
    
    UFUNCTION()
    void OnCharacterMovementComplete(ACombatCharacter* CombatChar);
};