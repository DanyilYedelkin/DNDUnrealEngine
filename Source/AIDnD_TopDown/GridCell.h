// GridCell.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GridCell.generated.h"

UENUM(BlueprintType)
enum class ECellState : uint8
{
    Normal UMETA(DisplayName = "Normal"),
    Highlighted UMETA(DisplayName = "Highlighted"),
    Walkable UMETA(DisplayName = "Walkable"),
    Attackable UMETA(DisplayName = "Attackable"),
    Occupied UMETA(DisplayName = "Occupied"),
    Blocked UMETA(DisplayName = "Blocked")
};

UCLASS()
class AIDND_TOPDOWN_API AGridCell : public AActor
{
    GENERATED_BODY()
    
public:    
    AGridCell();

    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    class UStaticMeshComponent* CellMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Grid")
    class UDecalComponent* CellDecal;

    // Grid coordinates
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    FIntPoint GridCoordinates;

    // Cell state
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    ECellState CellState;

    // Cell properties
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    bool bIsWalkable = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    bool bIsOccupied = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Grid")
    float MovementCost = 1.0f;

    // Occupying character
    UPROPERTY(BlueprintReadWrite, Category = "Grid")
    class ACombatCharacter* OccupyingCharacter;

    // Functions
    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SetCellState(ECellState NewState);

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void HighlightCell(bool bHighlight, FLinearColor Color = FLinearColor::Green);

    UFUNCTION(BlueprintCallable, Category = "Grid")
    void SetOccupied(bool bOccupied, ACombatCharacter* Character = nullptr);

    UFUNCTION(BlueprintCallable, Category = "Grid")
    bool CanBeWalkedOn() const;

protected:
    virtual void BeginPlay() override;

private:
    void UpdateCellVisual();
    
    // Materials for different states
    UPROPERTY()
    class UMaterialInstanceDynamic* DynamicMaterial;
};