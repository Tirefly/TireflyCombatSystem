// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "StructUtils/InstancedStruct.h"



/**
 * 参数快照条目（D3-12 / PV-3）：**已解析规范值 + 源引用位**双字段。
 *
 * **为什么留着源引用位**（今天零消费者）：设计明文要求（`SPEC-02-states` §3.6）——Debug 侧答
 * "这个数是从哪来的"，以及 Live 化（已承诺基建）时**零结构迁移**。删掉它会与设计冲突，
 * 而保留的成本只是一个可复制的 `FInstancedStruct`。判据见 `docs-convention` §6.4（定义只留一处，
 * 本类型就是那一处）。源引用按"值语义副本"持有（拷的是该行的数值来源，不是指向 Def 的指针）。
 */
struct FTcsParamSnapshotEntry
{
// 键与值
#pragma region Value

public:
	// 参数键（`TcsStateParam` 根；与 `FTcsNumericParamRow::Key` 同键空间）
	FGameplayTag Key;

	// 已解析的规范值（写入口完成 `ValueConvention` 转换——快照内永远规范值）
	double Value = 0.0;

#pragma endregion


// 源引用位
#pragma region Source

public:
	// 该行数值来源的副本（Debug 可见取值来源 / Live 化预留；无消费者，见类型注释）
	FInstancedStruct SourceRef;

#pragma endregion
};



/**
 * 参数快照（D3-12）：状态**施加瞬间**把全部生效参数一次性求值并冻结的结果。
 *
 * **语义三条**（设计 §3.6）：
 * ① **施加那一刻求值**——施加上下文覆盖值优先、Def 默认兜底；`ValueConvention` 转换发生在写入点；
 * ② **实例生命周期内读快照**——MUST NOT 重算（"运行中升级不追溯"的既定语义；tooltip 的当前档
 *    高亮也取快照的 `Level`，而非查询时单位等级）；
 * ③ **修改 = 重新施加**（快照重建，新 payload 覆盖旧的）。
 *
 * **非反射结构体**（无 `USTRUCT` 宏）：它只随 `FTcsStateInstance` 在 C++ 侧流转——反射化的时机
 * 是"快照要整体过网"的操作复制轮（`WAIT-3` / R7），不是现在。故它也**不能**作 `UPROPERTY`，
 * 其内层的对象引用由门面的 `AddReferencedObjects` 手动补（GC 纪律）。
 *
 * **命名**：实现名取设计名 `FTcsParamSnapshot`（`SPEC-02-states` §3.6 的 2026-09-14 命名批）——
 * 它是**参数域**的概念（修正器物化、技能侧参数行都要读它），不是状态模块专属。
 */
struct FTcsParamSnapshot
{
// 条目
#pragma region Entries

public:
	// 冻结的生效参数（写入序 = `Def.Params` 行序；同键重复由资产校验拦在作者期）
	TArray<FTcsParamSnapshotEntry> Entries;

#pragma endregion


// 查询
#pragma region Query

public:
	/**
	 * 按键读取已冻结的规范值。
	 *
	 * @param Key 参数键。
	 * @param OutValue 输出读到的值（miss 时内容未定义，调用方不得使用）。
	 * @return 返回是否命中；false = miss。
	 */
	bool TryGetNumericParam(FGameplayTag Key, double& OutValue) const
	{
		for (const FTcsParamSnapshotEntry& Entry : Entries)
		{
			if (Entry.Key == Key)
			{
				OutValue = Entry.Value;
				return true;
			}
		}

		return false;
	}

	// 条目数（观测与装置断言用）
	int32 Num() const
	{
		return Entries.Num();
	}

	// 清空（快照重建的第一步；槽位复用时也走它）
	void Reset()
	{
		Entries.Reset();
	}

#pragma endregion
};
