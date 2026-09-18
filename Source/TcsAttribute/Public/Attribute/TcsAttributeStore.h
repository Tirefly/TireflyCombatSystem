// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttributeInstance.h"
#include "Attribute/TcsAttributeName.h"



/**
 * 单位属性容器（02 §2.2a）：纯 C++ struct——每个单位一个，由属性门面按单位句柄键控持有。
 *
 * 形状理由（D2-9）：单位适配器可长期缓存本容器指针与实例指针以跳过外层查找——
 * UE TMap 的值节点堆分配且地址稳定，后续插入不会使既有指针失效。
 *
 * 取值口说明：本容器只读**缓存值**；当前值的"脏则惰性重算"语义由聚合管线承担
 * （管线是 CachedCurrent 的唯一生产者），故不在此提供看似会重算的当前值读取口。
 */
struct FTcsAttributeStore
{
	// 属性实例表（键 = 属性名；每属性一条，实例自持边界与修正器槽位）
	TMap<FTcsAttributeName, FTcsAttributeInstance> Attributes;

	// 查找属性实例（属性未定义时返回 nullptr——正常查询路径，不视为错误）
	FTcsAttributeInstance* FindInstance(const FTcsAttributeName& Attribute)
	{
		return Attributes.Find(Attribute);
	}

	// 查找属性实例（只读）
	const FTcsAttributeInstance* FindInstance(const FTcsAttributeName& Attribute) const
	{
		return Attributes.Find(Attribute);
	}

	// 读取属性基础值（不受修正器影响；属性未定义时返回 0）
	double GetBaseValue(const FTcsAttributeName& Attribute) const
	{
		const FTcsAttributeInstance* Instance = FindInstance(Attribute);
		return Instance ? Instance->BaseValue : 0.0;
	}
};
