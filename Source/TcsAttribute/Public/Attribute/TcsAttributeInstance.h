// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Attribute/TcsAttributeBounds.h"
#include "Attribute/TcsAttrModInstance.h"
#include "GameplayTagContainer.h"



/**
 * 单属性实例（M2 账本，02 §2.2a）：纯 C++ struct——聚合热路径的遍历对象，不进反射面。
 *
 * CachedCurrent 是**派生缓存非权威**：聚合管线是唯一生产者、脏则惰性重算（D2-8）；
 * 永远可由 BaseValue + 修正器 + 折叠公式 + 值域收口重建（操作复制 + 客户端重算的地基，NET-1/2）。
 */
struct FTcsAttributeInstance
{
	// 属性名（实例与定义的连接键；实例无需 Def 缓存，词表 schema 由属性名侧覆盖——D2-1）
	FGameplayTag Attr;

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

	/**
	 * 修正器条目集（挂在**被修饰属性**上——聚合按属性遍历，零查找；Source 级联摘除）。
	 *
	 * **命名规则（2026-10-09 立，MUST 遵守）**：容器字段名 = **元素类型名的复数形式**
	 * （`FTcsAttrModInstance` → `AttrModInstances`）。判据可自检：**字段名 MUST 能从元素类型名推出来**。
	 * **`Slot` 一词保留给"可按下标寻址、可复用、带代际的空位"**（本模块内如池的 `FreeList`、
	 * 各桶的 `SlotGenerations`）——本容器既不可按下标寻址、也不复用、也不带代际
	 * （只有 `.Add` / `.RemoveAll` / `.Num()` / 范围遍历）⇒ **MUST NOT** 以 `Slots` 命名。
	 * 本条原为 `ModifierSlots`——那个后缀沿袭自 AbilityKit 的"修改器槽位自由链表"
	 * （`ModifierSlot{Handle, ModifierData, NextFree, Active}`），而本仓早已拆掉槽壳、
	 * 元素直接就是修正器条目 ⇒ 后缀描述的是一个**已不存在**的包装层。
	 */
	TArray<FTcsAttrModInstance> AttrModInstances;
};
