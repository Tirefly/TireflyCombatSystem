// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Parameter/TcsParamEnumerableSource.h"
#include "Host/TcsEntityLevelProvider.h"

#include "TcsParamSource_InstigatorLevel.generated.h"



/**
 * 发起者等级数组源（D3-11 四源之一）："按施法者等级放大效果"的配置形态。
 *
 * **等级来源 = `Context.Instigator` → `FTcsStateEvaluateContext::LevelProvider` → level**
 * ——源只读上下文（取值归属纪律），MUST NOT 自行反查门面或全局表。
 *
 * **三条兜底路径**（都不留红字、不 ensure）：上下文不是状态域派生上下文 / 读口未注入 /
 * 读口给出 0（无等级语义）。其余口径（下标 = `Level - 1`、越界落最后一档、索引与求值同源）
 * 与 `FTcsParamSource_StateLevelArray` 一致。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsParamSource_InstigatorLevelArray : public FTcsParamEnumerableSource
{
	GENERATED_BODY()

// 取值配置
#pragma region Values

public:
	// 分档数值（第 1 档 = 等级 1）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	TArray<double> Values;

	// 兜底值（等级不可得或表为空时取用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	double Fallback = 0.0;

#pragma endregion


// 档位查询
#pragma region Index

public:
	// 等级 → 档位下标（`INDEX_NONE` = 该等级无对应档）
	virtual int32 GetIndexForLevel(int32 Level) const override
	{
		if (Level <= 0 || Values.Num() == 0)
		{
			return INDEX_NONE;
		}

		return FMath::Min(Level - 1, Values.Num() - 1);
	}

#pragma endregion


// 求值
#pragma region Evaluate

public:
	// 经上下文读口取发起者等级后按档取值；读口不可得落兜底
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const override
	{
		// 上下文不匹配 = 装配/配置错误（由校验矩阵兜底）——运行期只降级，不崩溃不 ensure
		if (!Context.GetScriptStruct()->IsChildOf(FTcsStateEvaluateContext::StaticStruct()))
		{
			return Fallback;
		}

		const FTcsStateEvaluateContext& StateContext = static_cast<const FTcsStateEvaluateContext&>(Context);
		UObject* ProviderObject = StateContext.LevelProvider.GetObject();
		if (!ProviderObject)
		{
			return Fallback;
		}

		const int32 InstigatorLevel = ITcsEntityLevelProvider::Execute_GetEntityLevel(ProviderObject, StateContext.Instigator);
		const int32 Index = GetIndexForLevel(InstigatorLevel);
		return Index == INDEX_NONE ? Fallback : Values[Index];
	}

#pragma endregion


// 约定能力位
#pragma region ValueConvention

public:
	// 表型源：书写值直接成为结果 ⇒ 允许配置值约定列
	virtual bool AllowsValueConvention() const override
	{
		return true;
	}

#pragma endregion
};



/**
 * 发起者等级映射源（D3-11 四源之一）：离散等级档。
 * 兜底路径与缺档语义同 `FTcsParamSource_StateLevelMap`（未命中落兜底，不取最近档）。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsParamSource_InstigatorLevelMap : public FTcsParamEnumerableSource
{
	GENERATED_BODY()

// 取值配置
#pragma region Values

public:
	// 分档数值（键 = 等级）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	TMap<int32, double> Values;

	// 兜底值（等级不可得或未命中时取用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	double Fallback = 0.0;

#pragma endregion


// 档位查询
#pragma region Index

public:
	// 映射源的档位是键本身、不是稠密下标 ⇒ 恒无"下标"语义
	virtual int32 GetIndexForLevel(int32 Level) const override
	{
		return INDEX_NONE;
	}

#pragma endregion


// 求值
#pragma region Evaluate

public:
	// 经上下文读口取发起者等级后按键取档
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const override
	{
		if (!Context.GetScriptStruct()->IsChildOf(FTcsStateEvaluateContext::StaticStruct()))
		{
			return Fallback;
		}

		const FTcsStateEvaluateContext& StateContext = static_cast<const FTcsStateEvaluateContext&>(Context);
		UObject* ProviderObject = StateContext.LevelProvider.GetObject();
		if (!ProviderObject)
		{
			return Fallback;
		}

		const int32 InstigatorLevel = ITcsEntityLevelProvider::Execute_GetEntityLevel(ProviderObject, StateContext.Instigator);
		const double* Found = Values.Find(InstigatorLevel);
		return Found ? *Found : Fallback;
	}

#pragma endregion


// 约定能力位
#pragma region ValueConvention

public:
	// 表型源：书写值直接成为结果 ⇒ 允许配置值约定列
	virtual bool AllowsValueConvention() const override
	{
		return true;
	}

#pragma endregion
};
