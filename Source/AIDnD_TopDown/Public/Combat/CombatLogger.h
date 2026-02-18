// Source/AIDnD_TopDown/Public/Combat/CombatLogger.h
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Combat/CombatTypes.h"
#include "CombatLogger.generated.h"

// ----------------------------------------------------------------
//  Delegate
// ----------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnNewLogEntry,
    const FCombatLogEntry&, Entry);

// ----------------------------------------------------------------
//  UCombatLogger
// ----------------------------------------------------------------

/**
 * Game Instance Subsystem — persists across level loads.
 * Stores and formats combat log entries in BG3 style.
 *
 * Access from anywhere:
 *   UCombatLogger* Logger = GameInstance->GetSubsystem<UCombatLogger>();
 *   Or use the static helper GetCombatLogger().
 */
UCLASS()
class AIDND_TOPDOWN_API UCombatLogger : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:

    // ============================================================
    //  CONFIGURATION
    // ============================================================

    /** Maximum entries to keep in memory */
    UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Combat|Log",
        meta = (ClampMin = 10))
    int32 MaxLogEntries = 200;

    // ============================================================
    //  DELEGATE
    // ============================================================

    /** Fired every time a new entry is added — bind in UI widget */
    UPROPERTY(BlueprintAssignable, Category = "Combat|Log")
    FOnNewLogEntry OnNewLogEntry;

    // ============================================================
    //  STATIC ACCESS
    // ============================================================

    UFUNCTION(BlueprintCallable, BlueprintPure,
        Category = "Combat|Log",
        meta = (WorldContext = "WorldContextObject",
                DisplayName = "Get Combat Logger"))
    static UCombatLogger* GetCombatLogger(const UObject* WorldContextObject);

    // ============================================================
    //  LOGGING — generic
    // ============================================================

    /** Add a pre-built log entry */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void AddEntry(const FCombatLogEntry& Entry);

    /** Add a simple text message */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogMessage(const FString& Message,
        ECombatLogEventType EventType = ECombatLogEventType::Misc,
        int32 Round = 0);

    // ============================================================
    //  LOGGING — formatted helpers (BG3 style)
    // ============================================================

    /**
     * Log an attack roll result.
     * Format: "Tavita attacks Goblin: d20(14)+3=17 vs AC 13 — Hit!"
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogAttack(const FText& SourceName, const FText& TargetName,
        const FAttackResult& AttackResult, int32 Round = 0);

    /**
     * Log damage dealt.
     * Format: "Goblin takes 11 Slashing damage (2d6+3) — 4 HP remaining"
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogDamage(const FText& TargetName, int32 DamageAmount,
        EDamageType DamageType, const FDiceResult& DiceRoll,
        int32 RemainingHP, int32 Round = 0);

    /**
     * Log a saving throw.
     * Format: "Goblin: CON Save d20(8)+1=9 vs DC 13 — Failed!"
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogSavingThrow(const FText& ActorName, EAbilityType Ability,
        int32 DC, const FDiceResult& Roll, bool bSuccess, int32 Round = 0);

    /**
     * Log a death saving throw.
     * Format: "Tavita: Death Save d20(15) — Success! (2/3)"
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogDeathSave(const FText& ActorName, const FDiceResult& Roll,
        bool bSuccess, int32 Successes, int32 Failures, int32 Round = 0);

    /**
     * Log a condition being applied or removed.
     * Format: "Goblin is now Poisoned (2 rounds)"
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogCondition(const FText& ActorName, EConditionType Condition,
        bool bApplied, int32 DurationRounds = 0, int32 Round = 0);

    /**
     * Log initiative roll.
     * Format: "Tavita rolls initiative: d20(12)+2=14"
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogInitiative(const FText& ActorName, const FDiceResult& Roll,
        int32 Round = 0);

    /**
     * Log round/turn transitions.
     * Format: "=== Round 3 begins ===" or "Tavita's turn starts"
     */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogRoundStart(int32 RoundNumber);

    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogTurnStart(const FText& ActorName, int32 Round = 0);

    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogCombatStart(const TArray<FText>& CombatantNames);

    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void LogCombatEnd(bool bPlayerVictory);

    // ============================================================
    //  RETRIEVAL
    // ============================================================

    /**
     * Returns the N most recent log entries.
     * @param Count  Number of entries (0 = all)
     */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Log")
    TArray<FCombatLogEntry> GetRecentLog(int32 Count = 20) const;

    /** Returns all entries of a specific event type */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Log")
    TArray<FCombatLogEntry> GetEntriesByType(
        ECombatLogEventType EventType) const;

    /** Total number of entries */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Log")
    int32 GetEntryCount() const { return LogEntries.Num(); }

    /** Clear all log entries */
    UFUNCTION(BlueprintCallable, Category = "Combat|Log")
    void ClearLog();

    /** Export full log as a single string (for save file / debug) */
    UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Combat|Log")
    FString ExportLogAsString() const;

    // ============================================================
    //  SUBSYSTEM LIFECYCLE
    // ============================================================

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:

    UPROPERTY()
    TArray<FCombatLogEntry> LogEntries;

    int32 CurrentRound = 0;

    /** Build a base entry with timestamp and round */
    FCombatLogEntry MakeEntry(ECombatLogEventType EventType,
        int32 Round) const;

    /** Trim log to MaxLogEntries */
    void TrimIfNeeded();

    /** Get short ability name for log formatting */
    static FString GetAbilityShortName(EAbilityType Ability);

    /** Get damage type display name */
    static FString GetDamageTypeName(EDamageType DamageType);
};