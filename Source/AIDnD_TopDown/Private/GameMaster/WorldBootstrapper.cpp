// WorldBootstrapper.cpp
#include "GameMaster/WorldBootstrapper.h"

#include "HttpModule.h"
#include "GameMaster/GameMasterSubsystem.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
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

FVector AWorldBootstrapper::FindSafeSpawnLocation() const
{
    UWorld* World = GetWorld();
    if (!World) return FVector(0, 0, 100);

    TArray<FVector> Candidates = {
        FVector(  0,    0, 100),
        FVector(200,    0, 100),
        FVector( -200,  0, 100),
        FVector(  0,  200, 100),
        FVector(  0, -200, 100),
        FVector(300,  300, 100),
        FVector(-300, 300, 100),
    };

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    for (const FVector& Candidate : Candidates)
    {
        FCollisionShape Sphere = FCollisionShape::MakeSphere(80.f);
        bool bBlocked = World->OverlapBlockingTestByChannel(
            Candidate,
            FQuat::Identity,
            ECC_Pawn,
            Sphere,
            Params);

        if (!bBlocked)
        {
            UE_LOG(LogTemp, Log, 
                TEXT("WorldBootstrapper: Safe spawn found at %s"), 
                *Candidate.ToString());
            return Candidate;
        }
    }

    UE_LOG(LogTemp, Warning, 
        TEXT("WorldBootstrapper: No safe spawn found, using elevated fallback"));
    return FVector(0, 0, 300);
}

void AWorldBootstrapper::TriggerLevelGeneration()
{
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> TestReq =
            FHttpModule::Get().CreateRequest();
    TestReq->SetURL(TEXT("https://httpbin.org/get"));
    TestReq->SetVerb(TEXT("GET"));
    TestReq->OnProcessRequestComplete().BindLambda(
        [](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            UE_LOG(LogTemp, Warning, TEXT("TEST HTTP: bOk=%d Code=%d"),
                bOk, Resp.IsValid() ? Resp->GetResponseCode() : -1);
        });
    TestReq->ProcessRequest();
    
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
