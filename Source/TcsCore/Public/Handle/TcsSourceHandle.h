// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"



/**
 * 归属来源标识（R0 §9：系统级通用归属机制，非战斗本体论）。
 * 进程内唯一 Id，0 保留为无效值；作为级联撤销锚点（D2-2：来源注销 → 消费方按 Source 级联移除）。
 *
 * 导出宏纪律（2026-09-16 编译实证）：全内联的 header-only 值类型**不加模块导出宏**——
 * 加了会让消费方期待"从 TcsCore.dll 导入"这些符号，而 MSVC 只为"本模块自己用到的类型"
 * 生成导出符号：本模块未使用的类型会在消费方以 LNK2019 炸开。需要导出宏的是
 * "有 out-of-line 成员或反射符号"的类型（TcsCoreStats / 反射 USTRUCT / 子系统类）。
 * **本文件正是该纪律的两面**：`FTcsSourceHandle` 恒为纯内联 ⇒ 不加宏；下方的
 * `FTcsSourceHandleRegistry` 自 2026-10-04 起持有 out-of-line 成员 ⇒ 必须加宏。
 */
struct FTcsSourceHandle
{
	// 来源唯一 Id（0 = 无效）
	uint64 Id = 0;

	// 来源有效性
	bool IsValid() const
	{
		return Id != 0;
	}

	// 相等比较（来源配对/级联过滤用）
	friend bool operator==(const FTcsSourceHandle& A, const FTcsSourceHandle& B)
	{
		return A.Id == B.Id;
	}
};



/**
 * 来源分配器：**进程唯一**原子递增发号（首个分配为 1）。
 * 释放/级联清理由需要方驱动（D2-2——本类只发号，不做注销登记）。
 *
 * **为什么必须是进程唯一、而不是每实例一份（2026-10-04 `WAIT-6` 修复）**：本类被**不同模块**的
 * 子系统各持一份（`UTcsDefinitionSubsystem::TriggerSourceRegistry` 住 TcsIntegration、
 * `UTcsDamageSubsystem::FlowSourceRegistry` 住 TcsDamage），而插件是模块化构建
 * （`UnrealEditor-TcsCore` / `-TcsIntegration` / `-TcsDamage` 各自成形）。原实现把计数器做成
 * 实例成员、`Allocate()` 全内联在头里 ⇒ **每个实例都从 1 开始发号、跨模块必然重号**，
 * 级联摘除（`UnregisterTriggerRowsBySource` / `RemoveBySource`）会连别人的来源一起摘掉。
 * **只把计数器改成 `static` 成员修不掉**：MSVC 的 COMDAT 折叠不跨 DLL 合并，内联变量在每个
 * 镜像各留一份（而 monolithic Shipping 又会合并成一份 ⇒ 症状随构建配置漂移）。
 * 故本类改为**出线**：`Allocate()` 的定义住 `Private/Handle/TcsSourceHandle.cpp`，计数器是该文件的
 * 匿名 namespace 变量，全进程只此一份；所有模块经 TcsCore.dll 的导出符号调到同一个它。
 */
struct TCSCORE_API FTcsSourceHandleRegistry
{
	// 分配新来源 Id（原子递增，进程内唯一；定义住 TcsCore 的 .cpp）
	FTcsSourceHandle Allocate();
};
