// GameMasterTypes.h
// Все типы данных для системы Game Master.
// Не зависит ни от каких BP-классов — только фундамент.
#pragma once

#include "CoreMinimal.h"
#include "GameMasterTypes.generated.h"

// ============================================================
//  ACTION TYPES — allow-list типов действий GM
// ============================================================

UENUM(BlueprintType)
enum class EGMActionType : uint8
{
    Unknown       UMETA(DisplayName = "Unknown"),
    SpawnActor    UMETA(DisplayName = "Spawn Actor"),      // стены, объекты окружения
    SpawnNPC      UMETA(DisplayName = "Spawn NPC"),        // NPC с персоной
    SpawnEnemy    UMETA(DisplayName = "Spawn Enemy"),      // враги
    SpawnManager  UMETA(DisplayName = "Spawn Manager"),    // менеджеры (QuestManager и т.д.)
    SpawnNavMesh  UMETA(DisplayName = "Spawn NavMesh"),    // NavMeshBoundsVolume
    SetQuest      UMETA(DisplayName = "Set Quest"),        // задание квеста
    PlayNarration UMETA(DisplayName = "Play Narration"),   // текст нарратива
    SetWeather    UMETA(DisplayName = "Set Weather"),      // атмосфера / погода
    TriggerEvent  UMETA(DisplayName = "Trigger Event"),   // игровое событие
};

// ============================================================
//  TRANSFORM SNAPSHOT — сериализуемый трансформ
// ============================================================

USTRUCT(BlueprintType)
struct FGMTransform
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FVector  Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FVector  Scale    = FVector::OneVector;

    FTransform ToUETransform() const
    {
        return FTransform(Rotation, Location, Scale);
    }
};

// ============================================================
//  QUEST STEP
// ============================================================

USTRUCT(BlueprintType)
struct FGMQuestStep
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString StepID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Description;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct FGMQuest
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString QuestID;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Title;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Description;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FGMQuestStep> Steps;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bActive = true;
};

// ============================================================
//  GM ACTION — одно действие из массива "actions" в JSON
// ============================================================

USTRUCT(BlueprintType)
struct FGMAction
{
    GENERATED_BODY()

    // Тип — результат парсинга "type" поля
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGMActionType ActionType = EGMActionType::Unknown;

    // Raw "type" строка из JSON (для логирования)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString RawType;

    // ID ассета из каталога (для SpawnActor/SpawnNPC/SpawnEnemy)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString AssetID;

    // Количество (для SpawnEnemy — группа)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 Count = 1;

    // Трансформ спавна
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FGMTransform SpawnTransform;

    // Для SpawnNPC — персона-промт
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString PersonaPrompt;

    // Для SetQuest
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FGMQuest Quest;

    // Для PlayNarration
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString NarrationText;

    // Произвольные extra параметры (metadata)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TMap<FString, FString> ExtraParams;
};

// ============================================================
//  LEVEL GENERATION RESULT — полный ответ GM на старте
// ============================================================

USTRUCT(BlueprintType)
struct FGMLevelGenerationResult
{
    GENERATED_BODY()

    // Метаданные уровня
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString LevelTheme;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString BiomeType;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32   Seed = 0;

    // Стартовый нарратив — то что выводится игроку при входе
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString OpeningNarration;

    // Список действий для выполнения в мире
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FGMAction> Actions;

    // Обновление памяти кампании
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString MemorySummary;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FString> MemoryFacts;

    // Активные квесты после генерации
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FGMQuest> InitialQuests;

    // Была ли генерация успешной
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bSuccess = false;

    // Сообщение об ошибке (если не success)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ErrorMessage;
};

// ============================================================
//  CAMPAIGN EVENT — то что игрок делает (отправляем GM)
// ============================================================

UENUM(BlueprintType)
enum class EGMEventType : uint8
{
    PlayerAction    UMETA(DisplayName = "Player Action"),
    CombatStarted   UMETA(DisplayName = "Combat Started"),
    CombatEnded     UMETA(DisplayName = "Combat Ended"),
    QuestUpdated    UMETA(DisplayName = "Quest Updated"),
    PlayerDied      UMETA(DisplayName = "Player Died"),
    NPCInteraction  UMETA(DisplayName = "NPC Interaction"),
    Custom          UMETA(DisplayName = "Custom"),
};

USTRUCT(BlueprintType)
struct FGMCampaignEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    EGMEventType EventType = EGMEventType::Custom;

    // Текстовое описание события
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString Description;

    // Дополнительные данные
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TMap<FString, FString> Context;

    FGMCampaignEvent() {}
    FGMCampaignEvent(EGMEventType InType, const FString& InDesc)
        : EventType(InType), Description(InDesc) {}
};

// ============================================================
//  CAMPAIGN STATE — состояние кампании в памяти
// ============================================================

USTRUCT(BlueprintType)
struct FGMCampaignState
{
    GENERATED_BODY()

    // Краткое саммари всего происходившего
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString NarrativeSummary;

    // Список фактов о мире/персонажах
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FString> WorldFacts;

    // Активные квесты
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FGMQuest> ActiveQuests;

    // Завершённые квесты
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FGMQuest> CompletedQuests;

    // Последние N событий для контекста GM
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FString> RecentEventLog;

    // Метаданные текущего уровня
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString CurrentBiome;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString CurrentTheme;

    // Счётчик ходов/событий
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    int32 TurnCount = 0;
};

// ============================================================
//  GM RESPONSE — ответ GM на событие игрока во время игры
// ============================================================

USTRUCT(BlueprintType)
struct FGMNarrativeResponse
{
    GENERATED_BODY()

    // Нарратив для вывода игроку
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString NarrationText;

    // Дополнительные действия (спавн врагов, квесты и т.д.)
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FGMAction> Actions;

    // Обновление памяти
    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString MemoryUpdate;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    TArray<FString> NewFacts;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    bool bSuccess = false;

    UPROPERTY(BlueprintReadWrite, EditAnywhere)
    FString ErrorMessage;
};
