// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "State/TcsStateSnapshot.h"



/**
 * 参数快照的读取适配器：把 `FTcsParamSnapshot` 当参数表暴露出来。
 *
 * **它是"从快照取值"的唯一入口**——修正器物化（R5 Task 4）与引用类参数源都经它读快照，
 * 消费方 MUST NOT 自行遍历 `Entries` 去拼一套平行的键查找（那会制造第二处查找语义）。
 *
 * **为什么是纯 C++ 类而不是 `UObject` + `ITcsParamTableReader` 实现**（本轮裁定）：
 * 今天唯一的消费方（Task 4 的物化器）是 C++ 直调；而 `ITcsParamTableReader` **不是
 * `Blueprintable`**（宿主脚本实现不了它）⇒ 造一个 `UObject` 壳属"零消费者预建"，
 * 且会把快照指针的寿命绑到 GC 上。**继承接口的时机** = 出现第一个需要把快照当参数表
 * 挂进 `FTcsParamEvaluateContext` 的反射消费方。
 *
 * **寿命契约**：本类**不持有**快照——构造方 MUST 保证快照在读取期间存活（实例里的快照
 * 在实例在册期间稳定；栈上构造的快照按作用域）。故本类只在一处调用期间使用，不跨帧携带。
 */
class FTcsStateParamTableReader
{
// 构造
#pragma region Lifetime

public:
	// 绑定一份快照（不接管所有权）
	explicit FTcsStateParamTableReader(const FTcsParamSnapshot* InSnapshot)
		: Snapshot(InSnapshot)
	{
	}

#pragma endregion


// 读取
#pragma region Query

public:
	/**
	 * 按键读取快照内的规范值。
	 *
	 * @param Key 参数键。
	 * @param OutValue 输出读到的值（miss 时内容未定义，调用方不得使用）。
	 * @return 返回是否命中；false = miss（未绑定快照也算 miss）。
	 */
	bool TryGetNumericParam(FGameplayTag Key, double& OutValue) const
	{
		return Snapshot ? Snapshot->TryGetNumericParam(Key, OutValue) : false;
	}

#pragma endregion


// 内核
#pragma region Core

private:
	// 被读取的快照（裸指针——本类不参与寿命管理，见类型注释）
	const FTcsParamSnapshot* Snapshot = nullptr;

#pragma endregion
};
