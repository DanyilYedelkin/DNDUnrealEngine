#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "ChatInteractable.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UChatInteractable : public UInterface
{
	GENERATED_BODY()
};

class AIDND_TOPDOWN_API IChatInteractable
{
	GENERATED_BODY()
public:
	// Вызывается когда игрок инициирует взаимодействие
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NPC Chat")
	void OnInteract(APawn* InstigatorPawn);

	// Вызывается при входе в зону
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NPC Chat")
	void OnPlayerEnterRange(APawn* PlayerPawn);

	// Вызывается при выходе из зоны
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="NPC Chat")
	void OnPlayerExitRange(APawn* PlayerPawn);
};