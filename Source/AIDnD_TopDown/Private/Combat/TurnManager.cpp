// Source/AIDnD_TopDown/Private/Combat/TurnManager.cpp
#include "Combat/TurnManager.h"
#include "Combat/DiceRoller.h"
#include "Combat/CombatStatsComponent.h"
#include "Combat/CombatLogger.h"
#include "Combat/CombatLog.h"
#include "Kismet/GameplayStatics.h"

ATurnManager::ATurnManager()
{
    PrimaryActorTick.bCanEverTick = false;
}

void ATurnManager::BeginPlay()
{
    Super::BeginPlay();
}

// ============================================================
//  SINGLETON
// ============================================================

ATurnManager* ATurnManager::GetTurnManager(const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    AActor* Found = UGameplayStatics::GetActorOfClass(World, ATurnManager::StaticClass());
    return Cast<ATurnManager>(Found);
}

// ============================================================
//  COMBAT LIFECYCLE
// ============================================================

void ATurnManager::StartCombat(
    const TArray<TScriptInterface<ICombatant>>& InCombatants)
{
    if (IsCombatActive())
    {
        UE_LOG(LogCombat, Warning,
            TEXT("TurnManager: StartCombat called while combat already active"));
        return;
    }

    Combatants = InCombatants;
    CurrentRound = 0;

    UE_LOG(LogCombat, Log,
        TEXT("TurnManager: Combat started with %d combatants"),
        Combatants.Num());

    // Log combat start
    if (UCombatLogger* Logger = UCombatLogger::GetCombatLogger(this))
    {
        TArray<FText> Names;
        for (const TScriptInterface<ICombatant>& C : Combatants)
        {
            if (C)
                Names.Add(ICombatant::Execute_GetCombatantName(C.GetObject()));
        }
        Logger->LogCombatStart(Names);
    }

    SetCombatState(ECombatState::RollingInit);
    RollInitiativeForAll();

    OnCombatStarted.Broadcast();

    BeginRound();
}

void ATurnManager::RegisterCombatant(TScriptInterface<ICombatant> Combatant)
{
    if (!Combatant) return;

    Combatants.AddUnique(Combatant);

    UCombatStatsComponent* Stats =
        ICombatant::Execute_GetCombatStats(Combatant.GetObject());

    FCombatantInitiative Entry;
    Entry.Combatant           = Combatant;
    Entry.CombatantName       = ICombatant::Execute_GetCombatantName(Combatant.GetObject());
    Entry.bIsPlayerControlled = ICombatant::Execute_IsPlayerControlled(Combatant.GetObject());
    Entry.DexModifier         = Stats
        ? Stats->GetAbilityModifier(EAbilityType::Dexterity) : 0;

    FDiceResult Roll = UDiceRoller::RollInitiativeFull(Entry.DexModifier);
    Entry.InitiativeValue = Roll.Total;
    Entry.NaturalRoll     = Roll.NaturalRoll;

    // Log initiative roll
    if (UCombatLogger* Logger = UCombatLogger::GetCombatLogger(this))
        Logger->LogInitiative(Entry.CombatantName, Roll, CurrentRound);

    // Insert sorted (descending)
    int32 InsertIdx = InitiativeOrder.Num();
    for (int32 i = 0; i < InitiativeOrder.Num(); ++i)
    {
        if (Entry.InitiativeValue > InitiativeOrder[i].InitiativeValue)
        {
            InsertIdx = i;
            break;
        }
    }
    InitiativeOrder.Insert(Entry, InsertIdx);

    OnInitiativeRolled.Broadcast(Entry);

    UE_LOG(LogCombat, Log,
        TEXT("TurnManager: %s joins combat (Initiative %d)"),
        *Entry.CombatantName.ToString(), Entry.InitiativeValue);
}

void ATurnManager::UnregisterCombatant(TScriptInterface<ICombatant> Combatant)
{
    if (!Combatant) return;

    Combatants.Remove(Combatant);

    const int32 RemovedIdx = InitiativeOrder.IndexOfByPredicate(
        [&Combatant](const FCombatantInitiative& E)
        { return E.Combatant == Combatant; });

    if (RemovedIdx != INDEX_NONE)
    {
        if (RemovedIdx < CurrentTurnIndex)
            CurrentTurnIndex--;
        else if (RemovedIdx == CurrentTurnIndex)
            CurrentTurnIndex--;

        InitiativeOrder.RemoveAt(RemovedIdx);
    }
}

void ATurnManager::RollInitiativeForAll()
{
    InitiativeOrder.Empty();

    for (TScriptInterface<ICombatant>& Combatant : Combatants)
    {
        if (!Combatant) continue;

        UCombatStatsComponent* Stats =
            ICombatant::Execute_GetCombatStats(Combatant.GetObject());

        FCombatantInitiative Entry;
        Entry.Combatant           = Combatant;
        Entry.CombatantName       = ICombatant::Execute_GetCombatantName(Combatant.GetObject());
        Entry.bIsPlayerControlled = ICombatant::Execute_IsPlayerControlled(Combatant.GetObject());
        Entry.DexModifier         = Stats
            ? Stats->GetAbilityModifier(EAbilityType::Dexterity) : 0;

        FDiceResult Roll = UDiceRoller::RollInitiativeFull(Entry.DexModifier);
        Entry.InitiativeValue = Roll.Total;
        Entry.NaturalRoll     = Roll.NaturalRoll;

        InitiativeOrder.Add(Entry);
        OnInitiativeRolled.Broadcast(Entry);

        // Log each initiative roll
        if (UCombatLogger* Logger = UCombatLogger::GetCombatLogger(this))
            Logger->LogInitiative(Entry.CombatantName, Roll, CurrentRound);

        UE_LOG(LogCombat, Log,
            TEXT("  Initiative: %s rolled d20(%d) + DEX(%d) = %d"),
            *Entry.CombatantName.ToString(),
            Entry.NaturalRoll,
            Entry.DexModifier,
            Entry.InitiativeValue);
    }

    // Sort descending
    InitiativeOrder.Sort([this](const FCombatantInitiative& A,
                                const FCombatantInitiative& B)
    {
        if (A.InitiativeValue != B.InitiativeValue)
            return A.InitiativeValue > B.InitiativeValue;
        return ResolveTiebreak(A, B);
    });

    UE_LOG(LogCombat, Log, TEXT("TurnManager: Initiative order set:"));
    for (int32 i = 0; i < InitiativeOrder.Num(); ++i)
    {
        UE_LOG(LogCombat, Log, TEXT("  %d. %s (%d)"),
            i + 1,
            *InitiativeOrder[i].CombatantName.ToString(),
            InitiativeOrder[i].InitiativeValue);
    }
}

void ATurnManager::EndCurrentTurn()
{
    if (!IsCombatActive()) return;
    if (InitiativeOrder.IsEmpty()) return;

    const FCombatantInitiative& Current = InitiativeOrder[CurrentTurnIndex];
    if (Current.Combatant)
        ICombatant::Execute_OnTurnEnd(Current.Combatant.GetObject());

    OnTurnEnded.Broadcast(Current);

    UE_LOG(LogCombat, Log,
        TEXT("TurnManager: %s ends their turn"),
        *Current.CombatantName.ToString());

    AdvanceToNextCombatant();
}

void ATurnManager::EndCombat(bool bPlayerVictory)
{
    SetCombatState(ECombatState::CombatEnd);

    UE_LOG(LogCombat, Log,
        TEXT("TurnManager: Combat ended — %s"),
        bPlayerVictory ? TEXT("Player Victory") : TEXT("Player Defeat"));

    // Log combat end
    if (UCombatLogger* Logger = UCombatLogger::GetCombatLogger(this))
        Logger->LogCombatEnd(bPlayerVictory);

    OnCombatEnded.Broadcast();

    CurrentRound     = 0;
    CurrentTurnIndex = 0;
    InitiativeOrder.Empty();
    Combatants.Empty();

    SetCombatState(ECombatState::Inactive);
}

void ATurnManager::CheckCombatEndCondition()
{
    if (!IsCombatActive()) return;

    bool bAnyPlayerAlive = false;
    bool bAnyEnemyAlive  = false;

    for (const FCombatantInitiative& Entry : InitiativeOrder)
    {
        if (!Entry.Combatant) continue;
        if (!ICombatant::Execute_IsAlive(Entry.Combatant.GetObject())) continue;

        if (Entry.bIsPlayerControlled)
            bAnyPlayerAlive = true;
        else
            bAnyEnemyAlive = true;
    }

    if (!bAnyPlayerAlive)
        EndCombat(false);
    else if (!bAnyEnemyAlive)
        EndCombat(true);
}

// ============================================================
//  QUERIES
// ============================================================

FCombatantInitiative ATurnManager::GetCurrentTurnCombatant() const
{
    if (InitiativeOrder.IsValidIndex(CurrentTurnIndex))
        return InitiativeOrder[CurrentTurnIndex];

    return FCombatantInitiative{};
}

bool ATurnManager::IsPlayerTurn() const
{
    if (!InitiativeOrder.IsValidIndex(CurrentTurnIndex)) return false;
    return InitiativeOrder[CurrentTurnIndex].bIsPlayerControlled;
}

// ============================================================
//  PRIVATE
// ============================================================

void ATurnManager::SetCombatState(ECombatState NewState)
{
    if (CombatState == NewState) return;
    CombatState = NewState;
    OnCombatStateChanged.Broadcast(NewState);
}

void ATurnManager::BeginRound()
{
    CurrentRound++;
    CurrentTurnIndex = 0;

    PruneDeadCombatants();

    UE_LOG(LogCombat, Log,
        TEXT("TurnManager: === Round %d begins ==="), CurrentRound);

    // Log round start
    if (UCombatLogger* Logger = UCombatLogger::GetCombatLogger(this))
        Logger->LogRoundStart(CurrentRound);

    OnRoundStarted.Broadcast(CurrentRound);

    if (!InitiativeOrder.IsEmpty())
        BeginTurn(0);
}

void ATurnManager::EndRound()
{
    UE_LOG(LogCombat, Log,
        TEXT("TurnManager: === Round %d ends ==="), CurrentRound);

    OnRoundEnded.Broadcast(CurrentRound);

    CheckCombatEndCondition();

    if (IsCombatActive())
        BeginRound();
}

void ATurnManager::BeginTurn(int32 TurnIndex)
{
    if (!InitiativeOrder.IsValidIndex(TurnIndex)) return;

    CurrentTurnIndex = TurnIndex;
    const FCombatantInitiative& Entry = InitiativeOrder[TurnIndex];

    // Skip dead combatants
    if (!Entry.Combatant ||
        !ICombatant::Execute_IsAlive(Entry.Combatant.GetObject()))
    {
        AdvanceToNextCombatant();
        return;
    }

    SetCombatState(Entry.bIsPlayerControlled
        ? ECombatState::PlayerTurn
        : ECombatState::EnemyTurn);

    // Log turn start
    if (UCombatLogger* Logger = UCombatLogger::GetCombatLogger(this))
        Logger->LogTurnStart(Entry.CombatantName, CurrentRound);

    ICombatant::Execute_OnTurnStart(Entry.Combatant.GetObject());
    OnTurnStarted.Broadcast(Entry);

    UE_LOG(LogCombat, Log,
        TEXT("TurnManager: %s begins their turn (Round %d)"),
        *Entry.CombatantName.ToString(), CurrentRound);
}

void ATurnManager::AdvanceToNextCombatant()
{
    const int32 NextIndex = CurrentTurnIndex + 1;

    if (NextIndex >= InitiativeOrder.Num())
        EndRound();
    else
        BeginTurn(NextIndex);
}

bool ATurnManager::ResolveTiebreak(const FCombatantInitiative& A,
                                    const FCombatantInitiative& B) const
{
    if (bPlayerWinsTiebreak)
    {
        if (A.bIsPlayerControlled && !B.bIsPlayerControlled) return true;
        if (!A.bIsPlayerControlled && B.bIsPlayerControlled) return false;
    }

    if (A.DexModifier != B.DexModifier)
        return A.DexModifier > B.DexModifier;

    return FMath::RandBool();
}

void ATurnManager::PruneDeadCombatants()
{
    InitiativeOrder.RemoveAll([](const FCombatantInitiative& E)
    {
        if (!E.Combatant) return true;
        return !ICombatant::Execute_IsAlive(E.Combatant.GetObject());
    });
}