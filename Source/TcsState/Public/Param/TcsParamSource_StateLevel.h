// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Parameter/TcsParamEnumerableSource.h"
#include "Host/TcsEntityLevelProvider.h"

#include "TcsParamSource_StateLevel.generated.h"



/**
 * 被施加方等级数组源（D3-11 四源之一）："buff 每级 +10 HP"的直接配置形态。
 *
 * **等级来源 = `Context.EffectiveLevel`**（由装配上下文的调用方填写——通常是状态的
 * `Def.LevelBase`）；源 MUST NOT 反查实例或全局表（取值归属纪律）。
 *
 * **口径**：下标 = `Level - 1`（等级自 1 起 ⇒ 第 1 档即首元素）；等级无效（`≤ 0`）或表为空
 * ⇒ 落兜底值；下标越过末档 ⇒ **落最后一档**（不回绕、不崩溃）。
 * `GetIndexForLevel` 与 `Evaluate` **同源**（索引解析的唯一真相在本源，PV-10）。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsParamSource_StateLevelArray : public FTcsParamEnumerableSource
{
	GENERATED_BODY()

// 取值配置
#pragma region Values

public:
	// 分档数值（第 1 档 = 等级 1）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	TArray<double> Values;

	// 兜底值（等级无效或表为空时取用——可失败源必填，PV-1）
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

		// 越界落最后一档（"等级超出表长时沿用顶档"是等级表的既定读法）
		return FMath::Min(Level - 1, Values.Num() - 1);
	}

#pragma endregion


// 求值
#pragma region Evaluate

public:
	// 按上下文等级取档；无档位语义落兜底
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const override
	{
		const int32 Index = GetIndexForLevel(Context.EffectiveLevel);
		return Index == INDEX_NONE ? Fallback : Values[Index];
	}

#pragma endregion


// 约定能力位
#pragma region ValueConvention

public:
	// 表型源：书写值直接成为结果 ⇒ 允许配置行级值约定列（D5-18 v3 白名单）
	virtual bool AllowsValueConvention() const override
	{
		return true;
	}

#pragma endregion
};



/**
 * 被施加方等级映射源（D3-11 四源之一）：离散等级档（"3 级才给眩晕"）。
 *
 * **未命中不落"最近档"**——映射的语义是"这个等级有没有配"，缺档即落 `Fallback`
 * （与数组源的"越界落最后一档"刻意不同：数组是连续档、映射是离散档）。
 */
USTRUCT(BlueprintType)
struct TCSSTATE_API FTcsParamSource_StateLevelMap : public FTcsParamEnumerableSource
{
	GENERATED_BODY()

// 取值配置
#pragma region Values

public:
	// 分档数值（键 = 等级）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	TMap<int32, double> Values;

	// 兜底值（等级无效或未命中时取用）
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Param Value")
	double Fallback = 0.0;

#pragma endregion


// 档位查询
#pragma region Index

public:
	// 映射源的档位是键本身、不是稠密下标 ⇒ 恒无"下标"语义（`INDEX_NONE`）
	virtual int32 GetIndexForLevel(int32 Level) const override
	{
		return INDEX_NONE;
	}

#pragma endregion


// 求值
#pragma region Evaluate

public:
	// 按键命中取档；未命中落兜底
	virtual double Evaluate(const FTcsParamEvaluateContext& Context) const override
	{
		const double* Found = Values.Find(Context.EffectiveLevel);
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
