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

    // --- Locate Wall and Floor asset classes ---
    FWorldCatalogEntry WallEntry, FloorEntry;
    FString WallID, FloorID;

    TArray<FString> EnvIDs;
    WorldCatalog->EnvironmentActors.GetKeys(EnvIDs);
    if (EnvIDs.Num() == 0) return;

    for (const FString& ID : EnvIDs)
    {
        if (WallID.IsEmpty()  && ID.Contains(TEXT("Wall"),  ESearchCase::IgnoreCase)) WallID  = ID;
        if (FloorID.IsEmpty() && ID.Contains(TEXT("Floor"), ESearchCase::IgnoreCase)) FloorID = ID;
    }
    if (WallID.IsEmpty())  WallID  = EnvIDs[0];
    if (FloorID.IsEmpty()) FloorID = EnvIDs.Num() > 1 ? EnvIDs[1] : EnvIDs[0];

    WorldCatalog->FindEntry(WallID,  WallEntry);
    WorldCatalog->FindEntry(FloorID, FloorEntry);
    UClass* WC = WallEntry.ActorClass.LoadSynchronous();
    UClass* FC = FloorEntry.ActorClass.LoadSynchronous();
    if (!WC || !FC) return;

    // --- Generation parameters ---
    const int32 NumRooms = LevelSettings ? FMath::Max(2, LevelSettings->NumRooms) : 3;
    const int32 UsedSeed = (LevelSettings && LevelSettings->Seed != 0)
                           ? LevelSettings->Seed : FMath::Rand();
    FRandomStream Rng(UsedSeed);

    const float WT  = 25.f;
    const float WH  = 4.f;
    const float WCZ = 200.f;
    const float CW  = 400.f;
    const float CHW = CW * 0.5f;
    const float MinGap = CW + 100.f; // gap must be wider than corridor

    // --- Spawn helper ---
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    auto Spawn = [&](UClass* Class, FVector Loc, FVector Scale) -> AActor*
    {
        AActor* A = World->SpawnActor<AActor>(
            Class, FTransform(FRotator::ZeroRotator, Loc), SP);
        if (A)
        {
            A->SetActorScale3D(Scale);
            if (USceneComponent* R = A->GetRootComponent())
                R->SetWorldScale3D(Scale);
            A->Tags.Add(TEXT("GM_Spawned"));
        }
        return A;
    };

    // --- Room data ---
    struct FRoom
    {
        FVector Center;
        float   HW;
        float   HD;
        bool OpenN=false, OpenS=false, OpenE=false, OpenW=false;
    };

    TArray<FRoom> Rooms;

    // Returns true if Candidate overlaps any existing room (AABB + MinGap)
    auto RoomOverlaps = [&](const FRoom& Candidate) -> bool
    {
        for (const FRoom& E : Rooms)
        {
            float DX = FMath::Abs(Candidate.Center.X - E.Center.X);
            float DY = FMath::Abs(Candidate.Center.Y - E.Center.Y);
            if (DX < Candidate.HW + E.HW + MinGap &&
                DY < Candidate.HD + E.HD + MinGap)
                return true;
        }
        return false;
    };

    // Returns true if the corridor rectangle between Prev and Candidate
    // (given Dir) overlaps any existing room other than Prev.
    // Prev is Rooms.Last() so Candidate is not in Rooms yet.
    auto CorridorOverlaps = [&](const FRoom& Prev,
                                const FRoom& Candidate,
                                int32 Dir) -> bool
    {
        float CMinX, CMaxX, CMinY, CMaxY;

        switch (Dir)
        {
        case 0: // West: corridor spans from Candidate's east edge to Prev's west edge
            CMinX = Candidate.Center.X + Candidate.HW;
            CMaxX = Prev.Center.X - Prev.HW;
            CMinY = Prev.Center.Y - CHW;
            CMaxY = Prev.Center.Y + CHW;
            break;
        case 1: // South
            CMinX = Prev.Center.X - CHW;
            CMaxX = Prev.Center.X + CHW;
            CMinY = Candidate.Center.Y + Candidate.HD;
            CMaxY = Prev.Center.Y - Prev.HD;
            break;
        case 2: // East
            CMinX = Prev.Center.X + Prev.HW;
            CMaxX = Candidate.Center.X - Candidate.HW;
            CMinY = Prev.Center.Y - CHW;
            CMaxY = Prev.Center.Y + CHW;
            break;
        default: // North
            CMinX = Prev.Center.X - CHW;
            CMaxX = Prev.Center.X + CHW;
            CMinY = Prev.Center.Y + Prev.HD;
            CMaxY = Candidate.Center.Y - Candidate.HD;
            break;
        }

        if (CMinX >= CMaxX || CMinY >= CMaxY)
            return false; // degenerate corridor — fine

        for (int32 k = 0; k < Rooms.Num() - 1; ++k) // skip Rooms.Last() == Prev
        {
            const FRoom& R = Rooms[k];
            float RMinX = R.Center.X - R.HW;
            float RMaxX = R.Center.X + R.HW;
            float RMinY = R.Center.Y - R.HD;
            float RMaxY = R.Center.Y + R.HD;

            if (CMinX < RMaxX && CMaxX > RMinX &&
                CMinY < RMaxY && CMaxY > RMinY)
                return true;
        }
        return false;
    };

    // --- Place rooms ---
    {
        FRoom R0;
        R0.Center = FVector(0, 0, 0);
        R0.HW = Rng.RandRange(5, 8) * 100.f;
        R0.HD = Rng.RandRange(5, 8) * 100.f;
        Rooms.Add(R0);
    }

    for (int32 i = 1; i < NumRooms; ++i)
    {
        FRoom NewRoom;
        NewRoom.HW = Rng.RandRange(4, 9) * 100.f;
        NewRoom.HD = Rng.RandRange(4, 9) * 100.f;

        TArray<int32> Dirs = {0, 1, 2, 3};
        for (int32 d = 3; d > 0; --d)
            Dirs.Swap(d, Rng.RandRange(0, d));

        bool bPlaced = false;

        for (int32 Dir : Dirs)
        {
            for (int32 Mult = 1; Mult <= 6 && !bPlaced; ++Mult)
            {
                FRoom& Prev = Rooms.Last();
                float CorLen = Rng.RandRange(4, 8) * 100.f * Mult;

                FRoom Candidate = NewRoom;
                bool PW=false, PS=false, PE=false, PN=false;

                switch (Dir)
                {
                case 0:
                    Candidate.Center = FVector(
                        Prev.Center.X - Prev.HW - CorLen - NewRoom.HW,
                        Prev.Center.Y, 0);
                    PW = true; Candidate.OpenE = true; break;
                case 1:
                    Candidate.Center = FVector(
                        Prev.Center.X,
                        Prev.Center.Y - Prev.HD - CorLen - NewRoom.HD, 0);
                    PS = true; Candidate.OpenN = true; break;
                case 2:
                    Candidate.Center = FVector(
                        Prev.Center.X + Prev.HW + CorLen + NewRoom.HW,
                        Prev.Center.Y, 0);
                    PE = true; Candidate.OpenW = true; break;
                default:
                    Candidate.Center = FVector(
                        Prev.Center.X,
                        Prev.Center.Y + Prev.HD + CorLen + NewRoom.HD, 0);
                    PN = true; Candidate.OpenS = true; break;
                }

                // Reject if the room itself overlaps, OR if the corridor
                // to it would pass through any existing room
                if (RoomOverlaps(Candidate)) continue;
                if (CorridorOverlaps(Prev, Candidate, Dir)) continue;

                Prev.OpenW |= PW;
                Prev.OpenS |= PS;
                Prev.OpenE |= PE;
                Prev.OpenN |= PN;
                Rooms.Add(Candidate);
                bPlaced = true;
            }
            if (bPlaced) break;
        }

        if (!bPlaced)
            UE_LOG(LogTemp, Warning,
                TEXT("SpawnDungeonGeometry: could not place room %d without overlap."), i);
    }

    // --- Spawn room geometry ---
    auto SpawnSplitWall = [&](FVector Base, float HalfLen, bool bAlongX)
    {
        float SegLenUU = HalfLen - CHW;
        if (SegLenUU < 50.f) return;
        float SegScale = SegLenUU / 100.f;
        float Off = CHW + SegLenUU * 0.5f;

        if (bAlongX)
        {
            Spawn(WC, Base + FVector(-Off, 0, 0), FVector(SegScale, 0.5f, WH));
            Spawn(WC, Base + FVector(+Off, 0, 0), FVector(SegScale, 0.5f, WH));
        }
        else
        {
            Spawn(WC, Base + FVector(0, -Off, 0), FVector(0.5f, SegScale, WH));
            Spawn(WC, Base + FVector(0, +Off, 0), FVector(0.5f, SegScale, WH));
        }
    };

    for (const FRoom& R : Rooms)
    {
        float cx = R.Center.X, cy = R.Center.Y;
        float sx = R.HW * 2.f / 100.f;
        float sy = R.HD * 2.f / 100.f;

        Spawn(FC, FVector(cx, cy, 0), FVector(sx, sy, 0.5f));

        FVector NB(cx, cy + R.HD + WT, WCZ);
        if (!R.OpenN) Spawn(WC, NB, FVector(sx, 0.5f, WH));
        else          SpawnSplitWall(NB, R.HW, true);

        FVector SB(cx, cy - R.HD - WT, WCZ);
        if (!R.OpenS) Spawn(WC, SB, FVector(sx, 0.5f, WH));
        else          SpawnSplitWall(SB, R.HW, true);

        FVector EB(cx + R.HW + WT, cy, WCZ);
        if (!R.OpenE) Spawn(WC, EB, FVector(0.5f, sy, WH));
        else          SpawnSplitWall(EB, R.HD, false);

        FVector WB(cx - R.HW - WT, cy, WCZ);
        if (!R.OpenW) Spawn(WC, WB, FVector(0.5f, sy, WH));
        else          SpawnSplitWall(WB, R.HD, false);
    }

    // --- Spawn corridors ---
    for (int32 i = 1; i < Rooms.Num(); ++i)
    {
        const FRoom& A = Rooms[i-1];
        const FRoom& B = Rooms[i];

        bool bHoriz = FMath::Abs(B.Center.X - A.Center.X) >
                      FMath::Abs(B.Center.Y - A.Center.Y);

        if (bHoriz)
        {
            bool bBRight = B.Center.X > A.Center.X;
            float StartX = bBRight ? A.Center.X + A.HW : B.Center.X + B.HW;
            float EndX   = bBRight ? B.Center.X - B.HW : A.Center.X - A.HW;
            float Len = EndX - StartX;
            if (Len <= 0.f) continue;

            float CX = (StartX + EndX) * 0.5f;
            float CY = (A.Center.Y + B.Center.Y) * 0.5f;
            float SX = Len / 100.f;

            Spawn(FC, FVector(CX, CY, 0), FVector(SX, CW/100.f, 0.5f));

            float WallLen = Len - WT * 4.f;
            if (WallLen > 50.f)
            {
                float WSX = WallLen / 100.f;
                Spawn(WC, FVector(CX, CY+CHW+WT, WCZ), FVector(WSX, 0.5f, WH));
                Spawn(WC, FVector(CX, CY-CHW-WT, WCZ), FVector(WSX, 0.5f, WH));
            }
        }
        else
        {
            bool bBUp = B.Center.Y > A.Center.Y;
            float StartY = bBUp ? A.Center.Y + A.HD : B.Center.Y + B.HD;
            float EndY   = bBUp ? B.Center.Y - B.HD : A.Center.Y - A.HD;
            float Len = EndY - StartY;
            if (Len <= 0.f) continue;

            float CX = (A.Center.X + B.Center.X) * 0.5f;
            float CY = (StartY + EndY) * 0.5f;
            float SY = Len / 100.f;

            Spawn(FC, FVector(CX, CY, 0), FVector(CW/100.f, SY, 0.5f));

            float WallLen = Len - WT * 4.f;
            if (WallLen > 50.f)
            {
                float WSY = WallLen / 100.f;
                Spawn(WC, FVector(CX+CHW+WT, CY, WCZ), FVector(0.5f, WSY, WH));
                Spawn(WC, FVector(CX-CHW-WT, CY, WCZ), FVector(0.5f, WSY, WH));
            }
        }
    }

    // --- Spawn characters ---
    auto SpawnChar = [&](const FString& ID, FVector RoomCenter,
                         float OffX, float OffY) -> AActor*
    {
        FWorldCatalogEntry E;
        if (!WorldCatalog->FindEntry(ID, E)) return nullptr;
        UClass* Cls = E.ActorClass.LoadSynchronous();
        if (!Cls) return nullptr;
        FVector Loc = RoomCenter + FVector(OffX, OffY, 100.f);
        AActor* A = World->SpawnActor<AActor>(
            Cls, FTransform(FRotator::ZeroRotator, Loc), SP);
        if (A) A->Tags.Add(TEXT("GM_Spawned"));
        return A;
    };

    if (Rooms.Num() >= 2)
    {
        TArray<FString> NPCIDs;
        WorldCatalog->NPCActors.GetKeys(NPCIDs);
        int32 MaxNPCs = LevelSettings ? LevelSettings->MaxNPCs : 3;

        for (int32 n = 0; n < FMath::Min(NPCIDs.Num(), MaxNPCs); ++n)
            SpawnChar(NPCIDs[n], Rooms[1].Center,
                      Rng.FRandRange(-150.f, 150.f),
                      Rng.FRandRange(-150.f, 150.f));
    }

    {
        TArray<FString> EnemyIDs;
        WorldCatalog->EnemyActors.GetKeys(EnemyIDs);
        int32 MaxEnemies = LevelSettings ? LevelSettings->MaxEnemies : 6;
        int32 Spawned = 0;

        for (const FString& EID : EnemyIDs)
        {
            if (Spawned >= MaxEnemies) break;
            int32 Count = FMath::Min(Rng.RandRange(1, 3), MaxEnemies - Spawned);
            for (int32 k = 0; k < Count; ++k)
            {
                SpawnChar(EID, Rooms.Last().Center,
                          Rng.FRandRange(-200.f, 200.f),
                          Rng.FRandRange(-200.f, 200.f));
                ++Spawned;
            }
        }

        for (int32 r = 2; r < Rooms.Num()-1 && Spawned < MaxEnemies; ++r)
        {
            if (EnemyIDs.Num() == 0) break;
            SpawnChar(EnemyIDs[Rng.RandRange(0, EnemyIDs.Num()-1)],
                      Rooms[r].Center,
                      Rng.FRandRange(-150.f, 150.f),
                      Rng.FRandRange(-150.f, 150.f));
            ++Spawned;
        }
    }

    // --- Rebuild NavMesh after all geometry is placed ---
    FTimerHandle NavTimer;
    GetWorldTimerManager().SetTimer(NavTimer, [World]()
    {
        if (UNavigationSystemV1* Nav =
                FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
            Nav->Build();
    }, 0.5f, false);

    UE_LOG(LogTemp, Warning,
        TEXT("SpawnDungeonGeometry: %d rooms, seed=%d"),
        Rooms.Num(), UsedSeed);
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
