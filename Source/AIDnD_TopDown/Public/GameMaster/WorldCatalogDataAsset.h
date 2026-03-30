#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "WorldCatalogDataAsset.generated.h"

USTRUCT(BlueprintType)
struct FWorldCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
    TSoftClassPtr<AActor> ActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog")
    FName Tag;
};

UCLASS(BlueprintType)
class AIDND_TOPDOWN_API UWorldCatalogDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Environment")
    TMap<FString, FWorldCatalogEntry> EnvironmentActors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|NPCs")
    TMap<FString, FWorldCatalogEntry> NPCActors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Enemies")
    TMap<FString, FWorldCatalogEntry> EnemyActors;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Catalog|Managers")
    TMap<FString, FWorldCatalogEntry> ManagerActors;

    UFUNCTION(BlueprintPure, Category = "Catalog")
    bool FindEntry(const FString& AssetID, FWorldCatalogEntry& OutEntry) const
    {
        if (const FWorldCatalogEntry* Found = EnvironmentActors.Find(AssetID))
        { OutEntry = *Found; return true; }
        if (const FWorldCatalogEntry* Found = NPCActors.Find(AssetID))
        { OutEntry = *Found; return true; }
        if (const FWorldCatalogEntry* Found = EnemyActors.Find(AssetID))
        { OutEntry = *Found; return true; }
        if (const FWorldCatalogEntry* Found = ManagerActors.Find(AssetID))
        { OutEntry = *Found; return true; }
        return false;
    }

    UFUNCTION(BlueprintCallable, Category = "Catalog")
    TArray<FString> GetAllRegisteredIDs() const
    {
        TArray<FString> IDs;
        EnvironmentActors.GetKeys(IDs);
        TArray<FString> Temp;
        NPCActors.GetKeys(Temp);   IDs.Append(Temp);
        EnemyActors.GetKeys(Temp); IDs.Append(Temp);
        ManagerActors.GetKeys(Temp); IDs.Append(Temp);
        return IDs;
    }
};