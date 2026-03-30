// WorldBootstrapper.h
// Поставь этот актор на уровень. Он запускает Game Master в BeginPlay.
// Настраивается через Details: укажи DataAssets и нужные параметры.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameMaster/GameMasterTypes.h"
#include "WorldBootstrapper.generated.h"

class UGameMasterSubsystem;
class UWorldCatalogDataAsset;
class ULevelGenerationSettingsDataAsset;

/**
 * AWorldBootstrapper
 *
 * КАК ИСПОЛЬЗОВАТЬ:
 * 1. Поставь в уровень (или создай BP_WorldBootstrapper : AWorldBootstrapper)
 * 2. В Details укажи WorldCatalog и LevelSettings
 * 3. Запусти игру — GM сгенерирует мир автоматически
 *
 * Blueprint events для кастомизации:
 *   BP_OnLevelGenerationStarted  — показать loading экран
 *   BP_OnLevelGenerationComplete — скрыть loading, проиграть музыку
 *   BP_OnNarrationReady          — показать текст игроку
 */
UCLASS(Blueprintable)
class AIDND_TOPDOWN_API AWorldBootstrapper : public AActor
{
    GENERATED_BODY()

public:

    AWorldBootstrapper();

    // ============================================================
    //  КОНФИГУРАЦИЯ (заполни в Details)
    // ============================================================

    /** Каталог объектов мира. ОБЯЗАТЕЛЬНО! */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMaster|Config")
    TObjectPtr<UWorldCatalogDataAsset> WorldCatalog;

    /** Параметры генерации уровня. ОБЯЗАТЕЛЬНО! */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMaster|Config")
    TObjectPtr<ULevelGenerationSettingsDataAsset> LevelSettings;

    /** Запускать генерацию автоматически в BeginPlay? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMaster|Config")
    bool bAutoGenerateOnBeginPlay = true;

    /** Загружать сохранённую кампанию при старте? */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMaster|Config")
    bool bLoadCampaignOnStart = true;

    /** Задержка перед генерацией (секунды, 0 = сразу) */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameMaster|Config",
        meta = (ClampMin = 0.f))
    float GenerationDelay = 0.5f;

    // ============================================================
    //  ПУБЛИЧНЫЙ API
    // ============================================================

    /** Запустить генерацию вручную (например из UI) */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void TriggerLevelGeneration();

    /** Перегенерировать уровень (уничтожает старые GM-акторы) */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void RegenerateLevel();

    /** Отправить событие GM из BP */
    UFUNCTION(BlueprintCallable, Category = "GameMaster")
    void SendGameMasterEvent(const FString& EventDescription);

    // ============================================================
    //  BLUEPRINT EVENTS (override в BP наследнике)
    // ============================================================

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMaster|Events")
    void BP_OnLevelGenerationStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMaster|Events")
    void BP_OnLevelGenerationComplete(const FGMLevelGenerationResult& Result);

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMaster|Events")
    void BP_OnNarrationReady(const FString& NarrationText);

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMaster|Events")
    void BP_OnQuestUpdated(const FGMQuest& Quest);

    UFUNCTION(BlueprintImplementableEvent, Category = "GameMaster|Events")
    void BP_OnGameMasterError(const FString& ErrorMessage);

protected:

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    FVector FindSafeSpawnLocation() const;

    UPROPERTY()
    TObjectPtr<UGameMasterSubsystem> GMSubsystem;

    FTimerHandle GenerationDelayHandle;

    void SetupSubsystem();
    void SubscribeToSubsystem();
    void SpawnDungeonGeometry();

    UFUNCTION()
    void OnLevelGenerationComplete(const FGMLevelGenerationResult& Result);

    UFUNCTION()
    void OnNarrationReady(const FString& NarrationText);

    UFUNCTION()
    void OnQuestUpdated(const FGMQuest& Quest);

    UFUNCTION()
    void OnGameMasterError(const FString& ErrorMessage);
};
