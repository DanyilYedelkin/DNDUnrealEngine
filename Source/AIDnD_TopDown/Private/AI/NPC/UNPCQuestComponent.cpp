#include "AI/NPC/UNPCQuestComponent.h"
#include "AI/NPC/UQuestDataAsset.h"
#include "AI/NPC/UTrustComponent.h"
#include "AI/NPC/UNPCRelationsSubsystem.h"
#include "AI/ChatNPC.h"

UNPCQuestComponent::UNPCQuestComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UNPCQuestComponent::BeginPlay()
{
    Super::BeginPlay();

    if (AChatNPC* Owner = Cast<AChatNPC>(GetOwner()))
        OwnerNPCID = Owner->NPCID;

    if (UGameInstance* GI = GetWorld()->GetGameInstance())
        RelationsSubsystem = GI->GetSubsystem<UNPCRelationsSubsystem>();
}

TArray<UQuestDataAsset*> UNPCQuestComponent::GetOfferedQuests() const
{
    TArray<UQuestDataAsset*> Result;
    UTrustComponent* TC = GetTrustComp();

    for (const auto& QuestRef : AvailableQuests)
    {
        UQuestDataAsset* Quest = ResolveQuest(QuestRef);
        if (!Quest) continue;

        ENPCQuestState State = GetQuestState(Quest->QuestID);
        
        if (State == ENPCQuestState::Active) continue;
        if (State == ENPCQuestState::Completed && !Quest->bIsRepeatable) continue;
        if (State == ENPCQuestState::Failed && !Quest->bIsRepeatable) continue;
        
        if (TC && !TC->MeetsTrustRequirement(Quest->RequiredTrust)) continue;

        Result.Add(Quest);
    }
    return Result;
}

bool UNPCQuestComponent::AcceptQuest(const FString& QuestID, FString& OutFailReason)
{
    UTrustComponent* TC = GetTrustComp();
    
    UQuestDataAsset* Quest = nullptr;
    for (const auto& Ref : AvailableQuests)
    {
        UQuestDataAsset* Q = ResolveQuest(Ref);
        if (Q && Q->QuestID == QuestID) { Quest = Q; break; }
    }

    if (!Quest)
    {
        OutFailReason = TEXT("Quest not found.");
        return false;
    }

    ENPCQuestState State = GetQuestState(QuestID);
    if (State == ENPCQuestState::Active)
    {
        OutFailReason = TEXT("Quest is already active.");
        return false;
    }

    if (TC && !TC->MeetsTrustRequirement(Quest->RequiredTrust))
    {
        OutFailReason = FString::Printf(
            TEXT("Need %d trust to accept this quest. You have %d."),
            Quest->RequiredTrust, TC->GetTrust());
        return false;
    }

    if (RelationsSubsystem)
        RelationsSubsystem->SetQuestState(OwnerNPCID, QuestID,
            ENPCQuestState::Active);

    if (TC)
    {
        TC->ModifyTrust(2, ENPCMemoryEventType::QuestAccepted,
            FString::Printf(TEXT("Accepted quest: %s"), *Quest->Title.ToString()));
    }

    OnQuestStateChanged.Broadcast(QuestID, ENPCQuestState::Active);
    return true;
}

void UNPCQuestComponent::CompleteQuestStep(const FString& QuestID,
                                            const FString& StepID)
{
    if (!RelationsSubsystem) return;
    RelationsSubsystem->MarkQuestStepComplete(OwnerNPCID, QuestID, StepID);
}

bool UNPCQuestComponent::TurnInQuest(const FString& QuestID, FString& OutFailReason)
{
    if (GetQuestState(QuestID) != ENPCQuestState::Active)
    {
        OutFailReason = TEXT("Quest is not active.");
        return false;
    }

    UQuestDataAsset* Quest = nullptr;
    for (const auto& Ref : AvailableQuests)
    {
        UQuestDataAsset* Q = ResolveQuest(Ref);
        if (Q && Q->QuestID == QuestID) { Quest = Q; break; }
    }

    if (!Quest)
    {
        OutFailReason = TEXT("Quest data not found.");
        return false;
    }

    if (!AreAllStepsComplete(Quest))
    {
        OutFailReason = TEXT("Not all quest steps are complete.");
        return false;
    }

    if (RelationsSubsystem)
        RelationsSubsystem->SetQuestState(OwnerNPCID, QuestID,
            ENPCQuestState::Completed);

    if (UTrustComponent* TC = GetTrustComp())
    {
        TC->ModifyTrust(Quest->TrustRewardOnComplete,
            ENPCMemoryEventType::QuestCompleted,
            FString::Printf(TEXT("Completed quest: %s"), *Quest->Title.ToString()));
    }

    OnQuestStateChanged.Broadcast(QuestID, ENPCQuestState::Completed);
    return true;
}

void UNPCQuestComponent::FailQuest(const FString& QuestID)
{
    if (!RelationsSubsystem) return;
    RelationsSubsystem->SetQuestState(OwnerNPCID, QuestID, ENPCQuestState::Failed);
    
    for (const auto& Ref : AvailableQuests)
    {
        UQuestDataAsset* Q = ResolveQuest(Ref);
        if (Q && Q->QuestID == QuestID)
        {
            if (UTrustComponent* TC = GetTrustComp())
            {
                TC->ModifyTrust(Q->TrustPenaltyOnFail,
                    ENPCMemoryEventType::QuestFailed,
                    FString::Printf(TEXT("Failed quest: %s"), *Q->Title.ToString()));
            }
            break;
        }
    }

    OnQuestStateChanged.Broadcast(QuestID, ENPCQuestState::Failed);
}

ENPCQuestState UNPCQuestComponent::GetQuestState(const FString& QuestID) const
{
    if (!RelationsSubsystem) return ENPCQuestState::Available;
    return RelationsSubsystem->GetQuestState(OwnerNPCID, QuestID);
}

bool UNPCQuestComponent::HasActiveQuests() const
{
    for (const auto& Ref : AvailableQuests)
    {
        UQuestDataAsset* Q = ResolveQuest(Ref);
        if (Q && GetQuestState(Q->QuestID) == ENPCQuestState::Active)
            return true;
    }
    return false;
}

FString UNPCQuestComponent::BuildQuestContextString() const
{
    FString Result;

    for (const auto& Ref : AvailableQuests)
    {
        UQuestDataAsset* Q = ResolveQuest(Ref);
        if (!Q) continue;

        ENPCQuestState State = GetQuestState(Q->QuestID);
        if (State == ENPCQuestState::Active)
        {
            Result += FString::Printf(
                TEXT("[Active quest you gave to player]: %s — %s\n"),
                *Q->Title.ToString(), *Q->Description.ToString());
        }
    }
    return Result;
}

bool UNPCQuestComponent::AreAllStepsComplete(const UQuestDataAsset* Quest) const
{
    if (!RelationsSubsystem) return false;
    for (const FNPCQuestStep& Step : Quest->Steps)
    {
        if (!RelationsSubsystem->IsQuestStepComplete(
            OwnerNPCID, Quest->QuestID, Step.StepID))
            return false;
    }
    return true;
}

UTrustComponent* UNPCQuestComponent::GetTrustComp() const
{
    if (AActor* Owner = GetOwner())
        return Owner->FindComponentByClass<UTrustComponent>();
    return nullptr;
}

UQuestDataAsset* UNPCQuestComponent::ResolveQuest(
    const TSoftObjectPtr<UQuestDataAsset>& Ref) const
{
    if (Ref.IsValid()) return Ref.Get();
    return Ref.LoadSynchronous();
}