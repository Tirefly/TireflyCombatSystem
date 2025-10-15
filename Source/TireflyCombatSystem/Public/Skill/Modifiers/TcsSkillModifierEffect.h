#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "TcsSkillModifierEffect.generated.h"

/**
 * 技能参数聚合效果
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsAggregatedParamEffect
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float AddSum = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float MulProd = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	bool bHasOverride = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float OverrideValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float CooldownMultiplier = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SkillModifier")
	float CostMultiplier = 1.f;
};

/**
 * 基于FName的技能参数效果容器
 * 将聚合效果和脏参数标记封装在一起，避免UE反射系统的嵌套容器限制
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillParamEffectByName
{
	GENERATED_BODY()

public:
	// 聚合的参数效果映射 (ParamName -> AggregatedEffect)
	UPROPERTY()
	TMap<FName, FTcsAggregatedParamEffect> AggregatedEffects;

	// 需要重新计算的脏参数名集合
	UPROPERTY()
	TSet<FName> DirtyParams;

public:
	// 标记某个参数为脏
	void MarkDirty(FName ParamName)
	{
		DirtyParams.Add(ParamName);
	}

	// 清除某个参数的脏标记
	void ClearDirty(FName ParamName)
	{
		DirtyParams.Remove(ParamName);
	}

	// 清除所有脏标记
	void ClearAllDirty()
	{
		DirtyParams.Empty();
	}

	// 检查某个参数是否是脏的
	bool IsDirty(FName ParamName) const
	{
		return DirtyParams.Contains(ParamName);
	}

	// 获取聚合效果（如果不存在则创建默认值）
	FTcsAggregatedParamEffect& GetOrAddEffect(FName ParamName)
	{
		return AggregatedEffects.FindOrAdd(ParamName);
	}

	// 获取聚合效果（只读，如果不存在返回nullptr）
	const FTcsAggregatedParamEffect* FindEffect(FName ParamName) const
	{
		return AggregatedEffects.Find(ParamName);
	}

	// 设置聚合效果并清除脏标记
	void SetEffect(FName ParamName, const FTcsAggregatedParamEffect& Effect)
	{
		AggregatedEffects.Add(ParamName, Effect);
		ClearDirty(ParamName);
	}

	// 移除某个参数的所有数据
	void RemoveParam(FName ParamName)
	{
		AggregatedEffects.Remove(ParamName);
		DirtyParams.Remove(ParamName);
	}

	// 清空所有数据
	void Reset()
	{
		AggregatedEffects.Empty();
		DirtyParams.Empty();
	}

	// 获取所有脏参数的数组（用于遍历）
	TArray<FName> GetDirtyParamsArray() const
	{
		return DirtyParams.Array();
	}
};

/**
 * 基于GameplayTag的技能参数效果容器
 * 将聚合效果和脏参数标记封装在一起，避免UE反射系统的嵌套容器限制
 */
USTRUCT(BlueprintType)
struct TIREFLYCOMBATSYSTEM_API FTcsSkillParamEffectByTag
{
	GENERATED_BODY()

public:
	// 聚合的参数效果映射 (ParamTag -> AggregatedEffect)
	UPROPERTY()
	TMap<FGameplayTag, FTcsAggregatedParamEffect> AggregatedEffects;

	// 需要重新计算的脏参数标签集合
	UPROPERTY()
	TSet<FGameplayTag> DirtyParams;

public:
	// 标记某个参数为脏
	void MarkDirty(FGameplayTag ParamTag)
	{
		DirtyParams.Add(ParamTag);
	}

	// 清除某个参数的脏标记
	void ClearDirty(FGameplayTag ParamTag)
	{
		DirtyParams.Remove(ParamTag);
	}

	// 清除所有脏标记
	void ClearAllDirty()
	{
		DirtyParams.Empty();
	}

	// 检查某个参数是否是脏的
	bool IsDirty(FGameplayTag ParamTag) const
	{
		return DirtyParams.Contains(ParamTag);
	}

	// 获取聚合效果（如果不存在则创建默认值）
	FTcsAggregatedParamEffect& GetOrAddEffect(FGameplayTag ParamTag)
	{
		return AggregatedEffects.FindOrAdd(ParamTag);
	}

	// 获取聚合效果（只读，如果不存在返回nullptr）
	const FTcsAggregatedParamEffect* FindEffect(FGameplayTag ParamTag) const
	{
		return AggregatedEffects.Find(ParamTag);
	}

	// 设置聚合效果并清除脏标记
	void SetEffect(FGameplayTag ParamTag, const FTcsAggregatedParamEffect& Effect)
	{
		AggregatedEffects.Add(ParamTag, Effect);
		ClearDirty(ParamTag);
	}

	// 移除某个参数的所有数据
	void RemoveParam(FGameplayTag ParamTag)
	{
		AggregatedEffects.Remove(ParamTag);
		DirtyParams.Remove(ParamTag);
	}

	// 清空所有数据
	void Reset()
	{
		AggregatedEffects.Empty();
		DirtyParams.Empty();
	}

	// 获取所有脏参数的数组（用于遍历）
	TArray<FGameplayTag> GetDirtyParamsArray() const
	{
		return DirtyParams.Array();
	}
};
