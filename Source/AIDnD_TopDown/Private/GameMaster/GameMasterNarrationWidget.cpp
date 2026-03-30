// GameMasterNarrationWidget.cpp
#include "GameMaster/GameMasterNarrationWidget.h"
#include "GameMaster/GameMasterSubsystem.h"
#include "Kismet/GameplayStatics.h"

void UGameMasterNarrationWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SubscribeToGM();
}

void UGameMasterNarrationWidget::NativeDestruct()
{
    UnsubscribeFromGM();
    Super::NativeDestruct();
}

void UGameMasterNarrationWidget::SubscribeToGM()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    GMSubsystem = GI->GetSubsystem<UGameMasterSubsystem>();
    if (!GMSubsystem) return;

    GMSubsystem->OnNarrationReady.AddDynamic(
        this, &UGameMasterNarrationWidget::OnNarrationReady);
    GMSubsystem->OnQuestUpdated.AddDynamic(
        this, &UGameMasterNarrationWidget::OnQuestUpdated_Internal);
    GMSubsystem->OnLevelGenerationComplete.AddDynamic(
        this, &UGameMasterNarrationWidget::OnLevelGenerationComplete);
    GMSubsystem->OnGameMasterError.AddDynamic(
        this, &UGameMasterNarrationWidget::OnGameMasterError);
}

void UGameMasterNarrationWidget::UnsubscribeFromGM()
{
    if (!GMSubsystem) return;

    GMSubsystem->OnNarrationReady.RemoveDynamic(
        this, &UGameMasterNarrationWidget::OnNarrationReady);
    GMSubsystem->OnQuestUpdated.RemoveDynamic(
        this, &UGameMasterNarrationWidget::OnQuestUpdated_Internal);
    GMSubsystem->OnLevelGenerationComplete.RemoveDynamic(
        this, &UGameMasterNarrationWidget::OnLevelGenerationComplete);
    GMSubsystem->OnGameMasterError.RemoveDynamic(
        this, &UGameMasterNarrationWidget::OnGameMasterError);
}

void UGameMasterNarrationWidget::ShowNarration(const FString& Text)
{
    CurrentNarration = Text;
    NarrationHistory.Add(Text);
    // Держим не более 50 записей в истории
    while (NarrationHistory.Num() > 50) NarrationHistory.RemoveAt(0);
    BP_OnNewNarration(Text);
}

void UGameMasterNarrationWidget::ClearHistory()
{
    NarrationHistory.Empty();
    CurrentNarration.Empty();
}

FString UGameMasterNarrationWidget::GetFullHistory() const
{
    FString Full;
    for (const FString& Line : NarrationHistory)
    {
        Full += Line + TEXT("\n\n");
    }
    return Full;
}

// ---- Callbacks ----

void UGameMasterNarrationWidget::OnNarrationReady(const FString& NarrationText)
{
    ShowNarration(NarrationText);
}

void UGameMasterNarrationWidget::OnQuestUpdated_Internal(const FGMQuest& Quest)
{
    // Обновляем список квестов
    bool bFound = false;
    for (FGMQuest& Existing : ActiveQuests)
    {
        if (Existing.QuestID == Quest.QuestID)
        {
            Existing = Quest;
            bFound = true;
            break;
        }
    }
    if (!bFound && Quest.bActive)
    {
        ActiveQuests.Add(Quest);
    }
    else if (!Quest.bActive)
    {
        ActiveQuests.RemoveAll([&Quest](const FGMQuest& Q){
            return Q.QuestID == Quest.QuestID;
        });
    }

    BP_OnQuestUpdated(Quest);
}

void UGameMasterNarrationWidget::OnLevelGenerationComplete(
    const FGMLevelGenerationResult& Result)
{
    BP_OnGenerationComplete(Result.bSuccess);
}

void UGameMasterNarrationWidget::OnGameMasterError(const FString& ErrorMessage)
{
    BP_OnError(ErrorMessage);
}
