// Source/AIDnD_TopDown/Public/Combat/MovementGridManager.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Combat/CombatTypes.h"
#include "MovementGridManager.generated.h"

class ACombatCharacter;
class UInstancedStaticMeshComponent;

// ----------------------------------------------------------------
//  Tile visualization types
// ----------------------------------------------------------------

UENUM(BlueprintType)
enum class ETileHighlightType : uint8
{
    None        UMETA(DisplayName = "None"),
    Movement    UMETA(DisplayName = "Movement"),   // Blue
    Attack      UMETA(DisplayName = "Attack"),     // Red
    Spell       UMETA(DisplayName = "Spell")       // Yellow
};

USTRUCT(BlueprintType)
struct AIDND_TOPDOWN_API FGridTile
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Grid")
    FVector WorldLocation = FVector::ZeroVector;

    /** Cost in feet to reach this tile from the character's position */
    UPROPERTY(BlueprintReadOnly, Category = "Grid")
    float MovementCost = 0.f;

    UPROPERTY(BlueprintReadOnly, Category = "Grid")
    ETileHighlightType HighlightType = ETileHighlightType::None;

    /** True if a character is standing on this tile */
    UPROPERTY(BlueprintReadOnly, Category = "Grid")
    bool bOccupied = false;
};

// ----------------------------------------------------------------
//  AMovementGridManager
// ----------------------------------------------------------------

/**
 * Manages movement range visualization for turn-based combat.
 * Uses NavMesh for pathfinding and Instanced Static Meshes for tile rendering.
 *
 * Place one instance on the level. Call ShowMovementRange() at turn start,
 * HideMovementRange() after the character moves or turn ends.
 */
UCLASS(BlueprintType)
class AIDND_TOPDOWN_API AMovementGridManager : public AActor
{
    GENERATED_BODY()

public:

    AMovementGridManager();

    // ============================================================
    //  CONFIGURATION
    // ============================================================

    /** Size of each tile in UU (100 = 1 meter ≈ 3.3 feet) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Grid|Config",
        meta = (ClampMin = 10.f))
    float TileSize = 100.f;

    /** How high above the ground to place tile meshes */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Grid|Config")
    float TileHeightOffset = 5.f;

    /** NavMesh query extent (controls how far off-mesh we search) */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Grid|Config")
    FVector NavQueryExtent = FVector(50.f, 50.f, 100.f);

    // ---- Tile Materials ----

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Grid|Visuals")
    TObjectPtr<UMaterialInterface> MovementMaterial;   // Blue

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Grid|Visuals")
    TObjectPtr<UMaterialInterface> AttackMaterial;     // Red

    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Grid|Visuals")
    TObjectPtr<UMaterialInterface> SpellMaterial;      // Yellow

    // ============================================================
    //  SINGLETON ACCESS
    // ============================================================

    UFUNCTION(BlueprintCallable, BlueprintPure,
        Category = "Combat|Grid",
        meta = (WorldContext = "WorldContextObject"))
    static AMovementGridManager* GetGridManager(const UObject* WorldContextObject);

    // ============================================================
    //  MAIN API
    // ============================================================

    /**
     * Compute and display reachable tiles for a character.
     * Uses remaining movement from CombatStatsComponent.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Grid")
    void ShowMovementRange(ACombatCharacter* InCharacter);

    /**
     * Display attack range overlay on top of existing tiles.
     * Call after ShowMovementRange.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Grid")
    void ShowAttackRange(ACombatCharacter* InCharacter, float RangeInFeet);

    /**
     * Display spell range overlay.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Grid")
    void ShowSpellRange(ACombatCharacter* InCharacter, float RangeInFeet);

    /** Remove all tile highlights */
    UFUNCTION(BlueprintCallable, Category = "Combat|Grid")
    void HideMovementRange();

    /**
     * Returns all reachable tile locations for a character.
     * Useful for AI pathfinding decisions.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Grid")
    TArray<FVector> GetReachableTiles(ACombatCharacter* InCharacter);

    /**
     * Returns path cost in feet from one world location to another.
     * Returns -1 if no path exists.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Grid")
    float GetPathCost(FVector From, FVector To);

    /**
     * Returns the closest reachable tile to a world location.
     * Used to snap cursor position to valid move targets.
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Grid")
    bool GetClosestReachableTile(FVector WorldPos, FVector& OutTileLocation);

    /**
     * True if the given world location is within the current movement range.
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Grid")
    bool IsLocationReachable(FVector WorldLocation) const;

    // ============================================================
    //  CACHED STATE (read-only, for UI)
    // ============================================================

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Grid")
    TArray<FGridTile> CurrentReachableTiles;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Grid")
    TObjectPtr<ACombatCharacter> CurrentCharacter;

protected:

    virtual void BeginPlay() override;

private:

    // ---- Instanced mesh components (one per highlight type) ----
    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> MovementTileMesh;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> AttackTileMesh;

    UPROPERTY()
    TObjectPtr<UInstancedStaticMeshComponent> SpellTileMesh;

    /** Compute reachable tiles via NavMesh flood fill */
    TArray<FGridTile> ComputeReachableTiles(FVector Origin,
        float MaxDistanceFeet);

    /** Compute range tiles (ring or filled circle) */
    TArray<FVector> ComputeRangeTiles(FVector Origin, float RangeInFeet);

    /** Project a world location onto the NavMesh */
    bool ProjectToNavMesh(FVector WorldPos, FVector& OutProjected) const;

    /** Convert UU distance to feet (100 UU ≈ 1 meter ≈ 3.28 feet) */
    static float UUToFeet(float UU) { return UU / 30.48f; }

    /** Convert feet to UU */
    static float FeetToUU(float Feet) { return Feet * 30.48f; }

    /** Rebuild instanced meshes from CurrentReachableTiles */
    void RebuildTileMeshes();

    /** Clear all instanced mesh instances */
    void ClearAllTiles();
};