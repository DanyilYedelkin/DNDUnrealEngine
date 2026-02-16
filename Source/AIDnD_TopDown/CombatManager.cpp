// CombatManager.cpp
#include "CombatManager.h"
#include "CombatCharacter.h"
#include "CharacterStatsComponent.h"
#include "CombatActionComponent.h"
#include "GridManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ACombatManager::ACombatManager()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACombatManager::BeginPlay()
{
    Super::BeginPlay();
    
    // Find GridManager if not set
    if (!GridManager)
    {
        TArray<AActor*> FoundActors;
        UGameplayStatics::GetAllActorsOfClass(GetWorld(), AGridManager::StaticClass(), FoundActors);
        
        if (FoundActors.Num() > 0)
        {
            GridManager = Cast<AGridManager>(FoundActors[0]);
        }
    }
}

void ACombatManager::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // Track turn time
    if (bIsInCombat && CurrentTurnCharacter)
    {
        CurrentTurnTime += DeltaTime;

        // Auto-end turn on timeout (optional)
        if (bAutoEndTurnOnTimeout && CurrentTurnTime >= TurnTimeLimit)
        {
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, 
                    TEXT("Turn timeout! Auto-ending turn..."));
            }
            EndCurrentTurn();
        }
    }
}

void ACombatManager::StartCombat()
{
    if (bIsInCombat)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatManager: Combat already in progress!"));
        return;
    }

    // Find all combat characters in the level
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), ACombatCharacter::StaticClass(), FoundActors);

    CombatParticipants.Empty();
    
    for (AActor* Actor : FoundActors)
    {
        ACombatCharacter* Character = Cast<ACombatCharacter>(Actor);
        if (Character && Character->StatsComponent && Character->StatsComponent->IsAlive())
        {
            CombatParticipants.Add(Character);
            Character->EnterCombat();
        }
    }

    if (CombatParticipants.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatManager: No valid characters found for combat!"));
        return;
    }

    // Roll initiative and sort turn order
    RollInitiative();
    SortTurnOrder();

    // Start combat
    bIsInCombat = true;
    CurrentRound = 1;
    CurrentTurnIndex = 0;
    CurrentCombatState = ECombatState::PlayerTurn;

    OnCombatStarted.Broadcast();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Green, 
            TEXT("COMBAT STARTED!"));
    }

    // Start first turn
    StartNewRound();
}

void ACombatManager::EndCombat()
{
    if (!bIsInCombat)
    {
        return;
    }

    // End turn for current character
    if (CurrentTurnCharacter)
    {
        CurrentTurnCharacter->EndTurn();
    }

    // Exit combat for all participants
    for (ACombatCharacter* Character : CombatParticipants)
    {
        if (Character)
        {
            Character->ExitCombat();
        }
    }

    // Reset state
    bIsInCombat = false;
    CurrentRound = 0;
    CurrentTurnIndex = 0;
    CurrentTurnCharacter = nullptr;
    CurrentCombatState = ECombatState::NotInCombat;
    TurnOrder.Empty();

    OnCombatEnded.Broadcast();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red, 
            TEXT("COMBAT ENDED!"));
    }
}

void ACombatManager::AddCombatant(ACombatCharacter* Character)
{
    if (!Character || CombatParticipants.Contains(Character))
    {
        return;
    }

    CombatParticipants.Add(Character);
    Character->EnterCombat();

    if (bIsInCombat)
    {
        // Roll initiative for new character
        if (Character->StatsComponent)
        {
            int32 Initiative = Character->StatsComponent->RollInitiative();
            
            FCombatTurn NewTurn;
            NewTurn.Character = Character;
            NewTurn.Initiative = Initiative;
            NewTurn.bHasTakenTurn = false;

            TurnOrder.Add(NewTurn);
            SortTurnOrder();
        }
    }
}

void ACombatManager::RemoveCombatant(ACombatCharacter* Character)
{
    if (!Character)
    {
        return;
    }

    CombatParticipants.Remove(Character);
    
    // Remove from turn order
    TurnOrder.RemoveAll([Character](const FCombatTurn& Turn)
    {
        return Turn.Character == Character;
    });

    Character->ExitCombat();
}

void ACombatManager::NextTurn()
{
    if (!bIsInCombat || TurnOrder.Num() == 0)
    {
        return;
    }

    // End current turn
    if (CurrentTurnCharacter)
    {
        CurrentTurnCharacter->EndTurn();
    }

    // Move to next turn
    CurrentTurnIndex++;

    // Check if round ended
    if (CurrentTurnIndex >= TurnOrder.Num())
    {
        CurrentTurnIndex = 0;
        CurrentRound++;
        OnRoundChanged.Broadcast(CurrentRound);
        StartNewRound();
        return;
    }

    // Start next character's turn
    FCombatTurn& CurrentTurn = TurnOrder[CurrentTurnIndex];
    CurrentTurnCharacter = CurrentTurn.Character;

    if (CurrentTurnCharacter && CurrentTurnCharacter->StatsComponent && 
        CurrentTurnCharacter->StatsComponent->IsAlive())
    {
        CurrentTurnCharacter->StartTurn();
        CurrentTurnTime = 0.0f;

        // Update combat state
        if (CurrentTurnCharacter->IsPlayerControlledCharacter())
        {
            CurrentCombatState = ECombatState::PlayerTurn;
        }
        else
        {
            CurrentCombatState = ECombatState::EnemyTurn;
            
            // Process AI turn after a short delay
            FTimerHandle TimerHandle;
            GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, 
                &ACombatManager::ProcessAITurn, 1.0f, false);
        }

        OnTurnChanged.Broadcast(CurrentTurnCharacter);

        if (GEngine)
        {
            FString CharacterName = CurrentTurnCharacter->GetName();
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan,
                FString::Printf(TEXT("Turn: %s (Initiative: %d)"), 
                    *CharacterName, CurrentTurn.Initiative));
        }
    }
    else
    {
        // Character is dead, skip to next turn
        NextTurn();
    }
}

void ACombatManager::EndCurrentTurn()
{
    if (!bIsInCombat || !CurrentTurnCharacter)
    {
        return;
    }

    // Check combat end conditions before moving to next turn
    CheckCombatEndConditions();

    if (bIsInCombat)
    {
        NextTurn();
    }
}

void ACombatManager::RollInitiative()
{
    TurnOrder.Empty();

    for (ACombatCharacter* Character : CombatParticipants)
    {
        if (Character && Character->StatsComponent)
        {
            int32 Initiative = Character->StatsComponent->RollInitiative();
            
            FCombatTurn Turn;
            Turn.Character = Character;
            Turn.Initiative = Initiative;
            Turn.bHasTakenTurn = false;

            TurnOrder.Add(Turn);

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::White,
                    FString::Printf(TEXT("%s rolled initiative: %d"), 
                        *Character->GetName(), Initiative));
            }
        }
    }
}

void ACombatManager::SortTurnOrder()
{
    // Sort by initiative (highest first)
    TurnOrder.Sort([](const FCombatTurn& A, const FCombatTurn& B)
    {
        return A.Initiative > B.Initiative;
    });
}

ACombatCharacter* ACombatManager::GetCurrentTurnCharacter() const
{
    return CurrentTurnCharacter;
}

TArray<ACombatCharacter*> ACombatManager::GetPlayerCharacters() const
{
    TArray<ACombatCharacter*> Players;

    for (ACombatCharacter* Character : CombatParticipants)
    {
        if (Character && Character->IsPlayerControlledCharacter())
        {
            Players.Add(Character);
        }
    }

    return Players;
}

TArray<ACombatCharacter*> ACombatManager::GetEnemyCharacters() const
{
    TArray<ACombatCharacter*> Enemies;

    for (ACombatCharacter* Character : CombatParticipants)
    {
        if (Character && !Character->IsPlayerControlledCharacter())
        {
            Enemies.Add(Character);
        }
    }

    return Enemies;
}

bool ACombatManager::AreAllEnemiesDead() const
{
    TArray<ACombatCharacter*> Enemies = GetEnemyCharacters();

    for (ACombatCharacter* Enemy : Enemies)
    {
        if (Enemy && Enemy->StatsComponent && Enemy->StatsComponent->IsAlive())
        {
            return false;
        }
    }

    return Enemies.Num() > 0; // Return true only if there were enemies and they're all dead
}

bool ACombatManager::AreAllPlayersDead() const
{
    TArray<ACombatCharacter*> Players = GetPlayerCharacters();

    for (ACombatCharacter* Player : Players)
    {
        if (Player && Player->StatsComponent && Player->StatsComponent->IsAlive())
        {
            return false;
        }
    }

    return Players.Num() > 0; // Return true only if there were players and they're all dead
}

void ACombatManager::CheckCombatEndConditions()
{
    if (!bIsInCombat)
    {
        return;
    }

    // Clean up dead characters first
    CleanupDeadCharacters();

    // Check win/loss conditions
    if (AreAllEnemiesDead())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, 
                TEXT("VICTORY! All enemies defeated!"));
        }
        EndCombat();
    }
    else if (AreAllPlayersDead())
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, 
                TEXT("DEFEAT! All players died!"));
        }
        EndCombat();
    }
}

void ACombatManager::StartNewRound()
{
    // Reset turn flags
    for (FCombatTurn& Turn : TurnOrder)
    {
        Turn.bHasTakenTurn = false;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Magenta,
            FString::Printf(TEXT("=== ROUND %d ==="), CurrentRound));
    }

    // Start first turn of the round
    NextTurn();
}

void ACombatManager::ProcessAITurn()
{
    if (!CurrentTurnCharacter || CurrentTurnCharacter->IsPlayerControlledCharacter())
    {
        return;
    }

    // Simple AI: attack nearest player character
    TArray<ACombatCharacter*> Players = GetPlayerCharacters();
    
    if (Players.Num() == 0)
    {
        EndCurrentTurn();
        return;
    }

    // Find nearest player
    ACombatCharacter* NearestPlayer = nullptr;
    float NearestDistance = FLT_MAX;

    for (ACombatCharacter* Player : Players)
    {
        if (Player && Player->StatsComponent && Player->StatsComponent->IsAlive())
        {
            float Distance = FVector::Dist(
                CurrentTurnCharacter->GetActorLocation(), 
                Player->GetActorLocation()
            );

            if (Distance < NearestDistance)
            {
                NearestDistance = Distance;
                NearestPlayer = Player;
            }
        }
    }

    // Attack nearest player
    if (NearestPlayer && CurrentTurnCharacter->ActionComponent)
    {
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange,
                FString::Printf(TEXT("%s attacks %s"), 
                    *CurrentTurnCharacter->GetName(), 
                    *NearestPlayer->GetName()));
        }

        CurrentTurnCharacter->ActionComponent->ExecuteAction(EActionType::Attack, NearestPlayer);
    }

    // End turn after AI action
    FTimerHandle TimerHandle;
    GetWorld()->GetTimerManager().SetTimer(TimerHandle, this, 
        &ACombatManager::EndCurrentTurn, 1.5f, false);
}

void ACombatManager::OnCharacterDied(ACombatCharacter* Character)
{
    if (!Character)
    {
        return;
    }

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 3.0f, FColor::Red,
            FString::Printf(TEXT("%s has died!"), *Character->GetName()));
    }

    // Check if combat should end
    CheckCombatEndConditions();
}

void ACombatManager::CleanupDeadCharacters()
{
    // Remove dead characters from turn order
    TurnOrder.RemoveAll([](const FCombatTurn& Turn)
    {
        return !Turn.Character || 
               !Turn.Character->StatsComponent || 
               !Turn.Character->StatsComponent->IsAlive();
    });

    // Remove from participants
    CombatParticipants.RemoveAll([](ACombatCharacter* Character)
    {
        return !Character || 
               !Character->StatsComponent || 
               !Character->StatsComponent->IsAlive();
    });
}