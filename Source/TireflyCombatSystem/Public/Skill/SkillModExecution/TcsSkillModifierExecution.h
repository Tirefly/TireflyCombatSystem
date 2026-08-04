// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "StructUtils/InstancedStruct.h"
#include "TcsSkillModifierExecution.generated.h"



class UTcsSkillEntry;
struct FStateParamNumericModifierInstance;
struct FStateParamBoolModifierInstance;
struct FStateParamVectorModifierInstance;



/**
 * Numeric ModifierExecution 的操作数配置。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierFloatConfig
{
	GENERATED_BODY()

	/** 操作数值（加法/乘算/覆盖共用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Operand = 0.0f;
};



/**
 * Bool ModifierExecution 的操作数配置。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierBoolConfig
{
	GENERATED_BODY()

	/** 目标布尔值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Value = false;
};



/**
 * Vector ModifierExecution 的操作数配置。
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillModifierVectorConfig
{
	GENERATED_BODY()

	/** 目标向量值。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Value = FVector::ZeroVector;
};





/**
 * Numeric 类型 SkillModifier 执行策略基类。
 *
 * 使用 CDO 策略模式执行 Numeric 类型的参数修改。
 */
UCLASS(Abstract, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsStateParamNumericModifierExecution : public UObject
{
	GENERATED_BODY()

	friend struct FTcsNumericStateParamInstance;


protected:
	/** 仅由 FTcsNumericStateParamInstance 的 effective 求值链调用。 */
	virtual float Evaluate(float CurrentValue, const FStateParamNumericModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};



/**
 * Bool 类型 SkillModifier 执行策略基类。
 */
UCLASS(Abstract, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsStateParamBoolModifierExecution : public UObject
{
	GENERATED_BODY()

	friend struct FTcsBoolStateParamInstance;


protected:
	/** 仅由 FTcsBoolStateParamInstance 的 effective 求值链调用。 */
	virtual bool Evaluate(bool CurrentValue, const FStateParamBoolModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};



/**
 * Vector 类型 SkillModifier 执行策略基类。
 */
UCLASS(Abstract, Blueprintable)
class TIREFLYCOMBATSYSTEM_API UTcsStateParamVectorModifierExecution : public UObject
{
	GENERATED_BODY()

	friend struct FTcsVectorStateParamInstance;


protected:
	/** 仅由 FTcsVectorStateParamInstance 的 effective 求值链调用。 */
	virtual FVector Evaluate(FVector CurrentValue, const FStateParamVectorModifierInstance& ModifierInst,
		UTcsSkillEntry* SkillEntry, AActor* Instigator) const PURE_VIRTUAL(, return CurrentValue;);
};
