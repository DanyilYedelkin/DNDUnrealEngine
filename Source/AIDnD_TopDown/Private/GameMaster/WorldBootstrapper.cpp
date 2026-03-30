// WorldBootstrapper.cpp
#include "GameMaster/WorldBootstrapper.h"

#include "HttpModule.h"
#include "NavigationSystem.h"
#include "GameMaster/GameMasterSubsystem.h"
#include "GameMaster/LevelGenerationSettingsDataAsset.h"
#include "GameMaster/WorldCatalogDataAsset.h"
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
    UE_LOG(LogTemp, Warning, TEXT("=== BOOTSTRAPPER: TriggerLevelGeneration ==="));

    if (!GMSubsystem) { SetupSubsystem(); if (!GMSubsystem) return; }
    
    if (LevelSettings && LevelSettings->IsDungeon())
    {
        SpawnDungeonGeometry();
    }

    BP_OnLevelGenerationStarted();
    GMSubsystem->GenerateLevel(GetWorld());
}

void AWorldBootstrapper::SpawnDungeonGeometry()
{
    if (!WorldCatalog) return;
    UWorld* World = GetWorld();
    if (!World) return;

    // ---- Ищем asset стены ----
    FWorldCatalogEntry WallEntry;
    FString WallID;
    TArray<FString> EnvIDs;
    WorldCatalog->EnvironmentActors.GetKeys(EnvIDs);
    if (EnvIDs.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("DungeonGeometry: No env assets!"));
        return;
    }
    // Ищем wall-asset (ключ содержит "Wall")
    for (const FString& ID : EnvIDs)
    {
        if (ID.Contains(TEXT("Wall"), ESearchCase::IgnoreCase))
        {
            WallID = ID;
            break;
        }
    }
    if (WallID.IsEmpty()) WallID = EnvIDs[0]; // fallback — первый asset
    WorldCatalog->FindEntry(WallID, WallEntry);
    UClass* WallClass = WallEntry.ActorClass.LoadSynchronous();

    // ---- Ищем asset пола ----
    FWorldCatalogEntry FloorEntry;
    FString FloorID;
    for (const FString& ID : EnvIDs)
    {
        if (ID.Contains(TEXT("Floor"), ESearchCase::IgnoreCase))
        {
            FloorID = ID;
            break;
        }
    }
    if (FloorID.IsEmpty()) FloorID = WallID; // fallback — тот же asset
    WorldCatalog->FindEntry(FloorID, FloorEntry);
    UClass* FloorClass = FloorEntry.ActorClass.LoadSynchronous();

    if (!WallClass || !FloorClass) return;

    // ---- Вспомогательная лямбда спавна ----
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    auto Spawn = [&](UClass* Class, FVector Loc, FVector Scale)
    {
        AActor* A = World->SpawnActor<AActor>(Class,
            FTransform(FRotator::ZeroRotator, Loc), Params);
        if (A)
        {
            A->SetActorScale3D(Scale);
            if (USceneComponent* Root = A->GetRootComponent())
                Root->SetWorldScale3D(Scale);
            A->Tags.Add(TEXT("GM_Spawned"));
            A->Tags.Add(TEXT("DungeonGeometry"));
        }
        return A;
    };

    // ================================================================
    // ROOM 1 — центр (0, 0), размер 14x14 (1400x1400 UU)
    // Края: X ±700, Y ±700
    // ================================================================

    // Пол
    Spawn(FloorClass, {0, 0, 0},        {14, 14, 0.5f});

    // Стены (толщина 0.5 = 50UU, высота 4 = 400UU, центр по Z = 200)
    Spawn(WallClass,  {0,    725,  200}, {14, 0.5f, 4}); // Север
    Spawn(WallClass,  {0,   -725,  200}, {14, 0.5f, 4}); // Юг
    Spawn(WallClass,  {725,  0,    200}, {0.5f, 14, 4}); // Восток

    // Запад — с проёмом для коридора (проём Y: -200 до +200, ширина 400)
    Spawn(WallClass, {-725,  450,  200}, {0.5f, 5, 4}); // Запад верх (Y: 200..700)
    Spawn(WallClass, {-725, -450,  200}, {0.5f, 5, 4}); // Запад низ  (Y:-700..-200)

    // ================================================================
    // КОРИДОР 1 — соединяет Room1(запад X=-700) с Room2(восток X=-1800)
    // Центр X = (-700 + -1800) / 2 = -1250
    // Длина = 1100 → scale X = 11
    // Ширина = 400 → scale Y = 4, стены на Y ±225
    // ================================================================

    Spawn(FloorClass, {-1250,  0,    0},  {11, 4, 0.5f}); // Пол
    Spawn(WallClass,  {-1250,  225,  200},{11, 0.5f, 4});  // Север
    Spawn(WallClass,  {-1250, -225,  200},{11, 0.5f, 4});  // Юг

    // ================================================================
    // ROOM 2 — центр (-2400, 0), размер 12x12 (1200x1200 UU)
    // Края: X: -1800 до -3000, Y: ±600
    // ================================================================

    Spawn(FloorClass, {-2400,  0,    0},  {12, 12, 0.5f});

    Spawn(WallClass,  {-2400,  625,  200},{12, 0.5f, 4}); // Север
    Spawn(WallClass,  {-3025,  0,    200},{0.5f, 12, 4}); // Запад

    // Восток с проёмом (X=-1800, проём Y: -200..+200)
    Spawn(WallClass,  {-1800,  450,  200},{0.5f, 4, 4}); // Вост верх (Y:200..600)
    Spawn(WallClass,  {-1800, -450,  200},{0.5f, 4, 4}); // Вост низ  (Y:-200..-600)

    // Юг с проёмом для коридора (Y=-625, проём X: -2600..-2200)
    // Левый кусок: X от -3000 до -2600 → длина 4, центр X=-2800
    Spawn(WallClass,  {-2800, -625,  200},{4, 0.5f, 4}); // Юг лево
    // Правый кусок: X от -2200 до -1800 → длина 4, центр X=-2000
    Spawn(WallClass,  {-2000, -625,  200},{4, 0.5f, 4}); // Юг право

    // ================================================================
    // КОРИДОР 2 — соединяет Room2(юг Y=-600) с Room3(север Y=-1600)
    // Центр Y = (-600 + -1600) / 2 = -1100
    // Длина = 1000 → scale Y = 10
    // Ширина = 400 → scale X = 4, стены на X ±225 от центра (-2400)
    // ================================================================

    Spawn(FloorClass, {-2400, -1100,  0},  {4, 10, 0.5f}); // Пол
    Spawn(WallClass,  {-2625, -1100,  200},{0.5f, 10, 4});  // Запад
    Spawn(WallClass,  {-2175, -1100,  200},{0.5f, 10, 4});  // Восток

    // ================================================================
    // ROOM 3 — центр (-2400, -2400), размер 16x16 (1600x1600 UU)
    // Края: X: -1600 до -3200, Y: -1600 до -3200
    // ================================================================

    Spawn(FloorClass, {-2400, -2400,  0},  {16, 16, 0.5f});

    Spawn(WallClass,  {-2400, -3225,  200},{16, 0.5f, 4}); // Юг
    Spawn(WallClass,  {-3225, -2400,  200},{0.5f, 16, 4}); // Запад
    Spawn(WallClass,  {-1575, -2400,  200},{0.5f, 16, 4}); // Восток

    // Север с проёмом (Y=-1600, проём X: -2600..-2200)
    // Левый кусок: X от -3200 до -2600 → длина 6, центр X=-2900
    Spawn(WallClass,  {-2900, -1625,  200},{6, 0.5f, 4}); // Север лево
    // Правый кусок: X от -2200 до -1600 → длина 6, центр X=-1900
    Spawn(WallClass,  {-1900, -1625,  200},{6, 0.5f, 4}); // Север право

    UE_LOG(LogTemp, Warning,
        TEXT("DungeonGeometry: Done. Wall='%s' Floor='%s'"),
        *WallID, *FloorID);

    // Пересобрать NavMesh после спавна геометрии
    UNavigationSystemV1* NavSys =
        FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    if (NavSys)
    {
        NavSys->Build();
        UE_LOG(LogTemp, Warning, TEXT("DungeonGeometry: NavMesh rebuild triggered"));
    }
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
