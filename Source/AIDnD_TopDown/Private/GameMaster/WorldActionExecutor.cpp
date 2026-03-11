// WorldActionExecutor.cpp
#include "GameMaster/WorldActionExecutor.h"
#include "GameMaster/WorldCatalogDataAsset.h"
#include "GameMaster/LevelGenerationSettingsDataAsset.h"
#include "AI/ChatNPC.h"

#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "NavMesh/NavMeshBoundsVolume.h"

// Для ребилда NavMesh после спавна
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogWorldExecutor, Log, All);

// ============================================================
//  PUBLIC
// ============================================================

void UWorldActionExecutor::ExecuteActions(const TArray<FGMAction>& Actions,
                                           UWorld* World)
{
    if (!World)
    {
        UE_LOG(LogWorldExecutor, Error, TEXT("ExecuteActions: World is null!"));
        return;
    }

    // --------------------------------------------------------
    // ШАГ 0: NavMesh спавнится ВСЕГДА — независимо от GM.
    // GM не обязан включать SpawnNavMesh в свой JSON.
    // --------------------------------------------------------
    SpawnGuaranteedNavMesh(World);

    // Сортируем по приоритету: Manager → Actor → NPC → Enemy → остальные
    TArray<FGMAction> Sorted = Actions;
    Sorted.Sort([](const FGMAction& A, const FGMAction& B)
    {
        auto Priority = [](EGMActionType T) -> int32
        {
            switch (T)
            {
                case EGMActionType::SpawnNavMesh:  return 0; // уже заспавнен, просто пропустим
                case EGMActionType::SpawnManager:  return 1;
                case EGMActionType::SpawnActor:    return 2;
                case EGMActionType::SpawnNPC:      return 3;
                case EGMActionType::SpawnEnemy:    return 4;
                case EGMActionType::SetQuest:      return 5;
                case EGMActionType::PlayNarration: return 6;
                case EGMActionType::TriggerEvent:  return 7;
                default: return 99;
            }
        };
        return Priority(A.ActionType) < Priority(B.ActionType);
    });

    int32 ExecutedCount = 0;
    for (const FGMAction& Action : Sorted)
    {
        // SpawnNavMesh из GM-ответа пропускаем — уже сделано в SpawnGuaranteedNavMesh
        if (Action.ActionType == EGMActionType::SpawnNavMesh)
            continue;

        ExecuteSingleAction(Action, World);
        ExecutedCount++;
    }

    UE_LOG(LogWorldExecutor, Log,
        TEXT("Executed %d GM actions. Spawned: %d env, %d NPC, %d enemies, %d managers."),
        ExecutedCount, SpawnedEnvironment, SpawnedNPCs, SpawnedEnemies, SpawnedManagers);

    OnActionsExecuted.Broadcast(
        SpawnedEnvironment + SpawnedNPCs + SpawnedEnemies + SpawnedManagers);
}

void UWorldActionExecutor::ResetCounters()
{
    SpawnedNPCs         = 0;
    SpawnedEnemies      = 0;
    SpawnedEnvironment  = 0;
    SpawnedManagers     = 0;
    bNavMeshSpawned     = false;
}

void UWorldActionExecutor::DestroyAllSpawnedActors()
{
    for (TObjectPtr<AActor>& Actor : SpawnedActors)
    {
        if (IsValid(Actor))
            Actor->Destroy();
    }
    SpawnedActors.Empty();
    ResetCounters();
}

// ============================================================
//  GUARANTEED NAVMESH
// ============================================================

void UWorldActionExecutor::SpawnGuaranteedNavMesh(UWorld* World)
{
    if (bNavMeshSpawned) return;

    // Если на уровне уже есть NavMeshBoundsVolume (поставлен вручную) — не дублируем
    for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
    {
        UE_LOG(LogWorldExecutor, Log,
            TEXT("NavMeshBoundsVolume already exists on level — skip auto-spawn"));
        bNavMeshSpawned = true;
        return;
    }

    // Берём параметры из Settings если есть, иначе дефолт
    FVector Center = FVector(0, 0, 100);
    FVector Extent = FVector(2500, 2500, 300);

    if (Settings)
    {
        Center = Settings->NavMeshCenter;
        Extent = Settings->NavMeshExtent;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ANavMeshBoundsVolume* NavVol = World->SpawnActor<ANavMeshBoundsVolume>(
        ANavMeshBoundsVolume::StaticClass(),
        FTransform(FRotator::ZeroRotator, Center),
        Params);

    if (NavVol)
    {
        // Задаём размер через SetBrushExtents (работает в UE5 на Game Thread)
        NavVol->SetActorScale3D(Extent / 100.f); // BrushBuilder default box = 100 units

        // Принудительно пересобрать NavMesh
        UNavigationSystemV1* NavSys =
            FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
        if (NavSys)
        {
            NavSys->OnNavigationBoundsUpdated(NavVol);
        }

        SpawnedActors.Add(NavVol);
        bNavMeshSpawned = true;

        UE_LOG(LogWorldExecutor, Log,
            TEXT("Auto-spawned NavMeshBoundsVolume: center=%s, extent=%s"),
            *Center.ToString(), *Extent.ToString());
    }
    else
    {
        UE_LOG(LogWorldExecutor, Warning,
            TEXT("Failed to spawn NavMeshBoundsVolume!"));
    }
}

// ============================================================
//  PRIVATE — основной диспетчер
// ============================================================

void UWorldActionExecutor::ExecuteSingleAction(const FGMAction& Action,
                                                UWorld* World)
{
    switch (Action.ActionType)
    {
        case EGMActionType::SpawnActor:    ExecuteSpawnActor(Action, World);   break;
        case EGMActionType::SpawnNPC:      ExecuteSpawnNPC(Action, World);     break;
        case EGMActionType::SpawnEnemy:    ExecuteSpawnEnemy(Action, World);   break;
        case EGMActionType::SpawnManager:  ExecuteSpawnManager(Action, World); break;
        case EGMActionType::SetQuest:      ExecuteSetQuest(Action, World);     break;
        case EGMActionType::TriggerEvent:  ExecuteTriggerEvent(Action, World); break;
        case EGMActionType::PlayNarration: /* обрабатывается в Subsystem */   break;
        default:
            ReportError(Action.RawType,
                FString::Printf(TEXT("Unknown action type '%s'"), *Action.RawType));
            break;
    }
}

// ============================================================
//  Спавн из каталога (используется для Environment, NPC, Enemy)
// ============================================================

AActor* UWorldActionExecutor::SpawnFromCatalog(const FString& AssetID,
                                                const FGMTransform& Transform,
                                                UWorld* World)
{
    if (!Catalog)
    {
        ReportError(AssetID, TEXT("Catalog is not assigned!"));
        return nullptr;
    }

    if (!IsLocationValid(Transform.Location))
    {
        ReportError(AssetID, FString::Printf(
            TEXT("Location %s out of bounds for '%s'"),
            *Transform.Location.ToString(), *AssetID));
        return nullptr;
    }

    FWorldCatalogEntry Entry;
    if (!Catalog->FindEntry(AssetID, Entry))
    {
        ReportError(AssetID, FString::Printf(
            TEXT("Asset '%s' not found in catalog"), *AssetID));
        return nullptr;
    }

    if (Entry.ActorClass.IsNull())
    {
        ReportError(AssetID, FString::Printf(
            TEXT("Asset '%s' has null class"), *AssetID));
        return nullptr;
    }

    // Синхронная загрузка — приемлемо при старте уровня
    UClass* ActorClass = Entry.ActorClass.LoadSynchronous();
    if (!ActorClass) return nullptr;

    FActorSpawnParameters SpawnParams;
    SpawnParams.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    // Transform включает Scale — именно так GM масштабирует блоки стен
    AActor* Spawned = World->SpawnActor<AActor>(
        ActorClass,
        Transform.ToUETransform(),
        SpawnParams);

    if (Spawned)
    {
        if (!Entry.Tag.IsNone())
            Spawned->Tags.AddUnique(Entry.Tag);
        Spawned->Tags.AddUnique(TEXT("GM_Spawned"));
    }

    return Spawned;
}

// ============================================================
//  SpawnActor — объекты окружения
//  Open World: деревья, камни, дома-декорации
//  Dungeon:    блоки стен (с произвольным масштабом!)
// ============================================================

void UWorldActionExecutor::ExecuteSpawnActor(const FGMAction& Action, UWorld* World)
{
    if (!Settings) { ReportError(TEXT("SpawnActor"), TEXT("Settings not assigned")); return; }
    if (SpawnedEnvironment >= Settings->MaxEnvironmentActors)
    {
        ReportError(TEXT("SpawnActor"), FString::Printf(
            TEXT("Env limit reached (%d)"), Settings->MaxEnvironmentActors));
        return;
    }

    AActor* Actor = SpawnFromCatalog(Action.AssetID, Action.SpawnTransform, World);
    if (Actor)
    {
        SpawnedActors.Add(Actor);
        SpawnedEnvironment++;
        UE_LOG(LogWorldExecutor, Verbose,
            TEXT("SpawnActor '%s' at %s scale %s"),
            *Action.AssetID,
            *Action.SpawnTransform.Location.ToString(),
            *Action.SpawnTransform.Scale.ToString());
    }
}

// ============================================================
//  SpawnNPC
// ============================================================

void UWorldActionExecutor::ExecuteSpawnNPC(const FGMAction& Action, UWorld* World)
{
    if (!Settings) { ReportError(TEXT("SpawnNPC"), TEXT("Settings not assigned")); return; }
    if (SpawnedNPCs >= Settings->MaxNPCs)
    {
        ReportError(TEXT("SpawnNPC"), FString::Printf(
            TEXT("NPC limit reached (%d)"), Settings->MaxNPCs));
        return;
    }

    AActor* Actor = SpawnFromCatalog(Action.AssetID, Action.SpawnTransform, World);
    if (Actor)
    {
        SpawnedActors.Add(Actor);
        SpawnedNPCs++;

        if (AChatNPC* NPC = Cast<AChatNPC>(Actor))
        {
            NPC->NPCID = Action.AssetID;
            if (!Action.PersonaPrompt.IsEmpty())
                NPC->SystemPrompt = Action.PersonaPrompt;
            NPC->NPCDisplayName = Action.AssetID;
        }

        UE_LOG(LogWorldExecutor, Log,
            TEXT("SpawnNPC '%s' at %s"),
            *Action.AssetID, *Action.SpawnTransform.Location.ToString());
    }
}

// ============================================================
//  SpawnEnemy — группой
// ============================================================

void UWorldActionExecutor::ExecuteSpawnEnemy(const FGMAction& Action, UWorld* World)
{
    if (!Settings) { ReportError(TEXT("SpawnEnemy"), TEXT("Settings not assigned")); return; }

    int32 CountToSpawn = FMath::Clamp(Action.Count, 1, 8);

    for (int32 i = 0; i < CountToSpawn; ++i)
    {
        if (SpawnedEnemies >= Settings->MaxEnemies)
        {
            ReportError(TEXT("SpawnEnemy"), FString::Printf(
                TEXT("Enemy limit reached (%d)"), Settings->MaxEnemies));
            break;
        }

        FGMTransform OffsetTransform = Action.SpawnTransform;
        OffsetTransform.Location += FVector(
            FMath::RandRange(-150.f, 150.f),
            FMath::RandRange(-150.f, 150.f),
            0.f);

        AActor* Actor = SpawnFromCatalog(Action.AssetID, OffsetTransform, World);
        if (Actor)
        {
            SpawnedActors.Add(Actor);
            SpawnedEnemies++;
        }
    }

    UE_LOG(LogWorldExecutor, Log,
        TEXT("SpawnEnemy '%s' x%d"), *Action.AssetID, CountToSpawn);
}

// ============================================================
//  SpawnManager — системные акторы (защита от дублей)
// ============================================================

void UWorldActionExecutor::ExecuteSpawnManager(const FGMAction& Action, UWorld* World)
{
    if (!Catalog) { ReportError(TEXT("SpawnManager"), TEXT("Catalog not assigned")); return; }

    FWorldCatalogEntry Entry;
    if (!Catalog->FindEntry(Action.AssetID, Entry))
    {
        UE_LOG(LogWorldExecutor, Log,
            TEXT("SpawnManager: '%s' not in catalog — may already be in level"),
            *Action.AssetID);
        return;
    }

    if (Entry.ActorClass.IsNull()) return;
    UClass* ManagerClass = Entry.ActorClass.LoadSynchronous();
    if (!ManagerClass) return;

    // Проверяем дубли
    for (TActorIterator<AActor> It(World, ManagerClass); It; ++It)
    {
        UE_LOG(LogWorldExecutor, Log,
            TEXT("SpawnManager: '%s' already on level, skip"), *Action.AssetID);
        return;
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    AActor* Manager = World->SpawnActor<AActor>(
        ManagerClass, Action.SpawnTransform.ToUETransform(), Params);

    if (Manager)
    {
        SpawnedActors.Add(Manager);
        SpawnedManagers++;
        UE_LOG(LogWorldExecutor, Log, TEXT("SpawnManager '%s'"), *Action.AssetID);
    }
}

void UWorldActionExecutor::ExecuteSetQuest(const FGMAction& Action, UWorld* World)
{
    UE_LOG(LogWorldExecutor, Log,
        TEXT("SetQuest: '%s' — '%s'"), *Action.Quest.QuestID, *Action.Quest.Title);
}

void UWorldActionExecutor::ExecuteTriggerEvent(const FGMAction& Action, UWorld* World)
{
    UE_LOG(LogWorldExecutor, Log, TEXT("TriggerEvent: '%s'"), *Action.NarrationText);
}

// ============================================================
//  Утилиты
// ============================================================

bool UWorldActionExecutor::IsLocationValid(const FVector& Location) const
{
    if (!Settings) return true;
    return Location.X >= Settings->WorldMin.X && Location.X <= Settings->WorldMax.X
        && Location.Y >= Settings->WorldMin.Y && Location.Y <= Settings->WorldMax.Y
        && Location.Z >= Settings->WorldMin.Z && Location.Z <= Settings->WorldMax.Z;
}

void UWorldActionExecutor::ReportError(const FString& ActionType,
                                        const FString& Message)
{
    UE_LOG(LogWorldExecutor, Warning,
        TEXT("[WorldActionExecutor] '%s': %s"), *ActionType, *Message);
    OnActionError.Broadcast(ActionType, Message);
}
