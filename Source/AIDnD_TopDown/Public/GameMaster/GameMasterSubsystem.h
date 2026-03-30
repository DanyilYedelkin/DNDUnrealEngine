// GameMasterSubsystem.h
// Главный мозг системы Game Master.
// GameInstance Subsystem — живёт всю сессию игры.
// Формирует промты, вызывает OpenAI, валидирует JSON, управляет кампанией.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameMaster/GameMasterTypes.h"
#include "AI/ChatTypes.h"  // FChatMessage, EChatRole — из существующего кода
#include "GameMasterSubsystem.generated.h"

class UOpenAIChatService;          // существующий HTTP сервис
class UWorldCatalogDataAsset;
class ULevelGenerationSettingsDataAsset;
class UWorldActionExecutor;
class USaveGame_CampaignMemory;

// ============================================================
//  Делегаты — видимы в Blueprint
// ============================================================

/** Генерация уровня завершена */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLevelGenerationComplete,
    const FGMLevelGenerationResult&, Result);

/** GM ответил на событие игрока */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrativeResponseReceived,
    const FGMNarrativeResponse&, Response);

/** Нарратив для вывода игроку */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNarrationReady,
    const FString&, NarrationText);

/** Квест добавлен / обновлён */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnQuestUpdated,
    const FGMQuest&, Quest);

/** Ошибка GM (нет интернета, неверный JSON и т.д.) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnGameMasterError,
    const FString&, ErrorMessage);

// ============================================================
//  UGameMasterSubsystem
// ============================================================

UCLASS()
class AIDND_TOPDOWN_API UGameMasterSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // ============================================================
    //  LIFECYCLE
    // ============================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ============================================================
    //  КОНФИГУРАЦИЯ (устанавливается из AWorldBootstrapper)
    // ============================================================

    /** Каталог объектов мира. Обязателен! */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameMaster|Config")
    TObjectPtr<UWorldCatalogDataAsset> WorldCatalog;

    /** Параметры генерации уровня */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameMaster|Config")
    TObjectPtr<ULevelGenerationSettingsDataAsset> LevelSettings;

    /** Модель OpenAI */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameMaster|Config")
    FString GMModel = TEXT("gpt-4o-mini");

    /** Максимальный кол-во токенов на генерацию уровня */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameMaster|Config",
        meta = (ClampMin = 500, ClampMax = 4000))
    int32 LevelGenerationMaxTokens = 3500;

    /** Максимальный кол-во токенов на narrative response */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameMaster|Config",
        meta = (ClampMin = 200, ClampMax = 2000))
    int32 NarrativeMaxTokens = 1000;

    /** Cooldown между GM-запросами (сек) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameMaster|Config",
        meta = (ClampMin = 2.f))
    float RequestCooldown = 5.f;

    // ============================================================
    //  ДЕЛЕГАТЫ
    // ============================================================

    UPROPERTY(BlueprintAssignable, Category = "GameMaster|Events")
    FOnLevelGenerationComplete OnLevelGenerationComplete;

    UPROPERTY(BlueprintAssignable, Category = "GameMaster|Events")
    FOnNarrativeResponseReceived OnNarrativeResponseReceived;

    UPROPERTY(BlueprintAssignable, Category = "GameMaster|Events")
    FOnNarrationReady OnNarrationReady;

    UPROPERTY(BlueprintAssignable, Category = "GameMaster|Events")
    FOnQuestUpdated OnQuestUpdated;

    UPROPERTY(BlueprintAssignable, Category = "GameMaster|Events")
    FOnGameMasterError OnGameMasterError;

    // ============================================================
    //  ПУБЛИЧНЫЙ API
    // ============================================================

    /**
     * Запустить генерацию уровня.
     * Вызывается из AWorldBootstrapper::BeginPlay.
     * Результат — через OnLevelGenerationComplete.
     */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void GenerateLevel(UWorld* World);

    /**
     * Отправить событие GM для narrative response.
     * Вызывается когда игрок что-то делает, входит в зону и т.д.
     * Результат — через OnNarrativeResponseReceived.
     */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void SendCampaignEvent(const FGMCampaignEvent& Event, UWorld* World);

    /** Удобная версия — просто строка события */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void SendSimpleEvent(const FString& EventDescription, UWorld* World);

    /** Завершить квест */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void CompleteQuest(const FString& QuestID);

    /** Обновить шаг квеста */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void CompleteQuestStep(const FString& QuestID, const FString& StepID);

    /** Получить текущее состояние кампании */
    UFUNCTION(BlueprintPure, Category = "GameMaster")
    FGMCampaignState GetCampaignState() const;

    /** Получить активные квесты */
    UFUNCTION(BlueprintPure, Category = "GameMaster")
    TArray<FGMQuest> GetActiveQuests() const;

    /** Идёт ли сейчас запрос к OpenAI? */
    UFUNCTION(BlueprintPure, Category = "GameMaster")
    bool IsRequestInFlight() const { return bRequestInFlight; }

    /** Уничтожить все GM-спавненные акторы (для повторной генерации) */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void ClearGeneratedWorld();

    /** Сохранить состояние кампании */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void SaveCampaign();

    /** Загрузить состояние кампании */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void LoadCampaign();

    // ============================================================
    //  PROMPT BUILDING — доступен для override в BP
    // ============================================================

    /** Получить полный системный промт для GM */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GameMaster|Prompts")
    FString BuildGMSystemPrompt() const;

    /** Получить промт для генерации уровня */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GameMaster|Prompts")
    FString BuildLevelGenerationPrompt() const;

    /** Получить промт для события */
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "GameMaster|Prompts")
    FString BuildCampaignEventPrompt(const FGMCampaignEvent& Event) const;

private:

    // ---- Зависимости ----

    UPROPERTY()
    TObjectPtr<UOpenAIChatService> OpenAIService;

    UPROPERTY()
    TObjectPtr<UWorldActionExecutor> ActionExecutor;

    UPROPERTY()
    TObjectPtr<USaveGame_CampaignMemory> SaveData;

    // ---- Состояние ----

    bool bRequestInFlight = false;
    bool bLevelGenerated  = false;
    double LastRequestTime = 0.0;

    // ---- HTTP helpers ----

    void SendGMRequest(
        const TArray<FChatMessage>& Messages,
        int32 MaxTokens,
        TFunction<void(const FString&)> OnSuccess,
        TFunction<void(const FString&)> OnError);

    // ---- JSON парсинг ----

    /** Парсить ответ GM на генерацию уровня */
    FGMLevelGenerationResult ParseLevelGenerationResponse(const FString& JsonString) const;

    /** Парсить ответ GM на событие кампании */
    FGMNarrativeResponse ParseNarrativeResponse(const FString& JsonString) const;

    /** Парсить один action из JSON объекта */
    FGMAction ParseSingleAction(const TSharedPtr<FJsonObject>& ActionObj) const;

    /** Парсить квест из JSON */
    FGMQuest ParseQuest(const TSharedPtr<FJsonObject>& QuestObj) const;

    /** Извлечь JSON из строки (убрать markdown блоки если модель их добавила) */
    FString ExtractJsonFromResponse(const FString& RawResponse) const;

    // ---- Валидация ----

    /** Валидировать список actions: лимиты, типы, координаты */
    TArray<FGMAction> ValidateActions(const TArray<FGMAction>& Actions) const;

    // ---- Campaign helpers ----

    void ApplyLevelResultToCampaign(const FGMLevelGenerationResult& Result);
    void ApplyNarrativeResponseToCampaign(const FGMNarrativeResponse& Response);
    void AddQuestToCampaign(const FGMQuest& Quest);
    FString GetCatalogSummaryForPrompt() const;

    // ---- Save/Load ----

    void EnsureSaveDataExists();
};
