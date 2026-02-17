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
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"

ACombatPlayerController::ACombatPlayerController()
{
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    bEnableMouseOverEvents = true;
}

void ACombatPlayerController::BeginPlay()
{
    Super::BeginPlay();
}

void ACombatPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);
    
    if (EnhancedInput)
    {
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
    HandleCombatClick();
}

void ACombatPlayerController::OnEndTurnTriggered(const FInputActionValue& Value)
{
    RequestEndTurn();
}

void ACombatPlayerController::OnAttackTriggered(const FInputActionValue& Value)
{
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

    // Deselect previous
    if (SelectedCharacter)
    {
        SelectedCharacter->SetSelected(false);
        SelectedCharacter->OnMovementComplete.RemoveDynamic(this, &ACombatPlayerController::OnCharacterMovementComplete);
    }

    // Select new
    SelectedCharacter = CombatChar;  
    SelectedCharacter->SetSelected(true);
    
    // Subscribe to movement events
    SelectedCharacter->OnMovementComplete.AddDynamic(this, &ACombatPlayerController::OnCharacterMovementComplete);

    if (GEngine)
    {
        FString MovementInfo = "";
        if (bIsInCombatMode && SelectedCharacter->bIsMyTurn)
        {
            MovementInfo = FString::Printf(TEXT(" | Movement: %.0f"), 
                SelectedCharacter->RemainingMovementDistance);
        }
        
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            FString::Printf(TEXT("Selected: %s%s"), *CombatChar->GetName(), *MovementInfo));
    }
}

void ACombatPlayerController::DeselectCharacter()
{
    if (SelectedCharacter)
    {
        SelectedCharacter->SetSelected(false);
        SelectedCharacter->OnMovementComplete.RemoveDynamic(this, &ACombatPlayerController::OnCharacterMovementComplete);
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

    if (!bIsInCombatMode)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                TEXT("Not in combat mode!"));
        }
        return;
    }

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

void ACombatPlayerController::RequestMovement(const FVector& TargetLocation)
{
    if (!SelectedCharacter)
    {
        return;
    }

    if (!bIsInCombatMode)
    {
        // Outside combat - free movement
        UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, TargetLocation);
        return;
    }

    // In combat - limited movement
    if (!SelectedCharacter->bIsMyTurn)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                TEXT("Not your turn!"));
        }
        return;
    }

    bool bSuccess = SelectedCharacter->MoveToLocation(TargetLocation);
    
    if (bSuccess)
    {
        bIsWaitingForMovement = true;
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
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan,
            TEXT("HandleCombatClick called"));
    }

    ACombatCharacter* ClickedCharacter = GetCharacterUnderCursor();

    if (ClickedCharacter)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Green,
                FString::Printf(TEXT("Clicked character: %s"), *ClickedCharacter->GetName()));
        }

        // Check if friendly or enemy
        if (ClickedCharacter->IsPlayerControlledCharacter())
        {
            HandleCharacterSelection(ClickedCharacter);
        }
        else
        {
            HandleEnemyClick(ClickedCharacter);
        }
    }
    else
    {
        // Clicked on ground
        FVector ClickLocation;
        if (GetWorldLocationUnderCursor(ClickLocation))
        {
            HandleGroundClick(ClickLocation);
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

    // Always allow selection of player characters
    SelectCharacter(CombatChar);
    
    // Auto-start turn for debugging (remove later when CombatManager handles it)
    if (bIsInCombatMode && !CombatChar->bIsMyTurn)
    {
        CombatChar->StartTurn();
    }
}

void ACombatPlayerController::HandleEnemyClick(ACombatCharacter* Enemy)
{
    if (!Enemy)
    {
        return;
    }

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
                    TEXT("Select your character first!"));
            }
        }
    }
    else
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                TEXT("Press Tab to enter combat mode"));
        }
    }
}

void ACombatPlayerController::HandleGroundClick(const FVector& Location)
{
    if (bIsInCombatMode && SelectedCharacter)
    {
        // In combat - try to move
        RequestMovement(Location);
    }
    else if (!bIsInCombatMode)
    {
        // Outside combat - free movement
        APawn* ControlledPawn = GetPawn();
        if (ControlledPawn)
        {
            UAIBlueprintHelperLibrary::SimpleMoveToLocation(this, Location);
        }
    }
}

void ACombatPlayerController::OnCharacterMovementComplete(ACombatCharacter* CombatChar)
{
    bIsWaitingForMovement = false;
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green,
            FString::Printf(TEXT("%s movement complete | Remaining: %.0f"), 
                *CombatChar->GetName(), CombatChar->RemainingMovementDistance));
    }
}