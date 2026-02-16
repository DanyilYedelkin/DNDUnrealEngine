// GridManager.cpp
#include "GridManager.h"
#include "GridCell.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

AGridManager::AGridManager()
{
    PrimaryActorTick.bCanEverTick = false;
    
    // Set default grid cell class
    // Will be set in Blueprint
}

void AGridManager::BeginPlay()
{
    Super::BeginPlay();
}

void AGridManager::GenerateGrid()
{
    // Clear existing grid
    ClearGrid();

    if (!GridCellClass)
    {
        UE_LOG(LogTemp, Error, TEXT("GridManager: GridCellClass is not set!"));
        return;
    }

    // Generate grid cells
    for (int32 Y = 0; Y < GridHeight; Y++)
    {
        for (int32 X = 0; X < GridWidth; X++)
        {
            FIntPoint Coordinates(X, Y);
            FVector SpawnLocation = GridToWorldLocation(Coordinates);
            
            FActorSpawnParameters SpawnParams;
            SpawnParams.Owner = this;
            
            AGridCell* NewCell = GetWorld()->SpawnActor<AGridCell>(
                GridCellClass,
                SpawnLocation,
                FRotator::ZeroRotator,
                SpawnParams
            );

            if (NewCell)
            {
                NewCell->GridCoordinates = Coordinates;
                GridCells.Add(NewCell);
            }
        }
    }

    UE_LOG(LogTemp, Log, TEXT("GridManager: Generated %d cells"), GridCells.Num());
}

void AGridManager::ClearGrid()
{
    for (AGridCell* Cell : GridCells)
    {
        if (Cell)
        {
            Cell->Destroy();
        }
    }
    
    GridCells.Empty();
}

AGridCell* AGridManager::GetCellAtCoordinates(FIntPoint Coordinates) const
{
    if (!IsValidCoordinate(Coordinates))
    {
        return nullptr;
    }

    int32 Index = CoordinatesToIndex(Coordinates);
    
    if (GridCells.IsValidIndex(Index))
    {
        return GridCells[Index];
    }

    return nullptr;
}

AGridCell* AGridManager::GetCellAtWorldLocation(FVector WorldLocation) const
{
    FIntPoint GridCoords = WorldLocationToGrid(WorldLocation);
    return GetCellAtCoordinates(GridCoords);
}

FIntPoint AGridManager::WorldLocationToGrid(FVector WorldLocation) const
{
    FVector LocalLocation = WorldLocation - GetActorLocation();
    
    int32 X = FMath::RoundToInt(LocalLocation.X / CellSize);
    int32 Y = FMath::RoundToInt(LocalLocation.Y / CellSize);
    
    return FIntPoint(X, Y);
}

FVector AGridManager::GridToWorldLocation(FIntPoint GridCoordinates) const
{
    FVector GridOrigin = GetActorLocation();
    
    float WorldX = GridOrigin.X + (GridCoordinates.X * CellSize);
    float WorldY = GridOrigin.Y + (GridCoordinates.Y * CellSize);
    float WorldZ = GridOrigin.Z;
    
    return FVector(WorldX, WorldY, WorldZ);
}

TArray<AGridCell*> AGridManager::GetWalkableCellsInRange(FIntPoint StartCoordinates, float Range) const
{
    TArray<AGridCell*> WalkableCells;

    for (AGridCell* Cell : GridCells)
    {
        if (Cell && Cell->CanBeWalkedOn())
        {
            float Distance = CalculateDistance(StartCoordinates, Cell->GridCoordinates);
            
            if (Distance <= Range)
            {
                WalkableCells.Add(Cell);
            }
        }
    }

    return WalkableCells;
}

TArray<AGridCell*> AGridManager::GetNeighborCells(FIntPoint Coordinates) const
{
    TArray<AGridCell*> Neighbors;

    // 4-directional neighbors (up, down, left, right)
    TArray<FIntPoint> Directions = {
        FIntPoint(0, 1),   // Up
        FIntPoint(0, -1),  // Down
        FIntPoint(1, 0),   // Right
        FIntPoint(-1, 0)   // Left
    };

    for (const FIntPoint& Dir : Directions)
    {
        FIntPoint NeighborCoord = Coordinates + Dir;
        AGridCell* Neighbor = GetCellAtCoordinates(NeighborCoord);
        
        if (Neighbor)
        {
            Neighbors.Add(Neighbor);
        }
    }

    return Neighbors;
}

void AGridManager::HighlightWalkableCells(FIntPoint StartCoordinates, float Range)
{
    ClearAllHighlights();

    TArray<AGridCell*> WalkableCells = GetWalkableCellsInRange(StartCoordinates, Range);

    for (AGridCell* Cell : WalkableCells)
    {
        if (Cell)
        {
            Cell->HighlightCell(true, FLinearColor::Green);
        }
    }
}

void AGridManager::ClearAllHighlights()
{
    for (AGridCell* Cell : GridCells)
    {
        if (Cell)
        {
            Cell->HighlightCell(false);
        }
    }
}

float AGridManager::CalculateDistance(FIntPoint A, FIntPoint B) const
{
    // Manhattan distance (grid-based)
    return FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y);
}

int32 AGridManager::CoordinatesToIndex(FIntPoint Coordinates) const
{
    return Coordinates.Y * GridWidth + Coordinates.X;
}

FIntPoint AGridManager::IndexToCoordinates(int32 Index) const
{
    int32 X = Index % GridWidth;
    int32 Y = Index / GridWidth;
    return FIntPoint(X, Y);
}

bool AGridManager::IsValidCoordinate(FIntPoint Coordinates) const
{
    return Coordinates.X >= 0 && Coordinates.X < GridWidth &&
           Coordinates.Y >= 0 && Coordinates.Y < GridHeight;
}