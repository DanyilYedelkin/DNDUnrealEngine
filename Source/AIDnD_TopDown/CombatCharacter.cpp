// CombatCharacter.cpp
#include "CombatCharacter.h"
#include "CharacterStatsComponent.h"
#include "CombatActionComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"  // ← ЭТО ВАЖНО!

ACombatCharacter::ACombatCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create stats component
    StatsComponent = CreateDefaultSubobject<UCharacterStatsComponent>(TEXT("StatsComponent"));

    // Create action component
    ActionComponent = CreateDefaultSubobject<UCombatActionComponent>(TEXT("ActionComponent"));

    // Create selection decal
    SelectionDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("SelectionDecal"));
    SelectionDecal->SetupAttachment(RootComponent);
    SelectionDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    SelectionDecal->DecalSize = FVector(64.0f, 64.0f, 64.0f);
    SelectionDecal->SetVisibility(false);
}

void ACombatCharacter::BeginPlay()
{
    Super::BeginPlay();
    SetupSelectionDecal();
}

void ACombatCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void ACombatCharacter::SetupSelectionDecal()
{
    // Position decal slightly below character
    if (SelectionDecal && GetCapsuleComponent())
    {
        float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
        SelectionDecal->SetRelativeLocation(FVector(0.0f, 0.0f, -CapsuleHalfHeight - 5.0f));
    }
}

void ACombatCharacter::StartTurn()
{
    bIsMyTurn = true;
    ResetTurnActions();
    
    OnTurnStarted.Broadcast(this);

    // Log for debugging
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, 
            FString::Printf(TEXT("%s turn started"), *GetName()));
    }
}

void ACombatCharacter::EndTurn()
{
    bIsMyTurn = false;
    
    OnTurnEnded.Broadcast(this);

    // Log for debugging
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, 
            FString::Printf(TEXT("%s turn ended"), *GetName()));
    }
}

void ACombatCharacter::EnterCombat()
{
    bIsInCombat = true;
    
    // Disable regular movement (we'll use grid movement)
    GetCharacterMovement()->DisableMovement();
}

void ACombatCharacter::ExitCombat()
{
    bIsInCombat = false;
    bIsMyTurn = false;
    SetSelected(false);
    
    // Re-enable regular movement
    GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

bool ACombatCharacter::CanTakeAction() const
{
    return bIsMyTurn && !bHasUsedAction && StatsComponent && StatsComponent->IsAlive();
}

void ACombatCharacter::ResetTurnActions()
{
    bHasUsedAction = false;
    bHasUsedBonusAction = false;
    bHasMoved = false;
}

void ACombatCharacter::SetSelected(bool bSelected)
{
    if (SelectionDecal)
    {
        SelectionDecal->SetVisibility(bSelected);
    }
}

bool ACombatCharacter::IsPlayerControlledCharacter() const 
{
    return StatsComponent && StatsComponent->bIsPlayerControlled;
}

void ACombatCharacter::SetGridPosition(FIntPoint NewPosition)
{
    GridPosition = NewPosition;
}

FIntPoint ACombatCharacter::GetGridPosition() const
{
    return GridPosition;
}