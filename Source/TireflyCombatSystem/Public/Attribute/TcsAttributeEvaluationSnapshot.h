// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



class UTcsAttributeComponent;



/** AttributeModifier 单轮求值使用的只读 Attribute 数值快照。 */
struct TIREFLYCOMBATSYSTEM_API FTcsAttributeEvaluationSnapshot
{
public:
	/**
	 * 读取目标组件中某个 Attribute 的 BaseValue 快照。
	 *
	 * @param AttributeId Attribute Definition Id。
	 * @param OutValue 成功时输出 BaseValue。
	 * @return Snapshot 含有该 Attribute 时返回 true。
	 */
	bool GetBaseValue(FName AttributeId, float& OutValue) const;

	/**
	 * 读取目标组件中某个 Attribute 的 CurrentValue 快照。
	 *
	 * @param AttributeId Attribute Definition Id。
	 * @param OutValue 成功时输出 CurrentValue。
	 * @return Snapshot 含有该 Attribute 时返回 true。
	 */
	bool GetCurrentValue(FName AttributeId, float& OutValue) const;

private:
	friend class UTcsAttributeComponent;

	// 目标组件中各 Attribute 的 BaseValue 值拷贝。
	TMap<FName, float> BaseValues;

	// 目标组件中各 Attribute 的 CurrentValue 值拷贝。
	TMap<FName, float> CurrentValues;
};
