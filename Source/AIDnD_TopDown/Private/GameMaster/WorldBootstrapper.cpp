// WorldBootstrapper.cpp
#include "GameMaster/WorldBootstrapper.h"
#include "GameMaster/GameMasterSubsystem.h"
#include "Kismet/GameplayStatics.h"

AWorldBootstrapper::AWorldBootstrapper()
{
    PrimaryActorTick.bCanEverTick = false;
    // Рут компонент — сцена (невидим)
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void AWorldBootstrapper::BeginPlay()
{
    Super::BeginPlay();

    SetupSubsystem();

    if (bLoadCampaignOnStart && GMSubsystem)
    {
        GMSubsystem->LoadCampaign();
    }

    if (bAutoGenerateOnBeginPlay)
    {
        if (GenerationDelay > 0.f)
        {
            GetWorldTimerManager().SetTimer(
                GenerationDelayHandle,
                this, &AWorldBootstrapper::TriggerLevelGeneration,
                GenerationDelay, false);
        }
        else
        {
            TriggerLevelGeneration();
        }
    }
}

void AWorldBootstrapper::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(GenerationDelayHandle);
    Super::EndPlay(EndPlayReason);
}

void AWorldBootstrapper::SetupSubsystem()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    GMSubsystem = GI->GetSubsystem<UGameMasterSubsystem>();
    if (!GMSubsystem) return;

    // Передаём конфигурацию в subsystem
    GMSubsystem->WorldCatalog   = WorldCatalog;
    GMSubsystem->LevelSettings  = LevelSettings;

    SubscribeToSubsystem();
}

void AWorldBootstrapper::SubscribeToSubsystem()
{
    if (!GMSubsystem) return;

    GMSubsystem->OnLevelGenerationComplete.AddDynamic(
        this, &AWorldBootstrapper::OnLevelGenerationComplete);
    GMSubsystem->OnNarrationReady.AddDynamic(
        this, &AWorldBootstrapper::OnNarrationReady);
    GMSubsystem->OnQuestUpdated.AddDynamic(
        this, &AWorldBootstrapper::OnQuestUpdated);
    GMSubsystem->OnGameMasterError.AddDynamic(
        this, &AWorldBootstrapper::OnGameMasterError);
}

void AWorldBootstrapper::TriggerLevelGeneration()
{
    if (!GMSubsystem)
    {
        SetupSubsystem();
        if (!GMSubsystem) return;
    }
    BP_OnLevelGenerationStarted();
    GMSubsystem->GenerateLevel(GetWorld());
}

void AWorldBootstrapper::RegenerateLevel()
{
    if (GMSubsystem)
    {
        GMSubsystem->ClearGeneratedWorld();
        TriggerLevelGeneration();
    }
}

void AWorldBootstrapper::SendGameMasterEvent(const FString& EventDescription)
{
    if (GMSubsystem)
    {
        GMSubsystem->SendSimpleEvent(EventDescription, GetWorld());
    }
}

// ---- Callbacks → Blueprint events ----

void AWorldBootstrapper::OnLevelGenerationComplete(
    const FGMLevelGenerationResult& Result)
{
    BP_OnLevelGenerationComplete(Result);
}

void AWorldBootstrapper::OnNarrationReady(const FString& NarrationText)
{
    BP_OnNarrationReady(NarrationText);
}

void AWorldBootstrapper::OnQuestUpdated(const FGMQuest& Quest)
{
    BP_OnQuestUpdated(Quest);
}

void AWorldBootstrapper::OnGameMasterError(const FString& ErrorMessage)
{
    BP_OnGameMasterError(ErrorMessage);
}
