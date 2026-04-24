#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "AI/ChatTypes.h"
#include "ChatWidget.generated.h"

class UScrollBox;
class UEditableTextBox;
class UButton;
class UTextBlock;

UCLASS(Abstract)
class AIDND_TOPDOWN_API UChatWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	// Вызвать из BP или C++ для инициализации
	UFUNCTION(BlueprintCallable, Category="Chat Widget")
	void InitWidget(class AChatNPC* NPC);

	UFUNCTION(BlueprintCallable, Category="Chat Widget")
	void AddMessage(const FString& SenderName, const FString& Text, bool bIsPlayer);

	UFUNCTION(BlueprintCallable, Category="Chat Widget")
	void ShowTypingIndicator(bool bShow);

	UFUNCTION(BlueprintCallable, Category="Chat Widget")
	void ClearHistory();

protected:
	virtual void NativeConstruct() override;

	// Bind в UMG Designer по имени (meta=BindWidget)
	UPROPERTY(meta=(BindWidget))
	UScrollBox* MessageScrollBox;

	UPROPERTY(meta=(BindWidget))
	UEditableTextBox* InputTextBox;

	UPROPERTY(meta=(BindWidget))
	UButton* SendButton;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* TypingIndicator;

	UPROPERTY(meta=(BindWidgetOptional))
	UTextBlock* NPCNameText;

	UFUNCTION()
	void OnSendClicked();

	UFUNCTION()
	void OnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UPROPERTY(BlueprintReadOnly, Category="Chat Widget")
	class AChatNPC* OwnerNPC = nullptr;
};