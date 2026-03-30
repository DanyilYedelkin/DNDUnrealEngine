// Source/AIDnD_TopDown/Private/Combat/BT/BTTask_DetectPlayer.cpp
#include "Combat/BT/BTTask_DetectPlayer.h"
#include "Combat/AI/EnemyAIController.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatLog.h"

#include "BehaviorTree/BlackboardComponent.h"
#include "Combat/CombatStatsComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"

UBTTask_DetectPlayer::UBTTask_DetectPlayer()
{
    NodeName = TEXT("Detect Player");
    bNotifyTick = false;
}

EBTNodeResult::Type UBTTask_DetectPlayer::ExecuteTask(
    UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    AEnemyAIController* Controller =
        Cast<AEnemyAIController>(OwnerComp.GetAIOwner());
    if (!Controller) return EBTNodeResult::Failed;

    APawn* EnemyPawn = Controller->GetPawn();
    if (!EnemyPawn) return EBTNodeResult::Failed;

    // Ищем всех ACombatCharacter на уровне
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(
        GetWorld(), ACombatCharacter::StaticClass(), FoundActors);

    ACombatCharacter* NearestPlayer = nullptr;
    float NearestDist = MAX_FLT;

    for (AActor* Actor : FoundActors)
    {
        ACombatCharacter* CombatChar = Cast<ACombatCharacter>(Actor);
        if (!CombatChar) continue;
        if (!CombatChar->bIsPlayerControlled) continue;
        if (!CombatChar->CombatStats) continue;
        if (!CombatChar->CombatStats->IsAlive()) continue;

        const float Dist = FVector::Dist(
            EnemyPawn->GetActorLocation(),
            CombatChar->GetActorLocation());

        if (Dist <= DetectionRadius && Dist < NearestDist)
        {
            NearestDist  = Dist;
            NearestPlayer = CombatChar;
        }
    }

    if (!NearestPlayer)
        return EBTNodeResult::Succeeded;
    
    UBlackboardComponent* BB = OwnerComp.GetBlackboardComponent();
    if (BB)
    {
        BB->SetValueAsObject(TargetActorKey.SelectedKeyName,
            NearestPlayer);
        BB->SetValueAsBool(IsInCombatKey.SelectedKeyName, true);
    }

    UE_LOG(LogCombat, Log,
        TEXT("BTTask_DetectPlayer: %s detected %s (%.0f UU)"),
        *EnemyPawn->GetName(),
        *NearestPlayer->CharacterName.ToString(),
        NearestDist);

    return EBTNodeResult::Succeeded;
}

FString UBTTask_DetectPlayer::GetStaticDescription() const
{
    return FString::Printf(
        TEXT("Detect player within %.0f UU"), DetectionRadius);
}