#include "AI/ChatWidget.h"
#include "AI/ChatNPC.h"
#include "AI/ChatManagerSubsystem.h"
#include "Components/ScrollBox.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UChatWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (SendButton)
        SendButton->OnClicked.AddDynamic(this, &UChatWidget::OnSendClicked);
    if (InputTextBox)
        InputTextBox->OnTextCommitted.AddDynamic(this, &UChatWidget::OnInputCommitted);
}

void UChatWidget::InitWidget(AChatNPC* NPC)
{
    OwnerNPC = NPC;
    if (NPCNameText && NPC)
        NPCNameText->SetText(FText::FromString(NPC->NPCDisplayName));

    // Загружаем историю из памяти
    if (NPC)
    {
        if (UChatManagerSubsystem* Sub = GetGameInstance()->
            GetSubsystem<UChatManagerSubsystem>())
        {
            TArray<FChatMessage> History = Sub->GetRecentMessages(NPC->NPCID);
            for (const FChatMessage& Msg : History)
            {
                bool bIsPlayer = (Msg.Role == EChatRole::User);
                FString Name = bIsPlayer ? TEXT("You") : NPC->NPCDisplayName;
                AddMessage(Name, Msg.Content, bIsPlayer);
            }
        }
    }
}

void UChatWidget::AddMessage(const FString& SenderName,
                              const FString& Text, bool bIsPlayer)
{
    if (!MessageScrollBox) return;
    
    UTextBlock* MsgText = NewObject<UTextBlock>(MessageScrollBox);
    
    FString Full = FString::Printf(TEXT("%s: %s"), *SenderName, *Text);
    MsgText->SetText(FText::FromString(Full));
    
    FSlateColor Color = bIsPlayer 
        ? FSlateColor(FLinearColor::White) 
        : FSlateColor(FLinearColor::Yellow);
    MsgText->SetColorAndOpacity(Color);
    MsgText->SetAutoWrapText(true);
    MessageScrollBox->AddChild(MsgText);
    MessageScrollBox->ScrollToEnd();
}

void UChatWidget::ClearHistory()
{
    if (MessageScrollBox)
    {
        MessageScrollBox->ClearChildren();
    }
}

void UChatWidget::ShowTypingIndicator(bool bShow)
{
    if (TypingIndicator)
        TypingIndicator->SetVisibility(bShow ? ESlateVisibility::Visible
                                             : ESlateVisibility::Collapsed);
}

void UChatWidget::OnSendClicked()
{
    if (!OwnerNPC || !InputTextBox) return;
    FString Text = InputTextBox->GetText().ToString().TrimStartAndEnd();
    if (Text.IsEmpty()) return;

    AddMessage(TEXT("You"), Text, true);
    ShowTypingIndicator(true);
    OwnerNPC->SendPlayerMessage(Text);
    InputTextBox->SetText(FText::GetEmpty());
}

void UChatWidget::OnInputCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
        OnSendClicked();
}