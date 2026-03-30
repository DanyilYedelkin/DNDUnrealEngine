#include "Combat/BT/BTTask_Patrol.h"
#include "Combat/AI/EnemyAIController.h"
#include "Combat/CombatLog.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Navigation/PathFollowingComponent.h"

UBTTask_Patrol::UBTTask_Patrol()
{
    NodeName            = TEXT("Patrol");
    bNotifyTick         = true;
    bCreateNodeInstance = true;
}

EBTNodeResult::Type UBTTask_Patrol::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AEnemyAIController* Controller =
        Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
    if (!Controller) return EBTNodeResult::Failed;

    FVector NextPoint = Controller->GetNextPatrolPoint();
    if (NextPoint.IsZero()) return EBTNodeResult::Failed;

    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (BB)
        BB->SetValueAsVector(PatrolLocationKey.SelectedKeyName, NextPoint);

    FAIMoveRequest MoveReq;
    MoveReq.SetGoalLocation(NextPoint);
    MoveReq.SetAcceptanceRadius(AcceptanceRadius);

    FNavPathSharedPtr NavPath;
    Controller->MoveTo(MoveReq, &NavPath);

    UE_LOG(LogCombat, Verbose,
        TEXT("BTTask_Patrol: Moving to (%.0f, %.0f)"),
        NextPoint.X, NextPoint.Y);

    return EBTNodeResult::InProgress;
}

void UBTTask_Patrol::TickTask(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory, float DeltaSeconds)
{
    AEnemyAIController* Controller =
        Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
    if (!Controller)
    {
        FinishLatentTask(OwnerComp, EBTNodeResult::Failed);
        return;
    }

    // Проверяем достигли ли точки
    const EPathFollowingStatus::Type MoveStatus =
        Controller->GetMoveStatus();

    if (MoveStatus == EPathFollowingStatus::Idle)
    {
        // Достигли точки — переходим к следующей
        Controller->AdvancePatrolIndex();
        FinishLatentTask(OwnerComp, EBTNodeResult::Succeeded);
    }
}

void UBTTask_Patrol::OnTaskFinished(
    UBehaviorTreeComponent& OwnerComp,
    uint8* NodeMemory,
    EBTNodeResult::Type TaskResult)
{
    // Ничего не делаем — индекс уже обновлён в TickTask
}

FString UBTTask_Patrol::GetStaticDescription() const
{
    return TEXT("Move to next patrol point");
}