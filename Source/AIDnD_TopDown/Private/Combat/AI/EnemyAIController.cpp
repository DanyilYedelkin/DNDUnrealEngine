// Source/AIDnD_TopDown/Private/Combat/AI/EnemyAIController.cpp
#include "Combat/AI/EnemyAIController.h"
#include "Combat/AI/LocalAIDecisionMaker.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/ConditionComponent.h"
#include "Combat/TurnManager.h"
#include "Combat/MovementGridManager.h"
#include "Combat/Actions/MeleeAttackAction.h"
#include "Combat/Actions/UtilityActions.h"
#include "Combat/CombatLog.h"

#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "AITypes.h"

AEnemyAIController::AEnemyAIController()
{
    // Default to local heuristic AI
    DecisionMakerClass = ULocalAIDecisionMaker::StaticClass();
}

void AEnemyAIController::BeginPlay()
{
    Super::BeginPlay();

    // Instantiate decision maker
    if (DecisionMakerClass)
    {
        DecisionMaker = NewObject<UAIDecisionMaker>(
            this, DecisionMakerClass);

        DecisionMaker->OnDecisionReady.AddDynamic(
            this, &AEnemyAIController::OnDecisionReceived);
    }
    else
    {
        // Fallback: always create local AI
        DecisionMaker = NewObject<ULocalAIDecisionMaker>(this);
        DecisionMaker->OnDecisionReady.AddDynamic(
            this, &AEnemyAIController::OnDecisionReceived);

        UE_LOG(LogCombat, Warning,
            TEXT("EnemyAIController: No DecisionMakerClass set — using LocalAI"));
    }

    // Bind to TurnManager
    ATurnManager* TM = GetTurnManager();
    if (TM)
    {
        TM->OnTurnStarted.AddDynamic(this, &AEnemyAIController::OnTurnStartedHandler);
    }
}

void AEnemyAIController::OnTurnStartedHandler(const FCombatantInitiative& Entry)
{
    ACombatCharacter* TurnChar =
        Cast<ACombatCharacter>(Entry.Combatant.GetObject());
    if (TurnChar && TurnChar == ControlledCharacter)
        StartAITurn(TurnChar);
}

void AEnemyAIController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    ControlledCharacter = Cast<ACombatCharacter>(InPawn);

    if (ControlledCharacter)
    {
        UE_LOG(LogCombat, Log,
            TEXT("EnemyAIController: Possessing %s"),
            *ControlledCharacter->CharacterName.ToString());
    }
}

// ============================================================
//  PUBLIC API
// ============================================================

void AEnemyAIController::StartAITurn(ACombatCharacter* InCharacter)
{
    if (!InCharacter || !InCharacter->CombatStats) return;
    if (!InCharacter->CombatStats->IsAlive()) return;

    ControlledCharacter = InCharacter;
    ActionsThisTurn     = 0;
    bTurnActive         = true;

    UE_LOG(LogCombat, Log,
        TEXT("EnemyAIController: %s begins AI turn"),
        *InCharacter->CharacterName.ToString());

    BP_OnAIThinking();

    // Small initial delay before first action
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            ActionDelayHandle,
            this,
            &AEnemyAIController::RequestNextDecision,
            ActionDelay,
            false);
    }
    else
    {
        RequestNextDecision();
    }
}

void AEnemyAIController::ForceEndTurn()
{
    if (!bTurnActive) return;

    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(ActionDelayHandle);

    EndAITurn();
}

void AEnemyAIController::SetDecisionMakerClass(
    TSubclassOf<UAIDecisionMaker> NewClass)
{
    if (!NewClass) return;

    // Unbind old
    if (DecisionMaker)
        DecisionMaker->OnDecisionReady.RemoveAll(this);

    DecisionMakerClass = NewClass;
    DecisionMaker = NewObject<UAIDecisionMaker>(this, NewClass);
    DecisionMaker->OnDecisionReady.AddDynamic(
        this, &AEnemyAIController::OnDecisionReceived);

    UE_LOG(LogCombat, Log,
        TEXT("EnemyAIController: Switched to %s"), *NewClass->GetName());
}

// ============================================================
//  TURN FLOW
// ============================================================

void AEnemyAIController::RequestNextDecision()
{
    if (!bTurnActive) return;
    if (!ControlledCharacter) { EndAITurn(); return; }

    // Safety guard
    if (ActionsThisTurn >= MaxActionsPerTurn)
    {
        UE_LOG(LogCombat, Warning,
            TEXT("EnemyAIController: Max actions reached — ending turn"));
        EndAITurn();
        return;
    }

    // Check if character still has resources
    UCombatStatsComponent* Stats = ControlledCharacter->CombatStats;
    if (!Stats) { EndAITurn(); return; }

    const bool bHasActions   = Stats->CurrentActions > 0;
    const bool bHasMovement  = Stats->MovementRemaining > 0.f;

    if (!bHasActions && !bHasMovement)
    {
        EndAITurn();
        return;
    }

    // Build context and request decision
    TArray<ACombatCharacter*> AllCombatants = GetAllCombatants();
    FAIBattleContext Context = UAIDecisionMaker::BuildContext(
        ControlledCharacter, AllCombatants);

    // Populate available actions
    if (bHasActions)
    {
        FAIActionOption MeleeOpt;
        MeleeOpt.ActionClass = UMeleeAttackAction::StaticClass();
        MeleeOpt.ActionName  = FText::FromString(TEXT("Melee Attack"));
        MeleeOpt.Range       = 5.f;
        MeleeOpt.ActionType  = EActionType::Action;
        Context.AvailableActions.Add(MeleeOpt);

        FAIActionOption DodgeOpt;
        DodgeOpt.ActionClass = UDodgeAction::StaticClass();
        DodgeOpt.ActionName  = FText::FromString(TEXT("Dodge"));
        DodgeOpt.Range       = 0.f;
        DodgeOpt.ActionType  = EActionType::Action;
        Context.AvailableActions.Add(DodgeOpt);

        FAIActionOption DashOpt;
        DashOpt.ActionClass = UDashAction::StaticClass();
        DashOpt.ActionName  = FText::FromString(TEXT("Dash"));
        DashOpt.Range       = 0.f;
        DashOpt.ActionType  = EActionType::Action;
        Context.AvailableActions.Add(DashOpt);
    }

    BP_OnAIThinking();

    if (DecisionMaker)
        DecisionMaker->RequestDecision(Context);
    else
        EndAITurn();
}

void AEnemyAIController::OnDecisionReceived(FAIDecision Decision)
{
    if (!bTurnActive) return;

    OnEnemyDecisionMade.Broadcast(Decision);

    ExecuteDecision(Decision);
}

void AEnemyAIController::ExecuteDecision(const FAIDecision& Decision)
{
    if (!ControlledCharacter) { EndAITurn(); return; }

    ActionsThisTurn++;

    FActionResult Result;

    switch (Decision.DecisionType)
    {
        case EAIDecisionType::Move:
            ExecuteMove(Decision);
            break;

        case EAIDecisionType::Attack:
            ExecuteAttack(Decision);
            break;

        case EAIDecisionType::Dodge:
            ExecuteDodge(Decision);
            break;

        case EAIDecisionType::Dash:
            ExecuteDash(Decision);
            break;

        case EAIDecisionType::EndTurn:
        default:
            EndAITurn();
            return;
    }

    BP_OnActionExecuted(Decision, Result);

    // Schedule next decision after delay
    if (bTurnActive)
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                ActionDelayHandle,
                this,
                &AEnemyAIController::RequestNextDecision,
                ActionDelay,
                false);
        }
        else
        {
            RequestNextDecision();
        }
    }
}

void AEnemyAIController::ExecuteMove(const FAIDecision& Decision)
{
    if (!ControlledCharacter || Decision.TargetLocation.IsZero()) return;

    UCombatStatsComponent* Stats = ControlledCharacter->CombatStats;
    if (!Stats || Stats->MovementRemaining <= 0.f) return;

    // Request navigation move
    EPathFollowingRequestResult::Type MoveResult =
        MoveToLocation(Decision.TargetLocation, 50.f);

    // Calculate cost and deduct movement
    const float DistUU = FVector::Dist(
        ControlledCharacter->GetActorLocation(),
        Decision.TargetLocation);
    const float DistFeet = DistUU / 30.48f;

    Stats->SpendMovement(FMath::Min(DistFeet, Stats->MovementRemaining));

    UE_LOG(LogCombat, Log,
        TEXT("EnemyAIController: %s moves to (%.0f, %.0f) — %.0f feet"),
        *ControlledCharacter->CharacterName.ToString(),
        Decision.TargetLocation.X,
        Decision.TargetLocation.Y,
        DistFeet);
}

void AEnemyAIController::ExecuteAttack(const FAIDecision& Decision)
{
    if (!ControlledCharacter || !Decision.TargetCharacter) return;

    UCombatStatsComponent* Stats = ControlledCharacter->CombatStats;
    if (!Stats || Stats->CurrentActions <= 0) return;

    // Instantiate and execute melee attack
    UMeleeAttackAction* Attack = NewObject<UMeleeAttackAction>(this);
    if (!Attack) return;

    // Set proficiency from stats
    Attack->bProficient = true;

    FText Reason;
    if (!Attack->CanExecute(ControlledCharacter, Reason))
    {
        UE_LOG(LogCombat, Log,
            TEXT("EnemyAIController: Cannot execute attack — %s"),
            *Reason.ToString());
        return;
    }

    FActionResult Result = Attack->Execute(
        ControlledCharacter,
        Decision.TargetCharacter->GetActorLocation(),
        Decision.TargetCharacter);

    UE_LOG(LogCombat, Log,
        TEXT("EnemyAIController: Attack result — %s"),
        *Result.ResultMessage.ToString());
}

void AEnemyAIController::ExecuteDodge(const FAIDecision& Decision)
{
    if (!ControlledCharacter) return;

    UDodgeAction* Dodge = NewObject<UDodgeAction>(this);
    if (!Dodge) return;

    FText Reason;
    if (!Dodge->CanExecute(ControlledCharacter, Reason)) return;

    Dodge->Execute(ControlledCharacter,
        ControlledCharacter->GetActorLocation(), nullptr);

    UE_LOG(LogCombat, Log,
        TEXT("EnemyAIController: %s takes Dodge action"),
        *ControlledCharacter->CharacterName.ToString());
}

void AEnemyAIController::ExecuteDash(const FAIDecision& Decision)
{
    if (!ControlledCharacter) return;

    UDashAction* Dash = NewObject<UDashAction>(this);
    if (!Dash) return;

    FText Reason;
    if (!Dash->CanExecute(ControlledCharacter, Reason)) return;

    Dash->Execute(ControlledCharacter,
        ControlledCharacter->GetActorLocation(), nullptr);

    UE_LOG(LogCombat, Log,
        TEXT("EnemyAIController: %s dashes"),
        *ControlledCharacter->CharacterName.ToString());
}

void AEnemyAIController::EndAITurn()
{
    bTurnActive = false;

    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(ActionDelayHandle);

    BP_OnTurnEnded();

    ATurnManager* TM = GetTurnManager();
    if (TM)
        TM->EndCurrentTurn();

    UE_LOG(LogCombat, Log,
        TEXT("EnemyAIController: %s ends turn (%d actions taken)"),
        ControlledCharacter
            ? *ControlledCharacter->CharacterName.ToString()
            : TEXT("?"),
        ActionsThisTurn);
}

// ============================================================
//  HELPERS
// ============================================================

TArray<ACombatCharacter*> AEnemyAIController::GetAllCombatants() const
{
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(), ACombatCharacter::StaticClass(), Found);

    TArray<ACombatCharacter*> Result;
    for (AActor* A : Found)
    {
        if (ACombatCharacter* C = Cast<ACombatCharacter>(A))
            Result.Add(C);
    }
    return Result;
}

ATurnManager* AEnemyAIController::GetTurnManager()
{
    if (IsValid(CachedTurnManager))
        return CachedTurnManager.Get();

    CachedTurnManager = ATurnManager::GetTurnManager(this);
    return CachedTurnManager.Get();
}