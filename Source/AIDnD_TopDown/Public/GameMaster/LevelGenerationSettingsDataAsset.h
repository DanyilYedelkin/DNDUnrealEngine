#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LevelGenerationSettingsDataAsset.generated.h"

// ============================================================
//  IP cards — the main choice
// ============================================================

UENUM(BlueprintType)
enum class EMapType : uint8
{
    /**
    * Open terrain.
    * The GM places decorative objects: trees, rocks, decorative houses.
    * No construction — only prefab spawns.
     */
    OpenWorld  UMETA(DisplayName = "Open World (outdoor)"),

    /**
    * Dungeon / enclosed space.
    * The GM places wall blocks with position + scale.
    * One StaticMesh wall block is scaled and rotated
    * to form corridors and rooms.
     */
    Dungeon    UMETA(DisplayName = "Dungeon (indoor)"),
};

UENUM(BlueprintType)
enum class ELevelDifficulty : uint8
{
    Trivial UMETA(DisplayName = "Trivial"),
    Easy    UMETA(DisplayName = "Easy"),
    Medium  UMETA(DisplayName = "Medium"),
    Hard    UMETA(DisplayName = "Hard"),
    Deadly  UMETA(DisplayName = "Deadly"),
};

// ============================================================
//  DataAsset
// ============================================================

UCLASS(BlueprintType)
class AIDND_TOPDOWN_API ULevelGenerationSettingsDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:

    // ---- Map type ----

    /** open location or a dungeon */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Type")
    EMapType MapType = EMapType::Dungeon;
    
    /** Number of dungeon rooms to generate (player-configurable) */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Dungeon",
        meta = (ClampMin = 2, ClampMax = 8))
    int32 NumRooms = 3;

    // ---- Basic parameters ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Basic")
    FString LevelName = TEXT("Unnamed Level");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Basic")
    ELevelDifficulty Difficulty = ELevelDifficulty::Medium;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Basic")
    FString Atmosphere = TEXT("Mysterious and dangerous");

    // ---- World boundaries (for GM coordinate validation) ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Bounds")
    FVector WorldMin = FVector(-3000, -3000, 0);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Bounds")
    FVector WorldMax = FVector( 3000,  3000, 600);

    /**
    * NavMeshBoundsVolume centre.
    * Usually coincides with the level centre (0,0,0) or slightly above the floor.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|NavMesh")
    FVector NavMeshCenter = FVector(0, 0, 100);

    /**
    * Half size (extent) NavMeshBoundsVolume.
    * Must cover the entire game world + a small margin.
    * For dungeons: ~2000x2000x300
    * For open areas: ~3000x3000x200
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|NavMesh")
    FVector NavMeshExtent = FVector(2500, 2500, 300);

    // ---- Spawn limits ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Limits",
        meta = (ClampMin = 1, ClampMax = 50))
    int32 MaxNPCs = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Limits",
        meta = (ClampMin = 1, ClampMax = 60))
    int32 MaxEnemies = 15;

    /**
     * For Open World: decorative objects (trees, rocks, houses).
     * For Dungeon: wall blocks.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Limits",
        meta = (ClampMin = 1, ClampMax = 150))
    int32 MaxEnvironmentActors = 60;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Limits",
        meta = (ClampMin = 1, ClampMax = 50))
    int32 MaxActionsPerResponse = 40;

    // ---- Narrative ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Narrative",
        meta = (MultiLine = true))
    FString StartingQuestHint = TEXT("Survive and discover what lies ahead.");

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Narrative",
        meta = (MultiLine = true))
    FString ExtraGMInstructions;

    // ---- Seed ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Seed")
    int32 Seed = 0;

    // ---- Mandatory managers ----

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Level|Managers")
    TArray<FString> RequiredManagerIDs;

    // ---- Utilities ----

    UFUNCTION(BlueprintPure, Category = "Level")
    bool IsDungeon() const { return MapType == EMapType::Dungeon; }

    UFUNCTION(BlueprintPure, Category = "Level")
    bool IsOpenWorld() const { return MapType == EMapType::OpenWorld; }

    UFUNCTION(BlueprintPure, Category = "Level")
    FString GetMapTypeString() const
    {
        return (MapType == EMapType::Dungeon) ? TEXT("dungeon") : TEXT("open world");
    }

    UFUNCTION(BlueprintPure, Category = "Level")
    FString GetDifficultyString() const
    {
        switch (Difficulty)
        {
            case ELevelDifficulty::Trivial: return TEXT("trivial");
            case ELevelDifficulty::Easy:    return TEXT("easy");
            case ELevelDifficulty::Medium:  return TEXT("medium");
            case ELevelDifficulty::Hard:    return TEXT("hard");
            case ELevelDifficulty::Deadly:  return TEXT("deadly");
        }
        return TEXT("medium");
    }
};
