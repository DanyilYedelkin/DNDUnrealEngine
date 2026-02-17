// CombatCharacter.cpp
#include "CombatCharacter.h"
#include "CharacterStatsComponent.h"
#include "CombatActionComponent.h"
#include "Components/DecalComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Navigation/PathFollowingComponent.h"
#include "AIController.h"
#include "NavigationSystem.h"      
#include "NavigationPath.h"

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

    // Configure character movement
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
    GetCharacterMovement()->bUseControllerDesiredRotation = false;
}

void ACombatCharacter::BeginPlay()
{
    Super::BeginPlay();
    SetupSelectionDecal();
    LastTickLocation = GetActorLocation();
}

void ACombatCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // Track movement distance in combat
    if (bIsInCombat && bIsMyTurn)
    {
        TrackMovement(DeltaTime);
    }
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

void ACombatCharacter::TrackMovement(float DeltaTime)
{
    FVector CurrentLocation = GetActorLocation();
    float DistanceThisTick = FVector::Dist2D(CurrentLocation, LastTickLocation);
    
    if (DistanceThisTick > 1.0f) // Ignore tiny movements (jitter)
    {
        TotalDistanceMoved += DistanceThisTick;
        RemainingMovementDistance = FMath::Max(0.0f, GetMaxMovementDistance() - TotalDistanceMoved);
        
        // Check if movement limit reached
        if (RemainingMovementDistance <= 0.0f && bIsMoving)
        {
            StopMovement();
            bHasMoved = true;
            
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow,
                    TEXT("Movement limit reached!"));
            }
        }
    }
    
    LastTickLocation = CurrentLocation;
}

void ACombatCharacter::StartTurn()
{
    bIsMyTurn = true;
    ResetTurnActions();
    
    // Reset movement tracking
    TotalDistanceMoved = 0.0f;
    RemainingMovementDistance = GetMaxMovementDistance();
    LastTickLocation = GetActorLocation();
    
    OnTurnStarted.Broadcast(this);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, 
            FString::Printf(TEXT("%s turn started - Movement: %.0f units"), 
                *GetName(), RemainingMovementDistance));
    }
}

void ACombatCharacter::EndTurn()
{
    bIsMyTurn = false;
    StopMovement();
    
    OnTurnEnded.Broadcast(this);

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, 
            FString::Printf(TEXT("%s turn ended"), *GetName()));
    }
}

void ACombatCharacter::EnterCombat()
{
    bIsInCombat = true;
    
    // DON'T disable movement - we need it for animations!
    // Just make sure movement is controlled through turns
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
            FString::Printf(TEXT("%s entered combat"), *GetName()));
    }
}

void ACombatCharacter::ExitCombat()
{
    bIsInCombat = false;
    bIsMyTurn = false;
    SetSelected(false);
    StopMovement();
    
    // Reset movement tracking
    TotalDistanceMoved = 0.0f;
    RemainingMovementDistance = 0.0f;
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

bool ACombatCharacter::MoveToLocation(const FVector& TargetLocation)
{
    if (bIsInCombat && !bIsMyTurn)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                TEXT("Can't move - not your turn!"));
        }
        return false;
    }

    float DistanceToTarget = FVector::Dist2D(GetActorLocation(), TargetLocation);
    
    if (bIsInCombat && !CanMoveDistance(DistanceToTarget))
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red,
                FString::Printf(TEXT("Not enough movement! Need: %.0f, Have: %.0f"), 
                    DistanceToTarget, RemainingMovementDistance));
        }
        return false;
    }

    AController* CharController = GetController();  // ← ИЗМЕНЕНО!
    if (CharController)
    {
        bIsMoving = true;
        
        UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
        if (NavSys)
        {
            FAIMoveRequest MoveRequest(TargetLocation);
            MoveRequest.SetAcceptanceRadius(50.0f);
            
            if (AAIController* AI = Cast<AAIController>(CharController))  // ← ИЗМЕНЕНО!
            {
                AI->MoveTo(MoveRequest);
                return true;
            }
        }
    }

    return false;
}

bool ACombatCharacter::MoveToActor(AActor* TargetActor, float AcceptanceRadius)
{
    if (!TargetActor)
    {
        return false;
    }

    if (bIsInCombat && !bIsMyTurn)
    {
        return false;
    }

    AController* CharController = GetController();  // ← ИЗМЕНЕНО!
    if (CharController)
    {
        bIsMoving = true;
        
        if (AAIController* AI = Cast<AAIController>(CharController))  // ← ИЗМЕНЕНО!
        {
            FAIMoveRequest MoveRequest(TargetActor);
            MoveRequest.SetAcceptanceRadius(AcceptanceRadius);
            AI->MoveTo(MoveRequest);
            return true;
        }
    }

    return false;
}

void ACombatCharacter::StopMovement()
{
    bIsMoving = false;
    
    AController* CharController = GetController();  // ← ИЗМЕНЕНО!
    if (CharController)
    {
        if (AAIController* AI = Cast<AAIController>(CharController))  // ← ИЗМЕНЕНО!
        {
            AI->StopMovement();
        }
    }
    
    OnMovementComplete.Broadcast(this);
}

float ACombatCharacter::GetMaxMovementDistance() const
{
    if (StatsComponent)
    {
        // Convert movement speed (meters) to centimeters
        // D&D movement speed is in meters, UE uses centimeters
        return StatsComponent->Stats.MovementSpeed * 100.0f;
    }
    return 600.0f; // Default: 6 meters = 600 cm
}

bool ACombatCharacter::CanMoveDistance(float Distance) const
{
    if (!bIsInCombat)
    {
        return true; // No limit outside combat
    }
    
    return RemainingMovementDistance >= Distance;
}

void ACombatCharacter::SetGridPosition(FIntPoint NewPosition)
{
    GridPosition = NewPosition;
}

FIntPoint ACombatCharacter::GetGridPosition() const
{
    return GridPosition;
}