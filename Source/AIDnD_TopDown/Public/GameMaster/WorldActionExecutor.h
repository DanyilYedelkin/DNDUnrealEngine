// WorldActionExecutor.h
#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "GameMaster/GameMasterTypes.h"
#include "WorldActionExecutor.generated.h"

class UWorldCatalogDataAsset;
class ULevelGenerationSettingsDataAsset;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnActionsExecuted, int32, SpawnedCount);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnActionError,
    const FString&, ActionType, const FString&, ErrorMessage);

UCLASS(BlueprintType)
class AIDND_TOPDOWN_API UWorldActionExecutor : public UObject
{
    GENERATED_BODY()

public:

    // ---- Конфигурация ----

    UPROPERTY(BlueprintReadWrite, Category = "Executor")
    TObjectPtr<UWorldCatalogDataAsset> Catalog;

    UPROPERTY(BlueprintReadWrite, Category = "Executor")
    TObjectPtr<ULevelGenerationSettingsDataAsset> Settings;

    // ---- Делегаты ----

    UPROPERTY(BlueprintAssignable, Category = "Executor")
    FOnActionsExecuted OnActionsExecuted;

    UPROPERTY(BlueprintAssignable, Category = "Executor")
    FOnActionError OnActionError;

    // ---- Счётчики ----

    UPROPERTY(BlueprintReadOnly, Category = "Executor|Stats")
    int32 SpawnedNPCs = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Executor|Stats")
    int32 SpawnedEnemies = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Executor|Stats")
    int32 SpawnedEnvironment = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Executor|Stats")
    int32 SpawnedManagers = 0;

    // ---- Публичный API ----

    /**
     * Выполнить список действий GM.
     * NavMesh спавнится АВТОМАТИЧЕСКИ перед всеми остальными действиями.
     */
    UFUNCTION(BlueprintCallable, Category = "Executor")
    void ExecuteActions(const TArray<FGMAction>& Actions, UWorld* World);

    UFUNCTION(BlueprintCallable, Category = "Executor")
    void ResetCounters();

    UFUNCTION(BlueprintPure, Category = "Executor")
    TArray<AActor*> GetSpawnedActors() const { return SpawnedActors; }

    UFUNCTION(BlueprintCallable, Category = "Executor")
    void DestroyAllSpawnedActors();

private:

    UPROPERTY()
    TArray<TObjectPtr<AActor>> SpawnedActors;

    // Флаг — NavMesh уже заспавнен в этой сессии генерации
    bool bNavMeshSpawned = false;

    // ---- NavMesh ----

    /** Спавнит NavMeshBoundsVolume гарантированно.
     *  Вызывается в начале ExecuteActions, до любых других спавнов.
     *  Если NavMesh уже есть на уровне (поставлен вручную) — пропускает. */
    void SpawnGuaranteedNavMesh(UWorld* World);

    // ---- Action handlers ----

    void ExecuteSingleAction(const FGMAction& Action, UWorld* World);

    AActor* SpawnFromCatalog(const FString& AssetID,
                              const FGMTransform& Transform,
                              UWorld* World);

    void ExecuteSpawnActor(const FGMAction& Action, UWorld* World);
    void ExecuteSpawnNPC(const FGMAction& Action, UWorld* World);
    void ExecuteSpawnEnemy(const FGMAction& Action, UWorld* World);
    void ExecuteSpawnManager(const FGMAction& Action, UWorld* World);
    void ExecuteSetQuest(const FGMAction& Action, UWorld* World);
    void ExecuteTriggerEvent(const FGMAction& Action, UWorld* World);

    bool IsLocationValid(const FVector& Location) const;
    void ReportError(const FString& ActionType, const FString& Message);
};
