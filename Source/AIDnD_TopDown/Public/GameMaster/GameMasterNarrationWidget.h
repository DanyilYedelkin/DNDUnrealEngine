// GameMasterNarrationWidget.h
// UMG виджет для отображения нарратива GM.
// Создай BP_NarrationWidget : UGameMasterNarrationWidget и дизайни UI в редакторе.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameMaster/GameMasterTypes.h"
#include "GameMasterNarrationWidget.generated.h"

class UTextBlock;
class UScrollBox;
class URichTextBlock;
class UGameMasterSubsystem;

/**
 * UGameMasterNarrationWidget
 *
 * Подписывается на UGameMasterSubsystem::OnNarrationReady автоматически.
 * В BP наследнике привяжи NarrationText к переменной типа UTextBlock или URichTextBlock.
 *
 * Blueprint события:
 *   BP_OnNewNarration     — вызывается с новым текстом (анимируй появление)
 *   BP_OnQuestUpdated     — уведомление о квесте
 *   BP_OnGenerationStart  — показать spinner/loading
 *   BP_OnGenerationEnd    — скрыть spinner
 */
UCLASS(Blueprintable)
class AIDND_TOPDOWN_API UGameMasterNarrationWidget : public UUserWidget
{
    GENERATED_BODY()

public:

    // ---- Текущий нарратив ----

    UPROPERTY(BlueprintReadOnly, Category = "Narration")
    FString CurrentNarration;

    // История нарративов (для scroll log)
    UPROPERTY(BlueprintReadOnly, Category = "Narration")
    TArray<FString> NarrationHistory;

    // Активные квесты (для quest log UI)
    UPROPERTY(BlueprintReadOnly, Category = "Quests")
    TArray<FGMQuest> ActiveQuests;

    // ---- Blueprint Events ----

    UFUNCTION(BlueprintImplementableEvent, Category = "Narration|Events")
    void BP_OnNewNarration(const FString& Text);

    UFUNCTION(BlueprintImplementableEvent, Category = "Narration|Events")
    void BP_OnQuestUpdated(const FGMQuest& Quest);

    UFUNCTION(BlueprintImplementableEvent, Category = "Narration|Events")
    void BP_OnGenerationStarted();

    UFUNCTION(BlueprintImplementableEvent, Category = "Narration|Events")
    void BP_OnGenerationComplete(bool bSuccess);

    UFUNCTION(BlueprintImplementableEvent, Category = "Narration|Events")
    void BP_OnError(const FString& ErrorMessage);

    // ---- Публичный BP API ----

    UFUNCTION(BlueprintCallable, Category = "Narration")
    void ShowNarration(const FString& Text);

    UFUNCTION(BlueprintCallable, Category = "Narration")
    void ClearHistory();

    UFUNCTION(BlueprintCallable, Category = "Narration")
    FString GetFullHistory() const;

protected:

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:

    UPROPERTY()
    TObjectPtr<UGameMasterSubsystem> GMSubsystem;

    void SubscribeToGM();
    void UnsubscribeFromGM();

    UFUNCTION()
    void OnNarrationReady(const FString& NarrationText);

    UFUNCTION()
    void OnQuestUpdated_Internal(const FGMQuest& Quest);

    UFUNCTION()
    void OnLevelGenerationComplete(const FGMLevelGenerationResult& Result);

    UFUNCTION()
    void OnGameMasterError(const FString& ErrorMessage);
};
