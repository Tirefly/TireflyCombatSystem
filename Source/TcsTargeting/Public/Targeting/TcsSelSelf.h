// Copyright Tirefly. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Targeting/TcsTargetSelectorStrategy.h"

#include "TcsSelSelf.generated.h"



/**
 * 内置选择器：**选自己**（设计 10 §2.2 承诺的框架默认选择器之一——Self / EventTarget；
 * EventTarget 依赖"事件载荷 → 目标"通路，随触发行轮落地）。
 *
 * 语义：把 `Context.Caster` 写进目标集。适用于"以自身为目标"的链（自我增益、自身 DOT 载体等）。
 *
 * **为什么它必须住框架而不是切片**（内容资产的类型稳定性）：`TInstancedStruct` 存的是**类型身份**
 * （包 import 表索引，非名字字符串）——链资产若引用切片侧的类型，切片退役后该步骤会**静默变空**
 * （降级路径只剩一条 `LogCore` Warning，`InstancedStruct.cpp:237-248`）。而"选自己"是**任何项目都
 * 会用到**的通用语义，故由框架提供（纪律：**内容引用的每个类型都必须长于内容**）。
 *
 * **非 `Hidden`**：与两个策略基类不同，本类型是**可选实现**而非抽象基类——必须出现在编辑器类型
 * picker 里（策划在链资产上直接选它）。
 *
 * 纪律（与基类契约一致）：
 * - **只填充不清空** `OutTargets`（清空是调用方 `SelectTargets` 执行器的责任）；
 * - **不做存活过滤**（框架不认识"存活"——候选有效性由宿主的 Filter 表达）；
 * - **不需要实体查询**（`EntityQuery` 为 `nullptr` 照常工作——本选择器不解析位置/存活）。
 */
USTRUCT()
struct TCSTARGETING_API FTcsSelSelf : public FTcsTargetSelectorStrategy
{
	GENERATED_BODY()

// 解析
#pragma region Resolve

public:
	/**
	 * 把施法者写进目标集。
	 *
	 * `Context.Caster` 无效时不产出任何目标 + Warning——"没配施法者"与"选中空集"必须可区分
	 * （后者是合法结果，前者是调用方装配缺失）。
	 *
	 * @param Context 链黑板（只读 `Caster`）。
	 * @param EntityQuery 宿主实体查询能力（本选择器不消费，允许为 `nullptr`）。
	 * @param OutTargets 目标集出参（只追加，不清空）。
	 */
	virtual void Resolve(const FTcsEffectContext& Context, ITcsEntityQuery* EntityQuery, TArray<FTcsCombatEntityHandle>& OutTargets) const override;

#pragma endregion
};
