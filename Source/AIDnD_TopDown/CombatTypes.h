// CombatTypes.h
#pragma once

#include "CoreMinimal.h"
#include "CombatTypes.generated.h"

// Combat state enum
UENUM(BlueprintType)
enum class ECombatState : uint8
{
    NotInCombat UMETA(DisplayName = "Not In Combat"),
    WaitingForTurn UMETA(DisplayName = "Waiting For Turn"),
    PlayerTurn UMETA(DisplayName = "Player Turn"),
    EnemyTurn UMETA(DisplayName = "Enemy Turn"),
    SelectingAction UMETA(DisplayName = "Selecting Action"),
    ExecutingAction UMETA(DisplayName = "Executing Action"),
    CombatEnded UMETA(DisplayName = "Combat Ended")
};

// Action types
UENUM(BlueprintType)
enum class EActionType : uint8
{
    Move UMETA(DisplayName = "Move"),
    Attack UMETA(DisplayName = "Attack"),
    Cast UMETA(DisplayName = "Cast Spell"),
    Dash UMETA(DisplayName = "Dash"),
    Dodge UMETA(DisplayName = "Dodge"),
    Help UMETA(DisplayName = "Help"),
    EndTurn UMETA(DisplayName = "End Turn")
};

// D&D Character stats structure
USTRUCT(BlueprintType)
struct FCharacterStats
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Strength = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Dexterity = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Constitution = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Intelligence = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Wisdom = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stats")
    int32 Charisma = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    int32 ArmorClass = 10;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    int32 MaxHP = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    int32 CurrentHP = 100;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
    int32 Initiative = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MovementSpeed = 9.0f; // 30 feet in D&D = 9 meters

    // Helper function to calculate modifier from stat
    int32 GetModifier(int32 StatValue) const
    {
        return (StatValue - 10) / 2;
    }
};

// Action data structure
USTRUCT(BlueprintType)
struct FActionData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    EActionType ActionType;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    FString ActionName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    int32 ActionCost = 1; // Action economy (1 = full action, 0.5 = bonus action)

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    float Range = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    int32 DamageDice = 8; // d8

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Action")
    int32 DiceCount = 1;
};

// Turn information structure
USTRUCT(BlueprintType)
struct FCombatTurn
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn")
    class ACombatCharacter* Character;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn")
    int32 Initiative;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Turn")
    bool bHasTakenTurn = false;
};