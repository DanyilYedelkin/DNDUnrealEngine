// CombatPlayerController.cpp
#include "CombatPlayerController.h"
#include "CombatCharacter.h"
#include "CombatActionComponent.h"
#include "CharacterStatsComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ACombatPlayerController::ACombatPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void ACombatPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Enhanced Input setup is handled in Blueprint
    // You can also add mapping context here if needed
}

void ACombatPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    // Cast to Enhanced Input Component
    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    
    if (EnhancedInput)
    {
        // Bind combat actions only if they are assigned
        if (IA_CombatSelect)
        {
            EnhancedInput->BindAction(IA_CombatSelect, ETriggerEvent::Triggered, 
                this, &ACombatPlayerController::OnCombatSelectTriggered);
        }

        if (IA_EndTurn)
        {
            EnhancedInput->BindAction(IA_EndTurn, ETriggerEvent::Triggered, 
                this, &ACombatPlayerController::OnEndTurnTriggered);
        }

        if (IA_Attack)
        {
            EnhancedInput->BindAction(IA_Attack, ETriggerEvent::Triggered, 
                this, &ACombatPlayerController::OnAttackTriggered);
        }

        if (IA_ToggleCombat)
        {
            EnhancedInput->BindAction(IA_ToggleCombat, ETriggerEvent::Triggered, 
                this, &ACombatPlayerController::OnToggleCombatTriggered);
        }
    }
}

// ============================================
// INPUT CALLBACKS
// ============================================

void ACombatPlayerController::OnCombatSelectTriggered(const FInputActionValue& Value)
{
    // This will be called when IA_CombatSelect is triggered
    HandleCombatClick();
}

void ACombatPlayerController::OnEndTurnTriggered(const FInputActionValue& Value)
{
    RequestEndTurn();
}

void ACombatPlayerController::OnAttackTriggered(const FInputActionValue& Value)
{
    // Quick attack with selected character
    if (SelectedCharacter)
    {
        ACombatCharacter* Target = GetCharacterUnderCursor();
        if (Target)
        {
            RequestAttack(Target);
        }
    }
}

void ACombatPlayerController::OnToggleCombatTriggered(const FInputActionValue& Value)
{
    ToggleCombatMode();
}

// ============================================
// COMBAT FUNCTIONS
// ============================================

void ACombatPlayerController::SelectCharacter(ACombatCharacter* CombatChar)  
{
    if (!CombatChar)  
    {
        return;
    }

    // Deselect previous character
    if (SelectedCharacter)
    {
        SelectedCharacter->SetSelected(false);
    }

    // Select new character
    SelectedCharacter = CombatChar;  
    SelectedCharacter->SetSelected(true);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            FString::Printf(TEXT("Selected: %s"), *CombatChar->GetName()));  
    }
}

void ACombatPlayerController::DeselectCharacter()
{
    if (SelectedCharacter)
    {
        SelectedCharacter->SetSelected(false);
        SelectedCharacter = nullptr;
    }
}

ACombatCharacter* ACombatPlayerController::GetSelectedCharacter() const
{
    return SelectedCharacter;
}

void ACombatPlayerController::RequestAttack(ACombatCharacter* Target)
{
    if (!SelectedCharacter || !Target)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                TEXT("No character selected or invalid target!"));
        }
        return;
    }

    // Check if in combat mode
    if (!bIsInCombatMode)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                TEXT("Not in combat mode!"));
        }
        return;
    }

    // Check if can take action
    if (!SelectedCharacter->CanTakeAction())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                TEXT("Cannot attack - not your turn or action used!"));
        }
        return;
    }

    // Execute attack
    if (SelectedCharacter->ActionComponent)
    {
        SelectedCharacter->ActionComponent->ExecuteAction(EActionType::Attack, Target);
    }
}

void ACombatPlayerController::RequestEndTurn()
{
    if (!SelectedCharacter)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                TEXT("No character selected!"));
        }
        return;
    }

    if (!bIsInCombatMode)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                TEXT("Not in combat mode!"));
        }
        return;
    }

    if (SelectedCharacter->ActionComponent)
    {
        SelectedCharacter->ActionComponent->ExecuteAction(EActionType::EndTurn);
    }
}

void ACombatPlayerController::ToggleCombatMode()
{
    bIsInCombatMode = !bIsInCombatMode;

    if (GEngine)
    {
        FString ModeText = bIsInCombatMode ? TEXT("ENTERED") : TEXT("EXITED");
        FColor ModeColor = bIsInCombatMode ? FColor::Green : FColor::Yellow;
        
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, ModeColor,
            FString::Printf(TEXT("Combat Mode: %s"), *ModeText));
    }
}

// ============================================
// HELPER FUNCTIONS
// ============================================

void ACombatPlayerController::HandleCombatClick()
{
    ACombatCharacter* ClickedCharacter = GetCharacterUnderCursor();

    if (ClickedCharacter)
    {
        // Check if it's a friendly or enemy character
        if (ClickedCharacter->IsPlayerControlled())
        {
            // Select friendly character
            HandleCharacterSelection(ClickedCharacter);
        }
        else
        {
            // Attack enemy character (if we have someone selected)
            HandleEnemyClick(ClickedCharacter);
        }
    }
    else
    {
        // Clicked on ground - could be for movement
        // Will implement with grid system later
        if (GEngine && bIsInCombatMode)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan,
                TEXT("Ground click - movement will be implemented with grid system"));
        }
    }
}

ACombatCharacter* ACombatPlayerController::GetCharacterUnderCursor() const
{
    FHitResult HitResult;
    GetHitResultUnderCursor(ECC_Pawn, false, HitResult);

    if (HitResult.bBlockingHit)
    {
        return Cast<ACombatCharacter>(HitResult.GetActor());
    }

    return nullptr;
}

bool ACombatPlayerController::GetWorldLocationUnderCursor(FVector& OutLocation) const
{
    FHitResult HitResult;
    bool bHit = GetHitResultUnderCursor(ECC_Visibility, false, HitResult);

    if (bHit)
    {
        OutLocation = HitResult.Location;
    }

    return bHit;
}


void ACombatPlayerController::HandleCharacterSelection(ACombatCharacter* CombatChar)  
{
    if (!CombatChar) 
    {
        return;
    }

    // In combat mode, only select if it's our turn or if character is player controlled
    if (bIsInCombatMode)
    {
        if (CombatChar->bIsMyTurn || CombatChar->IsPlayerControlledCharacter())  
        {
            SelectCharacter(CombatChar); 
        }
        else
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                    TEXT("Not this character's turn!"));
            }
        }
    }
    else
    {
        // In exploration mode, freely select any player character
        SelectCharacter(CombatChar); 
    }
}

void ACombatPlayerController::HandleEnemyClick(ACombatCharacter* Enemy)
{
    if (!Enemy)
    {
        return;
    }

    // Only attack in combat mode
    if (bIsInCombatMode)
    {
        if (SelectedCharacter)
        {
            RequestAttack(Enemy);
        }
        else
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                    TEXT("Select a character first!"));
            }
        }
    }
    else
    {
        // In exploration mode, clicking enemy could start combat
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                TEXT("Enemy detected - press Tab to enter combat mode"));
        }
    }
}