// GridCell.cpp
#include "GridCell.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "CombatCharacter.h"
#include "Materials/MaterialInstanceDynamic.h"

AGridCell::AGridCell()
{
    PrimaryActorTick.bCanEverTick = false;

    // Create root component
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    // Create static mesh for cell (plane)
    CellMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CellMesh"));
    CellMesh->SetupAttachment(RootComponent);
    CellMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CellMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    CellMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

    // Create decal for highlighting
    CellDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("CellDecal"));
    CellDecal->SetupAttachment(RootComponent);
    CellDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
    CellDecal->DecalSize = FVector(50.0f, 50.0f, 50.0f);
    CellDecal->SetVisibility(false);

    CellState = ECellState::Normal;
}

void AGridCell::BeginPlay()
{
    Super::BeginPlay();
    
    // Create dynamic material instance if mesh has material
    if (CellMesh && CellMesh->GetMaterial(0))
    {
        DynamicMaterial = CellMesh->CreateDynamicMaterialInstance(0);
    }
    
    UpdateCellVisual();
}

void AGridCell::SetCellState(ECellState NewState)
{
    CellState = NewState;
    UpdateCellVisual();
}

void AGridCell::HighlightCell(bool bHighlight, FLinearColor Color)
{
    if (CellDecal)
    {
        CellDecal->SetVisibility(bHighlight);
        
        // You can set decal color here if your material supports it
    }
}

void AGridCell::SetOccupied(bool bOccupied, ACombatCharacter* Character)
{
    bIsOccupied = bOccupied;
    OccupyingCharacter = Character;
    
    if (bIsOccupied)
    {
        CellState = ECellState::Occupied;
    }
    else
    {
        CellState = ECellState::Normal;
    }
    
    UpdateCellVisual();
}

bool AGridCell::CanBeWalkedOn() const
{
    return bIsWalkable && !bIsOccupied && CellState != ECellState::Blocked;
}

void AGridCell::UpdateCellVisual()
{
    // Update material color based on cell state
    if (DynamicMaterial)
    {
        FLinearColor Color;
        
        switch (CellState)
        {
            case ECellState::Normal:
                Color = FLinearColor(0.2f, 0.2f, 0.2f, 0.5f);
                break;
            case ECellState::Highlighted:
                Color = FLinearColor::Yellow;
                break;
            case ECellState::Walkable:
                Color = FLinearColor::Green;
                break;
            case ECellState::Attackable:
                Color = FLinearColor::Red;
                break;
            case ECellState::Occupied:
                Color = FLinearColor(0.5f, 0.5f, 0.5f, 0.8f);
                break;
            case ECellState::Blocked:
                Color = FLinearColor::Black;
                break;
        }
        
        DynamicMaterial->SetVectorParameterValue(FName("Color"), Color);
    }
}