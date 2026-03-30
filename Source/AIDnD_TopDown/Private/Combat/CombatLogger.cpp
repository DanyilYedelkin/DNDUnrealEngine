// Source/AIDnD_TopDown/Private/Combat/CombatLogger.cpp
#include "Combat/CombatLogger.h"
#include "Combat/CombatLog.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

// ============================================================
//  SUBSYSTEM LIFECYCLE
// ============================================================

void UCombatLogger::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    UE_LOG(LogCombat, Log, TEXT("CombatLogger: Initialized"));
}

void UCombatLogger::Deinitialize()
{
    LogEntries.Empty();
    Super::Deinitialize();
}

// ============================================================
//  STATIC ACCESS
// ============================================================

UCombatLogger* UCombatLogger::GetCombatLogger(
    const UObject* WorldContextObject)
{
    if (!WorldContextObject) return nullptr;

    UWorld* World = WorldContextObject->GetWorld();
    if (!World) return nullptr;

    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return nullptr;

    return GI->GetSubsystem<UCombatLogger>();
}

// ============================================================
//  CORE ADD
// ============================================================

void UCombatLogger::AddEntry(const FCombatLogEntry& Entry)
{
    LogEntries.Add(Entry);
    TrimIfNeeded();

    // Echo to UE log
    UE_LOG(LogCombat, Log, TEXT("[Round %d] %s"),
        Entry.Round, *Entry.Message.ToString());

    OnNewLogEntry.Broadcast(Entry);
}

void UCombatLogger::LogMessage(const FString& Message,
    ECombatLogEventType EventType, int32 Round)
{
    FCombatLogEntry Entry = MakeEntry(EventType, Round);
    Entry.Message = FText::FromString(Message);
    AddEntry(Entry);
}

// ============================================================
//  FORMATTED HELPERS
// ============================================================

void UCombatLogger::LogAttack(const FText& SourceName,
    const FText& TargetName, const FAttackResult& AttackResult, int32 Round)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::Attack, Round);
    Entry.SourceName  = SourceName;
    Entry.TargetName  = TargetName;
    Entry.PrimaryRoll = AttackResult.AttackRoll;

    // BG3-style format:
    // "Tavita attacks Goblin: d20(14)+3=17 vs AC 13 — Hit!"
    FString ResultStr;
    if (AttackResult.bCriticalHit)
        ResultStr = TEXT("CRITICAL HIT!");
    else if (AttackResult.bCriticalMiss)
        ResultStr = TEXT("Critical Miss!");
    else if (AttackResult.bHit)
        ResultStr = TEXT("Hit!");
    else
        ResultStr = TEXT("Miss!");

    const FString Msg = FString::Printf(
        TEXT("%s attacks %s: d20(%d)%s%d=%d vs AC %d — %s"),
        *SourceName.ToString(),
        *TargetName.ToString(),
        AttackResult.AttackRoll.NaturalRoll,
        AttackResult.AttackBonus >= 0 ? TEXT("+") : TEXT(""),
        AttackResult.AttackBonus,
        AttackResult.FinalAttackValue,
        AttackResult.TargetAC,
        *ResultStr);

    Entry.Message = FText::FromString(Msg);
    AddEntry(Entry);
}

void UCombatLogger::LogDamage(const FText& TargetName, int32 DamageAmount,
    EDamageType DamageType, const FDiceResult& DiceRoll,
    int32 RemainingHP, int32 Round)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::Damage, Round);
    Entry.TargetName    = TargetName;
    Entry.PrimaryRoll   = DiceRoll;
    Entry.Value         = DamageAmount;

    // Format dice rolls array: [3, 4, 2] → "3+4+2"
    FString DiceStr;
    for (int32 i = 0; i < DiceRoll.Rolls.Num(); ++i)
    {
        if (i > 0) DiceStr += TEXT("+");
        DiceStr += FString::FromInt(DiceRoll.Rolls[i]);
    }

    const FString Msg = FString::Printf(
        TEXT("%s takes %d %s damage (%s) — %d HP remaining"),
        *TargetName.ToString(),
        DamageAmount,
        *GetDamageTypeName(DamageType),
        *DiceStr,
        RemainingHP);

    Entry.Message = FText::FromString(Msg);
    AddEntry(Entry);
}

void UCombatLogger::LogSavingThrow(const FText& ActorName,
    EAbilityType Ability, int32 DC, const FDiceResult& Roll,
    bool bSuccess, int32 Round)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::SavingThrow, Round);
    Entry.SourceName  = ActorName;
    Entry.PrimaryRoll = Roll;

    const FString Modifier = Roll.Total - Roll.NaturalRoll >= 0
        ? FString::Printf(TEXT("+%d"), Roll.Total - Roll.NaturalRoll)
        : FString::Printf(TEXT("%d"), Roll.Total - Roll.NaturalRoll);

    const FString Msg = FString::Printf(
        TEXT("%s: %s Save d20(%d)%s=%d vs DC %d — %s"),
        *ActorName.ToString(),
        *GetAbilityShortName(Ability),
        Roll.NaturalRoll,
        *Modifier,
        Roll.Total,
        DC,
        bSuccess ? TEXT("Saved!") : TEXT("Failed!"));

    Entry.Message = FText::FromString(Msg);
    AddEntry(Entry);
}

void UCombatLogger::LogDeathSave(const FText& ActorName,
    const FDiceResult& Roll, bool bSuccess,
    int32 Successes, int32 Failures, int32 Round)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::DeathSave, Round);
    Entry.SourceName  = ActorName;
    Entry.PrimaryRoll = Roll;

    FString OutcomeStr;
    if (Roll.bIsCritical)
        OutcomeStr = TEXT("NAT 20 — Regains 1 HP!");
    else if (Roll.bIsCritFail)
        OutcomeStr = FString::Printf(
            TEXT("NAT 1 — 2 Failures! (%d/3)"), Failures);
    else
        OutcomeStr = FString::Printf(
            TEXT("%s (%d/3)"),
            bSuccess
                ? *FString::Printf(TEXT("Success! %d successes"), Successes)
                : *FString::Printf(TEXT("Failed! %d failures"), Failures),
            bSuccess ? Successes : Failures);

    const FString Msg = FString::Printf(
        TEXT("%s: Death Save d20(%d) — %s"),
        *ActorName.ToString(),
        Roll.NaturalRoll,
        *OutcomeStr);

    Entry.Message = FText::FromString(Msg);
    AddEntry(Entry);
}

void UCombatLogger::LogCondition(const FText& ActorName,
    EConditionType Condition, bool bApplied,
    int32 DurationRounds, int32 Round)
{
    const ECombatLogEventType Type = bApplied
        ? ECombatLogEventType::ConditionApplied
        : ECombatLogEventType::ConditionRemoved;

    FCombatLogEntry Entry = MakeEntry(Type, Round);
    Entry.SourceName = ActorName;

    FString ConditionName = UEnum::GetDisplayValueAsText(Condition).ToString();

    FString Msg;
    if (bApplied && DurationRounds > 0)
    {
        Msg = FString::Printf(TEXT("%s is now %s (%d round%s)"),
            *ActorName.ToString(),
            *ConditionName,
            DurationRounds,
            DurationRounds == 1 ? TEXT("") : TEXT("s"));
    }
    else if (bApplied)
    {
        Msg = FString::Printf(TEXT("%s is now %s"),
            *ActorName.ToString(), *ConditionName);
    }
    else
    {
        Msg = FString::Printf(TEXT("%s is no longer %s"),
            *ActorName.ToString(), *ConditionName);
    }

    Entry.Message = FText::FromString(Msg);
    AddEntry(Entry);
}

void UCombatLogger::LogInitiative(const FText& ActorName,
    const FDiceResult& Roll, int32 Round)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::Initiative, Round);
    Entry.SourceName  = ActorName;
    Entry.PrimaryRoll = Roll;

    const int32 Modifier = Roll.Total - Roll.NaturalRoll;
    const FString ModStr = Modifier >= 0
        ? FString::Printf(TEXT("+%d"), Modifier)
        : FString::Printf(TEXT("%d"), Modifier);

    const FString Msg = FString::Printf(
        TEXT("%s rolls initiative: d20(%d)%s=%d"),
        *ActorName.ToString(),
        Roll.NaturalRoll,
        *ModStr,
        Roll.Total);

    Entry.Message = FText::FromString(Msg);
    AddEntry(Entry);
}

void UCombatLogger::LogRoundStart(int32 RoundNumber)
{
    CurrentRound = RoundNumber;

    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::Misc, RoundNumber);

    const FString Msg = FString::Printf(
        TEXT("════════ Round %d ════════"), RoundNumber);

    Entry.Message = FText::FromString(Msg);
    AddEntry(Entry);
}

void UCombatLogger::LogTurnStart(const FText& ActorName, int32 Round)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::TurnStart, Round);
    Entry.SourceName = ActorName;
    Entry.Message    = FText::FromString(
        FString::Printf(TEXT("▶ %s's turn"), *ActorName.ToString()));
    AddEntry(Entry);
}

void UCombatLogger::LogCombatStart(const TArray<FText>& CombatantNames)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::CombatStart, 0);

    TArray<FString> Names;
    for (const FText& N : CombatantNames)
        Names.Add(N.ToString());

    Entry.Message = FText::FromString(
        FString::Printf(TEXT("⚔ Combat begins! Combatants: %s"),
            *FString::Join(Names, TEXT(", "))));

    AddEntry(Entry);
}

void UCombatLogger::LogCombatEnd(bool bPlayerVictory)
{
    FCombatLogEntry Entry = MakeEntry(ECombatLogEventType::CombatEnd,
        CurrentRound);

    Entry.Message = FText::FromString(bPlayerVictory
        ? TEXT("🏆 Victory! All enemies defeated.")
        : TEXT("💀 Defeat. The party has fallen."));

    AddEntry(Entry);
}

// ============================================================
//  RETRIEVAL
// ============================================================

TArray<FCombatLogEntry> UCombatLogger::GetRecentLog(int32 Count) const
{
    if (Count <= 0 || Count >= LogEntries.Num())
        return LogEntries;

    const int32 StartIdx = FMath::Max(0, LogEntries.Num() - Count);
    return TArray<FCombatLogEntry>(
        LogEntries.GetData() + StartIdx, Count);
}

TArray<FCombatLogEntry> UCombatLogger::GetEntriesByType(
    ECombatLogEventType EventType) const
{
    TArray<FCombatLogEntry> Result;
    for (const FCombatLogEntry& E : LogEntries)
    {
        if (E.EventType == EventType)
            Result.Add(E);
    }
    return Result;
}

void UCombatLogger::ClearLog()
{
    LogEntries.Empty();
    UE_LOG(LogCombat, Log, TEXT("CombatLogger: Log cleared"));
}

FString UCombatLogger::ExportLogAsString() const
{
    FString Result;
    for (const FCombatLogEntry& E : LogEntries)
    {
        Result += FString::Printf(TEXT("[R%d %s] %s\n"),
            E.Round,
            *E.Timestamp.ToString(TEXT("%H:%M:%S")),
            *E.Message.ToString());
    }
    return Result;
}

// ============================================================
//  PRIVATE
// ============================================================

FCombatLogEntry UCombatLogger::MakeEntry(ECombatLogEventType EventType,
    int32 Round) const
{
    FCombatLogEntry Entry;
    Entry.Timestamp = FDateTime::Now();
    Entry.EventType = EventType;
    Entry.Round     = (Round > 0) ? Round : CurrentRound;
    return Entry;
}

void UCombatLogger::TrimIfNeeded()
{
    while (LogEntries.Num() > MaxLogEntries)
        LogEntries.RemoveAt(0);
}

FString UCombatLogger::GetAbilityShortName(EAbilityType Ability)
{
    switch (Ability)
    {
        case EAbilityType::Strength:     return TEXT("STR");
        case EAbilityType::Dexterity:    return TEXT("DEX");
        case EAbilityType::Constitution: return TEXT("CON");
        case EAbilityType::Intelligence: return TEXT("INT");
        case EAbilityType::Wisdom:       return TEXT("WIS");
        case EAbilityType::Charisma:     return TEXT("CHA");
        default:                          return TEXT("???");
    }
}

FString UCombatLogger::GetDamageTypeName(EDamageType DamageType)
{
    return UEnum::GetDisplayValueAsText(DamageType).ToString();
}