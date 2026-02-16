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
    
    // Current selected character
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    class ACombatCharacter* SelectedCharacter;

    // Combat state
    UPROPERTY(BlueprintReadWrite, Category = "Combat")
    bool bIsInCombatMode = false;

    // ============================================
    // ENHANCED INPUT - Input Actions (set in Blueprint)
    // ============================================
    
    // These can be set in your BP_PlayerController
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_CombatSelect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_EndTurn;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_Attack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Input|Combat")
    UInputAction* IA_ToggleCombat;

    // ============================================
    // COMBAT FUNCTIONS - Callable from Blueprint
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
    void RequestEndTurn();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ToggleCombatMode();

    // ============================================
    // HELPER FUNCTIONS
    // ============================================

    // Click handling - can be called from Blueprint
    UFUNCTION(BlueprintCallable, Category = "Combat")
    void HandleCombatClick();

    // Get character under cursor
    UFUNCTION(BlueprintCallable, Category = "Combat")
    ACombatCharacter* GetCharacterUnderCursor() const;

    // Get world location under cursor
    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool GetWorldLocationUnderCursor(FVector& OutLocation) const;

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

private:
    // Input callbacks - these bind to Enhanced Input Actions
    void OnCombatSelectTriggered(const FInputActionValue& Value);
    void OnEndTurnTriggered(const FInputActionValue& Value);
    void OnAttackTriggered(const FInputActionValue& Value);
    void OnToggleCombatTriggered(const FInputActionValue& Value);

    // Internal helpers
    void HandleCharacterSelection(ACombatCharacter* CombatChar);
    void HandleEnemyClick(ACombatCharacter* Enemy);
};