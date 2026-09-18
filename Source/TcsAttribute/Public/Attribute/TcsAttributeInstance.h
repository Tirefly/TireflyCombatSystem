// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttributeBounds.h"
#include "Attribute/TcsAttrModInstance.h"
#include "Attribute/TcsAttributeName.h"



/**
 * 单属性实例（M2 账本，02 §2.2a）：纯 C++ struct——聚合热路径的遍历对象，不进反射面。
 *
 * CachedCurrent 是**派生缓存非权威**：聚合管线是唯一生产者、脏则惰性重算（D2-8）；
 * 永远可由 BaseValue + 修正器 + 折叠公式 + 值域收口重建（操作复制 + 客户端重算的地基，NET-1/2）。
 */
struct FTcsAttributeInstance
{
	// 属性名（实例与定义的连接键；实例无需 Def 缓存，词表 schema 由属性名侧覆盖——D2-1）
	FTcsAttributeName Attr;

	// 基础值（属性表默认 → 注册时初始化；等级成长由宿主升级事务改写，等级→数值映射归项目）
	double BaseValue = 0.0;

	// 当前值缓存（非权威：管线写入、脏则重算；新建时占位 = BaseValue，**尚未结算**——值域收口与既有修正器在首读/提交时生效）
	double CachedCurrent = 0.0;

	// 脏标记（新建实例、修正器挂/摘、基础值改写、依赖属性变更时置位；批外读取与提交时结算并清零）
	bool bDirty = false;

	// 值域边界（Min/Max 各自三态）
	FTcsAttributeBounds Bounds;

	// 值域模式（末端收口语义）
	ETcsAttributeValueDomain ValueDomain = ETcsAttributeValueDomain::AVD_Clamp;

	// 覆盖带同优先级裁决策略（定义侧展开——热路径不回查定义；含义见 ETcsAttrOverrideTieBreak）
	ETcsAttrOverrideTieBreak OverrideTieBreak = ETcsAttrOverrideTieBreak::OTB_Max;

	// 修正器槽位（挂在**被修饰属性**上——聚合按属性遍历，零查找；Source 级联摘除）
	TArray<FTcsAttrModInstance> ModifierSlots;
};
