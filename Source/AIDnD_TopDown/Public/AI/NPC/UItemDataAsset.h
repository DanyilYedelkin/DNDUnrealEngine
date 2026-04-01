// Source/AIDnD_TopDown/Public/NPC/UItemDataAsset.h
#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AI/NPC/NPCTypes.h"
#include "UItemDataAsset.generated.h"

UCLASS(BlueprintType)
class AIDND_TOPDOWN_API UItemDataAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:

	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId(TEXT("Item"), GetFName());
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Identity")
	FString ItemID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Identity",
			  meta=(MultiLine=true))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Identity")
	UTexture2D* Icon = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Commerce")
	ENPCItemCategory Category = ENPCItemCategory::Misc;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Commerce",
			  meta=(ClampMin=0))
	int32 BasePrice = 10;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Commerce",
			  meta=(ClampMin=-100, ClampMax=100))
	int32 RequiredTrust = -100;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Commerce")
	bool bIsUnique = false;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|Stats")
	TArray<FItemStatModifier> StatModifiers;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Item|World")
	TSoftClassPtr<AActor> WorldActorClass;
};