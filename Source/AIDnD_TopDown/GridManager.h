// GridManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridManager.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCellClicked, class AGridCell*, ClickedCell);

UCLASS()
class AIDND_TOPDOWN_API AGridManager : public AActor
{
    GENERATED_BODY()
    
public:    
    AGridManager();

    // Grid settings
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GridWidth = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    int32 GridHeight = 20;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float CellSize = 100.0f; // Size in cm

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    TSubclassOf<class AGridCell> GridCellClass;

    // Grid storage
    UPROPERTY(BlueprintReadOnly, Category = "Grid")
    TArray<AGridCell*> GridCells;

    // Delegates
    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnCellClicked OnCellClicked;

    // Functions
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void GenerateGrid();

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void ClearGrid();

    UFUNCTION(BlueprintCallable, Category = "Grid")
    AGridCell* GetCellAtCoordinates(FIntPoint Coordinates) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    AGridCell* GetCellAtWorldLocation(FVector WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    FIntPoint WorldLocationToGrid(FVector WorldLocation) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    FVector GridToWorldLocation(FIntPoint GridCoordinates) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    TArray<AGridCell*> GetWalkableCellsInRange(FIntPoint StartCoordinates, float Range) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    TArray<AGridCell*> GetNeighborCells(FIntPoint Coordinates) const;

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void HighlightWalkableCells(FIntPoint StartCoordinates, float Range);

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void ClearAllHighlights();

    UFUNCTION(BlueprintCallable, Category = "Grid")
    float CalculateDistance(FIntPoint A, FIntPoint B) const;

protected:
    virtual void BeginPlay() override;

private:
    int32 CoordinatesToIndex(FIntPoint Coordinates) const;
    FIntPoint IndexToCoordinates(int32 Index) const;
    bool IsValidCoordinate(FIntPoint Coordinates) const;
};