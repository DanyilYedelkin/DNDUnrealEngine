// Source/AIDnD_TopDown/Private/Combat/CombatPlayerController.cpp
#include "Combat/CombatPlayerController.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/TurnManager.h"
#include "Combat/CombatLog.h"
#include "Combat/Actions/CombatAction.h" 

ACombatPlayerController::ACombatPlayerController()
{
}

void ACombatPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Bind to TurnManager events
    ATurnManager* TM = GetTurnManager();
    if (TM)
    {
        TM->OnTurnStarted.AddDynamic(this,
            &ACombatPlayerController::HandleTurnStarted);
        TM->OnCombatEnded.AddDynamic(this,
            &ACombatPlayerController::HandleCombatEnded);
    }
}

// ============================================================
//  BLUEPRINT CALLABLE
// ============================================================

void ACombatPlayerController::SelectCharacter(ACombatCharacter* InCharacter)
{
    SelectedCharacter = InCharacter;
    UE_LOG(LogCombat, Log,
        TEXT("CombatPlayerController: Selected character: %s"),
        InCharacter ? *InCharacter->CharacterName.ToString() : TEXT("None"));
}

void ACombatPlayerController::SelectAction(TSubclassOf<UCombatAction> ActionClass)
{
    SelectedActionClass = ActionClass;

    UE_LOG(LogCombat, Log,
        TEXT("CombatPlayerController: Action selected: %s"),
        ActionClass ? *ActionClass->GetName() : TEXT("None"));
}

bool ACombatPlayerController::ConfirmMove(FVector TargetLocation, float CostInFeet)
{
    if (!SelectedCharacter)
    {
        UE_LOG(LogCombat, Warning,
            TEXT("CombatPlayerController: ConfirmMove — no character selected"));
        return false;
    }

    UCombatStatsComponent* Stats = SelectedCharacter->CombatStats;
    if (!Stats)
        return false;

    if (!Stats->SpendMovement(CostInFeet))
    {
        UE_LOG(LogCombat, Log,
            TEXT("CombatPlayerController: Not enough movement (%.0f feet needed, %.0f remaining)"),
            CostInFeet, Stats->MovementRemaining);
        return false;
    }

    ConfirmedMoveTarget = TargetLocation;

    UE_LOG(LogCombat, Log,
        TEXT("CombatPlayerController: %s moves to (%.0f, %.0f, %.0f) — %.0f feet spent"),
        *SelectedCharacter->CharacterName.ToString(),
        TargetLocation.X, TargetLocation.Y, TargetLocation.Z,
        CostInFeet);

    return true;
}

void ACombatPlayerController::EndTurn()
{
    if (!bInCombatMode)
    {
        UE_LOG(LogCombat, Warning,
            TEXT("CombatPlayerController: EndTurn called outside combat"));
        return;
    }

    ATurnManager* TM = GetTurnManager();
    if (TM)
        TM->EndCurrentTurn();
}

ACombatCharacter* ACombatPlayerController::GetCurrentTurnCharacter() const
{
    ATurnManager* TM = GetTurnManager();
    if (!TM) return nullptr;

    const FCombatantInitiative Current = TM->GetCurrentTurnCombatant();
    if (!Current.Combatant) return nullptr;

    return Cast<ACombatCharacter>(Current.Combatant.GetObject());
}

bool ACombatPlayerController::IsMyTurn() const
{
    ATurnManager* TM = GetTurnManager();
    return TM ? TM->IsPlayerTurn() : false;
}

void ACombatPlayerController::EnterCombatMode()
{
    if (bInCombatMode) return;

    bInCombatMode = true;

    UE_LOG(LogCombat, Log, TEXT("CombatPlayerController: Entering combat mode"));

    // Fire BP event — BP_PlayerController handles UI and input mode
    OnEnterCombatMode();
}

void ACombatPlayerController::ExitCombatMode()
{
    if (!bInCombatMode) return;

    bInCombatMode = false;
    SelectedCharacter = nullptr;
    SelectedActionClass = nullptr;

    UE_LOG(LogCombat, Log, TEXT("CombatPlayerController: Exiting combat mode"));

    OnExitCombatMode();
}

// ============================================================
//  PRIVATE
// ============================================================

ATurnManager* ACombatPlayerController::GetTurnManager() const
{
    if (IsValid(CachedTurnManager))
        return CachedTurnManager.Get();

    // Cache it on first access
    ACombatPlayerController* MutableThis =
        const_cast<ACombatPlayerController*>(this);
    MutableThis->CachedTurnManager =
        ATurnManager::GetTurnManager(this);

    return CachedTurnManager.Get();
}

void ACombatPlayerController::HandleTurnStarted(
    const FCombatantInitiative& CombatantEntry)
{
    if (!CombatantEntry.bIsPlayerControlled) return;

    // Auto-select character if it's a player turn
    ACombatCharacter* TurnChar =
        Cast<ACombatCharacter>(CombatantEntry.Combatant.GetObject());

    if (TurnChar)
    {
        SelectedCharacter = TurnChar;
        OnTurnStarted(TurnChar);
    }
}

void ACombatPlayerController::HandleCombatEnded()
{
    ExitCombatMode();
}