// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * TcsCore 模块级运行统计（plan1 Task 1：池占用统计 CVar Tcs.Core.PoolSlots 的维护口）。
 * 只在游戏线程被池调用（D0-4），计数非原子——CVar 值为只读参考。
 * 池槽位操作粒度恒为 1，故增/删为无参对——正负号与数量的歧义在签名上杜绝（2026-09-15 用户拍板拆分）。
 */
struct TCSCORE_API FTcsCorePoolStats
{
	// 新建稠密槽位时调用（池 Allocate 走新槽分支；FreeList 复用不调用——占用数净零不变）
	static void AddSlotCount();

	// 回收槽位时调用（池 Free）
	static void RemoveSlotCount();

	// 读取跨池已分配槽位总数（CVar Tcs.Core.PoolSlots 同源）
	static int32 GetSlotCount();
};

/**
 * 到期堆深度统计（plan1 Task 3：CVar Tcs.Core.ExpiryHeapDepth 的维护口）。
 * 只在游戏线程被到期堆调用（D0-4），计数非原子——CVar 值为只读参考。
 * 增/删为无参对——条目进出粒度恒为 1，与池统计同款签名纪律。
 */
struct TCSCORE_API FTcsCoreHeapStats
{
	// 入堆成功时调用（Push）
	static void AddDepth();

	// 条目移出时调用（Cancel 回收 / 到期回调完成释放 / Reset 清空）
	static void RemoveDepth();

	// 读取当前到期堆有效条目总数（CVar Tcs.Core.ExpiryHeapDepth 同源）
	static int32 GetDepth();
};
