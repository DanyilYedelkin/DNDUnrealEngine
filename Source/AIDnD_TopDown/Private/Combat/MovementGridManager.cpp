// Source/AIDnD_TopDown/Private/Combat/MovementGridManager.cpp
#include "Combat/MovementGridManager.h"
#include "Combat/CombatCharacter.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/CombatLog.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "NavigationSystem.h"
#include "NavMesh/NavMeshPath.h"
#include "AI/NavigationSystemBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"

AMovementGridManager::AMovementGridManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // Root
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    // Movement tiles (Blue)
    MovementTileMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
        TEXT("MovementTiles"));
    MovementTileMesh->SetupAttachment(Root);
    MovementTileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MovementTileMesh->SetCastShadow(false);

    // Attack tiles (Red)
    AttackTileMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
        TEXT("AttackTiles"));
    AttackTileMesh->SetupAttachment(Root);
    AttackTileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    AttackTileMesh->SetCastShadow(false);

    // Spell tiles (Yellow)
    SpellTileMesh = CreateDefaultSubobject<UInstancedStaticMeshComponent>(
        TEXT("SpellTiles"));
    SpellTileMesh->SetupAttachment(Root);
    SpellTileMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    SpellTileMesh->SetCastShadow(false);
}

void AMovementGridManager::BeginPlay()
{
    Super::BeginPlay();

    // Apply materials if assigned in editor
    if (MovementMaterial && MovementTileMesh)
        MovementTileMesh->SetMaterial(0, MovementMaterial);

    if (AttackMaterial && AttackTileMesh)
        AttackTileMesh->SetMaterial(0, AttackMaterial);

    if (SpellMaterial && SpellTileMesh)
        SpellTileMesh->SetMaterial(0, SpellMaterial);
}

// ============================================================
//  SINGLETON
// ============================================================

AMovementGridManager* AMovementGridManager::GetGridManager(
    const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;
    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    AActor* Found = UGameplayStatics::GetActorOfClass(
        World, AMovementGridManager::StaticClass());
    return Cast<AMovementGridManager>(Found);
}

// ============================================================
//  MAIN API
// ============================================================

void AMovementGridManager::ShowMovementRange(ACombatCharacter* InCharacter)
{
    if (!InCharacter || !InCharacter->CombatStats) return;

    CurrentCharacter = InCharacter;
    ClearAllTiles();

    const float MaxFeet = InCharacter->CombatStats->MovementRemaining;
    const FVector Origin = InCharacter->GetActorLocation();

    CurrentReachableTiles = ComputeReachableTiles(Origin, MaxFeet);

    RebuildTileMeshes();

    UE_LOG(LogCombat, Log,
        TEXT("GridManager: Showing movement range for %s (%.0f feet, %d tiles)"),
        *InCharacter->CharacterName.ToString(),
        MaxFeet,
        CurrentReachableTiles.Num());
}

void AMovementGridManager::ShowAttackRange(ACombatCharacter* InCharacter,
    float RangeInFeet)
{
    if (!InCharacter) return;

    const FVector Origin = InCharacter->GetActorLocation();
    TArray<FVector> RangeTiles = ComputeRangeTiles(Origin, RangeInFeet);

    AttackTileMesh->ClearInstances();

    const FVector TileScale = FVector(TileSize / 100.f, TileSize / 100.f, 0.1f);

    for (const FVector& Loc : RangeTiles)
    {
        FTransform T;
        T.SetLocation(FVector(Loc.X, Loc.Y, Loc.Z + TileHeightOffset));
        T.SetScale3D(TileScale);
        AttackTileMesh->AddInstance(T);
    }

    UE_LOG(LogCombat, Verbose,
        TEXT("GridManager: Attack range shown (%d tiles, %.0f feet)"),
        RangeTiles.Num(), RangeInFeet);
}

void AMovementGridManager::ShowSpellRange(ACombatCharacter* InCharacter,
    float RangeInFeet)
{
    if (!InCharacter) return;

    const FVector Origin = InCharacter->GetActorLocation();
    TArray<FVector> RangeTiles = ComputeRangeTiles(Origin, RangeInFeet);

    SpellTileMesh->ClearInstances();

    const FVector TileScale = FVector(TileSize / 100.f, TileSize / 100.f, 0.1f);

    for (const FVector& Loc : RangeTiles)
    {
        FTransform T;
        T.SetLocation(FVector(Loc.X, Loc.Y, Loc.Z + TileHeightOffset));
        T.SetScale3D(TileScale);
        SpellTileMesh->AddInstance(T);
    }
}

void AMovementGridManager::HideMovementRange()
{
    ClearAllTiles();
    CurrentReachableTiles.Empty();
    CurrentCharacter = nullptr;

    UE_LOG(LogCombat, Verbose, TEXT("GridManager: Tiles hidden"));
}

TArray<FVector> AMovementGridManager::GetReachableTiles(
    ACombatCharacter* InCharacter)
{
    if (!InCharacter || !InCharacter->CombatStats)
        return {};

    const float MaxFeet = InCharacter->CombatStats->MovementRemaining;
    TArray<FGridTile> Tiles = ComputeReachableTiles(
        InCharacter->GetActorLocation(), MaxFeet);

    TArray<FVector> Result;
    Result.Reserve(Tiles.Num());
    for (const FGridTile& T : Tiles)
        Result.Add(T.WorldLocation);

    return Result;
}

float AMovementGridManager::GetPathCost(FVector From, FVector To)
{
    UNavigationSystemV1* NavSys =
        UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return -1.f;

    FPathFindingQuery Query;
    FNavAgentProperties AgentProps;

    const ANavigationData* NavData =
        NavSys->GetDefaultNavDataInstance(FNavigationSystem::DontCreate);
    if (!NavData) return -1.f;

    FVector NavFrom, NavTo;
    if (!ProjectToNavMesh(From, NavFrom)) return -1.f;
    if (!ProjectToNavMesh(To, NavTo))   return -1.f;

    Query = FPathFindingQuery(this, *NavData, NavFrom, NavTo);

    FPathFindingResult PathResult =
        NavSys->FindPathSync(AgentProps, Query);

    if (!PathResult.IsSuccessful()) return -1.f;

    // Path length in UU → feet
    const float LengthUU = PathResult.Path->GetLength();
    return UUToFeet(LengthUU);
}

bool AMovementGridManager::GetClosestReachableTile(FVector WorldPos,
    FVector& OutTileLocation)
{
    if (CurrentReachableTiles.IsEmpty()) return false;

    float BestDist = MAX_FLT;
    bool  bFound   = false;

    for (const FGridTile& Tile : CurrentReachableTiles)
    {
        const float Dist = FVector::Dist2D(WorldPos, Tile.WorldLocation);
        if (Dist < BestDist)
        {
            BestDist        = Dist;
            OutTileLocation = Tile.WorldLocation;
            bFound          = true;
        }
    }

    return bFound;
}

bool AMovementGridManager::IsLocationReachable(FVector WorldLocation) const
{
    for (const FGridTile& Tile : CurrentReachableTiles)
    {
        if (FVector::Dist2D(WorldLocation, Tile.WorldLocation) < TileSize * 0.5f)
            return true;
    }
    return false;
}

// ============================================================
//  PRIVATE — PATHFINDING
// ============================================================

TArray<FGridTile> AMovementGridManager::ComputeReachableTiles(
    FVector Origin, float MaxDistanceFeet)
{
    TArray<FGridTile> Result;

    UNavigationSystemV1* NavSys =
        UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return Result;

    const float MaxDistUU = FeetToUU(MaxDistanceFeet);
    const float StepUU    = TileSize;

    // Grid-based flood fill around origin
    // Range in tiles
    const int32 GridRadius = FMath::CeilToInt(MaxDistUU / StepUU);

    FVector NavOrigin;
    if (!ProjectToNavMesh(Origin, NavOrigin)) return Result;

    // Visited set to avoid duplicates
    TSet<FIntPoint> Visited;

    for (int32 X = -GridRadius; X <= GridRadius; ++X)
    {
        for (int32 Y = -GridRadius; Y <= GridRadius; ++Y)
        {
            const FIntPoint Key(X, Y);
            if (Visited.Contains(Key)) continue;
            Visited.Add(Key);

            const FVector CandidatePos(
                Origin.X + X * StepUU,
                Origin.Y + Y * StepUU,
                Origin.Z);

            // Project candidate onto NavMesh
            FVector NavPos;
            if (!ProjectToNavMesh(CandidatePos, NavPos)) continue;

            // Get actual path cost
            const float CostFeet = GetPathCost(NavOrigin, NavPos);
            if (CostFeet < 0.f || CostFeet > MaxDistanceFeet) continue;

            // Skip origin tile
            if (FVector::Dist(NavOrigin, NavPos) < StepUU * 0.1f) continue;

            FGridTile Tile;
            Tile.WorldLocation  = FVector(NavPos.X, NavPos.Y,
                                          NavPos.Z + TileHeightOffset);
            Tile.MovementCost   = CostFeet;
            Tile.HighlightType  = ETileHighlightType::Movement;
            Result.Add(Tile);
        }
    }

    UE_LOG(LogCombat, Verbose,
        TEXT("GridManager: ComputeReachableTiles → %d tiles (%.0f feet max)"),
        Result.Num(), MaxDistanceFeet);

    return Result;
}

TArray<FVector> AMovementGridManager::ComputeRangeTiles(
    FVector Origin, float RangeInFeet)
{
    TArray<FVector> Result;

    const float RangeUU    = FeetToUU(RangeInFeet);
    const float StepUU     = TileSize;
    const int32 GridRadius = FMath::CeilToInt(RangeUU / StepUU);

    for (int32 X = -GridRadius; X <= GridRadius; ++X)
    {
        for (int32 Y = -GridRadius; Y <= GridRadius; ++Y)
        {
            const FVector Candidate(
                Origin.X + X * StepUU,
                Origin.Y + Y * StepUU,
                Origin.Z);

            if (FVector::Dist2D(Origin, Candidate) > RangeUU) continue;
            if (FVector::Dist2D(Origin, Candidate) < StepUU * 0.1f) continue;

            FVector NavPos;
            if (!ProjectToNavMesh(Candidate, NavPos)) continue;

            Result.Add(NavPos);
        }
    }

    return Result;
}

bool AMovementGridManager::ProjectToNavMesh(FVector WorldPos,
    FVector& OutProjected) const
{
    UNavigationSystemV1* NavSys =
        UNavigationSystemV1::GetCurrent(GetWorld());
    if (!NavSys) return false;

    FNavLocation NavLocation;
    const bool bFound = NavSys->ProjectPointToNavigation(
        WorldPos, NavLocation, NavQueryExtent);

    if (bFound)
        OutProjected = NavLocation.Location;

    return bFound;
}

void AMovementGridManager::RebuildTileMeshes()
{
    MovementTileMesh->ClearInstances();

    const FVector TileScale = FVector(TileSize / 100.f, TileSize / 100.f, 0.1f);

    for (const FGridTile& Tile : CurrentReachableTiles)
    {
        FTransform T;
        T.SetLocation(Tile.WorldLocation);
        T.SetScale3D(TileScale);
        MovementTileMesh->AddInstance(T);
    }
}

void AMovementGridManager::ClearAllTiles()
{
    if (MovementTileMesh)
    {
        MovementTileMesh->ClearInstances();
    }
    if (AttackTileMesh)
    {
        AttackTileMesh->ClearInstances();
    }
    if (SpellTileMesh)
    {
        SpellTileMesh->ClearInstances();
    }
}
